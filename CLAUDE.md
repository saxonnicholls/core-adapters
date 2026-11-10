# core-adapters

The third-party adapters of snicholls-core. Core (`third_party/core`, namespace `snicholls`, include root
`<snicholls/core/...>`) is header-only C++20 on the standard library alone and never takes a dependency; whatever joins
it to a third-party library lives here, behind targets of its own:

- `snicholls::core_nanobind` - nanobind helpers, `include/snicholls/core_adapters/nanobind/` (`namespace snicholls::python`);
- `snicholls::core_json` - nlohmann/json hooks for core's value types, `include/snicholls/core_adapters/nlohmann/`
  (in `namespace snicholls`, where argument-dependent lookup finds them);
- the Python package `snicholls_core` - bindings of core's `game/` and `stats/random` (`python/`).

This repository is public, so everything committed - files and commit messages - is written for anyone to read: no
names of private repositories, no local absolute paths, no quotations of people or dated personal decisions; reference
values from elsewhere are kept as anonymous fixtures.

## Authorship and commits

Commits are authored by the repository owner and carry **no AI attribution**: no `Co-Authored-By` trailers, no
"Generated with" lines, in commit messages, pull request descriptions, file headers, README, CHANGELOG or this file.
Every source file starts with the banner

    //
    //  <name>.hpp
    //  core-adapters
    //
    //  Copyright 2010-2026 Saxon Herschel Nicholls
    //

(`#` instead of `//` in Python and shell). Commits are small, one area each.

## One core for the whole family (MUST)

- `third_party/core` is a submodule pinned to a core **release tag**, https URL, shallow. Never a branch tip, a copy or
  `FetchContent`.
- `CMakeLists.txt` adds it with `add_subdirectory(... EXCLUDE_FROM_ALL)`, then calls `snicholls_core_require(<minimum>)`;
  as the top-level project it ends with `snicholls_core_assert_single_version()`. As a subdirectory of a consumer, the
  consumer's core wins (core skips the second copy) and the consumer's last line asserts one version.
- Raise `snicholls_core_require` only when the code here uses something new. Nothing else in the tree names the core
  version, so a pin bump is: `git -C third_party/core fetch --depth 1 origin tag vX.Y.Z && git -C third_party/core checkout
  vX.Y.Z`, update the pin in README.md, run the gate, commit as `third_party: bump core vA.B.C -> vX.Y.Z`.
- The family's sweep runs `scripts/gate.sh` before bumping this repository's pin.

## What belongs here

An adapter between core and one third-party library, with its own CMake target, used by more than one repository of
the family. Anything that needs no third-party library belongs in core instead; anything specific to one consumer stays
in that consumer. Dependencies never leak into core: core's headers are included here, never the reverse.

## Layout

- `include/snicholls/core_adapters/` - `version.hpp`; `nanobind/` (`gil`, `errors`, `ndarray`, `bytes`, `optional`);
  `nlohmann/` (`json`, `time_json`, `uuid_json`, `decimal_json`, `secret_json`). Header guards
  `snicholls_core_adapters_<path>_hpp`; macros `SNICHOLLS_CORE_ADAPTERS_*`; CMake options `CORE_ADAPTERS_*`.
- `python/` - `snicholls_core/` (pure Python: `__init__.py`, `errors.py`, `py.typed`), `src/<module>.cpp` (one
  nanobind module each: `stats`, `game`), `CMakeLists.txt` (modules `snicholls_core_<module>`, stubs).
- `tests/`, `demos/` mirror the headers and the package (below); `benchmarks/`; `scripts/`; `docs/`; `erratum/`;
  `docker/ubuntu/`; `requirements/gate.txt`; `third_party/` (submodules only).

## Python package rules

- nanobind + scikit-build-core (`pyproject.toml`), **abi3 for CPython 3.12+** (`STABLE_ABI`, `wheel.py-api = "cp312"`):
  one wheel for every later version, tested on each by the gate. Modules are `NB_STATIC NOMINSIZE`.
- Stubs come from nanobind's stubgen at build time (`nanobind_add_stub`) and ship in the wheel; never hand-written.
- **Python names are core's names** (`shapley`, `owen`, `uniform_index`); only a keyword clash changes one
  (`with` -> `with_player`; stubgen hides a trailing underscore). Classes are CamelCase (`Wyrand`, `Attribution`).
  Every docstring ends with `C++: <the function it calls>`, and `docs/python_api.md` maps every name both ways.
- **The GIL:** release it for every C++ computation on plain data (`release_gil()` or `without_gil`), never while
  touching a Python object. A Python callable needs the GIL per call: prefer tables (NumPy arrays) for anything
  evaluated many times - the exact game methods tabulate a callable once, then compute GIL-free.
- **Errors:** every module calls `snicholls::python::register_exception_translators()`; preconditions core only asserts
  are checked with `snicholls::python::require` (`PreconditionError`, a `ValueError`). No C++ exception escapes as
  anything but the documented class (`errors.hpp` table).
- **Determinism:** `stats.Wyrand` and everything drawn from it are bit-for-bit core's; tests compare with the
  independent fixture and with core's C++ output (`tests/support/reference_values.cpp`).

## JSON adapter rules

