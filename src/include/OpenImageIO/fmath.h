/*
  Copyright 2008-2014 Larry Gritz and the other authors and contributors.
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

  A few bits here are based upon code from NVIDIA that was also released
  under the same modified BSD license, and marked as:
     Copyright 2004 NVIDIA Corporation. All Rights Reserved.

  Some parts of this file were first open-sourced in Open Shading Language,
  then later moved here. The original copyright notice was:
     Copyright (c) 2009-2014 Sony Pictures Imageworks Inc., et al.

  Many of the math functions were copied from or inspired by other
  public domain sources or open source packages with compatible licenses.
  The individual functions give references were applicable.
*/


/// \file
///
/// A variety of floating-point math helper routines (and, slight
/// misnomer, some int stuff as well).
///


#pragma once

#include <cmath>
#include <limits>
#include <typeinfo>
#include <algorithm>
#include <cstring>
#include <cmath>

#include "oiioversion.h"   /* Just for the OIIO_NAMESPACE stuff */
#include "platform.h"
#include "dassert.h"
#include "missing_math.h"
#include "simd.h"
#include "array_view.h"


OIIO_NAMESPACE_BEGIN


/// Helper template to let us tell if two types are the same.
template<typename T, typename U> struct is_same { static const bool value = false; };
template<typename T> struct is_same<T,T> { static const bool value = true; };




////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
// INTEGER HELPER FUNCTIONS
//
// A variety of handy functions that operate on integers.
//

/// Quick test for whether an integer is a power of 2.
///
template<typename T>
inline bool
ispow2 (T x)
{
    // Numerous references for this bit trick are on the web.  The
    // principle is that x is a power of 2 <=> x == 1<<b <=> x-1 is
    // all 1 bits for bits < b.
    return (x & (x-1)) == 0 && (x >= 0);
}



/// Round up to next higher power of 2 (return x if it's already a power
/// of 2).
inline int
pow2roundup (int x)
{
    // Here's a version with no loops.
    if (x < 0)
        return 0;
    // Subtract 1, then round up to a power of 2, that way if we are
    // already a power of 2, we end up with the same number.
    --x;
    // Make all bits past the first 1 also be 1, i.e. 0001xxxx -> 00011111
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    // Now we have 2^n-1, by adding 1, we make it a power of 2 again
    return x+1;
}



/// Round down to next lower power of 2 (return x if it's already a power
/// of 2).
inline int
pow2rounddown (int x)
{
    // Make all bits past the first 1 also be 1, i.e. 0001xxxx -> 00011111
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    // Strip off all but the high bit, i.e. 00011111 -> 00010000
    // That's the power of two <= the original x
    return x & ~(x >> 1);
}



/// Round value up to the next whole multiple.
/// For example, round_to_multiple(7,10) returns 10.
template <typename V, typename M>
inline V round_to_multiple (V value, M multiple)
{
    return V (((value + V(multiple) - 1) / V(multiple)) * V(multiple));
}



/// Round up to the next whole multiple of m, for the special case where
/// m is definitely a power of 2 (somewhat simpler than the more general
/// round_to_multiple). This is a template that should work for any
// integer type.
template<typename T>
inline T
round_to_multiple_of_pow2 (T x, T m)
{
    DASSERT (ispow2 (m));
    return (x + m - 1) & (~(m-1));
}



/// Multiply two unsigned 32-bit ints safely, carefully checking for
/// overflow, and clamping to uint32_t's maximum value.
inline uint32_t
clamped_mult32 (uint32_t a, uint32_t b)
{
    const uint32_t Err = std::numeric_limits<uint32_t>::max();
    uint64_t r = (uint64_t)a * (uint64_t)b;   // Multiply into a bigger int
    return r < Err ? (uint32_t)r : Err;
}



/// Multiply two unsigned 64-bit ints safely, carefully checking for
/// overflow, and clamping to uint64_t's maximum value.
inline uint64_t
clamped_mult64 (uint64_t a, uint64_t b)
{
    uint64_t ab = a*b;
    if (b && ab/b != a)
        return std::numeric_limits<uint64_t>::max();
    else
        return ab;
}



/// Bitwise circular rotation left by k bits (for 32 bit unsigned integers)
OIIO_FORCEINLINE uint32_t rotl32 (uint32_t x, int k) {
    return (x<<k) | (x>>(32-k));
}

/// Bitwise circular rotation left by k bits (for 64 bit unsigned integers)
OIIO_FORCEINLINE uint64_t rotl64 (uint64_t x, int k) {
    return (x<<k) | (x>>(64-k));
}


// (end of integer helper functions)
////////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
// FLOAT UTILITY FUNCTIONS
//
// Helper/utility functions: clamps, blends, interpolations...
//


/// clamp a to bounds [low,high].
template <class T>
inline T
clamp (T a, T low, T high)
{
    return (a < low) ? low : ((a > high) ? high : a);
}


// Specialization of clamp for float4
template<> inline simd::float4
clamp (simd::float4 a, simd::float4 low, simd::float4 high)
{
    return simd::min (high, simd::max (low, a));
}

template<> inline simd::float8
clamp (simd::float8 a, simd::float8 low, simd::float8 high)
{
    return simd::min (high, simd::max (low, a));
}



/// Fused multiply and add: (a*b + c)
inline float madd (float a, float b, float c) {
#if OIIO_FMA_ENABLED && (OIIO_CPLUSPLUS_VERSION >= 11)
    // C++11 defines std::fma, which we assume is implemented using an
    // intrinsic.
    return std::fma (a, b, c);
#else
    // NOTE: GCC/ICC will turn this (for float) into a FMA unless
    // explicitly asked not to, clang will do so if -ffp-contract=fast.
    return a * b + c;
#endif
}


/// Fused multiply and subtract: -(a*b - c)
inline float msub (float a, float b, float c) {
    return a * b - c; // Hope for the best
}



/// Fused negative multiply and add: -(a*b) + c
inline float nmadd (float a, float b, float c) {
    return c - (a * b); // Hope for the best
}



/// Negative fused multiply and subtract: -(a*b) - c
inline float nmsub (float a, float b, float c) {
    return -(a * b) - c; // Hope for the best
}



/// Linearly interpolate values v0-v1 at x: v0*(1-x) + v1*x.
/// This is a template, and so should work for any types.
template <class T, class Q>
inline T
lerp (const T& v0, const T& v1, const Q& x)
{
    // NOTE: a*(1-x) + b*x is much more numerically stable than a+x*(b-a)
    return v0*(Q(1)-x) + v1*x;
}



/// Bilinearly interoplate values v0-v3 (v0 upper left, v1 upper right,
/// v2 lower left, v3 lower right) at coordinates (s,t) and return the
/// result.  This is a template, and so should work for any types.
template <class T, class Q>
inline T
bilerp(const T& v0, const T& v1, const T& v2, const T& v3, const Q& s, const Q& t)
{
    // NOTE: a*(t-1) + b*t is much more numerically stable than a+t*(b-a)
    Q s1 = Q(1) - s;
    return T ((Q(1)-t)*(v0*s1 + v1*s) + t*(v2*s1 + v3*s));
}



/// Bilinearly interoplate arrays of values v0-v3 (v0 upper left, v1
/// upper right, v2 lower left, v3 lower right) at coordinates (s,t),
/// storing the results in 'result'.  These are all vectors, so do it
/// for each of 'n' contiguous values (using the same s,t interpolants).
template <class T, class Q>
inline void
bilerp (const T *v0, const T *v1,
        const T *v2, const T *v3,
        Q s, Q t, int n, T *result)
{
    Q s1 = Q(1) - s;
    Q t1 = Q(1) - t;
    for (int i = 0;  i < n;  ++i)
        result[i] = T (t1*(v0[i]*s1 + v1[i]*s) + t*(v2[i]*s1 + v3[i]*s));
}



