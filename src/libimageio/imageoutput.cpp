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
#include <cmath>
#include <cstdarg>
#include <iostream>
#include <vector>

#include "dassert.h"
#include "paramtype.h"
#include "filesystem.h"
#include "plugin.h"
#include "thread.h"
#include "strutil.h"

#define DLL_EXPORT_PUBLIC /* Because we are implementing ImageIO */
#include "imageio.h"
#include "imageio_pvt.h"
#undef DLL_EXPORT_PUBLIC

using namespace OpenImageIO;
using namespace OpenImageIO::pvt;



int
ImageOutput::send_to_output (const char *format, ...)
{
    // FIXME -- I can't remember how this is supposed to work
    return 0;
}



int
ImageOutput::send_to_client (const char *format, ...)
{
    // FIXME -- I can't remember how this is supposed to work
    return 0;
}



void
ImageOutput::error (const char *message, ...)
{
    va_list ap;
    va_start (ap, message);
    m_errmessage = Strutil::vformat (message, ap);
    va_end (ap);
}



const void *
ImageOutput::to_native_scanline (ParamBaseType format,
                                 const void *data, int xstride,
                                 std::vector<char> &scratch)
{
    return to_native_rectangle (0, spec.width-1, 0, 0, 0, 0, format, data,
                                xstride, xstride*spec.width, 0, scratch);
}



const void *
ImageOutput::to_native_tile (ParamBaseType format, const void *data,
                             int xstride, int ystride, int zstride,
                             std::vector<char> &scratch)
{
    return to_native_rectangle (0, spec.tile_width-1, 0, spec.tile_height-1,
                                0, std::max(0,spec.tile_depth-1), format, data,
                                xstride, ystride, zstride, scratch);
}



const void *
ImageOutput::to_native_rectangle (int xmin, int xmax, int ymin, int ymax,
                                  int zmin, int zmax, 
                                  ParamBaseType format, const void *data,
                                  int xstride, int ystride, int zstride,
                                  std::vector<char> &scratch)
{
    // Compute width and height from the rectangle extents
    int width = xmax - xmin + 1;
    int height = ymax - ymin + 1;

    // Do the strides indicate that the data are already contiguous?
    bool contiguous = (xstride == spec.nchannels &&
                       ystride == spec.nchannels*width &&
                       (zstride == spec.nchannels*width*height || !zstride));
    // Is the only conversion we are doing that of data format?
    bool data_conversion_only =  (contiguous && spec.gamma == 1.0f);

    if (format == spec.format && data_conversion_only) {
        // Data are already in the native format, contiguous, and need
        // no gamma correction -- just return a ptr to the original data.
        return data;
    }

    int depth = zmax - zmin + 1;
    int rectangle_pixels = width * height * depth;
    int rectangle_values = rectangle_pixels * spec.nchannels;
    bool contiguoussize = contiguous ? 0 
                : rectangle_values * ParamBaseTypeSize(format);
    int rectangle_bytes = rectangle_pixels * spec.pixel_bytes();
    int floatsize = rectangle_values * sizeof(float);
    scratch.resize (contiguoussize + floatsize + rectangle_bytes);

    // Force contiguity if not already present
    if (! contiguous) {
        data = contiguize (data, spec.nchannels, xstride, ystride, zstride,
                           (void *)&scratch[0], width, height, depth, format);
        // Reset strides to indicate contiguous data
        xstride = spec.nchannels;
        ystride = spec.nchannels * width;
        zstride = spec.nchannels * width * height;
    }

    // Rather than implement the entire cross-product of possible
    // conversions, use float as an intermediate format, which generally
    // will always preserve enough precision.
    const float *buf;
    if (format == PT_FLOAT && spec.gamma == 1.0f) {
        // Already in float format and no gamma correction is needed --
        // leave it as-is.
        buf = (float *)data;
    } else {
        // Convert to from 'format' to float.
        buf = convert_to_float (data, (float *)&scratch[contiguoussize],
                                rectangle_values, format);
        // Now buf points to float
        if (spec.gamma != 1) {
            float invgamma = 1.0 / spec.gamma;
            float *f = (float *)buf;
            for (int p = 0;  p < rectangle_pixels;  ++p)
                for (int c = 0;  c < spec.nchannels;  ++c, ++f)
                    if (c != spec.alpha_channel)
                        *f = powf (*f, invgamma);
            // FIXME: we should really move the gamma correction to
            // happen immediately after contiguization.  That way,
            // byte->byte with gamma can use a table shortcut instead
            // of having to go through float just for gamma.
        }
        // Now buf points to gamma-corrected float
    }
    // Convert from float to native format.
    return convert_from_float (buf, &scratch[contiguoussize+floatsize], 
                       rectangle_pixels, spec.quant_black, spec.quant_white,
                       spec.quant_min, spec.quant_max, spec.quant_dither,
                       spec.format);
}
