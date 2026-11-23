//
//  core_adapters.hpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//

/// \file
/// \brief The umbrella: every core-adapters header that needs nothing but the standard library and core.
/// \ingroup core_adapters
///
/// **Overview.** One include for the part of core-adapters that links no third party, and the source of
/// the single include `snicholls_core_adapters.hpp` (tooling's `single_include.py`), which is that part with the core
/// headers it uses in one file, attached to every GitHub release and never committed. Every adapter needs its third-party library by nature, so they are
/// left out (`single_include_opt_in` in `tooling.conf`) and are included one by one, through their own targets:
/// `snicholls/core_adapters/nanobind/<name>.hpp` (`snicholls::core_nanobind`: nanobind and Python) and
/// `snicholls/core_adapters/nlohmann/<name>.hpp` (`snicholls::core_json`: nlohmann/json).
///
/// **Relationships.** `version.hpp`. Group: \ref core_adapters.
///
/// **Guarantees.** Those of the headers it includes.
///
/// **Errors.** None.
///
/// **Performance.** Compile time only.
///
/// **Tests and demos.** None of its own: an umbrella, exempt from the mirror (`scripts/check_mirror.py`); every build
/// generates the single include from it and compiles that alone (`core_adapters_single_include`).
///
/// **Dependencies.** The standard library and snicholls-core.
///
/// **Known issues.** None.
///
/// **Status.** Experimental, pre-1.0.

#ifndef snicholls_core_adapters_core_adapters_hpp
#define snicholls_core_adapters_core_adapters_hpp

#include <snicholls/core_adapters/version.hpp>

#endif /* snicholls_core_adapters_core_adapters_hpp */
