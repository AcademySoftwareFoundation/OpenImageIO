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
#  include <ext/atomicity.h>
#  define USE_INTEL_ASM_ATOMICS

#elif defined(__APPLE__)
#  if (defined(__i386__) || defined(__x86_64__))
#    define USE_INTEL_ASM_ATOMICS
#  else
#    include <libkern/OSAtomic.h>
#  endif

#elif defined(_WIN32)
#  include <windows.h>
#  include <winbase.h>
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
    int operator++ () { return exchange_and_add (&m_val, 1) + 1; }

    /// Post-increment:  foo++
    ///
    int operator++ (int) {  return exchange_and_add (&m_val, 1); }

    /// Pre-decrement:  --foo
    ///
    int operator-- () {  return exchange_and_add (&m_val, -1) - 1; }

    /// Post-decrement:  foo--
    ///
    int operator-- (int) {  return exchange_and_add (&m_val, -1); }

    /// Add to the value, return the new result
    ///
    int operator+= (int x) { return exchange_and_add (&m_val, x) + x; }

    /// Subtract from the value, return the new result
    ///
    int operator-= (int x) { return exchange_and_add (&m_val, -x) - x; }

private:
    volatile int m_val;

    /// Atomic version of:  r = *at, *at += x, return r
    /// For each of several architectures.
    int exchange_and_add (volatile int *at, int x) {
#if defined(USE_INTEL_ASM_ATOMICS)
        // Common case of i386 or x86_64 on either Linux or Mac.
        // Note slightly different instruction for 32 vs 64 bit.
        int result;
#ifdef __i386__
        __asm__ __volatile__("lock\nxaddl %0,%1"
                             : "=r"(result), "=m"(*at)
                             : "0"(x)
                             : "memory");
#else
        __asm__ __volatile__("lock\nxadd %0,%1"
                             : "=r"(result), "=m"(*at)
                             : "0"(x)
                             : "memory");
#endif
        return result;
#elif defined(linux)
        // Linux, not inline for Intel (does this ever get used?)
        __gnu_cxx::__exchange_and_add (at, x);
#elif defined(__APPLE__)
        // Apple, not inline for Intel (only PPC?)
        return OSAtomicAdd32Barrier (x, &m_val) - x;
#elif defined(_WIN32)
        // Windows
        return InterlockedExchangeAdd (at, x);
#endif
    }
};



#undef USE_INTEL_ASM_ATOMICS


#endif // ATOMIC_H

