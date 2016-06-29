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

#include <vector>

#include "oiioversion.h"
#include "platform.h"
#include "atomic.h"



#if OIIO_CPLUSPLUS_VERSION >= 11
# include <thread>
# include <mutex>
# include <atomic>
# define not_yet_OIIO_USE_STDATOMIC 1
#else   /* prior to C++11... */
  // Use Boost mutexes & guards when C++11 is not available
# include <boost/version.hpp>
# if defined(__GNUC__) && (BOOST_VERSION == 104500)
   // gcc reports errors inside some of the boost headers with boost 1.45
   // See: https://svn.boost.org/trac/boost/ticket/4818
#  pragma GCC diagnostic ignored "-Wunused-variable"
# endif
# include <boost/thread.hpp>
# if defined(__GNUC__) && (BOOST_VERSION == 104500)
   // can't restore via push/pop in all versions of gcc (warning push/pop implemented for 4.6+ only)
#  pragma GCC diagnostic error "-Wunused-variable"
# endif
#endif




// OIIO_THREAD_ALLOW_DCLP, if set to 0, prevents us from using a dodgy
// "double checked lock pattern" (DCLP).  We are very careful to construct
// it safely and correctly, and these uses improve thread performance for
// us.  But it confuses Thread Sanitizer, so this switch allows you to turn
// it off. Also set to 0 if you don't believe that we are correct in
// allowing this construct on all platforms.
#ifndef OIIO_THREAD_ALLOW_DCLP
#define OIIO_THREAD_ALLOW_DCLP 1
#endif



// Some helpful links:
//
// Descriptions of the "new" gcc atomic intrinsics:
//    https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html
// Old gcc atomic intrinsics:
//    https://gcc.gnu.org/onlinedocs/gcc-4.4.2/gcc/Atomic-Builtins.html
// C++11 and beyond std::atomic:
//    http://en.cppreference.com/w/cpp/atomic



OIIO_NAMESPACE_BEGIN

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
    bool try_lock () { return true; }
};

/// Null lock that can be substituted for a real one to test how much
/// overhead is associated with a particular lock.
template<typename T>
class null_lock {
public:
    null_lock (T &m) { }
};



#ifdef NOTHREADS

// Definitions that we use for debugging to turn off all mutexes, locks,
// and atomics in order to test the performance hit of our thread safety.

typedef null_mutex mutex;
typedef null_mutex recursive_mutex;
typedef null_lock<mutex> lock_guard;
typedef null_lock<recursive_mutex> recursive_lock_guard;

#elif OIIO_CPLUSPLUS_VERSION >= 11

typedef std::mutex mutex;
typedef std::recursive_mutex recursive_mutex;
typedef std::lock_guard< mutex > lock_guard;
typedef std::lock_guard< recursive_mutex > recursive_lock_guard;
typedef std::thread thread;

#else

// Fairly modern Boost has all the mutex and lock types we need.

typedef boost::mutex mutex;
typedef boost::recursive_mutex recursive_mutex;
typedef boost::lock_guard< mutex > lock_guard;
typedef boost::lock_guard< recursive_mutex > recursive_lock_guard;
typedef boost::thread thread;

#endif





/// Yield the processor for the rest of the timeslice.
///
inline void
yield ()
{
#if defined(__GNUC__)
    sched_yield ();
#elif defined(_MSC_VER)
    SwitchToThread ();
#else
#   error No yield on this platform.
#endif
}



// Slight pause
inline void
pause (int delay)
{
#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    for (int i = 0; i < delay; ++i)
        __asm__ __volatile__("pause;");

#elif defined(__GNUC__) && (defined(__arm__) || defined(__s390__))
    for (int i = 0; i < delay; ++i)
        __asm__ __volatile__("NOP;");

#elif defined(_MSC_VER)
    for (int i = 0; i < delay; ++i) {
#if defined (_WIN64)
        YieldProcessor();
#else
        _asm  pause
#endif /* _WIN64 */
    }

#else
    // No pause on this platform, just punt
    for (int i = 0; i < delay; ++i) ;
#endif
}



