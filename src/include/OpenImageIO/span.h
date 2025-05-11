// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// clang-format off

#pragma once

#include <array>
#include <cstddef>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/oiioversion.h>
#include <OpenImageIO/platform.h>
#include <OpenImageIO/detail/fmt.h>

// Span notes and helpful links:
// - cppreference on std::span:
//   https://en.cppreference.com/w/cpp/container/span
// - Another implementation, for reference:
//   https://github.com/tcbrindle/span/blob/master/include/tcb/span.hpp


OIIO_NAMESPACE_BEGIN

// Our pre-3.0 implementation had span::size() as a signed value, because we
// wrote it at a time that the draft of std::span said it should be signed.
// The final C++20 std::span ended up with an unsigned size, like all the
// other STL classes. It took us until OIIO 3.0 (or the in-progress 2.6.3)
// before we were able to break compatibility by switching it to match
// std::span::size() returning a size_t.
#ifndef OIIO_SPAN_SIZE_IS_UNSIGNED
#    define OIIO_SPAN_SIZE_IS_UNSIGNED
#endif

using span_size_t = size_t;
using oiio_span_size_type = OIIO::span_size_t;  // back-compat alias

inline constexpr span_size_t dynamic_extent
    = std::numeric_limits<span_size_t>::max();



/// `span<T>` is a non-owning, non-copying, non-allocating reference to a
/// contiguous array of T objects of known length. A 'span` encapsulates both
/// a pointer and a length, and thus is a safer way of passing pointers around
/// (because the function called knows how long the array is). A function
/// that might ordinarily take a `T*` and a length could instead just take a
/// `span<T>`.
///
/// A `span<T>` is mutable (the values in the array may be modified).  A
/// non-mutable (i.e., read-only) reference would be `span<const T>`. Thus,
/// a function that might ordinarily take a `const T*` and a length could
/// instead take a `span<const T>`.
///
/// For convenience, we also define `cspan<T>` as equivalent to
/// `span<const T>`.
///
/// A `span` may be initialized explicitly from a pointer and length, by
/// initializing with a `std::vector<T>`, or by initializing with a constant
/// (treated as an array of length 1). For all of these cases, no extra
/// allocations are performed, and no extra copies of the array contents are
/// made.
///
/// Important caveat: The `span` merely refers to items owned by another
/// array, so the `span` should not be used beyond the lifetime of the
/// array it refers to. Thus, `span` is great for parameter passing, but
/// it's not a good idea to use a `span` to store values in a data
/// structure (unless you are really sure you know what you're doing).
///

template <typename T, span_size_t Extent = dynamic_extent>
class span {
    static_assert (std::is_array<T>::value == false, "can't have span of an array");
public:
    using element_type = T;
    using value_type = typename std::remove_cv<T>::type;
    using size_type = span_size_t;
    using difference_type = ptrdiff_t;
    using pointer = element_type*;
    using reference = element_type&;
    using iterator = element_type*;
    using const_iterator = const element_type*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    static constexpr size_type extent = Extent;

    /// Default constructor -- the span will be `{nullptr,0}`.
    constexpr span () noexcept = default;

    /// Copy constructor (copies the span pointer and length, NOT the data).
    constexpr span (const span &copy) noexcept = default;

#ifndef OIIO_DOXYGEN  /* this declaration confuses doxygen */
    /// Copy constructor from a different extent (copies the span pointer and
    /// length, NOT the data). This allows for construction of `span<const T>`
    /// from `span<T>`, and for converting fixed extent to dynamic extent.
    /// It does not allow for converting to an incompatible data type.
    template<class U, span_size_t N,
             OIIO_ENABLE_IF(std::is_same_v<std::remove_cv_t<T>, std::remove_cv_t<U>>
                             && (extent == dynamic_extent || extent == N))>
    constexpr span (const span<U,N> &copy) noexcept
        : m_data(copy.data()), m_size(copy.size()) { }
#endif

    /// Construct from T* and length.
    constexpr span (pointer data, size_type size) noexcept
        : m_data(data), m_size(size) { }

    /// Construct from begin and end pointers.
    constexpr span (pointer b, pointer e) noexcept
        : m_data(b), m_size(e-b) { }

    /// Construct from a single T&.
    constexpr span (T &data) : m_data(&data), m_size(1) { }

