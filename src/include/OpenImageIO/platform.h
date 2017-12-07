/*
  Copyright 2014 Larry Gritz and the other authors and contributors.
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
/// @file  platform.h
///
/// @brief Platform-related macros.
/////////////////////////////////////////////////////////////////////////


#pragma once

#include <utility> // std::forward

// Make sure all platforms have the explicit sized integer types
#if defined(_MSC_VER) && _MSC_VER < 1600
   typedef __int8  int8_t;
   typedef __int16 int16_t;
   typedef __int32 int32_t;
   typedef __int64 int64_t;
   typedef unsigned __int8  uint8_t;
   typedef unsigned __int16 uint16_t;
# ifndef _UINT64_T
   typedef unsigned __int32 uint32_t;
   typedef unsigned __int64 uint64_t;
#  define _UINT32_T
#  define _UINT64_T
# endif
#else
#  ifndef __STDC_LIMIT_MACROS
#    define __STDC_LIMIT_MACROS  /* needed for some defs in stdint.h */
#  endif
#  include <cstdint>
#endif

#if defined(__FreeBSD__)
#include <sys/param.h>
#endif

#ifdef __MINGW32__
#include <malloc.h> // for alloca
#endif

#if defined(_MSC_VER) || defined(_WIN32)
# ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
# endif
# ifndef VC_EXTRALEAN
#   define VC_EXTRALEAN
# endif
# ifndef NOMINMAX
#   define NOMINMAX
# endif
# include <windows.h>
#endif

# ifdef _MSC_VER
#include <intrin.h>
#endif

#include <OpenImageIO/oiioversion.h>

// Detect which C++ standard we're using, and handy macros.
//
// OIIO_CPLUSPLUS_VERSION : which C++ standard is compiling (3, 11, 14, ...)
// OIIO_USING_CPP11 : (deprecated) defined and 1 if using C++11 or newer.
// OIIO_CONSTEXPR14 : constexpr for C++ >= 14, otherwise nothing (this is
//                      useful for things that can only be constexpr for 14)
//
// Note: oiioversion.h defines OIIO_BUILD_CPP11 or OIIO_BUILD_CPP14 to be 1
// if OIIO itself was built using C++11 or C++14, respectively. In contrast,
// OIIO_CPLUSPLUS_VERSION defined below will be set to the right number for
// the C++ standard being compiled RIGHT NOW. These two things may be the
// same when compiling OIIO, but they may not be the same if another
// packages is compiling against OIIO and using these headers (OIIO may be
// C++11 but the client package may be older, or vice versa -- use these two
// symbols to differentiate these cases, when important).
#if (__cplusplus >= 201703L)
#  define OIIO_CPLUSPLUS_VERSION  17
#  define OIIO_CONSTEXPR14        constexpr
#elif (__cplusplus >= 201402L)
#  define OIIO_CPLUSPLUS_VERSION  14
#  define OIIO_CONSTEXPR14        constexpr
#elif (__cplusplus >= 201103L) || _MSC_VER >= 1900
#  define OIIO_CPLUSPLUS_VERSION  11
#  define OIIO_CONSTEXPR14        /* not constexpr before C++14 */
#else
#  error "This version of OIIO is meant to work only with C++11 and above"
#endif

// DEPRECATED(1.8): use C++11 constexpr
#define OIIO_CONSTEXPR          constexpr
#define OIIO_CONSTEXPR_OR_CONST constexpr

// DEPRECATED(1.8): use C++11 noexcept
#define OIIO_NOEXCEPT noexcept


// Fallback definitions for feature testing. Some newer compilers define
// these for real, and it may be standard for C++17.
#ifndef __has_feature
#  define __has_feature(x) 0
#endif
#ifndef __has_extension
#  define __has_extension(x) __has_feature(x)
#endif
#ifndef __has_attribute
#  define __has_attribute(x) 0
#endif
#ifndef __has_builtin
#  define __has_builtin(x) 0
#endif
#ifndef __has_include
#  define __has_include(x) 0
#endif



// Detect which compiler and version we're using

// Define OIIO_GNUC_VERSION to hold an encoded gcc version (e.g. 40802 for
// 4.8.2), or 0 if not a GCC release. N.B.: This will be 0 for clang.
#if defined(__GNUC__) && !defined(__clang__)
#  define OIIO_GNUC_VERSION (10000*__GNUC__ + 100*__GNUC_MINOR__ + __GNUC_PATCHLEVEL__)
#else
#  define OIIO_GNUC_VERSION 0
#endif

