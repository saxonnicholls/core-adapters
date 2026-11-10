//
//  game.cpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//
//  The snicholls_core.game module: cooperative-game attribution from <snicholls/core/game/...>. Python names and
//  their C++ counterparts: docs/python_api.md.
//
//  Two kinds of game reach C++:
//
//    - a table of v(S), one float64 per coalition (a NumPy array of 2^n values, or 2^n x m for m metrics at once).
//      The arithmetic then runs with the GIL released, with no Python call at all: the fast route, and the one for
//      exact Shapley up to about 20 players.
//    - a Python callable v(s) -> float (or -> array, for the exact methods): every evaluation needs the GIL. The
//      exact methods tabulate it first (2^n calls, GIL held) and then take the table route; Owen and the samplers
//      call it as they go, GIL held, so they never evaluate more coalitions than they need. memoized(v) caches the
//      values in C++.
//

#include <snicholls/core/game/change.hpp>
#include <snicholls/core/game/coalition.hpp>
#include <snicholls/core/game/harsanyi.hpp>
#include <snicholls/core/game/owen.hpp>
#include <snicholls/core/game/properties.hpp>
#include <snicholls/core/game/shapley.hpp>
#include <snicholls/core/stats/random.hpp>
#include <snicholls/core_adapters/nanobind/errors.hpp>
#include <snicholls/core_adapters/nanobind/gil.hpp>
#include <snicholls/core_adapters/nanobind/ndarray.hpp>

#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace nb = nanobind;
using namespace nb::literals;
namespace game = snicholls::game;
namespace stats = snicholls::stats;
namespace py = snicholls::python;
using game::coalition;

namespace {

// The most players of the properties and interactions, which are instantiated for each player count
constexpr std::size_t max_dispatch_players = 20;

// ---- results ----------------------------------------------------------------------------------------------------

// An attribution as Python sees it. metrics == 0 for a scalar game: baseline and total are floats, values has shape
// (n,); otherwise baseline and total have shape (m,) and values (n, m).
struct attribution
{
    std::size_t players = 0;
    std::size_t metrics = 0;
    std::vector<double> baseline, total, values, standard_errors;
    std::size_t samples = 0;
    std::size_t evaluations = 0;
    bool sampled = false;

    [[nodiscard]] std::size_t width() const noexcept { return metrics == 0 ? 1 : metrics; }

    void resize(std::size_t n, std::size_t m)
    {
        players = n;
        metrics = m;
        baseline.assign(width(), 0.0);
        total.assign(width(), 0.0);
        values.assign(n * width(), 0.0);
    }

    // The baseline plus every player's amount, per metric
    [[nodiscard]] std::vector<double> attributed() const
    {
        std::vector<double> out = baseline;
        for (std::size_t i = 0; i < players; ++i)
            for (std::size_t j = 0; j < width(); ++j)
                out[j] += values[i * width() + j];
        return out;
    }
};

using any_numpy = nb::ndarray<nb::numpy, double>;

nb::object metric_out(const attribution& a, std::vector<double> v)
{
    if (a.metrics == 0)
        return nb::float_(v.at(0));
    return nb::cast(py::to_numpy(std::move(v)));
}

// ---- games ------------------------------------------------------------------------------------------------------

// A table of v(S): a view of 2^n rows of `width` values each (row-major), owned by the argument or by `owned`.
struct game_table
{
    std::vector<double> owned;
    std::span<const double> data;
    std::size_t n = 0;
    std::size_t metrics = 0;

    [[nodiscard]] std::size_t width() const noexcept { return metrics == 0 ? 1 : metrics; }
    [[nodiscard]] std::size_t size() const noexcept { return game::table_size(n); }

