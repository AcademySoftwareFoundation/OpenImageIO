/*
Copyright (c) 2014 Larry Gritz et al.
All Rights Reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
* Neither the name of Sony Pictures Imageworks nor the names of its
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
*/

/// @file  simd.h
///
/// @brief Classes for SIMD processing.
///
/// Nice references for all the Intel intrinsics (SSE*, AVX*, etc.):
///   https://software.intel.com/sites/landingpage/IntrinsicsGuide/
/// It helped me a lot to peruse the source of these packages:
///   Syrah: https://github.com/boulos/syrah
///   Embree: https://github.com/embree


#pragma once

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/platform.h>
#include <OpenEXR/ImathVec.h>

#if (defined(__SSE2__) || (_MSC_VER >= 1300 && !_M_CEE_PURE)) && !defined(OIIO_NO_SSE)
#  include <xmmintrin.h>
#  include <emmintrin.h>
#  if defined(__SSE3__)
#    include <pmmintrin.h>
#    include <tmmintrin.h>
#  endif
#  if (defined(__SSE4_1__) || defined(__SSE4_2__))
#    include <smmintrin.h>
#  endif
#  if (defined(__SSE4_1__) || defined(__SSE4_2__))
#    define OIIO_SIMD_SSE 4
#  elif defined(__SSE3__)
#    define OIIO_SIMD_SSE 3
#  else
#    define OIIO_SIMD_SSE 2
#  endif
#  define OIIO_SIMD 1
#  define OIIO_SIMD_MASK_BYTE_WIDTH 4
#  define OIIO_SIMD_MAX_SIZE_BYTES 16
#  define OIIO_SIMD_ALIGN OIIO_ALIGN(16)
#  define OIIO_SIMD4_ALIGN OIIO_ALIGN(16)
#  define OIIO_SSE_ALIGN OIIO_ALIGN(16)
#endif

// FIXME Future: support AVX
#if 0 && defined(__AVX__) && !defined(OIIO_NO_AVX)
   // N.B. Any machine with AVX will also have SSE
#  include <immintrin.h>
#  define OIIO_SIMD_AVX 1
#  define OIIO_SIMD_MASK_BYTE_WIDTH 4
#  undef OIIO_SIMD_MAX_SIZE_BYTES
#  define OIIO_SIMD_MAX_SIZE_BYTES 32
#  undef OIIO_SIMD_ALIGN
#  define OIIO_SIMD_ALIGN OIIO_ALIGN(32)
#  define OIIO_AVX_ALIGN OIIO_ALIGN(32)
#endif

// FIXME Future: support ARM Neon
#if 0 && defined(__ARM_NEON__) && !defined(OIIO_NO_NEON)
#  define OIIO_SIMD 1
#  define OIIO_SIMD_NEON 1
#  define OIIO_SIMD_MAX_SIZE_BYTES 16UL
#  define OIIO_SIMD_MAX_SIZE_FLOATS 4UL
#  define OIIO_SIMD_MASK_BYTE_WIDTH 4
#  define OIIO_SIMD_MAX_SIZE_BYTES 16
#  define OIIO_SIMD_ALIGN OIIO_ALIGN(16)
#  define OIIO_SIMD4_ALIGN OIIO_ALIGN(16)
#  define OIIO_SSE_ALIGN OIIO_ALIGN(16)
#endif

#ifndef OIIO_SIMD
#  define OIIO_SIMD 0
#  define OIIO_SIMD_ALIGN
#  define OIIO_SIMD4_ALIGN
#  define OIIO_SIMD_MAX_SIZE_BYTES 1
#endif

#include "platform.h"
#include "oiioversion.h"


OIIO_NAMESPACE_ENTER {

namespace simd {

class int4;
class float4;
class mask4;


//
// Additional private intrinsic wrappers
//
#if defined(OIIO_SIMD_SSE)

// Shamelessly lifted from Syrah which lifted from Manta which lifted it
// from intel.com
OIIO_FORCEINLINE __m128i mm_mul_epi32 (__m128i a, __m128i b) {
#if OIIO_SIMD_SSE >= 4  /* SSE >= 4.1 */
    return _mm_mullo_epi32(a, b);
#else
    // Prior to SSE 4.1, there is no _mm_mullo_epi32 instruction, so we have
    // to fake it.
    __m128i t0;
    __m128i t1;
    t0 = _mm_mul_epu32 (a, b);
    t1 = _mm_mul_epu32 (_mm_shuffle_epi32 (a, 0xB1),
                        _mm_shuffle_epi32 (b, 0xB1));
    t0 = _mm_shuffle_epi32 (t0, 0xD8);
    t1 = _mm_shuffle_epi32 (t1, 0xD8);
    return _mm_unpacklo_epi32 (t0, t1);
#endif
}


// Shuffling. Use like this:  x = shuffle<3,2,1,0>(b)
template<int i0, int i1, int i2, int i3>
OIIO_FORCEINLINE const __m128i shuffle_sse (__m128i v) {
    return _mm_shuffle_epi32(v, _MM_SHUFFLE(i3, i2, i1, i0));
}

template<int i0, int i1, int i2, int i3>
OIIO_FORCEINLINE const __m128 shuffle_sse (__m128 a) {
    return _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(a), _MM_SHUFFLE(i3, i2, i1, i0)));
}

#endif



/// mask4: A mask 4-vector, whose elements act mostly like bools,
/// accelerated by SIMD instructions when available. This is what is
/// naturally produced by SIMD comparison operators on the float4 and int4
/// types.
class mask4 {
public:
    static const char* type_name() { return "mask4"; }
    typedef bool value_t;     ///< Underlying equivalent scalar value type
    enum { elements = 4 };    ///< Number of scalar elements
    enum { bits = 128 };      ///< Total number of bits
    /// simd_t is the native SIMD type used
#if defined(OIIO_SIMD_SSE)
    typedef __m128   simd_t;
#else
    typedef int[4]   simd_t;
#endif

