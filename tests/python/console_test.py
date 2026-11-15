#
#  console_test.py
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
"""snicholls_core.console: the inventory, describe, fields, cpp, the tour and python -m snicholls_core."""

import inspect
import io
import os
import subprocess
import sys
import types

import numpy as np
import pytest
import snicholls_core
from snicholls_core import console, errors, game, stats


def own_public_names(module):
    out = set()
    for n in getattr(module, "__all__", None) or [n for n in dir(module) if not n.startswith("_")]:
        obj = getattr(module, n)
        if isinstance(obj, types.ModuleType):
            continue
        if (inspect.isclass(obj) or callable(obj)) and not (obj.__module__ or "").startswith("snicholls_core"):
            continue
        out.add(n)
    return out


def test_the_inventory_lists_every_public_name_of_every_module():
    rows = console.inventory()
    for name, module in (("errors", errors), ("game", game), ("stats", stats)):
        assert {e.name for e in rows if e.module == name} == own_public_names(module), name


def test_every_name_the_package_binds_names_its_cpp_counterpart():
    # CLAUDE.md: every docstring ends with "C++: <the function it calls>".
    missing = [e.qualified for e in console.inventory() if not e.cpp and e.kind != "constant"]
    assert missing == []


def test_a_constant_shows_its_value():
    by_name = {e.qualified: e for e in console.inventory()}
    assert by_name["game.max_table_players"].signatures == (f"max_table_players = {game.max_table_players!r}",)


def test_every_method_and_property_names_its_cpp_counterpart():
    missing = []
    for e in console.inventory():
        obj = getattr(getattr(snicholls_core, e.module), e.name)
        if inspect.isclass(obj) and not issubclass(obj, BaseException):
            for m, _, doc in console._members(obj):
                if "C++" not in doc:
                    missing.append(f"{e.qualified}.{m}")
    assert missing == []


def test_one_module_is_listed_alone_and_an_unknown_one_is_a_key_error():
    assert {e.module for e in console.inventory("stats")} == {"stats"}
    with pytest.raises(KeyError):
        console.inventory("nonexistent")


def test_kinds_are_class_function_and_exception():
    by_name = {e.qualified: e for e in console.inventory()}
    assert by_name["stats.Wyrand"].kind == "class"
    assert by_name["game.shapley"].kind == "function"
    assert by_name["errors.PreconditionError"].kind == "exception"


def test_signatures_list_every_overload_with_defaults_filled_in():
    by_name = {e.qualified: e for e in console.inventory()}
    assert len(by_name["game.shapley"].signatures) == 3
    assert by_name["stats.Wyrand"].signatures == ("Wyrand(seed: int = 0) -> None",)
    assert not any("\\0" in s for e in by_name.values() for s in e.signatures)


def test_cpp_finds_the_python_name_of_a_cpp_function_and_not_its_longer_relatives():
    assert [e.qualified for e in console.cpp("snicholls::game::shapley")] == ["game.shapley"]
    assert "game.shapley_sampled" in [e.qualified for e in console.cpp("snicholls::game::shapley_sampled")]
    assert [e.qualified for e in console.cpp("snicholls::stats::wyrand")] == ["stats.Wyrand"]
    assert console.cpp("snicholls::no_such_thing") == []


def test_describe_takes_a_qualified_name_a_bare_name_a_cpp_name_or_the_object():
    by_object = console.describe(game.shapley)
    assert console.describe("game.shapley") == by_object
    assert console.describe("shapley") == by_object
    assert console.describe("snicholls::game::shapley") == by_object
    assert "C++: snicholls::game::shapley" in by_object
    assert by_object.count("shapley(") >= 3


def test_describe_of_a_class_lists_its_methods_and_properties():
    text = console.describe(game.Attribution)
    for member in ("values", "total", "baseline", "attributed", "unattributed"):
        assert f"  {member} " in text


def test_describe_of_an_instance_shows_its_current_values():
    a = game.shapley(np.array([0.0, 1.0, 1.0, 4.0]))
    text = console.describe(a)
    assert "Current values:" in text and "total = 4.0" in text


def test_describe_of_an_unknown_name_is_a_key_error():
    with pytest.raises(KeyError):
        console.describe("game.no_such_function")
    with pytest.raises(KeyError):
        console.describe("snicholls::no_such_thing")


def test_fields_are_the_properties_and_equal_reading_them_directly():
    a = game.shapley(np.array([0.0, 1.0, 2.0, 4.0]))   # v({0})=1, v({1})=2, v({0,1})=4: Shapley (1.5, 2.5)
    f = console.fields(a)
    assert set(f) == {"baseline", "evaluations", "players", "samples", "standard_errors", "total", "values"}
    assert f["total"] == a.total == 4.0
    np.testing.assert_array_equal(f["values"], [1.5, 2.5])


def test_fields_refuses_an_object_that_is_not_bound_here():
    with pytest.raises(TypeError):
        console.fields(3.0)


def test_the_tour_is_right_and_prints_each_step():
    out = io.StringIO()
    assert console.tour(out) is True
    assert "tour: ok" in out.getvalue()


def test_the_namespace_holds_the_modules_and_helpers():
    ns = console.namespace()
    assert ns["game"] is game and ns["stats"] is stats and ns["errors"] is errors
    for helper in ("inventory", "describe", "fields", "cpp", "tour"):
        assert callable(ns[helper])


def run_module(*args, stdin=""):
    env = dict(os.environ)
    env["PYTHONPATH"] = os.pathsep.join(p for p in [os.path.dirname(os.path.dirname(snicholls_core.__file__)),
                                                    env.get("PYTHONPATH", "")] if p)
    return subprocess.run([sys.executable, "-m", "snicholls_core", *args], input=stdin, capture_output=True,
                          text=True, env=env, timeout=120)


def test_python_m_snicholls_core_list_prints_every_name():
    r = run_module("--list")
    assert r.returncode == 0, r.stderr
    for e in console.inventory():
        assert e.qualified in r.stdout


def test_python_m_snicholls_core_describe_and_an_unknown_name():
    r = run_module("--describe", "snicholls::stats::wyrand")
    assert r.returncode == 0 and "Wyrand(seed: int = 0)" in r.stdout
    r = run_module("--describe", "nothing_here")
    assert r.returncode == 2 and "nothing_here" in r.stderr


def test_python_m_snicholls_core_tour_exits_zero():
    r = run_module("--tour")
    assert r.returncode == 0 and "tour: ok" in r.stdout


def test_the_interactive_console_runs_what_it_is_given():
    r = run_module(stdin="print(fields(game.shapley(np.array([0.0, 2.0])))['total'])\n"
                         "print(cpp('snicholls::game::owen')[0].qualified)\n")
    assert r.returncode == 0, r.stderr
    assert "2.0" in r.stdout and "game.owen" in r.stdout
    assert "snicholls_core " + snicholls_core.__version__ in r.stderr   # the banner