    /// Construct from a fixed-length C array.  Template magic automatically
    /// finds the length from the declared type of the array.
    template<size_t N, OIIO_ENABLE_IF(Extent == dynamic_extent || Extent == N)>
    constexpr span (T (&data)[N]) noexcept : m_data(data), m_size(N) { }

    /// Construct from std::vector<T>.
    template<class Allocator>
    constexpr span (std::vector<T, Allocator> &v)
        : m_data(v.data()), m_size(v.size()) {
    }

    /// Construct from `const std::vector<T>.` This turns
    /// `const std::vector<T>` into a `span<const T>` (the span isn't const,
    /// but the data it points to will be).
    template<class Allocator>
    span (const std::vector<value_type, Allocator> &v) noexcept
        : m_data(v.data()), m_size(v.size()) { }

    /// Construct from mutable element std::array
    template <size_t N>
    constexpr span (std::array<value_type, N> &arr) noexcept
        : m_data(arr.data()), m_size(N) {}

    /// Construct from read-only element std::array
    template <size_t N>
    constexpr span (const std::array<value_type, N>& arr) noexcept
        : m_data(arr.data()), m_size(N) {}

    /// Construct a span from an initializer_list.
    constexpr span (std::initializer_list<T> il) noexcept
        : span (il.begin(), il.size()) { }

    /// Assignment copies the pointer and length, not the data.
    constexpr span& operator= (const span &copy) = default;

    /// Subspan containing the first Count elements of the span.
    template<size_type Count>
    constexpr span<element_type, Count> first () const {
        return { m_data, Count };
    }
    /// Subspan containing the last Count elements of the span.
    template<size_type Count>
    constexpr span<element_type, Count> last () const {
        return { m_data + m_size - Count, Count };
    }

    /// Subspan starting at templated Offset and containing Count elements.
    template<size_type Offset, size_type Count = dynamic_extent>
    constexpr span<element_type, Count> subspan () const {
        return { m_data + Offset, Count != dynamic_extent ? Count : (Extent != dynamic_extent ? Extent - Offset : m_size - Offset) };
    }

    /// Subspan containing just the first `count` elements. The count will be
    /// clamped to be no more than the current size.
    constexpr span<element_type, dynamic_extent> first (size_type count) const {
        return { m_data, std::min(count, m_size) };
    }

    /// Subspan containing just the last `count` elements. The count will be
    /// clamped to be no more than the current size.
    constexpr span<element_type, dynamic_extent> last (size_type count) const {
        count = std::min(count, m_size);
        return { m_data + ( m_size - count ), count };
    }

    /// Subspan starting at offset and containing count elements. The range
    /// requested will be clamped to the current size of the span.
    constexpr span<element_type, dynamic_extent>
    subspan (size_type offset, size_type count = dynamic_extent) const {
        offset = std::min(offset, m_size);
        count = std::min(count, m_size - offset);
        return { m_data + offset, count };
    }

    /// Return the number of elements in the span.
    constexpr size_type size() const noexcept { return m_size; }
    /// Return the size in bytes of the range of the span.
    constexpr size_type size_bytes() const noexcept { return size()*sizeof(T); }
    /// Is the span empty (containing 0 elements)?
    constexpr bool empty() const noexcept { return m_size == 0; }

    /// Return the underlying data pointer to the first element.
    constexpr pointer data() const noexcept { return m_data; }

    /// Element access. For debug build, does bounds check with assertion. For
    /// optimized builds, there is no bounds check.  Note: this is different
    /// from C++ std::span, which never bounds checks `operator[]`.
    constexpr reference operator[] (size_type idx) const {
        OIIO_DASSERT(idx < m_size && "OIIO::span::operator[] range check");
        return m_data[idx];
    }
    constexpr reference operator() (size_type idx) const {
        OIIO_DASSERT(idx < m_size && "OIIO::span::operator() range check");
        return m_data[idx];
    }
    /// Bounds-checked access, throws an assertion if out of range.
    reference at (size_type idx) const {
        if (idx >= size())
            throw (std::out_of_range ("OpenImageIO::span::at"));
        return m_data[idx];
    }

    /// The first element of the span.
    constexpr reference front() const noexcept {
        OIIO_DASSERT(m_size >= 1);
        return m_data[0];
    }
    /// The last element of the span.
    constexpr reference back() const noexcept {
        OIIO_DASSERT(m_size >= 1);
        return m_data[size() - 1];
    }