    /// Default constructor (contents undefined)
    OIIO_FORCEINLINE mask4 () { }

    /// Destructor
    OIIO_FORCEINLINE ~mask4 () { }

    /// Construct from a single value (store it in all slots)
    OIIO_FORCEINLINE mask4 (bool a) { load(a); }

    /// Construct from 4 values
    OIIO_FORCEINLINE mask4 (bool a, bool b, bool c, bool d) { load(a,b,c,d); }

    /// Copy construct from another mask4
    OIIO_FORCEINLINE mask4 (const mask4 &other) {
#if defined(OIIO_SIMD_SSE)
        m_vec = other.m_vec;
#else
        m_val[0] = other.m_val[0];
        m_val[1] = other.m_val[1];
        m_val[2] = other.m_val[2];
        m_val[3] = other.m_val[3];
#endif
    }

    /// Construct from the underlying SIMD type
    OIIO_FORCEINLINE mask4 (const simd_t m) : m_vec(m) { }

    /// Return the raw SIMD type
    OIIO_FORCEINLINE operator simd_t () const { return m_vec; }
    OIIO_FORCEINLINE simd_t simd () const { return m_vec; }

    /// Set all components to false
    OIIO_FORCEINLINE void clear () {
#if defined(OIIO_SIMD_SSE)
        m_vec = _mm_setzero_ps();
#else
        *this = false;
#endif
    }

    /// Return a mask4 the is 'false' for all values
    static OIIO_FORCEINLINE const mask4 False () { return mask4(false); }

    /// Return a mask4 the is 'true' for all values
    static OIIO_FORCEINLINE const mask4 True () { return mask4(true); }

    /// Assign one value to all components
    OIIO_FORCEINLINE const mask4 & operator= (bool a) { load(a); return *this; }

    /// Assignment of another mask4
    OIIO_FORCEINLINE const mask4 & operator= (mask4 other) {
#if defined(OIIO_SIMD_SSE)
        m_vec = other.m_vec;
#else
        m_val[0] = other.m_val[0];
        m_val[1] = other.m_val[1];
        m_val[2] = other.m_val[2];
        m_val[3] = other.m_val[3];
#endif
        return *this;
    }

    /// Component access (get)
    OIIO_FORCEINLINE bool operator[] (int i) const {
        DASSERT(i >= 0 && i < 4);
#if defined(OIIO_SIMD_SSE)
        return (_mm_movemask_ps(m_vec) >> i) & 1;
#else
        return bool(m_val[i]);
#endif
    }

    /// Component access (set).
    /// NOTE: use with caution. The implementation sets the integer
    /// value, which may not have the same bit pattern as the bool returned
    /// by operator[]const.
    OIIO_FORCEINLINE int& operator[] (int i) {
        DASSERT(i >= 0 && i < 4);
        return m_val[i];
    }

    /// Helper: load a single value into all components.
    OIIO_FORCEINLINE void load (bool a) {
#if defined(OIIO_SIMD_SSE)
        m_vec = _mm_castsi128_ps(_mm_set1_epi32(a ? -1 : 0));
#else
        int val = a ? -1 : 0;
        m_val[0] = val;
        m_val[1] = val;
        m_val[2] = val;
        m_val[3] = val;
#endif
    }

    /// Helper: load separate values into each component.
    OIIO_FORCEINLINE void load (bool a, bool b, bool c, bool d) {
#if defined(OIIO_SIMD_SSE)
        // N.B. -- we need to reverse the order because of our convention
        // of storing a,b,c,d in the same order in memory.
        m_vec = _mm_castsi128_ps(_mm_set_epi32(d ? -1 : 0,
                                               c ? -1 : 0,
                                               b ? -1 : 0,
                                               a ? -1 : 0));
#else
        m_val[0] = a ? -1 : 0;
        m_val[1] = b ? -1 : 0;
        m_val[2] = c ? -1 : 0;
        m_val[3] = d ? -1 : 0;
#endif
    }

    /// Helper: store the values into memory as bools.
    OIIO_FORCEINLINE void store (bool *values) {
#if 0 && defined(OIIO_SIMD_SSE)
        // FIXME: is there a more efficient way to do this?
#else
        values[0] = m_val[0] ? true : false;
        values[1] = m_val[1] ? true : false;
        values[2] = m_val[2] ? true : false;
        values[3] = m_val[3] ? true : false;
#endif
    }

    /// Store the first n values into memory.
    OIIO_FORCEINLINE void store (bool *values, int n) {
        DASSERT (n >= 0 && n < 4);
        for (int i = 0; i < n; ++i)
            values[i] = m_val[i] ? true : false;
    }

    /// Logical "not", component-by-component
    friend OIIO_FORCEINLINE mask4 operator! (mask4 a) {
#if defined(OIIO_SIMD_SSE)
        return _mm_xor_ps (a, True());
#else
        return mask4 (a.m_val[0] ^ (-1), a.m_val[1] ^ (-1),
                      a.m_val[2] ^ (-1), a.m_val[3] ^ (-1));
#endif
    }

    /// Logical "and", component-by-component
    friend OIIO_FORCEINLINE mask4 operator& (mask4 a, mask4 b) {
#if defined(OIIO_SIMD_SSE)
        return _mm_and_ps (a.m_vec, b.m_vec);
#else
        return mask4 (a.m_val[0] & b.m_val[0],
                      a.m_val[1] & b.m_val[1],
                      a.m_val[2] & b.m_val[2],
                      a.m_val[3] & b.m_val[3]);
#endif
    }
    OIIO_FORCEINLINE const mask4& operator&= (mask4 b) {
        return *this = *this & b;
    }


    /// Logical "or" component-by-component
    friend OIIO_FORCEINLINE mask4 operator| (mask4 a, mask4 b) {
#if defined(OIIO_SIMD_SSE)
        return _mm_or_si128 (a.m_vec, b.m_vec);
#else
        return mask4 (a.m_val[0] | b.m_val[0],
                      a.m_val[1] | b.m_val[1],
                      a.m_val[2] | b.m_val[2],
                      a.m_val[3] | b.m_val[3]);
#endif
    }
    OIIO_FORCEINLINE const mask4& operator|= (mask4 a) {
        return *this = *this | a;
    }

