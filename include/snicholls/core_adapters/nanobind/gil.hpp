//
//  gil.hpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//

/// \file
/// \brief The GIL: release it around C++ work, take it back to call Python, and a Python callable safe from any thread.
/// \ingroup core_nanobind
///
/// \overview
/// A binding that runs C++ for more than a few microseconds should release the global interpreter lock, so other
/// Python threads run meanwhile. Three ways, from the most declarative:
///
/// - `snicholls::python::release_gil` - nanobind's call guard, as a name: `.def("f", &f, release_gil())` releases the
///   GIL for the whole call, after the arguments are converted and before the result is converted back. Use it when
///   every argument converts to a C++ value (numbers, `std::span` of a NumPy array, strings); never when the function
///   touches a Python object.
/// - `snicholls::python::without_gil(f)` - inside a binding body: convert what needs the GIL first, then run `f` with
///   the GIL released and return its result.
/// - `snicholls::python::with_gil(f)` - the reverse, from C++ code running without the GIL (a worker thread, or
///   inside `without_gil`), to call into Python.
///
/// `snicholls::python::python_function<R(A...)>` wraps a Python callable as a C++ callable that takes the GIL for
/// each call (and for its own copy and destruction), so it can be handed to C++ that may run on any thread. Each call
/// then pays for the lock: for a cooperative game evaluated 2^20 times, tabulate in Python once and pass the table
/// instead (see `snicholls_core.game`).
///
/// \snippet nanobind/gil_demo.cpp gil
///
/// \relations
/// - nanobind's `gil_scoped_release` / `gil_scoped_acquire` / `call_guard`, which these name.
/// - `errors.hpp`: a C++ exception thrown with the GIL released is translated after nanobind takes it back.
/// - Group: \ref core_nanobind.
///
/// \contracts
/// - `without_gil(f)` and `with_gil(f)` are exception-neutral: an exception from `f` propagates after the GIL state
///   is restored.
/// - `python_function` holds a strong reference; copying and destroying take the GIL, so it may be copied and
///   destroyed on any thread. Calling it from a thread that already holds the GIL is fine (re-entrant).
/// - Not for free-threaded Python builds (no GIL to release); the package is built for the GIL-ful stable ABI.
///
/// \errors
/// A Python exception raised by the callable becomes `nanobind::python_error`, which nanobind turns back into the
/// same Python exception when it reaches the binding. A result that does not convert to `R` is `nanobind::type_error`
/// (Python `TypeError`). `R` must be default-constructible.
///
/// \perf
/// Releasing and re-taking the GIL costs about 0.1 to 0.3 us; a `python_function` call adds the Python call itself.
/// Measured as part of the call overhead in `BENCHMARKS.md` (tabular Shapley).
///
/// \testing
/// - Tests: tests/nanobind/gil_test.cpp (the test module), tests/nanobind/gil_test.py.
/// - Demo: demos/nanobind/gil_demo.cpp, demos/nanobind/gil_demo.py; `scripts/demos/gil_demo.sh`.
///
/// \deps
/// CMake target `snicholls::core_nanobind`, inside a module built with `nanobind_add_module` (nanobind v3).
///
/// \gotchas
/// - Never touch a `nanobind::object` inside `without_gil` - not even to copy or destroy it. Convert first.
/// - A `python_function` called on a C++ thread can throw (`nanobind::python_error`); no exception may leave a thread,
///   so catch it there, carry it back as a `std::exception_ptr`, and rethrow it on the calling thread after the join,
///   with the GIL held again (tests/nanobind/gil_test.cpp, `call_from_threads`).
/// - `release_gil` with an argument that is a Python object (`nanobind::object`, `nanobind::callable`) is a crash
///   waiting to happen: the object is destroyed after the call, with the GIL held again, but used during it without.
///
/// \status
/// Experimental, pre-1.0.

/// \defgroup core_nanobind snicholls::python - nanobind helpers
/// \brief Helpers for snicholls Python extensions built with nanobind (target `snicholls::core_nanobind`).

#ifndef snicholls_core_adapters_nanobind_gil_hpp
#define snicholls_core_adapters_nanobind_gil_hpp

#include <nanobind/nanobind.h>

