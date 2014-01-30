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
#include <algorithm>

#include "version.h"
#include "export.h"


OIIO_NAMESPACE_ENTER {


/// array_ref : a non-owning reference to a contiguous array with known
/// length.
///
/// Background: Functions whose input requires a set of contiguous values
/// (an array) are faced with a dilemma. If the caller passes just a
/// pointer, the function has no inherent way to determine how many elements
/// may safely be accessed. Passing a std::vector& is "safe", but the caller
/// may not have the data in a vector.  The function could require an
/// explicit length to be passed (or a begin/end pair of iterators or
/// pointers). Any way you shake it, there is some awkwardness.
///
/// The array_ref template tries to address this problem by providing
/// a way to pass array parameters that are non-owning, non-copying,
/// non-allocating, and contain a length reference (which in many cases
/// is transparently and automatically computed without additional user
/// code).


template <typename T>
class array_ref {
public:
    typedef T value_type;
    typedef const T* pointer;
    typedef const T* const_pointer;
    typedef const T& reference;
    typedef const T& const_reference;
    typedef const_pointer const_iterator;
    typedef const_iterator iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
    typedef const_reverse_iterator reverse_iterator;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    static const size_type npos = ~size_type(0);

    /// Default ctr -- points to nothing
    array_ref () : m_data(NULL), m_len(0) {}

    /// Copy constructor
    array_ref (const array_ref &copy)
        : m_data(copy.data()), m_len(copy.size()) {}

    /// Construct from T* and length.
    array_ref (const T *data, size_t len) : m_data(data), m_len(len) {}

    /// Construct from a single T&.
    array_ref (const T &data) : m_data(&data), m_len(1) {}

    /// Construct from a range (begin and end pointers).
    array_ref (const T *begin, const T *end)
        : m_data(begin), m_len(end-begin) {}

    /// Construct from a fixed-length C array.  Template magic automatically
    /// finds the length from the declared type of the array.
    template<size_t N>
    array_ref (const T (&data)[N]) : m_data(data), m_len(N) {}

    /// Construct from std::vector<T>.
    array_ref (const std::vector<T> &v)
        : m_data(v.size() ? &v[0] : NULL), m_len(v.size()) {}

    // assignments
    array_ref& operator= (const array_ref &copy) {
        m_data = copy.data();
        m_len = copy.size();
        return *this;
    }

    // iterators
    const_iterator begin() const { return m_data; }
    const_iterator end() const { return m_data + m_len; }
    const_iterator cbegin() const { return m_data; }
    const_iterator cend() const { return m_data + m_len; }
    const_reverse_iterator rbegin() const { return const_reverse_iterator (end()); }
    const_reverse_iterator rend() const { return const_reverse_iterator (begin()); }
    const_reverse_iterator crbegin() const { return const_reverse_iterator (end()); }
    const_reverse_iterator crend() const { return const_reverse_iterator (begin()); }

    size_type size() const { return m_len; }
    size_type max_size() const { return m_len; }
    bool empty() const { return m_len == 0; }

    const T& operator[] (size_type pos) const { return m_data[pos]; }
    const T& at (size_t pos) const {
        if (pos >= m_len)
            throw (std::out_of_range ("OpenImageIO::array_ref::at"));
        return m_data[pos];
    }
    const T& front() const { return m_data[0]; }
    const T& back() const { return m_data[m_len-1]; }
    const T* data() const { return m_data; }

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

    array_ref slice (size_type pos, size_type n=npos) const {
        if (pos > size())
            throw (std::out_of_range ("OIIO::array_ref::slice");
        if (n == npos || pos + n > size())
            n = size() - pos;
        return array_ref (data() + pos, n);
    }

    template<typename T>
    friend bool operator== (array_ref<T> x, array_ref<T> y) {
        if (x.size() != y.size())  // shortcut: different sizes are not ==
            return false;
        if (x.data() == y.data())  // shortcut: same size, same ptr: equal
            return true;
        for (size_t i = 0, e = x.size(); i != e; ++i)
            if (x[i] != y[i])
                return false;
        return true;
    }

    template<typename T>
    friend bool operator!= (array_ref<T> x, array_ref<T> y) {
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
    const T * m_data;
    size_t m_len;

    void init (const T *data, size_t len) {
        m_data = data;
        m_len = len;
    }
};




} OIIO_NAMESPACE_EXIT
