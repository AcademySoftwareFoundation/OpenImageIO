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

#if (BOOST_VERSION == 103500)
#include <boost/thread/shared_mutex.hpp>
#endif

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


#ifndef USE_TBB
#  if defined(_WIN32) && (_MSV_VER >= 1500)
#    define USE_TBB 0
#  else
#    define USE_TBB 1
#  endif
#endif

#if USE_TBB

#include <tbb/atomic.h>
using tbb::atomic;

#else /* the following defined only if not USE_TBB */

//
// Include files we need for atomic counters.
// Some day, we hope this is all replaced by use of std::atomic<>.
//

#  if defined(__GNUC__) && defined(_GLIBCXX_ATOMIC_BUILTINS)
     // we're good to go with GCC intrinsics
#  elif defined(__APPLE__)
#    include <libkern/OSAtomic.h>
#  elif defined(_WIN32)
#    include <windows.h>
#    include <winbase.h>
#  else
#    error "Ouch, no atomics!"
#  endif



/// Atomic version of:  r = *at, *at += x, return r
/// For each of several architectures.
inline int
atomic_exchange_and_add (volatile int *at, int x)
{
#if defined(__GNUC__) && defined(_GLIBCXX_ATOMIC_BUILTINS)
    return __sync_fetch_and_add ((int *)at, x);
#elif defined(__APPLE__)
    // Apple, not inline for Intel (only PPC?)
    return OSAtomicAdd32Barrier (x, at) - x;
#elif defined(_WIN32)
    // Windows
    return InterlockedExchangeAdd ((volatile LONG *)at, x);
#endif
}



inline long long
atomic_exchange_and_add (volatile long long *at, long long x)
{
#if defined(__GNUC__) && defined(_GLIBCXX_ATOMIC_BUILTINS)
    return __sync_fetch_and_add (at, x);
#elif defined(__APPLE__)
    // Apple, not inline for Intel (only PPC?)
    return OSAtomicAdd64Barrier (x, at) - x;
#elif defined(_WIN32)
    // Windows
    return InterlockedExchangeAdd64 ((volatile LONGLONG *)at, x);
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
#if defined(__GNUC__) && defined(_GLIBCXX_ATOMIC_BUILTINS)
    return __sync_bool_compare_and_swap (at, compareval, newval);
#elif defined(__APPLE__)
    return OSAtomicCompareAndSwap32Barrier (compareval, newval, at);
#elif defined(_WIN32)
    return (InterlockedCompareExchange ((volatile LONG *)at, newval, compareval) == compareval);
#endif
}



inline bool
atomic_compare_and_exchange (volatile long long *at, long long compareval, long long newval)
{
#if defined(__GNUC__) && defined(_GLIBCXX_ATOMIC_BUILTINS)
    return __sync_bool_compare_and_swap (at, compareval, newval);
#elif defined(__APPLE__)
    return OSAtomicCompareAndSwap64Barrier (compareval, newval, at);
#elif defined(_WIN32)
    return (InterlockedCompareExchange64 ((volatile LONGLONG *)at, newval, compareval) == compareval);
#endif
}



/// Atomic integer.  Increment, decrement, add, and subtract in a
/// totally thread-safe manner.
template<class T>
class atomic {
public:
    /// Construct with initial value.
    ///
    atomic (T val=0) : m_val(val) { }

    ~atomic () { }

    /// Retrieve value
    ///
    T operator() () const { return atomic_exchange_and_add (&m_val, 0); }

    /// Retrieve value
    ///
    operator T() const { return atomic_exchange_and_add (&m_val, 0); }

    /// Assign new value.
    ///
    T operator= (T x) {
        //incorrect? return (m_val = x);
        while (1) {
            T result = m_val;
            if (atomic_compare_and_exchange (&m_val, result, x))
                break;
        }
        return x;
    }

    /// Pre-increment:  ++foo
    ///
    T operator++ () { return atomic_exchange_and_add (&m_val, 1) + 1; }

    /// Post-increment:  foo++
    ///
    T operator++ (int) {  return atomic_exchange_and_add (&m_val, 1); }

    /// Pre-decrement:  --foo
    ///
    T operator-- () {  return atomic_exchange_and_add (&m_val, -1) - 1; }

    /// Post-decrement:  foo--
    ///
    T operator-- (int) {  return atomic_exchange_and_add (&m_val, -1); }

    /// Add to the value, return the new result
    ///
    T operator+= (T x) { return atomic_exchange_and_add (&m_val, x) + x; }

    /// Subtract from the value, return the new result
    ///
    T operator-= (T x) { return atomic_exchange_and_add (&m_val, -x) - x; }

    bool compare_and_swap (T compareval, T newval) {
        return atomic_compare_and_exchange (&m_val, compareval, newval);
    }

    T operator= (const atomic &x) {
        T r = x();
        *this = r;
        return r;
    }

private:
    volatile mutable T m_val;

    // Disallow copy construction by making private and unimplemented.
    atomic (atomic const &);
};


#endif /* USE_TBB */

typedef atomic<int> atomic_int;
typedef atomic<long long> atomic_ll;


#endif // THREAD_H
