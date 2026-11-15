#
#  console.py
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
"""An interactive console for interrogating snicholls-core from Python: ``python -m snicholls_core``.

It opens a Python prompt with the package's modules loaded and four helpers:

- ``inventory()`` - every class, function and exception the package binds, with the C++ each one calls;
- ``describe(x)`` - a name (``"game.shapley"``, ``"snicholls::stats::wyrand"``) or an object: its signatures, its
  documentation and its C++ counterpart; for a class, its methods and properties;
- ``fields(obj)`` - the properties of a bound object and their current values, as a dict;
- ``cpp(name)`` - the Python names that call a C++ entity.

Nothing here names a type or a function: the console reads what the modules bind, so whatever is bound later
appears in it with no change here. Command line: ``--list [MODULE]``, ``--describe NAME``, ``--tour``; with no
argument it is interactive.
"""

import argparse
import code
import dataclasses
import importlib
import inspect
import sys
import types

import snicholls_core

__all__ = ["Entry", "inventory", "describe", "fields", "cpp", "tour", "namespace", "main"]


@dataclasses.dataclass(frozen=True)
class Entry:
    """One name the package binds.

    ``module`` is the submodule (``"game"``), ``name`` the Python name, ``kind`` one of ``"class"``, ``"function"``,
    ``"exception"`` or ``"constant"``, ``signatures`` one line per overload, ``summary`` the first sentence of the
    documentation and ``cpp`` the text after ``C++:`` (empty when the documentation has none).

    C++: none - Python only (one row of the console's inventory).
    """

    module: str
    name: str
    kind: str
    signatures: tuple
    summary: str
    cpp: str

    @property
    def qualified(self):
        """``module.name``, the form ``describe`` takes. C++: none - Python only."""
        return f"{self.module}.{self.name}"


def _modules():
    """The package's submodules, from its ``__all__``."""
    out = []
    for name in snicholls_core.__all__:
        value = getattr(snicholls_core, name, None)
        if isinstance(value, types.ModuleType):
            out.append((name, value))
    return out


def _nb_overloads(obj):
    """(signature, doc) per overload of a nanobind function, or None for anything else."""
    sigs = getattr(obj, "__nb_signature__", None)
    if not sigs:
        return None
    out = []
    for text, doc, defaults in sigs:
        text = text.removeprefix("def ")
        # nanobind writes a default as \<index> into the tuple of default values.
        for i, value in reversed(list(enumerate(defaults or ()))):
            text = text.replace(f"\\{i}", repr(value))
        out.append((text, doc or ""))
    return out


def _doc_of(obj):
    """The documentation without nanobind's signature lines."""
    overloads = _nb_overloads(obj)
    if overloads:
        return next((d for _, d in overloads if d), "")
    return inspect.getdoc(obj) or ""


def _signatures(obj, name):
    overloads = _nb_overloads(obj)
    if overloads:
        return tuple(s for s, _ in overloads)
    if inspect.isclass(obj):
        init = _nb_overloads(getattr(obj, "__init__", None))
        if init:
            return tuple(s.replace("__init__(self, ", f"{name}(").replace("__init__(self)", f"{name}()")
                         for s, _ in init)
    try:
        return (f"{name}{inspect.signature(obj)}",)
    except (TypeError, ValueError):
        return ()


def _cpp_of(doc):
    """The text after the last ``C++:`` up to the end of its line, else the first ``C++ ``name`` `` in the text."""
    at = doc.rfind("C++:")
    if at >= 0:
        rest = doc[at + 4:].strip()
        return rest.splitlines()[0].strip() if rest else ""
    at = doc.find("C++ ``")
    if at >= 0:
        end = doc.find("``", at + 6)
        return doc[at + 6:end] if end > 0 else ""
    return ""


def _summary_of(doc):
    first = doc.strip().split("\n\n")[0].replace("\n", " ").strip()
    return "" if first.startswith("C++:") else first


def _kind(obj):
    if inspect.isclass(obj):
        return "exception" if issubclass(obj, BaseException) else "class"
    if callable(obj):
        return "function"
    return "constant"


def _public(module):
    names = getattr(module, "__all__", None)
    if names is None:
        names = [n for n in dir(module) if not n.startswith("_")]
    for n in sorted(names):
        obj = getattr(module, n)
        if isinstance(obj, types.ModuleType):
            continue
        # Names a pure-Python module imported from elsewhere are not its own.
        home = getattr(obj, "__module__", module.__name__) or module.__name__
        if inspect.isclass(obj) or callable(obj):
            if not home.startswith("snicholls_core"):
                continue
        yield n, obj


