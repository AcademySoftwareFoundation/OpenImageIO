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



/// Convert n consecutive values from the type of S to the type of D.
/// The conversion is not a simple cast, but correctly remaps the
/// 0.0->1.0 range from and to the full positive range of integral
/// types.  Take a memcpy shortcut if both types are the same and no
/// conversion is necessary. 
//
// FIXME: make table-based specializations for common types with only a
// few possible src values (like unsigned char -> float).
template<typename S, typename D>
void convert_type (const S *src, D *dst, size_t n)
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
        ((F)1.0)/std::numeric_limits<D>::max() : (F)1.0;
    if (std::numeric_limits<D>::is_integer) {
        // Converting to an integer-like type.
        F min = (F)std::numeric_limits<D>::min();
        F max = (F)std::numeric_limits<D>::max();
        scale *= max;
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


#endif // FMATH_H
