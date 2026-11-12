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


SECONDS = 0.3


def longest_pause_while(call, seconds=SECONDS):
    """The longest a Python thread went without running its loop while `call(seconds)` ran on this thread."""
    ticks = []
    stop = threading.Event()

    def counter():
        while not stop.is_set():
            ticks.append(time.perf_counter())
            time.sleep(0)

    t = threading.Thread(target=counter)
    t.start()
    time.sleep(0.02)
    start = time.perf_counter()
    call(seconds)
    end = time.perf_counter()
    stop.set()
    t.join()
    edges = [start, *(s for s in ticks if start < s < end), end]
    return max(b - a for a, b in zip(edges, edges[1:]))


# A count of ticks cannot tell the GIL held from released on a loaded machine: when the call returns, the waiting
# thread takes the GIL before this one reads the count, and a busy VM can leave it there for tens of milliseconds
# (79,000 ticks once, on a 3-CPU macOS runner). The longest pause can: with the GIL held the counter stops for the
# whole call, and with it released it never stops for long.

def test_holding_the_gil_stops_python_threads():
    assert longest_pause_while(m.spin_held) >= 0.9 * SECONDS


def test_release_gil_lets_python_threads_run():
    pause = longest_pause_while(m.spin_released)
    assert pause < 0.5 * SECONDS, pause


def test_without_gil_lets_python_threads_run():
    pause = longest_pause_while(m.spin_without_gil)
    assert pause < 0.5 * SECONDS, pause


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