    /// Iterator pointing to the beginning of the span.
    constexpr iterator begin() const noexcept { return m_data; }
    /// Iterator pointing to the end (one past the last element) of the span.
    constexpr iterator end() const noexcept { return m_data + m_size; }

    /// Const iterator pointing to the beginning of the span.
    constexpr const_iterator cbegin() const noexcept { return m_data; }
    /// Const iterator pointing to the end (one past the last element) of the
    /// span.
    constexpr const_iterator cend() const noexcept { return m_data + m_size; }

    /// Reverse iterator pointing to the last element of the span.
    constexpr reverse_iterator rbegin() const noexcept {
        return reverse_iterator(m_data + m_size - 1);
    }
    /// Reverse iterator pointing to "reverse end" (one element before the
    /// first element) of the span.
    constexpr reverse_iterator rend() const noexcept {
        return reverse_iterator(m_data - 1);
    }

    /// Const reverse iterator pointing to the last element of the span.
    constexpr const_reverse_iterator crbegin() const noexcept {
        return const_reverse_iterator(m_data + m_size - 1);
    }
    /// Const reverse iterator pointing to "reverse end" (one element before
    /// the first element) of the span.
    constexpr const_reverse_iterator crend() const noexcept {
        return const_reverse_iterator(m_data - 1);
    }

private:
    pointer     m_data = nullptr;
    size_type   m_size = 0;
};



/// cspan<T> is a synonym for a non-mutable span<const T>.
template <typename T, span_size_t Extent = dynamic_extent>
using cspan = span<const T, Extent>;



/// Compare all elements of two spans for equality
template <class T, span_size_t X, class U, span_size_t Y>
constexpr bool operator== (span<T,X> l, span<U,Y> r) {
#if OIIO_CPLUSPLUS_VERSION >= 20
    return std::equal (l.begin(), l.end(), r.begin(), r.end());
#else
    auto lsize = l.size();
    bool same = (lsize == r.size());
    for (span_size_t i = 0; same && i < lsize; ++i)
        same &= (l[i] == r[i]);
    return same;
#endif
}

/// Compare all elements of two spans for inequality
template <class T, span_size_t X, class U, span_size_t Y>
constexpr bool operator!= (span<T,X> l, span<U,Y> r) {
    return !(l == r);
}



/// span_strided<T> : a non-owning, mutable reference to a contiguous
/// array with known length and optionally non-default strides through the
/// data.  A span_strided<T> is mutable (the values in the array may
/// be modified), whereas a span_strided<const T> is not mutable.
template <typename T, span_size_t Extent = dynamic_extent>
class span_strided {
    static_assert (std::is_array<T>::value == false,
                   "can't have span_strided of an array");
public:
    using element_type = T;
    using value_type = typename std::remove_cv<T>::type;
    using size_type  = span_size_t;
    using difference_type = ptrdiff_t;
    using stride_type = ptrdiff_t;
    using pointer = element_type*;
    using reference = element_type&;
    using iterator = element_type*;
    using const_iterator = const element_type*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    static constexpr size_type extent = Extent;

    /// Default ctr -- points to nothing
    constexpr span_strided () noexcept {}

    /// Copy constructor
    constexpr span_strided (const span_strided &copy)
        : m_data(copy.data()), m_size(copy.size()), m_stride(copy.stride()) {}

    /// Construct from T* and size, and optionally stride.
    constexpr span_strided (pointer data, size_type size, stride_type stride=1)
        : m_data(data), m_size(size), m_stride(stride) { }

    /// Construct from a single T&.
    constexpr span_strided (T &data) : span_strided(&data,1,1) { }

    /// Construct from a fixed-length C array.  Template magic automatically
    /// finds the length from the declared type of the array.
    template<size_t N>
    constexpr span_strided (T (&data)[N]) : span_strided(data,N,1) {}

    /// Construct from std::vector<T>.
    template<class Allocator>
    constexpr span_strided (std::vector<T, Allocator> &v)
        : span_strided(v.data(), v.size(), 1) {}

