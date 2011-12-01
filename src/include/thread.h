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
/// @file   thread.h
///
/// @brief  Wrappers and utilities for multithreading.
/////////////////////////////////////////////////////////////////////////


#ifndef OPENIMAGEIO_THREAD_H
#define OPENIMAGEIO_THREAD_H

#include "version.h"

// defining NOMINMAX to prevent problems with std::min/std::max
// and std::numeric_limits<type>::min()/std::numeric_limits<type>::max()
// when boost include windows.h
#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN
# define VC_EXTRALEAN
# ifndef NOMINMAX
#   define NOMINMAX
# endif
#endif
 
#include <boost/version.hpp>
#if defined(__GNUC__) && (BOOST_VERSION == 104500)
// gcc reports errors inside some of the boost headers with boost 1.45
// See: https://svn.boost.org/trac/boost/ticket/4818
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

#include <boost/thread.hpp>
#include <boost/thread/tss.hpp>
#include <boost/version.hpp>
#if (BOOST_VERSION == 103500)
#  include <boost/thread/shared_mutex.hpp>
#endif

#if defined(__GNUC__) && (BOOST_VERSION == 104500)
// can't restore via push/pop in all versions of gcc (warning push/pop implemented for 4.6+ only)
#pragma GCC diagnostic error "-Wunused-variable"
#endif

#if (BOOST_VERSION < 103500)
#  include <pthread.h>
#endif

#ifndef USE_TBB
#  define USE_TBB 1
#endif

// Include files we need for atomic counters.
// Some day, we hope this is all replaced by use of std::atomic<>.
#if USE_TBB
#  include <tbb/atomic.h>
   using tbb::atomic;
#  include <tbb/spin_mutex.h>
#endif

#if defined(_WIN32) && !USE_TBB
#  include <windows.h>
#  include <winbase.h>
#  pragma intrinsic (_InterlockedExchangeAdd)
#  pragma intrinsic (_InterlockedCompareExchange)
#  pragma intrinsic (_InterlockedCompareExchange64)
#  if defined(_WIN64)
#    pragma intrinsic(_InterlockedExchangeAdd64)
#  endif
#endif

#ifdef __APPLE__
#  include <libkern/OSAtomic.h>
#endif

#if defined(__GNUC__) && (defined(_GLIBCXX_ATOMIC_BUILTINS) || (__GNUC__ * 100 + __GNUC_MINOR__ >= 401))
#if !defined(__FreeBSD__) || defined(__x86_64__)
#define USE_GCC_ATOMICS
#endif
#endif

