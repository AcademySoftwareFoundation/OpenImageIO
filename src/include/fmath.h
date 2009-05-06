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


/// \file
///
/// A variety of floating-point math helper routines (and, slight
/// misnomer, some int stuff as well).
///

#ifndef FMATH_H
#define FMATH_H

#include <cmath>
#include <limits>

#if defined(_MSC_VER)
# ifndef _UINT64_T
   typedef unsigned __int32 uint32_t;
   typedef unsigned __int64 uint64_t;
#  define _UINT32_T
#  define _UINT64_T
# endif
#else
# include <stdint.h>
#endif



#ifndef M_PI
/// PI
///
#  define M_PI 3.1415926535897932
#endif

#ifndef M_PI_2
/// PI / 2
///
#  define M_PI_2 1.5707963267948966
#endif

#ifndef M_TWO_PI
/// PI * 2
///
#  define M_TWO_PI (M_PI * 2.0)
#endif

#ifndef M_SQRT2
/// sqrt(2)
///
#  define M_SQRT2 1.414135623730950
#endif

#ifndef M_SQRT1_2
/// 1/sqrt(2)
///
#  define M_SQRT1_2 0.7071067811865475
#endif

#ifndef M_LN2
/// ln(2)
///
#  define M_LN2 0.6931471805599453
#endif

#ifndef M_LN10
/// ln(10)
///
#  define M_LN10 2.3025850929940457
#endif



// Some stuff we know easily if we're running on an Intel chip
#if (defined(_WIN32) || defined(__i386__) || defined(__x86_64__))
#  define USE_INTEL_MATH_SHORTCUTS 1
#  ifndef __LITTLE_ENDIAN__
#    define __LITTLE_ENDIAN__ 1
#    undef __BIG_ENDIAN__
#  endif
#endif



/// Large constant that we use to indicate a really large float
///
#define HUGE_FLOAT ((float)1.0e38)

/// Test a float for whether it's huge.  To account for awful fp roundoff,
/// consider it large if within a factor of 2 of HUGE_FLOAT.
inline bool huge (float f) { return (f >= HUGE_FLOAT/2); }

/// Special value we can use for an uninitialized float.
///
#define UNINITIALIZED_FLOAT (- std::numeric_limits<float>::max())




/// Quick test for whether an integer is a power of 2.
///
inline bool
ispow2 (int x)
{
    // Numerous references for this bit trick are on the web.  The
    // principle is that x is a power of 2 <=> x == 1<<b <=> x-1 is
    // all 1 bits for bits < b.
    return (x & (x-1)) == 0 && (x >= 0);
}