    // Column j as its own contiguous table (a view for a scalar game)
    [[nodiscard]] std::span<const double> column(std::size_t j, std::vector<double>& scratch) const
    {
        if (metrics == 0)
            return data;
        scratch.resize(size());
        for (std::size_t s = 0; s < size(); ++s)
            scratch[s] = data[s * metrics + j];
        return scratch;
    }
};

std::size_t players_of_table(std::size_t size)
{
    py::require(size > 0 && std::has_single_bit(size),
                "snicholls_core.game: a table has one value per coalition: 2^n rows");
    const auto n = static_cast<std::size_t>(std::countr_zero(size));
    py::require(n <= game::max_table_players, "snicholls_core.game: a table has at most 2^30 rows");
    return n;
}

game_table table_of(const py::vector_in<double>& t)
{
    game_table out;
    out.n = players_of_table(t.shape(0));
    out.data = py::as_span(t);
    return out;
}

game_table table_of(const py::matrix_in<double>& t)
{
    game_table out;
    out.n = players_of_table(t.shape(0));
    py::require(t.shape(1) > 0, "snicholls_core.game: a table of metrics needs at least one column");
    out.metrics = t.shape(1);
    out.data = py::as_span(t);
    return out;
}

[[noreturn]] void not_a_number(coalition s)
{
    throw nb::type_error(("snicholls_core.game: the game returned something that is neither a number nor an array "
                          "for coalition " + std::to_string(s)).c_str());
}

// v(s) for every coalition, with the GIL held: a number each (a scalar game) or an array of one shape each.
game_table tabulate(const nb::callable& v, std::size_t n, std::size_t& calls)
{
    py::require(n <= game::max_table_players, "snicholls_core.game: at most 30 players for an exact method");
    game_table out;
    out.n = n;
    const std::size_t size = out.size();
    nb::object asarray;
    for (coalition s = 0; s < size; ++s) {
        nb::object r = v(s);
        ++calls;
        if (nb::isinstance<nb::str>(r) || nb::isinstance<nb::bytes>(r))
            not_a_number(s);
        if (s == 0) {
            if (nb::isinstance<nb::float_>(r) || nb::isinstance<nb::int_>(r) || !nb::hasattr(r, "__len__")) {
                out.metrics = 0;
                out.owned.reserve(size);
            } else {
                asarray = nb::module_::import_("numpy").attr("asarray");
                out.metrics = nb::len(r);
                py::require(out.metrics > 0, "snicholls_core.game: the game returned an empty array");
                out.owned.reserve(size * out.metrics);
            }
        }
        if (out.metrics == 0) {
            double d = 0;
            if (!nb::try_cast(r, d))
                not_a_number(s);
            out.owned.push_back(d);
        } else {
            py::vector_in<double> row;
            nb::object flat;
            try {
                flat = asarray(r, "dtype"_a = "float64").attr("reshape")(-1);
            } catch (const nb::python_error&) {
                not_a_number(s);
            }
            if (!nb::try_cast(flat, row))
                not_a_number(s);
            py::require(row.shape(0) == out.metrics,
                        "snicholls_core.game: the game returned arrays of different sizes for different coalitions");
            const auto x = py::as_span(row);
            out.owned.insert(out.owned.end(), x.begin(), x.end());
        }
    }
    out.data = out.owned;
    return out;
}

// A Python game that must return a number, as a borrowed reference (its owner keeps it alive)
struct borrowed_game
{
    nb::handle f;

    double operator()(coalition s) const
    {
        nb::object r = f(s);
        double d = 0;
        if (!nb::try_cast(r, d))
            throw nb::type_error(("snicholls_core.game: this method needs a game that returns a number; it returned "
                                  "something else for coalition " + std::to_string(s)).c_str());
        return d;
    }
};

// memoized(v): core's memoized over a Python game; Python sees the class Memoized. The callable is owned here, where
// the garbage collector can see it (tp_traverse), so a game that refers back to its Memoized is still collected.
struct python_memoized
{
    explicit python_memoized(nb::object f) : function(std::move(f)), memo(borrowed_game{function}) {}
    nb::object function;
    game::memoized<borrowed_game> memo;
};

// set_game(f): calls f with a frozenset of the players
struct set_game
{
    nb::object function;

