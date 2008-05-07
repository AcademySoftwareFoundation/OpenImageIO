/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2008 Larry Gritz
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
// (this is the MIT license)
/////////////////////////////////////////////////////////////////////////////


#include <cstdio>
#include <cstdlib>

#include <half.h>
#include <ImathFun.h>

#include "dassert.h"
#include "paramtype.h"
#include "strutil.h"
#include "fmath.h"

#define DLL_EXPORT_PUBLIC /* Because we are implementing ImageIO */
#include "imageio.h"
#include "imageio_pvt.h"
#undef DLL_EXPORT_PUBLIC

using namespace OpenImageIO;



static std::string create_error_msg;

mutex OpenImageIO::imageio_mutex;



void
ImageIOParameter::init (const std::string &_name, ParamBaseType _type,
                        int _nvalues, const void *_value, bool _copy)
{
    name = _name;
    type = _type;
    nvalues = _nvalues;
    copy = _copy;
    if (copy) {
        size_t size = (size_t) (nvalues * ParamBaseTypeSize (type));
        value = malloc (size);
        memcpy ((void *)value, _value, size);
    } else {
        value = _value;
    }
}



void
ImageIOParameter::clear_value ()
{
    if (copy)
        free ((void *)value);
    value = NULL;
    copy = false;
}



/// Error reporting for the plugin implementation: call this with
/// printf-like arguments.
void
OpenImageIO::error (const char *message, ...)
{
    lock_guard lock (imageio_mutex);
    va_list ap;
    va_start (ap, message);
    create_error_msg = Strutil::vformat (message, ap);
    va_end (ap);
}



std::string
OpenImageIO::error_message ()
{
    lock_guard lock (imageio_mutex);
    return create_error_msg;
}



int
OpenImageIO::quantize (float value, int quant_black, int quant_white,
                       int quant_min, int quant_max, float quant_dither)
{
    value = Imath::lerp (quant_black, quant_white, value);
#if 0
    // FIXME
    if (quant_dither)
        value += quant_dither * (2.0f * rand() - 1.0f);
#endif
    return Imath::clamp ((int)(value + 0.5f), quant_min, quant_max);
}



float
OpenImageIO::exposure (float value, float gain, float invgamma)
{
    if (invgamma != 1 && value >= 0)
        return powf (gain * value, invgamma);
    // Simple case - skip the expensive pow; also fall back to this
    // case for negative values, for which gamma makes no sense.
    return gain * value;
}



/// Type-independent template for turning potentially
/// non-contiguous-stride data (e.g. "RGB RGB ") into contiguous-stride
/// ("RGBRGB").  Caller must pass in a dst pointing to enough memory to
/// hold the contiguous rectangle.  Return a ptr to where the contiguous
/// data ended up, which is either dst or src (if the strides indicated
/// that data were already contiguous).
template<typename T>
const T *
_contiguize (const T *src, int nchannels, int xstride, int ystride, int zstride, 
             T *dst, int width, int height, int depth)
{
    if (xstride == nchannels  &&  ystride == nchannels*width  &&
            (zstride == nchannels*width*height || !zstride))
        return src;

    if (depth < 1)     // Safeguard against volume-unaware clients
        depth == 1;
    T *dstsave = dst;
    for (int z = 0;  z < depth;  ++z, src += zstride) {
        const T *scanline = src;
        for (int y = 0;  y < height;  ++y, scanline += ystride) {
            const T *pixel = scanline;
            for (int x = 0;  x < width;  ++x, pixel += xstride)
                for (int c = 0;  c < nchannels;  ++c)
                    *dst++ = pixel[c];
        }
    }
    return dstsave;
}



const void *
OpenImageIO::contiguize (const void *src, int nchannels,
                         int xstride, int ystride, int zstride, 
                         void *dst, int width, int height, int depth,
                         ParamBaseType format)
{
    switch (format) {
    case PT_FLOAT :
        return _contiguize ((const float *)src, nchannels, 
                            xstride, ystride, zstride,
                            (float *)dst, width, height, depth);
    case PT_DOUBLE :
        return _contiguize ((const double *)src, nchannels, 
                            xstride, ystride, zstride,
                            (double *)dst, width, height, depth);
    case PT_INT8:
    case PT_UINT8 :
        return _contiguize ((const char *)src, nchannels, 
                            xstride, ystride, zstride,
                            (char *)dst, width, height, depth);
    case PT_HALF :
        DASSERT (sizeof(half) == sizeof(short));
    case PT_INT16 :
    case PT_UINT16 :
        return _contiguize ((const short *)src, nchannels, 
                            xstride, ystride, zstride,
                            (short *)dst, width, height, depth);
    case PT_INT :
    case PT_UINT :
        return _contiguize ((const int *)src, nchannels, 
                            xstride, ystride, zstride,
                            (int *)dst, width, height, depth);
    default:
        std::cerr << "ERROR OpenImageIO::contiguize : bad format\n";
        ASSERT (0);
        return NULL;
    }
}



