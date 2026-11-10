#
#  game_test.py
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
"""snicholls_core.game: closed-form games, the reference values, C++ determinism, errors and the GIL."""

import itertools
import math
import threading
import time

import numpy as np
import pytest
from snicholls_core import game, stats
from snicholls_core.errors import PreconditionError


# ---- games with closed-form answers -------------------------------------------------------------------------------

def glove(s):
    """Players 0 and 1 hold left gloves, player 2 a right glove: a pair is worth 1."""
    return float(min(bin(s & 3).count("1"), (s >> 2) & 1))


def unanimity(t):
    """u_T(S) = 1 when S contains T: Shapley 1/|T| to each member, Banzhaf 1/2^(|T|-1), one dividend of 1 at T."""
    return lambda s: float(s & t == t)


def weighted_majority(quota, weights):
    return lambda s: float(sum(w for i, w in enumerate(weights) if s >> i & 1) >= quota)


def airport(costs):
    """The cost of the longest runway any member needs: Shapley shares each segment equally among its users."""
    return lambda s: max([c for i, c in enumerate(costs) if s >> i & 1], default=0.0)


def airport_shapley(costs):
    c = [0.0] + sorted(costs)
    n = len(costs)
    out, acc = [], 0.0
    for k in range(1, n + 1):
        acc += (c[k] - c[k - 1]) / (n - k + 1)
        out.append(acc)
    order = sorted(range(n), key=lambda i: costs[i])
    phi = [0.0] * n
    for rank, i in enumerate(order):
        phi[i] = out[rank]
    return phi


def test_glove_game_both_routes():
    for a in (game.shapley(glove, 3), game.shapley(game.tabulate(glove, 3))):
        np.testing.assert_allclose(a.values, [1 / 6, 1 / 6, 2 / 3])
        assert a.baseline == 0.0 and a.total == 1.0 and a.players == 3
        assert abs(a.unattributed()) < 1e-15 and a.attributed() == pytest.approx(1.0)
        assert a.standard_errors is None and a.samples == 0 and a.evaluations == 8


def test_additive_game_gives_each_player_its_weight():
    w = [0.5, -1.25, 3.0, 7.0]
    a = game.shapley(lambda s: sum(x for i, x in enumerate(w) if s >> i & 1) + 10.0, 4)
    np.testing.assert_allclose(a.values, w)
    assert a.baseline == 10.0
    assert game.is_additive(lambda s: sum(x for i, x in enumerate(w) if s >> i & 1), 4)


def test_unanimity_games():
    t = 0b1011
    v = unanimity(t)
    np.testing.assert_allclose(game.shapley(v, 5).values, [1 / 3, 1 / 3, 0, 1 / 3, 0])
    np.testing.assert_allclose(game.banzhaf(v, 5).values, [1 / 4, 1 / 4, 0, 1 / 4, 0])
    d = game.harsanyi_dividends(v, 5)
    assert d[t] == 1.0 and np.count_nonzero(d) == 1
    np.testing.assert_allclose(game.shapley_from_dividends(d).values, [1 / 3, 1 / 3, 0, 1 / 3, 0])
    np.testing.assert_allclose(game.banzhaf_from_dividends(d).values, [1 / 4, 1 / 4, 0, 1 / 4, 0])


def test_weighted_majority_game():
    np.testing.assert_allclose(game.shapley(weighted_majority(51, [50, 49, 1]), 3).values, [2 / 3, 1 / 6, 1 / 6])


def test_airport_game():
    costs = [3.0, 1.0, 4.0, 1.5, 9.0, 2.5]
    np.testing.assert_allclose(game.shapley(airport(costs), 6).values, airport_shapley(costs), rtol=1e-13)


def test_owen_of_a_unanimity_game_in_closed_form():
    # u_T with T = {0, 1, 3}: the groups meeting T share equally, then their members of T share equally
    v = unanimity(0b1011)
    a = game.owen(v, 5, [[0, 1], [2, 3], [4]])
    np.testing.assert_allclose(a.values, [1 / 4, 1 / 4, 0, 1 / 2, 0])
    np.testing.assert_allclose(game.owen(game.tabulate(v, 5), [[0, 1], [2, 3], [4]]).values, a.values)


