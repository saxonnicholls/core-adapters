# nanobind errata and integration notes

## 1. Modules are optimised for size by default (note)

- **Where:** `nanobind_add_module` (v3.1.0) adds `-Os` unless `NOMINSIZE` is given.
- **Evidence:** tabular Shapley at n = 20 took 17.3 ms from Python against 13.8 ms in C++ (Release, macOS, 2026-11-10);
  with `NOMINSIZE`, 12.4 ms against 13.3 ms (BENCHMARKS.md).
- **Here:** every numeric module is built `NOMINSIZE` (`python/CMakeLists.txt`); consumers of `snicholls::core_nanobind`
  should do the same.

## 2. Getters that return an owning ndarray need an explicit policy (note)

- **Where:** `def_prop_ro` uses `rv_policy::reference_internal`; returning an ndarray that already owns its data then
  fails at run time: `ndarray_export(): reference_internal policy cannot be applied (ndarray already has an owner)`.
- **Here:** `Attribution.values` passes `nb::rv_policy::move`.

## 3. Stubgen treats a trailing underscore as private (note)

- **Where:** `stubgen.py` omits names with a single leading *or trailing* underscore unless `--include-private`.
- **Here:** C++ `snicholls::game::with` is `with_player` in Python, not `with_`.

## 4. `PyGILState_Check` and `PyObject_CallOneArg` are outside the 3.12 stable ABI (note)

- **Here:** the helpers call `PyObject_CallFunctionObjArgs`, and the GIL tests observe Python threads making progress
  instead of asking the interpreter whether the GIL is held.
