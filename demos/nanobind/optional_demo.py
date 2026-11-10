#
#  optional_demo.py
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
"""Demo for <snicholls/core_adapters/nanobind/optional.hpp>: std::optional as None, snicholls::result as raise."""

import sys

import optional_demo_module as m
from snicholls_core.errors import ResultError

print("canonical_decimal('72.50') =", repr(m.canonical_decimal("72.50")))
print("canonical_decimal('abc')   =", repr(m.canonical_decimal("abc")))
print("parse_decimal('0.1')       =", repr(m.parse_decimal("0.1")))
try:
    m.parse_decimal("0.10")
    raised = False
except ResultError as e:
    print("ResultError:", e)
    raised = True
ok = m.canonical_decimal("72.50") == "72.5" and m.canonical_decimal("abc") is None and m.parse_decimal("0.1") == "0.1" and raised
print("optional_demo:", "ok" if ok else "FAILED")
sys.exit(0 if ok else 1)
