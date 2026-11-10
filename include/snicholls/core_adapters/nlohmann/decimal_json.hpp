//
//  decimal_json.hpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//

/// \file
/// \brief nlohmann/json hooks for `snicholls::decimal`: its exact canonical text, as a JSON string.
/// \ingroup core_json
///
/// \overview
/// `nlohmann::json j = d;` writes `"72.5"`, `"0.0000001"`, `"-5.25"`: `decimal::text()`, the one canonical
/// spelling. It is a JSON *string*, not a number, because a JSON number is read as a `double` by most consumers
/// (JavaScript among them) and `0.1` has no exact `double`. `j.get<snicholls::decimal>()` reads the canonical text
/// only (`decimal::parse` with `decimal_syntax::canonical`); a JSON number is refused rather than guessed at.
///
/// \snippet nlohmann/decimal_json_demo.cpp decimal
///
/// \relations
/// - `snicholls::decimal::text`, `snicholls::decimal::parse` (`decimal.hpp`); `snicholls::json_error` (`json.hpp`).
/// - Group: \ref core_json.
///
/// \contracts
/// - Round trip for every decimal; the text is the same on every platform (no locale, no `double`).
/// - `to_json` throws only `std::bad_alloc`.
///
/// \errors
/// `snicholls::json_error`: not a string; not canonical decimal text ("72.50", "+1", "1e5" are refused); more than
/// 18 significant digits or an exponent outside [-18, 18].
///
/// \perf
/// Not benchmarked: `decimal::text` / `decimal::parse` plus one `std::string`.
///
/// \testing
/// - Tests: tests/nlohmann/decimal_json_test.cpp.
/// - Demo: demos/nlohmann/decimal_json_demo.cpp.
///
/// \deps
/// CMake target `snicholls::core_json`.
///
/// \gotchas
/// - A venue that sends strikes as JSON numbers needs its own reader: take the number's original text
///   (`nlohmann::json::parse` with a callback, or a SAX handler) and `decimal::parse` it with `decimal_syntax::plain`.
///
/// \status
/// Experimental, pre-1.0.

#ifndef snicholls_core_adapters_nlohmann_decimal_json_hpp
#define snicholls_core_adapters_nlohmann_decimal_json_hpp

#include <optional>
#include <string>

#include <snicholls/core_adapters/nlohmann/json.hpp>
#include <snicholls/core/decimal.hpp>

namespace snicholls {

/// \brief Writes `d` as its canonical text, a JSON string.
/// \ingroup core_json
/// \param j receives the string.
/// \param d the decimal.
/// \throws std::bad_alloc.
inline void to_json(nlohmann::json& j, const decimal& d) { j = std::string(d.text().view()); }

/// \brief Reads canonical decimal text from a JSON string.
/// \ingroup core_json
/// \param j the JSON value.
/// \param d receives the decimal; unchanged on failure.
/// \throws snicholls::json_error if `j` is not a string or not canonical, in-range decimal text.
inline void from_json(const nlohmann::json& j, decimal& d)
{
    const std::string& s = detail::json_string_or_throw(j, "snicholls::decimal", "a decimal string");
    const std::optional<decimal> r = decimal::parse(s);
    if (!r) [[unlikely]]
        detail::throw_json_error("snicholls::decimal", "not canonical decimal text in range:", s);
    d = *r;
}

} // namespace snicholls

#endif /* snicholls_core_adapters_nlohmann_decimal_json_hpp */
