//
//  reference_values.cpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//
//  Prints, as JSON, what core's C++ computes for the cases the Python tests replay through the binding: seeded
//  draws, shuffles and the Shapley samplers (on a table and on a function), Owen and exact Shapley. The Python tests
//  (tests/python/stats_test.py and game_test.py) run this program and require bit-for-bit equality, so a binding that changed
//  a draw, an order or an evaluation would be caught. Takes no input; the cases are fixed here and in the test.
//

#include <snicholls/core/game/coalition.hpp>
#include <snicholls/core/game/owen.hpp>
#include <snicholls/core/game/shapley.hpp>
#include <snicholls/core/stats/random.hpp>

#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <numeric>
#include <span>
#include <type_traits>
#include <vector>

namespace game = snicholls::game;
namespace stats = snicholls::stats;

namespace {

// The deterministic test table of n players: v(S) = sum of (i + 1) for i in S, plus 0.25 * |S|^2, plus 0.1 when
// players 0 and 1 are both in (an interaction). The Python test builds the same table.
std::vector<double> test_table(std::size_t n)
{
    std::vector<double> t(game::table_size(n));
    for (game::coalition s = 0; s < t.size(); ++s) {
        double v = 0;
        for (std::size_t i = 0; i < n; ++i)
            if (game::contains(s, i))
                v += static_cast<double>(i + 1);
        const auto k = static_cast<double>(std::popcount(s));
        v += 0.25 * k * k + ((s & 3u) == 3u ? 0.1 : 0.0);
        t[s] = v;
    }
    return t;
}

// A minimal JSON writer: numbers with 17 significant digits (exact binary64 round trip), NaN as null
std::string num(double x)
{
    if (std::isnan(x))
        return "null";
    std::ostringstream os;
    os.precision(17);
    os << x;
    return os.str();
}

template <class T>
std::string list(const T& values)
{
    std::string out = "[";
    for (const auto& v : values) {
        if (out.size() > 1)
            out += ", ";
        if constexpr (std::is_floating_point_v<std::remove_cvref_t<decltype(v)>>)
            out += num(v);
        else
            out += std::to_string(v);
    }
    return out + "]";
}

template <class A>
std::string out(const A& a)
{
    std::string j = "{\"baseline\": " + num(a.baseline) + ", \"total\": " + num(a.total) + ", \"values\": " + list(a.values);
    if constexpr (requires { a.standard_errors; })
        j += ", \"standard_errors\": " + list(a.standard_errors) + ", \"samples\": " + std::to_string(a.samples) +
             ", \"evaluations\": " + std::to_string(a.evaluations);
    return j + "}";
}

} // namespace

int main()
{
    std::vector<std::string> fields;
    const auto add = [&fields](const std::string& key, const std::string& value) {
        fields.push_back("\"" + key + "\": " + value);
    };
    {
        stats::wyrand g(2024);
        std::vector<std::uint64_t> draws, idx;
        std::vector<double> reals;
        for (int i = 0; i < 5; ++i)
            draws.push_back(g());
        for (int i = 0; i < 5; ++i)
            idx.push_back(stats::uniform_index(g, 1000));
        for (int i = 0; i < 5; ++i)
            reals.push_back(stats::uniform_real(g));
        std::vector<std::int64_t> perm(20);
        std::iota(perm.begin(), perm.end(), 0);
        stats::shuffle(std::span<std::int64_t>(perm), g);
        add("draws", "{\"seed\": 2024, \"u64\": " + list(draws) + ", \"index_1000\": " + list(idx) + ", \"real\": " +
                         list(reals) + ", \"permutation_20\": " + list(perm) + "}");
    }
    const std::vector<double> t6 = test_table(6);
    const auto v6 = game::from_table(t6);
    add("table6", list(t6));
    add("shapley6", out(game::shapley(6, v6)));
    {
        stats::wyrand g(11);
        add("sampled_table", out(game::shapley_sampled(6, v6, g, game::permutation_sampling{64, true})));
        add("sampled_table_next_draw", std::to_string(g()));   // the state after the call, so Python checks it too
    }
    {
        stats::wyrand g(12);
        add("sampled_plain", out(game::shapley_sampled(6, v6, g, game::permutation_sampling{50, false})));
    }
    {
        stats::wyrand g(13);
        add("lifts_table", out(game::shapley_sampled_lifts(6, v6, g, game::lift_sampling{40, true})));
    }
    {
        stats::wyrand g(14);
        add("lifts_unscaled", out(game::shapley_sampled_lifts(6, v6, g, game::lift_sampling{40, false})));
    }
    {
        const std::vector<game::coalition> groups{0b000011, 0b001100, 0b110000};
        add("owen6", out(game::owen(6, v6, std::span<const game::coalition>(groups))));
    }
    std::cout << "{\n";
    for (std::size_t i = 0; i < fields.size(); ++i)
        std::cout << " " << fields[i] << (i + 1 < fields.size() ? ",\n" : "\n");
    std::cout << "}\n";
    return 0;
}