// Define OIIO_CLANG_VERSION to hold an encoded generic Clang version (e.g.
// 30402 for clang 3.4.2), or 0 if not a generic Clang release.
// N.B. This will be 0 for the clang Apple distributes (which has different
// version numbers entirely).
#if defined(__clang__) && !defined(__apple_build_version__)
#  define OIIO_CLANG_VERSION (10000*__clang_major__ + 100*__clang_minor__ + __clang_patchlevel__)
#else
#  define OIIO_CLANG_VERSION 0
#endif

// Define OIIO_APPLE_CLANG_VERSION to hold an encoded Apple Clang version
// (e.g. 70002 for clang 7.0.2), or 0 if not an Apple Clang release.
#if defined(__clang__) && defined(__apple_build_version__)
#  define OIIO_APPLE_CLANG_VERSION (10000*__clang_major__ + 100*__clang_minor__ + __clang_patchlevel__)
#else
#  define OIIO_APPLE_CLANG_VERSION 0
#endif

// Tests for MSVS versions, always 0 if not MSVS at all.
#if defined(_MSC_VER)
#  define OIIO_MSVS_AT_LEAST_2013 (_MSC_VER >= 1800)
#  define OIIO_MSVS_BEFORE_2013   (_MSC_VER <  1800)
#  define OIIO_MSVS_AT_LEAST_2015 (_MSC_VER >= 1900)
#  define OIIO_MSVS_BEFORE_2015   (_MSC_VER <  1900)
#else
#  define OIIO_MSVS_AT_LEAST_2013 0
#  define OIIO_MSVS_BEFORE_2013   0
#  define OIIO_MSVS_AT_LEAST_2015 0
#  define OIIO_MSVS_BEFORE_2015   0
#endif


/// allocates memory, equivalent of C99 type var_name[size]
#define OIIO_ALLOCA(type, size) ((size) != 0 ? ((type*)alloca((size) * sizeof (type))) : nullptr)

/// Deprecated (for namespace pollution reasons)
#define ALLOCA(type, size) ((size) != 0 ? ((type*)alloca((size) * sizeof (type))) : nullptr)


// Define a macro that can be used for memory alignment.
// I think that in a future world of C++1x compatibility, all these can
// be replaced with [[ align(size) ]].
#if defined (__GNUC__) || __has_attribute(aligned)
#  define OIIO_ALIGN(size) __attribute__((aligned(size)))
#elif defined (_MSC_VER)
#  define OIIO_ALIGN(size) __declspec(align(size))
#elif defined (__INTEL_COMPILER)
#  define OIIO_ALIGN(size) __declspec(align((size)))
#else
#  error "Don't know how to define OIIO_ALIGN"
#endif

// Cache line size is 64 on all modern x86 CPUs. If this changes or we
// anticipate ports to other architectures, we'll need to change this.
#define OIIO_CACHE_LINE_SIZE 64

// Align the next declaration to be on its own cache line
#define OIIO_CACHE_ALIGN OIIO_ALIGN(OIIO_CACHE_LINE_SIZE)



// gcc defines a special intrinsic to use in conditionals that can speed
// up extremely performance-critical spots if the conditional usually
// (or rarely) is true.  You use it by replacing
//     if (x) ...
// with
//     if (OIIO_LIKELY(x)) ...     // if you think x will usually be true
// or
//     if (OIIO_UNLIKELY(x)) ...   // if you think x will rarely be true
// Caveat: Programmers are notoriously bad at guessing this, so it
// should be used only with thorough benchmarking.
#if defined(__GNUC__) || defined(__clang__) || defined(__INTEL_COMPILER)
#define OIIO_LIKELY(x)   (__builtin_expect(bool(x), true))
#define OIIO_UNLIKELY(x) (__builtin_expect(bool(x), false))
#else
#define OIIO_LIKELY(x)   (x)
#define OIIO_UNLIKELY(x) (x)
#endif


// OIIO_FORCELINE is a function attribute that attempts to make the function
// always inline. On many compilers regular 'inline' is only advisory. Put
// this attribute before the function return type, just like you would use
// 'inline'.
#if defined(__GNUC__) || defined(__clang__) || __has_attribute(always_inline)
#  define OIIO_FORCEINLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER) || defined(__INTEL_COMPILER)
#  define OIIO_FORCEINLINE __forceinline
#else
#  define OIIO_FORCEINLINE inline
#endif

