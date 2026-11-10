#
#  errors_demo.py
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
"""Demo for snicholls_core.errors: one hierarchy for every snicholls extension module."""

import sys

import numpy as np
from snicholls_core import errors, game

try:
    game.shapley(np.ones(6))            # not 2^n values
    caught = None
except errors.CoreError as e:           # the base catches every core error
    caught = e
print(f"{type(caught).__name__}: {caught}")
print("PreconditionError is a ValueError:", issubclass(errors.PreconditionError, ValueError))
print("TracedError is a ResultError:", issubclass(errors.TracedError, errors.ResultError))
ok = isinstance(caught, errors.PreconditionError) and isinstance(caught, ValueError)
print("errors_demo:", "ok" if ok else "FAILED")
sys.exit(0 if ok else 1)
