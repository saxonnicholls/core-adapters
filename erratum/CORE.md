# snicholls-core errata

Defects, gaps and integration gotchas found in snicholls-core while building these adapters. Each workaround in this
repository names its entry. When a fixed core is pinned, remove the workaround and mark the entry resolved.

## 1. `shapley_interactions<N>` does not compile under `-Werror` for N < 2 (resolved in v0.5.1)

- **Where:** `include/snicholls/core/game/shapley.hpp`, `shapley_interactions<N>` (v0.4.0): the lambda passed to
  `detail::with_table` captures `out` by reference, and its body uses `out` only inside `if constexpr (N >= 2)`.
- **Evidence:** instantiating `shapley_interactions<0>` or `<1>` with Clang 17 / AppleClang under `-Wall -Wextra`
  gives `error: lambda capture 'out' is not used [-Werror,-Wunused-lambda-capture]`.
- **Impact:** any consumer that dispatches a run-time player count onto `shapley_interactions<N>` from 0 (as the
  binding does) fails to build with warnings as errors.
- **Proposed fix:** capture `[&]`, or move the `if constexpr` outside the lambda, or add
  `static_cast<void>(out);` in the `N < 2` branch.
- **Resolved:** v0.5.1 drops the `if constexpr`, so the capture is always used; `shapley_interactions<0>` and `<1>`
  compile cleanly with warnings as errors. The workaround in `python/src/game.cpp` (`interactions`) is removed.

## 2. No published known-answer sequences for `stats::wyrand` (resolved in v0.5.1)

- **Where:** `tests/stats/random_test.cpp` (v0.4.0) checks reproducibility, bounds and uniformity, but pins no draw
  values, although `random.hpp` documents the draws as specified to the bit.
- **Impact:** nothing in core would catch a change to a draw; a binding cannot be checked against core's own record.
- **Proposed fix:** pin a few draws per seed, per `stream`, per `uniform_index` (including a rejection), per
  `uniform_real` and a shuffle in core's tests.
- **Workaround here:** `tests/fixtures/wyrand_sequences.json`, computed independently in Python integers by
  `scripts/generate_random_fixture.py` from wyhash's published wyrand; core's C++ (`tests/support/core_random_fixture.cpp`)
  and the binding (`tests/python/stats_test.py`) are both checked against it.
- **Resolved:** v0.5.1 pins wyrand draws in `tests/stats/random_test.cpp` (computed independently from wyhash 4.2's
  published algorithm); its sequences for seeds 0 and 42 equal this repository's fixture bit for bit. The fixture stays
  as this repository's own check of the binding.