    /// Equality comparison, component by component
    friend OIIO_FORCEINLINE const mask4 operator== (mask4 a, mask4 b) {
#if defined(OIIO_SIMD_SSE)
        return _mm_castsi128_ps (_mm_cmpeq_epi32 (a, b));
#else
        return mask4 (a[0] == b[0],
                      a[1] == b[1],
                      a[2] == b[2],
                      a[3] == b[3]);
#endif
    }

    /// Inequality comparison, component by component
    friend OIIO_FORCEINLINE const mask4 operator!= (mask4 a, mask4 b) {
#if defined(OIIO_SIMD_SSE)
        return _mm_xor_ps (a, b);
#else
        return mask4 (a[0] != b[0],
                      a[1] != b[1],
                      a[2] != b[2],
                      a[3] != b[3]);
#endif
    }

    /// Stream output
    friend inline std::ostream& operator<< (std::ostream& cout, mask4 a) {
        return cout << a[0] << ' ' << a[1] << ' ' << a[2] << ' ' << a[3];
    }

private:
    // The actual data representation
    union {
        simd_t m_vec;
        int m_val[4];
    };
};




/// Helper: shuffle/swizzle with constant (templated) indices.
/// Example: shuffle<1,1,2,2>(mask4(a,b,c,d)) returns (b,b,c,c)
template<int i0, int i1, int i2, int i3>
OIIO_FORCEINLINE const mask4 shuffle (mask4 a) {
#if defined(OIIO_SIMD_SSE)
    return shuffle_sse<i0,i1,i2,i3> (__m128(a));
#else
    return mask4(a[i0], a[i1], a[i2], a[i3]);
#endif
}

/// Helper: as rapid as possible extraction of one component, when the
/// index is fixed.
template<int i>
OIIO_FORCEINLINE const bool extract (mask4 v) {
    // No efficient way to do this in SSE?
    return v[i];
}

/// Logical "and" reduction, i.e., 'and' all components together, resulting
/// in a single bool.
OIIO_FORCEINLINE bool reduce_and (mask4 v) {
#if defined(OIIO_SIMD_SSE)
    return _mm_movemask_ps(v) == 0xf;
#else
    return v[0] & v[1] & v[2] & v[3];
#endif
}


/// Logical "or" reduction, i.e., 'or' all components together, resulting
/// in a single bool.
OIIO_FORCEINLINE bool reduce_or (mask4 v) {
#if defined(OIIO_SIMD_SSE)
    return _mm_movemask_ps(v) != 0;
#else
    return v[0] | v[1] | v[2] | v[3];
#endif
}


/// Are all components true?
OIIO_FORCEINLINE bool all  (mask4 v) { return reduce_and(v) == true; }

/// Are any components true?
OIIO_FORCEINLINE bool any  (mask4 v) { return reduce_or(v) == true; }

/// Are all components false:
OIIO_FORCEINLINE bool none (mask4 v) { return reduce_or(v) == false; }





/// Integer 4-vector, accelerated by SIMD instructions when available.
class int4 {
public:
    static const char* type_name() { return "int4"; }
    typedef int value_t;      ///< Underlying equivalent scalar value type
    enum { elements = 4 };    ///< Number of scalar elements
    enum { bits = 128 };      ///< Total number of bits
    /// simd_t is the native SIMD type used
#if defined(OIIO_SIMD_SSE)
    typedef __m128i simd_t;
#else
    typedef int[4]  simd_t;
#endif

    /// Default constructor (contents undefined)
    OIIO_FORCEINLINE int4 () { }

    /// Destructor
    OIIO_FORCEINLINE ~int4 () { }

    /// Construct from a single value (store it in all slots)
    OIIO_FORCEINLINE int4 (int a) { load(a); }

    /// Construct from 2 values -- (a,a,b,b)
    OIIO_FORCEINLINE int4 (int a, int b) { load(a,a,b,b); }

    /// Construct from 4 values
    OIIO_FORCEINLINE int4 (int a, int b, int c, int d) { load(a,b,c,d); }

    /// Construct from a pointer to 4 values
    OIIO_FORCEINLINE int4 (const int *vals) { load (vals); }

    /// Copy construct from another int4
    OIIO_FORCEINLINE int4 (const int4 & other) {
#if defined(OIIO_SIMD_SSE)
        m_vec = other.m_vec;
#else
        m_val[0] = other.m_val[0];
        m_val[1] = other.m_val[1];
        m_val[2] = other.m_val[2];
        m_val[3] = other.m_val[3];
#endif
    }

    /// Construct from the underlying SIMD type
    OIIO_FORCEINLINE int4 (simd_t m) : m_vec(m) { }

    /// Return the raw SIMD type
    OIIO_FORCEINLINE operator simd_t () const { return m_vec; }
    OIIO_FORCEINLINE simd_t simd () const { return m_vec; }

    /// Sset all components to 0
    OIIO_FORCEINLINE void clear () {
#if defined(OIIO_SIMD_SSE)
        m_vec = _mm_setzero_si128();
#else
        *this = 0.0f;
#endif
    }

    /// Return an int4 with all components set to 0
    static OIIO_FORCEINLINE const int4 Zero () { return int4(0); }

    /// Return an int4 with all components set to 1
    static OIIO_FORCEINLINE const int4 One () { return int4(1); }

    /// Return an int4 with incremented components (e.g., 0,1,2,3).
    /// Optional argument can give a non-zero starting point.
    static OIIO_FORCEINLINE const int4 Iota (int value=0) {
        return int4(value,value+1,value+2,value+3);
    }

    /// Assign one value to all components.
    OIIO_FORCEINLINE const int4 & operator= (int a) { load(a); return *this; }

