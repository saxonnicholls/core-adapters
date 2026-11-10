#
#  __init__.py
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
"""snicholls_core: Python bindings of snicholls-core.

- ``snicholls_core.game``: cooperative-game attribution - exact Shapley (from a callable or a table of v(S)),
  Banzhaf, Owen, Harsanyi dividends, the Shapley samplers, ``memoized``, change attribution, interactions and the
  game properties. C++ ``snicholls::game``.
- ``snicholls_core.stats``: ``Wyrand`` and the unbiased draws, bit for bit the C++ ``snicholls::stats`` generator.
- ``snicholls_core.errors``: the exception hierarchy every snicholls extension module shares.

Each function's docstring names its C++ counterpart; the C++ documentation names the Python one.
"""

__version__ = "0.1.0"

from . import errors  # noqa: E402  (first: the C++ modules look its classes up)
from . import stats  # noqa: E402
from . import game  # noqa: E402

__all__ = ["errors", "game", "stats", "__version__"]