def test_owen_with_trivial_groups_is_shapley():
    t = game.tabulate(airport([1.0, 2.0, 3.0, 4.0]), 4)
    s = game.shapley(t).values
    np.testing.assert_allclose(game.owen(t, [[0], [1], [2], [3]]).values, s)
    np.testing.assert_allclose(game.owen(t, [[0, 1, 2, 3]]).values, s)


def test_owen_calls_the_game_only_for_the_coalitions_it_needs():
    calls = []
    v = lambda s: calls.append(s) or float(bin(s).count("1") ** 2)
    a = game.owen(v, 12, [[0, 1, 2, 3], [4, 5, 6, 7], [8, 9, 10, 11]])
    assert abs(a.unattributed()) < 1e-9
    assert len(set(calls)) < 2 ** 12 and a.evaluations == len(calls)


# ---- reference values ---------------------------------------------------------------------------------------------

def test_reference_values(game_reference):
    for case in game_reference:
        name, method = case["name"], case["method"]
        if method in ("shapley", "shapley_metrics"):
            t = np.array(case["table"])
            a = game.shapley(t)
            np.testing.assert_allclose(a.values, case["values"], rtol=1e-12, atol=1e-14, err_msg=name)
            np.testing.assert_allclose(a.baseline, case["baseline"], err_msg=name)
            np.testing.assert_allclose(a.total, case["total"], err_msg=name)
            # the callable route agrees with the table route
            rows = t.tolist()
            b = game.shapley(lambda s: rows[s], case["n"])
            np.testing.assert_allclose(b.values, a.values, rtol=1e-14, atol=1e-15, err_msg=name)
        elif method == "owen":
            t = np.array(case["table"])
            a = game.owen(t, case["groups"])
            np.testing.assert_allclose(a.values, case["values"], rtol=1e-12, atol=1e-14, err_msg=name)
            rows = t.tolist()
            b = game.owen(lambda s: rows[s], case["n"], case["groups"])
            np.testing.assert_allclose(b.values, a.values, rtol=1e-14, atol=1e-15, err_msg=name)
        elif method == "interactions":
            np.testing.assert_allclose(game.interactions(np.array(case["table"])), case["matrix"], rtol=1e-12,
                                       atol=1e-14, err_msg=name)
        elif method == "attribute_change":
            if case.get("formula") == "exp_product":
                f = lambda x: math.exp(0.3 * x[0]) * x[1] - x[2] * x[3] ** 2
            else:
                f = lambda x: x[0] * x[1] + 3 * x[2]
            a = game.attribute_change(f, np.array(case["start"]), np.array(case["end"]))
            np.testing.assert_allclose(a.values, case["values"], rtol=1e-12, err_msg=name)
            assert a.baseline == pytest.approx(case["baseline"]) and a.total == pytest.approx(case["total"])
            assert abs(a.unattributed()) < 1e-12 and a.evaluations == 2 ** len(case["start"])
        else:
            pytest.fail(f"unknown method {method}")
    assert len(game_reference) >= 20


def test_the_shapley_axioms_on_a_random_game():
    rng = np.random.default_rng(5)
    n = 7
    t = rng.normal(size=2 ** n)
    a = game.shapley(t)
    assert abs(a.unattributed()) < 1e-12                                       # efficiency
    dummy = np.array(  # player 7 adds nothing
        [t[s & (2 ** n - 1)] for s in range(2 ** (n + 1))])
    assert abs(game.shapley(dummy).values[n]) < 1e-12                          # null player
    u = rng.normal(size=2 ** n)
    np.testing.assert_allclose(game.shapley(2 * t + u).values,                 # linearity
                               2 * a.values + game.shapley(u).values, rtol=1e-10, atol=1e-12)