    /// Assignment from another int4
    OIIO_FORCEINLINE const int4 & operator= (int4 other) {
#if defined(OIIO_SIMD_SSE)
        m_vec = other.m_vec;
#else
        m_val[0] = other.m_val[0];
        m_val[1] = other.m_val[1];
        m_val[2] = other.m_val[2];
        m_val[3] = other.m_val[3];
#endif
        return *this;
    }

    /// Component access (set)
    OIIO_FORCEINLINE int& operator[] (int i) {
        DASSERT(i<4);
        return m_val[i];
    }

    /// Component access (get)
    OIIO_FORCEINLINE int operator[] (int i) const {
        DASSERT(i<4);
        return m_val[i];
    }

    /// Helper: load a single int into all components
    OIIO_FORCEINLINE void load (int a) {
#if defined(OIIO_SIMD_SSE)
        m_vec = _mm_set1_epi32 (a);
#else
        m_val[0] = a;
        m_val[1] = a;
        m_val[2] = a;
        m_val[3] = a;
#endif
    }

    /// Helper: load separate values into each component.
    OIIO_FORCEINLINE void load (int a, int b, int c, int d) {
#if defined(OIIO_SIMD_SSE)
        m_vec = _mm_set_epi32 (d, c, b, a);
#else
        m_val[0] = a;
        m_val[1] = b;
        m_val[2] = c;
        m_val[3] = d;
#endif
    }

    /// Load from an array of 4 values
    OIIO_FORCEINLINE void load (const int *values) {
#if defined(OIIO_SIMD_SSE)
        m_vec = _mm_loadu_si128 ((simd_t *)values);
#else
        m_val[0] = values[0];
        m_val[1] = values[1];
        m_val[2] = values[2];
        m_val[3] = values[3];
#endif
    }

    /// Store the values into memory
    OIIO_FORCEINLINE void store (int *values) {
#if defined(OIIO_SIMD_SSE)
        // Use an unaligned store -- it's just as fast when the memory turns
        // out to be aligned, nearly as fast even when unaligned. Not worth
        // the headache of using stores that require alignment.
        _mm_storeu_si128 ((simd_t *)values, m_vec);
#else
        values[0] = m_val[0];
        values[1] = m_val[1];
        values[2] = m_val[2];
        values[3] = m_val[3];
#endif
    }

    /// Store the first n values into memory
    OIIO_FORCEINLINE void store (int *values, int n) {
        DASSERT (n >= 0 && n < 4);
#if defined(OIIO_SIMD_SSE)
        // For SSE, there is a speed advantage to storing all 4 components.
        if (n == 4)
            store (values);
        else
#endif
        for (int i = 0; i < n; ++i)
            values[i] = m_val[i];
    }

    friend OIIO_FORCEINLINE int4 operator+ (int4 a, int4 b) {
#if defined(OIIO_SIMD_SSE)
        return _mm_add_epi32 (a.m_vec, b.m_vec);
#else
        return int4 (a.m_val[0] + b.m_val[0],
                     a.m_val[1] + b.m_val[1],
                     a.m_val[2] + b.m_val[2],
                     a.m_val[3] + b.m_val[3]);
#endif
    }

    OIIO_FORCEINLINE const int4 & operator+= (int4 a) {
#if defined(OIIO_SIMD_SSE)
        m_vec = _mm_add_epi32 (m_vec, a.m_vec);
#else
        m_val[0] += a.m_val[0];
        m_val[1] += a.m_val[1];
        m_val[2] += a.m_val[2];
        m_val[3] += a.m_val[3];
#endif
        return *this;
    }

    friend OIIO_FORCEINLINE int4 operator- (int4 a, int4 b) {
#if defined(OIIO_SIMD_SSE)
        return _mm_sub_epi32 (a.m_vec, b.m_vec);
#else
        return int4 (a.m_val[0] - b.m_val[0],
                     a.m_val[1] - b.m_val[1],
                     a.m_val[2] - b.m_val[2],
                     a.m_val[3] - b.m_val[3]);
#endif
    }

    OIIO_FORCEINLINE const int4 & operator-= (int4 a) {
#if defined(OIIO_SIMD_SSE)
        m_vec = _mm_sub_epi32 (m_vec, a.m_vec);
#else
        m_val[0] -= a.m_val[0];
        m_val[1] -= a.m_val[1];
        m_val[2] -= a.m_val[2];
        m_val[3] -= a.m_val[3];
#endif
        return *this;
    }

    friend OIIO_FORCEINLINE int4 operator* (int4 a, int4 b) {
#if defined(OIIO_SIMD_SSE)
        return mm_mul_epi32 (a.m_vec, b.m_vec);
#else
        return int4 (a.m_val[0] * b.m_val[0],
                     a.m_val[1] * b.m_val[1],
                     a.m_val[2] * b.m_val[2],
                     a.m_val[3] * b.m_val[3]);
#endif
    }

    OIIO_FORCEINLINE const int4 & operator*= (int4 a) {
#if defined(OIIO_SIMD_SSE)
        m_vec = mm_mul_epi32 (m_vec, a.m_vec);
#else
        m_val[0] *= a.m_val[0];
        m_val[1] *= a.m_val[1];
        m_val[2] *= a.m_val[2];
        m_val[3] *= a.m_val[3];
#endif
        return *this;
    }

    OIIO_FORCEINLINE const int4 & operator*= (int val) {
#if defined(OIIO_SIMD_SSE)
        m_vec = mm_mul_epi32 (m_vec, _mm_set1_epi32(val));
#else
        m_val[0] *= val;
        m_val[1] *= val;
        m_val[2] *= val;
        m_val[3] *= val;
#endif
        return *this;
    }

    friend OIIO_FORCEINLINE int4 operator/ (int4 a, int4 b) {
        // NO INTEGER DIVISION IN SSE!
        return int4 (a.m_val[0] / b.m_val[0],
                     a.m_val[1] / b.m_val[1],
                     a.m_val[2] / b.m_val[2],
                     a.m_val[3] / b.m_val[3]);
    }