OIIO_NAMESPACE_ENTER
{

/// Null mutex that can be substituted for a real one to test how much
/// overhead is associated with a particular mutex.
class null_mutex {
public:
    null_mutex () { }
    ~null_mutex () { }
    void lock () { }
    void unlock () { }
    void lock_shared () { }
    void unlock_shared () { }
};

/// Null lock that can be substituted for a real one to test how much
/// overhead is associated with a particular lock.
template<typename T>
class null_lock {
public:
    null_lock (T &m) { }
};


// Null thread-specific ptr that just wraps a single ordinary pointer
//
template<typename T>
class null_thread_specific_ptr {
public:
    typedef void (*destructor_t)(T *);
    null_thread_specific_ptr (destructor_t dest=NULL)
        : m_ptr(NULL), m_dest(dest) { }
    ~null_thread_specific_ptr () { reset (NULL); }
    T * get () { return m_ptr; }
    void reset (T *newptr=NULL) {
        if (m_ptr) {
            if (m_dest)
                (*m_dest) (m_ptr);
            else
                delete m_ptr;
        }
        m_ptr = newptr;
    }
private:
    T *m_ptr;
    destructor_t m_dest;
};


#ifdef NOTHREADS

// Definitions that we use for debugging to turn off all mutexes, locks,
// and atomics in order to test the performance hit of our thread safety.

// Null thread-specific ptr that just wraps a single ordinary pointer
//
template<typename T>
class thread_specific_ptr {
public:
    typedef void (*destructor_t)(T *);
    thread_specific_ptr (destructor_t dest=NULL)
        : m_ptr(NULL), m_dest(dest) { }
    ~thread_specific_ptr () { reset (NULL); }
    T * get () { return m_ptr; }
    void reset (T *newptr=NULL) {
        if (m_ptr) {
            if (m_dest)
                (*m_dest) (m_ptr);
            else
                delete m_ptr;
        }
        m_ptr = newptr;
    }
private:
    T *m_ptr;
    destructor_t m_dest;
};


typedef null_mutex mutex;
typedef null_mutex recursive_mutex;
typedef null_mutex shared_mutex;
typedef null_lock<mutex> lock_guard;
typedef null_lock<recursive_mutex> recursive_lock_guard;
typedef null_lock<shared_mutex> shared_lock;
typedef null_lock<shared_mutex> unique_lock;


#elif (BOOST_VERSION >= 103500)

// Fairly modern Boost has all the mutex and lock types we need.

typedef boost::mutex mutex;
typedef boost::recursive_mutex recursive_mutex;
typedef boost::shared_mutex shared_mutex;
typedef boost::lock_guard< boost::mutex > lock_guard;
typedef boost::lock_guard< boost::recursive_mutex > recursive_lock_guard;
typedef boost::shared_lock< boost::shared_mutex > shared_lock;
typedef boost::unique_lock< boost::shared_mutex > unique_lock;
using boost::thread_specific_ptr;

#else

// Old Boost lacks reader-writer mutexes -- UGLY!!! Make stripped down
// versions of shared_mutex, shared_lock, and exclusive_lock.  I can't
// wait for the day when we get to remove these.  Note that this uses
// pthreads, so only works on Linux & OSX.  Windows will just have to
// use a more modern Boost.

typedef boost::mutex mutex;
typedef boost::recursive_mutex recursive_mutex;
typedef boost::mutex::scoped_lock lock_guard;
typedef boost::recursive_mutex::scoped_lock recursive_lock_guard;
using boost::thread_specific_ptr;


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



/// Atomic version of:  r = *at, *at += x, return r
/// For each of several architectures.
inline int
atomic_exchange_and_add (volatile int *at, int x)
{
#ifdef USE_GCC_ATOMICS
    return __sync_fetch_and_add ((int *)at, x);
#elif USE_TBB
    atomic<int> *a = (atomic<int> *)at;
    return a->fetch_and_add (x);
#elif defined(__APPLE__)
    // Apple, not inline for Intel (only PPC?)
    return OSAtomicAdd32Barrier (x, at) - x;
#elif defined(_WIN32)
    // Windows
    return _InterlockedExchangeAdd ((volatile LONG *)at, x);
#else
#   error No atomics on this platform.
#endif
}



inline long long
atomic_exchange_and_add (volatile long long *at, long long x)
{
#ifdef USE_GCC_ATOMICS
    return __sync_fetch_and_add (at, x);
#elif USE_TBB
    atomic<long long> *a = (atomic<long long> *)at;
    return a->fetch_and_add (x);
#elif defined(__APPLE__)
    // Apple, not inline for Intel (only PPC?)
    return OSAtomicAdd64Barrier (x, at) - x;
#elif defined(_WIN32)
    // Windows
#  if defined(_WIN64)
    return _InterlockedExchangeAdd64 ((volatile LONGLONG *)at, x);
#  else
    return InterlockedExchangeAdd64 ((volatile LONGLONG *)at, x);
#  endif
#else
#   error No atomics on this platform.
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
#ifdef USE_GCC_ATOMICS
    return __sync_bool_compare_and_swap (at, compareval, newval);
#elif USE_TBB
    atomic<int> *a = (atomic<int> *)at;
    return a->compare_and_swap (newval, compareval) == newval;
#elif defined(__APPLE__)
    return OSAtomicCompareAndSwap32Barrier (compareval, newval, at);
#elif defined(_WIN32)
    return (_InterlockedCompareExchange ((volatile LONG *)at, newval, compareval) == compareval);
#else
#   error No atomics on this platform.
#endif
}



inline bool
atomic_compare_and_exchange (volatile long long *at, long long compareval, long long newval)
{
#ifdef USE_GCC_ATOMICS
    return __sync_bool_compare_and_swap (at, compareval, newval);
#elif USE_TBB
    atomic<long long> *a = (atomic<long long> *)at;
    return a->compare_and_swap (newval, compareval) == newval;
#elif defined(__APPLE__)
    return OSAtomicCompareAndSwap64Barrier (compareval, newval, at);
#elif defined(_WIN32)
    return (_InterlockedCompareExchange64 ((volatile LONGLONG *)at, newval, compareval) == compareval);
#else
#   error No atomics on this platform.
#endif
}



#if (! USE_TBB)
// If we're not using TBB, we need to define our own atomic<>.


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

    bool bool_compare_and_swap (T compareval, T newval) {
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


#endif /* ! USE_TBB */


#ifdef NOTHREADS

typedef int atomic_int;
typedef long long atomic_ll;

#else

typedef atomic<int> atomic_int;
typedef atomic<long long> atomic_ll;

#endif



#ifdef NOTHREADS

typedef null_mutex spin_mutex;
typedef null_lock<spin_mutex> spin_lock;

#elif USE_TBB

// Use TBB's spin locks
typedef tbb::spin_mutex spin_mutex;
typedef tbb::spin_mutex::scoped_lock spin_lock;


#else

// Define our own spin locks.  Do we trust them?



/// A spin_mutex is semantically equivalent to a regular mutex, except
/// for the following:
///  - A spin_mutex is just 4 bytes, whereas a regular mutex is quite
///    large (44 bytes for pthread).
///  - A spin_mutex is extremely fast to lock and unlock, whereas a regular
///    mutex is surprisingly expensive just to acquire a lock.
///  - A spin_mutex takes CPU while it waits, so this can be very
///    wasteful compared to a regular mutex that blocks (gives up its
///    CPU slices until it acquires the lock).
///
/// The bottom line is that mutex is the usual choice, but in cases where
/// you need to acquire locks very frequently, but only need to hold the
/// lock for a very short period of time, you may save runtime by using
/// a spin_mutex, even though it's non-blocking.
///
/// N.B. A spin_mutex is only the size of an int.  To avoid "false
/// sharing", be careful not to put two spin_mutex objects on the same
/// cache line (within 128 bytes of each other), or the two mutexes may
/// effectively (and wastefully) lock against each other.
///
class spin_mutex {
public:
    /// Default constructor -- initialize to unlocked.
    ///
    spin_mutex (void) { m_locked = 0; }

    ~spin_mutex (void) { }

    /// Copy constructor -- initialize to unlocked.
    ///
    spin_mutex (const spin_mutex &) { m_locked = 0; }

    /// Assignment does not do anything, since lockedness should not
    /// transfer.
    const spin_mutex& operator= (const spin_mutex&) { return *this; }

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
        m_locked = 0;
#endif
    }

    /// Try to acquire the lock.  Return true if we have it, false if
    /// somebody else is holding the lock.
    bool try_lock () {
#if defined(__APPLE__)
        return OSSpinLockTry ((OSSpinLock *)&m_locked);
#else
#  if USE_TBB
        // TBB's compare_and_swap returns the original value
        return m_locked.compare_and_swap (0, 1) == 0;
#  else
        // Our compare_and_swap returns true if it swapped
        return m_locked.bool_compare_and_swap (0, 1);
#  endif
#endif
    }

    /// Helper class: scoped lock for a spin_mutex -- grabs the lock upon
    /// construction, releases the lock when it exits scope.
    class lock_guard {
    public:
        lock_guard (spin_mutex &fm) : m_fm(fm) { m_fm.lock(); }
        ~lock_guard () { m_fm.unlock(); }
    private:
        lock_guard(); // Do not implement (even though TBB does)
        lock_guard(const lock_guard& other); // Do not implement
        lock_guard& operator = (const lock_guard& other); // Do not implement
        spin_mutex & m_fm;
    };

private:
    atomic_int m_locked;  ///< Atomic counter is zero if nobody holds the lock
};


typedef spin_mutex::lock_guard spin_lock;

#endif



/// Spinning reader/writer mutex.  This is just like spin_mutex, except
/// that there are separate locking mechanisms for "writers" (exclusive
/// holders of the lock, presumably because they are modifying whatever
/// the lock is protecting) and "readers" (non-exclusive, non-modifying
/// tasks that may access the protectee simultaneously).
class spin_rw_mutex {
public:
    /// Default constructor -- initialize to unlocked.
    ///
    spin_rw_mutex (void) { m_readers = 0; }

    ~spin_rw_mutex (void) { }

    /// Copy constructor -- initialize to unlocked.
    ///
    spin_rw_mutex (const spin_rw_mutex &) { m_readers = 0; }

    /// Assignment does not do anything, since lockedness should not
    /// transfer.
    const spin_rw_mutex& operator= (const spin_rw_mutex&) { return *this; }

    /// Acquire the reader lock.
    ///
    void read_lock () {
        // Spin until there are no writers active
        m_locked.lock();
        // Register ourself as a reader
        ++m_readers;
        // Release the lock, to let other readers work
        m_locked.unlock();
    }

    /// Release the reader lock.
    ///
    void read_unlock () {
        --m_readers;  // it's atomic, no need to lock to release
    }

    /// Acquire the writer lock.
    ///
    void write_lock () {
        // Make sure no new readers (or writers) can start
        m_locked.lock();
        // Spin until the last reader is done, at which point we will be
        // the sole owners and nobody else (reader or writer) can acquire
        // the resource until we release it.
        while (m_readers > 0)
                ;
    }

    /// Release the writer lock.
    ///
    void write_unlock () {
        // Let other readers or writers get the lock
        m_locked.unlock ();
    }

    /// Helper class: scoped read lock for a spin_rw_mutex -- grabs the
    /// read lock upon construction, releases the lock when it exits scope.
    class read_lock_guard {
    public:
        read_lock_guard (spin_rw_mutex &fm) : m_fm(fm) { m_fm.read_lock(); }
        ~read_lock_guard () { m_fm.read_unlock(); }
    private:
        read_lock_guard(); // Do not implement
        read_lock_guard(const read_lock_guard& other); // Do not implement
        read_lock_guard& operator = (const read_lock_guard& other); // Do not implement
        spin_rw_mutex & m_fm;
    };

    /// Helper class: scoped write lock for a spin_rw_mutex -- grabs the
    /// read lock upon construction, releases the lock when it exits scope.
    class write_lock_guard {
    public:
        write_lock_guard (spin_rw_mutex &fm) : m_fm(fm) { m_fm.write_lock(); }
        ~write_lock_guard () { m_fm.write_unlock(); }
    private:
        write_lock_guard(); // Do not implement
        write_lock_guard(const write_lock_guard& other); // Do not implement
        write_lock_guard& operator = (const write_lock_guard& other); // Do not implement
        spin_rw_mutex & m_fm;
    };

private:
    spin_mutex m_locked;   // write lock
    atomic_int m_readers;  // number of readers
};


typedef spin_rw_mutex::read_lock_guard spin_rw_read_lock;
typedef spin_rw_mutex::write_lock_guard spin_rw_write_lock;


}
OIIO_NAMESPACE_EXIT

#endif // OPENIMAGEIO_THREAD_H
