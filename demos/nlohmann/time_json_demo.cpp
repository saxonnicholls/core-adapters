//
//  time_json_demo.cpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//
//  Demo for <snicholls/core_adapters/nlohmann/time_json.hpp>: utc<D> as canonical RFC 3339, read strictly.
//

#include <snicholls/core_adapters/nlohmann/time_json.hpp>

#include <cstdio>
#include <string>

int main()
{
    bool ok = true;
    //! [time]
    const auto t = snicholls::utc_ms::from_unix_millis(1714486985123);
    const nlohmann::json j = {{"as_of", t}};                       // {"as_of":"2024-04-30T14:23:05.123Z"}
    std::printf("%s\n", j.dump().c_str());
    const auto back = j.at("as_of").get<snicholls::utc_ms>();     // strict RFC 3339, offset applied
    const auto ny = nlohmann::json("2024-04-30T10:23:05.123-04:00").get<snicholls::utc_ms>();
    //! [time]
    ok = ok && j.dump() == R"({"as_of":"2024-04-30T14:23:05.123Z"})" && back == t && ny == t;
    try {
        static_cast<void>(nlohmann::json(1714486985).get<snicholls::utc_ms>());   // a number's unit would be a guess
        ok = false;
    } catch (const snicholls::json_error& e) {
        std::printf("refused: %s\n", e.what());
    }
    try {
        static_cast<void>(nlohmann::json("2024-04-30T14:23:05.1234Z").get<snicholls::utc_ms>());   // too precise
        ok = false;
    } catch (const snicholls::json_error& e) {
        std::printf("refused: %s\n", e.what());
    }
    try {
        const nlohmann::json far = snicholls::utc_s::from_unix_seconds(253402300800);   // year 10000
        static_cast<void>(far);
        ok = false;
    } catch (const snicholls::json_error& e) {
        std::printf("refused: %s\n", e.what());
    }
    std::printf("time_json_demo: %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
