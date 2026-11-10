//
//  secret_json_test.cpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//
//  Tests for <snicholls/core_adapters/nlohmann/secret_json.hpp>: read only, never written, never quoted in an error.
//

#include <snicholls/core_adapters/nlohmann/secret_json.hpp>

#include "../support/json_kinds.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <type_traits>

namespace {

using snicholls::secret;

// There is no to_json: a secret never becomes JSON, not even its mask
static_assert(!std::is_constructible_v<nlohmann::json, secret<std::string>>);
static_assert(!std::is_constructible_v<nlohmann::json, const secret<std::string>&>);

TEST(CoreJsonSecret, ReadsTheValueStraightIntoASecret)
{
    const nlohmann::json config = {{"api_key", "sk-live-123"}, {"port", 5432}};
    const auto key = config.at("api_key").get<secret<std::string>>();
    EXPECT_EQ(key.reveal(), "sk-live-123");
    const auto port = config.at("port").get<secret<std::uint16_t>>();
    EXPECT_EQ(port.reveal(), 5432);
}

TEST(CoreJsonSecret, AnErrorNamesTheTypesAndNeverTheValue)
{
    const std::string value = "sk-live-do-not-print";
    try {
        static_cast<void>(nlohmann::json(value).get<secret<int>>());
        FAIL() << "did not throw";
    } catch (const snicholls::json_error& e) {
        const std::string what = e.what();
        EXPECT_EQ(what.find(value), std::string::npos) << what;
        EXPECT_EQ(what.find("sk-live"), std::string::npos) << what;
        EXPECT_NE(what.find("got string"), std::string::npos) << what;
    }
}

TEST(CoreJsonSecret, OutOfRangeNumbersWrapAsInNlohmann)
{
    // nlohmann's get<std::uint16_t>() converts with static_cast and does not range-check (erratum/NLOHMANN-JSON.md #1),
    // so a secret of a narrow integer type sees the wrapped value. Read a wider type and check the range yourself.
    const auto port = nlohmann::json(70000).get<secret<std::uint16_t>>();
    EXPECT_EQ(port.reveal(), static_cast<std::uint16_t>(70000));
}

TEST(CoreJsonSecret, FailureLeavesTheTargetUnchanged)
{
    secret<std::string> s("kept");
    EXPECT_THROW(nlohmann::json(1).get_to(s), snicholls::json_error);
    EXPECT_EQ(s.reveal(), "kept");
}

TEST(CoreJsonSecret, NoNlohmannExceptionEscapesForAnyKindOfValue)
{
    for (const nlohmann::json& j : core_adapters_test::every_kind_of_json()) {
        try {
            static_cast<void>(j.get<secret<std::string>>());
            static_cast<void>(j.get<secret<double>>());
        } catch (const snicholls::json_error&) {
        } catch (const nlohmann::json::exception& e) {
            ADD_FAILURE() << "nlohmann exception escaped: " << e.what();
        }
    }
}

} // namespace
