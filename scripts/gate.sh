#!/usr/bin/env bash
#
#  gate.sh
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
#  The repository's gate, non-interactive, offline. Core's family sweep
#  (core's scripts/family.py) runs it before bumping this repository's core
#  pin; run it yourself before every commit you mean to push.
#
#      scripts/gate.sh                     Debug and Release, then the package
#      scripts/gate.sh --config Release    one configuration
#      scripts/gate.sh --cxx g++           another compiler (default: c++)
#      scripts/gate.sh --no-docs           skip the Doxygen check
#
#  Steps, each fatal on failure (exit status non-zero):
#
#    1. A development venv (build-family/venv-dev, Python 3.12) with numpy and
#       pytest from the wheelhouse.
#    2. For each configuration: configure into build-family/<config> with
#       warnings as errors and that venv's Python, build with -j4, run every
#       CTest (GoogleTest, demos, the pytest suites against the build tree, the
#       static checks; in Release also the benchmark smoke run and, when
#       Doxygen is installed, the docs).
#    3. `pip install .` into a clean venv (build-family/venv-install, Python
#       3.12), then pytest tests/python against the installed package.
#    4. The same wheel (abi3, cp312) into a clean venv of every newer Python
#       found (3.13, 3.14, 3.15), and pytest there too.
#
#  No network: Python packages come from the wheelhouse with --no-index
#  ($CORE_ADAPTERS_WHEELHOUSE, default ~/.cache/core-adapters/wheelhouse),
#  filled once by scripts/fetch_wheels.sh; C++ dependencies from the
#  submodules. Parallelism is -j4 throughout.
#

set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${GATE_BUILD:-$ROOT/build-family}"
WHEELHOUSE="${CORE_ADAPTERS_WHEELHOUSE:-$HOME/.cache/core-adapters/wheelhouse}"
JOBS=4
CONFIGS=(Debug Release)
CXX_COMPILER="${CXX:-c++}"
DOCS=1

while [ $# -gt 0 ]; do
    case "$1" in
        --config) CONFIGS=("$2"); shift 2 ;;
        --cxx) CXX_COMPILER="$2"; shift 2 ;;
        --no-docs) DOCS=0; shift ;;
        -h|--help) sed -n '2,35p' "$0" | sed 's/^#\s\{0,1\}//'; exit 0 ;;
        *) echo "gate: unknown argument $1" >&2; exit 2 ;;
    esac
done

export CMAKE_BUILD_PARALLEL_LEVEL=$JOBS CTEST_PARALLEL_LEVEL=$JOBS
export PIP_NO_INDEX=1 PIP_FIND_LINKS="$WHEELHOUSE" PIP_DISABLE_PIP_VERSION_CHECK=1 PYTHONDONTWRITEBYTECODE=1

step() { printf '\n== gate: %s\n' "$*"; }
fail() { echo "gate: FAILED - $*" >&2; exit 1; }

# --- preflight -------------------------------------------------------------------------------------------------
# Every submodule .gitmodules names, and theirs in turn, must be checked out
check_submodules() {   # check_submodules <dir>
    [ -f "$1/.gitmodules" ] || return 0
    local path
    for path in $(sed -n 's/^[[:space:]]*path[[:space:]]*=[[:space:]]*//p' "$1/.gitmodules"); do
        [ -n "$(ls -A "$1/$path" 2>/dev/null)" ] || fail "$1/$path is empty: git submodule update --init --recursive"
        check_submodules "$1/$path"
    done
}
check_submodules "$ROOT"
PY312="${PYTHON:-$(command -v python3.12 || true)}"
[ -n "$PY312" ] || fail "python3.12 not found (set PYTHON=...)"
for req in scikit_build_core numpy pytest; do
    ls "$WHEELHOUSE"/${req}-* >/dev/null 2>&1 || fail "no $req wheel in $WHEELHOUSE: run scripts/fetch_wheels.sh once"
done

mkdir -p "$OUT"
new_venv() {   # new_venv <python> <dir>: a clean venv with numpy and pytest from the wheelhouse
    rm -rf "$2"
    "$1" -m venv "$2"
    "$2/bin/python" -m pip install --quiet numpy pytest || fail "installing numpy/pytest into $2 from $WHEELHOUSE"
}