/// Bilinearly interoplate arrays of values v0-v3 (v0 upper left, v1
/// upper right, v2 lower left, v3 lower right) at coordinates (s,t),
/// SCALING the interpolated value by 'scale' and then ADDING to
/// 'result'.  These are all vectors, so do it for each of 'n'
/// contiguous values (using the same s,t interpolants).
template <class T, class Q>
inline void
bilerp_mad (const T *v0, const T *v1,
            const T *v2, const T *v3,
            Q s, Q t, Q scale, int n, T *result)
{
    Q s1 = Q(1) - s;
    Q t1 = Q(1) - t;
    for (int i = 0;  i < n;  ++i)
        result[i] += T (scale * (t1*(v0[i]*s1 + v1[i]*s) +
                                  t*(v2[i]*s1 + v3[i]*s)));
}



/// Trilinearly interoplate arrays of values v0-v7 (v0 upper left top, v1
/// upper right top, ...) at coordinates (s,t,r), and return the
/// result.  This is a template, and so should work for any types.
template <class T, class Q>
inline T
trilerp (T v0, T v1, T v2, T v3, T v4, T v5, T v6, T v7, Q s, Q t, Q r)
{
    // NOTE: a*(t-1) + b*t is much more numerically stable than a+t*(b-a)
    Q s1 = Q(1) - s;
    Q t1 = Q(1) - t;
    Q r1 = Q(1) - r;
    return T (r1*(t1*(v0*s1 + v1*s) + t*(v2*s1 + v3*s)) +
               r*(t1*(v4*s1 + v5*s) + t*(v6*s1 + v7*s)));
}



/// Trilinearly interoplate arrays of values v0-v7 (v0 upper left top, v1
/// upper right top, ...) at coordinates (s,t,r),
/// storing the results in 'result'.  These are all vectors, so do it
/// for each of 'n' contiguous values (using the same s,t,r interpolants).
template <class T, class Q>
inline void
trilerp (const T *v0, const T *v1, const T *v2, const T *v3,
         const T *v4, const T *v5, const T *v6, const T *v7,
         Q s, Q t, Q r, int n, T *result)
{
    Q s1 = Q(1) - s;
    Q t1 = Q(1) - t;
    Q r1 = Q(1) - r;
    for (int i = 0;  i < n;  ++i)
        result[i] = T (r1*(t1*(v0[i]*s1 + v1[i]*s) + t*(v2[i]*s1 + v3[i]*s)) +
                        r*(t1*(v4[i]*s1 + v5[i]*s) + t*(v6[i]*s1 + v7[i]*s)));
}



/// Trilinearly interoplate arrays of values v0-v7 (v0 upper left top, v1
/// upper right top, ...) at coordinates (s,t,r),
/// SCALING the interpolated value by 'scale' and then ADDING to
/// 'result'.  These are all vectors, so do it for each of 'n'
/// contiguous values (using the same s,t,r interpolants).
template <class T, class Q>
inline void
trilerp_mad (const T *v0, const T *v1, const T *v2, const T *v3,
             const T *v4, const T *v5, const T *v6, const T *v7,
             Q s, Q t, Q r, Q scale, int n, T *result)
{
    Q r1 = Q(1) - r;
    bilerp_mad (v0, v1, v2, v3, s, t, scale*r1, n, result);
    bilerp_mad (v4, v5, v6, v7, s, t, scale*r, n, result);
}



/// Evaluate B-spline weights in w[0..3] for the given fraction.  This
/// is an important component of performing a cubic interpolation.
template <typename T>
inline void evalBSplineWeights (T w[4], T fraction)
{
    T one_frac = 1 - fraction;
    w[0] = T(1.0 / 6.0) * one_frac * one_frac * one_frac;
    w[1] = T(2.0 / 3.0) - T(0.5) * fraction * fraction * (2 - fraction);
    w[2] = T(2.0 / 3.0) - T(0.5) * one_frac * one_frac * (2 - one_frac);
    w[3] = T(1.0 / 6.0) * fraction * fraction * fraction;
}


/// Evaluate B-spline derivative weights in w[0..3] for the given
/// fraction.  This is an important component of performing a cubic
/// interpolation with derivatives.
template <typename T>
inline void evalBSplineWeightDerivs (T dw[4], T fraction)
{
    T one_frac = 1 - fraction;
    dw[0] = -T(0.5) * one_frac * one_frac;
    dw[1] =  T(0.5) * fraction * (3 * fraction - 4);
    dw[2] = -T(0.5) * one_frac * (3 * one_frac - 4);
    dw[3] =  T(0.5) * fraction * fraction;
}



/// Bicubically interoplate arrays of pointers arranged in a 4x4 pattern
/// with val[0] pointing to the data in the upper left corner, val[15]
/// pointing to the lower right) at coordinates (s,t), storing the
/// results in 'result'.  These are all vectors, so do it for each of
/// 'n' contiguous values (using the same s,t interpolants).
template <class T>
inline void
bicubic_interp (const T **val, T s, T t, int n, T *result)
{
    for (int c = 0;  c < n;  ++c)
        result[c] = T(0);
    T wx[4]; evalBSplineWeights (wx, s);
    T wy[4]; evalBSplineWeights (wy, t);
    for (int j = 0;  j < 4;  ++j) {
        for (int i = 0;  i < 4;  ++i) {
            T w = wx[i] * wy[j];
            for (int c = 0;  c < n;  ++c)
                result[c] += w * val[j*4+i][c];
        }
    }
}



/// Return floor(x) as an int, as efficiently as possible.
inline int
ifloor (float x)
{
    // Find the greatest whole number <= x.  This cast is faster than
    // calling floorf.
    return (int) x - (x < 0.0f ? 1 : 0);
}



/// Return (x-floor(x)) and put (int)floor(x) in *xint.  This is similar
/// to the built-in modf, but returns a true int, always rounds down
/// (compared to modf which rounds toward 0), and always returns
/// frac >= 0 (comapred to modf which can return <0 if x<0).
inline float
floorfrac (float x, int *xint)
{
    int i = ifloor(x);
    *xint = i;
    return x - static_cast<float>(i);   // Return the fraction left over
}



/// Convert degrees to radians.
template <typename T>
inline T radians (T deg) { return deg * T(M_PI / 180.0); }

/// Convert radians to degrees
template <typename T>
inline T degrees (T rad) { return rad * T(180.0 / M_PI); }



inline void
sincos (float x, float* sine, float* cosine)
{
#if defined(__GNUC__) && defined(__linux__) && !defined(__clang__)
    __builtin_sincosf(x, sine, cosine);
#else
    *sine = std::sin(x);
    *cosine = std::cos(x);
#endif
}

inline void
sincos (double x, double* sine, double* cosine)
{
#if defined(__GNUC__) && defined(__linux__) && !defined(__clang__)
    __builtin_sincos(x, sine, cosine);
#else
    *sine = std::sin(x);
    *cosine = std::cos(x);
#endif
}


// (end of float helper functions)
////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
// CONVERSION
//
// Type and range conversion helper functions and classes.


template <typename IN_TYPE, typename OUT_TYPE>
inline OUT_TYPE bit_cast (const IN_TYPE in) {
    // NOTE: this is the only standards compliant way of doing this type of casting,
    // luckily the compilers we care about know how to optimize away this idiom.
    OUT_TYPE out;
    memcpy (&out, &in, sizeof(IN_TYPE));
    return out;
}


inline int bitcast_to_int (float x) { return bit_cast<float,int>(x); }
inline float bitcast_to_float (int x) { return bit_cast<int,float>(x); }



