#
#  optional_test.py
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
"""<snicholls/core_adapters/nanobind/optional.hpp>: std::optional both ways, snicholls::result as value-or-raise."""

import optional_module as m
import pytest
from snicholls_core.errors import ResultError, TracedError


def test_optional_is_value_or_none():
    assert m.half(4) == 2
    assert m.half(3) is None
    assert m.or_default() == "default"
    assert m.or_default(None) == "default"
    assert m.or_default("given") == "given"
    assert m.uuid_version("0190a5c4-7e1b-7c3a-9f12-3b4c5d6e7f80") == 7
    assert m.uuid_version("not a uuid") is None


def test_result_returns_the_value():
    assert m.checked_divide(7, 2) == 3
    assert m.traced_divide(1, 4) == 0.25
    assert m.check(True) is None
    assert m.index(3) == [7, 7, 7]


def test_result_raises_by_error_type():
    with pytest.raises(ResultError, match="division by zero"):
        m.checked_divide(1, 0)
    with pytest.raises(TracedError, match="division of 5 by zero") as e:
        m.traced_divide(5, 0)
    assert e.value.file.endswith("optional_test.cpp")
    with pytest.raises(ResultError, match="check failed"):
        m.check(False)
    with pytest.raises(IndexError, match="negative index"):
        m.index(-1)


def test_the_stub_signature_shows_the_value_type():
    assert "-> int" in m.checked_divide.__nb_signature__[0][0]
    assert "-> None" in m.check.__nb_signature__[0][0]
