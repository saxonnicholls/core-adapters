#
#  errors_demo.py
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
"""Demo for <snicholls/core_adapters/nanobind/errors.hpp>: C++ errors arrive as the shared Python classes."""

import math
import sys

import errors_demo_module as m
from snicholls_core.errors import CoreError, PreconditionError, TracedError

results = []
print("price(100) =", m.price(100.0))
results.append(m.price(100.0) == 101.0)
try:
    m.price(-1.0)
except TracedError as e:
    print(f"TracedError: {e} (error {e.error!r}, at {e.file.rsplit('/', 1)[-1]}:{e.line})")
    results.append(e.error == "spot -1 is not positive" and isinstance(e, CoreError))
try:
    m.price(math.nan)
except PreconditionError as e:
    print(f"PreconditionError (also a ValueError: {isinstance(e, ValueError)}): {e}")
    results.append(isinstance(e, ValueError))
try:
    m.lookup(7)
except IndexError as e:
    print("IndexError:", e)
    results.append(str(e) == "no instrument 7")
ok = results == [True] * 4
print("errors_demo:", "ok" if ok else "FAILED")
sys.exit(0 if ok else 1)