    OIIO_FORCEINLINE const int4 & operator/= (int4 a) {
        // NO INTEGER DIVISION IN SSE!
        m_val[0] /= a.m_val[0];
        m_val[1] /= a.m_val[1];
        m_val[2] /= a.m_val[2];
        m_val[3] /= a.m_val[3];
        return *this;
    }

    OIIO_FORCEINLINE const int4 & operator/= (int val) {
        // NO INTEGER DIVISION IN SSE!
        m_val[0] /= val;
        m_val[1] /= val;
        m_val[2] /= val;
        m_val[3] /= val;
        return *this;
    }

    friend OIIO_FORCEINLINE int4 operator% (int4 a, int4 b) {
        // NO INTEGER MODULUS in SSE!
        return int4 (a.m_val[0] % b.m_val[0],
                     a.m_val[1] % b.m_val[1],
                     a.m_val[2] % b.m_val[2],
                     a.m_val[3] % b.m_val[3]);
    }
    friend OIIO_FORCEINLINE int4 operator% (int4 a, int w) {
        // NO INTEGER MODULUS in SSE!
        return int4 (a.m_val[0] % w,
                     a.m_val[1] % w,
                     a.m_val[2] % w,
                     a.m_val[3] % w);
    }
    friend OIIO_FORCEINLINE int4 operator% (int a, int4 b) {
        // NO INTEGER MODULUS in SSE!
        return int4 (a % b.m_val[0],
                     a % b.m_val[1],
                     a % b.m_val[2],
                     a % b.m_val[3]);
    }


    friend OIIO_FORCEINLINE int4 operator& (int4 a, int4 b) {
#if defined(OIIO_SIMD_SSE)
        return _mm_and_si128 (a.m_vec, b.m_vec);
#else
        return int4 (a.m_val[0] & b.m_val[0],
                     a.m_val[1] & b.m_val[1],
                     a.m_val[2] & b.m_val[2],
                     a.m_val[3] & b.m_val[3]);
#endif
    }
    OIIO_FORCEINLINE int4 operator&= (int4 a) {
        return *this = *this & a;
    }


    friend OIIO_FORCEINLINE int4 operator| (int4 a, int4 b) {
#if defined(OIIO_SIMD_SSE)
        return _mm_or_si128 (a.m_vec, b.m_vec);
#else
        return int4 (a.m_val[0] | b.m_val[0],
                     a.m_val[1] | b.m_val[1],
                     a.m_val[2] | b.m_val[2],
                     a.m_val[3] | b.m_val[3]);
#endif
    }
    OIIO_FORCEINLINE int4 operator|= (int4 a) {
        return *this = *this | a;
    }

    friend OIIO_FORCEINLINE mask4 operator== (int4 a, int4 b) {
#if defined(OIIO_SIMD_SSE)
        return _mm_castsi128_ps(_mm_cmpeq_epi32 (a.m_vec, b.m_vec));
#else
        return mask4 (a[0] == b[0], a[1] == b[1], a[2] == b[2], a[3] == b[3]);
#endif
    }
  
    friend OIIO_FORCEINLINE mask4 operator!= (int4 a, int4 b) {
        return ! (a == b);
    }
  
    friend OIIO_FORCEINLINE mask4 operator< (int4 a, int4 b) {
#if defined(OIIO_SIMD_SSE)
        return _mm_castsi128_ps(_mm_cmplt_epi32 (a.m_vec, b.m_vec));
#else
        return mask4 (a[0] < b[0], a[1] < b[1], a[2] < b[2], a[3] < b[3]);
#endif
    }
  
    friend OIIO_FORCEINLINE mask4 operator>  (int4 a, int4 b) {
#if defined(OIIO_SIMD_SSE)
        return _mm_castsi128_ps(_mm_cmpgt_epi32 (a.m_vec, b.m_vec));
#else
        return mask4 (a[0] > b[0], a[1] > b[1], a[2] > b[2], a[3] > b[3]);
#endif
    }

    friend OIIO_FORCEINLINE mask4 operator>= (int4 a, int4 b) {
        return !(a < b);
    }

    friend OIIO_FORCEINLINE mask4 operator<= (int4 a, int4 b) {
        return !(a > b);
    }

    /// Stream output
    friend inline std::ostream& operator<< (std::ostream& cout, int4 val) {
        return cout << val[0] << ' ' << val[1] << ' ' << val[2] << ' ' << val[3];
    }

private:
    // The actual data representation
    union {
        simd_t  m_vec;
        value_t m_val[4];
    };

};



/// Helper: shuffle/swizzle with constant (templated) indices.
/// Example: shuffle<1,1,2,2>(mask4(a,b,c,d)) returns (b,b,c,c)
template<int i0, int i1, int i2, int i3>
OIIO_FORCEINLINE const int4 shuffle (int4 a) {
#if defined(OIIO_SIMD_SSE)
    return shuffle_sse<i0,i1,i2,i3> (__m128i(a));
#else
    return int4(a[i0], a[i1], a[i2], a[i3]);
#endif
}

/// Helper: as rapid as possible extraction of one component, when the
/// index is fixed.
template<int i>
OIIO_FORCEINLINE const int extract (int4 v) {
#if defined(OIIO_SIMD_SSE) && OIIO_SIMD_SSE >= 4
    return _mm_extract_epi32(v, i);  // SSE4.1 only
#else
    return v[i];
#endif
}

/// The sum of all components.
OIIO_FORCEINLINE int reduce_add (int4 v) {
#if defined(OIIO_SIMD_SSE) && OIIO_SIMD_SSE >= 3
    // People seem to agree that SSE3 does add reduction best with 2 
    // horizontal adds.
    // suppose v = (a, b, c, d)
    simd::int4 x = _mm_hadd_ps (v, v);
    // now x = (a+b, c+d, a+b, c+d)
    x = _mm_hadd_ps (x, x);
    // now all x elements are a+b+c+d
    return extract<0>(x);
#elif defined(OIIO_SIMD_SSE)
    // I think this is the best we can do for SSE2, and I'm still not sure
    // it's faster than the default scalar operation. But anyway...
    // suppose v = (a, b, c, d)
    int4 x = shuffle<1,0,3,2>(v) + v;
    // now x = (b,a,d,c) + (a,b,c,d) = (a+b,a+b,c+d,c+d)
    int4 y = shuffle<2,3,0,1>(x);
    // now y = (c+d,c+d,a+b,a+b)
    int4 z = x+y;   // a+b+c+d in all components
    return extract<0>(z);
#else
    return v[0] + v[1] + v[2] + v[3];
#endif
}


