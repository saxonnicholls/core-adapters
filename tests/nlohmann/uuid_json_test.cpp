//
//  uuid_json_test.cpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//
//  Tests for <snicholls/core_adapters/nlohmann/uuid_json.hpp>: canonical lowercase out, canonical (either case) in.
//

#include <snicholls/core_adapters/nlohmann/uuid_json.hpp>

#include "../support/json_kinds.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {

using namespace snicholls::literals;

TEST(CoreJsonUuid, WritesCanonicalLowercase)
{
    EXPECT_EQ(nlohmann::json("F81D4FAE-7DEC-11D0-A765-00A0C91E6BF6"_uuid), "f81d4fae-7dec-11d0-a765-00a0c91e6bf6");
    EXPECT_EQ(nlohmann::json(snicholls::uuid{}), "00000000-0000-0000-0000-000000000000");
}

TEST(CoreJsonUuid, RoundTripsAndReadsEitherCase)
{
    const auto id = "0190a5c4-7e1b-7c3a-9f12-3b4c5d6e7f80"_uuid;
    EXPECT_EQ(nlohmann::json(id).get<snicholls::uuid>(), id);
    EXPECT_EQ(nlohmann::json("0190A5C4-7E1B-7C3A-9F12-3B4C5D6E7F80").get<snicholls::uuid>(), id);
    const auto max = "ffffffff-ffff-ffff-ffff-ffffffffffff"_uuid;
    EXPECT_EQ(nlohmann::json(max).get<snicholls::uuid>(), max);
}

TEST(CoreJsonUuid, NonCanonicalFormsAreRefused)
{
    for (const char* bad : {"{0190a5c4-7e1b-7c3a-9f12-3b4c5d6e7f80}", "urn:uuid:0190a5c4-7e1b-7c3a-9f12-3b4c5d6e7f80",
                            "0190a5c47e1b7c3a9f123b4c5d6e7f80", "0190a5c4-7e1b-7c3a-9f12-3b4c5d6e7f8", "", "g190a5c4-7e1b-7c3a-9f12-3b4c5d6e7f80"}) {
        EXPECT_THROW(static_cast<void>(nlohmann::json(bad).get<snicholls::uuid>()), snicholls::json_error) << bad;
    }
}

TEST(CoreJsonUuid, FailureLeavesTheTargetUnchanged)
{
    auto id = "0190a5c4-7e1b-7c3a-9f12-3b4c5d6e7f80"_uuid;
    EXPECT_THROW(nlohmann::json(12).get_to(id), snicholls::json_error);
    EXPECT_EQ(id, "0190a5c4-7e1b-7c3a-9f12-3b4c5d6e7f80"_uuid);
}

TEST(CoreJsonUuid, NoNlohmannExceptionEscapesForAnyKindOfValue)
{
    for (const nlohmann::json& j : core_adapters_test::every_kind_of_json()) {
        try {
            static_cast<void>(j.get<snicholls::uuid>());
        } catch (const snicholls::json_error&) {
        } catch (const nlohmann::json::exception& e) {
            ADD_FAILURE() << "nlohmann exception escaped: " << e.what();
        }
    }
}

} // namespace
