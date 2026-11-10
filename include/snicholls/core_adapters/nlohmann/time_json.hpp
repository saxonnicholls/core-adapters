//
//  time_json.hpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//

/// \file
/// \brief nlohmann/json hooks for `snicholls::utc<D>`: a canonical RFC 3339 string.
/// \ingroup core_json
///
/// \overview
/// `nlohmann::json j = t;` writes `"2024-04-30T14:23:05.123456789Z"`: `snicholls::to_text(t)`, with exactly the
/// resolution's fraction digits (9 for `utc<>`, 3 for `utc_ms`, none for `utc_s`). `j.get<snicholls::utc<D>>()`
/// reads it back with `snicholls::parse_utc<D>` and default (strict) options: an offset is required and applied,
/// digits finer than D are an error, a leap second is an error. A JSON number is not accepted, since its unit would
/// be a guess; parse Unix numbers explicitly with `snicholls::parse_unix<Unit, D>`.
///
/// \snippet nlohmann/time_json_demo.cpp time
///
/// \relations
/// - `snicholls::to_text`, `snicholls::parse_utc` (`time/rfc3339.hpp`); `snicholls::json_error` (`json.hpp`).
/// - Group: \ref core_json.
///
/// \contracts
/// - Round trip: `j.get<utc<D>>() == t` for every `t` in years 0000..9999, and the text is identical on every
///   platform (see "Portability" in `time/utc.hpp`).
/// - `to_json` throws only for a year outside 0000..9999 (possible for `utc_us` and coarser), and `std::bad_alloc`.
///
/// \errors
/// `snicholls::json_error`: not a string; not RFC 3339; out of range or too precise for D; a year outside 0000..9999
/// on output.
///
/// \perf
/// Not benchmarked: `to_text` / `parse_utc` (see `time/rfc3339.hpp`) plus one `std::string`.
///
/// \testing
/// - Tests: tests/nlohmann/time_json_test.cpp.
/// - Demo: demos/nlohmann/time_json_demo.cpp.
///
/// \deps
/// CMake target `snicholls::core_json`.
///
/// \gotchas
/// - None known.
///
/// \status
/// Experimental, pre-1.0.

#ifndef snicholls_core_adapters_nlohmann_time_json_hpp
#define snicholls_core_adapters_nlohmann_time_json_hpp

#include <string>
#include <system_error>

#include <snicholls/core_adapters/nlohmann/json.hpp>
#include <snicholls/core/time/rfc3339.hpp>
#include <snicholls/core/time/utc.hpp>

namespace snicholls {

/// \brief Writes `t` as its canonical RFC 3339 string.
/// \ingroup core_json
/// \tparam D the resolution.
/// \param j receives the string.
/// \param t the instant.
/// \throws snicholls::json_error if the year is outside 0000..9999; `std::bad_alloc`.
template <utc_duration D>
void to_json(nlohmann::json& j, const utc<D>& t)
{
    const rfc3339_text text = to_text(t);
    if (!text) [[unlikely]]
        throw json_error("snicholls::utc to_json: a year outside 0000..9999 has no RFC 3339 form");
    j = std::string(text.view());
}

/// \brief Reads an RFC 3339 string (strict: `snicholls::parse_utc<D>` with default options).
/// \ingroup core_json
/// \tparam D the resolution.
/// \param j the JSON value.
/// \param t receives the instant; unchanged on failure.
/// \throws snicholls::json_error if `j` is not a string, not RFC 3339, or out of range or too precise for D.
template <utc_duration D>
void from_json(const nlohmann::json& j, utc<D>& t)
{
    const std::string& s = detail::json_string_or_throw(j, "snicholls::utc", "an RFC 3339 string");
    const parse_result<utc<D>> r = parse_utc<D>(s);
    if (!r) [[unlikely]]
        detail::throw_json_error("snicholls::utc",
                                 r.error() == std::errc::result_out_of_range ? "out of range or too precise:"
                                                                             : "not RFC 3339:",
                                 s);
    t = *r;
}

} // namespace snicholls

#endif /* snicholls_core_adapters_nlohmann_time_json_hpp */