// OIIO_PURE_FUNC is a function attribute that assures the compiler that the
// function does not write to any non-local memory other than its return
// value and has no side effects. This can ebable additional compiler
// optimizations by knowing that calling the function cannot possibly alter
// any other memory. This declaration goes after the function declaration:
//   int blah (int arg) OIIO_PURE_FUNC;
#if defined(__GNUC__) || defined(__clang__) || defined(__INTEL_COMPILER) || __has_attribute(pure)
#  define OIIO_PURE_FUNC __attribute__((pure))
#elif defined(_MSC_VER)
#  define OIIO_PURE_FUNC  /* seems not supported by MSVS */
#else
#  define OIIO_PURE_FUNC
#endif

// OIIO_CONST_FUNC is a function attribute that assures the compiler that
// the function does not examine (read) any values except for its arguments,
// does not write any non-local memory other than its return value, and has
// no side effects. This is even more strict than 'pure', and allows even
// more optimizations (such as eliminating multiple calls to the function
// that have the exact same argument values).
#if defined(__GNUC__) || defined(__clang__) || defined(__INTEL_COMPILER) || __has_attribute(const)
#  define OIIO_CONST_FUNC __attribute__((const))
#elif defined(_MSC_VER)
#  define OIIO_CONST_FUNC  /* seems not supported by MSVS */
#else
#  define OIIO_CONST_FUNC
#endif

// OIIO_UNUSED_OK is a function or variable attribute that assures tells the
// compiler that it's fine for the item to appear to be unused.
#if defined(__GNUC__) || defined(__clang__) || defined(__INTEL_COMPILER) || __has_attribute(unused)
#  define OIIO_UNUSED_OK __attribute__((unused))
#else
#  define OIIO_UNUSED_OK
#endif

// OIIO_RESTRICT is a parameter attribute that indicates a promise that the
// parameter definitely will not alias any other parameters in such a way
// that creates a data dependency. Use with caution!
#if defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER) || defined(__INTEL_COMPILER)
#  define OIIO_RESTRICT __restrict
#else
#  define OIIO_RESTRICT 
#endif


#if OIIO_CPLUSPLUS_VERSION >= 14 && __has_attribute(deprecated)
#  define OIIO_DEPRECATED(msg) [[deprecated(msg)]]
#elif OIIO_GNUC_VERSION >= 40600 || defined(__clang__)
#  define OIIO_DEPRECATED(msg) __attribute__((deprecated(msg)))
#elif defined(__GNUC__) /* older gcc -- only the one with no message */
#  define OIIO_DEPRECATED(msg) __attribute__((deprecated))
#elif defined(_MSC_VER)
#  define OIIO_DEPRECATED(msg) __declspec(deprecated(msg))
#else
#  define OIIO_DEPRECATED(msg)
#endif


// OIIO_NO_SANITIZE_ADDRESS can be used to mark a function that you don't
// want address sanitizer to catch. Only use this if you know there are
// false positives that you can't easily get rid of.
// This should work for any clang >= 3.3 and gcc >= 4.8, which are
// guaranteed by our minimum requirements.
#if defined(__clang__) || defined (__GNUC__)
#  define OIIO_NO_SANITIZE_ADDRESS __attribute__((no_sanitize_address))
#else
#  define OIIO_NO_SANITIZE_ADDRESS
#endif


// Try to deduce endianness
#if (defined(_WIN32) || defined(__i386__) || defined(__x86_64__))
#  ifndef __LITTLE_ENDIAN__
#    define __LITTLE_ENDIAN__ 1
#    undef __BIG_ENDIAN__
#  endif
#endif


OIIO_NAMESPACE_BEGIN

/// Return true if the architecture we are running on is little endian
OIIO_FORCEINLINE bool littleendian (void)
{
#if defined(__BIG_ENDIAN__)
    return false;
#elif defined(__LITTLE_ENDIAN__)
    return true;
#else
    // Otherwise, do something quick to compute it
    int i = 1;
    return *((char *) &i);
#endif
}


/// Return true if the architecture we are running on is big endian
OIIO_FORCEINLINE bool bigendian (void)
{
    return ! littleendian();
}