def test_vector_valued_games():
    a = game.shapley(lambda s: np.array([glove(s), bin(s).count("1")]), 3)
    assert a.values.shape == (3, 2) and a.baseline.shape == (2,)
    np.testing.assert_allclose(a.values[:, 0], [1 / 6, 1 / 6, 2 / 3])
    np.testing.assert_allclose(a.values[:, 1], [1, 1, 1])
    np.testing.assert_allclose(a.unattributed(), [0, 0], atol=1e-15)
    t = game.tabulate(lambda s: [glove(s), 2.0], 3)
    assert t.shape == (8, 2)
    assert game.harsanyi_dividends(t).shape == (8, 2)
    np.testing.assert_allclose(game.banzhaf(t).values[:, 1], [0, 0, 0])


def test_set_game_adapts_a_frozenset_game():
    v = game.set_game(lambda s: float(min(len(s & {0, 1}), len(s & {2}))))
    np.testing.assert_allclose(game.shapley(v, 3).values, [1 / 6, 1 / 6, 2 / 3])


# ---- the samplers ---------------------------------------------------------------------------------------------------

def test_sampled_converges_and_is_always_efficient():
    t = game.tabulate(glove, 3)
    a = game.shapley_sampled(t, stats.Wyrand(7), permutations=2000)
    np.testing.assert_allclose(a.values, [1 / 6, 1 / 6, 2 / 3], atol=0.02)
    assert abs(a.unattributed()) < 1e-12
    assert a.standard_errors.shape == (3,) and (a.standard_errors < 0.02).all()
    assert a.samples == 2000


def test_sampled_on_a_callable_equals_sampled_on_its_table():
    t = game.tabulate(airport([1.0, 5.0, 2.0, 7.0, 3.0]), 5)
    rows = t.tolist()
    for antithetic in (True, False):
        a = game.shapley_sampled(t, stats.Wyrand(3), 40, antithetic)
        b = game.shapley_sampled(lambda s: rows[s], 5, stats.Wyrand(3), 40, antithetic)
        assert a.values.tolist() == b.values.tolist()
        assert a.evaluations == b.evaluations
    a = game.shapley_sampled_lifts(t, stats.Wyrand(4), 30)
    b = game.shapley_sampled_lifts(lambda s: rows[s], 5, stats.Wyrand(4), 30)
    assert a.values.tolist() == b.values.tolist()


def test_lifts_sampler():
    t = game.tabulate(airport([1.0, 5.0, 2.0, 7.0, 3.0]), 5)
    exact = game.shapley(t).values
    a = game.shapley_sampled_lifts(t, stats.Wyrand(9), samples_per_player=4000)
    np.testing.assert_allclose(a.values, exact, atol=0.15)
    assert abs(a.unattributed()) < 1e-9                      # scale_to_total
    b = game.shapley_sampled_lifts(t, stats.Wyrand(9), samples_per_player=50, scale_to_total=False)
    assert b.samples == 50


def test_memoized_evaluates_each_coalition_once():
    calls = []
    v = game.memoized(lambda s: calls.append(s) or glove(s))
    a = game.shapley_sampled(v, 3, stats.Wyrand(1), 500)
    assert len(calls) == v.evaluations == 8
    assert a.evaluations > 8                                 # calls to the memoized game, cache hits included
    assert v(5) == 1.0 and len(calls) == 8
    o = game.owen(v, 3, [[0, 1], [2]])
    np.testing.assert_allclose(o.values, [0.25, 0.25, 0.5])
    assert len(calls) == 8


def test_memoized_with_a_cycle_is_collected():
    import gc
    import weakref

    class Holder:
        pass

    h = Holder()
    h.v = game.memoized(lambda s: h and 1.0)                 # the game refers back to its holder
    r = weakref.ref(h)
    del h
    gc.collect()
    assert r() is None


