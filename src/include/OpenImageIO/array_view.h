/*
  Copyright 2014 Larry Gritz and the other authors and contributors.
  All Rights Reserved.
  Based on BSD-licensed software Copyright 2004 NVIDIA Corp.

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

#include "oiioversion.h"
#include "strided_ptr.h"


OIIO_NAMESPACE_ENTER {


/// array_view : a non-owning reference to a contiguous array with known
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
public:
    typedef T value_type;
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T& reference;
    typedef const T& const_reference;
    typedef const_pointer const_iterator;
    typedef const_iterator iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
    typedef const_reverse_iterator reverse_iterator;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    static const size_type npos = ~size_type(0);

    /// Default ctr -- points to nothing
    array_view () : m_data(NULL), m_len(0) {}

    /// Copy constructor
    array_view (const array_view &copy)
        : m_data(copy.data()), m_len(copy.size()) {}

    /// Construct from T* and length.
    array_view (T *data, size_t len) : m_data(data), m_len(len) {}

    /// Construct from a single T&.
    array_view (T &data) : m_data(&data), m_len(1) {}

    /// Construct from a range (begin and end pointers).
    array_view (T *begin, T *end)
        : m_data(begin), m_len(end-begin) {}

    /// Construct from a fixed-length C array.  Template magic automatically
    /// finds the length from the declared type of the array.
    template<size_t N>
    array_view (T (&data)[N]) : m_data(data), m_len(N) {}

    /// Construct from std::vector<T>.
    array_view (std::vector<T> &v)
        : m_data(v.size() ? &v[0] : NULL), m_len(v.size()) {}

    // assignments
    array_view& operator= (const array_view &copy) {
        m_data = copy.data();
        m_len = copy.size();
        return *this;
    }

    // iterators
    iterator begin() const { return m_data; }
    iterator end() const { return m_data + m_len; }
    const_iterator cbegin() const { return m_data; }
    const_iterator cend() const { return m_data + m_len; }
    reverse_iterator rbegin() const { return const_reverse_iterator (end()); }
    reverse_iterator rend() const { return const_reverse_iterator (begin()); }
    const_reverse_iterator crbegin() const { return const_reverse_iterator (end()); }
    const_reverse_iterator crend() const { return const_reverse_iterator (begin()); }

    size_type size() const { return m_len; }
    size_type max_size() const { return m_len; }
    bool empty() const { return m_len == 0; }

    T& operator[] (size_type pos) const { return m_data[pos]; }
    T& at (size_t pos) const {
        if (pos >= m_len)
            throw (std::out_of_range ("OpenImageIO::array_view::at"));
        return m_data[pos];
    }
    T& front() const { return m_data[0]; }
    T& back() const { return m_data[m_len-1]; }
    T* data() const { return m_data; }

    void clear() { m_data = NULL;  m_len = 0; }

    void remove_prefix(size_type n) {
        if (n > m_len)
            n = m_len;
        m_data += n;
        m_len -= n;
    }
    void remove_suffix(size_type n) {
        if (n > m_len)
            n = m_len;
        m_len -= n;
    }

    array_view slice (size_type pos, size_type n=npos) const {
        if (pos > size())
            throw (std::out_of_range ("OIIO::array_view::slice"));
        if (n == npos || pos + n > size())
            n = size() - pos;
        return array_view (data() + pos, n);
    }

    friend bool operator== (array_view<T> x, array_view<T> y) {
        if (x.size() != y.size())  // shortcut: different sizes are not ==
            return false;
        if (x.data() == y.data())  // shortcut: same size, same ptr: equal
            return true;
        for (size_t i = 0, e = x.size(); i != e; ++i)
            if (x[i] != y[i])
                return false;
        return true;
    }

    friend bool operator!= (array_view<T> x, array_view<T> y) {
        if (x.size() != y.size())  // shortcut: different sizes are !=
            return true;
        if (x.data() == y.data())  // shortcut: same size, same ptr: equal
            return false;
        for (size_t i = 0, e = x.size(); i != e; ++i)
            if (x[i] == y[i])
                return false;
        return true;
    }

private:
    T * m_data;
    size_t m_len;

    void init (T *data, size_t len) {
        m_data = data;
        m_len = len;
    }
};




/// array_view_strided : a non-owning, mutable reference to a contiguous
/// array with known length and optionally non-default strides through the
/// data.   An array_view_strided<T> is mutable (the values in the array may
/// be modified), whereas an array_view_strided<const T> is not mutable.
template <typename T>
class array_view_strided {
public:
    typedef T value_type;
    typedef strided_ptr<T> pointer;
    typedef strided_ptr<const T> const_pointer;
    typedef T& reference;
    typedef const T& const_reference;
    typedef const_pointer const_iterator;
    typedef pointer iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
    typedef std::reverse_iterator<iterator> reverse_iterator;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef ptrdiff_t stride_t;
    static const size_type npos = ~size_type(0);
    static const stride_t AutoStride = ~stride_t(0);

    /// Default ctr -- points to nothing
    array_view_strided () : m_data(NULL), m_len(0), m_stride(0) {}

    /// Copy constructor
    array_view_strided (const array_view_strided &copy)
        : m_data(copy.data()), m_len(copy.size()), m_stride(copy.stride()) {}

    /// Construct from T* and length.
    array_view_strided (T *data, size_t len) { init(data,len); }

    /// Construct from T*, length, and stride (in bytes).
    array_view_strided (T *data, size_t len, stride_t stride) {
        init(data,len,stride);
    }

    /// Construct from a single T&.
    array_view_strided (T &data) { init (&data, 1); }

    /// Construct from a range (begin and end pointers).
    array_view_strided (T *begin, T *end) { init (begin, end-begin); }

    /// Construct from a fixed-length C array.  Template magic automatically
    /// finds the length from the declared type of the array.
    template<size_t N>
    array_view_strided (T (&data)[N]) { init(data,N); }

    /// Construct from std::vector<T>.
    array_view_strided (std::vector<T> &v)
        : m_data(v.size() ? &v[0] : NULL), m_len(v.size()), m_stride(sizeof(T)) {}

    // assignments
    array_view_strided& operator= (const array_view_strided &copy) {
        m_data = copy.data();
        m_len = copy.size();
        m_stride = copy.stride();
        return *this;
    }

    // iterators
    iterator begin() const { return pointer(m_data,m_stride); }
    iterator end() const { return pointer(getptr(m_len),m_stride); }
    const_iterator cbegin() const { return const_pointer(m_data,m_stride); }
    const_iterator cend() const { return const_pointer(getptr(m_len),m_stride); }
    reverse_iterator rbegin() const { return reverse_iterator (end()); }
    reverse_iterator rend() const { return reverse_iterator (begin()); }
    const_reverse_iterator crbegin() const { return const_reverse_iterator (end()); }
    const_reverse_iterator crend() const { return const_reverse_iterator (begin()); }

    size_type size() const { return m_len; }
    size_type max_size() const { return m_len; }
    bool empty() const { return m_len == 0; }
    stride_t stride() const { return m_stride; }

    const T& operator[] (size_type pos) const { return get(pos); }
    const T& at (size_t pos) const {
        if (pos >= m_len)
            throw (std::out_of_range ("OpenImageIO::array_view_strided::at"));
        return get(pos);
    }
    T& front() const { return m_data[0]; }
    T& back() const { return get(m_len-1); }
    pointer data() const { return pointer(m_data,m_stride); }

    void clear() { m_data = NULL;  m_len = 0;  m_stride = 0; }

    array_view_strided slice (size_type pos, size_type n=npos) const {
        if (pos > size())
            throw (std::out_of_range ("OIIO::array_view_strided::slice"));
        if (n == npos || pos + n > size())
            n = size() - pos;
        return array_view_strided (getptr(pos), n, m_stride);
    }

    friend bool operator== (array_view_strided<T> x, array_view_strided<T> y) {
        if (x.size() != y.size())  // shortcut: different sizes are not ==
            return false;
        if (x.data() == y.data())  // shortcut: same size, same ptr: equal
            return true;
        for (size_t i = 0, e = x.size(); i != e; ++i)
            if (x[i] != y[i])
                return false;
        return true;
    }

    friend bool operator!= (array_view_strided<T> x, array_view_strided<T> y) {
        if (x.size() != y.size())  // shortcut: different sizes are !=
            return true;
        if (x.data() == y.data())  // shortcut: same size, same ptr: equal
            return false;
        for (size_t i = 0, e = x.size(); i != e; ++i)
            if (x[i] == y[i])
                return false;
        return true;
    }

private:
    T * m_data;
    size_t m_len;
    stride_t m_stride;

    void init (T *data, size_t len, stride_t stride=AutoStride) {
        m_data = data;
        m_len = len;
        m_stride = stride == AutoStride ? sizeof(T) : stride;
    }
    inline T* getptr (stride_t pos=0) const {
        return (T*)((char *)m_data + pos*m_stride);
    }
    inline T& get (stride_t pos=0) const {
        return *getptr(pos);
    }

};



} OIIO_NAMESPACE_EXIT
