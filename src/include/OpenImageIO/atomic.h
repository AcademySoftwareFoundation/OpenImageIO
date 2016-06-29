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
/// @file   atomic.h
///
/// @brief  Wrappers and utilities for atomics.
/////////////////////////////////////////////////////////////////////////


#ifndef OPENIMAGEIO_ATOMIC_H
#define OPENIMAGEIO_ATOMIC_H

#include "oiioversion.h"
#include "platform.h"


#if defined(_MSC_VER)
   // N.B. including platform.h also included <windows.h>
#  pragma intrinsic (_InterlockedExchangeAdd)
#  pragma intrinsic (_InterlockedCompareExchange)
#  pragma intrinsic (_InterlockedCompareExchange64)
#  if defined(_WIN64)
#    pragma intrinsic(_InterlockedExchangeAdd64)
#  endif
// InterlockedExchangeAdd64 & InterlockedExchange64 are not available for XP
#  if defined(_WIN32_WINNT) && _WIN32_WINNT <= 0x0501
inline long long
InterlockedExchangeAdd64 (volatile long long *Addend, long long Value)
{
    long long Old;
    do {
        Old = *Addend;
    } while (_InterlockedCompareExchange64(Addend, Old + Value, Old) != Old);
    return Old;
}

inline long long
InterlockedExchange64 (volatile long long *Target, long long Value)
{
    long long Old;
    do {
        Old = *Target;
    } while (_InterlockedCompareExchange64(Target, Value, Old) != Old);
    return Old;
}
#  endif
#endif

#if defined(__GNUC__) && (defined(_GLIBCXX_ATOMIC_BUILTINS) || (__GNUC__ * 100 + __GNUC_MINOR__ >= 401))
#  define USE_GCC_ATOMICS
#  if !defined(__clang__) && (__GNUC__ * 100 + __GNUC_MINOR__ >= 408)
#    define OIIO_USE_GCC_NEW_ATOMICS
#  endif
#endif


OIIO_NAMESPACE_BEGIN

#if OIIO_USE_STDATOMIC
using std::memory_order;
#else
enum memory_order {
#if defined(OIIO_USE_GCC_NEW_ATOMICS)
    memory_order_relaxed = __ATOMIC_RELAXED,
    memory_order_consume = __ATOMIC_CONSUME,
    memory_order_acquire = __ATOMIC_ACQUIRE,
    memory_order_release = __ATOMIC_RELEASE,
    memory_order_acq_rel = __ATOMIC_ACQ_REL,
    memory_order_seq_cst = __ATOMIC_SEQ_CST
#else
    memory_order_relaxed,
    memory_order_consume,
    memory_order_acquire,
    memory_order_release,
    memory_order_acq_rel,
    memory_order_seq_cst
#endif
};
#endif

/// Atomic version of:  r = *at, *at += x, return r
/// For each of several architectures.
inline int
atomic_exchange_and_add (volatile int *at, int x,
                         memory_order order = memory_order_seq_cst)
{
#ifdef NOTHREADS
    int r = *at;  *at += x;  return r;
#elif defined(OIIO_USE_GCC_NEW_ATOMICS)
    return __atomic_fetch_add (at, x, order);
#elif defined(USE_GCC_ATOMICS)
    return __sync_fetch_and_add ((int *)at, x);
#elif defined(_MSC_VER)
    // Windows
    return _InterlockedExchangeAdd ((volatile LONG *)at, x);
#else
#   error No atomics on this platform.
#endif
}