def test_sampled_results_equal_cpp(cpp_reference):
    t = np.array(cpp_reference["table6"])
    exact = game.shapley(t)
    assert exact.values.tolist() == cpp_reference["shapley6"]["values"]

    def same(a, ref):
        assert a.values.tolist() == ref["values"]
        assert [None if math.isnan(x) else x for x in a.standard_errors.tolist()] == ref["standard_errors"]
        assert (a.samples, a.evaluations) == (ref["samples"], ref["evaluations"])
        assert (a.baseline, a.total) == (ref["baseline"], ref["total"])

    g = stats.Wyrand(11)
    same(game.shapley_sampled(t, g, 64, True), cpp_reference["sampled_table"])
    assert g() == cpp_reference["sampled_table_next_draw"]   # the generator advanced exactly as in C++
    rows = t.tolist()
    same(game.shapley_sampled(lambda s: rows[s], 6, stats.Wyrand(12), 50, False), cpp_reference["sampled_plain"])
    same(game.shapley_sampled_lifts(t, stats.Wyrand(13), 40, True), cpp_reference["lifts_table"])
    same(game.shapley_sampled_lifts(t, stats.Wyrand(14), 40, False), cpp_reference["lifts_unscaled"])
    o = game.owen(t, [[0, 1], [2, 3], [4, 5]])
    assert o.values.tolist() == cpp_reference["owen6"]["values"]


# ---- interactions and properties -----------------------------------------------------------------------------------

def test_interactions_detect_complements_and_substitutes():
    i = game.interactions(glove, 3)
    assert i[0, 2] > 0 and i[1, 2] > 0 and i[0, 1] < 0
    np.testing.assert_allclose(i, i.T)
    assert (np.diag(i) == 0).all()
    assert game.interactions(np.array([0.0, 1.0])).shape == (1, 1)
    assert game.interactions(np.array([0.0])).shape == (0, 0)


def test_properties_of_the_glove_game():
    t = game.tabulate(glove, 3)
    assert game.is_monotone(t) and game.is_superadditive(t)
    assert not game.is_supermodular(t) and not game.is_submodular(t) and not game.is_additive(t)
    v = game.find_supermodularity_violation(t)
    assert set(v) == {"base", "first", "second", "difference"} and v["difference"] < 0
    assert game.find_monotonicity_violation(t) is None
    assert game.find_superadditivity_violation(t) is None
    assert game.find_interaction(t) is not None
    assert game.find_submodularity_violation(t) is not None
    assert game.is_supermodular(t, tolerance=1.0)


def test_monotonicity_and_superadditivity_violations():
    t = np.array([0.0, 2.0, 3.0, 1.0])   # v(01) = 1 < v(1) = 3
    m = game.find_monotonicity_violation(t)
    assert m is not None and m["lift"] < 0 and not game.is_monotone(t)
    s = game.find_superadditivity_violation(t)
    assert s is not None and s["shortfall"] == -4.0 and s["first"] & s["second"] == 0
    assert game.is_monotone(t, tolerance=10.0)


def test_the_core():
    t = game.tabulate(glove, 3)
    assert game.in_core(t, np.array([0.0, 0.0, 1.0]))
    assert not game.in_core(t, np.array([1 / 6, 1 / 6, 2 / 3]))      # the Shapley value is outside it
    v = game.find_core_violation(t, np.array([1 / 6, 1 / 6, 2 / 3]))
    assert v["shortfall"] < 0 and game.coalition_size(v["blocking"]) == 2
    assert game.find_core_violation(t, np.array([0.0, 0.0, 1.0])) is None
    with pytest.raises(PreconditionError, match="one amount per player"):
        game.in_core(t, np.array([1.0]))


def test_properties_accept_a_callable_too():
    assert game.is_monotone(glove, 3)
    assert game.in_core(glove, 3, np.array([0.0, 0.0, 1.0]))


# ---- coalitions ------------------------------------------------------------------------------------------------------

def test_coalition_helpers():
    assert game.singleton(3) == 8 and game.grand_coalition(4) == 15 and game.grand_coalition(64) == 2 ** 64 - 1
    assert game.contains(0b101, 2) and not game.contains(0b101, 1) and not game.contains(1, 100)
    assert game.with_player(0b1, 3) == 0b1001 and game.without(0b1001, 0) == 0b1000
    assert game.coalition_size(0b10110) == 3 and game.table_size(5) == 32
    assert game.members(0b10110) == [1, 2, 4] and game.from_members([4, 1, 2]) == 0b10110
    assert game.max_table_players == 30
    for bad in (lambda: game.singleton(64), lambda: game.with_player(0, 64), lambda: game.table_size(31),
                lambda: game.from_members([64])):
        with pytest.raises(PreconditionError):
            bad()


