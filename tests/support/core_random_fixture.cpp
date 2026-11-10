//
//  core_random_fixture.cpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//
//  core's snicholls::stats generator and draws against tests/fixtures/wyrand_sequences.json, computed independently
//  in Python integers by scripts/generate_random_fixture.py. The Python binding is checked against the same file
//  (tests/python/stats_test.py), so C++, the binding and the independent implementation agree bit for bit.
//

#include <snicholls/core/stats/random.hpp>

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <fstream>
#include <numeric>
#include <span>
#include <vector>

namespace {

namespace stats = snicholls::stats;

const nlohmann::json& fixture()
{
    static const nlohmann::json j = [] {
        std::ifstream in(CORE_ADAPTERS_FIXTURES "/wyrand_sequences.json");
        return nlohmann::json::parse(in);
    }();
    return j;
}

TEST(CoreRandomFixture, WyrandDrawsMatchTheIndependentImplementation)
{
    for (const auto& [seed, draws] : fixture().at("draws").items()) {
        stats::wyrand g(std::stoull(seed));
        for (const auto& d : draws)
            EXPECT_EQ(g(), d.get<std::uint64_t>()) << "seed " << seed;
    }
}

TEST(CoreRandomFixture, StreamsMatch)
{
    for (const auto& s : fixture().at("streams")) {
        auto g = stats::wyrand::stream(s.at("seed").get<std::uint64_t>(), s.at("k").get<std::uint64_t>());
        for (const auto& d : s.at("draws"))
            EXPECT_EQ(g(), d.get<std::uint64_t>());
    }
}

TEST(CoreRandomFixture, UniformIndexMatchesIncludingRejections)
{
    for (const auto& s : fixture().at("uniform_index")) {
        stats::wyrand g(s.at("seed").get<std::uint64_t>());
        const auto n = s.at("n").get<std::uint64_t>();
        for (const auto& d : s.at("draws"))
            EXPECT_EQ(stats::uniform_index(g, n), d.get<std::uint64_t>()) << "n " << n;
    }
}

TEST(CoreRandomFixture, UniformRealMatchesExactly)
{
    const auto& r = fixture().at("uniform_real");
    stats::wyrand g(r.at("seed").get<std::uint64_t>());
    for (const auto& d : r.at("draws"))
        EXPECT_EQ(stats::uniform_real(g), d.get<double>());
}

TEST(CoreRandomFixture, ShuffleMatches)
{
    for (const auto& s : fixture().at("shuffle")) {
        std::vector<std::int64_t> v(s.at("size").get<std::size_t>());
        std::iota(v.begin(), v.end(), 0);
        stats::wyrand g(s.at("seed").get<std::uint64_t>());
        stats::shuffle(std::span<std::int64_t>(v), g);
        EXPECT_EQ(v, s.at("result").get<std::vector<std::int64_t>>());
    }
}

} // namespace
