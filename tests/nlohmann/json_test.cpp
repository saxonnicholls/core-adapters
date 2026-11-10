//
//  json_test.cpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//
//  Tests for <snicholls/core_adapters/nlohmann/json.hpp>: json_error, and the vendored nlohmann's configuration.
//

#include <snicholls/core_adapters/nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <type_traits>

namespace {

static_assert(std::is_base_of_v<std::runtime_error, snicholls::json_error>);
static_assert(!std::is_base_of_v<nlohmann::json::exception, snicholls::json_error>);
static_assert(JSON_USE_IMPLICIT_CONVERSIONS == 0, "the vendored nlohmann/json is built with JSON_ImplicitConversions=OFF");
static_assert(NLOHMANN_JSON_VERSION_MAJOR == 3 && NLOHMANN_JSON_VERSION_MINOR == 12 && NLOHMANN_JSON_VERSION_PATCH == 0,
              "the pin: third_party/nlohmann_json at v3.12.0");

TEST(CoreJsonError, IsARuntimeErrorWithTheMessage)
{
    const snicholls::json_error e("snicholls::uuid from_json: nope");
    const std::runtime_error& base = e;
    EXPECT_STREQ(base.what(), "snicholls::uuid from_json: nope");
}

TEST(CoreJsonError, MessageQuotesAtMost64CharactersOfInput)
{
    try {
        snicholls::detail::throw_json_error("T", "bad:", std::string(100, 'a'));
        FAIL() << "did not throw";
    } catch (const snicholls::json_error& e) {
        const std::string what = e.what();
        EXPECT_EQ(what, "T from_json: bad: \"" + std::string(64, 'a') + "...\"");
    }
}

TEST(CoreJsonError, ShortInputIsQuotedWhole)
{
    EXPECT_THROW(
        {
            try {
                snicholls::detail::throw_json_error("T", "bad:", "xyz");
            } catch (const snicholls::json_error& e) {
                EXPECT_STREQ(e.what(), "T from_json: bad: \"xyz\"");
                throw;
            }
        },
        snicholls::json_error);
}

TEST(CoreJsonStringOrThrow, ReturnsTheStringOrNamesTheTypeReceived)
{
    const nlohmann::json s = "text";
    EXPECT_EQ(snicholls::detail::json_string_or_throw(s, "T", "a string"), "text");
    try {
        static_cast<void>(snicholls::detail::json_string_or_throw(nlohmann::json(5), "T", "a string"));
        FAIL() << "did not throw";
    } catch (const snicholls::json_error& e) {
        EXPECT_STREQ(e.what(), "T from_json: expected a string, got \"number\"");
    }
}

TEST(CoreJsonImplicitConversions, AreOffSoGetIsExplicit)
{
    const nlohmann::json j = 3;
    static_assert(!std::is_convertible_v<nlohmann::json, int>);
    EXPECT_EQ(j.get<int>(), 3);
}

} // namespace
