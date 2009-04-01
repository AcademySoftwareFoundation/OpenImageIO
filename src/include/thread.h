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

typedef boost::shared_mutex shared_mutex;
typedef boost::lock_guard< boost::mutex > lock_guard;
typedef boost::lock_guard< boost::recursive_mutex > recursive_lock_guard;
typedef boost::shared_lock< boost::shared_mutex > shared_lock;
typedef boost::unique_lock< boost::shared_mutex > unique_lock;

#else

// Old Boost lacks reader-writer mutexes -- UGLY!!! Make stripped down
// versions of shared_mutex, shared_lock, and exclusive_lock.  I can't
// wait for the day when we get to remove these.  Note that this uses
// pthreads, so only works on Linux & OSX.  Windows will just have to
// use a more modern Boost.

typedef boost::mutex::scoped_lock lock_guard;
typedef boost::recursive_mutex::scoped_lock recursive_lock_guard;

#include <pthread.h>

class shared_mutex {
public:
    shared_mutex () { pthread_rwlock_init (&m_rwlock, NULL); }
    ~shared_mutex () { pthread_rwlock_destroy (&m_rwlock); }
    void lock () { pthread_rwlock_wrlock (&m_rwlock); }
    void unlock () { pthread_rwlock_unlock (&m_rwlock); }
    void lock_shared () { pthread_rwlock_rdlock (&m_rwlock); }
    void unlock_shared () { pthread_rwlock_unlock (&m_rwlock); }
private:
    pthread_rwlock_t m_rwlock;
};

class shared_lock {
public:
    shared_lock (shared_mutex &m) : m_mutex(m) { m_mutex.lock_shared (); }
    ~shared_lock () { m_mutex.unlock_shared (); }
private:
    shared_mutex &m_mutex;
};

class unique_lock {
public:
    unique_lock (shared_mutex &m) : m_mutex(m) { m_mutex.lock (); }
    ~unique_lock () { m_mutex.unlock (); }
private:
    shared_mutex &m_mutex;
};

#endif


//
// Include files we need for atomic counters.
// Some day, we hope this is all replaced by use of std::atomic<>.
//

#ifndef USE_INTEL_ASM_ATOMICS
#  if (defined(__i386__) || defined(__x86_64__))
#    define USE_INTEL_ASM_ATOMICS 1
#  else
#    define USE_INTEL_ASM_ATOMICS 0
#  endif
#endif

#if (USE_INTEL_ASM_ATOMIC == 0)
#  if defined(__linux__)
#    include <bits/atomicity.h>
#  elif defined(__APPLE__)
#    include <libkern/OSAtomic.h>
#endif

#elif defined(_WIN32)
#  include <windows.h>
#  include <winbase.h>
#endif



#if 0  /* unused */
/// Atomic version of:  r = *at, *at = x, return r
/// For each of several architectures.
inline int
atomic_exchange (volatile int *at, int x)
{
#if USE_INTEL_ASM_ATOMICS
    // Common case of i386 or x86_64 on either Linux or Mac.
    // Note slightly different instruction for 32 vs 64 bit.
    int result;
    __asm__ __volatile__(
#ifdef __i386__
                         "lock\nxchgl %0,%1"
#else
                         "lock\nxchg %0,%1"
#endif
                         : "=r"(result), "=m"(*at)
                         : "0"(x)
                         : "memory");
    return result;
#elif defined(_WIN32)
    // Windows
    return InterlockedExchange (at, x);
#else
    asfaef  // force compile to fail, I have no idea what to do here
#endif
}
#endif



/// Atomic version of:  r = *at, *at += x, return r
/// For each of several architectures.
inline int
atomic_exchange_and_add (volatile int *at, int x)
{
#if USE_INTEL_ASM_ATOMICS
    // Common case of i386 or x86_64 on either Linux or Mac.
    // Note slightly different instruction for 32 vs 64 bit.
    int result;
    __asm__ __volatile__(
#ifdef __i386__
                         "lock\nxaddl %0,%1"
#else
                         "lock\nxadd %0,%1"
#endif
                         : "=r"(result), "=m"(*at)
                         : "0"(x)
                         : "memory");
    return result;
#elif defined(linux)
    // Linux, not inline for Intel (does this ever get used?)
    __gnu_cxx::__exchange_and_add (at, x);
#elif defined(__APPLE__)
    // Apple, not inline for Intel (only PPC?)
    return OSAtomicAdd32Barrier (x, at) - x;
#elif defined(_WIN32)
    // Windows
    return InterlockedExchangeAdd (at, x);
#endif
}



