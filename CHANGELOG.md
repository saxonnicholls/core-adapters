# Changelog

All notable changes to core-adapters. The format follows Keep a Changelog; versions follow semantic versioning, and the
C++ headers, the CMake project and the Python package share one version (`scripts/check_version.py`).

## [Unreleased]

## [0.1.1] - 2026-11-17

### Changed

- `third_party/core` pinned to snicholls-core `v0.9.0`; `snicholls_core_require(0.5.1)` stays, since nothing newer is
  used.

### Added

- `snicholls_core.console` and `python -m snicholls_core`: an interactive console over the package, with
  `inventory()`, `describe(x)` (by Python or C++ name), `fields(obj)`, `cpp(name)` and `tour()`. It reads what the
  modules bind, so anything bound later appears in it unchanged.
- The Doxygen documentation is published to GitHub Pages (<https://saxonnicholls.github.io/core-adapters/>) by CI:
  `main` to `/main/`, each release tag to `/vX.Y.Z/`, and the root to the newest release. The README carries the CI
  and documentation badges, as core's does.

### Fixed

- Every docstring of the package ends with its C++ counterpart (`Memoized`, `SetGame`, `set_game`, the
  `Attribution` properties and the `errors` classes did not), and a test keeps it so.

## [0.1.0] - 2026-11-16

### Changed

- `third_party/core` pinned to snicholls-core `v0.8.0` (the public repository, via `v0.5.1` and `v0.7.0`);
  `snicholls_core_require(0.5.1)` stays, since nothing newer is used.
- The GIL rule is hard (CLAUDE.md): GIL builds of CPython only; a call that would wait on C++ returns a future; Python
  never runs on a C++ thread and a C++ thread never waits for the GIL.
- `snicholls_core.game.interactions` calls core's `shapley_interactions<N>` for every N again: the N < 2 workaround is
  gone with the fix in core v0.5.1 (erratum/CORE.md #1, #2 resolved).

### Added

- Pins: snicholls-core (`third_party/core`, with `snicholls_core_require` and
  `snicholls_core_assert_single_version()`), nanobind `v3.1.0`, nlohmann/json `v3.12.0`; GoogleTest and Google Benchmark
  are core's pinned copies.
- `snicholls::core_nanobind` (`<snicholls/core_adapters/nanobind/...>`): `gil.hpp` (`release_gil`, `without_gil`,
  `with_gil`, `python_function`), `errors.hpp` (`register_exception_translators`, `require`, `raise_error`, `unwrap`,
  `set_python_error`, `precondition_error`, `traced_exception`), `ndarray.hpp` (`vector_in`, `matrix_in`,
  `vector_inout`, `as_span`, `to_numpy`, and the documented template), `bytes.hpp` (`byte_view`, `mutable_byte_view`,
  `text_view`, `to_bytes`), `optional.hpp` (`std::optional` both ways, `snicholls::result` as value-or-raise).
- The Python package `snicholls_core` (abi3, CPython 3.12+, stubs shipped): `game` (Shapley exact and sampled,
  Banzhaf, Harsanyi dividends, Owen, the lifts sampler, `memoized`, change attribution, interactions, the properties
  and the core, coalition helpers, `set_game`), `stats` (`Wyrand` and the draws), `errors` (the shared hierarchy).
- `snicholls::core_json` (`<snicholls/core_adapters/nlohmann/...>`): `json_error` and hooks for `utc<D>` (canonical RFC
  3339, strict), `uuid`, `decimal` (exact text; JSON numbers refused) and `secret` (read only), with their tests and
  demos.
- `scripts/gate.sh`, the offline gate; `scripts/test_ubuntu.sh`; the tabular Shapley benchmark (BENCHMARKS.md).
