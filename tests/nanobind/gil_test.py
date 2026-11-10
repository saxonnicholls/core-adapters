#
#  gil_test.py
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
"""<snicholls/core_adapters/nanobind/gil.hpp>: release_gil, without_gil, with_gil and python_function."""

import threading
import time

import gil_module as m
import pytest


def progress_while(call, seconds=0.3):
    """How many times a Python thread ran its loop while `call(seconds)` ran on this thread."""
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
    call(seconds)
    during = count - before
    stop.set()
    t.join()
    return during


# Absolute tick counts depend on the machine and its load (a container running four test suites at once ticks far
# less), so each test compares with the GIL held: then the counter only ticks at the call's boundaries, when the
# waiting thread takes the GIL the moment the call returns.

@pytest.fixture(scope="module")
def held_ticks():
    return progress_while(m.spin_held)


def test_release_gil_lets_python_threads_run(held_ticks):
    released = progress_while(m.spin_released)
    assert released > 10 and released > 5 * held_ticks, (released, held_ticks)


def test_without_gil_lets_python_threads_run(held_ticks):
    released = progress_while(m.spin_without_gil)
    assert released > 10 and released > 5 * held_ticks, (released, held_ticks)


def test_with_gil_inside_without_gil_can_call_python():
    assert m.call_python_inside_without_gil(lambda: 41 + 1) == 42


def test_an_exception_thrown_without_the_gil_is_translated():
    with pytest.raises(IndexError, match="thrown with the GIL released"):
        m.throw_without_gil()


def test_python_function_from_many_threads():
    lock = threading.Lock()
    seen = set()

    def f(t):
        with lock:
            seen.add(threading.get_ident())
        return t + 1

    assert m.call_from_threads(f, 4, 250) == 250 * (1 + 2 + 3 + 4)
    assert len(seen) == 4


def test_python_function_void_and_conversion():
    calls = []
    m.call_void(lambda: calls.append(1))
    assert calls == [1]
    assert m.call_and_convert(lambda: 2) == 2.0


def test_python_function_propagates_python_exceptions():
    def boom(_):
        raise KeyError("from Python")

    with pytest.raises(KeyError, match="from Python"):
        m.call_from_threads(boom, 1, 1)


def test_python_function_reports_a_result_that_does_not_convert():
    with pytest.raises(TypeError):
        m.call_and_convert(lambda: "not a number")
