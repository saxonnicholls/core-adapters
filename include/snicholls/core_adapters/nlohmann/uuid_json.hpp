//
//  uuid_json.hpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//

/// \file
/// \brief nlohmann/json hooks for `snicholls::uuid`: the canonical lowercase 8-4-4-4-12 string.
/// \ingroup core_json
///
/// \overview
/// `nlohmann::json j = id;` writes `"f81d4fae-7dec-11d0-a765-00a0c91e6bf6"` (`snicholls::to_text`, lowercase).
/// `j.get<snicholls::uuid>()` reads the canonical form in either case (`snicholls::parse_uuid` with default
/// options); braces, `urn:uuid:` and the 32-digit form are refused, as is a JSON number or byte array.
///
/// \snippet nlohmann/uuid_json_demo.cpp uuid
///
/// \relations
/// - `snicholls::to_text`, `snicholls::parse_uuid` (`uuid.hpp`); `snicholls::json_error` (`json.hpp`).
/// - Group: \ref core_json.
///
/// \contracts
/// - Round trip for every 128-bit value; `to_json` throws only `std::bad_alloc`.
///
/// \errors
/// `snicholls::json_error`: not a string, or not a canonical UUID.
///
/// \perf
/// Not benchmarked: `to_text` / `parse_uuid` plus one `std::string`.
///
/// \testing
/// - Tests: tests/nlohmann/uuid_json_test.cpp.
/// - Demo: demos/nlohmann/uuid_json_demo.cpp.
///
/// \deps
/// CMake target `snicholls::core_json`.
///
/// \gotchas
/// - None known.
///
/// \status
/// Experimental, pre-1.0.

#ifndef snicholls_core_adapters_nlohmann_uuid_json_hpp
#define snicholls_core_adapters_nlohmann_uuid_json_hpp

#include <optional>
#include <string>

#include <snicholls/core_adapters/nlohmann/json.hpp>
#include <snicholls/core/uuid.hpp>

namespace snicholls {

/// \brief Writes `u` as its canonical lowercase string.
/// \ingroup core_json
/// \param j receives the string.
/// \param u the UUID.
/// \throws std::bad_alloc.
inline void to_json(nlohmann::json& j, const uuid& u) { j = std::string(to_text(u).view()); }

/// \brief Reads a canonical 8-4-4-4-12 string (either case).
/// \ingroup core_json
/// \param j the JSON value.
/// \param u receives the UUID; unchanged on failure.
/// \throws snicholls::json_error if `j` is not a string or not a canonical UUID.
inline void from_json(const nlohmann::json& j, uuid& u)
{
    const std::string& s = detail::json_string_or_throw(j, "snicholls::uuid", "a UUID string");
    const std::optional<uuid> r = parse_uuid(s);
    if (!r) [[unlikely]]
        detail::throw_json_error("snicholls::uuid", "not a canonical 8-4-4-4-12 UUID:", s);
    u = *r;
}

} // namespace snicholls

#endif /* snicholls_core_adapters_nlohmann_uuid_json_hpp */
