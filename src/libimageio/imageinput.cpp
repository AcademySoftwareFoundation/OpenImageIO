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
#include <unistd.h>

#include "dassert.h"
#include "paramtype.h"
#include "strutil.h"

#define DLL_EXPORT_PUBLIC /* Because we are implementing ImageIO */
#include "imageio.h"
#include "imageio_pvt.h"
#undef DLL_EXPORT_PUBLIC

using namespace OpenImageIO;
using namespace OpenImageIO::pvt;



bool 
ImageInput::read_scanline (int y, int z, ParamBaseType format, void *data,
                           int xstride)
{
    spec.auto_stride (xstride);
    bool contiguous = (xstride == spec.nchannels);
    if (contiguous && spec.format == format)  // Simple case
        return read_native_scanline (y, z, data);

    // Complex case -- either changing data type or stride
    int scanline_values = spec.width * spec.nchannels;
    unsigned char *buf = (unsigned char *) alloca (spec.scanline_bytes());
    bool ok = read_native_scanline (y, z, buf);
    if (! ok)
        return false;
    ok = contiguous 
        ? convert_types (spec.format, buf, format, data, scanline_values)
        : convert_image (spec.nchannels, spec.width, 1, 1, 
                         buf, spec.format, spec.width, spec.height, spec.depth,
                         data, format, xstride, 0, 0);
    if (! ok)
        error ("ImageInput::read_scanline : no support for format %s",
               ParamBaseTypeNameString(spec.format));
    return ok;
}



bool 
ImageInput::read_tile (int x, int y, int z, ParamBaseType format, void *data,
                       int xstride, int ystride, int zstride)
{
    spec.auto_stride (xstride, ystride, zstride);
    bool contiguous = (xstride == spec.nchannels &&
                       ystride == xstride*spec.tile_width &&
                       zstride == ystride*spec.tile_height);
    if (contiguous && spec.format == format)  // Simple case
        return read_native_tile (x, y, z, data);

    // Complex case -- either changing data type or stride
    int tile_values = spec.tile_width * spec.tile_height * 
                      spec.tile_depth * spec.nchannels;
    unsigned char *buf = (unsigned char *) alloca (spec.tile_bytes());
    bool ok = read_native_tile (x, y, z, buf);
    if (! ok)
        return false;
    ok = contiguous 
        ? convert_types (spec.format, buf, format, data, tile_values)
        : convert_image (spec.nchannels, spec.tile_width, spec.tile_height, spec.tile_depth, 
                         buf, spec.format, spec.tile_width, spec.tile_height, spec.tile_depth,
                         data, format, xstride, ystride, zstride);
    if (! ok)
        error ("ImageInput::read_tile : no support for format %s",
               ParamBaseTypeNameString(spec.format));
    return ok;

}



bool
ImageInput::read_image (ParamBaseType format, void *data,
                        int xstride, int ystride, int zstride,
                        OpenImageIO::ProgressCallback progress_callback,
                        void *progress_callback_data)
{
    spec.auto_stride (xstride, ystride, zstride);
    // Rescale strides to be in bytes, not channel elements
    int xstride_bytes = xstride * ParamBaseTypeSize (format);
    int ystride_bytes = ystride * ParamBaseTypeSize (format);
    int zstride_bytes = zstride * ParamBaseTypeSize (format);
    bool ok = true;
    if (progress_callback)
        if (progress_callback (progress_callback_data, 0.0f))
            return ok;
    if (spec.tile_width) {
        // Tiled image

        // FIXME: what happens if the image dimensions are smaller than
        // the tile dimensions?  Or if one of the tiles runs past the
        // right or bottom edge?  Do we need to allocate a full tile and
        // copy into it into buf?  That's probably the safe thing to do.
        // Or should that handling be pushed all the way into read_tile
        // itself?
        for (int z = 0;  z < spec.depth;  z += spec.tile_depth)
            for (int y = 0;  y < spec.height;  y += spec.tile_height) {
                for (int x = 0;  x < spec.width && ok;  y += spec.tile_width)
                    ok &= read_tile (x, y, z, format,
                                     (char *)data + z*zstride_bytes + y*ystride_bytes + x*xstride_bytes,
                                     xstride, ystride, zstride);
                if (progress_callback)
                    if (progress_callback (progress_callback_data, (float)y/spec.height))
                        return ok;
            }
    } else {
        // Scanline image
        for (int z = 0;  z < spec.depth;  ++z)
            for (int y = 0;  y < spec.height && ok;  ++y) {
                ok &= read_scanline (y, z, format,
                                     (char *)data + z*zstride_bytes + y*ystride_bytes,
                                     xstride);
                if (progress_callback && !(y & 0x0f))
                    if (progress_callback (progress_callback_data, (float)y/spec.height))
                        return ok;
            }
    }
    if (progress_callback)
        progress_callback (progress_callback_data, 1.0f);
    return ok;
}



int 
ImageInput::send_to_input (const char *format, ...)
{
    // FIXME -- I can't remember how this is supposed to work
    return 0;
}



int 
ImageInput::send_to_client (const char *format, ...)
{
    // FIXME -- I can't remember how this is supposed to work
    return 0;
}



void 
ImageInput::error (const char *message, ...)
{
    va_list ap;
    va_start (ap, message);
    m_errmessage = Strutil::vformat (message, ap);
    va_end (ap);
}

