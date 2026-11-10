//
//  errors.hpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//

/// \file
/// \brief C++ errors to Python exceptions: core's `result` / `traced` errors to the `snicholls_core.errors` hierarchy.
/// \ingroup core_nanobind
///
/// \overview
/// One mapping for every snicholls extension module, so a Python caller catches the same classes whichever module
/// raised. Call `snicholls::python::register_exception_translators()` once in each module's `NB_MODULE` body. Then:
///
/// | C++ | Python |
/// |---|---|
/// | `snicholls::python::traced_exception` (from a `snicholls::traced<E>` error) | `snicholls_core.errors.TracedError` |
/// | `snicholls::bad_result_access<E>` (any E: `unwrap()` / `expect()` of an error) | `snicholls_core.errors.ResultError` |
/// | `snicholls::python::precondition_error` (`require(...)`) | `snicholls_core.errors.PreconditionError` (also a `ValueError`) |
/// | `std::bad_alloc` | `MemoryError` |
/// | `std::invalid_argument`, `std::domain_error`, `std::length_error` | `ValueError` |
/// | `std::out_of_range` | `IndexError` |
/// | `std::overflow_error` | `OverflowError` |
/// | any other `std::exception` | `RuntimeError` |
///
/// The last five rows are nanobind's own defaults, kept and tested. The Python classes live in pure Python
/// (`snicholls_core/errors.py`) and are looked up by import, so every module shares them; a module used without the
/// `snicholls_core` package installed falls back to the builtin in the second column's base (`ValueError`,
/// `RuntimeError`).
///
/// For values: `raise_error(e)` throws the right C++ exception for any core error type, and `unwrap(r)` returns the
/// value of a `snicholls::result<V, E>` or raises; `optional.hpp` makes a returned `result` do this by itself.
///
/// \snippet nanobind/errors_demo.cpp errors
///
/// \relations
/// - `snicholls::result`, `snicholls::bad_result_access`, `snicholls::traced`, `snicholls::error_report` (core).
/// - `optional.hpp` (the `result` type caster), `gil.hpp`.
/// - Group: \ref core_nanobind.
///
/// \contracts
/// - Registration is idempotent in effect: registering again adds a translator that gives the same answer.
/// - `traced_exception` carries the error report, with fields already masked by core; a secret never reaches Python.
/// - `set_python_error` never throws (it reports a failure to build the exception as that Python error instead).
///
/// \errors
/// The point of the header. `require(false, ...)` throws `precondition_error`; `raise_error(e)` throws as described.
///
/// \perf
/// Not on a hot path: only failing calls pay. The import of `snicholls_core.errors` is done once per class.
///
/// \testing
/// - Tests: tests/nanobind/errors_test.cpp (the test module), tests/nanobind/errors_test.py.
/// - Demo: demos/nanobind/errors_demo.cpp, demos/nanobind/errors_demo.py; `scripts/demos/errors_demo.sh`.
///
/// \deps
/// CMake target `snicholls::core_nanobind`; core's `result.hpp` and `traced.hpp`.
///
/// \gotchas
/// - Build every module with `SNICHOLLS_RESULT_THROW=1` (core's default with exceptions); with 0, `unwrap()` of an
///   error terminates the interpreter instead of raising.
/// - The classes are looked up at the first error, so `snicholls_core` must be importable then, not at module import.
///
/// \status
/// Experimental, pre-1.0.

#ifndef snicholls_core_adapters_nanobind_errors_hpp
#define snicholls_core_adapters_nanobind_errors_hpp

#include <nanobind/nanobind.h>

#include <snicholls/core/result.hpp>
#include <snicholls/core/traced.hpp>

#include <exception>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace snicholls::python {

/// \brief A violated precondition of a bound function: `snicholls_core.errors.PreconditionError` (a `ValueError`).
/// \ingroup core_nanobind
///
/// Core asserts its preconditions; a binding checks them first with `require`, because Python callers pass anything.
class precondition_error : public std::invalid_argument
{
public:
    /// \brief Constructs it. \param what the message.
    explicit precondition_error(const std::string& what) : std::invalid_argument(what) {}
};

