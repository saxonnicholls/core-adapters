//
//  optional_demo.cpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//
//  Demo module for <snicholls/core_adapters/nanobind/optional.hpp>, driven by optional_demo.py.
//

#include <snicholls/core/decimal.hpp>
#include <snicholls/core/result.hpp>
#include <snicholls/core_adapters/nanobind/errors.hpp>
#include <snicholls/core_adapters/nanobind/optional.hpp>

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>

#include <optional>
#include <string>

namespace nb = nanobind;
using namespace nb::literals;

NB_MODULE(optional_demo_module, m)
{
    snicholls::python::register_exception_translators();

    //! [optional]
    // std::optional: None when absent, both ways
    m.def("canonical_decimal", [](const std::string& text) -> std::optional<std::string> {
        const auto d = snicholls::decimal::parse(text, snicholls::decimal_syntax::plain);
        if (!d)
            return std::nullopt;
        return d->to_string();
    }, "text"_a);

    // snicholls::result: the value, or the error raised - no unwrap in the binding
    m.def("parse_decimal", [](const std::string& text) -> snicholls::result<std::string, std::string> {
        const auto d = snicholls::decimal::parse(text);
        if (!d)
            return snicholls::err("not canonical decimal text: " + text);
        return snicholls::ok(d->to_string());
    }, "text"_a);
    //! [optional]
}