/// Retrieve cpuid flags into 'info'.
inline void cpuid (int info[4], int infoType, int extra)
{
    // Implementation cribbed from Halide (http://halide-lang.org), which
    // cribbed it from ISPC (https://github.com/ispc/ispc).
#if (defined(_WIN32) || defined(__i386__) || defined(__x86_64__))
# ifdef _MSC_VER
    __cpuidex(info, infoType, extra);
# elif defined(__x86_64__)
    __asm__ __volatile__ (
        "cpuid                 \n\t"
        : "=a" (info[0]), "=b" (info[1]), "=c" (info[2]), "=d" (info[3])
        : "0" (infoType), "2" (extra));
# else
    __asm__ __volatile__ (
        "mov{l}\t{%%}ebx, %1  \n\t"
        "cpuid                 \n\t"
        "xchg{l}\t{%%}ebx, %1  \n\t"
        : "=a" (info[0]), "=r" (info[1]), "=c" (info[2]), "=d" (info[3])
        : "0" (infoType), "2" (extra));
# endif
#else
    info[0] = 0; info[1] = 0; info[2] = 0; info[3] = 0;
#endif
}


inline bool cpu_has_sse2  () {int i[4]; cpuid(i,1,0); return (i[3] & (1<<26)) != 0; }
inline bool cpu_has_sse3  () {int i[4]; cpuid(i,1,0); return (i[2] & (1<<0)) != 0; }
inline bool cpu_has_ssse3 () {int i[4]; cpuid(i,1,0); return (i[2] & (1<<9)) != 0; }
inline bool cpu_has_fma   () {int i[4]; cpuid(i,1,0); return (i[2] & (1<<12)) != 0; }
inline bool cpu_has_sse41 () {int i[4]; cpuid(i,1,0); return (i[2] & (1<<19)) != 0; }
inline bool cpu_has_sse42 () {int i[4]; cpuid(i,1,0); return (i[2] & (1<<20)) != 0; }
inline bool cpu_has_popcnt() {int i[4]; cpuid(i,1,0); return (i[2] & (1<<23)) != 0; }
inline bool cpu_has_avx   () {int i[4]; cpuid(i,1,0); return (i[2] & (1<<28)) != 0; }
inline bool cpu_has_f16c  () {int i[4]; cpuid(i,1,0); return (i[2] & (1<<29)) != 0; }
inline bool cpu_has_rdrand() {int i[4]; cpuid(i,1,0); return (i[2] & (1<<30)) != 0; }
inline bool cpu_has_avx2  () {int i[4]; cpuid(i,7,0); return (i[1] & (1<<5)) != 0; }
inline bool cpu_has_avx512f() {int i[4]; cpuid(i,7,0); return (i[1] & (1<<16)) != 0; }
inline bool cpu_has_avx512dq() {int i[4]; cpuid(i,7,0); return (i[1] & (1<<17)) != 0; }
inline bool cpu_has_avx512ifma() {int i[4]; cpuid(i,7,0); return (i[1] & (1<<21)) != 0; }
inline bool cpu_has_avx512pf() {int i[4]; cpuid(i,7,0); return (i[1] & (1<<26)) != 0; }
inline bool cpu_has_avx512er() {int i[4]; cpuid(i,7,0); return (i[1] & (1<<27)) != 0; }
inline bool cpu_has_avx512cd() {int i[4]; cpuid(i,7,0); return (i[1] & (1<<28)) != 0; }
inline bool cpu_has_avx512bw() {int i[4]; cpuid(i,7,0); return (i[1] & (1<<30)) != 0; }
inline bool cpu_has_avx512vl() {int i[4]; cpuid(i,7,0); return (i[1] & (0x80000000 /*1<<31*/)) != 0; }

// portable aligned malloc
void* aligned_malloc(std::size_t size, std::size_t align);
void  aligned_free(void* ptr);

// basic wrappers to new/delete over-aligned types since this isn't guaranteed to be supported until C++17
template <typename T, class... Args>
inline T* aligned_new(Args&&... args) {
    static_assert(alignof(T) > alignof(void*), "Type doesn't seem to be over-aligned, aligned_new is not required");
    void* ptr = aligned_malloc(sizeof(T), alignof(T));
    return ptr ? new (ptr) T(std::forward<Args>(args)...) : nullptr;
}

template <typename T>
inline void aligned_delete(T* t) {
    if (t) {
        t->~T();
        aligned_free(t);
    }
}


OIIO_NAMESPACE_END

