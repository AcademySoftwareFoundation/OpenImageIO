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

  A few bits here are based upon code from NVIDIA that was also released
  under the same modified BSD license, and marked as:
     Copyright 2004 NVIDIA Corporation. All Rights Reserved.
*/


/// \file
///
/// A variety of floating-point math helper routines (and, slight
/// misnomer, some int stuff as well).
///

#ifndef OPENIMAGEIO_FMATH_H
#define OPENIMAGEIO_FMATH_H

#include <cmath>
#include <limits>
#include <typeinfo>
#include <algorithm>

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
#  include <stdint.h>
#endif

#if defined(__FreeBSD__)
#include <sys/param.h>
#endif

#include "oiioversion.h"

OIIO_NAMESPACE_ENTER
{

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

#ifndef M_1_PI
/// 1/PI
///
#  define M_1_PI 0.318309886183790671538
#endif

#ifndef M_2_PI
/// 2/PI
///
#  define M_2_PI 0.63661977236758134
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



/// Round up to the next whole multiple of m.
///
inline int
round_to_multiple (int x, int m)
{
    return ((x + m - 1) / m) * m;
}



/// Round up to the next whole multiple of m, for the special case where
/// m is definitely a power of 2 (somewhat simpler than the more general
/// round_to_multiple).
inline int
round_to_multiple_of_pow2 (int x, int m)
{
    return (x + m - 1) & (~(m-1));
}



/// Return true if the architecture we are running on is little endian
///
inline bool littleendian (void)
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



// Helper template to let us tell if two types are the same.
template<typename T, typename U> struct is_same { static const bool value = false; };
template<typename T> struct is_same<T,T> { static const bool value = true; };



/// Multiply src by scale, clamp to [min,max], and round to the nearest D
/// (presumed to be integer).  This is just a helper for the convert_type
/// templates, it probably has no other use.
template<typename S, typename D, typename F>
D scaled_conversion (const S &src, F scale, F min, F max)
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
void convert_type (const S *src, D *dst, size_t n, D _zero=0, D _one=1,
                   D _min=std::numeric_limits<D>::min(),
                   D _max=std::numeric_limits<D>::max())
{
    if (is_same<S,D>::value) {
        // They must be the same type.  Just memcpy.
        memcpy (dst, src, n*sizeof(D));
        return;
    }
    typedef double F;
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



/// Convert a single value from the type of S to the type of D.
/// The conversion is not a simple cast, but correctly remaps the
/// 0.0->1.0 range from and to the full positive range of integral
/// types.  Take a copy shortcut if both types are the same and no
/// conversion is necessary.
template<typename S, typename D>
D convert_type (const S &src)
{
    if (is_same<S,D>::value) {
        // They must be the same type.  Just return it.
        return (D)src;
    }
    typedef double F;
    F scale = std::numeric_limits<S>::is_integer ?
        ((F)1.0)/std::numeric_limits<S>::max() : (F)1.0;
    if (std::numeric_limits<D>::is_integer) {
        // Converting to an integer-like type.
        typedef double F;
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




/// Linearly interpolate values v0-v1 at x: v0*(1-x) + v1*x.
/// This is a template, and so should work for any types.
template <class T, class Q>
inline T
lerp (T v0, T v1, Q x)
{
    // NOTE: a*(1-x) + b*x is much more numerically stable than a+x*(b-a)
    return v0*(Q(1)-x) + v1*x;
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
    return (T) (((Q)1-t)*(v0*s1 + v1*s) + t*(v2*s1 + v3*s));
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
        result[i] += (T) (scale * (t1*(v0[i]*s1 + v1[i]*s) +
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
    Q s1 = (Q)1 - s;
    Q t1 = (Q)1 - t;
    Q r1 = (Q)1 - r;
    return (T) (r1*(t1*(v0*s1 + v1*s) + t*(v2*s1 + v3*s)) +
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
    Q s1 = (Q)1 - s;
    Q t1 = (Q)1 - t;
    Q r1 = (Q)1 - r;
    for (int i = 0;  i < n;  ++i)
        result[i] = (T) (r1*(t1*(v0[i]*s1 + v1[i]*s) + t*(v2[i]*s1 + v3[i]*s)) +
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
    Q r1 = (Q)1 - r;
    bilerp_mad (v0, v1, v2, v3, s, t, scale*r1, n, result);
    bilerp_mad (v4, v5, v6, v7, s, t, scale*r, n, result);
}



/// Fast rounding to nearest integer.
/// See Michael Herf's "Know Your FPU" page:
/// http://www.stereopsis.com/sree/fpu2006.html
inline int
RoundToInt (double val)
{
#ifdef USE_INTEL_MATH_SHORTCUTS
    union { int i; double d; } myunion;
    myunion.d = val;
    const double doublemagic = double (6755399441055744.0);
        // 2^52 * 1.5, uses limited precisicion to floor
    myunion.d += doublemagic;
    return myunion.i;
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
    return x - static_cast<float>(i);   // Return the fraction left over
}



/// Convert degrees to radians.
///
inline float radians (float deg) { return deg * (float)(M_PI / 180.0); }

/// Convert radians to degrees
///
inline float degrees (float rad) { return rad * (float)(180.0 / M_PI); }



/// Fast float exp
inline float fast_expf(float x)
{
#if defined(__x86_64__) && defined(__GNU_LIBRARY__) && defined(__GLIBC__ ) && defined(__GLIBC_MINOR__) && __GLIBC__ <= 2 && __GLIBC_MINOR__ < 16
    // On x86_64, versions of glibc < 2.16 have an issue where expf is
    // much slower than the double version.  This was fixed in glibc 2.16.
    return static_cast<float>(std::exp(static_cast<double>(x)));
#else
    return std::exp(x);
#endif
}



/// Fast approximate sin(x*M_PI) with ~0.001 maximum absolute error.
/// http://devmaster.net/posts/9648/fast-and-accurate-sine-cosine#comment-76773
static inline float fast_sinpi (float x)
{
    // Fast trick to strip the integral part off, so our domain is [-1,1]
    float z = (x + 25165824.0f);
    x = x - (z - 25165824.0f);

    float y = x - x * fabsf(x);
    const float Q = 3.10396624f;
    const float P = 3.584135056f;
    return y * (Q + P * fabsf(y));
    // N.B. This approximates sin(pi*x) as a polynomial, and guarantees that
    // sin(i*PI) == 0 and sin(+/- PI/2) == +/- 1 if Q/4+P/16 == 1.  In other
    // words, P = 16 * (1-Q/4).
    //
    // The original citation (using Q=3.1, P=3.6) had max error 0.001091.
    // Chris Kulla supplied the above, slightly modified constants that
    // reduce the maximum absolute error to 0.000918954611.
}


/// Fast approximate sin(x) with ~0.1% absolute error.
static inline float fast_sin(float x)
{
    return fast_sinpi (x * float(M_1_PI));
}



/// Fast approximate cos(x*M_PI) with ~0.1% absolute error.
static inline float fast_cospi (float x)
{
    return fast_sinpi (x+0.5f);
}



/// Fast approximate sin(x) with ~0.1% absolute error.
static inline float fast_cos(float x)
{
    return fast_cospi (x * float(M_1_PI));
}



#ifdef _WIN32
// Windows doesn't define these functions from math.h
#define hypotf _hypotf
#define copysign(x,y) _copysign(x,y)
#define copysignf(x,y) copysign(x,y)

#define M_E        2.71828182845904523536
#define M_LOG2E    1.44269504088896340736
//#define M_LOG10E   0.434294481903251827651
//#define M_LN2      0.693147180559945309417
//#define M_LN10     2.30258509299404568402
//#define M_PI       3.14159265358979323846
//#define M_PI_2     1.57079632679489661923
#define M_PI_4     0.785398163397448309616
//#define M_1_PI     0.318309886183790671538
//#define M_2_PI     0.636619772367581343076
//#define M_2_SQRTPI 1.12837916709551257390
//#define M_SQRT2    1.41421356237309504880
//#define M_SQRT1_2  0.707106781186547524401


inline float
truncf(float val)
{
    return (float)(int)val;
}
using OIIO::truncf;


#if defined(_MSC_VER) && _MSC_VER < 1800 /* Needed for MSVS prior to 2013 */

template<class T>
inline int isnan (T x) {
    return _isnan(x);
}
using OIIO::isnan;

template<class T>
inline int isfinite (T x) {
    return _finite(x);
}
using OIIO::isfinite;

template<class T>
inline int isinf (T x) {
    return (isfinite(x)||isnan(x)) ? 0 : static_cast<int>(copysign(T(1.0), x));
}
using OIIO::isinf;

inline double
round (float val) {
    return floor (val + 0.5);
}
using OIIO::round;

inline float
roundf (float val) {
    return static_cast<float>(round (val));
}
using OIIO::roundf;
#endif /* MSVS < 2013 */


inline float
log2f (float val) {
    return logf (val)/static_cast<float>(M_LN2);
}
using OIIO::log2f;


inline float
exp2f(float val) {
   // 2^val = e^(val*ln(2))
   return exp( val*log(2.0f) );
}
using OIIO::exp2f;


#if defined(_MSC_VER) && _MSC_VER < 1800 /* Needed for MSVS prior to 2013 */
inline float
logbf(float val) {
   // please see http://www.kernel.org/doc/man-pages/online/pages/man3/logb.3.html
   return logf(val)/logf(FLT_RADIX);
}
using OIIO::logbf;

// from http://www.johndcook.com/cpp_expm1.html
inline double
expm1(double val)
{
    // exp(x) - 1 without loss of precision for small values of x.
    if (fabs(val) < 1e-5)
        return val + 0.5*val*val;
    else
        return exp(val) - 1.0;
}
using OIIO::expm1;

inline float
expm1f(float val)
{
    return (float)expm1(val);
}
using OIIO::expm1f;

// from http://www.johndcook.com/cpp_erf.html
inline double
erf(double x)
{
    // constants
    double a1 =  0.254829592;
    double a2 = -0.284496736;
    double a3 =  1.421413741;
    double a4 = -1.453152027;
    double a5 =  1.061405429;
    double p  =  0.3275911;

    // Save the sign of x
    int sign = 1;
    if (x < 0)
        sign = -1;
    x = fabs(x);

    // A&S formula 7.1.26
    double t = 1.0/(1.0 + p*x);
    double y = 1.0 - (((((a5*t + a4)*t) + a3)*t + a2)*t + a1)*t*exp(-x*x);

    return sign*y;
}
using OIIO::erf;

inline float
erff(float val)
{
    return (float)erf(val);
}
using OIIO::erff;

inline double
erfc(double val)
{
    return 1.0 - erf(val);
}
using OIIO::erfc;

inline float
erfcf(float val)
{
    return (float)erfc(val);
}
using OIIO::erfcf;
#endif /* MSVS < 2013 */


#endif  /* _WIN32 */


// Some systems have isnan, isinf and isfinite in the std namespace.
#ifndef _MSC_VER
 using std::isnan;
 using std::isinf;
 using std::isfinite;
#endif



// Functions missing from FreeBSD
#if (defined(__FreeBSD__) && (__FreeBSD_version < 803000))

inline float
log2f (float val) {
    return logf (val)/static_cast<float>(M_LN2);
}

using OIIO::log2f;
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


inline void
sincos(float x, float* sine, float* cosine)
{
#if defined(__GNUC__) && defined(__linux__) && !defined(__clang__)
    __builtin_sincosf(x, sine, cosine);
#else
    *sine = std::sin(x);
    *cosine = std::cos(x);
#endif
}

inline void
sincos(double x, double* sine, double* cosine)
{
#if defined(__GNUC__) && defined(__linux__) && !defined(__clang__)
    __builtin_sincos(x, sine, cosine);
#else
    *sine = std::sin(x);
    *cosine = std::cos(x);
#endif
}



/// Safe (clamping) arcsine.
///
inline float
safe_asinf (float x)
{
    if (x >=  1.0f) return  static_cast<float>(M_PI)/2;
    if (x <= -1.0f) return -static_cast<float>(M_PI)/2;
    return std::asin (x);
}


/// Safe (clamping) arccosine.
///
inline float
safe_acosf (float x) {
    if (x >=  1.0f) return 0.0f;
    if (x <= -1.0f) return static_cast<float>(M_PI);
    return std::acos (x);
}


/// Safe (clamping) sqrt.
///
inline double
safe_sqrt (double x)
{
    return (x > 0.0) ? sqrt(x) : 0.0;
}

inline float
safe_sqrtf (float x)
{
    return (x > 0.0f) ? sqrtf(x) : 0.0f;
}

/// Safe (clamping) inverse sqrt
inline float safe_inversesqrt (float x) {
    return (x > 0.0f) ? 1.0f/std::sqrt(x) : 0.0f;
}

inline float safe_log (float x) {
    return (x > 0.0f) ? logf(x) : -std::numeric_limits<float>::max();
}

inline float safe_log2(float x) {
    return (x > 0.0f) ? log2f(x) : -std::numeric_limits<float>::max();
}

inline float safe_log10(float x) {
    return (x > 0.0f) ? log10f(x) : -std::numeric_limits<float>::max();
}

inline float safe_logb (float x) {
    return (x != 0.0f) ? logbf(x) : -std::numeric_limits<float>::max();
}




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



}
OIIO_NAMESPACE_EXIT

#endif // OPENIMAGEIO_FMATH_H