# --- 1. development venv ---------------------------------------------------------------------------------------
step "development venv ($("$PY312" --version))"
new_venv "$PY312" "$OUT/venv-dev"
DEV_PY="$OUT/venv-dev/bin/python"

# --- 2. CMake: build and every CTest ---------------------------------------------------------------------------
for config in "${CONFIGS[@]}"; do
    dir="$OUT/$config"
    docs=OFF bench=OFF
    if [ "$DOCS" = 1 ] && [ "$config" = Release ] && command -v doxygen >/dev/null; then docs=ON; fi
    if [ "$config" = Release ]; then bench=ON; fi
    step "$config: configure, build (-j$JOBS), ctest  [$CXX_COMPILER, docs $docs]"
    cmake -S "$ROOT" -B "$dir" -G Ninja -DCMAKE_BUILD_TYPE="$config" -DCMAKE_CXX_COMPILER="$CXX_COMPILER" \
        -DCORE_ADAPTERS_WERROR=ON -DCORE_ADAPTERS_BUILD_DOCS=$docs -DCORE_ADAPTERS_BUILD_BENCHMARKS=$bench -DPython_EXECUTABLE="$DEV_PY" \
        -DPython3_EXECUTABLE="$DEV_PY" >"$dir.configure.log" 2>&1 || { tail -40 "$dir.configure.log"; fail "$config configure"; }
    cmake --build "$dir" -j "$JOBS" || fail "$config build"
    ctest --test-dir "$dir" -j "$JOBS" --output-on-failure || fail "$config ctest"
done
REFERENCE_DIR="$OUT/${CONFIGS[${#CONFIGS[@]}-1]}"

# --- 3. pip install . into a clean venv ------------------------------------------------------------------------
if [ -f "$ROOT/pyproject.toml" ]; then
    run_pytest() {   # run_pytest <venv>: the package suite against the installed package, outside the source tree
        local work; work="$(mktemp -d)"
        ( cd "$work" && CORE_ADAPTERS_REFERENCE_DIR="$REFERENCE_DIR" "$1/bin/python" -m pytest -q -p no:cacheprovider \
            --import-mode=importlib --rootdir "$ROOT/tests/python" -c /dev/null "$ROOT/tests/python" )
        local status=$?
        rm -rf "$work"
        return $status
    }

    step "pip install . into a clean venv ($("$PY312" --version))"
    new_venv "$PY312" "$OUT/venv-install"
    "$OUT/venv-install/bin/python" -m pip install --quiet "$ROOT" || fail "pip install ."
    "$OUT/venv-install/bin/python" -c "import snicholls_core, sys; print('  installed', snicholls_core.__version__, 'from', snicholls_core.__file__)"
    run_pytest "$OUT/venv-install" || fail "pytest against the installed package (3.12)"

    step "one abi3 wheel, every newer Python"
    rm -rf "$OUT/wheels"
    "$OUT/venv-install/bin/python" -m pip wheel --quiet --no-deps -w "$OUT/wheels" "$ROOT" || fail "pip wheel"
    wheel="$(ls "$OUT"/wheels/snicholls_core-*.whl)"
    case "$wheel" in *-cp312-abi3-*) echo "  $(basename "$wheel")" ;; *) fail "$wheel is not a cp312-abi3 wheel" ;; esac
    for v in 3.13 3.14 3.15; do
        py="$(command -v "python$v" || true)"
        [ -n "$py" ] || continue
        ls "$WHEELHOUSE"/numpy-*-cp${v/./}-* >/dev/null 2>&1 || { echo "  python$v: no numpy wheel in the wheelhouse, skipped"; continue; }
        new_venv "$py" "$OUT/venv-$v"
        "$OUT/venv-$v/bin/python" -m pip install --quiet "$wheel" || fail "installing the abi3 wheel into python$v"
        run_pytest "$OUT/venv-$v" || fail "pytest against the abi3 wheel on python$v"
    done
fi

step "passed"
