#!/usr/bin/env bash
#
#  run_demos.sh
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
#  Runs every demo start script (scripts/demos/*.sh) with its output shown; exits non-zero if any fails.
#
#      scripts/run_demos.sh            every demo
#      scripts/run_demos.sh gil_demo   one, by name
#

set -uo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/demos" && pwd)"
failed=()
names=("$@")
[ ${#names[@]} -gt 0 ] || for f in "$DIR"/*_demo.sh; do names+=("$(basename "$f" .sh)"); done
for n in "${names[@]}"; do
    echo "== $n"
    "$DIR/$n.sh" || failed+=("$n")
done
[ ${#failed[@]} -eq 0 ] && { echo "run_demos: all ${#names[@]} passed"; exit 0; }
echo "run_demos: FAILED: ${failed[*]}"; exit 1
