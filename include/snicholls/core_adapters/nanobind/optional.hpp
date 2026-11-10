//
//  optional.hpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//

/// \file
/// \brief Values that may be missing: `std::optional` both ways, and a returned `snicholls::result` as value-or-raise.
/// \ingroup core_nanobind
///
/// \overview
/// - `std::optional<T>` is `T | None` in both directions (nanobind's caster, included here so one include covers
///   the family's two "maybe" types). Core uses `std::optional` where absence is not an error: a parse that may
///   fail, `uint256` arithmetic, `uuid` text.
/// - `snicholls::result<V, E>` as a **return type** is `V` (`None` for `result<void, E>`): an error result raises,
///   through `snicholls::python::raise_error` (`errors.hpp`) - a `traced` error as `TracedError` with its location,
///   anything else as `ResultError` or as the standard exception it already is. A binding can therefore return core's
///   results directly, with no `unwrap` in sight. A `result` is never accepted as an argument.
///
/// \snippet nanobind/optional_demo.cpp optional
///
/// \relations
/// - `errors.hpp` (`raise_error`, `set_python_error`, `register_exception_translators`); `<nanobind/stl/optional.h>`.
/// - Group: \ref core_nanobind.
///
/// \contracts
/// - The `result` caster converts the value with `V`'s own caster and return-value policy; it never throws (it sets
///   the Python error and returns a null handle, as nanobind requires).
///
/// \errors
/// See `errors.hpp`: the Python class follows the error type.
///
/// \perf
/// No cost beyond `V`'s own conversion on success.
///
/// \testing
/// - Tests: tests/nanobind/optional_test.cpp (the test module), tests/nanobind/optional_test.py.
/// - Demo: demos/nanobind/optional_demo.cpp, demos/nanobind/optional_demo.py; `scripts/demos/optional_demo.sh`.
///
/// \deps
/// CMake target `snicholls::core_nanobind`; core's `result.hpp`.
///
/// \gotchas
/// - The stubs show a `result<V, E>` function as returning `V`; document what it raises in the docstring.
///
/// \status
/// Experimental, pre-1.0.

#ifndef snicholls_core_adapters_nanobind_optional_hpp
#define snicholls_core_adapters_nanobind_optional_hpp

#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>

#include <snicholls/core/result.hpp>
#include <snicholls/core_adapters/nanobind/errors.hpp>

#include <exception>
#include <type_traits>
#include <utility>

namespace nanobind::detail {

/// \cond
template <class V, class E>
struct type_caster<snicholls::result<V, E>>
{
    using Value = snicholls::result<V, E>;
    static constexpr auto Name = [] {
        if constexpr (std::is_void_v<V>)
            return const_name("None");
        else
            return make_caster<V>::Name;
    }();
    template <typename T_>
    using Cast = movable_cast_t<T_>;
    template <typename T_>
    static constexpr bool can_cast()
    {
        return false;
    }

    bool from_python(handle, uint32_t, cleanup_list*) noexcept { return false; }   // never an argument

    template <typename T_>
    static handle from_cpp(T_&& r, rv_policy policy, cleanup_list* cleanup) noexcept
    {
        if (r.is_err()) {
            try {
                snicholls::python::raise_error(r.unwrap_err_unchecked());
            } catch (...) {
                snicholls::python::set_python_error(std::current_exception());
            }
            return {};
        }
        if constexpr (std::is_void_v<V>)
            return none().release();
        else
            return make_caster<V>::from_cpp(std::forward<T_>(r).unwrap_unchecked(), policy, cleanup);
    }
};
/// \endcond

} // namespace nanobind::detail

#endif /* snicholls_core_adapters_nanobind_optional_hpp */