/// Bitwise "and" of all components.
OIIO_FORCEINLINE int reduce_and (int4 v) {
#if defined(OIIO_SIMD_SSE)
    // I think this is the best we can do for SSE, and I'm still not sure
    // it's faster than the default scalar operation. But anyway...
    // suppose v = (a, b, c, d)
    int4 x = shuffle<1,0,3,2>(v) & v;
    return extract<0>(x) & extract<2>(x);
#else
    return v[0] & v[1] & v[2] & v[3];
#endif
}


/// Bitwise "or" of all components.
OIIO_FORCEINLINE int reduce_or (int4 v) {
#if defined(OIIO_SIMD_SSE)
    // I think this is the best we can do for SSE, and I'm still not sure
    // it's faster than the default scalar operation. But anyway...
    // suppose v = (a, b, c, d)
    int4 x = shuffle<1,0,3,2>(v) | v;
    return extract<0>(x) | extract<2>(x);
#else
    return v[0] | v[1] | v[2] | v[3];
#endif
}

/// Use a mask to select between components of a (if mask[i] is false) and
/// b (if mask[i] is true).
OIIO_FORCEINLINE int4 blend (int4 a, int4 b, mask4 mask)
{
#if defined(OIIO_SIMD_SSE) && OIIO_SIMD_SSE >= 4 /* SSE >= 4.1 */
    return _mm_blendv_epi8 (a, b, mask);
#elif defined(OIIO_SIMD_SSE) && OIIO_SIMD_SSE >= 4 /* SSE >= 4.1 */
    return _mm_or_si128 (_mm_and_si128(mask, b), _mm_andnot_si128(mask, a));
#else
    return int4 (mask[0] ? b[0] : a[0],
                 mask[1] ? b[1] : a[1],
                 mask[2] ? b[2] : a[2],
                 mask[3] ? b[3] : a[3]);
#endif
}





/// Floating point 4-vector, accelerated by SIMD instructions when
/// available.
class float4 {
public:
    static const char* type_name() { return "float4"; }
    typedef float value_t;    ///< Underlying equivalent scalar value type
    enum { elements = 4 };    ///< Number of scalar elements
    enum { bits = 128 };      ///< Total number of bits
    /// simd_t is the native SIMD type used
#if defined(OIIO_SIMD_SSE)
    typedef __m128   simd_t;
#else
    typedef float[4] simd_t;
#endif

    /// Default constructor (contents undefined)
    OIIO_FORCEINLINE float4 () { }

    /// Destructor
    OIIO_FORCEINLINE ~float4 () { }

    /// Construct from a single value (store it in all slots)
    OIIO_FORCEINLINE float4 (float a) { load(a); }

    /// Construct from 3 or 4 values
    OIIO_FORCEINLINE float4 (float a, float b, float c, float d=0.0f) { load(a,b,c,d); }

    /// Construct from a pointer to 4 values
    OIIO_FORCEINLINE float4 (const float *f) { load (f); }

    /// Copy construct from another float4
    OIIO_FORCEINLINE float4 (const float4 &other) {
#if defined(OIIO_SIMD_SSE)
        m_vec = other.m_vec;
#else
        m_val[0] = other.m_val[0];
        m_val[1] = other.m_val[1];
        m_val[2] = other.m_val[2];
        m_val[3] = other.m_val[3];
#endif
    }

    /// Construct from an int4 (promoting all components to float)
    OIIO_FORCEINLINE explicit float4 (int4 i) {
#if defined(OIIO_SIMD_SSE)
        m_vec = _mm_cvtepi32_ps (i);
#else
        m_val[0] = float(i[0]);
        m_val[1] = float(i[1]);
        m_val[2] = float(i[2]);
        m_val[3] = float(i[3]);
#endif
    }

    /// Construct from a Imath::V4f
    OIIO_FORCEINLINE float4 (const Imath::V4f &v) { load ((const float *)&v); }

    /// Construct from a Imath::V3f
    OIIO_FORCEINLINE float4 (const Imath::V3f &v) { load (v[0], v[1], v[2], 0.0f); }

    /// Construct from the underlying SIMD type
    OIIO_FORCEINLINE float4 (const simd_t m) : m_vec(m) { }

    /// Return the raw SIMD type
    OIIO_FORCEINLINE operator simd_t () const { return m_vec; }
    OIIO_FORCEINLINE simd_t simd () const { return m_vec; }

    /// Cast to a Imath::V3f
    OIIO_FORCEINLINE const Imath::V3f& V3f () const { return *(const Imath::V3f*)this; }

    /// Cast to a Imath::V4f
    OIIO_FORCEINLINE const Imath::V4f& V4f () const { return *(const Imath::V4f*)this; }

    /// Assign a single value to all components
    OIIO_FORCEINLINE const float4 & operator= (float a) { load(a); return *this; }

    /// Assign a float4
    OIIO_FORCEINLINE const float4 & operator= (float4 other) {
#if defined(OIIO_SIMD_SSE)
        m_vec = other.m_vec;
#else
        m_val[0] = other.m_val[0];
        m_val[1] = other.m_val[1];
        m_val[2] = other.m_val[2];
        m_val[3] = other.m_val[3];
#endif
        return *this;
    }

    /// Return a float4 with all components set to 0.0
    static OIIO_FORCEINLINE const float4 Zero () { return float4(0.0f); }

    /// Return a float4 with all components set to 1.0
    static OIIO_FORCEINLINE const float4 One () { return float4(1.0f); }

