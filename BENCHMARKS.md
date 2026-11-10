# Benchmarks

Written by `scripts/run_benchmarks.sh` (`benchmarks/tabular_shapley_benchmark.py`); never edited by hand.

## Tabular Shapley: Python call against C++

- Date 2026-11-10, Darwin 25.6.0 x86_64, 32 CPUs, Release build, Python 3.12.14, snicholls_core 0.1.0.
- C++: `snicholls::game::shapley(n, table_game)` (Google Benchmark, median of 5). Python: `snicholls_core.game.shapley(table)` on the same float64 table (median of 5 timed loops). Fairness: the values are equal bit for bit for every n, checked before timing.
- Overhead = Python - C++: argument conversion (a zero-copy view of the array), releasing and re-taking the GIL, and building the Attribution (one allocation and copy of the n values).

| players n | coalitions | C++ | Python | overhead | overhead / Python | CV C++ | CV Python |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 2 | 74 ns | 893 ns | 819 ns | 91.7% | 1.9% | 1.6% |
| 4 | 16 | 175 ns | 1,014 ns | 839 ns | 82.7% | 0.6% | 1.2% |
| 8 | 256 | 1,757 ns | 2,489 ns | 732 ns | 29.4% | 3.5% | 2.1% |
| 12 | 4,096 | 33,345 ns | 31,462 ns | -1,883 ns | -6.0% | 5.1% | 0.6% |
| 16 | 65,536 | 0.668 ms | 0.632 ms | -36,522 ns | -5.8% | 0.2% | 1.0% |
| 20 | 1,048,576 | 12.886 ms | 12.640 ms | -245,886 ns | -1.9% | 1.7% | 0.5% |

The fixed cost of a call is what the small-n rows show. At large n the arithmetic dominates and the two are separately compiled copies of the same template (the module with nanobind's flags, built NOMINSIZE): a difference of a few percent either way there is code layout, not overhead.

Callable route, for scale: `game.shapley(v, 10)` with a Python `v` costs 85,334 ns, about 83 ns per evaluation of v (2^10 calls with the GIL held). Pass a table instead whenever v can be computed in bulk.