// Helper class to deliver ever longer pauses until we yield our timeslice.
class atomic_backoff {
public:
    atomic_backoff () : m_count(1) { }

    void operator() () {
        if (m_count <= 16) {
            pause (m_count);
            m_count *= 2;
        } else {
            yield();
        }
    }

private:
    int m_count;
};






#ifdef NOTHREADS

typedef null_mutex spin_mutex;
typedef null_lock<spin_mutex> spin_lock;

#else

// Define our own spin locks.


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
        // To avoid spinning too tightly, we use the atomic_backoff to
        // provide increasingly longer pauses, and if the lock is under
        // lots of contention, eventually yield the timeslice.
        atomic_backoff backoff;

        // Try to get ownership of the lock. Though experimentation, we
        // found that OIIO_UNLIKELY makes this just a bit faster on 
        // gcc x86/x86_64 systems.
        while (! OIIO_UNLIKELY(try_lock())) {
#if OIIO_THREAD_ALLOW_DCLP
            // The full try_lock() involves a compare_and_swap, which
            // writes memory, and that will lock the bus.  But a normal
            // read of m_locked will let us spin until the value
            // changes, without locking the bus. So it's faster to
            // check in this manner until the mutex appears to be free.
            // HOWEVER... Thread Sanitizer things this is an instance of
            // an unsafe "double checked lock pattern" (DCLP) and flags it
            // as an error. I think it's a false negative, because the
            // outer loop is still an atomic check, the inner non-atomic
            // loop only serves to delay, and can't lead to a true data
            // race. But we provide this build-time switch to, at least,
            // give a way to use tsan for other checks.
            do {
                backoff();
            } while (m_locked);
#else
            backoff();
#endif
        }
    }

    /// Release the lock that we hold.
    ///
    void unlock () {
        // Fastest way to do it is with a store with "release" semantics
#if defined(OIIO_USE_GCC_NEW_ATOMICS)
        __atomic_clear (&m_locked, __ATOMIC_RELEASE);
#elif defined(USE_GCC_ATOMICS)
        __sync_lock_release (&m_locked);
        //   Equivalent, x86 specific code:
        //   __asm__ __volatile__("": : :"memory");
        //   m_locked = 0;
#elif defined(_MSC_VER)
        MemoryBarrier ();
        m_locked = 0;
#else
        // Otherwise, just assign zero to the atomic (but that's a full 
        // memory barrier).
        *(atomic_int *)&m_locked = 0;
#endif
    }

    /// Try to acquire the lock.  Return true if we have it, false if
    /// somebody else is holding the lock.
    bool try_lock () {
#if defined(OIIO_USE_GCC_NEW_ATOMICS)
        return __atomic_test_and_set (&m_locked, __ATOMIC_ACQUIRE) == 0;
#elif defined(USE_GCC_ATOMICS)
        // GCC gives us an intrinsic that is even better -- an atomic
        // exchange with "acquire" barrier semantics.
        return __sync_lock_test_and_set (&m_locked, 1) == 0;
#else
        // Our compare_and_swap returns true if it swapped
        return atomic_compare_and_exchange (&m_locked, 0, 1);
#endif
    }

    /// Helper class: scoped lock for a spin_mutex -- grabs the lock upon
    /// construction, releases the lock when it exits scope.
    class lock_guard {
    public:
        lock_guard (spin_mutex &fm) : m_fm(fm) { m_fm.lock(); }
        ~lock_guard () { m_fm.unlock(); }
    private:
        lock_guard(); // Do not implement
        lock_guard(const lock_guard& other); // Do not implement
        lock_guard& operator = (const lock_guard& other); // Do not implement
        spin_mutex & m_fm;
    };

