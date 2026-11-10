//
//  version_test.cpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//
//  Tests for <snicholls/core_adapters/version.hpp>.
//

#include <snicholls/core_adapters/version.hpp>

#include <gtest/gtest.h>

#include <string>

namespace {

namespace ca = snicholls::core_adapters;

static_assert(ca::version() == ca::version_major() * 10000 + ca::version_minor() * 100 + ca::version_patch());
static_assert(noexcept(ca::version_string()));
SNICHOLLS_CORE_ADAPTERS_REQUIRE_VERSION(0, 1, 0);
SNICHOLLS_CORE_REQUIRE_VERSION(0, 5, 1);   // the minimum, as snicholls_core_require(0.5.1) in CMakeLists.txt

TEST(CoreAdaptersVersion, StringIsMajorDotMinorDotPatch)
{
    const std::string expected = std::to_string(ca::version_major()) + "." + std::to_string(ca::version_minor()) + "." +
                                 std::to_string(ca::version_patch());
    EXPECT_EQ(ca::version_string(), expected);
}

TEST(CoreAdaptersVersion, CoreVersionIsTheOneInTheBuild)
{
    EXPECT_EQ(ca::core_version_string(), snicholls::core::version_string());
    EXPECT_GE(snicholls::core::version(), SNICHOLLS_CORE_VERSION_CHECK(0, 5, 1));
}

} // namespace
