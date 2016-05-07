/*
  Copyright 2015 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/


#pragma once

#include <vector>
#include <stdexcept>
#include <iostream>

#if OIIO_CPLUSPLUS_VERSION >= 11
# include <initializer_list>
# include <type_traits>
#else /* FIXME(C++11): this case can go away when C++11 is our minimum */
# include <boost/type_traits.hpp>
#endif

#include "oiioversion.h"
#include "platform.h"
#include "dassert.h"
#include "coordinate.h"

OIIO_NAMESPACE_BEGIN

#if OIIO_CPLUSPLUS_VERSION >= 11
using std::remove_const;
using std::is_array;
#else /* FIXME(C++11): this case can go away when C++11 is our minimum */
using boost::remove_const;
using boost::is_array;
#endif


template <typename T, size_t Rank> class array_view;
template <typename T, size_t Rank> class array_view_strided;




/// array_view<T,Rank> : a non-owning reference to a contiguous array with
/// known length. If Rank > 1, it's multi-dimensional. An array_view<T> is
/// mutable (the values in the array may be modified), whereas an
/// array_view<const T> is not mutable.
///
/// Background: Functions whose input requires a set of contiguous values
/// (an array) are faced with a dilemma. If the caller passes just a
/// pointer, the function has no inherent way to determine how many elements
/// may safely be accessed. Passing a std::vector& is "safe", but the caller
/// may not have the data in a vector.  The function could require an
/// explicit length to be passed (or a begin/end pair of iterators or
/// pointers). Any way you shake it, there is some awkwardness.
///
/// The array_view template tries to address this problem by providing
/// a way to pass array parameters that are non-owning, non-copying,
/// non-allocating, and contain a length reference (which in many cases
/// is transparently and automatically computed without additional user
/// code).

template <typename T, size_t Rank=1>
class array_view {
    OIIO_STATIC_ASSERT (Rank >= 1);
    OIIO_STATIC_ASSERT (is_array<T>::value == false);
public:
#if OIIO_CPLUSPLUS_VERSION >= 11
    // using iterator        = bounds_iterator<Rank>;
    // using const_iterator  = bounds_iterator<Rank>;
    static OIIO_CONSTEXPR_OR_CONST size_t rank = Rank;
    using offset_type     = offset<Rank>;
    using bounds_type     = OIIO::bounds<Rank>;
    using stride_type     = offset<Rank>;
    using size_type       = size_t;
    using value_type      = T;
    using pointer         = T*;
    using const_pointer   = const T*;
    using reference       = T&;
#else
    static const size_t rank = Rank;
    typedef offset<Rank> offset_type;
    typedef OIIO::bounds<Rank> bounds_type;
    typedef offset<Rank> stride_type;
    typedef size_t size_type;
    typedef T value_type;
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T& reference;
#endif

    /// Default ctr -- points to nothing
    array_view () : m_data(NULL) { }

    /// Copy constructor
    array_view (const array_view &copy)
        : m_data(copy.data()), m_bounds(copy.bounds()) {}

    /// Construct from T* and length.
    array_view (pointer data, bounds_type bounds)
        : m_data(data), m_bounds(bounds) { }

    /// Construct from a single T&.
    array_view (T &data) : m_data(&data), m_bounds(1) { }

    /// Construct from a fixed-length C array.  Template magic automatically
    /// finds the length from the declared type of the array.
    template<size_t N>
    array_view (T (&data)[N]) : m_data(data), m_bounds(N) {
        DASSERT (Rank == 1);
    }

    /// Construct from std::vector<T>.
    array_view (std::vector<T> &v)
        : m_data(v.size() ? &v[0] : NULL), m_bounds(v.size()) {
        DASSERT (Rank == 1);
    }

    /// Construct from const std::vector<T>.
    /// This turns const std::vector<T> into an array_view<const T> (the
    /// array_view isn't const, but the data it points to will be).
    array_view (const std::vector<typename remove_const<T>::type> &v)
        : m_data(v.size() ? &v[0] : NULL), m_bounds(v.size()) {
        DASSERT (Rank == 1);
    }

#if OIIO_CPLUSPLUS_VERSION >= 11
    /// Construct an array_view from an initializer_list.
    constexpr array_view (std::initializer_list<T> il)
        : array_view (il.begin(), il.size())
    { }
#endif

    // assignments
    array_view& operator= (const array_view &copy) {
        m_data = copy.data();
        m_bounds = copy.bounds();
        return *this;
    }

    OIIO_CONSTEXPR bounds_type bounds() const OIIO_NOEXCEPT {
        return m_bounds;
    }
    OIIO_CONSTEXPR14 size_type size() const OIIO_NOEXCEPT {
        return m_bounds.size();
    }
    OIIO_CONSTEXPR14 offset_type stride() const OIIO_NOEXCEPT {
        if (Rank == 1) {
            return offset_type(1);
        } else {
            offset_type offset;
            offset[Rank-1] = 1;
            for (int i = int(Rank)-2; i >= 0; --i)
                offset[i] = offset[i+1] * m_bounds[i+1];
            return offset;
        }
    }
    OIIO_CONSTEXPR pointer data() const OIIO_NOEXCEPT { return m_data; }

