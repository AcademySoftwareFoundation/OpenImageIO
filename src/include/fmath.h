/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2008 Larry Gritz.
// 
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
// 
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 
// (This is the MIT open source license.)
/////////////////////////////////////////////////////////////////////////////


// A variety of floating-point math helper routines (and, slight
// misnomer, some int stuff as well).
//

#ifndef FMATH_H
#define FMATH_H


#include <ImathFun.h>


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
inline bool
ispow2 (int x)
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
    // There's probably a bit twiddling trick that does this without
    // looping.
    int p = 1;
    while (p < x)
        p <<= 1;
    return p;
}



/// Round down to next lower power of 2 (return x if it's already a power
/// of 2).
inline int
pow2rounddown (int x)
{
    // There's probably a bit twiddling trick that does this without
    // looping.
    if (x <= 0)
        return 0;
    int p = 1;
    while (2*p <= x)
        p <<= 1;
    return p;
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



/// Change endian-ness of one or more data items that are each either 2
/// or 4 bytes.  This should work for any of short, unsigned short, int,
/// unsigned int, float.
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
            c += 4;
        }
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
            *dst++ = (D)(Imath::clamp ((F)(*src++) * scale, min, max));
            *dst++ = (D)(Imath::clamp ((F)(*src++) * scale, min, max));
            *dst++ = (D)(Imath::clamp ((F)(*src++) * scale, min, max));
            *dst++ = (D)(Imath::clamp ((F)(*src++) * scale, min, max));
            *dst++ = (D)(Imath::clamp ((F)(*src++) * scale, min, max));
            *dst++ = (D)(Imath::clamp ((F)(*src++) * scale, min, max));
            *dst++ = (D)(Imath::clamp ((F)(*src++) * scale, min, max));
            *dst++ = (D)(Imath::clamp ((F)(*src++) * scale, min, max));
            *dst++ = (D)(Imath::clamp ((F)(*src++) * scale, min, max));
            *dst++ = (D)(Imath::clamp ((F)(*src++) * scale, min, max));
            *dst++ = (D)(Imath::clamp ((F)(*src++) * scale, min, max));
            *dst++ = (D)(Imath::clamp ((F)(*src++) * scale, min, max));
            *dst++ = (D)(Imath::clamp ((F)(*src++) * scale, min, max));
            *dst++ = (D)(Imath::clamp ((F)(*src++) * scale, min, max));
            *dst++ = (D)(Imath::clamp ((F)(*src++) * scale, min, max));
            *dst++ = (D)(Imath::clamp ((F)(*src++) * scale, min, max));
        }
        while (n--)
            *dst++ = (D)(Imath::clamp ((F)(*src++) * scale, min, max));
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



template <class T, class Q>
inline T
bilerp (T v0, T v1, T v2, T v3, Q s, Q t)
{
    // NOTE: a*(t-1) + b*t is much more numerically stable than a+t*(b-a)
    Q s1 = 1.0f - s;
    return (T) ((1-t)*(v0*s1 + v1*s) + t*(v2*s1 + v3*s));
}



template <class T, class Q>
inline void
bilerp (const T *v0, const T *v1,
        const T *v2, const T *v3,
        Q s, Q t, int n, T *result)
{
    T s1 = 1 - s;
    T t1 = 1 - t;
    for (int i = 0;  i < n;  ++i)
        result[i] = (T) t1*(v0[i]*s1 + v1[i]*s) + t*(v2[i]*s1 + v3[i]*s);
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
    int i = (int) x - (x < 0.0f ? 1 : 0);
    *xint = i;
    return x - i;   // Return the fraction left over
}



inline float radians (float deg) { return deg * M_PI / 180.0f; }
inline float degrees (float rad) { return rad * 180.0 / M_PI; }


#endif // FMATH_H
