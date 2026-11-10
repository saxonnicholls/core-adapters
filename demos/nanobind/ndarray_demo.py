#
#  ndarray_demo.py
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
"""Demo for <snicholls/core_adapters/nanobind/ndarray.hpp>: NumPy in, C++ without the GIL, NumPy out."""

import sys

import ndarray_demo_module as m
import numpy as np
from snicholls_core.errors import PreconditionError

v = np.array([0.0, 1.0, 2.0, 4.0])            # v(none), v({0}), v({1}), v({0, 1})
d = m.harsanyi_dividends(v)
print("dividends of", v, "=", d)               # [0, 1, 2, 1]: the pair adds 1 beyond its members
o = m.outer(np.array([1.0, 2.0]), np.arange(3, dtype=np.int32))   # int32 is converted on the way in
print("outer:\n", o)
a = np.ones(3)
m.scale(a, 2.5)
print("scaled in place:", a)
try:
    m.harsanyi_dividends(np.ones(3))
    refused = False
except PreconditionError as e:
    print("refused:", e)
    refused = True
ok = d.tolist() == [0.0, 1.0, 2.0, 1.0] and o.shape == (2, 3) and o[1, 2] == 4.0 and a.tolist() == [2.5] * 3 and refused
print("ndarray_demo:", "ok" if ok else "FAILED")
sys.exit(0 if ok else 1)
