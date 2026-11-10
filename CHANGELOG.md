# Changelog

All notable changes to core-adapters. The format follows Keep a Changelog; versions follow semantic versioning, and the
C++ headers, the CMake project and the Python package share one version (`scripts/check_version.py`).

## [Unreleased] - 0.1.0

### Added

- Pins: snicholls-core `v0.4.0` (`third_party/core`, with `snicholls_core_require(0.4.0)` and
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
