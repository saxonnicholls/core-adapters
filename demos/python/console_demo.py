#
#  console_demo.py
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
"""Demo for snicholls_core.console: interrogating the bound objects, as at the python -m snicholls_core prompt."""

import io
import sys

import numpy as np
from snicholls_core import console, game, stats

checks = []

rows = console.inventory()
print(f"inventory(): {len(rows)} names in {sorted({e.module for e in rows})}")
for e in console.inventory("stats"):
    print(f"  {e.qualified:<22} {e.kind:<9} C++: {e.cpp}")
checks.append(any(e.qualified == "game.shapley" for e in rows))

print("\ndescribe('snicholls::game::banzhaf_from_dividends'):")
text = console.describe("snicholls::game::banzhaf_from_dividends")
print(text)
checks.append("banzhaf_from_dividends(" in text)

a = game.shapley(np.array([0.0, 1.0, 2.0, 4.0]))
print("\nfields(game.shapley(table)):", console.fields(a))
checks.append(list(np.asarray(console.fields(a)["values"])) == [1.5, 2.5])

print("\ncpp('snicholls::stats::wyrand'):", [e.qualified for e in console.cpp("snicholls::stats::wyrand")])
checks.append([e.qualified for e in console.cpp("snicholls::stats::wyrand")] == ["stats.Wyrand"])

print("\ndescribe(stats.Wyrand(7)):")
print(console.describe(stats.Wyrand(7)))

print("\nnamespace():", sorted(console.namespace()))
checks.append({"game", "stats", "describe"} <= set(console.namespace()))

buf = io.StringIO()
checks.append(console.tour(buf))
print("\ntour(): ", buf.getvalue().splitlines()[-1])

checks.append(console.main(["--describe", "game.owen"]) == 0)
checks.append(console.main(["--describe", "nothing_here"]) == 2)

try:
    console.fields(1.0)
    checks.append(False)
except TypeError as e:
    print("refused:", e)

ok = all(checks)
print("console_demo:", "ok" if ok else f"FAILED {checks}")
sys.exit(0 if ok else 1)