private:
#if defined(OIIO_USE_GCC_NEW_ATOMICS)
    // Using the gcc >= 4.8 new atomics, we can easily do a single byte flag
    volatile char m_locked; ///< Atomic counter is zero if nobody holds the lock
#else
    // Otherwise, fall back on it being an int
    volatile int m_locked;  ///< Atomic counter is zero if nobody holds the lock
#endif
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
#if OIIO_THREAD_ALLOW_DCLP
        while (*(volatile int *)&m_readers > 0)
                ;
#else
        while (m_readers > 0)
                ;
#endif
    }

    /// Release the writer lock.
    ///
    void write_unlock () {
        // Let other readers or writers get the lock
        m_locked.unlock ();
    }

    /// Acquire an exclusive ("writer") lock.
    void lock () { write_lock(); }

    /// Release an exclusive ("writer") lock.
    void unlock () { write_unlock(); }

    /// Acquire a shared ("reader") lock.
    void lock_shared () { read_lock(); }

    /// Release a shared ("reader") lock.
    void unlock_shared () { read_unlock(); }

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
    OIIO_CACHE_ALIGN
    spin_mutex m_locked;   // write lock
    char pad1_[OIIO_CACHE_LINE_SIZE-sizeof(spin_mutex)];
    OIIO_CACHE_ALIGN
    atomic_int m_readers;  // number of readers
    char pad2_[OIIO_CACHE_LINE_SIZE-sizeof(atomic_int)];
};


typedef spin_rw_mutex::read_lock_guard spin_rw_read_lock;
typedef spin_rw_mutex::write_lock_guard spin_rw_write_lock;



/// Mutex pool. Sometimes, we have lots of objects that need to be
/// individually locked for thread safety, but two separate objects don't
/// need to lock against each other. If there are many more objects than
/// threads, it's wasteful for each object to contain its own mutex. So a
/// solution is to make a mutex_pool -- a collection of several mutexes.
/// Each object uses a hash to choose a consistent mutex for itself, but
/// which will be unlikely to be locked simultaneously by different object.
/// Semantically, it looks rather like an associative array of mutexes. We
/// also ensure that the mutexes are all on different cache lines, to ensure
/// that they don't exhibit false sharing. Try to choose Bins larger than
/// the expected number of threads that will be simultaneously locking
/// mutexes.
template<class Mutex, class Key, class Hash, size_t Bins=16>
class mutex_pool
{
public:
    mutex_pool () { }
    Mutex& operator[] (const Key &key) {
        return m_mutex[m_hash(key) % Bins].m;
    }
private:
    // Helper type -- force cache line alignment. This should make an array
    // of these also have padding so that each individual mutex is aligned
    // to its own cache line, thus eliminating any "false sharing."
    struct AlignedMutex {
        OIIO_CACHE_ALIGN Mutex m;
    };

    AlignedMutex m_mutex[Bins];
    Hash m_hash;
};



/// Simple thread group class. This is just as good as boost::thread_group,
/// for the limited functionality that we use.
class thread_group {
public:
    thread_group () {}
    ~thread_group () {
        for (size_t i = 0, e = m_threads.size(); i < e; ++i)
            delete m_threads[i];
    }
    void add_thread (thread *t) {
        if (t) {
            lock_guard lock (m_mutex);
            m_threads.push_back (t);
        }
    }
    template<typename FUNC>
    thread *create_thread (FUNC func) {
        lock_guard lock (m_mutex);
        thread *t = new thread (func);
        m_threads.push_back (t);
        return t;
    }
    void join_all () {
        lock_guard lock (m_mutex);
        for (size_t i = 0, e = m_threads.size(); i < e; ++i) {
            if (m_threads[i]->joinable())
                m_threads[i]->join();
        }
    }
    size_t size () {
        lock_guard lock (m_mutex);
        return m_threads.size();
    }
private:
    mutex m_mutex;
    std::vector<thread *> m_threads;
};


OIIO_NAMESPACE_END

#endif // OPENIMAGEIO_THREAD_H