nlohmann/json is used only under `include/snicholls/core_adapters/nlohmann/`, through `snicholls::core_json`, built
with `JSON_ImplicitConversions=OFF`, includes not SYSTEM. **No nlohmann exception escapes an adapter**: each throws
`snicholls::json_error`, and each has a test over every kind of JSON value proving it. Wire forms: `utc` canonical RFC
3339 (strict in), `uuid` canonical lowercase, `decimal` exact canonical text as a JSON string (numbers refused),
`secret` read only and never quoted in an error.

## Tests (MUST)

Every public entity has tests. GoogleTest (core's pinned `third_party/core/third_party/googletest`) for C++; pytest for
Python.

- Mirror: `<sub>/<name>.hpp` -> `tests/<sub>/<name>_test.cpp`; for `nanobind/` headers that file is a **test module**
  (`<name>_module`) driven by `tests/nanobind/<name>_test.py`. Package modules -> `tests/python/<module>_test.py`.
  `scripts/check_mirror.py` (CTest `check.mirror`) enforces it, both ways.
- Test names `TEST(<Unit><Aspect>, <BehaviourInPlainWords>)`; pytest functions `test_<behaviour_in_plain_words>`.
- Tests check against closed forms, independent implementations and core's own C++ output - never against the
  binding's own earlier output.
- Fixtures: `tests/fixtures/wyrand_sequences.json` is generated (`scripts/generate_random_fixture.py`, CTest
  `generated.random_fixture_up_to_date`); `tests/fixtures/game_reference.json` holds reference values for games from an
  independent Python implementation. Never edit either by hand.
- CTest labels: `unit`, `demo`, `boundary`, `docs`, `benchmark`.

## Demos (MUST)

Every public header and package module has a demo that runs every public function and exits non-zero if a result is
not what it prints. `<sub>/<name>.hpp` -> `demos/<sub>/<name>_demo.cpp` (for `nanobind/`: a demo module plus
`<name>_demo.py`); package modules -> `demos/python/<module>_demo.py`. Each has a start script,
`scripts/demos/<name>_demo.sh` (`python_<module>_demo.sh` for the package), using `scripts/demos/_common.sh`. Regions
`//! [tag]` are the `\snippet`s of the documentation. `demos/nanobind/ndarray_demo.cpp` (region `template`) is the
template for exposing a header-only function over NumPy arrays.

## Documentation (MUST) - Doxygen

Core's standard, word for word (core's CLAUDE.md "Documentation"): every public C++ entity, in the order brief,
overview, relationships, implementations, guarantees, errors, examples (`\snippet` from a compiled demo), performance
(numbers or "not benchmarked"), tests and demos, dependencies, known issues, status. Warnings are errors (CTest
`docs.build` in the gate's Release build). Python is documented in docstrings, which reach the stubs.

## Benchmarks

`scripts/run_benchmarks.sh` builds Release and rewrites `BENCHMARKS.md` (never edited by hand). The tabular Shapley
benchmark compares the Python call with the same C++ call on the same table, refusing to measure unless the two agree
bit for bit; the smoke run is a CTest (label `benchmark`). Only Release numbers mean anything.

## The gate (MUST)

`scripts/gate.sh` - non-interactive, offline, `-j4`, non-zero on any failure:

1. a development venv (Python 3.12) with numpy and pytest from the wheelhouse;
2. Debug and Release into `build-family/<config>`, warnings as errors, every CTest (Release adds the benchmark smoke
   run and the Doxygen check);
3. `pip install .` into a clean venv and pytest against the installed package;
4. the same abi3 wheel into a clean venv of every newer CPython found, and pytest.

The wheelhouse (`$CORE_ADAPTERS_WHEELHOUSE`, default `~/.cache/core-adapters/wheelhouse`) is filled once by
`scripts/fetch_wheels.sh` from `requirements/gate.txt` - the only networked step. Ubuntu: `scripts/test_ubuntu.sh`
(core's image approach: `docker/ubuntu/Dockerfile`, GCC 13 and Clang 18, a wheelhouse baked into the image; the gate
under GCC, then Clang Release), niced, on 4 CPUs. A change is done when the gate passes on macOS and on Ubuntu. CI
(`.github/workflows/ci.yml`) runs the same gate when GitHub Actions is available; until then the local runs are the
gate.

## Dependencies

| Dependency | Path | Pinned to | Used by |
|---|---|---|---|
| snicholls-core | `third_party/core` | `v0.5.1` | everything |
| nanobind | `third_party/nanobind` | `v3.1.0` | `snicholls::core_nanobind` consumers, the package |
| nlohmann/json | `third_party/nlohmann_json` | `v3.12.0` | `snicholls::core_json`, the C++ tests |
| GoogleTest, Google Benchmark | core's `third_party/` | `v1.18.0`, `v1.9.5` | tests, benchmarks |

Rules (core's): pin a release tag, https, `shallow = true`, never edit inside a submodule, `add_subdirectory(...
EXCLUDE_FROM_ALL)` with the dependency's tests and install off, never SYSTEM includes, one bump per commit
(`third_party: bump <name> <old> -> <new>`). Every defect or gotcha found in a dependency is recorded in
`erratum/<LIBRARY>.md` with evidence, impact, proposed fix and the workaround; workarounds name their entry.