def inventory(module=None):
    """Every name the package binds, as ``Entry`` rows sorted by module and name.

    ``module`` (``"game"``, ``"stats"``, ...) keeps one submodule; an unknown one is ``KeyError``.

    C++: none - Python only (reads the bound modules and their docstrings).
    """
    mods = _modules()
    if module is not None:
        mods = [(n, m) for n, m in mods if n == module]
        if not mods:
            raise KeyError(f"no submodule {module!r}; there are {[n for n, _ in _modules()]}")
    rows = []
    for mod_name, mod in mods:
        for name, obj in _public(mod):
            kind = _kind(obj)
            if kind == "constant":   # a value has no docstring of its own (getdoc would give its type's)
                rows.append(Entry(mod_name, name, kind, (f"{name} = {obj!r}",), repr(obj), ""))
                continue
            doc = _doc_of(obj)
            rows.append(Entry(mod_name, name, kind, _signatures(obj, name), _summary_of(doc), _cpp_of(doc)))
    return rows


def _members(cls):
    """(name, kind, doc) for the public methods and properties a class defines."""
    out = []
    for name in sorted(vars(cls)):
        if name.startswith("_"):
            continue
        attr = vars(cls)[name]
        if isinstance(attr, property) or type(attr).__name__ in ("nb_property", "getset_descriptor"):
            out.append((name, "property", (getattr(attr, "__doc__", "") or "").strip()))
        elif callable(attr) or isinstance(attr, (staticmethod, classmethod)):
            target = attr.__func__ if isinstance(attr, (staticmethod, classmethod)) else attr
            out.append((name, "method", _doc_of(target)))
    return out


def _resolve(name):
    """A Python object from ``"module.name"``, ``"name"`` or a C++ name."""
    if "::" in name:
        found = cpp(name)
        if not found:
            raise KeyError(f"nothing in the package calls {name!r}")
        return [getattr(getattr(snicholls_core, e.module), e.name) for e in found]
    parts = name.removeprefix("snicholls_core.").split(".")
    if len(parts) >= 2 and hasattr(snicholls_core, parts[0]):
        obj = getattr(snicholls_core, parts[0])
        try:
            for p in parts[1:]:
                obj = getattr(obj, p)
        except AttributeError:
            raise KeyError(f"no {name!r} in the package") from None
        return [obj]
    matches = [e for e in inventory() if e.name == name]
    if not matches:
        raise KeyError(f"no {name!r} in the package")
    return [getattr(getattr(snicholls_core, e.module), e.name) for e in matches]


def describe(x):
    """What a bound name or object is, as text: signatures, documentation and the C++ it calls.

    ``x`` is a name (``"game.shapley"``, ``"shapley"``, ``"snicholls::game::shapley"``), a bound function or
    class, or an instance (then its class is described and its current ``fields`` are listed). An unknown name is
    ``KeyError``.

    C++: none - Python only (reads the docstrings, which name the C++).
    """
    if isinstance(x, str):
        if "::" not in x:
            rows = {e.qualified: e for e in inventory()}
            row = rows.get(x.removeprefix("snicholls_core."))
            if row is not None and row.kind == "constant":
                return f"snicholls_core.{row.signatures[0]}"
        return "\n\n".join(describe(obj) for obj in _resolve(x))
    instance = not (inspect.isclass(x) or callable(x) or isinstance(x, types.ModuleType))
    target = type(x) if instance else x
    name = getattr(target, "__qualname__", getattr(target, "__name__", repr(target)))
    module = getattr(target, "__module__", "")
    lines = [f"{module}.{name}" if module else name]
    lines += [f"  {s}" for s in _signatures(target, name)]
    doc = _doc_of(target)
    if doc:
        lines += ["", doc]
    if inspect.isclass(target):
        members = _members(target)
        if members:
            lines.append("")
            for m, kind, mdoc in members:
                first = _summary_of(mdoc) or mdoc.split("\n")[0]
                lines.append(f"  {m:<20} {kind:<9} {first}")
    if instance:
        lines += ["", "Current values:"]
        lines += [f"  {k} = {v!r}" for k, v in fields(x).items()]
    return "\n".join(lines)


def fields(obj):
    """The properties of a bound object and their current values, in the class's order of names.

    A bound class with ``__fields__`` (the reflection binder) is read through it; otherwise every public property
    the class defines. Anything that is not an instance of a bound class is ``TypeError``.

    C++: none - Python only (reads the properties the binding defines).
    """
    cls = type(obj)
    if not (getattr(cls, "__module__", "") or "").startswith("snicholls_core"):
        raise TypeError(f"{cls.__name__} is not a snicholls_core class")
    declared = getattr(cls, "__fields__", None)
    if declared is not None:
        names = [f if isinstance(f, str) else f[0] for f in declared]
    else:
        names = [n for n, kind, _ in _members(cls) if kind == "property"]
    return {n: getattr(obj, n) for n in names}


