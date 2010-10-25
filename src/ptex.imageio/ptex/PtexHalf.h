#ifndef PtexHalf_h
#define PtexHalf_h

/* 
PTEX SOFTWARE
Copyright 2009 Disney Enterprises, Inc.  All rights reserved

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.

  * The names "Disney", "Walt Disney Pictures", "Walt Disney Animation
    Studios" or the names of its contributors may NOT be used to
    endorse or promote products derived from this software without
    specific prior written permission from Walt Disney Pictures.

Disclaimer: THIS SOFTWARE IS PROVIDED BY WALT DISNEY PICTURES AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE, NONINFRINGEMENT AND TITLE ARE DISCLAIMED.
IN NO EVENT SHALL WALT DISNEY PICTURES, THE COPYRIGHT HOLDER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND BASED ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
*/

/**
  @file PtexHalf.h
  @brief Half-precision floating-point type.
*/

#ifndef PTEXAPI
#  if defined(_WIN32) || defined(_WINDOWS) || defined(_MSC_VER)
#    ifndef PTEX_STATIC
#      ifdef PTEX_EXPORTS
#         define PTEXAPI __declspec(dllexport)
#      else
#         define PTEXAPI __declspec(dllimport)
#      endif
#    else
#      define PTEXAPI
#    endif
#  else
#    define PTEXAPI
#  endif
#endif

#include "PtexInt.h"

/**
   @class PtexHalf
   @brief Half-precision (16-bit) floating-point type.

   This type should be compatible with opengl, openexr, and IEEE 754r.
   The range is [-65504.0, 65504.0] and the precision is about 1 part
   in 2000 (3.3 decimal places).

   From OpenGL spec 2.1.2:

   A 16-bit floating-point number has a 1-bit sign (S), a 5-bit
   exponent (E), and a 10-bit mantissa (M).  The value of a 16-bit
   floating-point number is determined by the following:

   \verbatim
        (-1)^S * 0.0,                        if E == 0 and M == 0,
        (-1)^S * 2^-14 * (M/2^10),           if E == 0 and M != 0,
        (-1)^S * 2^(E-15) * (1 + M/2^10),    if 0 < E < 31,
        (-1)^S * INF,                        if E == 31 and M == 0, or
        NaN,                                 if E == 31 and M != 0 \endverbatim
*/

struct PtexHalf {
    uint16_t bits;

    /// Default constructor, value is undefined
    PtexHalf() {}
    PtexHalf(float val) : bits(fromFloat(val)) {}
    PtexHalf(double val) : bits(fromFloat(float(val))) {}
    operator float() const { return toFloat(bits); }
    PtexHalf& operator=(float val) { bits = fromFloat(val); return *this; }

    static float toFloat(uint16_t h)
    {
	union { uint32_t i; float f; } u;
	u.i = h2fTable[h];
	return u.f;
    }

    static uint16_t fromFloat(float val)
    {
	if (val==0) return 0;
	union { uint32_t i; float f; } u;
	u.f = val;
	int e = f2hTable[(u.i>>23)&0x1ff];
	if (e) return e + (((u.i&0x7fffff) + 0x1000) >> 13);
	return fromFloat_except(u.i);
    }

 private:
    PTEXAPI static uint16_t fromFloat_except(uint32_t val);
#ifndef DOXYGEN
    /* internal */ public:
#endif
    PTEXAPI static uint32_t h2fTable[65536];
    PTEXAPI static uint16_t f2hTable[512];
};

#endif
