#
#  conftest.py
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
"""Fixtures shared by the snicholls_core tests: the JSON fixtures, and what core's C++ computes (reference_values)."""

import json
import os
import pathlib
import subprocess

import pytest

FIXTURES = pathlib.Path(__file__).resolve().parent.parent / "fixtures"


@pytest.fixture(scope="session")
def wyrand_fixture():
    return json.loads((FIXTURES / "wyrand_sequences.json").read_text())


@pytest.fixture(scope="session")
def game_reference():
    return json.loads((FIXTURES / "game_reference.json").read_text())["cases"]


@pytest.fixture(scope="session")
def cpp_reference():
    """The output of tests/support/reference_values.cpp, built in $CORE_ADAPTERS_REFERENCE_DIR (a build directory).

    Missing means the test is not being run by CTest or scripts/gate.sh: the comparison with C++ is skipped then,
    saying so - never silently passed.
    """
    build = os.environ.get("CORE_ADAPTERS_REFERENCE_DIR")
    if not build:
        pytest.skip("CORE_ADAPTERS_REFERENCE_DIR is not set: no C++ reference to compare with")
    exe = pathlib.Path(build) / "tests" / "core_adapters_reference_values"
    if not exe.exists():
        pytest.skip(f"{exe} is not built")
    return json.loads(subprocess.run([str(exe)], check=True, capture_output=True, text=True).stdout)