    /// Construct from const std::vector<T>. This turns const std::vector<T>
    /// into a span_strided<const T> (the span_strided isn't
    /// const, but the data it points to will be).
    template<class Allocator>
    constexpr span_strided (const std::vector<value_type, Allocator> &v)
        : span_strided(v.data(), v.size(), 1) {}

    /// Construct a span from an initializer_list.
    constexpr span_strided (std::initializer_list<T> il)
        : span_strided (il.begin(), il.size()) { }

    /// Initialize from a span (stride will be 1).
    constexpr span_strided (span<T> av)
        : span_strided(av.data(), av.size(), 1) { }

    // assignments
    span_strided& operator= (const span_strided &copy) {
        m_data   = copy.data();
        m_size   = copy.size();
        m_stride = copy.stride();
        return *this;
    }

    constexpr size_type size() const noexcept { return m_size; }
    constexpr stride_type stride() const noexcept { return m_stride; }

    constexpr reference operator[] (size_type idx) const {
        return m_data[m_stride*idx];
    }
    constexpr reference operator() (size_type idx) const {
        return m_data[m_stride*idx];
    }
    reference at (size_type idx) const {
        if (idx >= size())
            throw (std::out_of_range ("OpenImageIO::span_strided::at"));
        return m_data[m_stride*idx];
    }
    constexpr reference front() const noexcept { return m_data[0]; }
    constexpr reference back() const noexcept { return (*this)[size()-1]; }
    constexpr pointer data() const noexcept { return m_data; }

private:
    pointer       m_data   = nullptr;
    size_type     m_size   = 0;
    stride_type   m_stride = 1;
};



/// cspan_strided<T> is a synonym for a non-mutable span_strided<const T>.
template <typename T, span_size_t Extent = dynamic_extent>
using cspan_strided = span_strided<const T, Extent>;



/// Compare all elements of two spans for equality
template <class T, span_size_t X, class U, span_size_t Y>
constexpr bool operator== (span_strided<T,X> l, span_strided<U,Y> r) {
    auto lsize = l.size();
    if (lsize != r.size())
        return false;
    for (span_size_t i = 0; i < lsize; ++i)
        if (l[i] != r[i])
            return false;
    return true;
}

/// Compare all elements of two spans for inequality
template <class T, span_size_t X, class U, span_size_t Y>
constexpr bool operator!= (span_strided<T,X> l, span_strided<U,Y> r) {
    return !(l == r);
}


// clang-format on


// Explanation about make_span/make_cspan:
//
// If you want to write a function that takes a span of a known type, you can
// do so and call it with any of the kinds of containers that you could
// construct a span from. For example:
//
//     void myfunc(span<float> x) { ... }
//
//     std::vector<float> v;
//     myfunc(v);   // OK
//     float arr[10];
//     myfunc(arr); // OK
//
// But if you want to write a templated function that takes a span of the
// templated type, you can't do that. For example:
//
//     template<typename T>
//     void myfunc(span<T> x) { ... }
//
//     std::vector<float> v;
//     myfunc(v);   // ERROR
//     float arr[10];
//     myfunc(arr); // ERROR
//
// The problem is that span<T> is not span<float>, so the compiler can't
// deduce the template type. You can't even explicitly cast it:
//
//     myfunc(span<float>(v));   // ERROR
//     myfunc(span<float>(arr)); // ERROR
//
// The solution is to write `make_span()` and `make_cspan()` function
// templates that will deduce the template type and return the correct kind of
// span. So we do so for the particular cases of std::vector<T> and T[N]
// arrays. This is not a complete solution, but it's enough for our purposes.

// Helpers: make spans out of common containers
template<typename T, class Allocator>
inline constexpr span<T>
make_span(std::vector<T, Allocator>& arg)  // span from vector
{
    return { arg };
}

template<typename T, class Allocator>
inline constexpr cspan<T>
make_cspan(const std::vector<T, Allocator>& arg)  // cspan from vector
{
    return { arg };
}


template<typename T, size_t N>
inline constexpr span<T>
make_span(T (&arg)[N])  // span from C array of known length
{
    return { arg };
}

template<typename T>
inline constexpr span<T>
make_span(T* data, span_size_t size)  // span from ptr + size
{
    return { data, size };
}

template<typename T, size_t N>
inline constexpr cspan<T>
make_cspan(T (&arg)[N])  // cspan from C array of known length
{
    return { arg };
}

