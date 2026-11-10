//
//  ndarray.hpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//

/// \file
/// \brief NumPy arrays in and out of a header-only C++ function, with the GIL released: the names and the template.
/// \ingroup core_nanobind
///
/// \overview
/// Core's numerics take `std::span` of plain data and return plain data, so binding one is always the same four
/// steps. This header names the pieces, and the demo holds the template that maintainers copy:
///
/// 1. **In:** `vector_in<T>` / `matrix_in<T>` - a read-only, C-contiguous, CPU `nanobind::ndarray`. NumPy arrays,
///    and anything else exporting DLPack or the buffer protocol, convert; a non-contiguous array or one of another
///    dtype is copied by nanobind first (pass `.noconvert()` on the argument to refuse instead).
/// 2. **View:** `as_span(a)` - a `std::span` over the array's memory, no copy.
/// 3. **Compute:** `without_gil(...)` (`gil.hpp`) around the core call; check preconditions with `require(...)`
///    (`errors.hpp`) before, because Python passes anything.
/// 4. **Out:** `to_numpy(std::move(vector))` (or with rows and columns) - the vector's buffer handed to a NumPy array,
///    no copy, freed when the array is.
///
/// \snippet nanobind/ndarray_demo.cpp template
///
/// \relations
/// - nanobind's `ndarray`; `gil.hpp` (`without_gil`, `release_gil`); `errors.hpp` (`require`).
/// - Group: \ref core_nanobind.
///
/// \contracts
/// - `as_span` is O(1), `noexcept`, and valid while the ndarray argument lives (the whole call).
/// - `to_numpy` is O(1) after one allocation for the owner; the NumPy array owns the data from then on.
/// - Reading an input array with the GIL released is safe as long as no other Python thread writes to it at the same
///   time - the same rule as NumPy's own released-GIL operations.
///
/// \errors
/// A wrong dtype, rank or device fails overload resolution: `TypeError` naming the expected signature. `to_numpy`
/// throws `std::bad_alloc` (`MemoryError`), and `std::length_error` (`ValueError`) when rows times columns is not the
/// vector's size.
///
/// \perf
/// The fixed cost of a call - argument conversion, the GIL round trip, building the result array - is measured in
/// `BENCHMARKS.md` (tabular Shapley, Python against C++).
///
/// \testing
/// - Tests: tests/nanobind/ndarray_test.cpp (the test module), tests/nanobind/ndarray_test.py.
/// - Demo: demos/nanobind/ndarray_demo.cpp, demos/nanobind/ndarray_demo.py; `scripts/demos/ndarray_demo.sh`.
///
/// \deps
/// CMake target `snicholls::core_nanobind`; `<nanobind/ndarray.h>`. NumPy is needed at run time only by functions
/// that return NumPy arrays.
///
/// \gotchas
/// - Never return a view of an input array as a new array without passing the input as its owner: the input may be
///   freed first.
/// - A `uint64` coalition table or index array must be asked for as such; NumPy's default integer is `int64`.
///
/// \status
/// Experimental, pre-1.0.

#ifndef snicholls_core_adapters_nanobind_ndarray_hpp
#define snicholls_core_adapters_nanobind_ndarray_hpp

#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>

#include <cstddef>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace snicholls::python {

/// \brief A read-only, C-contiguous, one-dimensional CPU array of `T`, as a function argument.
/// \ingroup core_nanobind
template <class T>
using vector_in = ::nanobind::ndarray<const T, ::nanobind::ndim<1>, ::nanobind::c_contig, ::nanobind::device::cpu>;

/// \brief A read-only, C-contiguous (row-major), two-dimensional CPU array of `T`, as a function argument.
/// \ingroup core_nanobind
template <class T>
using matrix_in = ::nanobind::ndarray<const T, ::nanobind::ndim<2>, ::nanobind::c_contig, ::nanobind::device::cpu>;

/// \brief A writable, C-contiguous, one-dimensional CPU array of `T`, for functions that work in place.
/// \ingroup core_nanobind
template <class T>
using vector_inout = ::nanobind::ndarray<T, ::nanobind::ndim<1>, ::nanobind::c_contig, ::nanobind::device::cpu>;

/// \brief A one-dimensional NumPy array of `T`, as a result.
/// \ingroup core_nanobind
template <class T>
using numpy_vector = ::nanobind::ndarray<::nanobind::numpy, T, ::nanobind::ndim<1>>;

/// \brief A two-dimensional, row-major NumPy array of `T`, as a result.
/// \ingroup core_nanobind
template <class T>
using numpy_matrix = ::nanobind::ndarray<::nanobind::numpy, T, ::nanobind::ndim<2>, ::nanobind::c_contig>;

/// \brief A span over a C-contiguous array's elements, all dimensions flattened in row-major order.
/// \ingroup core_nanobind
/// \tparam Args the ndarray's parameters; it must be C-contiguous (`c_contig`) and on the CPU.
/// \param a the array.
/// \return the span: `const` elements for a read-only array.
template <class... Args>
[[nodiscard]] auto as_span(const ::nanobind::ndarray<Args...>& a) noexcept
{
    using array = ::nanobind::ndarray<Args...>;
    static_assert(array::Order == 'C', "snicholls::python::as_span: the array must be C-contiguous (nanobind::c_contig)");
    static_assert(array::DeviceType == ::nanobind::device::cpu::value,
                  "snicholls::python::as_span: the array must be on the CPU (nanobind::device::cpu)");
    using scalar = typename array::Scalar;
    return std::span<scalar>(a.data(), a.size());
}

namespace detail {

template <class T>
void delete_vector(void* p) noexcept
{
    delete static_cast<std::vector<T>*>(p);
}

} // namespace detail

/// \brief A NumPy array of shape (rows, columns) owning `values`' buffer, without copying it.
/// \ingroup core_nanobind
/// \tparam T the element type.
/// \param values the elements in row-major order; moved from.
/// \param rows the rows.
/// \param columns the columns.
/// \return the array.
/// \throws std::length_error if `rows * columns != values.size()`; `std::bad_alloc`.
/// \pre the GIL is held.
template <class T>
[[nodiscard]] numpy_matrix<T> to_numpy(std::vector<T>&& values, std::size_t rows, std::size_t columns)
{
    if (rows * columns != values.size()) [[unlikely]]
        throw std::length_error("snicholls::python::to_numpy: rows * columns is not the number of values");
    auto owned = std::make_unique<std::vector<T>>(std::move(values));
    T* data = owned->data();
    ::nanobind::capsule owner(owned.get(), &detail::delete_vector<T>);
    owned.release();
    return numpy_matrix<T>(data, {rows, columns}, owner);
}

/// \brief A one-dimensional NumPy array owning `values`' buffer, without copying it.
/// \ingroup core_nanobind
/// \tparam T the element type.
/// \param values the elements; moved from.
/// \return the array.
/// \throws std::bad_alloc.
/// \pre the GIL is held.
template <class T>
[[nodiscard]] numpy_vector<T> to_numpy(std::vector<T>&& values)
{
    const std::size_t n = values.size();
    auto owned = std::make_unique<std::vector<T>>(std::move(values));
    T* data = owned->data();
    ::nanobind::capsule owner(owned.get(), &detail::delete_vector<T>);
    owned.release();
    return numpy_vector<T>(data, {n}, owner);
}

} // namespace snicholls::python

#endif /* snicholls_core_adapters_nanobind_ndarray_hpp */
