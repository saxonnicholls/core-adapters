//
//  optional_test.cpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//
//  The test module for <snicholls/core_adapters/nanobind/optional.hpp>, driven by optional_test.py.
//

#include <snicholls/core/result.hpp>
#include <snicholls/core/traced.hpp>
#include <snicholls/core/uuid.hpp>
#include <snicholls/core_adapters/nanobind/errors.hpp>
#include <snicholls/core_adapters/nanobind/optional.hpp>

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace nb = nanobind;
namespace py = snicholls::python;
using namespace nb::literals;

NB_MODULE(optional_module, m)
{
    py::register_exception_translators();

    // std::optional, both ways
    m.def("half", [](int x) -> std::optional<int> {
        if (x % 2 != 0)
            return std::nullopt;
        return x / 2;
    }, "x"_a);
    m.def("or_default", [](std::optional<std::string> s) { return s.value_or("default"); }, "s"_a = nb::none());
    m.def("uuid_version", [](const std::string& text) -> std::optional<int> {
        const auto u = snicholls::parse_uuid(text);
        if (!u)
            return std::nullopt;
        return static_cast<int>(u->version());
    }, "text"_a);

    // snicholls::result as a return type
    m.def("checked_divide", [](int a, int b) -> snicholls::result<int, std::string> {
        if (b == 0)
            return snicholls::err(std::string("division by zero"));
        return snicholls::ok(a / b);
    }, "a"_a, "b"_a);
    m.def("traced_divide", [](int a, int b) -> snicholls::result<double, snicholls::traced<std::string>> {
        if (b == 0)
            return snicholls::fail("division of {} by zero", a);
        return snicholls::ok(double(a) / b);
    }, "a"_a, "b"_a);
    m.def("check", [](bool ok) -> snicholls::result<void, std::string> {
        if (!ok)
            return snicholls::err(std::string("check failed"));
        return snicholls::ok();
    }, "ok"_a);
    m.def("index", [](int i) -> snicholls::result<std::vector<int>, std::out_of_range> {
        if (i < 0)
            return snicholls::err(std::out_of_range("negative index"));
        return snicholls::ok(std::vector<int>(static_cast<std::size_t>(i), 7));
    }, "i"_a);
}
