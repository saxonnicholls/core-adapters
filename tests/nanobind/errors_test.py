#
#  errors_test.py
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
"""<snicholls/core_adapters/nanobind/errors.hpp>: every C++ error maps to the documented Python class."""

import errors_module as m
import pytest
from snicholls_core.errors import CoreError, PreconditionError, ResultError, TracedError


def test_require():
    m.require(True)
    with pytest.raises(PreconditionError, match="the argument must be true") as e:
        m.require(False)
    assert isinstance(e.value, ValueError) and isinstance(e.value, CoreError)


def test_unwrap_returns_the_value_or_raises_result_error():
    assert m.unwrap_ok() == 5
    assert m.unwrap_void_ok() is None
    with pytest.raises(ResultError, match="not positive: -2"):
        m.unwrap_err()


def test_cores_own_unwrap_is_a_result_error():
    with pytest.raises(ResultError, match="not positive: -1"):
        m.core_unwrap()


def test_traced_error_carries_location_context_and_masked_fields():
    with pytest.raises(TracedError) as e:
        m.traced()
    t = e.value
    assert isinstance(t, ResultError) and isinstance(t, CoreError)
    assert t.error == "ledger is closed"
    assert "posting a trade" in str(t) and "ledger is closed" in str(t)
    assert t.file.endswith("errors_test.cpp") and t.line > 0 and t.function
    assert t.frames == ["posting a trade"]
    assert t.fields["account"] == "42"
    assert "sk-never-shown" not in t.fields["api_key"] and "sk-never-shown" not in str(t)


def test_an_enum_error_is_a_result_error_with_its_number():
    with pytest.raises(ResultError, match="error 3"):
        m.raise_enum()


def test_a_standard_exception_as_an_error_keeps_its_class():
    with pytest.raises(IndexError, match="index 9 of 3"):
        m.raise_std_out_of_range()


@pytest.mark.parametrize("kind, cls", [
    ("bad_alloc", MemoryError),
    ("invalid_argument", ValueError),
    ("domain_error", ValueError),
    ("length_error", ValueError),
    ("out_of_range", IndexError),
    ("overflow_error", OverflowError),
    ("runtime_error", RuntimeError),
    ("system_error", RuntimeError),
    ("precondition", PreconditionError),
    ("other", RuntimeError),
])
def test_standard_exceptions_keep_nanobinds_mapping(kind, cls):
    with pytest.raises(cls):
        m.throw(kind)


@pytest.mark.parametrize("kind, cls", [
    ("traced", TracedError), ("out_of_range", IndexError), ("bad_alloc", MemoryError), ("result", ResultError),
    ("other", RuntimeError),
])
def test_set_python_error_matches_the_translators(kind, cls):
    with pytest.raises(cls):
        m.set_python_error(kind)
