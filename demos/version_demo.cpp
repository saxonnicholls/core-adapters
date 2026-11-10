//
//  version_demo.cpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//
//  Demo for <snicholls/core_adapters/version.hpp>: prints both versions and checks them.
//

#include <snicholls/core_adapters/version.hpp>

#include <cstdio>
#include <string>

int main()
{
    //! [version]
    SNICHOLLS_CORE_ADAPTERS_REQUIRE_VERSION(0, 1, 0);   // fails to compile against an older core-adapters
    const std::string adapters(snicholls::core_adapters::version_string());
    const std::string core(snicholls::core_adapters::core_version_string());
    std::printf("core-adapters %s, built against snicholls-core %s\n", adapters.c_str(), core.c_str());
    //! [version]
    if (adapters != SNICHOLLS_CORE_ADAPTERS_VERSION_STRING || snicholls::core::version() < SNICHOLLS_CORE_VERSION_CHECK(0, 5, 1)) {
        std::printf("version_demo: FAILED - expected core 0.5.1 or newer\n");
        return 1;
    }
    std::printf("version_demo: ok\n");
    return 0;
}
