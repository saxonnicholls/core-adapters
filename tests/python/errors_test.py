#
#  errors_test.py
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
"""snicholls_core.errors: the shared hierarchy, and that the bound modules raise it."""

import numpy as np
import pytest
from snicholls_core import errors, game, stats


def test_hierarchy():
    assert issubclass(errors.ResultError, errors.CoreError)
    assert issubclass(errors.TracedError, errors.ResultError)
    assert issubclass(errors.PreconditionError, errors.CoreError)
    assert issubclass(errors.PreconditionError, ValueError)
    assert set(errors.__all__) == {"CoreError", "ResultError", "TracedError", "PreconditionError"}


def test_traced_error_raised_by_hand_has_empty_defaults():
    e = errors.TracedError("message")
    assert (e.error, e.file, e.line, e.function, e.stack) == ("", "", 0, "", "")
    assert e.frames == [] and e.fields == {}
    assert str(e) == "message"


def test_bound_modules_raise_the_shared_classes():
    with pytest.raises(errors.PreconditionError):
        stats.uniform_index(stats.Wyrand(), 0)
    with pytest.raises(errors.PreconditionError):
        game.shapley(np.ones(3))
    with pytest.raises(ValueError):   # PreconditionError is a ValueError, so this works as well
        game.owen(np.ones(4), [[0]])
