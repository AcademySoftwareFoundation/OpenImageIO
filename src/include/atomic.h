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
/// Atomic operations
/////////////////////////////////////////////////////////////////////////


#ifndef ATOMIC_H
#define ATOMIC_H


#if defined(__linux__)
# include <ext/atomicity.h>
#elif defined(__APPLE__)
# include <libkern/OSAtomic.h>
#elif defined(_WIN32)
# include <windows.h>
# include <winbase.h>
#endif


/// Atomic integer.  Increment, decrement, add, and subtrace in a
/// totally thread-safe manner.
class AtomicInt {
public:
    /// Construct with initial value.
    ///
    AtomicInt (int val=0) : m_val(val) { }

    ~AtomicInt () { }

    /// Retrieve value
    ///
    int operator() () { return m_val; }

    /// Assign new value.
    ///
    int operator= (int x) { m_val = x; }

    /// Pre-increment:  ++foo
    ///
    int operator++ () {
#if defined(__linux__)
        return __gnu_cxx::__exchange_and_add (&m_val, 1) + 1;
#elif defined(__APPLE__)
        return OSAtomicIncrement32Barrier (&m_val);
#elif defined(_WIN32)
        InterlockedIncrement (&m_val);
#endif
    }

    /// Post-increment:  foo++
    ///
    int operator++ (int) {  // post-increment
#if defined(__linux__)
        return __gnu_cxx::__exchange_and_add (&m_val, 1);
#elif defined(__APPLE__)
        return OSAtomicIncrement32Barrier (&m_val) - 1;
#elif defined(_WIN32)
        return InterlockedExchangeAdd (&m_val, 1);
#endif
    }

    /// Pre-decrement:  --foo
    ///
    int operator-- () {  // pre-decrement
#if defined(__linux__)
        return __gnu_cxx::__exchange_and_add (&m_val, -1) - 1;
#elif defined(__APPLE__)
        return OSAtomicDecrement32Barrier (&m_val);
#elif defined(_WIN32)
        return InterlockedDecrement (&m_val);
#endif
    }

    /// Post-decrement:  foo--
    ///
    int operator-- (int) {  // post-decrement
#if defined(__linux__)
        return __gnu_cxx::__exchange_and_add (&m_val, -1);
#elif defined(__APPLE__)
        return OSAtomicDecrement32Barrier (&m_val) + 1;
#elif defined(_WIN32)
        return InterlockedExchangeAdd (&m_val, -1);
#endif
    }

    /// Add to the value, return the new sum
    ///
    int operator+= (int x) {
#if defined(__linux__)
        return __gnu_cxx::__exchange_and_add (&m_val, x) + x;
#elif defined(__APPLE__)
        return OSAtomicAdd32Barrier (x, &m_val);
#elif defined(_WIN32)
        return InterlockedExchangeAdd (&m_val, x) + x;
#endif
    }

    /// Subtrace from the value, return the new value
    ///
    int operator-= (int x) {
#if defined(__linux__)
        return __gnu_cxx::__exchange_and_add (&m_val, -x) - x;
#elif defined(__APPLE__)
        return OSAtomicAdd32Barrier (-x, &m_val);
#elif defined(_WIN32)
        return InterlockedExchangeAdd (&m_val, -x) - x;
#endif
    }

private:
    volatile int m_val;
};





#endif // ATOMIC_H

