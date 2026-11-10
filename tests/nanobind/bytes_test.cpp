//
//  bytes_test.cpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//
//  The test module for <snicholls/core_adapters/nanobind/bytes.hpp>, driven by bytes_test.py.
//

#include <snicholls/core_adapters/nanobind/bytes.hpp>
#include <snicholls/core_adapters/nanobind/gil.hpp>

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>

#include <cstddef>
#include <string>

namespace nb = nanobind;
namespace py = snicholls::python;
using namespace nb::literals;

NB_MODULE(bytes_module, m)
{
    m.def("length", [](const py::byte_view& b) { return b.size(); }, "b"_a);
    m.def("checksum", [](py::byte_view b) {
        return py::without_gil([&] {
            unsigned sum = 0;
            for (const std::byte x : b.bytes())
                sum += static_cast<unsigned>(x);
            return sum;
        });
    }, "b"_a);
    m.def("chars", [](const py::byte_view& b) { return std::string(b.chars()); }, "b"_a);
    m.def("upper", [](py::mutable_byte_view b) {
        for (std::byte& x : b.bytes())
            if (x >= std::byte{'a'} && x <= std::byte{'z'})
                x = static_cast<std::byte>(static_cast<unsigned>(x) - 32);
    }, "b"_a);
    m.def("text", [](const py::text_view& t) { return std::string(t.view()); }, "t"_a);
    m.def("text_size", [](const py::text_view& t) { return t.size(); }, "t"_a);
    m.def("reverse", [](const py::byte_view& b) {
        std::string s(b.chars().rbegin(), b.chars().rend());
        return py::to_bytes(std::string_view(s));
    }, "b"_a);
    m.def("copy", [](const py::byte_view& b) { return py::to_bytes(b.bytes()); }, "b"_a);
}
