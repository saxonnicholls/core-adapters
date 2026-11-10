//
//  decimal_json_demo.cpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//
//  Demo for <snicholls/core_adapters/nlohmann/decimal_json.hpp>: exact text as a JSON string; numbers refused.
//

#include <snicholls/core_adapters/nlohmann/decimal_json.hpp>

#include <cstdio>

int main()
{
    bool ok = true;
    //! [decimal]
    const auto strike = snicholls::decimal::parse("72.5").value();
    const nlohmann::json j = {{"strike", strike}};                  // {"strike":"72.5"}: a string, so it stays exact
    std::printf("%s\n", j.dump().c_str());
    const auto back = j.at("strike").get<snicholls::decimal>();
    //! [decimal]
    ok = ok && j.dump() == R"({"strike":"72.5"})" && back == strike;
    for (const nlohmann::json& bad : {nlohmann::json(72.5), nlohmann::json("72.50")}) {
        try {
            static_cast<void>(bad.get<snicholls::decimal>());
            ok = false;
        } catch (const snicholls::json_error& e) {
            std::printf("refused: %s\n", e.what());
        }
    }
    std::printf("decimal_json_demo: %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