template<typename T>
inline constexpr cspan<T>
make_cspan(const T& arg)  // cspan from a single value
{
    return { &arg, 1 };
}

template<typename T>
inline constexpr cspan<T>
make_cspan(const T* data, span_size_t size)  // cspan from ptr + size
{
    return { data, size };
}



/// Convert a span of any type to a span of a differing type covering the same
/// memory. If the sizes are not identical, it will truncate length if
/// necessary to not spill past the bounds of the input span. Use with
/// caution!
template<class T, class S = std::byte, span_size_t Extent>
span<T>
span_cast(const span<S, Extent>& s) noexcept
{
    return make_span(reinterpret_cast<T*>(s.data()),
                     s.size_bytes() / sizeof(T));
}



/// Convert a span of any type to a span of bytes covering the same range of
/// memory.
template<typename T, span_size_t Extent>
span<const std::byte,
     ((Extent == dynamic_extent) ? dynamic_extent : sizeof(T) * Extent)>
as_bytes(span<T, Extent> s) noexcept
{
    return { reinterpret_cast<const std::byte*>(s.data()), s.size_bytes() };
}



/// Convert a span of any type to a span of mutable bytes covering the same
/// range of memory.
template<class T, span_size_t Extent, OIIO_ENABLE_IF(!std::is_const<T>::value)>
span<std::byte,
     ((Extent == dynamic_extent) ? dynamic_extent : sizeof(T) * Extent)>
as_writable_bytes(span<T, Extent> s) noexcept
{
    return { reinterpret_cast<std::byte*>(s.data()), s.size_bytes() };
}



/// Convert a raw `const T*` ptr + length to a span of const bytes covering
/// the same range of memory. For non-void pointers, the length is in the
/// number of elements of T; for void pointers, the length is measured in
/// bytes.
template<typename T>
span<const std::byte>
as_bytes(const T* ptr, size_t len) noexcept
{
    size_t nbytes = len;
    if constexpr (!std::is_void_v<T>)
        nbytes *= sizeof(T);
    return make_cspan(reinterpret_cast<const std::byte*>(ptr), nbytes);
}



/// Convert a raw `T*` ptr + length to a span of mutable bytes covering the
/// same range of memory. For non-void pointers, the length is in the number
/// of elements of T; for void pointers, the length is measured in bytes.
template<class T, OIIO_ENABLE_IF(!std::is_const_v<T>)>
span<std::byte>
as_writable_bytes(T* ptr, size_t len) noexcept
{
    size_t nbytes = len;
    if constexpr (!std::is_void_v<T>)
        nbytes *= sizeof(T);
    return make_span(reinterpret_cast<std::byte*>(ptr), nbytes);
}



/// Convert a reference to a single variable to a const byte span of that
/// object's memory.
template<typename T>
span<const std::byte>
as_bytes_ref(const T& ref) noexcept
{
    return make_cspan(reinterpret_cast<const std::byte*>(&ref), sizeof(T));
}



/// Copy the memory contents of `src` to `dst`. They must have the same
/// total size.
template<typename T, typename S>
inline void
spancpy(span<T> dst, span<S> src)
{
    OIIO_DASSERT(dst.size_bytes() == src.size_bytes());
    memcpy(dst.data(), src.data(), src.size_bytes());
}



/// Try to copy `n` items of type `T` from `src[srcoffset...]` to
/// `dst[dstoffset...]`. Don't read or write outside the respective span
/// boundaries. Return the number of items actually copied, which should be
/// `n` if the operation was fully successful, but may be less if the request
/// could not be satisfied while staying within the span bounds.
///
/// If `n` is not supplied, it will default to filling as much of `src` (from
/// `srcoffset` to its end) as will fit into `dst`. If `srcoffset` is not
/// supplied, it will default to 0 (the beginning of `src`).
///
/// This is intended to be used as a memory-safe replacement for memcpy if
/// you're using spans.
template<typename T>
size_t
spancpy(span<T> dst, size_t dstoffset, cspan<T> src, size_t srcoffset = 0,
        size_t n = size_t(-1))
{
    // Where do the requests end (limited by span boundaries)?
    n = std::min(n, src.size() - srcoffset);
    n = std::min(n, dst.size() - dstoffset);
    memcpy(dst.data() + dstoffset, src.data() + srcoffset, n * sizeof(T));
    return n;
}