def cpp(name):
    """The ``Entry`` rows whose C++ counterpart mentions ``name`` (``"snicholls::game::shapley"``, ``"wyrand"``).

    A whole C++ name matches only itself, not a longer one it is a prefix of (``snicholls::game::shapley`` does not
    match ``snicholls::game::shapley_sampled``).

    C++: none - Python only (searches the ``C++:`` lines of the docstrings).
    """
    def mentions(text):
        at = text.find(name)
        while at >= 0:
            end = at + len(name)
            if end == len(text) or not (text[end].isalnum() or text[end] == "_"):
                return True
            at = text.find(name, at + 1)
        return False

    return [e for e in inventory() if mentions(e.cpp)]


def tour(out=None):
    """A short guided session: draws from ``Wyrand``, a three-player game, its Shapley values and their fields.

    Prints each step to ``out`` (default ``sys.stdout``) and returns True when every result is the closed form it
    prints.

    C++: none - Python only (calls the bound modules).
    """
    import numpy as np
    from snicholls_core import game, stats

    out = out or sys.stdout
    ok = []

    def show(text):
        print(text, file=out)

    show(">>> g = stats.Wyrand(1); [g() for _ in range(3)]")
    g = stats.Wyrand(1)
    draws = [g() for _ in range(3)]
    show(f"{draws}")
    ok.append(draws == [12120239713417281201, 9861924296701902397, 4420488659902023302])

    show("\n>>> # v(S) for every coalition S of 3 players, indexed by bitmask: v(S) = |S|^2")
    table = np.array([bin(s).count("1") ** 2 for s in range(8)], dtype=np.float64)
    show(f">>> table = {table.tolist()}")
    show(">>> a = game.shapley(table)")
    a = game.shapley(table)
    show(">>> fields(a)")
    f = fields(a)
    for k, v in f.items():
        show(f"    {k} = {v!r}")
    ok.append(np.allclose(np.asarray(a.values), [3.0, 3.0, 3.0]) and a.total == 9.0)

    show("\n>>> cpp('snicholls::game::shapley')")
    rows = cpp("snicholls::game::shapley")
    show(f"{[e.qualified for e in rows]}")
    ok.append(any(e.qualified == "game.shapley" for e in rows))

    show("\n>>> print(describe('stats.Wyrand'))")
    show(describe("stats.Wyrand"))
    show("\ntour: " + ("ok" if all(ok) else f"FAILED {ok}"))
    return all(ok)


def namespace():
    """The names the interactive console starts with: the package, its submodules, numpy as ``np`` and the helpers.

    C++: none - Python only.
    """
    ns = {"snicholls_core": snicholls_core}
    ns.update(dict(_modules()))
    try:
        ns["np"] = importlib.import_module("numpy")
    except ImportError:  # numpy is a dependency of the package, but the console works without it
        pass
    ns.update(inventory=inventory, describe=describe, fields=fields, cpp=cpp, tour=tour)
    return ns


def _print_inventory(rows, out):
    for e in rows:
        sig = e.signatures[0] if e.signatures else e.name
        more = f"  (+{len(e.signatures) - 1} overloads)" if len(e.signatures) > 1 else ""
        print(f"{e.module}.{e.name:<34} {e.kind:<9} C++: {e.cpp or '-'}", file=out)
        print(f"    {sig}{more}", file=out)


def _banner():
    mods = ", ".join(n for n, _ in _modules())
    return (f"snicholls_core {snicholls_core.__version__} - Python {sys.version.split()[0]}\n"
            f"Loaded: {mods}, np\n"
            "Helpers: inventory(), describe(x), fields(obj), cpp(name), tour()\n"
            "Try: describe('game.shapley')   fields(game.shapley(np.arange(8.0)))   help(stats.Wyrand)")


def main(argv=None):
    """The command line of ``python -m snicholls_core``; returns the exit status.

    ``--list [MODULE]`` prints the inventory, ``--describe NAME`` one name, ``--tour`` the guided session (status 1
    if a result is wrong); with none of them an interactive console starts. An unknown name or module is status 2.

    C++: none - Python only.
    """
    ap = argparse.ArgumentParser(prog="python -m snicholls_core", description=__doc__.split("\n")[0])
    ap.add_argument("--list", nargs="?", const="", metavar="MODULE", help="print every bound name (or one module's)")
    ap.add_argument("--describe", metavar="NAME", help="describe a name: game.shapley, shapley or a C++ name")
    ap.add_argument("--tour", action="store_true", help="run the guided session and exit")
    args = ap.parse_args(argv)
    try:
        if args.list is not None:
            _print_inventory(inventory(args.list or None), sys.stdout)
            return 0
        if args.describe:
            print(describe(args.describe))
            return 0
    except KeyError as e:
        print(f"snicholls_core: {e.args[0]}", file=sys.stderr)
        return 2
    if args.tour:
        return 0 if tour() else 1
    code.interact(banner=_banner(), local=namespace(), exitmsg="")
    return 0
