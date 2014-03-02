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

#include "oiioversion.h"


OIIO_NAMESPACE_ENTER {


/// strided_ptr<T> looks like a 'T*', but it incorporates a stride (in
/// bytes) that may be different than sizeof(T).  Operators ++, --, [], and
/// so on, take the stride into account when computing where each "array
/// element" actually exists.  A strided_ptr<T> is mutable (the values
/// pointed to may be modified), whereas an strided_ptr<const T> is not
/// mutable.
template <typename T>
class strided_ptr {
public:
    strided_ptr (T* ptr=NULL, ptrdiff_t stride=sizeof(T))
        : m_ptr(ptr), m_stride(stride) { }
    strided_ptr (const strided_ptr &p)
        : m_ptr(p.m_ptr), m_stride(p.m_stride) {}
    const strided_ptr& operator= (const strided_ptr &p) {
        m_ptr = p.m_ptr;
        m_stride = p.m_stride;
        return *this;
    }

    T& operator* () const { return *m_ptr; }
    T& operator[] (ptrdiff_t pos) const { return get(pos); }
    T* data() const { return m_ptr; }
    ptrdiff_t stride () const { return m_stride; }
    bool operator== (const T *p) const { return m_ptr == p; }
    bool operator!= (const T *p) const { return m_ptr != p; }

    const strided_ptr& operator++ () {   // prefix
        m_ptr = getptr(1);
        return *this;
    }
    strided_ptr operator++(int) {  // postfix
        strided_ptr r;
        ++(*this);
        return r;
    }
    const strided_ptr& operator-- () {   // prefix
        m_ptr = getptr(-1);
        return *this;
    }
    strided_ptr operator--(int) {  // postfix
        strided_ptr r;
        --(*this);
        return r;
    }

    strided_ptr operator+ (int d) const {
        return strided_ptr (getptr(d), m_stride);
    }
    const strided_ptr& operator+= (int d) {
        m_ptr = getptr(d);
        return *this;
    }
    strided_ptr operator- (int d) const {
        return strided_ptr (getptr(-d), m_stride);
    }
    const strided_ptr& operator-= (int d) {
        m_ptr = getptr(-d);
        return *this;
    }

private:
    T *        m_ptr;
    ptrdiff_t  m_stride;
    inline T* getptr (ptrdiff_t pos=0) const {
        return (T*)((char *)m_ptr + pos*m_stride);
    }
    inline T& get (ptrdiff_t pos=0) const {
        return *getptr(pos);
    }
};



} OIIO_NAMESPACE_EXIT