/// Perform a safe `memcpy(dst, src, n*sizeof(T))` but ensuring that the
/// memory accesses stay within the boundaries of spans `dst_span` and
/// `src_span`.
///
/// This is intended to be used as a memory-safe replacement for memcpy if
/// you know the spans representing safe areas.
template<typename T>
inline size_t
span_memcpy(T* dst, const T* src, size_t n, span<T> dst_span, cspan<T> src_span)
{
    return spancpy(dst_span, dst - dst_span.begin(), src_span,
                   src - src_span.begin(), n);
}



/// Try to write `n` copies of `val` into `dst[offset...]`. Don't write
/// outside the span boundaries. Return the number of items actually written,
/// which should be `n` if the operation was fully successful, but may be less
/// if the request could not be satisfied while staying within the span
/// bounds.
///
/// If `n` is not supplied, it will default to filling from `offset` to the
/// end of the span.
///
/// This is intended to be used as a memory-safe replacement for memset if
/// you're using spans.
template<typename T>
size_t
spanset(span<T> dst, size_t offset, const T& val, size_t n = size_t(-1))
{
    // Where does the request end (limited by span boundary)?
    n = std::min(n, dst.size() - offset);
    for (size_t i = 0; i < n; ++i)
        dst[offset + i] = val;
    return n;
}



/// Try to fill `n` elements of `dst[offset...]` with 0-valued bytes. Don't
/// write outside the span boundaries. Return the number of items actually
/// written, which should be `n` if the operation was fully successful, but
/// may be less if the request could not be satisfied while staying within the
/// span bounds.
///
/// If `n` is not supplied, it will default to filling from `offset` to the
/// end of the span. If `offset` is not supplied, it will default 0 (the
/// beginning of the span).
///
/// This is intended to be used as a memory-safe replacement for
/// `memset(ptr,0,n)` if you're using spans.
template<typename T>
size_t
spanzero(span<T> dst, size_t offset = 0, size_t n = size_t(-1))
{
    // Where does the request end (limited by span boundary)?
    n = std::min(n, dst.size() - offset);
    memset(dst.data() + offset, 0, n * sizeof(T));
    return n;
}



/// Does the byte span `query` lie entirely within the safe `bounds` span?
inline bool
span_within(cspan<std::byte> bounds, cspan<std::byte> query)
{
    return query.data() >= bounds.data()
           && query.data() + query.size() <= bounds.data() + bounds.size();
}



/// Verify the `ptr[0..len-1]` lies entirely within the given span `s`, which
/// does not need to be the same data type.  Return true if that is the case,
/// false if it extends beyond the safe limits fo the span.
template<typename SpanType, typename PtrType>
inline bool
check_span(span<SpanType> s, const PtrType* ptr, size_t len = 1)
{
    return span_within(as_bytes(s), as_bytes(make_cspan(ptr, len)));
}



/// OIIO_ALLOCASPAN is used to allocate smallish amount of memory on the
/// stack, equivalent of C99 type var_name[size], and then return a span
/// encompassing it.
#define OIIO_ALLOCA_SPAN(type, size) span<type>(OIIO_ALLOCA(type, size), size)


OIIO_NAMESPACE_END



// Declare std::size and std::ssize for our span.
namespace std {

#if OIIO_CPLUSPLUS_VERSION < 20
// C++20 and beyond already have these declared.
template<class T, OIIO::span_size_t E = OIIO::dynamic_extent>
constexpr ptrdiff_t
ssize(const OIIO::span<T, E>& c)
{
    return static_cast<ptrdiff_t>(c.size());
}

template<class T, OIIO::span_size_t E = OIIO::dynamic_extent>
constexpr ptrdiff_t
ssize(const OIIO::span_strided<T, E>& c)
{
    return static_cast<ptrdiff_t>(c.size());
}
#endif

// Allow client software to easily know if the std::size/ssize was added for
// our span templates.
#define OIIO_SPAN_HAS_STD_SIZE 1

}  // namespace std

// clang-format on



/// Custom fmtlib formatters for span/cspan types.
namespace fmt {
template<typename T, OIIO::span_size_t Extent>
struct formatter<OIIO::span<T, Extent>>
    : OIIO::pvt::index_formatter<OIIO::span<T, Extent>> {};
}  // namespace fmt