/// \brief A `snicholls::traced<E>` error on its way to Python: `snicholls_core.errors.TracedError`.
/// \ingroup core_nanobind
class traced_exception : public std::runtime_error
{
public:
    /// \brief Constructs it from a report. \param report the error report (fields already masked).
    explicit traced_exception(error_report report)
        : std::runtime_error(report.message), report_(std::move(report))
    {
    }

    /// \brief The report. \return the report.
    [[nodiscard]] const error_report& report() const noexcept { return report_; }

private:
    error_report report_;
};

namespace detail {

[[noreturn]] inline void throw_precondition(const char* message) { throw precondition_error(message); }

// A class of snicholls_core.errors, imported once and kept for the life of the process (a leaked reference: a static
// nanobind::object would be destroyed after the interpreter). Falls back to `fallback` when the package is missing.
inline PyObject* error_class(const char* name, PyObject* fallback) noexcept
{
    PyObject* module = PyImport_ImportModule("snicholls_core.errors");
    if (module == nullptr) {
        PyErr_Clear();
        return fallback;
    }
    PyObject* cls = PyObject_GetAttrString(module, name);
    Py_DECREF(module);
    if (cls == nullptr) {
        PyErr_Clear();
        return fallback;
    }
    return cls;   // leaked on purpose
}

inline PyObject* precondition_class() noexcept
{
    static PyObject* cls = error_class("PreconditionError", PyExc_ValueError);
    return cls;
}

inline PyObject* result_class() noexcept
{
    static PyObject* cls = error_class("ResultError", PyExc_RuntimeError);
    return cls;
}

inline PyObject* traced_class() noexcept
{
    static PyObject* cls = error_class("TracedError", PyExc_RuntimeError);
    return cls;
}

inline bool set_attr(PyObject* target, const char* name, PyObject* value) noexcept
{
    if (value == nullptr)
        return false;
    const int status = PyObject_SetAttrString(target, name, value);
    Py_DECREF(value);
    return status == 0;
}

inline PyObject* text(const std::string& s) noexcept
{
    return PyUnicode_DecodeUTF8(s.data(), static_cast<Py_ssize_t>(s.size()), "replace");
}

// TracedError(message) with error, file, line, function, frames, fields and stack set from the report.
inline void set_traced(const error_report& r) noexcept
{
    PyObject* cls = traced_class();
    PyObject* message = text(r.message);
    if (message == nullptr)
        return;
    PyObject* instance = PyObject_CallFunctionObjArgs(cls, message, nullptr);
    Py_DECREF(message);
    if (instance == nullptr)
        return;   // the Python error from the call stands
    bool ok = set_attr(instance, "error", text(r.error)) && set_attr(instance, "file", text(r.where.file_name())) &&
              set_attr(instance, "line", PyLong_FromLong(static_cast<long>(r.where.line()))) &&
              set_attr(instance, "function", text(r.where.function_name())) &&
              set_attr(instance, "stack", text(r.stack));
    if (ok) {
        PyObject* frames = PyList_New(0);
        for (std::size_t i = 0; ok && frames != nullptr && i < r.frames.size(); ++i) {
            PyObject* f = text(r.frames[i].message);
            ok = f != nullptr && PyList_Append(frames, f) == 0;
            Py_XDECREF(f);
        }
        ok = ok && set_attr(instance, "frames", frames);
    }
    if (ok) {
        PyObject* fields = PyDict_New();
        for (std::size_t i = 0; ok && fields != nullptr && i < r.fields.size(); ++i) {
            PyObject* v = text(r.fields[i].value);
            ok = v != nullptr && PyDict_SetItemString(fields, r.fields[i].key.c_str(), v) == 0;
            Py_XDECREF(v);
        }
        ok = ok && set_attr(instance, "fields", fields);
    }
    if (!ok) {
        Py_DECREF(instance);
        return;   // the Python error from building it stands
    }
    PyErr_SetObject(cls, instance);
    Py_DECREF(instance);
}

// The core-specific part of the mapping; rethrows anything else.
inline void translate_core(const std::exception_ptr& p, void*)
{
    try {
        std::rethrow_exception(p);
    } catch (const traced_exception& e) {
        set_traced(e.report());
    } catch (const precondition_error& e) {
        PyErr_SetString(precondition_class(), e.what());
    } catch (const bad_result_access<void>& e) {
        PyErr_SetString(result_class(), e.what());
    }
}

} // namespace detail