    /// Return a float4 with incremented components (e.g., 0.0,1.0,2.0,3.0).
    /// Optional argument can give a non-zero starting point.
    static OIIO_FORCEINLINE const float4 Iota (float value=0.0f) {
        return float4(value,value+1.0f,value+2.0f,value+3.03);
    }

    /// Sset all components to 0.0
    OIIO_FORCEINLINE void clear () {
#if defined(OIIO_SIMD_SSE)
        m_vec = _mm_setzero_ps();
#else
        *this = 0.0f;
#endif
    }

    /// Assign from a Imath::V4f
    OIIO_FORCEINLINE const float4 & operator= (const Imath::V4f &v) {
        load ((const float *)&v);
        return *this;
    }

    /// Assign from a Imath::V3f
    OIIO_FORCEINLINE const float4 & operator= (const Imath::V3f &v) {
        load (v[0], v[1], v[2], 0.0f);
        return *this;
    }

    /// Component access (set)
    OIIO_FORCEINLINE float& operator[] (int i) {
        DASSERT(i<4);
        return m_val[i];
    }
    /// Component access (get)
    OIIO_FORCEINLINE float operator[] (int i) const {
        DASSERT(i<4);
        return m_val[i];
    }

    /// Helper: load a single value into all components
    OIIO_FORCEINLINE void load (float val) {
#if defined(OIIO_SIMD_SSE)
        m_vec = _mm_set1_ps (val);
#else
        m_val[0] = val;
        m_val[1] = val;
        m_val[2] = val;
        m_val[3] = val;
#endif
    }

    /// Helper: load 3 or 4 values. (If 3 are supplied, the 4th will be 0.)
    OIIO_FORCEINLINE void load (float a, float b, float c, float d=0.0f) {
#if defined(OIIO_SIMD_SSE)
        m_vec = _mm_set_ps (d, c, b, a);
#else
        m_val[0] = a;
        m_val[1] = b;
        m_val[2] = c;
        m_val[3] = d;
#endif
    }

    /// Load from an array of 4 values
    OIIO_FORCEINLINE void load (const float *values) {
#if defined(OIIO_SIMD_SSE)
        m_vec = _mm_loadu_ps (values);
#else
        m_val[0] = values[0];
        m_val[1] = values[1];
        m_val[2] = values[2];
        m_val[3] = values[3];
#endif
    }

    OIIO_FORCEINLINE void store (float *values) {
#if defined(OIIO_SIMD_SSE)
        // Use an unaligned store -- it's just as fast when the memory turns
        // out to be aligned, nearly as fast even when unaligned. Not worth
        // the headache of using stores that require alignment.
        _mm_storeu_ps (values, m_vec);
#else
        values[0] = m_val[0];
        values[1] = m_val[1];
        values[2] = m_val[2];
        values[3] = m_val[3];
#endif
    }

    /// Store the first n values into memory
    OIIO_FORCEINLINE void store (float *values, int n) {
        DASSERT (n >= 0 && n < 4);
#if defined(OIIO_SIMD_SSE)
        // For SSE, there is a speed advantage to storing all 4 components.
        if (n == 4)
            store (values);
        else
#endif
        for (int i = 0; i < n; ++i)
            values[i] = m_val[i];
    }

    friend OIIO_FORCEINLINE float4 operator+ (float4 a, float4 b) {
#if defined(OIIO_SIMD_SSE)
        return _mm_add_ps (a.m_vec, b.m_vec);
#else
        return float4 (a.m_val[0] + b.m_val[0],
                       a.m_val[1] + b.m_val[1],
                       a.m_val[2] + b.m_val[2],
                       a.m_val[3] + b.m_val[3]);
#endif
    }

    OIIO_FORCEINLINE const float4 & operator+= (float4 a) {
#if defined(OIIO_SIMD_SSE)
        m_vec = _mm_add_ps (m_vec, a.m_vec);
#else
        m_val[0] += a.m_val[0];
        m_val[1] += a.m_val[1];
        m_val[2] += a.m_val[2];
        m_val[3] += a.m_val[3];
#endif
        return *this;
    }

    friend OIIO_FORCEINLINE float4 operator- (float4 a, float4 b) {
#if defined(OIIO_SIMD_SSE)
        return _mm_sub_ps (a.m_vec, b.m_vec);
#else
        return float4 (a.m_val[0] - b.m_val[0],
                       a.m_val[1] - b.m_val[1],
                       a.m_val[2] - b.m_val[2],
                       a.m_val[3] - b.m_val[3]);
#endif
    }

    OIIO_FORCEINLINE const float4 & operator-= (float4 a) {
#if defined(OIIO_SIMD_SSE)
        m_vec = _mm_sub_ps (m_vec, a.m_vec);
#else
        m_val[0] -= a.m_val[0];
        m_val[1] -= a.m_val[1];
        m_val[2] -= a.m_val[2];
        m_val[3] -= a.m_val[3];
#endif
        return *this;
    }

    friend OIIO_FORCEINLINE float4 operator* (float4 a, float4 b) {
#if defined(OIIO_SIMD_SSE)
        return _mm_mul_ps (a.m_vec, b.m_vec);
#else
        return float4 (a.m_val[0] * b.m_val[0],
                       a.m_val[1] * b.m_val[1],
                       a.m_val[2] * b.m_val[2],
                       a.m_val[3] * b.m_val[3]);
#endif
    }

    OIIO_FORCEINLINE const float4 & operator*= (float4 a) {
#if defined(OIIO_SIMD_SSE)
        m_vec = _mm_mul_ps (m_vec, a.m_vec);
#else
        m_val[0] *= a.m_val[0];
        m_val[1] *= a.m_val[1];
        m_val[2] *= a.m_val[2];
        m_val[3] *= a.m_val[3];
#endif
        return *this;
    }
    OIIO_FORCEINLINE const float4 & operator*= (float val) {
#if defined(OIIO_SIMD_SSE)
        m_vec = _mm_mul_ps (m_vec, _mm_set1_ps(val));
#else
        m_val[0] *= val;
        m_val[1] *= val;
        m_val[2] *= val;
        m_val[3] *= val;
#endif
        return *this;
    }

