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
/// Wrappers and utilities for multithreading.
/////////////////////////////////////////////////////////////////////////


#ifndef THREAD_H
#define THREAD_H

#include <boost/thread.hpp>
#include <boost/version.hpp>

typedef boost::mutex mutex;
typedef boost::recursive_mutex recursive_mutex;

#if (BOOST_VERSION >= 103500)

typedef boost::lock_guard< boost::mutex > lock_guard;
typedef boost::lock_guard< boost::recursive_mutex > recursive_lock_guard;

#else

typedef boost::mutex::scoped_lock lock_guard;
typedef boost::recursive_mutex::scoped_lock recursive_lock_guard;

#endif


//
// Include files we need for atomic counters
//

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



/// Atomic integer.  Increment, decrement, add, and subtract in a
/// totally thread-safe manner.
class atomic_int {
public:
    /// Construct with initial value.
    ///
    atomic_int (int val=0) : m_val(val) { }

    ~atomic_int () { }

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




/// A fast_mutex is a spin lock.  It's semantically equivalent to a
/// regular mutex, except for the following:
///  - A fast_mutex is just 4 bytes, whereas a regular mutex is quite
///    large (44 bytes for pthread). is just 4 bytes.
///  - A fast_mutex is extremely fast to lock and unlock, whereas a regular
///    mutex is surprisingly expensive just to acquire a lock.
///  - A fast_mutex spins, taking CPU while it waits, so this can be very
///    wasteful compared to a regular mutex that blocks (gives up its CPU
///    slices until it acquires the lock).
///
/// The bottom line is that mutex is the usual choice, but in cases where
/// you need to acquire locks very frequently, but only need to hold the
/// lock for a very short period of time, you may save runtime by using
/// a FastMutex, even though it's non-blocking.
class fast_mutex {
public:
    /// Default constructor -- initialize to unlocked.
    ///
    fast_mutex (void) : m_locked(0) { }

    ~fast_mutex (void) { }

    /// Copy constructor -- initialize to unlocked.
    ///
    fast_mutex (const fast_mutex &) : m_locked(0) { }

    /// Assignment does not do anything, since lockedness should not
    /// transfer.
    const fast_mutex& operator= (const fast_mutex&) { return *this; }

    /// Acquire the lock, spin until we have it.
    ///
    void lock () {
        while (! try_lock())
            ;
    }

    /// Release the lock that we hold.
    ///
    void unlock () {
        --m_locked;
    }

    /// Try to acquire the lock.  Return true if we have it, false if
    /// somebody else is holding the lock.
    bool try_lock () {
        if (++m_locked == 1) {
            // We incremented it to 1, so we are the ones who hold the lock.
            return true;
        } else {
            // We incremented the atomic counter, but the new value wasn't
            // 1, so obviously we weren't the ones to incremented to 1 and
            // hold the lock.  So decrement it again and then return false.
            --m_locked;
            return false;
        }
    }

private:
    atomic_int m_locked;  ///< Atomic counter is zero if nobody holds the lock
};



#endif // THREAD_H

