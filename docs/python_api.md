# Python API and its C++ counterparts {#python_api}

Every name of the `snicholls_core` package and the C++ it calls. The reverse direction is in the stubs: each
docstring (and so each `.pyi` entry) ends with `C++: ...`.

## snicholls_core.game (python/src/game.cpp)

| Python | C++ |
|---|---|
| `shapley(table)`, `shapley(v, n)` | `snicholls::game::shapley(n, v)` on a `table_game<double>` (a callable is tabulated first) |
| `banzhaf(table)`, `banzhaf(v, n)` | `snicholls::game::to_dividends`, then `banzhaf_from_dividends` (equal to `banzhaf<N>`) |
| `harsanyi_dividends(table)`, `harsanyi_dividends(v, n)` | `snicholls::game::to_dividends` / `harsanyi_dividends(n, v)` |
| `shapley_from_dividends(d)`, `banzhaf_from_dividends(d)` | the functions of the same names |
| `owen(table, groups)`, `owen(v, n, groups)` | `snicholls::game::owen(n, v, groups)`; groups as lists of players |
| `shapley_sampled(table, g, permutations=256, antithetic=True)`, `shapley_sampled(v, n, g, ...)` | `snicholls::game::shapley_sampled(n, v, g, permutation_sampling{...})` |
| `shapley_sampled_lifts(table, g, samples_per_player=256, scale_to_total=True)`, `(v, n, g, ...)` | `snicholls::game::shapley_sampled_lifts(n, v, g, lift_sampling{...})` |
| `memoized(v)` -> `Memoized` (`evaluations`) | `snicholls::game::memoized<V>` |
| `attribute_change(f, start, end)` | `snicholls::game::attribute_change(f, from, to)` |
| `interactions(table)`, `interactions(v, n)` | `snicholls::game::shapley_interactions<N>` (n <= 20) |
| `is_monotone`, `is_superadditive`, `is_supermodular`, `is_submodular`, `is_additive` | the `is_*<N>` of `game/properties.hpp` (n <= 20) |
| `find_monotonicity_violation`, `find_superadditivity_violation`, `find_supermodularity_violation`, `find_submodularity_violation`, `find_interaction` | the `find_*<N>`; a violation is a dict of the struct's fields |
| `in_core(table, allocation)`, `find_core_violation(...)` | `snicholls::game::in_core<N>`, `find_core_violation<N>` |
| `Attribution` (`baseline`, `total`, `values`, `standard_errors`, `samples`, `evaluations`, `players`, `attributed()`, `unattributed()`) | `snicholls::game::attribution` / `sampled_attribution` |
| `tabulate(v, n)` | `snicholls::game::tabulate(n, v)` |
| `singleton`, `grand_coalition`, `contains`, `with_player`, `without`, `coalition_size`, `table_size`, `max_table_players` | the functions of `game/coalition.hpp` (`with` is a Python keyword) |
| `members(s)`, `from_members(players)`, `set_game(f)` -> `SetGame` | Python only: bit iteration, `with` repeatedly, a frozenset adapter |

## snicholls_core.stats (python/src/stats.cpp)

| Python | C++ |
|---|---|
| `Wyrand(seed=0)`, `Wyrand.stream(seed, k)`, `g()`, `==`, `copy.copy` | `snicholls::stats::wyrand`, `wyrand::stream`, `operator()`, `operator==` |
| `random_u64(g[, count])` | `snicholls::stats::random_u64` |
| `uniform_index(g, n[, count])` | `snicholls::stats::uniform_index` |
| `uniform_real(g[, count])` | `snicholls::stats::uniform_real` |
| `shuffle(array, g)` (int64, uint64 or float64, in place) | `snicholls::stats::shuffle(std::span<T>, g)` |
| `permutation(n, g)` | `std::iota`, then `snicholls::stats::shuffle` |

## snicholls_core.errors (python/snicholls_core/errors.py)

| Python | C++ |
|---|---|
| `CoreError` | the base of the classes below |
| `ResultError` | `snicholls::bad_result_access<E>`; any non-exception error raised by `snicholls::python::raise_error` |
| `TracedError` | `snicholls::python::traced_exception`, from a `snicholls::traced<E>` error |
| `PreconditionError` (a `ValueError`) | `snicholls::python::precondition_error` (`snicholls::python::require`) |
