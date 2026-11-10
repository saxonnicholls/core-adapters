//
//  secret_json_demo.cpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//
//  Demo for <snicholls/core_adapters/nlohmann/secret_json.hpp>: a secret is read from JSON, never written to it.
//

#include <snicholls/core_adapters/nlohmann/secret_json.hpp>

#include <cstdio>
#include <string>
#include <type_traits>

int main()
{
    bool ok = true;
    //! [secret]
    const auto config = nlohmann::json::parse(R"({"api_key": "demo-key-123", "port": 5432})");
    const auto key = config.at("api_key").get<snicholls::secret<std::string>>();
    // nlohmann::json out = key;        // does not compile: there is no to_json for a secret
    static_assert(!std::is_constructible_v<nlohmann::json, snicholls::secret<std::string>>);
    std::printf("the key has %zu characters\n", key.reveal().size());   // reveal(): the one way to the value
    //! [secret]
    ok = ok && key.reveal() == "demo-key-123";
    try {
        static_cast<void>(config.at("api_key").get<snicholls::secret<int>>());
        ok = false;
    } catch (const snicholls::json_error& e) {
        const std::string what = e.what();
        std::printf("refused, without the value: %s\n", what.c_str());
        ok = ok && what.find("demo-key") == std::string::npos;
    }
    std::printf("secret_json_demo: %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
