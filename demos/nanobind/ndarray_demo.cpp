//
//  ndarray_demo.cpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//
//  Demo module for <snicholls/core_adapters/nanobind/ndarray.hpp>, driven by ndarray_demo.py. The region "template" is
//  the template the header documents: copy it to expose any header-only function over NumPy arrays.
//

#include <snicholls/core/game/harsanyi.hpp>
#include <snicholls/core_adapters/nanobind/errors.hpp>
#include <snicholls/core_adapters/nanobind/gil.hpp>
#include <snicholls/core_adapters/nanobind/ndarray.hpp>

#include <nanobind/nanobind.h>

#include <bit>
#include <span>
#include <vector>

namespace nb = nanobind;
using namespace nb::literals;

NB_MODULE(ndarray_demo_module, m)
{
    snicholls::python::register_exception_translators();

    //! [template]
    m.def(
        "harsanyi_dividends",
        [](snicholls::python::vector_in<double> table) {                       // 1. in: read-only, contiguous, CPU
            snicholls::python::require(std::has_single_bit(table.size()),      //    check what core only asserts
                                       "harsanyi_dividends: the table has 2^n values");
            const std::span<const double> in = snicholls::python::as_span(table);   // 2. view: no copy
            std::vector<double> out = snicholls::python::without_gil([&] {     // 3. compute: GIL released,
                std::vector<double> d(in.begin(), in.end());                   //    plain data only
                snicholls::game::to_dividends(std::span<double>(d));
                return d;
            });
            return snicholls::python::to_numpy(std::move(out));                // 4. out: the vector's buffer
        },
        "table"_a,
        "The Harsanyi dividends of a table of v(S). C++: snicholls::game::to_dividends.");
    //! [template]

    m.def("outer", [](snicholls::python::vector_in<double> a, snicholls::python::vector_in<double> b) {
        const auto x = snicholls::python::as_span(a);
        const auto y = snicholls::python::as_span(b);
        std::vector<double> out(x.size() * y.size());
        snicholls::python::without_gil([&] {
            for (std::size_t i = 0; i < x.size(); ++i)
                for (std::size_t j = 0; j < y.size(); ++j)
                    out[i * y.size() + j] = x[i] * y[j];
        });
        return snicholls::python::to_numpy(std::move(out), x.size(), y.size());
    }, "a"_a, "b"_a, "The outer product, as a matrix.");

    m.def("scale", [](snicholls::python::vector_inout<double> a, double k) {
        for (double& x : snicholls::python::as_span(a))
            x *= k;
    }, "a"_a.noconvert(), "k"_a, "Scales a float64 array in place.");
}
