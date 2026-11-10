#!/usr/bin/env python3
#
#  check_mirror.py
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
#  tests/, demos/ and scripts/demos/ mirror include/snicholls/core_adapters/
#  and the Python package (CLAUDE.md "Tests", "Demos"). For every public header
#  <sub>/<name>.hpp:
#
#    tests/<sub>/<name>_test.cpp     listed in a tests/ CMakeLists.txt
#    tests/<sub>/<name>_test.py      as well, for a nanobind/ header (the .cpp
#                                    is then the test module it drives)
#    demos/<sub>/<name>_demo.cpp     listed in a demos/ CMakeLists.txt
#    demos/<sub>/<name>_demo.py      as well, for a nanobind/ header (the .cpp
#                                    is then the demo module it drives)
#    scripts/demos/<name>_demo.sh    its start script
#
#  and for every module of the snicholls_core package (python/src/<m>.cpp
#  or python/snicholls_core/<m>.py): tests/python/<m>_test.py,
#  demos/python/<m>_demo.py and scripts/demos/python_<m>_demo.sh. A test or demo with
#  no header or module is an error too. Exit status 1 on any finding.
#

import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INCLUDE = os.path.join(ROOT, "include", "snicholls", "core_adapters")


def cmake_text(tree):
    text = ""
    for dirpath, _, files in os.walk(os.path.join(ROOT, tree)):
        if "CMakeLists.txt" in files:
            text += open(os.path.join(dirpath, "CMakeLists.txt"), encoding="utf-8").read()
    return text


def exists(rel):
    return os.path.exists(os.path.join(ROOT, rel))


def main():
    problems = []
    tests_cmake, demos_cmake = cmake_text("tests"), cmake_text("demos")
    expected_tests, expected_demos = set(), set()

    for dirpath, _, files in os.walk(INCLUDE):
        for f in sorted(files):
            if not f.endswith(".hpp"):
                continue
            rel = os.path.relpath(os.path.join(dirpath, f), INCLUDE)[:-4]
            if rel.split(os.sep)[0] == "detail" or "/detail/" in rel:
                continue
            name = os.path.basename(rel)
            test, demo = f"{rel}_test.cpp", f"{rel}_demo.cpp"
            expected_tests.add(test)
            expected_demos.add(demo)
            if not exists("tests/" + test):
                problems.append(f"{rel}.hpp: no tests/{test}")
            elif test not in tests_cmake:
                problems.append(f"tests/{test} is not listed in a tests/ CMakeLists.txt")
            if rel.startswith("nanobind" + os.sep):
                expected_demos.add(f"{rel}_demo.py")
                if not exists(f"demos/{rel}_demo.py"):
                    problems.append(f"{rel}.hpp: no demos/{rel}_demo.py (the driver of its demo module)")
                expected_tests.add(f"{rel}_test.py")
                if not exists(f"tests/{rel}_test.py"):
                    problems.append(f"{rel}.hpp: no tests/{rel}_test.py (the pytest file of its test module)")
            if not exists("demos/" + demo):
                problems.append(f"{rel}.hpp: no demos/{demo}")
            elif demo not in demos_cmake:
                problems.append(f"demos/{demo} is not listed in a demos/ CMakeLists.txt")
            if not exists(f"scripts/demos/{name}_demo.sh"):
                problems.append(f"{rel}.hpp: no scripts/demos/{name}_demo.sh")

    modules = set()
    if exists("python/src"):
        modules |= {f[:-4] for f in os.listdir(os.path.join(ROOT, "python/src")) if f.endswith(".cpp")}
    if exists("python/snicholls_core"):
        modules |= {f[:-3] for f in os.listdir(os.path.join(ROOT, "python/snicholls_core"))
                    if f.endswith(".py") and not f.startswith("_")}
    for m in sorted(modules):
        for rel in (f"tests/python/{m}_test.py", f"demos/python/{m}_demo.py", f"scripts/demos/python_{m}_demo.sh"):
            if not exists(rel):
                problems.append(f"snicholls_core.{m}: no {rel}")
        expected_tests.add(f"python/{m}_test.py")
        expected_demos.add(f"python/{m}_demo.py")

    for tree, expected, suffixes in (("tests", expected_tests, ("_test.cpp", "_test.py")),
                                     ("demos", expected_demos, ("_demo.cpp", "_demo.py"))):
        for dirpath, _, files in os.walk(os.path.join(ROOT, tree)):
            for f in files:
                if f.endswith(suffixes):
                    rel = os.path.relpath(os.path.join(dirpath, f), os.path.join(ROOT, tree))
                    if rel not in expected:
                        problems.append(f"{tree}/{rel} has no header or module")

    for p in problems:
        print("check_mirror: " + p, file=sys.stderr)
    if not problems:
        print(f"check_mirror: ok ({len(expected_tests)} tests, {len(expected_demos)} demos)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