inline long long
atomic_exchange_and_add (volatile long long *at, long long x,
                         memory_order order = memory_order_seq_cst)
{
#ifdef NOTHREADS
    long long r = *at;  *at += x;  return r;
#elif defined(OIIO_USE_GCC_NEW_ATOMICS)
    return __atomic_fetch_add (at, x, order);
#elif defined(USE_GCC_ATOMICS)
    return __sync_fetch_and_add (at, x);
#elif defined(_MSC_VER)
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



/// Atomic version of:  r = *at, *at &= x, return r
/// For each of several architectures.
inline int
atomic_exchange_and_and (volatile int *at, int x,
                        memory_order order = memory_order_seq_cst)
{
#ifdef NOTHREADS
    int r = *at;  *at &= x;  return r;
#elif defined(OIIO_USE_GCC_NEW_ATOMICS)
    return __atomic_fetch_and (at, x, order);
#elif defined(USE_GCC_ATOMICS)
    return __sync_fetch_and_and ((int *)at, x);
#elif defined(_MSC_VER)
    // Windows
    return _InterlockedAnd ((volatile LONG *)at, x);
#else
#   error No atomics on this platform.
#endif
}



inline long long
atomic_exchange_and_and (volatile long long *at, long long x,
                        memory_order order = memory_order_seq_cst)
{
#ifdef NOTHREADS
    long long r = *at;  *at &= x;  return r;
#elif defined(OIIO_USE_GCC_NEW_ATOMICS)
    return __atomic_fetch_and (at, x, order);
#elif defined(USE_GCC_ATOMICS)
    return __sync_fetch_and_and (at, x);
#elif defined(_MSC_VER)
    // Windows
#  if defined(_WIN64)
    return _InterlockedAnd64 ((volatile LONGLONG *)at, x);
#  else
    return InterlockedAnd64 ((volatile LONGLONG *)at, x);
#  endif
#else
#   error No atomics on this platform.
#endif
}



/// Atomic version of:  r = *at, *at |= x, return r
/// For each of several architectures.
inline int
atomic_exchange_and_or (volatile int *at, int x,
                        memory_order order = memory_order_seq_cst)
{
#ifdef NOTHREADS
    int r = *at;  *at |= x;  return r;
#elif defined(OIIO_USE_GCC_NEW_ATOMICS)
    return __atomic_fetch_or (at, x, order);
#elif defined(USE_GCC_ATOMICS)
    return __sync_fetch_and_or ((int *)at, x);
#elif defined(_MSC_VER)
    // Windows
    return _InterlockedOr ((volatile LONG *)at, x);
#else
#   error No atomics on this platform.
#endif
}



inline long long
atomic_exchange_and_or (volatile long long *at, long long x,
                        memory_order order = memory_order_seq_cst)
{
#ifdef NOTHREADS
    long long r = *at;  *at |= x;  return r;
#elif defined(OIIO_USE_GCC_NEW_ATOMICS)
    return __atomic_fetch_or (at, x, order);
#elif defined(USE_GCC_ATOMICS)
    return __sync_fetch_and_or (at, x);
#elif defined(_MSC_VER)
    // Windows
#  if defined(_WIN64)
    return _InterlockedOr64 ((volatile LONGLONG *)at, x);
#  else
    return InterlockedOr64 ((volatile LONGLONG *)at, x);
#  endif
#else
#   error No atomics on this platform.
#endif
}



/// Atomic version of:  r = *at, *at ^= x, return r
/// For each of several architectures.
inline int
atomic_exchange_and_xor (volatile int *at, int x,
                         memory_order order = memory_order_seq_cst)
{
#ifdef NOTHREADS
    int r = *at;  *at ^= x;  return r;
#elif defined(OIIO_USE_GCC_NEW_ATOMICS)
    return __atomic_fetch_xor (at, x, order);
#elif defined(USE_GCC_ATOMICS)
    return __sync_fetch_and_xor ((int *)at, x);
#elif defined(_MSC_VER)
    // Windows
    return _InterlockedXor ((volatile LONG *)at, x);
#else
#   error No atomics on this platform.
#endif
}



inline long long
atomic_exchange_and_xor (volatile long long *at, long long x,
                         memory_order order = memory_order_seq_cst)
{
#ifdef NOTHREADS
    long long r = *at;  *at ^= x;  return r;
#elif defined(OIIO_USE_GCC_NEW_ATOMICS)
    return __atomic_fetch_xor (at, x, order);
#elif defined(USE_GCC_ATOMICS)
    return __sync_fetch_and_xor (at, x);
#elif defined(_MSC_VER)
    // Windows
#  if defined(_WIN64)
    return _InterlockedXor64 ((volatile LONGLONG *)at, x);
#  else
    return InterlockedXor64 ((volatile LONGLONG *)at, x);
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
///    }
inline bool
atomic_compare_and_exchange (volatile int *at, int compareval, int newval,
                             bool weak = false,
                             memory_order success = memory_order_seq_cst,
                             memory_order failure = memory_order_seq_cst)
{
#ifdef NOTHREADS
    if (*at == compareval) {
        *at = newval;  return true;
    } else {
        return false;
    }
#elif defined(OIIO_USE_GCC_NEW_ATOMICS)
    return __atomic_compare_exchange_n (at, &compareval, newval, weak,
                                        success, failure);
#elif defined(USE_GCC_ATOMICS)
    return __sync_bool_compare_and_swap (at, compareval, newval);
#elif defined(_MSC_VER)
    return (_InterlockedCompareExchange ((volatile LONG *)at, newval, compareval) == compareval);
#else
#   error No atomics on this platform.
#endif
}



inline bool
atomic_compare_and_exchange (volatile long long *at, long long compareval, long long newval,
                             bool weak = false,
                             memory_order success = memory_order_seq_cst,
                             memory_order failure = memory_order_seq_cst)
{
#ifdef NOTHREADS
    if (*at == compareval) {
        *at = newval;  return true;
    } else {
        return false;
    }
#elif defined(OIIO_USE_GCC_NEW_ATOMICS)
    return __atomic_compare_exchange_n (at, &compareval, newval, weak,
                                        success, failure);
#elif defined(USE_GCC_ATOMICS)
    return __sync_bool_compare_and_swap (at, compareval, newval);
#elif defined(_MSC_VER)
    return (_InterlockedCompareExchange64 ((volatile LONGLONG *)at, newval, compareval) == compareval);
#else
#   error No atomics on this platform.
#endif
}



/// Atomic version of:  r = *at, *at = x, return r
/// For each of several architectures.
inline int
atomic_exchange (volatile int *at, int x,
                 memory_order order = memory_order_seq_cst)
{
#ifdef NOTHREADS
    int r = *at;  *at = x;  return r;
#elif defined(OIIO_USE_GCC_NEW_ATOMICS)
    return __atomic_exchange_n (at, x, order);
#elif defined(USE_GCC_ATOMICS)
    // No __sync version of atomic exchange! Do it the hard way:
    while (1) {
        int old = *at;
        if (atomic_compare_and_exchange (at, old, x))
            return old;
    }
    return 0; // can never happen
#elif defined(_MSC_VER)
    // Windows
    return _InterlockedExchange ((volatile LONG *)at, x);
#else
#   error No atomics on this platform.
#endif
}



inline long long
atomic_exchange (volatile long long *at, long long x,
                 memory_order order = memory_order_seq_cst)
{
#ifdef NOTHREADS
    long long r = *at;  *at = x;  return r;
#elif defined(OIIO_USE_GCC_NEW_ATOMICS)
    return __atomic_exchange_n (at, x, order);
#elif defined(USE_GCC_ATOMICS)
    // No __sync version of atomic exchange! Do it the hard way:
    while (1) {
        long long old = *at;
        if (atomic_compare_and_exchange (at, old, x))
            return old;
    }
    return 0; // can never happen
#elif defined(_MSC_VER)
    // Windows
#  if defined(_WIN64)
    return _InterlockedExchange64 ((volatile LONGLONG *)at, x);
#  else
    return InterlockedExchange64 ((volatile LONGLONG *)at, x);
#  endif
#else
#   error No atomics on this platform.
#endif
}



/// Memory fence / synchronization barrier
OIIO_FORCEINLINE void
atomic_thread_fence (memory_order order = memory_order_seq_cst)
{
#ifdef NOTHREADS
    // nothing
#elif OIIO_USE_STDATOMIC
    std::__atomic_thread_fence (order);
#elif defined(OIIO_USE_GCC_NEW_ATOMICS)
    __atomic_thread_fence (order);
#elif defined(USE_GCC_ATOMICS)
    __sync_synchronize ();
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    __asm__ __volatile__ ("":::"memory");
#elif defined(_MSC_VER)
    MemoryBarrier ();
#else
#   error No atomics on this platform.
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
    T load (memory_order order = memory_order_seq_cst) const {
        return atomic_exchange_and_add (&m_val, 0, order);
    }

    /// Retrieve value
    ///
    T operator() () const { return load(); }

    /// Retrieve value
    ///
    operator T() const { return load(); }

    /// Fast retrieval of value, no interchange, don't care about memory
    /// fences. Use with extreme caution!
    T fast_value () const { return m_val; }

    /// Assign new value, atomically.
    void store (T x, memory_order order = memory_order_seq_cst) {
        atomic_exchange (&m_val, x, order);
    }

    /// Atomic exchange
    T exchange (T x, memory_order order = memory_order_seq_cst) {
        return atomic_exchange (&m_val, x, order);
    }

    /// Atomic fetch-and-add: add x and return the old value.
    T fetch_add (T x, memory_order order = memory_order_seq_cst) {
        return atomic_exchange_and_add (&m_val, x, order);
    }
    /// Atomic fetch-and-subtract: subtract x and return the old value.
    T fetch_sub (T x, memory_order order = memory_order_seq_cst) {
        return atomic_exchange_and_add (&m_val, -x, order);
    }
    /// Atomic fetch-and-and: bitwise and with x and return the old value.
    T fetch_and (T x, memory_order order = memory_order_seq_cst) {
        return atomic_exchange_and_and (&m_val, x, order);
    }
    /// Atomic fetch-and-or: bitwise or with x and return the old value.
    T fetch_or (T x, memory_order order = memory_order_seq_cst) {
        return atomic_exchange_and_or (&m_val, x, order);
    }
    /// Atomic fetch-and-xor: bitwise xor with x and return the old value.
    T fetch_xor (T x, memory_order order = memory_order_seq_cst) {
        return atomic_exchange_and_xor (&m_val, x, order);
    }

    /// Assign new value.
    ///
    T operator= (T x) { store(x); return x; }

    /// Pre-increment:  ++foo
    ///
    T operator++ () { return fetch_add(1) + 1; }

    /// Post-increment:  foo++
    ///
    T operator++ (int) {  return fetch_add(1); }

    /// Pre-decrement:  --foo
    ///
    T operator-- () {  return fetch_sub(1) - 1; }

    /// Post-decrement:  foo--
    ///
    T operator-- (int) {  return fetch_sub(1); }

    /// Add to the value, return the new result
    ///
    T operator+= (T x) { return fetch_add(x) + x; }

    /// Subtract from the value, return the new result
    ///
    T operator-= (T x) { return fetch_sub(x) - x; }

    /// Logical and, return the new result
    ///
    T operator&= (T x) { return fetch_and(x) & x; }

    /// Logical or, return the new result
    ///
    T operator|= (T x) { return fetch_or(x) | x; }

    /// Logical xor, return the new result
    ///
    T operator^= (T x) { return fetch_xor(x) ^ x; }

    bool bool_compare_and_swap (T compareval, T newval) {
        return atomic_compare_and_exchange (&m_val, compareval, newval);
    }

    T operator= (const atomic &x) {
        T r = x();
        *this = r;
        return r;
    }

private:
#ifdef __arm__
    OIIO_ALIGN(8)
#endif 
    volatile mutable T m_val;

    // Disallow copy construction by making private and unimplemented.
    atomic (atomic const &);
};


#ifdef NOTHREADS

typedef int atomic_int;
typedef long long atomic_ll;

#else

typedef atomic<int> atomic_int;
typedef atomic<long long> atomic_ll;

#endif

OIIO_NAMESPACE_END


#endif // OPENIMAGEIO_ATOMIC_H
