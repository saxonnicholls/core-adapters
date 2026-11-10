//
//  decimal_json_test.cpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//
//  Tests for <snicholls/core_adapters/nlohmann/decimal_json.hpp>: exact canonical text as a JSON string; numbers refused.
//

#include <snicholls/core_adapters/nlohmann/decimal_json.hpp>

#include "../support/json_kinds.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {

snicholls::decimal dec(const char* text) { return snicholls::decimal::parse(text).value(); }

TEST(CoreJsonDecimal, WritesTheCanonicalTextAsAString)
{
    EXPECT_EQ(nlohmann::json(dec("72.5")), "72.5");
    EXPECT_EQ(nlohmann::json(dec("0.0000001")), "0.0000001");
    EXPECT_EQ(nlohmann::json(dec("-5.25")), "-5.25");
    EXPECT_EQ(nlohmann::json(snicholls::decimal{}), "0");
    EXPECT_TRUE(nlohmann::json(dec("0.1")).is_string());
}

TEST(CoreJsonDecimal, RoundTripsExactly)
{
    for (const char* text : {"0.1", "123456789012345678", "-0.000000000000000001", "1000000", "3.14159"}) {
        const auto d = dec(text);
        EXPECT_EQ(nlohmann::json(d).get<snicholls::decimal>(), d) << text;
        EXPECT_EQ(nlohmann::json(d).dump(), std::string("\"") + text + "\"");
    }
}

TEST(CoreJsonDecimal, NonCanonicalTextIsRefused)
{
    for (const char* bad : {"72.50", "+1", "1e5", "01", ".5", "-0", "1234567890123456789", "", "abc"}) {
        EXPECT_THROW(static_cast<void>(nlohmann::json(bad).get<snicholls::decimal>()), snicholls::json_error) << bad;
    }
}

TEST(CoreJsonDecimal, AJsonNumberIsRefusedRatherThanGuessedAt)
{
    EXPECT_THROW(static_cast<void>(nlohmann::json(0.1).get<snicholls::decimal>()), snicholls::json_error);
    EXPECT_THROW(static_cast<void>(nlohmann::json(5).get<snicholls::decimal>()), snicholls::json_error);
}

TEST(CoreJsonDecimal, FailureLeavesTheTargetUnchanged)
{
    auto d = dec("1.5");
    EXPECT_THROW(nlohmann::json("1.50").get_to(d), snicholls::json_error);
    EXPECT_EQ(d, dec("1.5"));
}

TEST(CoreJsonDecimal, NoNlohmannExceptionEscapesForAnyKindOfValue)
{
    for (const nlohmann::json& j : core_adapters_test::every_kind_of_json()) {
        try {
            static_cast<void>(j.get<snicholls::decimal>());
        } catch (const snicholls::json_error&) {
        } catch (const nlohmann::json::exception& e) {
            ADD_FAILURE() << "nlohmann exception escaped: " << e.what();
        }
    }
}

} // namespace
