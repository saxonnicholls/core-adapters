# nlohmann/json errata

## 1. Numeric `get<T>()` narrows without a range check (by design upstream; open here)

- **Where:** nlohmann/json v3.12.0, `from_json` for arithmetic types: a JSON integer is converted with `static_cast`.
- **Evidence:** `nlohmann::json(70000).get<std::uint16_t>()` is 4464; no exception.
- **Impact:** `secret_json.hpp` reads `secret<T>` through `j.get_to(value)`, so a secret of a narrow integer type
  silently takes the wrapped value. The test `CoreJsonSecret.OutOfRangeNumbersWrapAsInNlohmann` records the behaviour.
- **Workaround:** read a wider type and check the range in the caller. A range-checked numeric adapter would be an
  addition to `snicholls::core_json`, not a change to nlohmann.
