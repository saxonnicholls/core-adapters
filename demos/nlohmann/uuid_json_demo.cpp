//
//  uuid_json_demo.cpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//
//  Demo for <snicholls/core_adapters/nlohmann/uuid_json.hpp>: the canonical lowercase string, both ways.
//

#include <snicholls/core_adapters/nlohmann/uuid_json.hpp>

#include <cstdio>

int main()
{
    using namespace snicholls::literals;
    bool ok = true;
    //! [uuid]
    const auto id = "0190A5C4-7E1B-7C3A-9F12-3B4C5D6E7F80"_uuid;
    const nlohmann::json j = {{"order", id}};                       // written lowercase
    std::printf("%s\n", j.dump().c_str());
    const auto back = j.at("order").get<snicholls::uuid>();
    //! [uuid]
    ok = ok && j.dump() == R"({"order":"0190a5c4-7e1b-7c3a-9f12-3b4c5d6e7f80"})" && back == id;
    try {
        static_cast<void>(nlohmann::json("{0190a5c4-7e1b-7c3a-9f12-3b4c5d6e7f80}").get<snicholls::uuid>());
        ok = false;
    } catch (const snicholls::json_error& e) {
        std::printf("refused: %s\n", e.what());
    }
    std::printf("uuid_json_demo: %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