    nb::object operator()(coalition s) const
    {
        nb::list players;
        for (coalition b = s; b != 0; b &= b - 1)
            players.append(std::countr_zero(b));
        return function(nb::frozenset(players));
    }
};

// The GC slots of a class holding one Python callable in `function`
template <class T>
int traverse_function(PyObject* self, visitproc visit, void* arg)
{
    Py_VISIT(Py_TYPE(self));
    if (!nb::inst_ready(self))
        return 0;
    Py_VISIT(nb::inst_ptr<T>(self)->function.ptr());
    return 0;
}

template <class T>
int clear_function(PyObject* self)
{
    nb::inst_ptr<T>(self)->function.reset();
    return 0;
}

template <class T>
PyType_Slot function_slots[] = {{Py_tp_traverse, reinterpret_cast<void*>(&traverse_function<T>)},
                                {Py_tp_clear, reinterpret_cast<void*>(&clear_function<T>)},
                                {0, nullptr}};

// Counts the calls a method makes to a game
template <class V>
struct counted
{
    const V* v;
    std::size_t* calls;
    auto operator()(coalition s) const -> decltype((*v)(s))
    {
        ++*calls;
        return (*v)(s);
    }
};

// A table as a game, for the methods that take a callable
struct table_view
{
    const double* t;
    double operator()(coalition s) const noexcept { return t[s]; }
};

// Calls f with the scalar game behind `v`: core's memoized when v is a Memoized, else the callable itself.
template <class F>
decltype(auto) with_scalar_game(nb::handle v, F&& f)
{
    python_memoized* memo = nullptr;
    if (nb::try_cast(v, memo) && memo != nullptr)
        return f(memo->memo);
    py::require(PyCallable_Check(v.ptr()) != 0, "snicholls_core.game: the game is not callable");
    const borrowed_game g{v};
    return f(g);
}

// f(std::integral_constant<std::size_t, N>) for the run-time n, N <= max_dispatch_players
template <class F, std::size_t... I>
auto dispatch_players(std::size_t n, F&& f, std::index_sequence<I...>)
{
    using R = decltype(f(std::integral_constant<std::size_t, 0>{}));
    R out{};
    static_cast<void>(((n == I ? (out = f(std::integral_constant<std::size_t, I>{}), true) : false) || ...));
    return out;
}

template <class F>
auto with_players(std::size_t n, F&& f)
{
    py::require(n <= max_dispatch_players, "snicholls_core.game: this method takes at most 20 players");
    return dispatch_players(n, std::forward<F>(f), std::make_index_sequence<max_dispatch_players + 1>{});
}

std::vector<coalition> groups_of(std::size_t n, const std::vector<std::vector<std::size_t>>& groups)
{
    std::vector<coalition> out;
    out.reserve(groups.size());
    for (const auto& g : groups) {
        coalition c = 0;
        for (const std::size_t i : g) {
            py::require(i < n, "snicholls_core.game.owen: a player in a group is out of range");
            py::require(!game::contains(c, i), "snicholls_core.game.owen: a player is twice in one group");
            c = game::with(c, i);
        }
        out.push_back(c);
    }
    py::require(game::is_partition_of(n, out),
                "snicholls_core.game.owen: the groups must partition the players 0..n-1 (each exactly once, none empty)");
    return out;
}

// ---- the methods, on tables (GIL released) ---------------------------------------------------------------------

attribution exact_shapley(const game_table& t)
{
    attribution a;
    a.resize(t.n, t.metrics);
    py::without_gil([&] {
        std::vector<double> scratch;
        for (std::size_t j = 0; j < t.width(); ++j) {
            const std::span<const double> col = t.column(j, scratch);
            const auto r = game::shapley(t.n, game::table_game<double>(col));
            a.baseline[j] = r.baseline;
            a.total[j] = r.total;
            for (std::size_t i = 0; i < t.n; ++i)
                a.values[i * t.width() + j] = r.values[i];
        }
    });
    a.evaluations = t.size();
    return a;
}

// Shapley or Banzhaf from dividends; `from_values` first turns a table of v(S) into dividends
template <bool Banzhaf>
attribution from_dividends(const game_table& t, bool from_values)
{
    attribution a;
    a.resize(t.n, t.metrics);
    py::without_gil([&] {
        std::vector<double> scratch, d;
        for (std::size_t j = 0; j < t.width(); ++j) {
            const std::span<const double> col = t.column(j, scratch);
            d.assign(col.begin(), col.end());
            if (from_values)
                game::to_dividends(std::span<double>(d));
            const auto r = Banzhaf ? game::banzhaf_from_dividends(std::span<const double>(d))
                                   : game::shapley_from_dividends(std::span<const double>(d));
            a.baseline[j] = r.baseline;
            a.total[j] = r.total;
            for (std::size_t i = 0; i < t.n; ++i)
                a.values[i * t.width() + j] = r.values[i];
        }
    });
    a.evaluations = t.size();
    return a;
}

any_numpy dividends_of(const game_table& t)
{
    std::vector<double> out(t.size() * t.width());
    py::without_gil([&] {
        std::vector<double> scratch, d;
        for (std::size_t j = 0; j < t.width(); ++j) {
            const std::span<const double> col = t.column(j, scratch);
            d.assign(col.begin(), col.end());
            game::to_dividends(std::span<double>(d));
            for (std::size_t s = 0; s < t.size(); ++s)
                out[s * t.width() + j] = d[s];
        }
    });
    if (t.metrics == 0)
        return any_numpy(py::to_numpy(std::move(out)));
    return any_numpy(py::to_numpy(std::move(out), t.size(), t.metrics));
}

template <class R>
attribution scalar_attribution(const R& r, std::size_t n)
{
    attribution a;
    a.resize(n, 0);
    a.baseline[0] = r.baseline;
    a.total[0] = r.total;
    std::copy(r.values.begin(), r.values.end(), a.values.begin());
    return a;
}

template <class R>
attribution sampled_attribution(const R& r, std::size_t n)
{
    attribution a = scalar_attribution(r, n);
    a.sampled = true;
    a.standard_errors.assign(r.standard_errors.begin(), r.standard_errors.end());
    a.samples = r.samples;
    a.evaluations = r.evaluations;
    return a;
}

void require_scalar(const game_table& t, const char* method)
{
    py::require(t.metrics == 0, (std::string("snicholls_core.game.") + method +
                                 ": needs a scalar game (a one-dimensional table, or a game returning a number)")
                                    .c_str());
}

nb::object interaction_out(const std::optional<game::interaction_violation<double>>& v)
{
    if (!v)
        return nb::none();
    nb::dict d;
    d["base"] = v->base;
    d["first"] = v->first;
    d["second"] = v->second;
    d["difference"] = v->difference;
    return d;
}

// Binds name(table), name(table of metrics) and name(v, n) with one body(const game_table&, Extra...)
template <class... Extra, class Body, class... Args>
void def_game(nb::module_& m, const char* name, Body body, const char* doc, Args... args)
{
    m.def(name, [body](py::vector_in<double> t, Extra... e) { return body(table_of(t), e...); }, "table"_a, args...,
          doc);
    m.def(name, [body](py::matrix_in<double> t, Extra... e) { return body(table_of(t), e...); }, "table"_a, args...,
          doc);
    m.def(
        name,
        [body](nb::callable v, std::size_t n, Extra... e) {
            std::size_t calls = 0;
            const game_table t = tabulate(v, n, calls);
            return body(t, e...);
        },
        "v"_a, "n"_a, args..., doc);
}

any_numpy attribution_repr_values(const attribution& a)
{
    std::vector<double> v = a.values;
    if (a.metrics == 0)
        return any_numpy(py::to_numpy(std::move(v)));
    return any_numpy(py::to_numpy(std::move(v), a.players, a.metrics));
}

} // namespace

