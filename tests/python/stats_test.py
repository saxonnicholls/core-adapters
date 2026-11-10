#
#  stats_test.py
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
"""snicholls_core.stats: Wyrand and the draws, bit for bit against an independent implementation and against C++."""

import copy
import threading
import time

import numpy as np
import pytest
from snicholls_core import stats
from snicholls_core.errors import PreconditionError


def test_draws_match_the_independent_implementation(wyrand_fixture):
    for seed, draws in wyrand_fixture["draws"].items():
        g = stats.Wyrand(int(seed))
        assert [g() for _ in draws] == draws
        assert list(stats.random_u64(stats.Wyrand(int(seed)), len(draws))) == draws


def test_streams_match(wyrand_fixture):
    for s in wyrand_fixture["streams"]:
        g = stats.Wyrand.stream(s["seed"], s["k"])
        assert [g() for _ in s["draws"]] == s["draws"]


def test_uniform_index_matches_including_rejections(wyrand_fixture):
    for s in wyrand_fixture["uniform_index"]:
        g = stats.Wyrand(s["seed"])
        assert [stats.uniform_index(g, s["n"]) for _ in s["draws"]] == s["draws"]
        assert list(stats.uniform_index(stats.Wyrand(s["seed"]), s["n"], len(s["draws"]))) == s["draws"]


def test_uniform_real_matches_exactly(wyrand_fixture):
    r = wyrand_fixture["uniform_real"]
    g = stats.Wyrand(r["seed"])
    assert [stats.uniform_real(g) for _ in r["draws"]] == r["draws"]
    assert list(stats.uniform_real(stats.Wyrand(r["seed"]), len(r["draws"]))) == r["draws"]


def test_shuffle_matches(wyrand_fixture):
    for s in wyrand_fixture["shuffle"]:
        a = np.arange(s["size"], dtype=np.int64)
        stats.shuffle(a, stats.Wyrand(s["seed"]))
        assert a.tolist() == s["result"]
        assert stats.permutation(s["size"], stats.Wyrand(s["seed"])).tolist() == s["result"]


def test_seeded_draws_equal_cpp(cpp_reference):
    d = cpp_reference["draws"]
    g = stats.Wyrand(d["seed"])
    assert [g() for _ in range(5)] == d["u64"]
    assert [stats.uniform_index(g, 1000) for _ in range(5)] == d["index_1000"]
    assert [stats.uniform_real(g) for _ in range(5)] == d["real"]
    assert stats.permutation(20, g).tolist() == d["permutation_20"]


def test_shuffle_of_every_dtype_is_the_same_permutation():
    order = stats.permutation(50, stats.Wyrand(3))
    for dtype in (np.int64, np.uint64, np.float64):
        a = np.arange(50).astype(dtype)
        stats.shuffle(a, stats.Wyrand(3))
        assert a.astype(np.int64).tolist() == order.tolist()


def test_shuffle_refuses_what_it_cannot_shuffle_in_place():
    with pytest.raises(TypeError):
        stats.shuffle(np.arange(10, dtype=np.int32), stats.Wyrand())
    with pytest.raises(TypeError):
        stats.shuffle(np.arange(10, dtype=np.int64)[::2], stats.Wyrand())
    with pytest.raises(TypeError):
        stats.shuffle([1, 2, 3], stats.Wyrand())


def test_generator_state_copy_and_equality():
    a = stats.Wyrand(5)
    a()
    b = copy.copy(a)
    c = copy.deepcopy(a)
    assert a == b == c
    assert a() == b() == c()
    a()
    assert a != b
    assert stats.Wyrand() == stats.Wyrand(0)
    assert stats.Wyrand.stream(7, 0) != stats.Wyrand.stream(7, 1)


def test_bounds_and_errors():
    g = stats.Wyrand(1)
    assert stats.uniform_index(g, 1) == 0
    idx = stats.uniform_index(g, 7, 10000)
    assert idx.dtype == np.uint64 and idx.min() == 0 and idx.max() == 6
    u = stats.uniform_real(g, 10000)
    assert u.dtype == np.float64 and 0.0 <= u.min() and u.max() < 1.0 and abs(u.mean() - 0.5) < 0.02
    with pytest.raises(PreconditionError, match="n must be positive"):
        stats.uniform_index(g, 0)
    with pytest.raises(PreconditionError):
        stats.uniform_index(g, 0, 5)
    with pytest.raises(TypeError):
        stats.uniform_index(g, -1)
    with pytest.raises(TypeError):
        stats.Wyrand(-1)
    assert stats.random_u64(g, 0).shape == (0,)


def test_bulk_draws_release_the_gil():
    count = 0
    stop = threading.Event()

    def counter():
        nonlocal count
        while not stop.is_set():
            count += 1
            time.sleep(0)

    t = threading.Thread(target=counter)
    t.start()
    time.sleep(0.02)
    before = count
    stats.uniform_real(stats.Wyrand(1), 30_000_000)
    during = count - before
    stop.set()
    t.join()
    assert during > 5   # a GIL-held call would allow ~0 ticks; machines under load tick far less than idle ones