/// \brief Registers the translators for core's errors with nanobind. Call once in each `NB_MODULE` body.
/// \ingroup core_nanobind
///
/// The standard exceptions keep nanobind's mapping (see the file's table).
inline void register_exception_translators() { ::nanobind::register_exception_translator(&detail::translate_core); }

/// \brief Throws `precondition_error(message)` unless `condition` holds.
/// \ingroup core_nanobind
/// \param condition the precondition.
/// \param message what was violated, for the Python message.
/// \throws precondition_error if `!condition`.
inline void require(bool condition, const char* message)
{
    if (!condition) [[unlikely]]
        detail::throw_precondition(message);
}

/// \brief Throws `e` as the C++ exception that maps to the right Python class.
/// \ingroup core_nanobind
///
/// A `traced<X>` becomes `traced_exception` (Python `TracedError`, with the location, frames and masked fields); an
/// error that is itself a `std::exception` is thrown as it is (so `std::out_of_range` is an `IndexError`); anything
/// else becomes `bad_result_access<void>` with `snicholls::error_text(e)` (Python `ResultError`).
/// \tparam E the error type.
/// \param e the error.
/// \throws as described; `std::bad_alloc`.
template <class E>
[[noreturn]] void raise_error(const E& e)
{
    using T = std::remove_cvref_t<E>;
    if constexpr (is_traced_v<T>)
        throw traced_exception(make_error_report(e));
    else if constexpr (std::is_base_of_v<std::exception, T> && std::is_copy_constructible_v<T>)
        throw e;
    else
        throw bad_result_access<void>(error_text(e));
}

/// \brief The value of a result, or `raise_error` of its error.
/// \ingroup core_nanobind
/// \tparam V the value type (`void` allowed).
/// \tparam E the error type.
/// \param r the result.
/// \return the value, moved out.
/// \throws as `raise_error` when `r` holds an error.
template <class V, class E>
decltype(auto) unwrap(result<V, E>&& r)
{
    if (r.is_err()) [[unlikely]]
        raise_error(r.unwrap_err_unchecked());
    if constexpr (std::is_void_v<V>)
        return;
    else
        return V(std::move(r).unwrap_unchecked());
}

/// \brief Sets the Python error for a C++ exception, by the file's table. For type casters, which return a null
/// handle instead of throwing.
/// \ingroup core_nanobind
/// \param p the exception.
/// \pre the GIL is held.
inline void set_python_error(const std::exception_ptr& p) noexcept
{
    try {
        try {
            detail::translate_core(p, nullptr);
        } catch (::nanobind::python_error& e) {
            e.restore();   // already a Python exception
        } catch (const std::bad_alloc&) {
            PyErr_NoMemory();
        } catch (const std::out_of_range& e) {
            PyErr_SetString(PyExc_IndexError, e.what());
        } catch (const std::overflow_error& e) {
            PyErr_SetString(PyExc_OverflowError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::domain_error& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::length_error& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (...) {
            PyErr_SetString(PyExc_RuntimeError, "unknown C++ exception");
        }
    } catch (...) {
        PyErr_SetString(PyExc_RuntimeError, "snicholls::python: an exception while translating an exception");
    }
}

} // namespace snicholls::python

#endif /* snicholls_core_adapters_nanobind_errors_hpp */
