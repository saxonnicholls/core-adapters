//
//  bytes.hpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//

/// \file
/// \brief bytes, bytearray, memoryview and str as zero-copy C++ views, and C++ bytes back as Python `bytes`.
/// \ingroup core_nanobind
///
/// \overview
/// Core reads bytes as `std::span<const std::byte>` and text as `std::string_view`. These argument types give a
/// binding exactly that, with no copy, from whatever a Python caller has:
///
/// | C++ argument | accepts | gives |
/// |---|---|---|
/// | `snicholls::python::byte_view` | `bytes`, `bytearray`, `memoryview`, `array.array`, a C-contiguous NumPy array - anything with a contiguous buffer | `std::span<const std::byte>` |
/// | `snicholls::python::mutable_byte_view` | a writable contiguous buffer (`bytearray`, writable `memoryview`, NumPy) | `std::span<std::byte>` |
/// | `snicholls::python::text_view` | `str` (its UTF-8), and the bytes-like types above (taken as UTF-8 as they are) | `std::string_view` |
///
/// `str` is deliberately not a `byte_view`: text has no bytes until an encoding is chosen, so a caller writes
/// `s.encode()`. Back to Python, `to_bytes(span)` / `to_bytes(string_view)` copy into a new `bytes`; a `std::string`
/// or `std::string_view` result is a `str` as usual (`<nanobind/stl/string.h>`).
///
/// \snippet nanobind/bytes_demo.cpp bytes
///
/// \relations
/// - core's `reflect/string_access.hpp` (`as_bytes_view`) on the C++ side; nanobind's string casters.
/// - Group: \ref core_nanobind.
///
/// \contracts
/// - The views hold the buffer (or the `str`) for their own lifetime, so the memory stays valid and, for a
///   `bytearray`, its size stays fixed while a view exists (Python raises `BufferError` on a resize).
/// - The views may be destroyed with or without the GIL: they take it to release the buffer.
/// - Move-only.
///
/// \errors
/// An argument of another type, or a non-contiguous buffer, fails overload resolution (`TypeError`); a read-only
/// buffer fails for `mutable_byte_view`. `to_bytes` throws `std::bad_alloc` (`MemoryError`).
///
/// \perf
/// O(1) for the views (one buffer request); `to_bytes` copies once.
///
/// \testing
/// - Tests: tests/nanobind/bytes_test.cpp (the test module), tests/nanobind/bytes_test.py.
/// - Demo: demos/nanobind/bytes_demo.cpp, demos/nanobind/bytes_demo.py; `scripts/demos/bytes_demo.sh`.
///
/// \deps
/// CMake target `snicholls::core_nanobind`. The buffer protocol is in the stable ABI from Python 3.11, so these work
/// in abi3 modules.
///
/// \gotchas
/// - Take the views by value or `const&` only in the function signature; never store one beyond the call.
/// - A `text_view` of `bytes` is not checked for valid UTF-8, exactly as `std::string_view` would not be.
///
/// \status
/// Experimental, pre-1.0.

#ifndef snicholls_core_adapters_nanobind_bytes_hpp
#define snicholls_core_adapters_nanobind_bytes_hpp

#include <nanobind/nanobind.h>

#include <cstddef>
#include <span>
#include <string_view>
#include <utility>

namespace snicholls::python {

namespace detail {

// A held buffer (or a str's UTF-8), released with the GIL taken.
class held_buffer
{
public:
    held_buffer() noexcept = default;
    held_buffer(const held_buffer&) = delete;
    held_buffer& operator=(const held_buffer&) = delete;
    held_buffer(held_buffer&& o) noexcept : buffer_(o.buffer_), held_(std::exchange(o.held_, false)),
                                            text_(std::exchange(o.text_, nullptr)), data_(o.data_), size_(o.size_)
    {
    }
    held_buffer& operator=(held_buffer&& o) noexcept
    {
        if (this != &o) {
            release();
            buffer_ = o.buffer_;
            held_ = std::exchange(o.held_, false);
            text_ = std::exchange(o.text_, nullptr);
            data_ = o.data_;
            size_ = o.size_;
        }
        return *this;
    }
    ~held_buffer() { release(); }

    // A contiguous buffer; writable when asked. False, with no Python error left, when src has none.
    bool acquire_buffer(PyObject* src, bool writable) noexcept
    {
        release();
        if (PyUnicode_Check(src))
            return false;
        if (PyObject_GetBuffer(src, &buffer_, writable ? (PyBUF_SIMPLE | PyBUF_WRITABLE) : PyBUF_SIMPLE) != 0) {
            PyErr_Clear();
            return false;
        }
        held_ = true;
        data_ = buffer_.buf;
        size_ = static_cast<std::size_t>(buffer_.len);
        return true;
    }

    // A str's UTF-8 (kept alive by a reference to the str), or a contiguous buffer.
    bool acquire_text(PyObject* src) noexcept
    {
        release();
        if (!PyUnicode_Check(src))
            return acquire_buffer(src, false);
        Py_ssize_t n = 0;
        const char* s = PyUnicode_AsUTF8AndSize(src, &n);
        if (s == nullptr) {
            PyErr_Clear();
            return false;
        }
        Py_INCREF(src);
        text_ = src;
        data_ = const_cast<char*>(s);
        size_ = static_cast<std::size_t>(n);
        return true;
    }