    friend OIIO_FORCEINLINE float4 operator/ (float4 a, float4 b) {
#if defined(OIIO_SIMD_SSE)
        return _mm_div_ps (a.m_vec, b.m_vec);
#else
        return float4 (a.m_val[0] / b.m_val[0],
                       a.m_val[1] / b.m_val[1],
                       a.m_val[2] / b.m_val[2],
                       a.m_val[3] / b.m_val[3]);
#endif
    }
    OIIO_FORCEINLINE const float4 & operator/= (float4 a) {
#if defined(OIIO_SIMD_SSE)
        m_vec = _mm_div_ps (m_vec, a.m_vec);
#else
        m_val[0] /= a.m_val[0];
        m_val[1] /= a.m_val[1];
        m_val[2] /= a.m_val[2];
        m_val[3] /= a.m_val[3];
#endif
        return *this;
    }
    OIIO_FORCEINLINE const float4 & operator/= (float val) {
#if defined(OIIO_SIMD_SSE)
        m_vec = _mm_div_ps (m_vec, _mm_set1_ps(val));
#else
        m_val[0] /= val;
        m_val[1] /= val;
        m_val[2] /= val;
        m_val[3] /= val;
#endif
        return *this;
    }


    friend OIIO_FORCEINLINE mask4 operator== (float4 a, float4 b) {
#if defined(OIIO_SIMD_SSE)
        return _mm_cmpeq_ps (a.m_vec, b.m_vec);
#else
        return mask4 (a[0] == b[0], a[1] == b[1], a[2] == b[2], a[3] == b[3]);
#endif
    }
  
    friend OIIO_FORCEINLINE mask4 operator!= (float4 a, float4 b) {
#if defined(OIIO_SIMD_SSE)
        return _mm_cmpneq_ps (a.m_vec, b.m_vec);
#else
        return mask4 (a[0] != b[0], a[1] != b[1], a[2] != b[2], a[3] != b[3]);
#endif
    }
  
    friend OIIO_FORCEINLINE mask4 operator< (float4 a, float4 b) {
#if defined(OIIO_SIMD_SSE)
        return _mm_cmplt_ps (a.m_vec, b.m_vec);
#else
        return mask4 (a[0] < b[0], a[1] < b[1], a[2] < b[2], a[3] < b[3]);
#endif
    }
  
    friend OIIO_FORCEINLINE mask4 operator>  (float4 a, float4 b) {
#if defined(OIIO_SIMD_SSE)
        return _mm_cmpgt_ps (a.m_vec, b.m_vec);
#else
        return mask4 (a[0] > b[0], a[1] > b[1], a[2] > b[2], a[3] > b[3]);
#endif
    }

    friend OIIO_FORCEINLINE mask4 operator>= (float4 a, float4 b) {
#if defined(OIIO_SIMD_SSE)
        return _mm_cmpge_ps (a.m_vec, b.m_vec);
#else
        return mask4 (a[0] >= b[0], a[1] >= b[1], a[2] >= b[2], a[3] >= b[3]);
#endif
    }

    friend OIIO_FORCEINLINE mask4 operator<= (float4 a, float4 b) {
#if defined(OIIO_SIMD_SSE)
        return _mm_cmple_ps (a.m_vec, b.m_vec);
#else
        return mask4 (a[0] <= b[0], a[1] <= b[1], a[2] <= b[2], a[3] <= b[3]);
#endif
    }

    /// Stream output
    friend inline std::ostream& operator<< (std::ostream& cout, float4 val) {
        return cout << val[0] << ' ' << val[1] << ' ' << val[2] << ' ' << val[3];
    }

private:
    // The actual data representation
    union {
        simd_t  m_vec;
        value_t m_val[4];
    };
};




/// Helper: shuffle/swizzle with constant (templated) indices.
/// Example: shuffle<1,1,2,2>(mask4(a,b,c,d)) returns (b,b,c,c)
template<int i0, int i1, int i2, int i3>
OIIO_FORCEINLINE const float4 shuffle (float4 a) {
#if defined(OIIO_SIMD_SSE)
    return shuffle_sse<i0,i1,i2,i3> (__m128(a));
#else
    return float4(a[i0], a[i1], a[i2], a[i3]);
#endif
}

/// Helper: as rapid as possible extraction of one component, when the
/// index is fixed.
template<int i>
OIIO_FORCEINLINE const float extract (float4 a) {
#if defined(OIIO_SIMD_SSE)
    return _mm_cvtss_f32(shuffle<i,i,i,i>(a));
#else
    return a[i];
#endif
}

/// Use a mask to select between components of a (if mask[i] is false) and
/// b (if mask[i] is true).
OIIO_FORCEINLINE float4 blend (float4 a, float4 b, mask4 mask)
{
#if defined(OIIO_SIMD_SSE) && OIIO_SIMD_SSE >= 4
    // SSE >= 4.1 only
    return _mm_blendv_ps (a, b, mask);
#elif defined(OIIO_SIMD_SSE)
    // Trick for SSE < 4.1
    return _mm_or_ps (_mm_and_ps(mask, b), _mm_andnot_ps(mask, a));
#else
    return float4 (mask[0] ? b[0] : a[0],
                   mask[1] ? b[1] : a[1],
                   mask[2] ? b[2] : a[2],
                   mask[3] ? b[3] : a[3]);
#endif
}



/// Template to retrieve the vector type from the scalar. For example,
/// simd::VecType<int,4> will be int4.
template<typename T,int elements> struct VecType {};
template<> struct VecType<int,4>   { typedef int4 type; };
template<> struct VecType<float,4> { typedef float4 type; };
template<> struct VecType<bool,4>  { typedef mask4 type; };


} // end namespace

} OIIO_NAMESPACE_EXIT