/// Change endian-ness of one or more data items that are each 2, 4,
/// or 8 bytes.  This should work for any of short, unsigned short, int,
/// unsigned int, float, long long, pointers.
template<class T>
inline void
swap_endian (T *f, int len=1)
{
    for (char *c = (char *) f;  len--;  c += sizeof(T)) {
        if (sizeof(T) == 2) {
            std::swap (c[0], c[1]);
        } else if (sizeof(T) == 4) {
            std::swap (c[0], c[3]);
            std::swap (c[1], c[2]);
        } else if (sizeof(T) == 8) {
            std::swap (c[0], c[7]);
            std::swap (c[1], c[6]);
            std::swap (c[2], c[5]);
            std::swap (c[3], c[4]);
        }
    }
}



// big_enough_float<T>::float_t is a floating-point type big enough to
// handle the range and precision of a <T>. It's a float, unless T is big.
template <typename T> struct big_enough_float    { typedef float float_t; };
template<> struct big_enough_float<int>          { typedef double float_t; };
template<> struct big_enough_float<unsigned int> { typedef double float_t; };
template<> struct big_enough_float<int64_t>      { typedef double float_t; };
template<> struct big_enough_float<uint64_t>     { typedef double float_t; };
template<> struct big_enough_float<double>       { typedef double float_t; };


/// Multiply src by scale, clamp to [min,max], and round to the nearest D
/// (presumed to be integer).  This is just a helper for the convert_type
/// templates, it probably has no other use.
template<typename S, typename D, typename F>
inline D
scaled_conversion (const S &src, F scale, F min, F max)
{
    if (std::numeric_limits<S>::is_signed) {
        F s = src * scale;
        s += (s < 0 ? (F)-0.5 : (F)0.5);
        return (D) clamp (s, min, max);
    } else {
        return (D) clamp ((F)src * scale + (F)0.5, min, max);
    }
}



/// Convert n consecutive values from the type of S to the type of D.
/// The conversion is not a simple cast, but correctly remaps the
/// 0.0->1.0 range from and to the full positive range of integral
/// types.  Take a memcpy shortcut if both types are the same and no
/// conversion is necessary.  Optional arguments can give nonstandard
/// quantizations.
//
// FIXME: make table-based specializations for common types with only a
// few possible src values (like unsigned char -> float).
template<typename S, typename D>
void convert_type (const S *src, D *dst, size_t n, D _min, D _max)
{
    if (is_same<S,D>::value) {
        // They must be the same type.  Just memcpy.
        memcpy (dst, src, n*sizeof(D));
        return;
    }
    typedef typename big_enough_float<D>::float_t F;
    F scale = std::numeric_limits<S>::is_integer ?
        ((F)1.0)/std::numeric_limits<S>::max() : (F)1.0;
    if (std::numeric_limits<D>::is_integer) {
        // Converting to an integer-like type.
        F min = (F)_min;  // std::numeric_limits<D>::min();
        F max = (F)_max;  // std::numeric_limits<D>::max();
        scale *= _max;
        // Unroll loop for speed
        for ( ; n >= 16; n -= 16) {
            *dst++ = scaled_conversion<S,D,F> (*src++, scale, min, max);
            *dst++ = scaled_conversion<S,D,F> (*src++, scale, min, max);
            *dst++ = scaled_conversion<S,D,F> (*src++, scale, min, max);
            *dst++ = scaled_conversion<S,D,F> (*src++, scale, min, max);
            *dst++ = scaled_conversion<S,D,F> (*src++, scale, min, max);
            *dst++ = scaled_conversion<S,D,F> (*src++, scale, min, max);
            *dst++ = scaled_conversion<S,D,F> (*src++, scale, min, max);
            *dst++ = scaled_conversion<S,D,F> (*src++, scale, min, max);
            *dst++ = scaled_conversion<S,D,F> (*src++, scale, min, max);
            *dst++ = scaled_conversion<S,D,F> (*src++, scale, min, max);
            *dst++ = scaled_conversion<S,D,F> (*src++, scale, min, max);
            *dst++ = scaled_conversion<S,D,F> (*src++, scale, min, max);
            *dst++ = scaled_conversion<S,D,F> (*src++, scale, min, max);
            *dst++ = scaled_conversion<S,D,F> (*src++, scale, min, max);
            *dst++ = scaled_conversion<S,D,F> (*src++, scale, min, max);
            *dst++ = scaled_conversion<S,D,F> (*src++, scale, min, max);
        }
        while (n--)
            *dst++ = scaled_conversion<S,D,F> (*src++, scale, min, max);
    } else {
        // Converting to a float-like type, so we don't need to remap
        // the range
        // Unroll loop for speed
        for ( ; n >= 16; n -= 16) {
            *dst++ = (D)((*src++) * scale);
            *dst++ = (D)((*src++) * scale);
            *dst++ = (D)((*src++) * scale);
            *dst++ = (D)((*src++) * scale);
            *dst++ = (D)((*src++) * scale);
            *dst++ = (D)((*src++) * scale);
            *dst++ = (D)((*src++) * scale);
            *dst++ = (D)((*src++) * scale);
            *dst++ = (D)((*src++) * scale);
            *dst++ = (D)((*src++) * scale);
            *dst++ = (D)((*src++) * scale);
            *dst++ = (D)((*src++) * scale);
            *dst++ = (D)((*src++) * scale);
            *dst++ = (D)((*src++) * scale);
            *dst++ = (D)((*src++) * scale);
            *dst++ = (D)((*src++) * scale);
        }
        while (n--)
            *dst++ = (D)((*src++) * scale);
    }
}



template<>
inline void convert_type<uint8_t,float> (const uint8_t *src,
                                         float *dst, size_t n,
                                         float _min, float _max)
{
    float scale (1.0f/std::numeric_limits<uint8_t>::max());
    simd::float4 scale_simd (scale);
    for ( ; n >= 4; n -= 4, src += 4, dst += 4) {
        simd::float4 s_simd (src);
        simd::float4 d_simd = s_simd * scale_simd;
        d_simd.store (dst);
    }
    while (n--)
        *dst++ = (*src++) * scale;
}



template<>
inline void convert_type<uint16_t,float> (const uint16_t *src,
                                          float *dst, size_t n,
                                          float _min, float _max)
{
    float scale (1.0f/std::numeric_limits<uint16_t>::max());
    simd::float4 scale_simd (scale);
    for ( ; n >= 4; n -= 4, src += 4, dst += 4) {
        simd::float4 s_simd (src);
        simd::float4 d_simd = s_simd * scale_simd;
        d_simd.store (dst);
    }
    while (n--)
        *dst++ = (*src++) * scale;
}


#ifdef _HALF_H_
template<>
inline void convert_type<half,float> (const half *src,
                                      float *dst, size_t n,
                                      float _min, float _max)
{
    for ( ; n >= 4; n -= 4, src += 4, dst += 4) {
        simd::float4 s_simd (src);
        s_simd.store (dst);
    }
    while (n--)
        *dst++ = (*src++);
}
#endif



template<>
inline void
convert_type<float,uint16_t> (const float *src, uint16_t *dst, size_t n,
                              uint16_t _min, uint16_t _max)
{
    float min = std::numeric_limits<uint16_t>::min();
    float max = std::numeric_limits<uint16_t>::max();
    float scale = max;
    simd::float4 max_simd (max);
    simd::float4 one_half_simd (0.5f);
    simd::float4 zero_simd (0.0f);
    for ( ; n >= 4; n -= 4, src += 4, dst += 4) {
        simd::float4 scaled = simd::round (simd::float4(src) * max_simd);
        simd::float4 clamped = clamp (scaled, zero_simd, max_simd);
        simd::int4 i (clamped);
        i.store (dst);
    }
    while (n--)
        *dst++ = scaled_conversion<float,uint16_t,float> (*src++, scale, min, max);
}