/// Quick test for whether an unsigned integer is a power of 2.
///
inline bool
ispow2 (unsigned int x)
{
    // Numerous references for this bit trick are on the web.  The
    // principle is that x is a power of 2 <=> x == 1<<b <=> x-1 is
    // all 1 bits for bits < b.
    return (x & (x-1)) == 0;
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



/// Return true if the architecture we are running on is little endian
///
inline bool littleendian (void)
{
#ifdef __BIG_ENDIAN__
    return false;
#endif
#ifdef __LITTLE_ENDIAN__
    return true;
#endif
    // Otherwise, do something quick to compute it
    int i = 1;
    return *((char *) &i);
}



/// Return true if the architecture we are running on is big endian
///
inline bool bigendian (void)
{
    return ! littleendian();
}



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



/// clamp a to bounds [l,h].
///
template <class T>
inline T
clamp (T a, T l, T h)
{
    return (a < l)? l : ((a > h)? h : a);
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
void convert_type (const S *src, D *dst, size_t n, D _zero=0, D _one=1,
                   D _min=std::numeric_limits<D>::min(),
                   D _max=std::numeric_limits<D>::max())
{
    if (sizeof(D) == sizeof(S) &&
        std::numeric_limits<D>::min() == std::numeric_limits<S>::min() &&
        std::numeric_limits<D>::max() == std::numeric_limits<S>::max()) {
        // They must be the same type.  Just memcpy.
        memcpy (dst, src, n*sizeof(D));
        return;
    }
    typedef float F;
    F scale = std::numeric_limits<S>::is_integer ?
        ((F)1.0)/std::numeric_limits<S>::max() : (F)1.0;
    if (std::numeric_limits<D>::is_integer) {
        // Converting to an integer-like type.
        F min = (F)_min;  // std::numeric_limits<D>::min();
        F max = (F)_max;  // std::numeric_limits<D>::max();
        scale *= _max;
        // Unroll loop for speed
        for ( ; n >= 16; n -= 16) {
            *dst++ = (D)(clamp ((F)(*src++) * scale, min, max));
            *dst++ = (D)(clamp ((F)(*src++) * scale, min, max));
            *dst++ = (D)(clamp ((F)(*src++) * scale, min, max));
            *dst++ = (D)(clamp ((F)(*src++) * scale, min, max));
            *dst++ = (D)(clamp ((F)(*src++) * scale, min, max));
            *dst++ = (D)(clamp ((F)(*src++) * scale, min, max));
            *dst++ = (D)(clamp ((F)(*src++) * scale, min, max));
            *dst++ = (D)(clamp ((F)(*src++) * scale, min, max));
            *dst++ = (D)(clamp ((F)(*src++) * scale, min, max));
            *dst++ = (D)(clamp ((F)(*src++) * scale, min, max));
            *dst++ = (D)(clamp ((F)(*src++) * scale, min, max));
            *dst++ = (D)(clamp ((F)(*src++) * scale, min, max));
            *dst++ = (D)(clamp ((F)(*src++) * scale, min, max));
            *dst++ = (D)(clamp ((F)(*src++) * scale, min, max));
            *dst++ = (D)(clamp ((F)(*src++) * scale, min, max));
            *dst++ = (D)(clamp ((F)(*src++) * scale, min, max));
        }
        while (n--)
            *dst++ = (D)(clamp ((F)(*src++) * scale, min, max));
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



/// Bilinearly interoplate values v0-v3 (v0 upper left, v1 upper right,
/// v2 lower left, v3 lower right) at coordinates (s,t) and return the
/// result.  This is a template, and so should work for any types.
template <class T, class Q>
inline T
bilerp (T v0, T v1, T v2, T v3, Q s, Q t)
{
    // NOTE: a*(t-1) + b*t is much more numerically stable than a+t*(b-a)
    Q s1 = (Q)1 - s;
    return (T) ((1-t)*(v0*s1 + v1*s) + t*(v2*s1 + v3*s));
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
    Q s1 = (Q)1 - s;
    Q t1 = (Q)1 - t;
    for (int i = 0;  i < n;  ++i)
        result[i] = (T) (t1*(v0[i]*s1 + v1[i]*s) + t*(v2[i]*s1 + v3[i]*s));
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
    Q s1 = (Q)1 - s;
    Q t1 = (Q)1 - t;
    for (int i = 0;  i < n;  ++i)
        result[i] += (T) (scale * (t1*(v0[i]*s1 + v1[i]*s) + t*(v2[i]*s1 + v3[i]*s)));
}



/// Fast rounding to nearest integer.
/// See Michael Herf's "Know Your FPU" page:
/// http://www.stereopsis.com/sree/fpu2006.html
inline int
RoundToInt (double val)
{
#ifdef USE_INTEL_MATH_SHORTCUTS
    const double doublemagic = double (6755399441055744.0);
        // 2^52 * 1.5, uses limited precisicion to floor
    val += doublemagic;
    return ((int*)&val)[0];
#else
    return round (val);
#endif
}


inline int
RoundToInt (float val)
{
#ifdef USE_INTEL_MATH_SHORTCUTS
    return RoundToInt (double(val));
#else
    return roundf (val);
#endif
}


/// Fast (int)floor(val)
/// See Michael Herf's "Know Your FPU" page:
/// http://www.stereopsis.com/sree/fpu2006.html
inline int
FloorToInt (double val)
{
#ifdef USE_INTEL_MATH_SHORTCUTS
    const double doublemagicdelta = (1.5e-8);
        // almost .5f = .5f + 1e^(number of exp bit)
    const double doublemagicroundeps = (0.5f-doublemagicdelta);
        // almost .5f = .5f - 1e^(number of exp bit)
    return RoundToInt (val - doublemagicroundeps);
#else
    return (int) floor (val);
#endif
}


inline int
FloorToInt (float val)
{
#ifdef USE_INTEL_MATH_SHORTCUTS
    return FloorToInt  (double(val));
#else
    return (int) floorf (val);
#endif
}


/// Fast (int)ceil(val)
/// See Michael Herf's "Know Your FPU" page:
/// http://www.stereopsis.com/sree/fpu2006.html
inline int
CeilToInt (double val)
{
#ifdef USE_INTEL_MATH_SHORTCUTS
    const double doublemagicdelta = (1.5e-8);
        // almost .5f = .5f + 1e^(number of exp bit)
    const double doublemagicroundeps = (0.5f-doublemagicdelta);
        // almost .5f = .5f - 1e^(number of exp bit)
    return RoundToInt (val + doublemagicroundeps);
#else
    return (int) ceil (val);
#endif
}

inline int
CeilToInt (float val)
{
#ifdef USE_INTEL_MATH_SHORTCUTS
    return CeilToInt (double(val));
#else
    return (int) ceilf (val);
#endif
}

/// Fast (int)val
/// See Michael Herf's "Know Your FPU" page:
/// http://www.stereopsis.com/sree/fpu2006.html
inline int
FloatToInt (double val)
{
#ifdef USE_INTEL_MATH_SHORTCUTS
    return (val<0) ? CeilToInt(val) : FloorToInt(val);
#else
    return (int) val;
#endif
}


inline int
FloatToInt (float val)
{
#ifdef USE_INTEL_MATH_SHORTCUTS
    return FloatToInt (double(val));
#else
    return (int) val;
#endif
}




/// Return (x-floor(x)) and put (int)floor(x) in *xint.  This is similar
/// to the built-in modf, but returns a true int, always rounds down
/// (compared to modf which rounds toward 0), and always returns 
/// frac >= 0 (comapred to modf which can return <0 if x<0).
inline float
floorfrac (float x, int *xint)
{
    // Find the greatest whole number <= x.  This cast is faster than
    // calling floorf.
#if 1
    int i = (int) x - (x < 0.0f ? 1 : 0);
#else
    // Why isn't this faster than the cast?
    int i = FloorToInt (x);
#endif
    *xint = i;
    return x - i;   // Return the fraction left over
}



/// Convert degrees to radians.
///
inline float radians (float deg) { return deg * (float)(M_PI / 180.0); }

/// Convert radians to degrees
///
inline float degrees (float rad) { return rad * (float)(180.0 / M_PI); }



#ifdef _WIN32
// Windows doesn't define these functions from math.h
#define hypotf _hypotf
#define copysign(x,y) _copysign(x,y)
#define copysignf(x,y) copysign(x,y)
#define isnan(x) _isnan(x)
#define isfinite(x) _finite(x)
template<class T>
inline int isinf (T x) {
    return (isfinite(x)||isnan(x)) ? 0 : copysign(1.0f, x);
}
#endif



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
        while (fabsf(f-num) > 0.00001 && den < 1000000) {
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





#endif // FMATH_H
