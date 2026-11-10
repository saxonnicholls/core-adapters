//
//  gil_test.cpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//
//  The test module for <snicholls/core_adapters/nanobind/gil.hpp>, driven by gil_test.py.
//

#include <snicholls/core_adapters/nanobind/errors.hpp>
#include <snicholls/core_adapters/nanobind/gil.hpp>

#include <nanobind/nanobind.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <thread>
#include <vector>

namespace nb = nanobind;
namespace py = snicholls::python;
using namespace nb::literals;

namespace {

// Busy C++ work for `seconds`, touching no Python object
std::uint64_t spin(double seconds)
{
    const auto end = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);
    std::uint64_t n = 0;
    while (std::chrono::steady_clock::now() < end)
        ++n;
    return n;
}

} // namespace

NB_MODULE(gil_module, m)
{
    py::register_exception_translators();

    m.def("spin_released", &spin, "seconds"_a, py::release_gil());
    m.def("spin_held", &spin, "seconds"_a);
    m.def("spin_without_gil", [](double seconds) { return py::without_gil([&] { return spin(seconds); }); }, "seconds"_a);
    // Python called from inside without_gil, through with_gil: would crash if with_gil did not take the GIL
    m.def("call_python_inside_without_gil", [](nb::callable f) {
        return py::without_gil([&] { return py::with_gil([&] { return nb::cast<int>(f()); }); });
    }, "f"_a);
    m.def("throw_without_gil", [] {
        py::without_gil([] { throw std::out_of_range("thrown with the GIL released"); });
    });

    // python_function: called from several C++ threads at once, each taking the GIL per call
    m.def("call_from_threads", [](nb::callable f, int threads, int calls) {
        const py::python_function<long(long)> fn(f);
        std::atomic<long> total{0};
        std::vector<std::exception_ptr> errors(static_cast<std::size_t>(threads));
        py::without_gil([&] {
            std::vector<std::thread> pool;
            for (int t = 0; t < threads; ++t)
                pool.emplace_back([&, t] {
                    try {   // no exception may leave a thread: carry it back
                        py::python_function<long(long)> mine = fn;   // copied and destroyed off the main thread
                        for (int i = 0; i < calls; ++i)
                            total += mine(t);
                    } catch (...) {
                        errors[static_cast<std::size_t>(t)] = std::current_exception();
                    }
                });
            for (auto& th : pool)
                th.join();
        });
        for (const auto& e : errors)
            if (e)
                std::rethrow_exception(e);   // with the GIL held again
        return total.load();
    }, "f"_a, "threads"_a, "calls"_a);
    m.def("call_void", [](nb::callable f) {
        py::python_function<void()> fn(f);
        py::without_gil([&] { fn(); });
    }, "f"_a);
    m.def("call_and_convert", [](nb::callable f) {
        py::python_function<double()> fn(f);
        return py::without_gil([&] { return fn(); });
    }, "f"_a);
}