template<>
inline void
convert_type<float,uint8_t> (const float *src, uint8_t *dst, size_t n,
                             uint8_t _min, uint8_t _max)
{
    float min = std::numeric_limits<uint8_t>::min();
    float max = std::numeric_limits<uint8_t>::max();
    float scale = max;
    simd::float4 max_simd (max);
    simd::float4 one_half_simd (0.5f);
    simd::float4 zero_simd (0.0f);
    for ( ; n >= 4; n -= 4, src += 4, dst += 4) {
        simd::float4 scaled = simd::round (simd::float4(src) * max_simd);
        simd::float4 clamped = clamp (scaled, zero_simd, max_simd);
        simd::int4 i (clamped);
        i.store (dst);
    }
    while (n--)
        *dst++ = scaled_conversion<float,uint8_t,float> (*src++, scale, min, max);
}


#ifdef _HALF_H_
template<>
inline void
convert_type<float,half> (const float *src, half *dst, size_t n,
                          half _min, half _max)
{
    for ( ; n >= 4; n -= 4, src += 4, dst += 4) {
        simd::float4 s (src);
        s.store (dst);
    }
    while (n--)
        *dst++ = *src++;
}
#endif



template<typename S, typename D>
inline void convert_type (const S *src, D *dst, size_t n)
{
    convert_type<S,D> (src, dst, n,
                       std::numeric_limits<D>::min(),
                       std::numeric_limits<D>::max());
}




/// Convert a single value from the type of S to the type of D.
/// The conversion is not a simple cast, but correctly remaps the
/// 0.0->1.0 range from and to the full positive range of integral
/// types.  Take a copy shortcut if both types are the same and no
/// conversion is necessary.
template<typename S, typename D>
inline D
convert_type (const S &src)
{
    if (is_same<S,D>::value) {
        // They must be the same type.  Just return it.
        return (D)src;
    }
    typedef typename big_enough_float<D>::float_t F;
    F scale = std::numeric_limits<S>::is_integer ?
        ((F)1.0)/std::numeric_limits<S>::max() : (F)1.0;
    if (std::numeric_limits<D>::is_integer) {
        // Converting to an integer-like type.
        F min = (F) std::numeric_limits<D>::min();
        F max = (F) std::numeric_limits<D>::max();
        scale *= max;
        return scaled_conversion<S,D,F> (src, scale, min, max);
    } else {
        // Converting to a float-like type, so we don't need to remap
        // the range
        return (D)((F)src * scale);
    }
}



/// Helper function to convert channel values between different bit depths.
/// Roughly equivalent to:
///
/// out = round (in * (pow (2, TO_BITS) - 1) / (pow (2, FROM_BITS) - 1));
///
/// but utilizes an integer math trick for speed. It can be proven that the
/// absolute error of this method is less or equal to 1, with an average error
/// (with respect to the entire domain) below 0.2.
///
/// It is assumed that the original value is a valid FROM_BITS integer, i.e.
/// shifted fully to the right.
template<unsigned int FROM_BITS, unsigned int TO_BITS>
inline unsigned int bit_range_convert(unsigned int in) {
    unsigned int out = 0;
    int shift = TO_BITS - FROM_BITS;
    for (; shift > 0; shift -= FROM_BITS)
        out |= in << shift;
    out |= in >> -shift;
    return out;
}



// non-templated version.  Slow but general
inline unsigned int
bit_range_convert(unsigned int in, unsigned int FROM_BITS, unsigned int TO_BITS)
{
    unsigned int out = 0;
    int shift = TO_BITS - FROM_BITS;
    for (; shift > 0; shift -= FROM_BITS)
        out |= in << shift;
    out |= in >> -shift;
    return out;
}



/// A DataProxy<I,E> looks like an (E &), but it really holds an (I &)
/// and does conversions (via convert_type) as it reads in and out.
/// (I and E are for INTERNAL and EXTERNAL data types, respectively).
template<typename I, typename E>
struct DataProxy {
    DataProxy (I &data) : m_data(data) { }
    E operator= (E newval) { m_data = convert_type<E,I>(newval); return newval; }
    operator E () const { return convert_type<I,E>(m_data); }
private:
    DataProxy& operator = (const DataProxy&); // Do not implement
    I &m_data;
};



/// A ConstDataProxy<I,E> looks like a (const E &), but it really holds
/// a (const I &) and does conversions (via convert_type) as it reads.
/// (I and E are for INTERNAL and EXTERNAL data types, respectively).
template<typename I, typename E>
struct ConstDataProxy {
    ConstDataProxy (const I &data) : m_data(data) { }
    operator E () const { return convert_type<E,I>(*m_data); }
private:
    const I &m_data;
};



/// A DataArrayProxy<I,E> looks like an (E *), but it really holds an (I *)
/// and does conversions (via convert_type) as it reads in and out.
/// (I and E are for INTERNAL and EXTERNAL data types, respectively).
template<typename I, typename E>
struct DataArrayProxy {
    DataArrayProxy (I *data=NULL) : m_data(data) { }
    E operator* () const { return convert_type<I,E>(*m_data); }
    E operator[] (int i) const { return convert_type<I,E>(m_data[i]); }
    DataProxy<I,E> operator[] (int i) { return DataProxy<I,E> (m_data[i]); }
    void set (I *data) { m_data = data; }
    I * get () const { return m_data; }
    const DataArrayProxy<I,E> & operator+= (int i) {
        m_data += i;  return *this;
    }
private:
    I *m_data;
};



/// A ConstDataArrayProxy<I,E> looks like an (E *), but it really holds an
/// (I *) and does conversions (via convert_type) as it reads in and out.
/// (I and E are for INTERNAL and EXTERNAL data types, respectively).
template<typename I, typename E>
struct ConstDataArrayProxy {
    ConstDataArrayProxy (const I *data=NULL) : m_data(data) { }
    E operator* () const { return convert_type<I,E>(*m_data); }
    E operator[] (int i) const { return convert_type<I,E>(m_data[i]); }
    void set (const I *data) { m_data = data; }
    const I * get () const { return m_data; }
    const ConstDataArrayProxy<I,E> & operator+= (int i) {
        m_data += i;  return *this;
    }
private:
    const I *m_data;
};



/// Fast table-based conversion of 8-bit to other types.  Declare this
/// as static to avoid the expensive ctr being called all the time.
template <class T=float>
class EightBitConverter {
public:
    EightBitConverter () { init(); }
    T operator() (unsigned char c) const { return val[c]; }
private:
    T val[256];
    void init () {
        float scale = 1.0f / 255.0f;
        if (std::numeric_limits<T>::is_integer)
            scale *= (float)std::numeric_limits<T>::max();
        for (int i = 0;  i < 256;  ++i)
            val[i] = (T)(i * scale);
    }
};



/// Simple conversion of a (presumably non-negative) float into a
/// rational.  This does not attempt to find the simplest fraction
/// that approximates the float, for example 52.83 will simply
/// return 5283/100.  This does not attempt to gracefully handle
/// floats that are out of range that could be easily int/int.
inline void
float_to_rational (float f, unsigned int &num, unsigned int &den)
{
    if (f <= 0) {   // Trivial case of zero, and handle all negative values
        num = 0;
        den = 1;
    } else if ((int)(1.0/f) == (1.0/f)) { // Exact results for perfect inverses
        num = 1;
        den = (int)f;
    } else {
        num = (int)f;
        den = 1;
        while (fabsf(f-static_cast<float>(num)) > 0.00001f && den < 1000000) {
            den *= 10;
            f *= 10;
            num = (int)f;
        }
    }
}



