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


#ifndef REFCNT_H
#define REFCNT_H

#include "thread.h"
#include "atomic.h"

// Use Boost for shared pointers
#include <boost/tr1/memory.hpp>
using std::tr1::shared_ptr;
#include <boost/intrusive_ptr.hpp>
using boost::intrusive_ptr;



/// Mix-in class that adds a reference count, implemented as an atomic
/// counter.
class RefCnt {
public:
    AtomicInt m_refcnt;
};



/// Generic implementation of intrusive_ptr_add_ref, which is needed for
/// any class that you use with Boost's intrusive_ptr.
template<class T>
inline void intrusive_ptr_add_ref (T *x)
{
    ++ x->m_refcnt;
}

/// Generic implementation of intrusive_ptr_release, which is needed for
/// any class that you use with Boost's intrusive_ptr.
template<class T>
inline void intrusive_ptr_release (T *x)
{
    if (-- x->m_refcnt == 0)
        delete x;
}


#endif // REFCNT_H

