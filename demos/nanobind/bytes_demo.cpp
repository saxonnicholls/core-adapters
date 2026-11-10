//
//  bytes_demo.cpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//
//  Demo module for <snicholls/core_adapters/nanobind/bytes.hpp>, driven by bytes_demo.py.
//

#include <snicholls/core/hash/fnv.hpp>
#include <snicholls/core_adapters/nanobind/bytes.hpp>

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace nb = nanobind;
using namespace nb::literals;

NB_MODULE(bytes_demo_module, m)
{
    //! [bytes]
    // Any bytes-like object, no copy: a core function over bytes sees a std::span<const std::byte>
    m.def("fnv1a_64", [](const snicholls::python::byte_view& b) { return snicholls::fnv1a_64(b.chars()); }, "data"_a);

    // Text: a str's UTF-8 (or bytes as they are), as a std::string_view
    m.def("shout", [](const snicholls::python::text_view& t) { return std::string(t.view()) + "!"; }, "text"_a);

    // Writable buffers, changed in place
    m.def("zero", [](snicholls::python::mutable_byte_view b) {
        for (std::byte& x : b.bytes())
            x = std::byte{0};
    }, "buffer"_a);

    // Bytes back to Python
    m.def("header", [](std::uint32_t length) {
        const std::byte h[4] = {std::byte(length >> 24), std::byte(length >> 16), std::byte(length >> 8), std::byte(length)};
        return snicholls::python::to_bytes(std::span<const std::byte>(h));
    }, "length"_a);
    //! [bytes]
}
