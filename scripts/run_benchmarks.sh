#!/usr/bin/env bash
#
#  run_benchmarks.sh
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
#  Builds the benchmarks in Release (build-bench) and rewrites BENCHMARKS.md. Python: $PYTHON, else the gate's
#  development venv (build-family/venv-dev), which has numpy. -j4.
#

set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PY="${PYTHON:-$ROOT/build-family/venv-dev/bin/python}"
[ -x "$PY" ] || { echo "run_benchmarks: $PY not found - run scripts/gate.sh once, or set PYTHON" >&2; exit 2; }
cmake -S "$ROOT" -B "$ROOT/build-bench" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCORE_ADAPTERS_BUILD_TESTS=OFF \
      -DCORE_ADAPTERS_BUILD_BENCHMARKS=ON -DPython_EXECUTABLE="$PY" -DPython3_EXECUTABLE="$PY" >/dev/null
cmake --build "$ROOT/build-bench" -j 4
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$ROOT/build-bench/python" "$PY" "$ROOT/benchmarks/tabular_shapley_benchmark.py" \
    --cpp "$ROOT/build-bench/benchmarks/tabular_shapley_benchmark" --out "$ROOT/BENCHMARKS.md"