    OIIO_CONSTEXPR T& operator[] (offset_type idx) const {
        return VIEW_ACCESS(data(), idx, stride(), Rank);
    }
    T& at (offset_type idx) const {  // FIXME -- should be offset_type
        if (! bounds().contains(idx))
            throw (std::out_of_range ("OpenImageIO::array_view::at"));
        return VIEW_ACCESS(data(), idx, stride(), Rank);
    }
    // T& front() const { return m_data[0]; }   // FIXME - delete?
    // T& back() const { return m_data[size()-1]; }   // FIXME - delete?

    // FIXME -- slicing and sectioning

private:
    T * m_data;
    bounds_type m_bounds;

    reference VIEW_ACCESS (T* data, const offset_type &idx,
                           const stride_type &stride, size_t rank=Rank) const {
        ptrdiff_t offset = 0;
        for (size_t i = 0; i < rank; ++i)
            offset += idx[i] * stride[i];
        return data[offset];
    }
};




/// array_view_strided : a non-owning, mutable reference to a contiguous
/// array with known length and optionally non-default strides through the
/// data.   An array_view_strided<T> is mutable (the values in the array may
/// be modified), whereas an array_view_strided<const T> is not mutable.
template <typename T, size_t Rank=1>
class array_view_strided {
    OIIO_STATIC_ASSERT (Rank >= 1);
    OIIO_STATIC_ASSERT (is_array<T>::value == false);
public:
#if OIIO_CPLUSPLUS_VERSION >= 11
    static OIIO_CONSTEXPR_OR_CONST size_t rank = Rank;
    using offset_type     = offset<Rank>;
    using bounds_type     = OIIO::bounds<Rank>;
    using stride_type     = offset<Rank>;
    using size_type       = size_t;
    using value_type      = T;
    using pointer         = T*;
    using const_pointer   = const T*;
    using reference       = T&;
#else
    static const size_t rank = Rank;
    typedef offset<Rank> offset_type;
    typedef OIIO::bounds<Rank> bounds_type;
    typedef offset<Rank> stride_type;
    typedef size_t size_type;
    typedef T value_type;
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T& reference;
#endif

    /// Default ctr -- points to nothing
    array_view_strided () : m_data(NULL), m_stride(0) { }

    /// Copy constructor
    array_view_strided (const array_view_strided &copy)
        : m_data(copy.data()), m_bounds(copy.bounds()), m_stride(copy.stride()) {}

    /// Construct from T* and bounds.
    array_view_strided (T *data, bounds_type bounds)
        : m_data(data), m_bounds(bounds), m_stride(1) { }

    /// Construct from T*, bounds, and stride.
    array_view_strided (T *data, bounds_type bounds, stride_type stride)
        : m_data(data), m_bounds(bounds), m_stride(stride) { }

    /// Construct from a single T&.
    array_view_strided (T &data) : m_data(&data), m_bounds(1), m_stride(1) { }

    /// Construct from a fixed-length C array.  Template magic automatically
    /// finds the length from the declared type of the array.
    template<size_t N>
    array_view_strided (T (&data)[N]) : m_data(data), m_bounds(N), m_stride(1) {
        DASSERT (Rank == 1);
    }
    /// Construct from std::vector<T>.
    array_view_strided (std::vector<T> &v)
        : m_data(v.size() ? &v[0] : NULL), m_bounds(v.size()), m_stride(1) {
        DASSERT (Rank == 1);
    }

    /// Construct from const std::vector<T>.
    /// This turns const std::vector<T> into an array_view<const T> (the
    /// array_view isn't const, but the data it points to will be).
    array_view_strided (const std::vector<typename remove_const<T>::type> &v)
        : m_data(v.size() ? &v[0] : NULL), m_bounds(v.size()), m_stride(1) {
        DASSERT (Rank == 1);
    }

#if OIIO_CPLUSPLUS_VERSION >= 11
    /// Construct an array_view from an initializer_list.
    constexpr array_view_strided (std::initializer_list<T> il)
        : array_view_strided (il.begin(), il.size())
    { }
#endif

    // assignments
    array_view_strided& operator= (const array_view_strided &copy) {
        m_data = copy.data();
        m_bounds = copy.bounds();
        m_stride = copy.stride();
        return *this;
    }

    size_type size() const { return m_bounds.size(); }
    stride_type stride() const { return m_stride; }

    OIIO_CONSTEXPR T& operator[] (size_type idx) const {
        return VIEW_ACCESS(data(), idx, stride(), Rank);
    }
    const T& at (size_t idx) const {
        if (! bounds().contains(idx))
            throw (std::out_of_range ("OpenImageIO::array_view_strided::at"));
        return VIEW_ACCESS(data(), idx, stride(), Rank);
    }
    T& front() const { return m_data[0]; }
    T& back() const { return get(size()-1); }
    pointer data() const { return m_data; }
    bounds_type bounds () const { return m_bounds; }

private:
    T * m_data;
    bounds_type m_bounds;
    stride_type m_stride;

    reference VIEW_ACCESS (T* data, const offset_type &idx,
                           const stride_type &stride, size_t rank=Rank) const {
        ptrdiff_t offset = 0;
        for (size_t i = 0; i < rank; ++i)
            offset += idx[i] * stride[i];
        return data[offset];
    }
};



OIIO_NAMESPACE_END