/// Simple conversion of a float into a rational.  This does not attempt
/// to find the simplest fraction that approximates the float, for
/// example 52.83 will simply return 5283/100.  This does not attempt to
/// gracefully handle floats that are out of range that could be easily
/// int/int.
inline void
float_to_rational (float f, int &num, int &den)
{
    unsigned int n, d;
    float_to_rational (fabsf(f), n, d);
    num = (f >= 0) ? (int)n : -(int)n;
    den = (int) d;
}


// (end of conversion helpers)
////////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
// SAFE MATH
//
// The functions named "safe_*" are versions with various internal clamps
// or other deviations from IEEE standards with the specific intent of
// never producing NaN or Inf values or throwing exceptions. But within the
// valid range, they should be full precision and match IEEE standards.
//


/// Safe (clamping) sqrt: safe_sqrt(x<0) returns 0, not NaN.
template <typename T>
inline T safe_sqrt (T x) {
    return x >= T(0) ? std::sqrt(x) : T(0);
}

/// Safe (clamping) inverse sqrt: safe_inversesqrt(x<=0) returns 0.
template <typename T>
inline T safe_inversesqrt (T x) {
    return x > T(0) ? T(1) / std::sqrt(x) : T(0);
}


/// Safe (clamping) arcsine: clamp to the valid domain.
template <typename T>
inline T safe_asin (T x) {
    if (x <= T(-1)) return T(-M_PI_2);
    if (x >= T(+1)) return T(+M_PI_2);
    return std::asin(x);
}

/// Safe (clamping) arccosine: clamp to the valid domain.
template <typename T>
inline T safe_acos (T x) {
    if (x <= T(-1)) return T(M_PI);
    if (x >= T(+1)) return T(0);
    return std::acos(x);
}


/// Safe log2: clamp to valid domain.
template <typename T>
inline T safe_log2 (T x) {
    // match clamping from fast version
    if (x < std::numeric_limits<T>::min()) x = std::numeric_limits<T>::min();
    if (x > std::numeric_limits<T>::max()) x = std::numeric_limits<T>::max();
#if OIIO_CPLUSPLUS_VERSION >= 11
    return std::log2(x);
#else
    return log2f(x);   // punt: just use the float one
#endif
}

/// Safe log: clamp to valid domain.
template <typename T>
inline T safe_log (T x) {
    // slightly different than fast version since clamping happens before scaling
    if (x < std::numeric_limits<T>::min()) x = std::numeric_limits<T>::min();
    if (x > std::numeric_limits<T>::max()) x = std::numeric_limits<T>::max();
    return std::log(x);
}

/// Safe log10: clamp to valid domain.
template <typename T>
inline T safe_log10 (T x) {
    // slightly different than fast version since clamping happens before scaling
    if (x < std::numeric_limits<T>::min()) x = std::numeric_limits<T>::min();
    if (x > std::numeric_limits<T>::max()) x = std::numeric_limits<T>::max();
    return log10f(x);
}

/// Safe logb: clamp to valid domain.
template <typename T>
inline T safe_logb (T x) {
#if OIIO_CPLUSPLUS_VERSION >= 11
    return (x != T(0)) ? std::logb(x) : -std::numeric_limits<T>::max();
#else
    return (x != T(0)) ? logbf(x) : -std::numeric_limits<T>::max();
#endif
}

/// Safe pow: clamp the domain so it never returns Inf or NaN or has divide
/// by zero error.
template <typename T>
inline T safe_pow (T x, T y) {
    if (y == T(0)) return T(1);
    if (x == T(0)) return T(0);
    // if x is negative, only deal with integer powers
    if ((x < T(0)) && (y != floor(y))) return T(0);
    // FIXME: this does not match "fast" variant because clamping limits are different
    T r = std::pow(x, y);
    // Clamp to avoid returning Inf.
    const T big = std::numeric_limits<T>::max();
    return clamp (r, -big, big);
}

// (end of safe_* functions)
////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
// FAST & APPROXIMATE MATH
//
// The functions named "fast_*" provide a set of replacements to libm that
// are much faster at the expense of some accuracy and robust handling of
// extreme values. One design goal for these approximation was to avoid
// branches as much as possible and operate on single precision values only
// so that SIMD versions should be straightforward ports We also try to
// implement "safe" semantics (ie: clamp to valid range where possible)
// natively since wrapping these inline calls in another layer would be
// wasteful.
//
// Some functions are fast_safe_*, which is both a faster approximation as
// well as clamped input domain to ensure no NaN, Inf, or divide by zero.
//


/// Round to nearest integer, returning as an int.
inline int fast_rint (float x) {
    // used by sin/cos/tan range reduction
#if OIIO_SIMD_SSE >= 4
    // single roundps instruction on SSE4.1+ (for gcc/clang at least)
    return static_cast<int>(rintf(x));
#else
    // emulate rounding by adding/substracting 0.5
    return static_cast<int>(x + copysignf(0.5f, x));
#endif
}

inline simd::int4 fast_rint (const simd::float4& x) {
    return simd::rint (x);
}


inline float fast_sin (float x) {
    // very accurate argument reduction from SLEEF
    // starts failing around x=262000
    // Results on: [-2pi,2pi]
    // Examined 2173837240 values of sin: 0.00662760244 avg ulp diff, 2 max ulp, 1.19209e-07 max error
    int q = fast_rint (x * float(M_1_PI));
    float qf = q;
    x = madd(qf, -0.78515625f*4, x);
    x = madd(qf, -0.00024187564849853515625f*4, x);
    x = madd(qf, -3.7747668102383613586e-08f*4, x);
    x = madd(qf, -1.2816720341285448015e-12f*4, x);
    x = float(M_PI_2) - (float(M_PI_2) - x); // crush denormals
    float s = x * x;
    if ((q & 1) != 0) x = -x;
    // this polynomial approximation has very low error on [-pi/2,+pi/2]
    // 1.19209e-07 max error in total over [-2pi,+2pi]
    float u = 2.6083159809786593541503e-06f;
    u = madd(u, s, -0.0001981069071916863322258f);
    u = madd(u, s, +0.00833307858556509017944336f);
    u = madd(u, s, -0.166666597127914428710938f);
    u = madd(s, u * x, x);
    // For large x, the argument reduction can fail and the polynomial can be
    // evaluated with arguments outside the valid internal. Just clamp the bad
    // values away (setting to 0.0f means no branches need to be generated).
    if (fabsf(u) > 1.0f) u = 0.0f;
    return u;
}


inline float fast_cos (float x) {
    // same argument reduction as fast_sin
    int q = fast_rint (x * float(M_1_PI));
    float qf = q;
    x = madd(qf, -0.78515625f*4, x);
    x = madd(qf, -0.00024187564849853515625f*4, x);
    x = madd(qf, -3.7747668102383613586e-08f*4, x);
    x = madd(qf, -1.2816720341285448015e-12f*4, x);
    x = float(M_PI_2) - (float(M_PI_2) - x); // crush denormals
    float s = x * x;
    // polynomial from SLEEF's sincosf, max error is
    // 4.33127e-07 over [-2pi,2pi] (98% of values are "exact")
    float u = -2.71811842367242206819355e-07f;
    u = madd(u, s, +2.47990446951007470488548e-05f);
    u = madd(u, s, -0.00138888787478208541870117f);
    u = madd(u, s, +0.0416666641831398010253906f);
    u = madd(u, s, -0.5f);
    u = madd(u, s, +1.0f);
    if ((q & 1) != 0) u = -u;
    if (fabsf(u) > 1.0f) u = 0.0f;
    return u;
}