const float *
OpenImageIO::convert_to_float (const void *src, float *dst, int nvals,
                               ParamBaseType format)
{
    switch (format) {
    case PT_FLOAT :
        return (float *)src;
    case PT_HALF :
        to_float<half> ((const half *)src, dst, nvals);
    case PT_DOUBLE :
        to_float<double> ((const double *)src, dst, nvals);
    case PT_INT8:
        to_float<char> ((const char *)src, dst, nvals);
    case PT_UINT8 :
        to_float<unsigned char> ((const unsigned char *)src, dst, nvals);
    case PT_INT16 :
        to_float<short> ((const short *)src, dst, nvals);
    case PT_UINT16 :
        to_float<unsigned short> ((const unsigned short *)src, dst, nvals);
    case PT_INT :
        to_float<int> ((const int *)src, dst, nvals);
    case PT_UINT :
        to_float<unsigned int> ((const unsigned int *)src, dst, nvals);
    default:
        std::cerr << "ERROR to_float: bad format\n";
        ASSERT (0);
        return NULL;
    }
    return dst;
}



template<typename T>
const void *
_from_float (const float *src, T *dst, size_t nvals,
            int quant_black, int quant_white, int quant_min, int quant_max,
            float quant_dither)
{
    T *d = (T *)dst;

    if (! src) {
        // If no source pixels, assume zeroes
        memset (dst, 0, nvals * sizeof(T));
    } else if (std::numeric_limits <T>::is_integer) {
        // Convert float to non-float native format, with quantization
        for (size_t p = 0;  p < nvals;  ++p)
            dst[p] = (T) quantize (src[p], quant_black, quant_white,
                                   quant_min, quant_max, quant_dither);
    } else {
        // It's a floating-point type of some kind -- we don't apply 
        // quantization
        if (sizeof(T) == sizeof(float)) {
            // It's already float -- return the source itself
            return src;
        }
        // Otherwise, it's converting between two fp types
        for (size_t p = 0;  p < nvals;  ++p)
            dst[p] = src[p];
    }

    return dst;
}



const void *
OpenImageIO::convert_from_float (const float *src, void *dst, size_t nvals,
                                 int quant_black, int quant_white,
                                 int quant_min, int quant_max, float quant_dither, 
                                 ParamBaseType format)
{
    switch (format) {
    case PT_FLOAT :
        return src;
    case PT_HALF :
        return _from_float<half> (src, (half *)dst, nvals,
                           quant_black, quant_white, quant_min,
                           quant_max, quant_dither);
    case PT_DOUBLE :
        return _from_float (src, (double *)dst, nvals,
                           quant_black, quant_white, quant_min,
                           quant_max, quant_dither);
    case PT_INT8:
        return _from_float (src, (char *)dst, nvals,
                           quant_black, quant_white, quant_min,
                           quant_max, quant_dither);
    case PT_UINT8 :
        return _from_float (src, (unsigned char *)dst, nvals,
                           quant_black, quant_white, quant_min,
                           quant_max, quant_dither);
    case PT_INT16 :
        return _from_float (src, (short *)dst, nvals,
                           quant_black, quant_white, quant_min,
                           quant_max, quant_dither);
    case PT_UINT16 :
        return _from_float (src, (unsigned short *)dst, nvals,
                           quant_black, quant_white, quant_min,
                           quant_max, quant_dither);
    case PT_INT :
        return _from_float (src, (int *)dst, nvals,
                           quant_black, quant_white, quant_min,
                           quant_max, quant_dither);
    case PT_UINT :
        return _from_float (src, (unsigned int *)dst, nvals,
                           quant_black, quant_white, quant_min,
                           quant_max, quant_dither);
    default:
        std::cerr << "ERROR from_float: bad format\n";
        ASSERT (0);
        return NULL;
    }
}