/// Atomic version of: 
///    if (*at == compareval) {
///        *at = newval;  return true;
///    } else {
///        return false;
///
inline bool
atomic_compare_and_exchange (volatile int *at, int compareval, int newval)
{
#if USE_INTEL_ASM_ATOMICS
    // Common case of i386 or x86_64 on either Linux or Mac.
    // Note slightly different instruction for 32 vs 64 bit.
    int result;
    __asm__ __volatile__(
#ifdef __i386__
                         "lock\ncmpxchgl %2,%1"
#else
                         "lock\ncmpxchg %2,%1"
#endif
                         : "=a"(result), "=m"(*at)
                         : "q"(newval), "0"(compareval)
                         : "memory");
    return result;
#elif defined(linux)
    // Linux, not inline for Intel (does this ever get used?)
//    __gnu_cxx::__exchange_and_add (at, x);
#elif defined(__APPLE__)
    // Apple, not inline for Intel (only PPC?)
    return OSAtomicCompareAndSwap32Barrier (compareval, newval, at);
#elif defined(_WIN32)
    // Windows
    return (InterlockedCompareExchange (at, newval, compareval) == compareval);
#endif
}



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
    int operator() () const { return atomic_exchange_and_add (&m_val, 0); }

    /// Retrieve value
    ///
    operator int() const { return atomic_exchange_and_add (&m_val, 0); }

    /// Assign new value.
    ///
    int operator= (int x) {
        //better? (void)atomic_exchange (&m_val, x); return x;
        return (m_val = x);
    }

    /// Pre-increment:  ++foo
    ///
    int operator++ () { return atomic_exchange_and_add (&m_val, 1) + 1; }

    /// Post-increment:  foo++
    ///
    int operator++ (int) {  return atomic_exchange_and_add (&m_val, 1); }

    /// Pre-decrement:  --foo
    ///
    int operator-- () {  return atomic_exchange_and_add (&m_val, -1) - 1; }

    /// Post-decrement:  foo--
    ///
    int operator-- (int) {  return atomic_exchange_and_add (&m_val, -1); }

    /// Add to the value, return the new result
    ///
    int operator+= (int x) { return atomic_exchange_and_add (&m_val, x) + x; }

    /// Subtract from the value, return the new result
    ///
    int operator-= (int x) { return atomic_exchange_and_add (&m_val, -x) - x; }

    bool compare_and_exchange (int compareval, int newval) {
        return atomic_compare_and_exchange (&m_val, compareval, newval);
    }

private:
    volatile mutable int m_val;

    // Disallow assignment and copy construction by making private and
    // unimplemented.
    atomic_int (atomic_int const &);
    atomic_int & operator= (atomic_int const &);
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
#if defined(__APPLE__)
        // OS X has dedicated spin lock routines, may as well use them.
        OSSpinLockLock ((OSSpinLock *)&m_locked);
#else
        while (! try_lock())
            ;
#endif
    }

    /// Release the lock that we hold.
    ///
    void unlock () {
#if defined(__APPLE__)
        OSSpinLockUnlock ((OSSpinLock *)&m_locked);
#else
        --m_locked;
#endif
    }

    /// Try to acquire the lock.  Return true if we have it, false if
    /// somebody else is holding the lock.
    bool try_lock () {
#if defined(__APPLE__)
        return OSSpinLockTry ((OSSpinLock *)&m_locked);
#else
        return m_locked.compare_and_exchange (0, 1);
#endif
    }

    /// Helper class: scoped lock for a fast_mutex -- grabs the lock upon
    /// construction, releases the lock when it exits scope.
    class lock_guard {
    public:
        lock_guard (fast_mutex &fm) : m_fm(fm) { m_fm.lock(); }
        ~lock_guard () { m_fm.unlock(); }
    private:
        fast_mutex & m_fm;
    };

private:
    atomic_int m_locked;  ///< Atomic counter is zero if nobody holds the lock
};



#endif // THREAD_H

