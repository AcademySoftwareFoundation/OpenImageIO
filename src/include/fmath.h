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


#ifndef FMATH_H
#define FMATH_H

// A variety of floating-point math helper routines (and, slight
// misnomer, some int stuff as well).
//



/// Template to convert contiguous data of type T to floats.
///
template<typename T>
void to_float (const T *src, float *dst, size_t n)
{
    // FIXME: it would be a good idea to have table-based specializations
    // for common types with only a few values (like unsigned char/short).
    float scale = std::numeric_limits<T>::is_integer ?
                      1.0f/(float)std::numeric_limits<T>::max() : 1.0f;
    // Unroll loop for speed
    for ( ; n >= 16; n -= 16) {
        *dst++ = (float)(*src++) * scale;
        *dst++ = (float)(*src++) * scale;
        *dst++ = (float)(*src++) * scale;
        *dst++ = (float)(*src++) * scale;
        *dst++ = (float)(*src++) * scale;
        *dst++ = (float)(*src++) * scale;
        *dst++ = (float)(*src++) * scale;
        *dst++ = (float)(*src++) * scale;
        *dst++ = (float)(*src++) * scale;
        *dst++ = (float)(*src++) * scale;
        *dst++ = (float)(*src++) * scale;
        *dst++ = (float)(*src++) * scale;
        *dst++ = (float)(*src++) * scale;
        *dst++ = (float)(*src++) * scale;
        *dst++ = (float)(*src++) * scale;
        *dst++ = (float)(*src++) * scale;
    }
    while (n--)
        *dst++ = (float)(*src++) * scale;
}






#endif // FMATH_H
