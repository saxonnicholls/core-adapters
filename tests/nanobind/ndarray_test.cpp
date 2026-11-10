//
//  ndarray_test.cpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//
//  The test module for <snicholls/core_adapters/nanobind/ndarray.hpp>, driven by ndarray_test.py.
//

#include <snicholls/core/game/harsanyi.hpp>
#include <snicholls/core_adapters/nanobind/errors.hpp>
#include <snicholls/core_adapters/nanobind/gil.hpp>
#include <snicholls/core_adapters/nanobind/ndarray.hpp>

#include <nanobind/nanobind.h>

#include <bit>
#include <cstdint>
#include <numeric>
#include <span>
#include <vector>

namespace nb = nanobind;
namespace py = snicholls::python;
using namespace nb::literals;

NB_MODULE(ndarray_module, m)
{
    py::register_exception_translators();

    m.def("sum", [](py::vector_in<double> a) {
        const std::span<const double> x = py::as_span(a);
        return py::without_gil([&] { return std::accumulate(x.begin(), x.end(), 0.0); });
    }, "a"_a);
    m.def("sum_strict", [](py::vector_in<double> a) { return std::accumulate(a.data(), a.data() + a.size(), 0.0); },
          "a"_a.noconvert());
    m.def("matrix_row_sums", [](py::matrix_in<double> a) {
        const std::span<const double> x = py::as_span(a);
        std::vector<double> out(a.shape(0));
        for (std::size_t i = 0; i < a.shape(0); ++i)
            for (std::size_t j = 0; j < a.shape(1); ++j)
                out[i] += x[i * a.shape(1) + j];
        return py::to_numpy(std::move(out));
    }, "a"_a);
    m.def("scale", [](py::vector_inout<double> a, double k) {
        for (double& x : py::as_span(a))
            x *= k;
    }, "a"_a.noconvert(), "k"_a);
    m.def("iota", [](std::size_t n) {
        std::vector<std::int64_t> v(n);
        std::iota(v.begin(), v.end(), 0);
        return py::to_numpy(std::move(v));
    }, "n"_a);
    m.def("grid", [](std::size_t rows, std::size_t columns) {
        std::vector<double> v(rows * columns);
        std::iota(v.begin(), v.end(), 0.0);
        return py::to_numpy(std::move(v), rows, columns);
    }, "rows"_a, "columns"_a);
    m.def("bad_grid", [] { return py::to_numpy(std::vector<double>(5), 2, 3); });
    // The template of the header's documentation, as a test
    m.def("harsanyi_dividends", [](py::vector_in<double> table) {
        py::require(std::has_single_bit(table.size()), "the table has 2^n values");
        const std::span<const double> in = py::as_span(table);
        std::vector<double> out = py::without_gil([&] {
            std::vector<double> d(in.begin(), in.end());
            snicholls::game::to_dividends(std::span<double>(d));
            return d;
        });
        return py::to_numpy(std::move(out));
    }, "table"_a);
}
