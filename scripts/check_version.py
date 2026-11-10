#!/usr/bin/env python3
#
#  check_version.py
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
#  The version is written down in four places: the macros in
#  include/snicholls/core_adapters/version.hpp, project(... VERSION) in
#  CMakeLists.txt, version in pyproject.toml and __version__ in the Python
#  package. They are one fact; this fails if any two disagree (and with
#  --tag, if the release tag disagrees too). The last two are checked once the
#  Python package exists.
#
#      python3 scripts/check_version.py [--tag v0.1.0]
#

import argparse
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def read(rel):
    path = os.path.join(ROOT, rel)
    return open(path, encoding="utf-8").read() if os.path.exists(path) else None


def found(rel, pattern):
    src = read(rel)
    if src is None:
        return None
    m = re.search(pattern, src, re.M | re.S)
    if not m:
        sys.exit(f"check_version: no version found in {rel}")
    return ".".join(g for g in m.groups())


def main():
    ap = argparse.ArgumentParser(description="Check that the version is the same everywhere")
    ap.add_argument("--tag", default=os.environ.get("RELEASE_TAG"), help="a release tag to check too")
    args = ap.parse_args()

    versions = {
        "version.hpp": found("include/snicholls/core_adapters/version.hpp",
                             r"SNICHOLLS_CORE_ADAPTERS_VERSION_MAJOR\s+(\d+)\s*$.*?"
                             r"SNICHOLLS_CORE_ADAPTERS_VERSION_MINOR\s+(\d+)\s*$.*?"
                             r"SNICHOLLS_CORE_ADAPTERS_VERSION_PATCH\s+(\d+)\s*$"),
        "CMakeLists.txt": found("CMakeLists.txt", r"project\s*\(\s*snicholls_core_adapters\s+VERSION\s+(\d+\.\d+\.\d+)"),
        "pyproject.toml": found("pyproject.toml", r'^version\s*=\s*"(\d+\.\d+\.\d+)"'),
        "python/snicholls_core/__init__.py": found("python/snicholls_core/__init__.py",
                                                   r'^__version__\s*=\s*"(\d+\.\d+\.\d+)"'),
    }
    if args.tag:
        versions["tag " + args.tag] = args.tag.lstrip("v")
    present = {k: v for k, v in versions.items() if v is not None}
    for k, v in present.items():
        print(f"  {k:36} {v}")
    if len(set(present.values())) != 1:
        print("check_version: the versions disagree", file=sys.stderr)
        return 1
    print(f"check_version: ok ({next(iter(present.values()))})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
