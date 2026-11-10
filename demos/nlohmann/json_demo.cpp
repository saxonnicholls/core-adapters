//
//  json_demo.cpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//
//  Demo for <snicholls/core_adapters/nlohmann/json.hpp>: one error type for every adapter, never nlohmann's.
//

#include <snicholls/core_adapters/nlohmann/json.hpp>
#include <snicholls/core_adapters/nlohmann/uuid_json.hpp>

#include <cstdio>
#include <string>

int main()
{
    bool ok = true;
    //! [error]
    const nlohmann::json j = nlohmann::json::parse(R"({"id": 42})");
    try {
        const auto id = j.at("id").get<snicholls::uuid>();   // a number where a UUID string belongs
        static_cast<void>(id);
    } catch (const snicholls::json_error& e) {                // never a nlohmann::json::exception
        std::printf("refused: %s\n", e.what());
    }
    //! [error]
    try {
        static_cast<void>(nlohmann::json("x").get<snicholls::uuid>());
        ok = false;
    } catch (const snicholls::json_error& e) {
        ok = ok && std::string(e.what()) == "snicholls::uuid from_json: not a canonical 8-4-4-4-12 UUID: \"x\"";
    } catch (...) {
        ok = false;
    }
    std::printf("json_demo: %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
