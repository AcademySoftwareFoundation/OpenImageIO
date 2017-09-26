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

#include <array>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <cstddef>
#include <initializer_list>
#include <type_traits>

#include <OpenImageIO/oiioversion.h>
#include <OpenImageIO/platform.h>
#include <OpenImageIO/dassert.h>

OIIO_NAMESPACE_BEGIN



/// array_view<T> : a non-owning reference to a contiguous array with known
/// length.  An array_view<T> is mutable (the values in the array may be
/// modified), whereas an array_view<const T> is not mutable.
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

template <typename T>
class array_view {
    static_assert (std::is_array<T>::value == false, "can't have array_view of an array");
public:
    using size_type       = size_t;
    using value_type      = T;
    using pointer         = T*;
    using const_pointer   = const T*;
    using reference       = T&;
    using iterator        = T*;
    using const_iterator  = const T*;
    using nonconst_value_type = typename std::remove_const<value_type>::type;

    /// Default ctr -- points to nothing
    constexpr array_view () { }

    /// Copy constructor
    constexpr array_view (const array_view &copy) noexcept
        : m_data(copy.data()), m_size(copy.size()) { }

    /// Construct from T* and length.
    constexpr array_view (pointer data, size_t size) noexcept
        : m_data(data), m_size(size) { }

    /// Construct from begin and end pointers
    constexpr array_view (pointer b, pointer e) noexcept
        : m_data(b), m_size(e-b) { }

    /// Construct from a single T&.
    constexpr array_view (T &data) : m_data(&data), m_size(1) { }

    /// Construct from a fixed-length C array.  Template magic automatically
    /// finds the length from the declared type of the array.
    template<size_t N>
    constexpr array_view (T (&data)[N]) : m_data(data), m_size(N) { }

    /// Construct from std::vector<T>.
    constexpr array_view (std::vector<T> &v)
        : m_data(v.size() ? &v[0] : nullptr), m_size(v.size()) {
    }

    /// Construct from const std::vector<T>.
    /// This turns const std::vector<T> into an array_view<const T> (the
    /// array_view isn't const, but the data it points to will be).
    array_view (const std::vector<typename std::remove_const<T>::type> &v)
        : m_data(v.size() ? &v[0] : nullptr), m_size(v.size()) { }

    /// Construct from mutable element std::array
    template <size_t N>
    constexpr array_view (std::array<nonconst_value_type, N> &arr)
        : m_data(arr.data()), m_size(N) {}

    /// Construct from read-only element std::array
    template <size_t N>
    constexpr array_view (const std::array<nonconst_value_type, N>& arr)
        : m_data(arr.data()), m_size(N) {}

    /// Construct an array_view from an initializer_list.
    constexpr array_view (std::initializer_list<T> il)
        : array_view (il.begin(), il.size()) { }

    // assignments
    array_view& operator= (const array_view &copy) {
        m_data = copy.data();
        m_size = copy.size();
        return *this;
    }

    constexpr size_t size() const noexcept { return m_size; }

    constexpr pointer data() const noexcept { return m_data; }

    constexpr T& operator[] (size_t idx) const noexcept {
        return m_data[idx];
    }
    T& at (size_t idx) const {
        if (idx >= size())
            throw (std::out_of_range ("OpenImageIO::array_view::at"));
        return m_data[idx];
    }

    constexpr T& front() const noexcept { return m_data[0]; }
    constexpr T& back() const noexcept { return m_data[size()-1]; }

    constexpr pointer begin() const noexcept { return m_data; }
    constexpr pointer end() const noexcept { return m_data + m_size; }

    constexpr const_pointer cbegin() const noexcept { return m_data; }
    constexpr const_pointer cend() const noexcept { return m_data + m_size; }

private:
    T*     m_data = nullptr;
    size_t m_size = 0;
};




/// array_view_strided<T> : a non-owning, mutable reference to a contiguous
/// array with known length and optionally non-default strides through the
/// data.  An array_view_strided<T> is mutable (the values in the array may
/// be modified), whereas an array_view_strided<const T> is not mutable.
template <typename T>
class array_view_strided {
    static_assert (std::is_array<T>::value == false,
                   "can't have array_view_strided of an array");
public:
    using size_type       = size_t;
    using stride_type     = ptrdiff_t;
    using value_type      = T;
    using pointer         = T*;
    using const_pointer   = const T*;
    using reference       = T&;

    /// Default ctr -- points to nothing
    constexpr array_view_strided () {}

    /// Copy constructor
    constexpr array_view_strided (const array_view_strided &copy)
        : m_data(copy.data()), m_size(copy.size()), m_stride(copy.stride()) {}

    /// Construct from T* and size, and optionally stride.
    constexpr array_view_strided (T *data, size_t size, stride_type stride=1)
        : m_data(data), m_size(size), m_stride(stride) { }

    /// Construct from a single T&.
    constexpr array_view_strided (T &data) : array_view_strided(&data,1,1) { }

    /// Construct from a fixed-length C array.  Template magic automatically
    /// finds the length from the declared type of the array.
    template<size_t N>
    constexpr array_view_strided (T (&data)[N]) : array_view_strided(data,N,1) {}

    /// Construct from std::vector<T>.
    OIIO_CONSTEXPR14 array_view_strided (std::vector<T> &v)
        : array_view_strided(v.size() ? &v[0] : nullptr, v.size(), 1) {}

    /// Construct from const std::vector<T>. This turns const std::vector<T>
    /// into an array_view_strided<const T> (the array_view_strided isn't
    /// const, but the data it points to will be).
    constexpr array_view_strided (const std::vector<typename std::remove_const<T>::type> &v)
        : array_view_strided(v.size() ? &v[0] : nullptr, v.size(), 1) {}

    /// Construct an array_view from an initializer_list.
    constexpr array_view_strided (std::initializer_list<T> il)
        : array_view_strided (il.begin(), il.size())
    { }

    /// Initialize from an array_view (stride will be 1).
    constexpr array_view_strided (array_view<T> av)
        : array_view_strided(av.data(), av.size(), 1) { }

    // assignments
    array_view_strided& operator= (const array_view_strided &copy) {
        m_data   = copy.data();
        m_size   = copy.size();
        m_stride = copy.stride();
        return *this;
    }

    constexpr size_type size() const noexcept { return m_size; }
    constexpr stride_type stride() const noexcept { return m_stride; }

    constexpr T& operator[] (size_type idx) const {
        return m_data[m_stride*idx];
    }
    const T& at (size_t idx) const {
        if (idx >= size())
            throw (std::out_of_range ("OpenImageIO::array_view_strided::at"));
        return m_data[m_stride*idx];
    }
    constexpr T& front() const noexcept { return m_data[0]; }
    constexpr T& back() const noexcept { return (*this)[size()-1]; }
    constexpr pointer data() const noexcept { return m_data; }

private:
    T *         m_data   = nullptr;
    size_t      m_size   = 0;
    stride_type m_stride = 1;
};



OIIO_NAMESPACE_END