NB_MODULE(game, m)
{
    py::register_exception_translators();
    nb::module_::import_("snicholls_core.stats");   // Wyrand, taken by the samplers

    m.doc() =
        "Cooperative-game attribution (C++ snicholls::game): Shapley, Banzhaf, Owen, Harsanyi dividends, the "
        "Shapley samplers, change attribution, interactions and the game properties.\n\n"
        "A game is either a table of v(S) - a float64 array with one value per coalition, 2^n of them (or 2^n x m "
        "for m metrics at once), indexed by the coalition's bits (bit i set when player i is in) - or a Python "
        "callable v(s) taking the coalition as an int bitmask. Tables are the fast route: the arithmetic runs in C++ "
        "with the GIL released. A callable needs the GIL for every evaluation; the exact methods tabulate it first "
        "(2^n calls), Owen and the samplers call it as they go. Use set_game(f) for a callable written for "
        "frozensets of players, and memoized(v) for an expensive one.";

    // ---- coalitions ----
    m.attr("max_table_players") = game::max_table_players;
    m.def("singleton", [](std::size_t i) {
        py::require(i < 64, "snicholls_core.game.singleton: a player is below 64");
        return game::singleton(i);
    }, "i"_a, "The coalition of player i alone: 1 << i. C++: snicholls::game::singleton.");
    m.def("grand_coalition", [](std::size_t n) {
        py::require(n <= 64, "snicholls_core.game.grand_coalition: at most 64 players");
        return game::grand_coalition(n);
    }, "n"_a, "The coalition of all n players. C++: snicholls::game::grand_coalition.");
    m.def("contains", [](coalition s, std::size_t i) { return i < 64 && game::contains(s, i); }, "s"_a, "i"_a,
          "Whether player i is in s. C++: snicholls::game::contains.");
    m.def("with_player", [](coalition s, std::size_t i) {
        py::require(i < 64, "snicholls_core.game.with_player: a player is below 64");
        return game::with(s, i);
    }, "s"_a, "i"_a, "s with player i added. C++: snicholls::game::with (with is a Python keyword).");
    m.def("without", [](coalition s, std::size_t i) { return i < 64 ? game::without(s, i) : s; }, "s"_a, "i"_a,
          "s with player i removed. C++: snicholls::game::without.");
    m.def("coalition_size", &game::coalition_size, "s"_a, "The number of players in s. C++: snicholls::game::coalition_size.");
    m.def("table_size", [](std::size_t n) {
        py::require(n <= game::max_table_players, "snicholls_core.game.table_size: at most 30 players");
        return game::table_size(n);
    }, "n"_a, "2^n, the number of coalitions of n players. C++: snicholls::game::table_size.");
    m.def("members", [](coalition s) {
        std::vector<std::size_t> out;
        for (coalition b = s; b != 0; b &= b - 1)
            out.push_back(static_cast<std::size_t>(std::countr_zero(b)));
        return out;
    }, "s"_a, "The players in s, ascending. C++: iterate the set bits (for b = s; b; b &= b - 1).");
    m.def("from_members", [](const std::vector<std::size_t>& players) {
        coalition s = 0;
        for (const std::size_t i : players) {
            py::require(i < 64, "snicholls_core.game.from_members: a player is below 64");
            s = game::with(s, i);
        }
        return s;
    }, "players"_a, "The coalition of the given players. C++: snicholls::game::with, repeatedly.");
    m.def(
        "tabulate",
        [](nb::callable v, std::size_t n) {
            std::size_t calls = 0;
            game_table t = tabulate(v, n, calls);
            if (t.metrics == 0)
                return any_numpy(py::to_numpy(std::move(t.owned)));
            const std::size_t rows = t.size(), cols = t.metrics;
            return any_numpy(py::to_numpy(std::move(t.owned), rows, cols));
        },
        "v"_a, "n"_a,
        "v(s) for every coalition s of n players: the table every exact method accepts. 2^n calls, GIL held.\n\n"
        "C++: snicholls::game::tabulate(n, v).");

    nb::class_<python_memoized>(m, "Memoized", nb::type_slots(function_slots<python_memoized>),
                                "A game that evaluates each coalition once (C++ snicholls::game::memoized). Pass it "
                                "to owen and the samplers in place of the callable: cached values never reach Python.")
        .def("__call__", [](const python_memoized& self, coalition s) { return self.memo(s); }, "s"_a,
             "v(s), from the cache after the first call.")
        .def_prop_ro("evaluations", [](const python_memoized& self) { return self.memo.evaluations(); },
                     "Distinct coalitions evaluated so far. C++: memoized::evaluations().");
    m.def(
        "memoized", [](nb::callable v) { return python_memoized(std::move(v)); }, "v"_a,
        "Wraps a game returning a number so each coalition is evaluated once. C++: snicholls::game::memoized.");
    nb::class_<set_game>(m, "SetGame", nb::type_slots(function_slots<set_game>),
                         "A game written for frozensets of players, as the bitmask form the methods call.")
        .def("__call__", [](const set_game& self, coalition s) { return self(s); }, "s"_a,
             "f(frozenset of the players in s).");
    m.def(
        "set_game", [](nb::callable f) { return set_game{std::move(f)}; }, "f"_a,
        "Adapts a game written for frozensets of player indices, f(frozenset) -> value, to the bitmask form the "
        "methods call. Python only (C++ games take snicholls::game::coalition).");

    // ---- the attribution result ----
    nb::class_<attribution>(m, "Attribution",
                            "An attribution: the baseline v(none), the amount for each player, the total v(all). C++: "
                            "snicholls::game::attribution (sampled_attribution for the samplers).\n\n"
                            "For a scalar game baseline and total are floats and values has shape (n,); for a game "
                            "of m metrics they have shapes (m,), (m,) and (n, m).")
        .def_prop_ro("baseline", [](const attribution& a) { return metric_out(a, a.baseline); }, "v(none).")
        .def_prop_ro("total", [](const attribution& a) { return metric_out(a, a.total); }, "v(all).")
        .def_prop_ro("values", [](const attribution& a) { return attribution_repr_values(a); }, nb::rv_policy::move,
                     "The amount attributed to each player.")
        .def_prop_ro("standard_errors", [](const attribution& a) -> nb::object {
            if (!a.sampled)
                return nb::none();
            std::vector<double> v = a.standard_errors;
            return nb::cast(py::to_numpy(std::move(v)));
        }, "One per player for a sampled attribution (NaN with fewer than two samples); None for an exact one.")
        .def_ro("samples", &attribution::samples,
                "Samples averaged: orders (an antithetic pair counts once) or lifts per player; 0 when exact.")
        .def_ro("evaluations", &attribution::evaluations,
                "Calls the method made to the game, repeats included (2^n for the exact methods).")
        .def_prop_ro("players", [](const attribution& a) { return a.players; }, "n.")
        .def("attributed", [](const attribution& a) { return metric_out(a, a.attributed()); },
             "The baseline plus every player's amount. C++: attribution::attributed().")
        .def("unattributed", [](const attribution& a) {
            std::vector<double> out = a.attributed();
            for (std::size_t j = 0; j < out.size(); ++j)
                out[j] = a.total[j] - out[j];
            return metric_out(a, std::move(out));
        }, "total - attributed(): zero (to rounding) for a method with full attribution. C++: attribution::unattributed().")
        .def("__repr__", [](const attribution& a) {
            return nb::str("Attribution(baseline={}, values={}, total={})")
                .format(metric_out(a, a.baseline), attribution_repr_values(a), metric_out(a, a.total));
        });

    // ---- exact ----
    def_game(m, "shapley", [](const game_table& t) { return exact_shapley(t); },
             "Exact Shapley values from every coalition: from a table, with the GIL released, or from a callable "
             "tabulated first (2^n calls). The table route is the one to use up to about 20 players.\n\n"
             "C++: snicholls::game::shapley(n, v) (table_game for a table).");
    def_game(m, "banzhaf", [](const game_table& t) { return from_dividends<true>(t, true); },
             "Banzhaf values: each player's mean marginal contribution over the 2^(n-1) coalitions without it. Not "
             "efficient: unattributed() is not zero in general.\n\n"
             "C++: snicholls::game::to_dividends, then banzhaf_from_dividends (equal to snicholls::game::banzhaf<N>).");
    def_game(m, "harsanyi_dividends", [](const game_table& t) { return dividends_of(t); },
             "The Harsanyi dividends of every coalition (the Moebius transform of v), same shape as the table.\n\n"
             "C++: snicholls::game::to_dividends / harsanyi_dividends(n, v).");
    m.def("shapley_from_dividends", [](py::vector_in<double> d) { return from_dividends<false>(table_of(d), false); },
          "dividends"_a, "Shapley values from a table of dividends. C++: snicholls::game::shapley_from_dividends.");
    m.def("shapley_from_dividends", [](py::matrix_in<double> d) { return from_dividends<false>(table_of(d), false); },
          "dividends"_a, "Shapley values from a table of dividends. C++: snicholls::game::shapley_from_dividends.");
    m.def("banzhaf_from_dividends", [](py::vector_in<double> d) { return from_dividends<true>(table_of(d), false); },
          "dividends"_a, "Banzhaf values from a table of dividends. C++: snicholls::game::banzhaf_from_dividends.");
    m.def("banzhaf_from_dividends", [](py::matrix_in<double> d) { return from_dividends<true>(table_of(d), false); },
          "dividends"_a, "Banzhaf values from a table of dividends. C++: snicholls::game::banzhaf_from_dividends.");

    def_game(m, "interactions", [](const game_table& t) {
        require_scalar(t, "interactions");
        std::vector<double> out = py::without_gil([&] {
            return with_players(t.n, [&](auto N) {
                std::vector<double> flat(N * N);
                if constexpr (N < 2)   // all zero; core's shapley_interactions<0|1> warns (erratum/CORE.md #1)
                    return flat;
                else {
                const auto r = game::shapley_interactions<N>(game::table_game<double>(t.data));
                for (std::size_t i = 0; i < N; ++i)
                    for (std::size_t j = 0; j < N; ++j)
                        flat[i * N + j] = r[i][j];
                return flat;
                }
            });
        });
        return py::to_numpy(std::move(out), t.n, t.n);
    }, "The Shapley interaction index of every pair, an n x n array: positive for complements, negative for "
       "substitutes; zero on the diagonal. At most 20 players.\n\nC++: snicholls::game::shapley_interactions<N>.");

    // ---- Owen ----
    m.def(
        "owen",
        [](py::vector_in<double> table, const std::vector<std::vector<std::size_t>>& groups) {
            const game_table t = table_of(table);
            const std::vector<coalition> g = groups_of(t.n, groups);
            std::size_t calls = 0;
            const table_view v{t.data.data()};
            attribution a = py::without_gil([&] {
                return scalar_attribution(game::owen(t.n, counted<table_view>{&v, &calls}, std::span<const coalition>(g)), t.n);
            });
            a.evaluations = calls;
            return a;
        },
        "table"_a, "groups"_a,
        "The Owen value: Shapley between the groups, then within each group. groups is a list of lists of players "
        "that partitions 0..n-1. One group of everyone, or n singletons, gives the Shapley value.\n\n"
        "C++: snicholls::game::owen(n, v, groups).");
    m.def(
        "owen",
        [](nb::object v, std::size_t n, const std::vector<std::vector<std::size_t>>& groups) {
            py::require(n <= 64, "snicholls_core.game.owen: at most 64 players");
            const std::vector<coalition> g = groups_of(n, groups);
            std::size_t calls = 0;
            attribution a = with_scalar_game(v, [&](const auto& game_) {
                using G = std::remove_cvref_t<decltype(game_)>;
                return scalar_attribution(game::owen(n, counted<G>{&game_, &calls}, std::span<const coalition>(g)), n);
            });
            a.evaluations = calls;
            return a;
        },
        "v"_a, "n"_a, "groups"_a,
        "The Owen value of a game returning a number (or a Memoized), calling it as needed with the GIL held: "
        "sum over groups k of 2^(m-1) * 2^(b_k) evaluations rather than 2^n.\n\nC++: snicholls::game::owen(n, v, groups).");

    // ---- sampled ----
    m.def(
        "shapley_sampled",
        [](py::vector_in<double> table, stats::wyrand& g, std::size_t permutations, bool antithetic) {
            py::require(permutations > 0, "snicholls_core.game.shapley_sampled: permutations must be positive");
            const game_table t = table_of(table);
            const table_view v{t.data.data()};
            return py::without_gil([&] {
                return sampled_attribution(
                    game::shapley_sampled(t.n, v, g, game::permutation_sampling{permutations, antithetic}), t.n);
            });
        },
        "table"_a, "g"_a, "permutations"_a = 256, "antithetic"_a = true,
        "Shapley values from random orders of the players, with standard errors; every sample is efficient, so the "
        "values always sum to total - baseline. antithetic walks each order both ways (lower variance).\n\n"
        "C++: snicholls::game::shapley_sampled(n, v, g, permutation_sampling{permutations, antithetic}). The same "
        "Wyrand state gives the same result as C++.");
    m.def(
        "shapley_sampled",
        [](nb::object v, std::size_t n, stats::wyrand& g, std::size_t permutations, bool antithetic) {
            py::require(permutations > 0, "snicholls_core.game.shapley_sampled: permutations must be positive");
            py::require(n <= 64, "snicholls_core.game.shapley_sampled: at most 64 players");
            return with_scalar_game(v, [&](const auto& game_) {
                return sampled_attribution(
                    game::shapley_sampled(n, game_, g, game::permutation_sampling{permutations, antithetic}), n);
            });
        },
        "v"_a, "n"_a, "g"_a, "permutations"_a = 256, "antithetic"_a = true,
        "shapley_sampled for a game returning a number (or a Memoized), called with the GIL held.");
    m.def(
        "shapley_sampled_lifts",
        [](py::vector_in<double> table, stats::wyrand& g, std::size_t samples_per_player, bool scale_to_total) {
            py::require(samples_per_player > 0, "snicholls_core.game.shapley_sampled_lifts: samples_per_player must be positive");
            const game_table t = table_of(table);
            const table_view v{t.data.data()};
            return py::without_gil([&] {
                return sampled_attribution(
                    game::shapley_sampled_lifts(t.n, v, g, game::lift_sampling{samples_per_player, scale_to_total}), t.n);
            });
        },
        "table"_a, "g"_a, "samples_per_player"_a = 256, "scale_to_total"_a = true,
        "Shapley values from random lifts v(S + i) - v(S), S drawn with the Shapley weights, per player; "
        "scale_to_total rescales to full attribution.\n\nC++: snicholls::game::shapley_sampled_lifts.");
    m.def(
        "shapley_sampled_lifts",
        [](nb::object v, std::size_t n, stats::wyrand& g, std::size_t samples_per_player, bool scale_to_total) {
            py::require(samples_per_player > 0, "snicholls_core.game.shapley_sampled_lifts: samples_per_player must be positive");
            py::require(n <= 64, "snicholls_core.game.shapley_sampled_lifts: at most 64 players");
            return with_scalar_game(v, [&](const auto& game_) {
                return sampled_attribution(
                    game::shapley_sampled_lifts(n, game_, g, game::lift_sampling{samples_per_player, scale_to_total}), n);
            });
        },
        "v"_a, "n"_a, "g"_a, "samples_per_player"_a = 256, "scale_to_total"_a = true,
        "shapley_sampled_lifts for a game returning a number (or a Memoized), called with the GIL held.");

    // ---- change attribution ----
    m.def(
        "attribute_change",
        [](nb::callable f, py::vector_in<double> start, py::vector_in<double> end) {
            py::require(start.shape(0) == end.shape(0), "snicholls_core.game.attribute_change: start and end differ in length");
            py::require(start.shape(0) <= game::max_table_players, "snicholls_core.game.attribute_change: at most 30 inputs");
            const auto a0 = py::as_span(start), a1 = py::as_span(end);
            const std::vector<double> from(a0.begin(), a0.end()), to(a1.begin(), a1.end());
            std::size_t calls = 0;
            const auto fn = [&](const std::vector<double>& x) {
                ++calls;
                std::vector<double> copy = x;
                nb::object r = f(py::to_numpy(std::move(copy)));
                double d = 0;
                if (!nb::try_cast(r, d))
                    throw nb::type_error("snicholls_core.game.attribute_change: f must return a number");
                return d;
            };
            attribution a = scalar_attribution(game::attribute_change(fn, from, to), from.size());
            a.evaluations = calls;
            return a;
        },
        "f"_a, "start"_a, "end"_a,
        "Why did f move: the exact Shapley attribution of f(end) - f(start) to the inputs that changed. Player i is "
        "input i, taken from end when in the coalition and from start otherwise; f receives a float64 array. "
        "2^n calls of f; cross effects are shared fairly.\n\nC++: snicholls::game::attribute_change(f, from, to).");

    // ---- properties (at most 20 players) ----
    const auto bool_property = [&m](const char* name, auto method, const char* doc) {
        def_game<double>(m, name, [method](const game_table& t, double tolerance) {
            require_scalar(t, "properties");
            return py::without_gil([&] {
                return with_players(t.n, [&](auto N) { return method(N, game::table_game<double>(t.data), tolerance); });
            });
        }, doc, "tolerance"_a = 0.0);
    };
    bool_property("is_monotone", [](auto N, const auto& v, double tol) { return game::is_monotone<N>(v, tol); },
                  "Whether v(S + i) >= v(S) - tolerance for every S and i. At most 20 players. C++: snicholls::game::is_monotone<N>.");
    bool_property("is_superadditive", [](auto N, const auto& v, double tol) { return game::is_superadditive<N>(v, tol); },
                  "Whether v(S + T) >= v(S) + v(T) - tolerance for disjoint S, T (3^n pairs). C++: snicholls::game::is_superadditive<N>.");
    bool_property("is_supermodular", [](auto N, const auto& v, double tol) { return game::is_supermodular<N>(v, tol); },
                  "Whether every second difference is >= -tolerance (a convex game). C++: snicholls::game::is_supermodular<N>.");
    bool_property("is_submodular", [](auto N, const auto& v, double tol) { return game::is_submodular<N>(v, tol); },
                  "Whether every second difference is <= tolerance. C++: snicholls::game::is_submodular<N>.");
    bool_property("is_additive", [](auto N, const auto& v, double tol) { return game::is_additive<N>(v, tol); },
                  "Whether every second difference is within tolerance of 0 (no interactions). C++: snicholls::game::is_additive<N>.");

    const auto interaction_property = [&m](const char* name, auto method, const char* doc) {
        def_game<double>(m, name, [method](const game_table& t, double tolerance) {
            require_scalar(t, "properties");
            const auto found = py::without_gil([&] {
                return with_players(t.n, [&](auto N) { return method(N, game::table_game<double>(t.data), tolerance); });
            });
            return interaction_out(found);
        }, doc, "tolerance"_a = 0.0);
    };
    interaction_property("find_supermodularity_violation",
                         [](auto N, const auto& v, double tol) { return game::find_supermodularity_violation<N>(v, tol); },
                         "The first second difference below -tolerance, as {base, first, second, difference}, or None. "
                         "C++: snicholls::game::find_supermodularity_violation<N>.");
    interaction_property("find_submodularity_violation",
                         [](auto N, const auto& v, double tol) { return game::find_submodularity_violation<N>(v, tol); },
                         "The first second difference above tolerance, as {base, first, second, difference}, or None. "
                         "C++: snicholls::game::find_submodularity_violation<N>.");
    interaction_property("find_interaction",
                         [](auto N, const auto& v, double tol) { return game::find_interaction<N>(v, tol); },
                         "The first second difference farther than tolerance from 0, as {base, first, second, difference}, "
                         "or None. C++: snicholls::game::find_interaction<N>.");

    def_game<double>(m, "find_monotonicity_violation", [](const game_table& t, double tolerance) -> nb::object {
        require_scalar(t, "properties");
        const auto found = py::without_gil([&] {
            return with_players(t.n, [&](auto N) { return game::find_monotonicity_violation<N>(game::table_game<double>(t.data), tolerance); });
        });
        if (!found)
            return nb::none();
        nb::dict d;
        d["base"] = found->base;
        d["player"] = found->player;
        d["lift"] = found->lift;
        return d;
    }, "The first lift below -tolerance, as {base, player, lift}, or None. C++: snicholls::game::find_monotonicity_violation<N>.",
       "tolerance"_a = 0.0);

    def_game<double>(m, "find_superadditivity_violation", [](const game_table& t, double tolerance) -> nb::object {
        require_scalar(t, "properties");
        const auto found = py::without_gil([&] {
            return with_players(t.n, [&](auto N) { return game::find_superadditivity_violation<N>(game::table_game<double>(t.data), tolerance); });
        });
        if (!found)
            return nb::none();
        nb::dict d;
        d["first"] = found->first;
        d["second"] = found->second;
        d["shortfall"] = found->shortfall;
        return d;
    }, "The first disjoint pair with v(S + T) < v(S) + v(T) - tolerance, as {first, second, shortfall}, or None. "
       "C++: snicholls::game::find_superadditivity_violation<N>.", "tolerance"_a = 0.0);

    const auto core_check = [](const game_table& t, const py::vector_in<double>& allocation, double tolerance) {
        require_scalar(t, "core");
        py::require(allocation.shape(0) == t.n, "snicholls_core.game: the allocation has one amount per player");
        const auto alloc = py::as_span(allocation);
        return py::without_gil([&] {
            return with_players(t.n, [&](auto N) {
                std::array<double, N> a{};
                for (std::size_t i = 0; i < N; ++i)   // not std::copy: GCC 13 -O3 warns falsely for N < 2
                    a[i] = alloc[i];
                return game::find_core_violation<N>(game::table_game<double>(t.data), a, tolerance);
            });
        });
    };
    def_game<py::vector_in<double>, double>(m, "find_core_violation", [core_check](const game_table& t, py::vector_in<double> allocation, double tolerance) -> nb::object {
        const auto found = core_check(t, allocation, tolerance);
        if (!found)
            return nb::none();
        nb::dict d;
        d["blocking"] = found->blocking;
        d["shortfall"] = found->shortfall;
        return d;
    }, "A coalition the allocation gives less than v(S) - v(none) (allocation minus value below -tolerance), or the "
       "grand coalition if the total is wrong, as {blocking, shortfall}; None if the allocation is in the core. "
       "C++: snicholls::game::find_core_violation<N>.", "allocation"_a, "tolerance"_a = 0.0);
    def_game<py::vector_in<double>, double>(m, "in_core", [core_check](const game_table& t, py::vector_in<double> allocation, double tolerance) {
        return !core_check(t, allocation, tolerance).has_value();
    }, "Whether the allocation is in the core. C++: snicholls::game::in_core<N>.", "allocation"_a, "tolerance"_a = 0.0);
}
