/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
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


/////////////////////////////////////////////////////////////////////////
/// \file
///
/// Wrappers and utilities for reference counting.
/////////////////////////////////////////////////////////////////////////


#ifndef OPENIMAGEIO_REFCNT_H
#define OPENIMAGEIO_REFCNT_H

#include <OpenImageIO/atomic.h>

#if OIIO_CPLUSPLUS_VERSION >= 11
# include <memory>
#else
# include <boost/shared_ptr.hpp>
#endif



OIIO_NAMESPACE_BEGIN

#if OIIO_CPLUSPLUS_VERSION < 11
using boost::shared_ptr;
#else
using std::shared_ptr;
#endif



/// A simple intrusive pointer, modeled after std::shared_ptr and
/// boost::intrusive_ptr.
template<class T>
class intrusive_ptr
{
public:
    typedef T element_type;

    /// Default ctr
    intrusive_ptr () OIIO_NOEXCEPT : m_ptr(NULL) { }

    /// Construct from a raw pointer (presumed to be just now allocated,
    /// and now owned by us).
    intrusive_ptr (T *ptr) : m_ptr(ptr) {
        if (m_ptr) intrusive_ptr_add_ref(m_ptr);
    }

    /// Construct from another intrusive_ptr.
    intrusive_ptr (const intrusive_ptr &r) : m_ptr(r.get()) {
        if (m_ptr) intrusive_ptr_add_ref (m_ptr);
    }

#if OIIO_CPLUSPLUS_VERSION >= 11
    /// Move construct from another intrusive_ptr.
    intrusive_ptr (intrusive_ptr &&r) OIIO_NOEXCEPT : m_ptr(r.get()) {
        r.m_ptr = NULL;
    }
#endif

    /// Destructor
    ~intrusive_ptr () { if (m_ptr) intrusive_ptr_release (m_ptr); }

    /// Assign from intrusive_ptr
    intrusive_ptr & operator= (const intrusive_ptr &r) {
        intrusive_ptr(r).swap (*this);
        return *this;
    }

#if OIIO_CPLUSPLUS_VERSION >= 11
    /// Move assignment from intrusive_ptr
    intrusive_ptr & operator= (intrusive_ptr&& r) OIIO_NOEXCEPT {
        intrusive_ptr (static_cast<intrusive_ptr&&>(r)).swap(*this);
        return *this;
    }
#endif

    /// Reset to null reference
    void reset () OIIO_NOEXCEPT {
        if (m_ptr) { intrusive_ptr_release (m_ptr); m_ptr = NULL; }
    }

    /// Reset to point to a pointer
    void reset (T *r) {
        if (r != m_ptr) {
            if (r) intrusive_ptr_add_ref (r);
            if (m_ptr) intrusive_ptr_release (m_ptr);
            m_ptr = r;
        }
    }

    /// Swap intrusive pointers
    void swap (intrusive_ptr &r) OIIO_NOEXCEPT {
        T *tmp = m_ptr; m_ptr = r.m_ptr; r.m_ptr = tmp;
    }

    /// Dereference
    T& operator*() const { DASSERT (m_ptr); return *m_ptr; }

    /// Dereference
    T* operator->() const { DASSERT (m_ptr); return m_ptr; }

    /// Get raw pointer
    T* get() const OIIO_NOEXCEPT { return m_ptr; }

    /// Cast to bool to detect whether it points to anything
    operator bool () const OIIO_NOEXCEPT { return m_ptr != NULL; }

private:
    T* m_ptr;   // the raw pointer
};



/// Mix-in class that adds a reference count, implemented as an atomic
/// counter.
class RefCnt {
protected:
    // Declare RefCnt constructors and destructors protected because they
    // should only be called implicitly from within child class constructors or
    // destructors.  In particular, this prevents users from deleting a RefCnt*
    // which is important because the destructor is non-virtual.

    RefCnt () { m_refcnt = 0; }

    /// Define copy constructor to NOT COPY reference counts! Copying a
    /// struct doesn't change how many other things point to it.
    RefCnt (RefCnt&) { m_refcnt = 0; }

    ~RefCnt () {}

public:
    /// Add a reference
    ///
    void _incref () const { ++m_refcnt; }

    /// Delete a reference, return true if that was the last reference.
    ///
    bool _decref () const { return (--m_refcnt) == 0; }

    /// Define operator= to NOT COPY reference counts!  Assigning a struct
    /// doesn't change how many other things point to it.
    const RefCnt & operator= (const RefCnt&) const { return *this; }

private:
    mutable atomic_int m_refcnt;
};



/// Implementation of intrusive_ptr_add_ref, which is needed for
/// any class that you use with intrusive_ptr.
template <class T>
inline void intrusive_ptr_add_ref (T *x)
{
    x->_incref ();
}

/// Implementation of intrusive_ptr_release, which is needed for
/// any class that you use with intrusive_ptr.
template <class T>
inline void intrusive_ptr_release (T *x)
{
    if (x->_decref ())
        delete x;
}

// Note that intrusive_ptr_add_ref and intrusive_ptr_release MUST be a
// templated on the full type, so that they pass the right address to
// 'delete' and destroy the right type.  If you try to just 
// 'inline void intrusive_ptr_release (RefCnt *x)', that might seem 
// clever, but it will end up getting the address of (and destroying)
// just the inherited RefCnt sub-object, not the full subclass you
// meant to delete and destroy.


OIIO_NAMESPACE_END

#endif // OPENIMAGEIO_REFCNT_H