    [[nodiscard]] void* data() const noexcept { return data_; }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }

private:
    void release() noexcept
    {
        if (!held_ && text_ == nullptr)
            return;
        const PyGILState_STATE state = PyGILState_Ensure();
        if (held_)
            PyBuffer_Release(&buffer_);
        Py_XDECREF(text_);
        PyGILState_Release(state);
        held_ = false;
        text_ = nullptr;
        data_ = nullptr;
        size_ = 0;
    }

    Py_buffer buffer_{};
    bool held_ = false;
    PyObject* text_ = nullptr;
    void* data_ = nullptr;
    std::size_t size_ = 0;
};

} // namespace detail

/// \brief A read-only view of a Python bytes-like object's contiguous buffer.
/// \ingroup core_nanobind
class byte_view
{
public:
    /// \brief The bytes. \return a span valid while this view lives.
    [[nodiscard]] std::span<const std::byte> bytes() const noexcept
    {
        return {static_cast<const std::byte*>(held_.data()), held_.size()};
    }
    /// \brief The bytes as characters. \return a string_view valid while this view lives.
    [[nodiscard]] std::string_view chars() const noexcept { return {static_cast<const char*>(held_.data()), held_.size()}; }
    /// \brief The length in bytes. \return the size.
    [[nodiscard]] std::size_t size() const noexcept { return held_.size(); }

    /// \brief Takes `src`'s buffer (the type caster's step). \param src the object. \return whether it has one.
    bool acquire(PyObject* src) noexcept { return held_.acquire_buffer(src, false); }

private:
    detail::held_buffer held_;
};

/// \brief A writable view of a Python object's contiguous buffer.
/// \ingroup core_nanobind
class mutable_byte_view
{
public:
    /// \brief The bytes. \return a span valid while this view lives.
    [[nodiscard]] std::span<std::byte> bytes() const noexcept { return {static_cast<std::byte*>(held_.data()), held_.size()}; }
    /// \brief The length in bytes. \return the size.
    [[nodiscard]] std::size_t size() const noexcept { return held_.size(); }

    /// \brief Takes `src`'s writable buffer (the type caster's step). \param src the object. \return whether it has one.
    bool acquire(PyObject* src) noexcept { return held_.acquire_buffer(src, true); }

private:
    detail::held_buffer held_;
};

/// \brief A view of a `str`'s UTF-8, or of a bytes-like buffer taken as UTF-8.
/// \ingroup core_nanobind
class text_view
{
public:
    /// \brief The text. \return a string_view valid while this view lives.
    [[nodiscard]] std::string_view view() const noexcept { return {static_cast<const char*>(held_.data()), held_.size()}; }
    /// \brief The length in bytes of UTF-8. \return the size.
    [[nodiscard]] std::size_t size() const noexcept { return held_.size(); }

    /// \brief Takes `src`'s UTF-8 or buffer (the type caster's step). \param src the object. \return success.
    bool acquire(PyObject* src) noexcept { return held_.acquire_text(src); }

private:
    detail::held_buffer held_;
};

/// \brief A new Python `bytes` holding a copy of `b`.
/// \ingroup core_nanobind
/// \param b the bytes.
/// \return the object.
/// \throws std::bad_alloc (via nanobind's `python_error` for `MemoryError`).
/// \pre the GIL is held.
[[nodiscard]] inline ::nanobind::bytes to_bytes(std::span<const std::byte> b)
{
    return ::nanobind::bytes(b.data(), b.size());
}

/// \brief A new Python `bytes` holding a copy of `s`.
/// \ingroup core_nanobind
/// \param s the characters.
/// \return the object.
/// \pre the GIL is held.
[[nodiscard]] inline ::nanobind::bytes to_bytes(std::string_view s) { return ::nanobind::bytes(s.data(), s.size()); }

} // namespace snicholls::python

namespace nanobind::detail {

/// \cond
template <class View, auto Descr>
struct snicholls_view_caster
{
    NB_TYPE_CASTER(View, Descr)

    bool from_python(handle src, uint32_t, cleanup_list*) noexcept { return value.acquire(src.ptr()); }

    static handle from_cpp(const View&, rv_policy, cleanup_list*) noexcept
    {
        PyErr_SetString(PyExc_TypeError, "snicholls::python: a byte or text view is an argument type only");
        return {};
    }
};

template <>
struct type_caster<snicholls::python::byte_view>
    : snicholls_view_caster<snicholls::python::byte_view, const_name("collections.abc.Buffer")>
{
};

template <>
struct type_caster<snicholls::python::mutable_byte_view>
    : snicholls_view_caster<snicholls::python::mutable_byte_view, const_name("collections.abc.Buffer")>
{
};

template <>
struct type_caster<snicholls::python::text_view>
    : snicholls_view_caster<snicholls::python::text_view, const_name("str | collections.abc.Buffer")>
{
};
/// \endcond

} // namespace nanobind::detail

#endif /* snicholls_core_adapters_nanobind_bytes_hpp */