# ---- errors ----------------------------------------------------------------------------------------------------------

def test_errors():
    with pytest.raises(PreconditionError, match="2\\^n rows"):
        game.shapley(np.ones(6))
    with pytest.raises(PreconditionError, match="2\\^n rows"):
        game.shapley(np.ones(0))
    with pytest.raises(PreconditionError, match="partition"):
        game.owen(np.ones(8), [[0, 1]])
    with pytest.raises(PreconditionError, match="partition"):
        game.owen(np.ones(8), [[0, 1], [2], []])
    with pytest.raises(PreconditionError, match="out of range"):
        game.owen(np.ones(8), [[0, 1], [5]])
    with pytest.raises(PreconditionError, match="twice"):
        game.owen(np.ones(8), [[0, 0, 1], [2]])
    with pytest.raises(TypeError, match="neither a number nor an array"):
        game.shapley(lambda s: "x", 2)
    with pytest.raises(PreconditionError, match="different sizes"):
        game.shapley(lambda s: np.ones(1 + (s == 3)), 2)
    with pytest.raises(TypeError, match="returns a number"):
        game.owen(lambda s: np.ones(2), 2, [[0, 1]])
    with pytest.raises(PreconditionError, match="scalar game"):
        game.interactions(np.ones((4, 2)))
    with pytest.raises(PreconditionError, match="at most 20 players"):
        game.is_monotone(np.zeros(2 ** 21))
    with pytest.raises(PreconditionError, match="positive"):
        game.shapley_sampled(np.ones(4), stats.Wyrand(), permutations=0)
    with pytest.raises(PreconditionError, match="not callable"):
        game.owen(5, 2, [[0, 1]])
    with pytest.raises(PreconditionError, match="differ in length"):
        game.attribute_change(lambda x: 0.0, np.ones(2), np.ones(3))

    def boom(s):
        if s == 3:
            raise KeyError("raised by the game")
        return 0.0

    with pytest.raises(KeyError, match="raised by the game"):
        game.shapley(boom, 2)
    with pytest.raises(KeyError, match="raised by the game"):
        game.shapley_sampled(boom, 2, stats.Wyrand(), 10)


# ---- the GIL ---------------------------------------------------------------------------------------------------------

def test_table_shapley_releases_the_gil_and_runs_in_parallel():
    t = np.random.default_rng(1).normal(size=2 ** 20)
    count = 0
    stop = threading.Event()

    def counter():
        nonlocal count
        while not stop.is_set():
            count += 1
            time.sleep(0)

    th = threading.Thread(target=counter)
    th.start()
    time.sleep(0.02)
    before = count
    a = game.shapley(t)
    during = count - before
    stop.set()
    th.join()
    assert abs(a.unattributed()) < 1e-6
    assert during > 5

    results = [None] * 4

    def work(k):
        results[k] = game.shapley(t * (k + 1)).values

    threads = [threading.Thread(target=work, args=(k,)) for k in range(4)]
    for x in threads:
        x.start()
    for x in threads:
        x.join()
    for k in range(4):
        np.testing.assert_allclose(results[k], results[0] * (k + 1), rtol=1e-10, atol=1e-12)


def test_repr():
    assert repr(game.shapley(glove, 3)).startswith("Attribution(baseline=0.0, values=[")


def test_all_methods_against_brute_force_permutations():
    n = 5
    t = np.random.default_rng(2).normal(size=2 ** n)
    phi = np.zeros(n)
    for order in itertools.permutations(range(n)):
        s = 0
        for i in order:
            phi[i] += t[s | 1 << i] - t[s]
            s |= 1 << i
    phi /= math.factorial(n)
    np.testing.assert_allclose(game.shapley(t).values, phi, rtol=1e-12, atol=1e-14)
