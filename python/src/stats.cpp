//
//  stats.cpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//
//  The snicholls_core.stats module: snicholls::stats::wyrand and the unbiased draws of
//  <snicholls/core/stats/random.hpp>, bit for bit. Python names and their C++ counterparts: docs/python_api.md.
//

#include <snicholls/core/stats/random.hpp>
#include <snicholls/core_adapters/nanobind/errors.hpp>
#include <snicholls/core_adapters/nanobind/gil.hpp>
#include <snicholls/core_adapters/nanobind/ndarray.hpp>

#include <nanobind/nanobind.h>

#include <cstddef>
#include <cstdint>
#include <numeric>
#include <span>
#include <vector>

namespace nb = nanobind;
using namespace nb::literals;
namespace stats = snicholls::stats;
namespace py = snicholls::python;

namespace {

template <class T>
void bind_shuffle(nb::module_& m)
{
    m.def(
        "shuffle",
        [](py::vector_inout<T> values, stats::wyrand& g) {
            const std::span<T> v = py::as_span(values);
            py::without_gil([&] { stats::shuffle(v, g); });
        },
        "values"_a.noconvert(), "g"_a,
        "Shuffles a writable, contiguous NumPy array in place (Fisher-Yates with unbiased uniform_index draws), with "
        "the GIL released. The same generator state gives the same order as C++ everywhere.\n\n"
        "C++: snicholls::stats::shuffle(std::span<T>, G&).");
}

} // namespace

NB_MODULE(stats, m)
{
    py::register_exception_translators();
    m.doc() = "snicholls-core's random generator and unbiased draws (C++ snicholls::stats, stats/random.hpp).\n\n"
              "Every draw is specified to the bit, so a seed reproduces the same values in C++ and Python on every "
              "platform - unlike numpy.random or std::uniform_int_distribution. A Wyrand is not thread-safe: give "
              "each thread its own (Wyrand.stream).";

    nb::class_<stats::wyrand>(m, "Wyrand",
                              "wyhash's generator: 64 random bits per call from one add and one wide multiply.\n\n"
                              "C++: snicholls::stats::wyrand. Every seed, zero included, is good. Not cryptographic.")
        .def(nb::init<std::uint64_t>(), "seed"_a = 0, "A generator from a seed. C++: wyrand(seed).")
        .def_static("stream", &stats::wyrand::stream, "seed"_a, "k"_a,
                    "The k-th of a family of generators from one seed, for parallel work. "
                    "C++: wyrand::stream(seed, k).")
        .def("__call__", [](stats::wyrand& g) { return g(); }, "The next 64 bits. C++: wyrand::operator()().")
        .def("__eq__", [](const stats::wyrand& a, const stats::wyrand& b) { return a == b; }, "other"_a.noconvert(),
             "Whether two generators are in the same state (and so draw the same values). C++: operator==.")
        .def("__copy__", [](const stats::wyrand& g) { return g; }, "A copy in the same state.")
        .def("__deepcopy__", [](const stats::wyrand& g, nb::handle) { return g; }, "memo"_a, "A copy in the same state.")
        .def("__repr__", [](const stats::wyrand&) { return "snicholls_core.stats.Wyrand(...)"; });

    m.def("random_u64", [](stats::wyrand& g) { return stats::random_u64(g); }, "g"_a,
          "64 random bits. C++: snicholls::stats::random_u64(g).");
    m.def(
        "random_u64",
        [](stats::wyrand& g, std::size_t count) {
            std::vector<std::uint64_t> out(count);
            py::without_gil([&] {
                for (auto& x : out)
                    x = stats::random_u64(g);
            });
            return py::to_numpy(std::move(out));
        },
        "g"_a, "count"_a, "count draws of random_u64, as a uint64 array, with the GIL released.");

    m.def(
        "uniform_index",
        [](stats::wyrand& g, std::uint64_t n) {
            py::require(n > 0, "snicholls_core.stats.uniform_index: n must be positive");
            return stats::uniform_index(g, n);
        },
        "g"_a, "n"_a,
        "An unbiased index in [0, n) (Lemire's nearly divisionless method). C++: snicholls::stats::uniform_index(g, n).\n\n"
        "Raises PreconditionError (a ValueError) when n is 0.");
    m.def(
        "uniform_index",
        [](stats::wyrand& g, std::uint64_t n, std::size_t count) {
            py::require(n > 0, "snicholls_core.stats.uniform_index: n must be positive");
            std::vector<std::uint64_t> out(count);
            py::without_gil([&] {
                for (auto& x : out)
                    x = stats::uniform_index(g, n);
            });
            return py::to_numpy(std::move(out));
        },
        "g"_a, "n"_a, "count"_a, "count draws of uniform_index(g, n), as a uint64 array, with the GIL released.");

    m.def("uniform_real", [](stats::wyrand& g) { return stats::uniform_real(g); }, "g"_a,
          "A double in [0, 1): the top 53 bits of a draw times 2^-53. C++: snicholls::stats::uniform_real(g).");
    m.def(
        "uniform_real",
        [](stats::wyrand& g, std::size_t count) {
            std::vector<double> out(count);
            py::without_gil([&] {
                for (auto& x : out)
                    x = stats::uniform_real(g);
            });
            return py::to_numpy(std::move(out));
        },
        "g"_a, "count"_a, "count draws of uniform_real(g), as a float64 array, with the GIL released.");

    bind_shuffle<std::int64_t>(m);
    bind_shuffle<std::uint64_t>(m);
    bind_shuffle<double>(m);

    m.def(
        "permutation",
        [](std::size_t n, stats::wyrand& g) {
            std::vector<std::int64_t> out(n);
            py::without_gil([&] {
                std::iota(out.begin(), out.end(), std::int64_t{0});
                stats::shuffle(std::span<std::int64_t>(out), g);
            });
            return py::to_numpy(std::move(out));
        },
        "n"_a, "g"_a,
        "A random permutation of 0..n-1 as an int64 array.\n\n"
        "C++: std::iota, then snicholls::stats::shuffle - the order a C++ caller gets from the same generator.");
}
