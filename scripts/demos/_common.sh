#!/usr/bin/env bash
#
#  _common.sh
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
#  Shared by the demo start scripts: configure once into build-demos (Release, with the Python modules), build what
#  the demo needs, run it. Idempotent: safe to re-run at any time; nothing is left running.
#
#  Python: $PYTHON, else build-family/venv-dev/bin/python (made by scripts/gate.sh, with numpy), else python3.12.
#

set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="${BUILD:-$ROOT/build-demos}"
JOBS="${JOBS:-4}"

configure_once() {
    if [ ! -f "$BUILD/CMakeCache.txt" ]; then
        local python="${PYTHON:-}"
        [ -n "$python" ] || { [ -x "$ROOT/build-family/venv-dev/bin/python" ] && python="$ROOT/build-family/venv-dev/bin/python"; }
        [ -n "$python" ] || python="$(command -v python3.12 || command -v python3)"
        cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DPython_EXECUTABLE="$python" \
              -DPython3_EXECUTABLE="$python" >/dev/null
    fi
}

# run_demo <name> [args...]
#   <name>                a C++ demo (demos/**/<name>.cpp, an executable), or a nanobind demo (demos/nanobind/<name>.py
#                         driving its module)
#   python_<module>_demo  a demo of the snicholls_core package (demos/python/<module>_demo.py)
run_demo() {
    local name="$1"; shift
    configure_once
    cmake --build "$BUILD" --target "core_adapters_$name" -j "$JOBS" >/dev/null
    local python script=""
    python="$(sed -n 's/^Python_EXECUTABLE:[A-Z]*=//p' "$BUILD/CMakeCache.txt")"
    case "$name" in
        python_*) script="$ROOT/demos/python/${name#python_}.py" ;;
        *) [ -f "$ROOT/demos/nanobind/$name.py" ] && script="$ROOT/demos/nanobind/$name.py" ;;
    esac
    if [ -n "$script" ]; then
        PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$BUILD/demos/modules:$BUILD/python" "$python" "$script" "$@"
    else
        "$BUILD/demos/core_adapters_$name" "$@"
    fi
}
