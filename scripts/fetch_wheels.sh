#!/usr/bin/env bash
#
#  fetch_wheels.sh
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
#  The one step that uses the network: download the pinned Python packages of
#  requirements/gate.txt, with their dependencies, into the wheelhouse that
#  scripts/gate.sh installs from offline. Run it once per machine and Python
#  version, and again after requirements/gate.txt changes.
#
#      scripts/fetch_wheels.sh [python ...]     default: python3.12, and every python3.1x found
#
#  Wheelhouse: $CORE_ADAPTERS_WHEELHOUSE, default ~/.cache/core-adapters/wheelhouse.
#

set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WHEELHOUSE="${CORE_ADAPTERS_WHEELHOUSE:-$HOME/.cache/core-adapters/wheelhouse}"
mkdir -p "$WHEELHOUSE"

pythons=("$@")
if [ ${#pythons[@]} -eq 0 ]; then
    for v in 3.12 3.13 3.14 3.15; do command -v "python$v" >/dev/null && pythons+=("python$v"); done
fi
[ ${#pythons[@]} -gt 0 ] || { echo "fetch_wheels: no python3.12+ found" >&2; exit 2; }

for py in "${pythons[@]}"; do
    echo "fetch_wheels: $py -> $WHEELHOUSE"
    "$py" -m pip download --quiet --disable-pip-version-check -d "$WHEELHOUSE" -r "$ROOT/requirements/gate.txt"
done
echo "fetch_wheels: $(ls "$WHEELHOUSE" | wc -l | tr -d ' ') files in $WHEELHOUSE"
