// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <type_traits>

#include <OpenImageIO/span.h>


OIIO_NAMESPACE_BEGIN

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



/// Try to copy `n` items of type `T` from `src[srcoffset...]` to
/// `dst[dstoffset...]`. Don't read or write outside the respective span
/// boundaries. Return the number of items actually copied, which should be
/// `n` if the operation was fully successful, but may be less if the request
/// could not be satisfied while staying within the span bounds.
///
/// This is intended to be used as a memory-safe replacement for memcpy if
/// you're using spans.
template<typename T>
size_t
spancpy(span<T> dst, size_t dstoffset, cspan<T> src, size_t srcoffset, size_t n)
{
    // Where do the requests end (limited by span boundaries)?
    size_t dstend = std::min(dstoffset + n, std::size(dst));
    size_t srcend = std::min(srcoffset + n, std::size(src));
    // How many can/should we copy?
    size_t ndst = dstend - dstoffset;
    size_t nsrc = srcend - srcoffset;
    n           = std::min(ndst, nsrc);
    memcpy(dst.data() + dstoffset, src.data() + srcoffset, n * sizeof(T));
    return n;
}



/// Try to write `n` copies of `val` into `dst[offset...]`. Don't write
/// outside the span boundaries. Return the number of items actually written,
/// which should be `n` if the operation was fully successful, but may be less
/// if the request could not be satisfied while staying within the span
/// bounds.
///
/// This is intended to be used as a memory-safe replacement for memset if
/// you're using spans.
template<typename T>
size_t
spanset(span<T> dst, size_t offset, const T& val, size_t n)
{
    // Where does the request end (limited by span boundary)?
    size_t dstend = std::min(offset + n, std::size(dst));
    // How many can/should we copy?
    n = dstend - offset;
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
/// This is intended to be used as a memory-safe replacement for
/// `memset(ptr,0,n)` if you're using spans.
template<typename T>
size_t
spanzero(span<T> dst, size_t offset, size_t n)
{
    // Where does the request end (limited by span boundary)?
    size_t dstend = std::min(offset + n, std::size(dst));
    // How many can/should we copy?
    n = dstend - offset;
    memset(dst.data() + offset, 0, n * sizeof(T));
    return n;
}


OIIO_NAMESPACE_END
