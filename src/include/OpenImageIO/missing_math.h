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
*/


/// \file
///
/// This adds definitions we think *should* be in math.h, but which on
/// on some platforms appears to be missing in action.
///


#pragma once

#include <cmath>

#include "oiioversion.h"   /* Just for the OIIO_NAMESPACE stuff */


#ifndef M_PI
/// PI
#  define M_PI 3.14159265358979323846264338327950288 
#endif

#ifndef M_PI_2
/// PI / 2
#  define M_PI_2 1.57079632679489661923132169163975144
#endif

#ifndef M_PI_4
/// PI / 4
#  define M_PI_4 0.785398163397448309615660845819875721
#endif

#ifndef M_TWO_PI
/// PI * 2
#  define M_TWO_PI (M_PI * 2.0)
#endif

#ifndef M_1_PI
/// 1/PI
#  define M_1_PI 0.318309886183790671537767526745028724
#endif

#ifndef M_2_PI
/// 2/PI
#  define M_2_PI 0.636619772367581343075535053490057448
#endif

#ifndef M_SQRT2
/// sqrt(2)
#  define M_SQRT2 1.41421356237309504880168872420969808
#endif

#ifndef M_SQRT1_2
/// 1/sqrt(2)
#  define M_SQRT1_2 0.707106781186547524400844362104849039
#endif

#ifndef M_LN2
/// ln(2)
#  define M_LN2 0.69314718055994530941723212145817656
#endif

#ifndef M_LN10
/// ln(10)
#  define M_LN10 2.30258509299404568401799145468436421
#endif

#ifndef M_E
/// e, Euler's number
#  define M_E 2.71828182845904523536028747135266250
#endif

#ifndef M_LOG2E
/// log2(e)
#  define M_LOG2E 1.44269504088896340735992468100189214 
#endif


OIIO_NAMESPACE_BEGIN

#ifdef _WIN32
// Windows doesn't define these functions from math.h
#define hypotf _hypotf
#define copysign(x,y) _copysign(x,y)
#define copysignf(x,y) float(copysign(x,y))



inline float
truncf (float val)
{
    return (float)(int)val;
}


#if defined(_MSC_VER)
#if _MSC_VER < 1800 /* Needed for MSVS prior to 2013 */

template<class T>
inline int isnan (T x) {
    return _isnan(x);
}


template<class T>
inline int isfinite (T x) {
    return _finite(x);
}


template<class T>
inline int isinf (T x) {
    return (isfinite(x)||isnan(x)) ? 0 : static_cast<int>(copysign(T(1.0), x));
}


inline double
round (float val) {
    return floor (val + 0.5);
}


inline float
roundf (float val) {
    return static_cast<float>(round (val));
}


inline float
log2f (float val) {
    return logf (val)/static_cast<float>(M_LN2);
}


inline float
cbrtf (float val) {
	return powf (val, 1.0/3.0);
}


inline float
rintf (float val) {
    return val + copysignf(0.5f, val);
}

#elif _MSC_VER >= 1800 && __cplusplus <= 201103L
// Prior to c++11, these were implementation defined, and on msvc, were not in the
// std namespace
using ::isnan;
using ::isinf;
using ::isfinite;
#else
using std::isnan;
using std::isinf;
using std::isfinite;
#endif

#endif /* MSVS < 2013 */


inline float
exp2f (float val) {
   // 2^val = e^(val*ln(2))
   return (float) exp( val * M_LN2 );
}


#if defined(_MSC_VER) && _MSC_VER < 1800 /* Needed for MSVS prior to 2013 */
inline float
logbf (float val) {
   // please see http://www.kernel.org/doc/man-pages/online/pages/man3/logb.3.html
   return floorf (logf(fabsf(val))/logf(FLT_RADIX));
}


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


inline float
expm1f (float val)
{
    return (float)expm1(val);
}


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


inline float
erff (float val)
{
    return (float)erf(val);
}


inline double
erfc(double val)
{
    return 1.0 - erf(val);
}


inline float
erfcf (float val)
{
    return (float)erfc(val);
}

#endif /* MSVS < 2013 */

#endif  /* _WIN32 */


#if !defined(_MSC_VER)
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

#endif


OIIO_NAMESPACE_END