inline void fast_sincos (float x, float* sine, float* cosine) {
    // same argument reduction as fast_sin
    int q = fast_rint (x * float(M_1_PI));
    float qf = q;
    x = madd(qf, -0.78515625f*4, x);
    x = madd(qf, -0.00024187564849853515625f*4, x);
    x = madd(qf, -3.7747668102383613586e-08f*4, x);
    x = madd(qf, -1.2816720341285448015e-12f*4, x);
    x = float(M_PI_2) - (float(M_PI_2) - x); // crush denormals
    float s = x * x;
    // NOTE: same exact polynomials as fast_sin and fast_cos above
    if ((q & 1) != 0) x = -x;
    float su = 2.6083159809786593541503e-06f;
    su = madd(su, s, -0.0001981069071916863322258f);
    su = madd(su, s, +0.00833307858556509017944336f);
    su = madd(su, s, -0.166666597127914428710938f);
    su = madd(s, su * x, x);
    float cu = -2.71811842367242206819355e-07f;
    cu = madd(cu, s, +2.47990446951007470488548e-05f);
    cu = madd(cu, s, -0.00138888787478208541870117f);
    cu = madd(cu, s, +0.0416666641831398010253906f);
    cu = madd(cu, s, -0.5f);
    cu = madd(cu, s, +1.0f);
    if ((q & 1) != 0) cu = -cu;
    if (fabsf(su) > 1.0f) su = 0.0f;
    if (fabsf(cu) > 1.0f) cu = 0.0f;
    *sine   = su;
    *cosine = cu;
}

// NOTE: this approximation is only valid on [-8192.0,+8192.0], it starts becoming
// really poor outside of this range because the reciprocal amplifies errors
inline float fast_tan (float x) {
    // derived from SLEEF implementation
    // note that we cannot apply the "denormal crush" trick everywhere because
    // we sometimes need to take the reciprocal of the polynomial
    int q = fast_rint (x * float(2 * M_1_PI));
    float qf = q;
    x = madd(qf, -0.78515625f*2, x);
    x = madd(qf, -0.00024187564849853515625f*2, x);
    x = madd(qf, -3.7747668102383613586e-08f*2, x);
    x = madd(qf, -1.2816720341285448015e-12f*2, x);
    if ((q & 1) == 0)
    x = float(M_PI_4) - (float(M_PI_4) - x); // crush denormals (only if we aren't inverting the result later)
    float s = x * x;
    float u = 0.00927245803177356719970703f;
    u = madd(u, s, 0.00331984995864331722259521f);
    u = madd(u, s, 0.0242998078465461730957031f);
    u = madd(u, s, 0.0534495301544666290283203f);
    u = madd(u, s, 0.133383005857467651367188f);
    u = madd(u, s, 0.333331853151321411132812f);
    u = madd(s, u * x, x);
    if ((q & 1) != 0) u = -1.0f / u;
    return u;
}

/// Fast, approximate sin(x*M_PI) with maximum absolute error of 0.000918954611.
/// Adapted from http://devmaster.net/posts/9648/fast-and-accurate-sine-cosine#comment-76773
/// Note that this is MUCH faster, but much less accurate than fast_sin.
inline float fast_sinpi (float x)
{
	// Fast trick to strip the integral part off, so our domain is [-1,1]
	const float z = x - ((x + 25165824.0f) - 25165824.0f);
    const float y = z - z * fabsf(z);
    const float Q = 3.10396624f;
    const float P = 3.584135056f; // P = 16-4*Q
    return y * (Q + P * fabsf(y));
    /* The original article used used inferior constants for Q and P and
     * so had max error 1.091e-3.
     *
     * The optimal value for Q was determined by exhaustive search, minimizing
     * the absolute numerical error relative to float(std::sin(double(phi*M_PI)))
     * over the interval [0,2] (which is where most of the invocations happen).
     * 
     * The basic idea of this approximation starts with the coarse approximation:
     *      sin(pi*x) ~= f(x) =  4 * (x - x * abs(x))
     *
     * This approximation always _over_ estimates the target. On the otherhand, the
     * curve:
     *      sin(pi*x) ~= f(x) * abs(f(x)) / 4
     *
     * always lies _under_ the target. Thus we can simply numerically search for the
     * optimal constant to LERP these curves into a more precise approximation.
     * After folding the constants together and simplifying the resulting math, we
     * end up with the compact implementation below.
     *
     * NOTE: this function actually computes sin(x * pi) which avoids one or two
     * mults in many cases and guarantees exact values at integer periods.
     */
}

/// Fast approximate cos(x*M_PI) with ~0.1% absolute error.
/// Note that this is MUCH faster, but much less accurate than fast_cos.
inline float fast_cospi (float x)
{
    return fast_sinpi (x+0.5f);
}

inline float fast_acos (float x) {
    const float f = fabsf(x);
    const float m = (f < 1.0f) ? 1.0f - (1.0f - f) : 1.0f; // clamp and crush denormals
    // based on http://www.pouet.net/topic.php?which=9132&page=2
    // 85% accurate (ulp 0)
    // Examined 2130706434 values of acos: 15.2000597 avg ulp diff, 4492 max ulp, 4.51803e-05 max error // without "denormal crush"
    // Examined 2130706434 values of acos: 15.2007108 avg ulp diff, 4492 max ulp, 4.51803e-05 max error // with "denormal crush"
    const float a = sqrtf(1.0f - m) * (1.5707963267f + m * (-0.213300989f + m * (0.077980478f + m * -0.02164095f)));
    return x < 0 ? float(M_PI) - a : a;
}

inline float fast_asin (float x) {
    // based on acosf approximation above
    // max error is 4.51133e-05 (ulps are higher because we are consistently off by a little amount)
    const float f = fabsf(x);
    const float m = (f < 1.0f) ? 1.0f - (1.0f - f) : 1.0f; // clamp and crush denormals
    const float a = float(M_PI_2) - sqrtf(1.0f - m) * (1.5707963267f + m * (-0.213300989f + m * (0.077980478f + m * -0.02164095f)));
    return copysignf(a, x);
}

inline float fast_atan (float x) {
    const float a = fabsf(x);
    const float k = a > 1.0f ? 1 / a : a;
    const float s = 1.0f - (1.0f - k); // crush denormals
    const float t = s * s;
    // http://mathforum.org/library/drmath/view/62672.html
    // Examined 4278190080 values of atan: 2.36864877 avg ulp diff, 302 max ulp, 6.55651e-06 max error      // (with  denormals)
    // Examined 4278190080 values of atan: 171160502 avg ulp diff, 855638016 max ulp, 6.55651e-06 max error // (crush denormals)
    float r = s * madd(0.43157974f, t, 1.0f) / madd(madd(0.05831938f, t, 0.76443945f), t, 1.0f);
    if (a > 1.0f) r = 1.570796326794896557998982f - r;
    return copysignf(r, x);
}

inline float fast_atan2 (float y, float x) {
    // based on atan approximation above
    // the special cases around 0 and infinity were tested explicitly
    // the only case not handled correctly is x=NaN,y=0 which returns 0 instead of nan
    const float a = fabsf(x);
    const float b = fabsf(y);

    const float k = (b == 0) ? 0.0f : ((a == b) ? 1.0f : (b > a ? a / b : b / a));
    const float s = 1.0f - (1.0f - k); // crush denormals
    const float t = s * s;

    float r = s * madd(0.43157974f, t, 1.0f) / madd(madd(0.05831938f, t, 0.76443945f), t, 1.0f);

    if (b > a) r = 1.570796326794896557998982f - r; // account for arg reduction
    if (bit_cast<float, unsigned>(x) & 0x80000000u) // test sign bit of x
        r = float(M_PI) - r;
    return copysignf(r, y);
}