#include <functional>
#include <type_traits>
#include <utility>

static_assert(NB_VERSION_MAJOR >= 3, "snicholls::python: the nanobind helpers need nanobind 3 or newer");

namespace snicholls::python {

/// \brief nanobind's call guard that releases the GIL for the duration of a bound call.
/// \ingroup core_nanobind
///
/// `m.def("f", &f, snicholls::python::release_gil());` - only for functions whose arguments and result are C++
/// values; see the file's gotchas.
using release_gil = ::nanobind::call_guard<::nanobind::gil_scoped_release>;

/// \brief Runs `f()` with the GIL released and returns its result.
/// \ingroup core_nanobind
/// \tparam F a callable taking no arguments; it must not touch Python objects.
/// \param f the work.
/// \return what `f()` returns.
/// \throws whatever `f` throws, after the GIL is taken back.
/// \pre the calling thread holds the GIL.
template <class F>
decltype(auto) without_gil(F&& f)
{
    ::nanobind::gil_scoped_release released;
    return std::invoke(std::forward<F>(f));
}

/// \brief Runs `f()` holding the GIL (taking it if this thread does not hold it) and returns its result.
/// \ingroup core_nanobind
/// \tparam F a callable taking no arguments.
/// \param f the work, which may use Python.
/// \return what `f()` returns.
/// \throws whatever `f` throws, after the GIL state is restored.
template <class F>
decltype(auto) with_gil(F&& f)
{
    ::nanobind::gil_scoped_acquire acquired;
    return std::invoke(std::forward<F>(f));
}

/// \brief A Python callable as a C++ callable of a fixed signature, safe to copy, call and destroy on any thread.
/// \ingroup core_nanobind
/// \tparam Signature `R(A...)`.
template <class Signature>
class python_function;

/// \brief `python_function` for `R(A...)`: takes the GIL, calls, converts the result to `R`.
/// \ingroup core_nanobind
/// \tparam R the result type (`void` allowed), converted with `nanobind::cast<R>`.
/// \tparam A the argument types, converted with `nanobind::cast`.
template <class R, class... A>
class python_function<R(A...)>
{
public:
    /// \brief Wraps `f`. \param f the callable. \pre the GIL is held.
    explicit python_function(::nanobind::object f) noexcept : f_(std::move(f)) {}

    /// \brief Copies, taking the GIL. \param o the other.
    python_function(const python_function& o) : f_(with_gil([&] { return o.f_; })) {}

    /// \brief Moves (no reference count changes, no GIL needed). \param o the other.
    python_function(python_function&& o) noexcept = default;

    /// \brief Copy-assigns, taking the GIL. \param o the other. \return `*this`.
    python_function& operator=(const python_function& o)
    {
        with_gil([&] { f_ = o.f_; });
        return *this;
    }

    /// \brief Move-assigns, taking the GIL to drop the old reference. \param o the other. \return `*this`.
    python_function& operator=(python_function&& o) noexcept
    {
        with_gil([&] { f_ = std::move(o.f_); });
        return *this;
    }

    /// \brief Drops the reference, taking the GIL.
    ~python_function()
    {
        if (f_.is_valid())
            with_gil([&] { f_.reset(); });
    }

    /// \brief Calls the Python callable with the GIL held.
    /// \param a the arguments.
    /// \return the result converted to `R`.
    /// \throws nanobind::python_error if Python raised; nanobind::type_error if the result does not convert.
    R operator()(A... a) const
    {
        return with_gil([&]() -> R {
            ::nanobind::object out = f_(std::forward<A>(a)...);
            if constexpr (!std::is_void_v<R>) {
                std::remove_cvref_t<R> value{};
                if (!::nanobind::try_cast(out, value))
                    throw ::nanobind::type_error("snicholls::python::python_function: the callable's result does not "
                                                 "convert to the declared result type");
                return value;
            }
        });
    }

    /// \brief The callable. \return a new reference (take the GIL first).
    [[nodiscard]] const ::nanobind::object& object() const noexcept { return f_; }

private:
    ::nanobind::object f_;
};

} // namespace snicholls::python

#endif /* snicholls_core_adapters_nanobind_gil_hpp */
