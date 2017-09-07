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

#include <cstddef>

#include <OpenImageIO/oiioversion.h>
#include <OpenImageIO/platform.h>


OIIO_NAMESPACE_BEGIN


/// strided_ptr<T> looks like a 'T*', but it incorporates a stride, so
/// it's not limited to adjacent elements.
/// Operators ++, --, [], and so on, take the stride into account when
/// computing where each "array element" actually exists.
///
/// A strided_ptr<T> is mutable (the values pointed to may be modified),
/// whereas an strided_ptr<const T> is not mutable.
///
/// Fun trick: strided_ptr<T>(&my_value,0) makes a strided_pointer that
/// is addressed like an array, but because the stride is 0, every
/// accessed "element" actually will actually refer to the same value.
///
/// By default, if StrideUnits == sizeof(T), then the stride refers to
/// multiples of the size of T. But every once in a while, you need a
/// a byte-addressable stride, and in that case you use a StrideUnits
/// of 1, like:   strided_ptr<T,1>.
template <typename T, int StrideUnits = sizeof(T) >
class strided_ptr {
public:
    constexpr strided_ptr (T* ptr=nullptr, ptrdiff_t stride=1) noexcept
        : m_ptr(ptr), m_stride(stride) { }
    constexpr strided_ptr (const strided_ptr &p) noexcept
        : strided_ptr(p.data(), p.stride()) { }

    const strided_ptr& operator= (const strided_ptr &p) noexcept {
        m_ptr = p.m_ptr;
        m_stride = p.m_stride;
        return *this;
    }

    // Assignment of a pointer sets the pointer and implies a stride of 1.
    const strided_ptr& operator= (T *p) noexcept {
        m_ptr = p;
        m_stride = 1;
        return *this;
    }

    constexpr T& operator* () const { return *m_ptr; }
    constexpr T& operator[] (ptrdiff_t pos) const { return get(pos); }
    constexpr T* data() const { return m_ptr; }
    constexpr ptrdiff_t stride () const { return m_stride; }

    // Careful: == and != only compare the pointer
    constexpr bool operator== (const T *p) const { return m_ptr == p; }
    constexpr bool operator!= (const T *p) const { return m_ptr != p; }

    // Increment and decrement moves the pointer to the next element
    // one stride length away.
    const strided_ptr& operator++ () {   // prefix
        m_ptr = getptr(1);
        return *this;
    }
    strided_ptr operator++(int) {  // postfix
        strided_ptr r(*this);
        ++(*this);
        return r;
    }
    const strided_ptr& operator-- () {   // prefix
        m_ptr = getptr(-1);
        return *this;
    }
    strided_ptr operator--(int) {  // postfix
        strided_ptr r(*this);
        --(*this);
        return r;
    }

    // Addition and subtraction returns new strided pointers that are
    // the given number of strides away.
    constexpr strided_ptr operator+ (ptrdiff_t d) const noexcept {
        return strided_ptr (getptr(d), m_stride);
    }
    constexpr strided_ptr operator- (ptrdiff_t d) const noexcept {
        return strided_ptr (getptr(-d), m_stride);
    }
    const strided_ptr& operator+= (ptrdiff_t d) noexcept {
        m_ptr = getptr(d);
        return *this;
    }
    const strided_ptr& operator-= (ptrdiff_t d) {
        m_ptr = getptr(-d);
        return *this;
    }

private:
    // The implementation of a strided_ptr is just the pointer and a stride.
    // Note that when computing addressing, the stride is implicitly
    // multiplied by the StrideUnits, which defaults to sizeof(T), but when
    // StrideUnits==1 means that your stride value is measured in bytes.
    T *        m_ptr    = nullptr;
    ptrdiff_t  m_stride = 1;

    // getptr is the real brains of the operation, computing the pointer
    // for a given element, with strides taken into consideration.
    constexpr inline T* getptr (ptrdiff_t pos=0) const noexcept {
        return (T*)((char *)m_ptr + pos*m_stride*StrideUnits);
    }
    constexpr inline T& get (ptrdiff_t pos=0) const noexcept {
        return *getptr(pos);
    }
};



OIIO_NAMESPACE_END