template<typename T>
inline T fast_log2 (const T& xval) {
    using namespace simd;
    typedef typename T::int_t intN;
    // See float fast_log2 for explanations
    T x = clamp (xval, T(std::numeric_limits<float>::min()), T(std::numeric_limits<float>::max()));
    intN bits = bitcast_to_int(x);
    intN exponent = srl (bits, 23) - intN(127);
    T f = bitcast_to_float ((bits & intN(0x007FFFFF)) | intN(0x3f800000)) - T(1.0f);
    T f2 = f * f;
    T f4 = f2 * f2;
    T hi = madd(f, T(-0.00931049621349f), T( 0.05206469089414f));
    T lo = madd(f, T( 0.47868480909345f), T(-0.72116591947498f));
    hi = madd(f, hi, T(-0.13753123777116f));
    hi = madd(f, hi, T( 0.24187369696082f));
    hi = madd(f, hi, T(-0.34730547155299f));
    lo = madd(f, lo, T( 1.442689881667200f));
    return ((f4 * hi) + (f * lo)) + T(exponent);
}


template<>
inline float fast_log2 (const float& xval) {
    // NOTE: clamp to avoid special cases and make result "safe" from large negative values/nans
    float x = clamp (xval, std::numeric_limits<float>::min(), std::numeric_limits<float>::max());
    // based on https://github.com/LiraNuna/glsl-sse2/blob/master/source/vec4.h
    unsigned bits = bit_cast<float, unsigned>(x);
    int exponent = int(bits >> 23) - 127;
    float f = bit_cast<unsigned, float>((bits & 0x007FFFFF) | 0x3f800000) - 1.0f;
    // Examined 2130706432 values of log2 on [1.17549435e-38,3.40282347e+38]: 0.0797524457 avg ulp diff, 3713596 max ulp, 7.62939e-06 max error
    // ulp histogram:
    //  0  = 97.46%
    //  1  =  2.29%
    //  2  =  0.11%
    float f2 = f * f;
    float f4 = f2 * f2;
    float hi = madd(f, -0.00931049621349f,  0.05206469089414f);
    float lo = madd(f,  0.47868480909345f, -0.72116591947498f);
    hi = madd(f, hi, -0.13753123777116f);
    hi = madd(f, hi,  0.24187369696082f);
    hi = madd(f, hi, -0.34730547155299f);
    lo = madd(f, lo,  1.442689881667200f);
    return ((f4 * hi) + (f * lo)) + exponent;
}



template<typename T>
inline T fast_log (const T& x) {
    // Examined 2130706432 values of logf on [1.17549435e-38,3.40282347e+38]: 0.313865375 avg ulp diff, 5148137 max ulp, 7.62939e-06 max error
    return fast_log2(x) * T(M_LN2);
}


template<typename T>
inline T fast_log10 (const T& x) {
    // Examined 2130706432 values of log10f on [1.17549435e-38,3.40282347e+38]: 0.631237033 avg ulp diff, 4471615 max ulp, 3.8147e-06 max error
    return fast_log2(x) * T(M_LN2 / M_LN10);
}

inline float fast_logb (float x) {
    // don't bother with denormals
    x = fabsf(x);
    if (x < std::numeric_limits<float>::min()) x = std::numeric_limits<float>::min();
    if (x > std::numeric_limits<float>::max()) x = std::numeric_limits<float>::max();
    unsigned bits = bit_cast<float, unsigned>(x);
    return float (int(bits >> 23) - 127);
}

inline float fast_log1p (float x) {
    if (fabsf(x) < 0.01f) {
        float y = 1.0f - (1.0f - x); // crush denormals
        return copysignf(madd(-0.5f, y * y, y), x);
    } else {
        return fast_log(x + 1);
    }
}



template<typename T>
inline T fast_exp2 (const T& xval) {
    using namespace simd;
    typedef typename T::int_t intN;
#if OIIO_SIMD_SSE
    // See float specialization for explanations
    T x = clamp (xval, T(-126.0f), T(126.0f));
    intN m (x); x -= T(m);
    T one (1.0f);
    x = one - (one - x); // crush denormals (does not affect max ulps!)
    const T kA (1.33336498402e-3f);
    const T kB (9.810352697968e-3f);
    const T kC (5.551834031939e-2f);
    const T kD (0.2401793301105f);
    const T kE (0.693144857883f);
    T r (kA);
    r = madd(x, r, kB);
    r = madd(x, r, kC);
    r = madd(x, r, kD);
    r = madd(x, r, kE);
    r = madd(x, r, one);
    return bitcast_to_float (bitcast_to_int(r) + (m << 23));
#else
    T r;
    for (int i = 0; i < r.elements; ++i)
        r[i] = fast_exp2(xval[i]);
    for (int i = r.elements; i < r.paddedelements; ++i)
        r[i] = 0.0f;
    return r;
#endif
}


template<>
inline float fast_exp2 (const float& xval) {
    // clamp to safe range for final addition
    float x = clamp (xval, -126.0f, 126.0f);
    // range reduction
    int m = int(x); x -= m;
    x = 1.0f - (1.0f - x); // crush denormals (does not affect max ulps!)
    // 5th degree polynomial generated with sollya
    // Examined 2247622658 values of exp2 on [-126,126]: 2.75764912 avg ulp diff, 232 max ulp
    // ulp histogram:
    //  0  = 87.81%
    //  1  =  4.18%
    float r = 1.33336498402e-3f;
    r = madd(x, r, 9.810352697968e-3f);
    r = madd(x, r, 5.551834031939e-2f);
    r = madd(x, r, 0.2401793301105f);
    r = madd(x, r, 0.693144857883f);
    r = madd(x, r, 1.0f);
    // multiply by 2 ^ m by adding in the exponent
    // NOTE: left-shift of negative number is undefined behavior
    return bit_cast<unsigned, float>(bit_cast<float, unsigned>(r) + (unsigned(m) << 23));
}




template <typename T>
inline T fast_exp (const T& x) {
    // Examined 2237485550 values of exp on [-87.3300018,87.3300018]: 2.6666452 avg ulp diff, 230 max ulp
    return fast_exp2(x * T(1 / M_LN2));
}



/// Faster float exp than is in libm, but still 100% accurate
inline float fast_correct_exp (float x)
{
#if defined(__x86_64__) && defined(__GNU_LIBRARY__) && defined(__GLIBC__ ) && defined(__GLIBC_MINOR__) && __GLIBC__ <= 2 && __GLIBC_MINOR__ < 16
    // On x86_64, versions of glibc < 2.16 have an issue where expf is
    // much slower than the double version.  This was fixed in glibc 2.16.
    return static_cast<float>(std::exp(static_cast<double>(x)));
#else
    return std::exp(x);
#endif
}


inline float fast_exp10 (float x) {
    // Examined 2217701018 values of exp10 on [-37.9290009,37.9290009]: 2.71732409 avg ulp diff, 232 max ulp
    return fast_exp2(x * float(M_LN10 / M_LN2));
}

inline float fast_expm1 (float x) {
    if (fabsf(x) < 0.03f) {
        float y = 1.0f - (1.0f - x); // crush denormals
        return copysignf(madd(0.5f, y * y, y), x);
    } else
        return fast_exp(x) - 1.0f;
}

inline float fast_sinh (float x) {
    float a = fabsf(x);
    if (a > 1.0f) {
        // Examined 53389559 values of sinh on [1,87.3300018]: 33.6886442 avg ulp diff, 178 max ulp
        float e = fast_exp(a);
        return copysignf(0.5f * e - 0.5f / e, x);
    } else {
        a = 1.0f - (1.0f - a); // crush denorms
        float a2 = a * a;
        // degree 7 polynomial generated with sollya
        // Examined 2130706434 values of sinh on [-1,1]: 1.19209e-07 max error
        float r = 2.03945513931e-4f;
        r = madd(r, a2, 8.32990277558e-3f);
        r = madd(r, a2, 0.1666673421859f);
        r = madd(r * a, a2, a);
        return copysignf(r, x);
    }
}

