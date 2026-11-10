#!/usr/bin/env python3
#
#  tabular_shapley_benchmark.py
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
"""Tabular Shapley: the Python call against C++, and so the binding's call overhead.

    tabular_shapley_benchmark.py --cpp <tabular_shapley_benchmark binary> [--out BENCHMARKS.md] [--smoke]

Fairness first: for every n, the binding's values must equal the C++ program's bit for bit (same table, same
arithmetic), or nothing is measured. Then, per n: the C++ time (Google Benchmark, 5 repetitions, median) and the
Python time of snicholls_core.game.shapley(table) (5 repetitions of a timed loop, median), their difference - the
call overhead - and the coefficient of variation of each. A last row times the callable route per evaluation (a Python
game, tabulated by the binding), for scale. Only Release numbers mean anything.
"""

import argparse
import datetime
import json
import platform
import statistics
import subprocess
import sys
import time

import numpy as np
from snicholls_core import __version__, game, stats

SIZES = (1, 4, 8, 12, 16, 20)


def table(n):
    return stats.uniform_real(stats.Wyrand(2026), 2 ** n)


def fairness(cpp):
    for n in SIZES:
        expected = json.loads(subprocess.run([cpp, "--values", str(n)], check=True, capture_output=True,
                                             text=True).stdout)
        got = game.shapley(table(n)).values.tolist()
        if got != expected:
            sys.exit(f"fairness check failed for n = {n}: the binding and C++ disagree; nothing measured")
    print(f"fairness: the binding equals C++ bit for bit for n in {SIZES}")


def cpp_times(cpp, smoke):
    args = [cpp, "--benchmark_format=json"]
    args += ["--benchmark_min_time=0.01s"] if smoke else ["--benchmark_repetitions=5", "--benchmark_min_time=0.2s"]
    out = json.loads(subprocess.run(args, check=True, capture_output=True, text=True).stdout)
    times, cvs = {}, {}
    for b in out["benchmarks"]:
        n = int(b["run_name"].split("/")[1])
        if smoke and b.get("run_type") == "iteration":
            times[n] = b["real_time"] if b["time_unit"] == "ns" else b["real_time"] * {"us": 1e3, "ms": 1e6}[b["time_unit"]]
        elif b.get("aggregate_name") == "median":
            times[n] = b["real_time"] * {"ns": 1, "us": 1e3, "ms": 1e6}[b["time_unit"]]
        elif b.get("aggregate_name") == "cv":
            cvs[n] = b["real_time"]
    return times, cvs, out["context"]


def py_time(f, smoke):
    """Median ns per call over 5 repetitions of a loop of at least 0.2 s (0.01 s for a smoke run)."""
    target = 0.01 if smoke else 0.2
    k = 1
    while True:
        t0 = time.perf_counter_ns()
        for _ in range(k):
            f()
        dt = time.perf_counter_ns() - t0
        if dt >= target * 1e9:
            break
        k *= 2
    reps = []
    for _ in range(1 if smoke else 5):
        t0 = time.perf_counter_ns()
        for _ in range(k):
            f()
        reps.append((time.perf_counter_ns() - t0) / k)
    med = statistics.median(reps)
    cv = statistics.pstdev(reps) / statistics.mean(reps) if len(reps) > 1 else 0.0
    return med, cv


def fmt(ns):
    return f"{ns:,.0f} ns" if ns < 1e5 else f"{ns / 1e6:,.3f} ms"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cpp", required=True)
    ap.add_argument("--out")
    ap.add_argument("--smoke", action="store_true")
    ap.add_argument("--build-type", default="Release")
    args = ap.parse_args()

    fairness(args.cpp)
    ct, ccv, context = cpp_times(args.cpp, args.smoke)
    rows = []
    for n in SIZES:
        t = table(n)
        pt, pcv = py_time(lambda: game.shapley(t), args.smoke)
        rows.append((n, ct[n], ccv.get(n, 0.0), pt, pcv, pt - ct[n]))
    n_call = 10
    v = lambda s: 0.5
    ct_call, _ = py_time(lambda: game.shapley(v, n_call), args.smoke)
    per_eval = ct_call / 2 ** n_call   # an upper bound: includes the (microseconds of) arithmetic

    lines = [
        "# Benchmarks",
        "",
        "Written by `scripts/run_benchmarks.sh` (`benchmarks/tabular_shapley_benchmark.py`); never edited by hand.",
        "",
        "## Tabular Shapley: Python call against C++",
        "",
        f"- Date {datetime.date.today().isoformat()}, {platform.system()} {platform.release()} {platform.machine()}, "
        f"{context.get('num_cpus', '?')} CPUs, {args.build_type} build, Python {platform.python_version()}, "
        f"snicholls_core {__version__}.",
        "- C++: `snicholls::game::shapley(n, table_game)` (Google Benchmark, median of 5). Python: "
        "`snicholls_core.game.shapley(table)` on the same float64 table (median of 5 timed loops). Fairness: the values "
        "are equal bit for bit for every n, checked before timing.",
        "- Overhead = Python - C++: argument conversion (a zero-copy view of the array), releasing and re-taking the GIL, "
        "and building the Attribution (one allocation and copy of the n values).",
        "",
        "| players n | coalitions | C++ | Python | overhead | overhead / Python | CV C++ | CV Python |",
        "|---:|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for n, c, ccv_, p, pcv_, o in rows:
        lines.append(f"| {n} | {2 ** n:,} | {fmt(c)} | {fmt(p)} | {fmt(o)} | {100 * o / p:.1f}% | "
                     f"{100 * ccv_:.1f}% | {100 * pcv_:.1f}% |")
    lines += [
        "",
        "The fixed cost of a call is what the small-n rows show. At large n the arithmetic dominates and the two are "
        "separately compiled copies of the same template (the module with nanobind's flags, built NOMINSIZE): a "
        "difference of a few percent either way there is code layout, not overhead.",
        "",
        f"Callable route, for scale: `game.shapley(v, {n_call})` with a Python `v` costs {fmt(ct_call)}, about "
        f"{fmt(per_eval)} per evaluation of v (2^{n_call} calls with the GIL held). Pass a table instead whenever v "
        "can be computed in bulk.",
        "",
    ]
    text = "\n".join(lines)
    print(text)
    if args.out and not args.smoke:
        open(args.out, "w").write(text)


if __name__ == "__main__":
    main()
