#
#  gil_demo.py
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
"""Demo for <snicholls/core_adapters/nanobind/gil.hpp>: C++ work runs while Python threads keep running."""

import sys
import threading
import time

import gil_demo_module as m

ticks = 0
stop = threading.Event()


def ticker():
    global ticks
    while not stop.is_set():
        ticks += 1
        time.sleep(0)


t = threading.Thread(target=ticker)
t.start()
time.sleep(0.02)
before = ticks
total = m.integrate(30_000_000)
during = ticks - before
stop.set()
t.join()
print(f"integrate(30e6) = {total:.6f}; a Python thread ticked {during} times meanwhile (GIL released)")
twice = m.integrate_twice(1000)
squares = m.map_in_threads(lambda i: i * i, 8)
print(f"integrate_twice(1000) = {twice:.9f}; sum of squares from 8 C++ threads = {squares}")
ok = during > 5 and abs(twice - 2 * m.integrate(1000)) < 1e-12 and squares == sum(i * i for i in range(8))
print("gil_demo:", "ok" if ok else "FAILED")
sys.exit(0 if ok else 1)