inline float fast_cosh (float x) {
    // Examined 2237485550 values of cosh on [-87.3300018,87.3300018]: 1.78256726 avg ulp diff, 178 max ulp
    float e = fast_exp(fabsf(x));
    return 0.5f * e + 0.5f / e;
}

inline float fast_tanh (float x) {
    // Examined 4278190080 values of tanh on [-3.40282347e+38,3.40282347e+38]: 3.12924e-06 max error
    // NOTE: ulp error is high because of sub-optimal handling around the origin
    float e = fast_exp(2.0f * fabsf(x));
    return copysignf(1 - 2 / (1 + e), x);
}

inline float fast_safe_pow (float x, float y) {
    if (y == 0) return 1.0f; // x^0=1
    if (x == 0) return 0.0f; // 0^y=0
    // be cheap & exact for special case of squaring and identity
    if (y == 1.0f)
        return x;
    if (y == 2.0f)
        return std::min (x*x, std::numeric_limits<float>::max());
    float sign = 1.0f;
    if (x < 0) {
        // if x is negative, only deal with integer powers
        // powf returns NaN for non-integers, we will return 0 instead
        int ybits = bit_cast<float, int>(y) & 0x7fffffff;
        if (ybits >= 0x4b800000) {
            // always even int, keep positive
        } else if (ybits >= 0x3f800000) {
            // bigger than 1, check
            int k = (ybits >> 23) - 127;  // get exponent
            int j =  ybits >> (23 - k);   // shift out possible fractional bits
            if ((j << (23 - k)) == ybits) // rebuild number and check for a match
                sign = bit_cast<int, float>(0x3f800000 | (j << 31)); // +1 for even, -1 for odd
            else
                return 0.0f; // not integer
        } else {
            return 0.0f; // not integer
        }
    }
    return sign * fast_exp2(y * fast_log2(fabsf(x)));
}


// Fast simd pow that only needs to work for positive x
template<typename T, typename U>
inline T fast_pow_pos (const T& x, const U& y) {
    return fast_exp2(y * fast_log2(x));
}


inline float fast_erf (float x)
{
    // Examined 1082130433 values of erff on [0,4]: 1.93715e-06 max error
    // Abramowitz and Stegun, 7.1.28
    const float a1 = 0.0705230784f;
    const float a2 = 0.0422820123f;
    const float a3 = 0.0092705272f;
    const float a4 = 0.0001520143f;
    const float a5 = 0.0002765672f;
    const float a6 = 0.0000430638f;
    const float a = fabsf(x);
    const float b = 1.0f - (1.0f - a); // crush denormals
    const float r = madd(madd(madd(madd(madd(madd(a6, b, a5), b, a4), b, a3), b, a2), b, a1), b, 1.0f);
    const float s = r * r; // ^2
    const float t = s * s; // ^4
    const float u = t * t; // ^8
    const float v = u * u; // ^16
    return copysignf(1.0f - 1.0f / v, x);
}

inline float fast_erfc (float x)
{
    // Examined 2164260866 values of erfcf on [-4,4]: 1.90735e-06 max error
    // ulp histogram:
    //   0  = 80.30%
    return 1.0f - fast_erf(x);
}

inline float fast_ierf (float x)
{
    // from: Approximating the erfinv function by Mike Giles
    // to avoid trouble at the limit, clamp input to 1-eps
    float a = fabsf(x); if (a > 0.99999994f) a = 0.99999994f;
    float w = -fast_log((1.0f - a) * (1.0f + a)), p;
    if (w < 5.0f) {
        w = w - 2.5f;
        p =  2.81022636e-08f;
        p = madd(p, w,  3.43273939e-07f);
        p = madd(p, w, -3.5233877e-06f );
        p = madd(p, w, -4.39150654e-06f);
        p = madd(p, w,  0.00021858087f );
        p = madd(p, w, -0.00125372503f );
        p = madd(p, w, -0.00417768164f );
        p = madd(p, w,  0.246640727f   );
        p = madd(p, w,  1.50140941f    );
    } else {
        w = sqrtf(w) - 3.0f;
        p = -0.000200214257f;
        p = madd(p, w,  0.000100950558f);
        p = madd(p, w,  0.00134934322f );
        p = madd(p, w, -0.00367342844f );
        p = madd(p, w,  0.00573950773f );
        p = madd(p, w, -0.0076224613f  );
        p = madd(p, w,  0.00943887047f );
        p = madd(p, w,  1.00167406f    );
        p = madd(p, w,  2.83297682f    );
    }
    return p * x;
}

// (end of fast* functions)
////////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
// MISCELLANEOUS NUMERICAL METHODS
//


/// Solve for the x for which func(x) == y on the interval [xmin,xmax].
/// Use a maximum of maxiter iterations, and stop any time the remaining
/// search interval or the function evaluations <= eps.  If brack is
/// non-NULL, set it to true if y is in [f(xmin), f(xmax)], otherwise
/// false (in which case the caller should know that the results may be
/// unreliable.  Results are undefined if the function is not monotonic
/// on that interval or if there are multiple roots in the interval (it
/// may not converge, or may converge to any of the roots without
/// telling you that there are more than one).
template<class T, class Func>
T invert (Func &func, T y, T xmin=0.0, T xmax=1.0,
          int maxiters=32, T eps=1.0e-6, bool *brack=0)
{
    // Use the Regula Falsi method, falling back to bisection if it
    // hasn't converged after 3/4 of the maximum number of iterations.
    // See, e.g., Numerical Recipes for the basic ideas behind both
    // methods.
    T v0 = func(xmin), v1 = func(xmax);
    T x = xmin, v = v0;
    bool increasing = (v0 < v1);
    T vmin = increasing ? v0 : v1;
    T vmax = increasing ? v1 : v0;
    bool bracketed = (y >= vmin && y <= vmax);
    if (brack)
        *brack = bracketed;
    if (! bracketed) {
        // If our bounds don't bracket the zero, just give up, and
        // return the approprate "edge" of the interval
        return ((y < vmin) == increasing) ? xmin : xmax;
    }
    if (fabs(v0-v1) < eps)   // already close enough
        return x;
    int rfiters = (3*maxiters)/4;   // how many times to try regula falsi
    for (int iters = 0;  iters < maxiters;  ++iters) {
        T t;  // interpolation factor
        if (iters < rfiters) {
            // Regula falsi
            t = (y-v0)/(v1-v0);
            if (t <= T(0) || t >= T(1))
                t = T(0.5);  // RF convergence failure -- bisect instead
        } else {
            t = T(0.5);            // bisection
        }
        x = lerp (xmin, xmax, t);
        v = func(x);
        if ((v < y) == increasing) {
            xmin = x; v0 = v;
        } else {
            xmax = x; v1 = v;
        }
        if (fabs(xmax-xmin) < eps || fabs(v-y) < eps)
            return x;   // converged
    }
    return x;
}



/// Linearly interpolate a list of evenly-spaced knots y[0..len-1] with
/// y[0] corresponding to the value at x==0.0 and y[len-1] corresponding to
/// x==1.0.
inline float
interpolate_linear (float x, array_view_strided<const float> y)
{
    DASSERT_MSG (y.size() >= 2, "interpolate_linear needs at least 2 knot values (%zd)", y.size());
    x = clamp (x, float(0.0), float(1.0));
    int nsegs = int(y.size()) - 1;
    int segnum;
    x = floorfrac (x*nsegs, &segnum);
    int nextseg = std::min (segnum+1, nsegs);
    return lerp (y[segnum], y[nextseg], x);
}

// (end miscellaneous numerical methods)
////////////////////////////////////////////////////////////////////////////



OIIO_NAMESPACE_END
