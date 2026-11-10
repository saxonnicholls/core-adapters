//
//  errors_test.cpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//
//  The test module for <snicholls/core_adapters/nanobind/errors.hpp>, driven by errors_test.py.
//

#include <snicholls/core/result.hpp>
#include <snicholls/core/secret.hpp>
#include <snicholls/core/traced.hpp>
#include <snicholls/core_adapters/nanobind/errors.hpp>

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>

#include <new>
#include <stdexcept>
#include <string>
#include <system_error>

namespace nb = nanobind;
namespace py = snicholls::python;
using namespace nb::literals;

namespace {

enum class code { overdrawn = 3 };

snicholls::result<int, std::string> parse_positive(int x)
{
    if (x > 0)
        return snicholls::ok(x);
    return snicholls::err(std::string("not positive: ") + std::to_string(x));
}

snicholls::result<int, snicholls::traced<std::string>> traced_failure()
{
    auto e = snicholls::traced<std::string>(std::string("ledger is closed"));
    e.add_context("posting a trade");
    e.add_field("account", 42);
    e.add_field("api_key", snicholls::secret<std::string>("sk-never-shown"));
    return snicholls::err(std::move(e));
}

} // namespace

NB_MODULE(errors_module, m)
{
    py::register_exception_translators();

    m.def("require", [](bool ok) { py::require(ok, "the argument must be true"); }, "ok"_a);
    m.def("unwrap_ok", [] { return py::unwrap(parse_positive(5)); });
    m.def("unwrap_err", [] { return py::unwrap(parse_positive(-2)); });
    m.def("unwrap_void_ok", [] { py::unwrap(snicholls::result<void, std::string>(snicholls::ok())); });
    m.def("core_unwrap", [] { return parse_positive(-1).unwrap(); });   // core's own unwrap: bad_result_access<E>
    m.def("traced", [] { return py::unwrap(traced_failure()); });
    m.def("raise_enum", [] { py::raise_error(code::overdrawn); });
    m.def("raise_std_out_of_range", [] { py::raise_error(std::out_of_range("index 9 of 3")); });
    m.def("throw", [](const std::string& kind) {
        if (kind == "bad_alloc") throw std::bad_alloc();
        if (kind == "invalid_argument") throw std::invalid_argument("invalid");
        if (kind == "domain_error") throw std::domain_error("domain");
        if (kind == "length_error") throw std::length_error("length");
        if (kind == "out_of_range") throw std::out_of_range("range");
        if (kind == "overflow_error") throw std::overflow_error("overflow");
        if (kind == "runtime_error") throw std::runtime_error("runtime");
        if (kind == "system_error") throw std::system_error(std::make_error_code(std::errc::timed_out));
        if (kind == "precondition") throw py::precondition_error("precondition");
        throw std::logic_error("unknown kind");
    }, "kind"_a);
    m.def("set_python_error", [](const std::string& kind) {
        // What a type caster does: set the error, return null
        try {
            if (kind == "traced") py::unwrap(traced_failure());
            if (kind == "out_of_range") throw std::out_of_range("range");
            if (kind == "bad_alloc") throw std::bad_alloc();
            if (kind == "result") py::unwrap(parse_positive(0));
            throw std::runtime_error("other");
        } catch (...) {
            py::set_python_error(std::current_exception());
        }
        throw nb::python_error();
    }, "kind"_a);
}
