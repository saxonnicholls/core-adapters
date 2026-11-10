//
//  time_json_test.cpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//
//  Tests for <snicholls/core_adapters/nlohmann/time_json.hpp>: canonical RFC 3339 out, strict RFC 3339 in.
//

#include <snicholls/core_adapters/nlohmann/time_json.hpp>

#include "../support/json_kinds.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <string>

namespace {

using snicholls::utc;
using snicholls::utc_ms;
using snicholls::utc_s;

TEST(CoreJsonTime, WritesCanonicalTextWithTheResolutionsDigits)
{
    const auto ns = utc<>::from_unix(std::chrono::nanoseconds(1714486985123456789));
    EXPECT_EQ(nlohmann::json(ns), "2024-04-30T14:23:05.123456789Z");
    EXPECT_EQ(nlohmann::json(utc_ms::from_unix_millis(1714486985123)), "2024-04-30T14:23:05.123Z");
    EXPECT_EQ(nlohmann::json(utc_s::from_unix_seconds(1714486985)), "2024-04-30T14:23:05Z");
    EXPECT_EQ(nlohmann::json(utc<>::epoch()), "1970-01-01T00:00:00.000000000Z");
}

TEST(CoreJsonTime, RoundTripsAcrossTheRange)
{
    for (const std::int64_t s : {std::int64_t{-62135596800}, std::int64_t{-1}, std::int64_t{0}, std::int64_t{951782400},
                                 std::int64_t{253402300799}}) {
        const auto t = utc_s::from_unix_seconds(s);
        EXPECT_EQ(nlohmann::json(t).get<utc_s>(), t) << s;
    }
    const auto ns = utc<>::from_unix(std::chrono::nanoseconds(-1));
    EXPECT_EQ(nlohmann::json(ns).get<utc<>>(), ns);
}

TEST(CoreJsonTime, ReadsAndAppliesAnOffset)
{
    const nlohmann::json j = "2024-04-30T10:23:05-04:00";
    EXPECT_EQ(j.get<utc_s>(), utc_s::from_unix_seconds(1714486985));
}

TEST(CoreJsonTime, StrictInputIsRefused)
{
    for (const char* bad : {"2024-04-30T14:23:05", "2024-04-30T14:23Z", "2024-02-30T00:00:00Z", "2016-12-31T23:59:60Z",
                            "2024-04-30T14:23:05.123Z" /* too precise for seconds */, "", "yesterday"}) {
        EXPECT_THROW(static_cast<void>(nlohmann::json(bad).get<utc_s>()), snicholls::json_error) << bad;
    }
}

TEST(CoreJsonTime, ANumberIsRefusedBecauseItsUnitWouldBeAGuess)
{
    try {
        static_cast<void>(nlohmann::json(1714486985).get<utc<>>());
        FAIL() << "did not throw";
    } catch (const snicholls::json_error& e) {
        EXPECT_NE(std::string(e.what()).find("expected an RFC 3339 string, got \"number\""), std::string::npos) << e.what();
    }
}

TEST(CoreJsonTime, FailureLeavesTheTargetUnchanged)
{
    auto t = utc_s::from_unix_seconds(42);
    EXPECT_THROW(nlohmann::json("nope").get_to(t), snicholls::json_error);
    EXPECT_EQ(t, utc_s::from_unix_seconds(42));
}

TEST(CoreJsonTime, NoNlohmannExceptionEscapesForAnyKindOfValue)
{
    for (const nlohmann::json& j : core_adapters_test::every_kind_of_json()) {
        try {
            static_cast<void>(j.get<utc<>>());
        } catch (const snicholls::json_error&) {
        } catch (const nlohmann::json::exception& e) {
            ADD_FAILURE() << "nlohmann exception escaped: " << e.what();
        }
    }
}

TEST(CoreJsonTime, AYearOutsideRfc3339IsAJsonErrorOnOutput)
{
    const auto far = utc_s::from_unix_seconds(253402300800);   // 10000-01-01
    EXPECT_THROW(nlohmann::json j = far, snicholls::json_error);
}

} // namespace
