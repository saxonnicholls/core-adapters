#
#  errors.py
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
"""The exceptions every snicholls extension module raises for core's errors.

One hierarchy for the whole family: the C++ helper ``snicholls::python::register_exception_translators()``
(``<snicholls/core_adapters/nanobind/errors.hpp>``) looks these classes up here, so an error from any module that
uses the helpers is caught by the same ``except`` clause.

=========================  ====================================================================================
Python                     C++
=========================  ====================================================================================
``CoreError``              the base of the classes below
``ResultError``            ``snicholls::bad_result_access<E>``: an error ``snicholls::result`` was unwrapped, or
                           returned to Python
``TracedError``            a ``snicholls::traced<E>`` error (``snicholls::python::traced_exception``), with
                           where it was created
``PreconditionError``      ``snicholls::python::precondition_error``: an argument broke a documented
                           precondition; also a ``ValueError``
=========================  ====================================================================================

Standard C++ exceptions keep nanobind's mapping: ``std::bad_alloc`` is ``MemoryError``, ``std::invalid_argument``
``ValueError``, ``std::out_of_range`` ``IndexError``, ``std::overflow_error`` ``OverflowError``, any other
``std::exception`` ``RuntimeError``.
"""

from __future__ import annotations

__all__ = ["CoreError", "ResultError", "TracedError", "PreconditionError"]


class CoreError(Exception):
    """The base of every snicholls-core error raised from C++."""


class ResultError(CoreError):
    """An error ``snicholls::result<V, E>`` reached Python (C++ ``snicholls::bad_result_access``).

    The message is ``snicholls::error_text`` of the error.
    """


class TracedError(ResultError):
    """A ``snicholls::traced<E>`` error, with where it was created.

    Attributes (set by the C++ translator; defaults for one raised by hand):

    - ``error``: the error alone, without context (``error_report::error``).
    - ``file``, ``line``, ``function``: where the error was created.
    - ``frames``: the context messages, innermost first.
    - ``fields``: key-value fields, secrets already masked by core.
    - ``stack``: the captured stack trace, or ``""``.
    """

    error: str = ""
    file: str = ""
    line: int = 0
    function: str = ""
    frames: list[str]
    fields: dict[str, str]
    stack: str = ""

    def __init__(self, *args: object) -> None:
        super().__init__(*args)
        self.frames = []
        self.fields = {}


class PreconditionError(CoreError, ValueError):
    """An argument broke a documented precondition (C++ ``snicholls::python::precondition_error``)."""
