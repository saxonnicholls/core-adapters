#
#  stats_demo.py
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
"""Demo for snicholls_core.stats: Wyrand and the draws, the same bits as C++ on every platform."""

import copy
import sys

import numpy as np
from snicholls_core import stats

checks = []
g = stats.Wyrand(1)
first = [g() for _ in range(3)]
print("Wyrand(1) draws:", first)
checks.append(first == [12120239713417281201, 9861924296701902397, 4420488659902023302])

h = stats.Wyrand(42)
print("random_u64:", stats.random_u64(h), " x3:", stats.random_u64(h, 3))
print("uniform_index(6):", stats.uniform_index(h, 6), " x10:", stats.uniform_index(h, 6, 10))
print("uniform_real:", stats.uniform_real(h), " x3:", stats.uniform_real(h, 3))
p = stats.permutation(10, stats.Wyrand(5))
a = np.arange(10, dtype=np.int64)
stats.shuffle(a, stats.Wyrand(5))
print("permutation(10):", p, " shuffle gives the same:", (a == p).all())
checks.append((a == p).all() and sorted(p.tolist()) == list(range(10)))

streams = [stats.Wyrand.stream(7, k) for k in range(3)]
print("three parallel streams:", [s() for s in streams])
c = copy.copy(h)
checks.append(c == h and c() == h())

try:
    stats.uniform_index(h, 0)
    checks.append(False)
except ValueError as e:
    print("refused:", e)
ok = all(checks)
print("stats_demo:", "ok" if ok else f"FAILED {checks}")
sys.exit(0 if ok else 1)
