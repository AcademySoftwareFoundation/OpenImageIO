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

#include <OpenImageIO/oiioversion.h>


#ifndef M_PI
/// PI
#    define M_PI 3.14159265358979323846264338327950288
#endif

#ifndef M_PI_2
/// PI / 2
#    define M_PI_2 1.57079632679489661923132169163975144
#endif

#ifndef M_PI_4
/// PI / 4
#    define M_PI_4 0.785398163397448309615660845819875721
#endif

#ifndef M_TWO_PI
/// PI * 2
#    define M_TWO_PI (M_PI * 2.0)
#endif

#ifndef M_1_PI
/// 1/PI
#    define M_1_PI 0.318309886183790671537767526745028724
#endif

#ifndef M_2_PI
/// 2/PI
#    define M_2_PI 0.636619772367581343075535053490057448
#endif

#ifndef M_SQRT2
/// sqrt(2)
#    define M_SQRT2 1.41421356237309504880168872420969808
#endif

#ifndef M_SQRT1_2
/// 1/sqrt(2)
#    define M_SQRT1_2 0.707106781186547524400844362104849039
#endif

#ifndef M_LN2
/// ln(2)
#    define M_LN2 0.69314718055994530941723212145817656
#endif

#ifndef M_LN10
/// ln(10)
#    define M_LN10 2.30258509299404568401799145468436421
#endif

#ifndef M_E
/// e, Euler's number
#    define M_E 2.71828182845904523536028747135266250
#endif

#ifndef M_LOG2E
/// log2(e)
#    define M_LOG2E 1.44269504088896340735992468100189214
#endif


OIIO_NAMESPACE_BEGIN

#ifdef _WIN32
// Windows doesn't define these functions from math.h
#    define hypotf _hypotf
#    define copysign(x, y) _copysign(x, y)
#    define copysignf(x, y) float(copysign(x, y))


inline float
truncf(float val)
{
    return (float)(int)val;
}


inline float
exp2f(float val)
{
    // 2^val = e^(val*ln(2))
    return (float)exp(val * M_LN2);
}


#endif /* _WIN32 */


#if OIIO_MSVS_AT_LEAST_2013 && __cplusplus <= 201103L
// Prior to c++11, these were implementation defined, and on msvc, were
// not in the std namespace.
using ::isfinite;
using ::isinf;
using ::isnan;
#else
using std::isfinite;
using std::isinf;
using std::isnan;
#endif



// Functions missing from FreeBSD
#if (defined(__FreeBSD__) && (__FreeBSD_version < 803000))

inline float
log2f(float val)
{
    return logf(val) / static_cast<float>(M_LN2);
}

#endif


OIIO_NAMESPACE_END
