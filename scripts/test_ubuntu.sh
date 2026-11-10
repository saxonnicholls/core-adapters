#!/usr/bin/env bash
#
#  test_ubuntu.sh
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
#  The gate on Ubuntu 24.04, in Docker - the local half of what CI runs, after
#  core's scripts/test_ubuntu.sh. Builds the image (docker/ubuntu/Dockerfile,
#  tagged by a hash of it and of requirements/gate.txt), copies the tracked
#  files and the submodules' tracked files into the container (the checkout is
#  mounted read-only, nothing is written back), and runs scripts/gate.sh:
#
#      gcc     Debug and Release, then pip install . and pytest
#      clang   Release (C++ and the build-tree pytest suites)
#
#      scripts/test_ubuntu.sh            both compilers
#      scripts/test_ubuntu.sh --quick    GCC only
#
#  Runs niced, on 4 CPUs. Exits non-zero if any run fails.
#

set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
QUICK=0
for arg in "$@"; do
    case "$arg" in
        --quick) QUICK=1 ;;
        -h|--help) sed -n '2,22p' "$0" | sed 's/^#\s\{0,1\}//'; exit 0 ;;
        *) echo "test_ubuntu: unknown argument $arg" >&2; exit 2 ;;
    esac
done

command -v docker >/dev/null || { echo "test_ubuntu: docker is not installed" >&2; exit 2; }
docker info >/dev/null 2>&1 || { echo "test_ubuntu: docker is not running" >&2; exit 2; }

HASH="$(cat "$ROOT/docker/ubuntu/Dockerfile" "$ROOT/requirements/gate.txt" | (shasum -a 256 2>/dev/null || sha256sum))"
IMAGE="core-adapters-ubuntu:${HASH:0:12}"
if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    echo "test_ubuntu: building $IMAGE"
    DOCKER_BUILDKIT=1 nice docker build -q -t "$IMAGE" -f "$ROOT/docker/ubuntu/Dockerfile" "$ROOT/requirements" >/dev/null
fi

LIST="$(mktemp)"
trap 'rm -f "$LIST"' EXIT
( cd "$ROOT" && git ls-files && git submodule foreach --quiet --recursive \
    'git ls-files | sed "s|^|$displaypath/|"' ) | while read -r f; do [ -f "$ROOT/$f" ] && echo "$f"; done > "$LIST"

nice docker run --rm -i --cpus 4 \
    -v "$ROOT:/src:ro" -v "$LIST:/filelist:ro" -e QUICK="$QUICK" \
    "$IMAGE" bash -s <<'IN_CONTAINER'
set -uo pipefail
mkdir -p /work/core-adapters && cd /src && tar -cf - -T /filelist | tar -C /work/core-adapters -xf -
cd /work/core-adapters
echo "core-adapters on $(. /etc/os-release && echo "$PRETTY_NAME"), $(uname -m)"
echo "  $(g++ --version | head -1)"; echo "  $(clang++ --version | head -1)"; echo "  $(python3.12 --version)"
failed=0
GATE_BUILD=/work/build-gcc scripts/gate.sh --cxx g++ --no-docs || failed=1
if [ "$QUICK" != 1 ]; then
    for config in Release; do
        dir=/work/build-clang-$config
        echo; echo "== clang $config"
        if cmake -S . -B "$dir" -G Ninja -DCMAKE_BUILD_TYPE=$config -DCMAKE_CXX_COMPILER=clang++ \
               -DCORE_ADAPTERS_WERROR=ON -DPython_EXECUTABLE=/work/build-gcc/venv-dev/bin/python \
               -DPython3_EXECUTABLE=/work/build-gcc/venv-dev/bin/python >/dev/null &&
           cmake --build "$dir" -j 4 && ctest --test-dir "$dir" -j 4 --output-on-failure; then
            echo "  clang $config: ok"
        else
            echo "  clang $config: FAILED"; failed=1
        fi
    done
fi
[ $failed = 0 ] && echo "test_ubuntu: passed" || echo "test_ubuntu: FAILED"
exit $failed
IN_CONTAINER
