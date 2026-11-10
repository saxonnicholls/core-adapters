#
#  game_demo.py
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
"""Demo for snicholls_core.game: attribution from a table (fast, GIL released) and from a callable."""

import sys

import numpy as np
from snicholls_core import game, stats


def glove(s):
    """Players 0 and 1 hold left gloves, player 2 a right glove: a pair is worth 1."""
    return float(min(bin(s & 3).count("1"), (s >> 2) & 1))


checks = []

# Exact Shapley: tabulate once, then the arithmetic runs in C++ with the GIL released
table = game.tabulate(glove, 3)
a = game.shapley(table)
print("Shapley of the glove game:", a)
checks.append(np.allclose(a.values, [1 / 6, 1 / 6, 2 / 3]) and abs(a.unattributed()) < 1e-15)

# The same from the callable, from a frozenset game, and Banzhaf / dividends
checks.append(np.allclose(game.shapley(glove, 3).values, a.values))
fs = game.set_game(lambda s: float(min(len(s & {0, 1}), len(s & {2}))))
checks.append(np.allclose(game.shapley(fs, 3).values, a.values))
print("Banzhaf:", game.banzhaf(table).values, " dividends:", game.harsanyi_dividends(table))
d = game.harsanyi_dividends(table)
checks.append(np.allclose(game.shapley_from_dividends(d).values, a.values))
checks.append(np.allclose(game.banzhaf_from_dividends(d).values, game.banzhaf(table).values))

# Owen: the two left gloves as one group
o = game.owen(table, [[0, 1], [2]])
print("Owen with groups [[0, 1], [2]]:", o.values)
checks.append(np.allclose(o.values, [0.25, 0.25, 0.5]))
checks.append(np.allclose(game.owen(glove, 3, [[0, 1], [2]]).values, o.values))

# Sampled, reproducible from the seed; memoized for an expensive game
g = stats.Wyrand(7)
s = game.shapley_sampled(table, g, permutations=2000, antithetic=True)
print(f"sampled: {s.values} +- {s.standard_errors} ({s.samples} orders, {s.evaluations} evaluations)")
checks.append(np.allclose(s.values, a.values, atol=0.02))
m = game.memoized(glove)
lifts = game.shapley_sampled_lifts(m, 3, stats.Wyrand(8), samples_per_player=500)
print(f"lifts sampler on a memoized game: {lifts.values} ({m.evaluations} distinct evaluations)")
checks.append(m.evaluations == 8 and abs(lifts.unattributed()) < 1e-9)
checks.append(np.allclose(game.shapley_sampled(m, 3, stats.Wyrand(7), 2000).values, s.values))

# Why did f move from start to end
c = game.attribute_change(lambda x: x[0] * x[1] + 3 * x[2], np.array([1.0, 1.0, 0.0]), np.array([2.0, 3.0, 1.0]))
print("change attribution of x0*x1 + 3*x2:", c.values)
checks.append(np.allclose(c.values, [2.0, 3.0, 3.0]))

# Several metrics at once
mv = game.shapley(lambda s: [glove(s), bin(s).count("1")], 3)
checks.append(mv.values.shape == (3, 2))

# Interactions and properties
i = game.interactions(table)
print("interactions:\n", i)
checks.append(i[0, 2] > 0 and i[0, 1] < 0)
print("monotone:", game.is_monotone(table), " superadditive:", game.is_superadditive(table),
      " supermodular:", game.is_supermodular(table), " additive:", game.is_additive(table),
      " submodular:", game.is_submodular(table))
checks.append(game.is_monotone(table) and game.is_superadditive(table) and not game.is_supermodular(table))
print("first supermodularity violation:", game.find_supermodularity_violation(table))
print("first submodularity violation:", game.find_submodularity_violation(table))
print("first interaction:", game.find_interaction(table))
checks.append(game.find_monotonicity_violation(table) is None and game.find_superadditivity_violation(table) is None)
print("(0, 0, 1) in the core:", game.in_core(table, np.array([0.0, 0.0, 1.0])),
      " Shapley's core violation:", game.find_core_violation(table, a.values))
checks.append(game.in_core(table, np.array([0.0, 0.0, 1.0])) and game.find_core_violation(table, a.values) is not None)

# Coalitions are bitmasks
s3 = game.from_members([0, 2])
print("coalition {0, 2} =", s3, "members", game.members(s3), "size", game.coalition_size(s3))
checks.append(s3 == game.with_player(game.singleton(0), 2) and game.without(s3, 2) == 1 and game.contains(s3, 2)
              and game.grand_coalition(3) == 7 and game.table_size(3) == 8 and game.max_table_players == 30)

ok = all(checks)
print("game_demo:", "ok" if ok else f"FAILED {checks}")
sys.exit(0 if ok else 1)
