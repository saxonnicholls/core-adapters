//
//  json.hpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//

/// \file
/// \brief `snicholls::json_error` and the nlohmann/json adapter layer: shared by every adapter.
/// \ingroup core_json
///
/// \overview
/// snicholls-core is standard-library-only and stays so. Its value types still need a JSON form, and the snicholls
/// libraries that speak JSON use nlohmann/json, so the JSON hooks live in this repository, under
/// `include/snicholls/core_adapters/nlohmann/`, behind their own CMake target `snicholls::core_json`.
///
/// Include the adapter for the type you serialise - `time_json.hpp`, `uuid_json.hpp`, `decimal_json.hpp`,
/// `secret_json.hpp` - and `nlohmann::json j = value;` / `j.get<T>()` work through argument-dependent lookup. This
/// header is what they share: the error type and the rule that no nlohmann exception escapes an adapter.
///
/// \relations
/// - `snicholls::to_json` / `snicholls::from_json` overloads in the sibling adapters.
/// - Group: \ref core_json.
///
/// \contracts
/// - **Errors are translated at the boundary:** an adapter's `from_json` throws `snicholls::json_error`, never a
///   `nlohmann::json::exception`. Tests prove it for every adapter and every kind of JSON value.
/// - **No implicit conversions:** the vendored nlohmann/json is built with `JSON_ImplicitConversions=OFF`; convert
///   with `j.get<T>()`. The adapters do not depend on the setting.
///
/// \errors
/// `snicholls::json_error` (a `std::runtime_error`), with a message naming the type and quoting at most 64
/// characters of the offending text (never for `secret`, whose text is never quoted).
///
/// \examples
/// \snippet nlohmann/json_demo.cpp error
///
/// \perf
/// Not benchmarked: each hook is the core text function plus one `std::string` assignment.
///
/// \testing
/// - Tests: tests/nlohmann/json_test.cpp.
/// - Demo: demos/nlohmann/json_demo.cpp.
///
/// \deps
/// CMake target `snicholls::core_json` (= `snicholls::core` + `nlohmann_json::nlohmann_json`, the vendored
/// `third_party/nlohmann_json` at v3.12.0 unless the consumer already defines that target). Enabled by
/// `CORE_ADAPTERS_WITH_NLOHMANN` (ON by default).
///
/// \gotchas
/// - `snicholls::core_json` is not installed: consume it from the source tree (`add_subdirectory`).
///
/// \status
/// Experimental, pre-1.0.

/// \defgroup core_json snicholls-core JSON adapters
/// \brief Opt-in nlohmann/json hooks for core's value types (target `snicholls::core_json`).

#ifndef snicholls_core_adapters_nlohmann_json_hpp
#define snicholls_core_adapters_nlohmann_json_hpp

#include <stdexcept>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace snicholls {

/// \brief A JSON value that does not convert to or from a core type.
/// \ingroup core_json
///
/// Thrown by every adapter under `adapters/nlohmann/` in place of nlohmann's own exceptions, so a caller catches one
/// type and no third-party exception crosses into code that only knows core.
struct json_error : std::runtime_error
{
    /// \brief Constructs it.
    /// \param what the message.
    explicit json_error(const std::string& what) : std::runtime_error(what) {}
};

namespace detail {

// "<type> from_json: <what> "<input>"" with the input cut at 64 characters, built out of line from the hot path.
[[noreturn]] inline void throw_json_error(std::string_view type, std::string_view what, std::string_view input)
{
    constexpr std::size_t max_quoted = 64;
    std::string message(type);
    message += " from_json: ";
    message += what;
    message += " \"";
    message += input.substr(0, max_quoted);
    if (input.size() > max_quoted) message += "...";
    message += '"';
    throw json_error(message);
}

// The string a text-form adapter reads, or a json_error naming the JSON type it got instead. Never throws nlohmann.
inline const std::string& json_string_or_throw(const nlohmann::json& j, std::string_view type, std::string_view expected)
{
    const auto* s = j.get_ptr<const nlohmann::json::string_t*>();
    if (s == nullptr) [[unlikely]] {
        std::string what("expected ");
        what += expected;
        what += ", got";
        throw_json_error(type, what, j.type_name());
    }
    return *s;
}

} // namespace detail

} // namespace snicholls

#endif /* snicholls_core_adapters_nlohmann_json_hpp */
