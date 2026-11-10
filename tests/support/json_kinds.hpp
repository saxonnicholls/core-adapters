//
//  json_kinds.hpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//
//  One JSON value of every kind, for the tests that prove no nlohmann exception escapes an adapter.
//

#ifndef core_adapters_tests_support_json_kinds_hpp
#define core_adapters_tests_support_json_kinds_hpp

#include <nlohmann/json.hpp>

#include <cstdint>
#include <vector>

namespace core_adapters_test {

// null, booleans, integers, unsigned, floats, strings (empty, junk, long), arrays, objects, binary, discarded
inline std::vector<nlohmann::json> every_kind_of_json()
{
    return {
        nlohmann::json(),
        nlohmann::json(true),
        nlohmann::json(false),
        nlohmann::json(0),
        nlohmann::json(-17),
        nlohmann::json(std::uint64_t{18446744073709551615u}),
        nlohmann::json(72.5),
        nlohmann::json(1e300),
        nlohmann::json(""),
        nlohmann::json("not a value of any kind"),
        nlohmann::json(std::string(1000, 'x')),
        nlohmann::json::array(),
        nlohmann::json::array({1, "two", nullptr}),
        nlohmann::json::object(),
        nlohmann::json::object({{"value", "2024-04-30T14:23:05Z"}}),
        nlohmann::json::binary({1, 2, 3}),
        nlohmann::json(nlohmann::json::value_t::discarded),
    };
}

} // namespace core_adapters_test

#endif /* core_adapters_tests_support_json_kinds_hpp */
