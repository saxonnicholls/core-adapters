//
//  errors_demo.cpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//
//  Demo module for <snicholls/core_adapters/nanobind/errors.hpp>, driven by errors_demo.py.
//

#include <snicholls/core/result.hpp>
#include <snicholls/core/traced.hpp>
#include <snicholls/core_adapters/nanobind/errors.hpp>

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>

#include <string>

namespace nb = nanobind;
namespace py = snicholls::python;
using namespace nb::literals;

namespace {

snicholls::result<double, snicholls::traced<std::string>> price(double spot)
{
    if (spot <= 0)
        return snicholls::fail("spot {} is not positive", spot);
    return snicholls::ok(spot * 1.01);
}

} // namespace

//! [errors]
NB_MODULE(errors_demo_module, m)
{
    snicholls::python::register_exception_translators();   // once per module

    m.def("price", [](double spot) {
        py::require(spot == spot, "spot is NaN");             // PreconditionError (a ValueError)
        return py::unwrap(price(spot));                       // the value, or TracedError
    }, "spot"_a);
    m.def("lookup", [](int i) {
        if (i > 2)
            throw std::out_of_range("no instrument " + std::to_string(i));   // IndexError
        return i * 10;
    }, "i"_a);
}
//! [errors]
