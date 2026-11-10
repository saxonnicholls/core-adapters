//
//  gil_demo.cpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//
//  Demo module for <snicholls/core_adapters/nanobind/gil.hpp>, driven by gil_demo.py.
//

#include <snicholls/core_adapters/nanobind/gil.hpp>

#include <nanobind/nanobind.h>

#include <cmath>
#include <cstddef>
#include <thread>
#include <vector>

namespace nb = nanobind;
using namespace nb::literals;

namespace {

double integrate(std::size_t steps)   // pure C++: no Python object touched
{
    double sum = 0;
    for (std::size_t i = 0; i < steps; ++i)
        sum += std::sin(static_cast<double>(i) * 1e-6);
    return sum;
}

} // namespace

NB_MODULE(gil_demo_module, m)
{
    //! [gil]
    // 1. The whole call without the GIL: arguments and result are C++ values
    m.def("integrate", &integrate, "steps"_a, snicholls::python::release_gil());

    // 2. Convert first, then release for the C++ part only
    m.def("integrate_twice", [](std::size_t steps) {
        const double once = snicholls::python::without_gil([&] { return integrate(steps); });
        return 2 * once;
    }, "steps"_a);

    // 3. A Python callable handed to C++ threads: each call takes the GIL
    m.def("map_in_threads", [](nb::callable f, int n) {
        const snicholls::python::python_function<int(int)> fn(f);
        std::vector<int> out(static_cast<std::size_t>(n));
        snicholls::python::without_gil([&] {
            std::vector<std::thread> threads;
            for (int i = 0; i < n; ++i)
                threads.emplace_back([&, i] {
                    try {   // no exception may leave a thread (the Python callable may raise)
                        out[static_cast<std::size_t>(i)] = fn(i);
                    } catch (...) {
                        out[static_cast<std::size_t>(i)] = -1;
                    }
                });
            for (auto& t : threads)
                t.join();
        });
        int sum = 0;
        for (const int x : out)
            sum += x;
        return sum;
    }, "f"_a, "n"_a);
    //! [gil]
}
