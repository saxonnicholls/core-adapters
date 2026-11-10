//
//  secret_json.hpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//

/// \file
/// \brief nlohmann/json hook for `snicholls::secret<T>`: read only. A secret is never written to JSON.
/// \ingroup core_json
///
/// \overview
/// Configuration arrives as JSON with credentials in it, so `j.get<snicholls::secret<std::string>>()` reads the
/// value into a secret straight away, and the plain value never sits in an ordinary variable. The other direction
/// is deliberately missing: there is no `to_json` for `secret<T>`, so `nlohmann::json j = s;` does not compile.
/// Writing the mask would lose the value silently; writing the value would leak it. To send a secret on purpose,
/// write `s.reveal()` - a call that is easy to find in review.
///
/// \snippet nlohmann/secret_json_demo.cpp secret
///
/// \relations
/// - `snicholls::secret` (`secret.hpp`); `snicholls::json_error` (`json.hpp`).
/// - Group: \ref core_json.
///
/// \contracts
/// - **The value never appears in an error:** a failure names the expected type and the JSON type received, never
///   the text.
/// - Strong guarantee: on failure the target secret is unchanged.
///
/// \errors
/// `snicholls::json_error` for any value that does not convert to `T` (nlohmann's own `type_error` /
/// `out_of_range` are caught and translated, without their message, which may quote the value).
///
/// \perf
/// Not benchmarked.
///
/// \testing
/// - Tests: tests/nlohmann/secret_json_test.cpp (including that `nlohmann::json(secret)` is ill-formed).
/// - Demo: demos/nlohmann/secret_json_demo.cpp.
///
/// \deps
/// CMake target `snicholls::core_json`.
///
/// \gotchas
/// - `T` must itself be readable by nlohmann/json (`j.get<T>()`).
///
/// \status
/// Experimental, pre-1.0.

#ifndef snicholls_core_adapters_nlohmann_secret_json_hpp
#define snicholls_core_adapters_nlohmann_secret_json_hpp

#include <string>
#include <utility>

#include <snicholls/core_adapters/nlohmann/json.hpp>
#include <snicholls/core/secret.hpp>

namespace snicholls {

/// \brief Reads a secret from JSON. There is no `to_json`: a secret is never written.
/// \ingroup core_json
/// \tparam T the secret's value type, readable by `j.get<T>()`.
/// \param j the JSON value.
/// \param s receives the secret; unchanged on failure.
/// \throws snicholls::json_error if `j` does not convert to `T` (the message never quotes the value).
template <typename T>
void from_json(const nlohmann::json& j, secret<T>& s)
{
    T value{};
    try {
        j.get_to(value);
    } catch (const nlohmann::json::exception&) {
        // Translated without nlohmann's message: it can quote the value.
        std::string what("snicholls::secret from_json: the value does not convert to the secret's type (got ");
        what += j.type_name();
        what += ')';
        throw json_error(what);
    }
    s = secret<T>(std::move(value));
}

} // namespace snicholls

#endif /* snicholls_core_adapters_nlohmann_secret_json_hpp */
