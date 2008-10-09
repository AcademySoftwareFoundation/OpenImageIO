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
*/

#include <cstdio>
#include <cstdlib>
#include <cmath>

#include <boost/scoped_array.hpp>

#include "dassert.h"
#include "typedesc.h"
#include "strutil.h"

#define DLL_EXPORT_PUBLIC /* Because we are implementing ImageIO */
#include "imageio.h"
#include "imageio_pvt.h"
#undef DLL_EXPORT_PUBLIC

using namespace OpenImageIO;
using namespace OpenImageIO::pvt;



bool 
ImageInput::read_scanline (int y, int z, TypeDesc format, void *data,
                           stride_t xstride)
{
    m_spec.auto_stride (xstride, format, m_spec.nchannels);
    bool contiguous = (xstride == m_spec.nchannels*format.size());
    if (contiguous && m_spec.format == format)  // Simple case
        return read_native_scanline (y, z, data);

    // Complex case -- either changing data type or stride
    int scanline_values = m_spec.width * m_spec.nchannels;
    unsigned char *buf = (unsigned char *) alloca (m_spec.scanline_bytes());
    bool ok = read_native_scanline (y, z, buf);
    if (! ok)
        return false;
    ok = contiguous 
        ? convert_types (m_spec.format, buf, format, data, scanline_values)
        : convert_image (m_spec.nchannels, m_spec.width, 1, 1, 
                         buf, m_spec.format, AutoStride, AutoStride, AutoStride,
                         data, format, xstride, AutoStride, AutoStride);
    if (! ok)
        error ("ImageInput::read_scanline : no support for format %s",
               m_spec.format.c_str());
    return ok;
}



bool 
ImageInput::read_tile (int x, int y, int z, TypeDesc format, void *data,
                       stride_t xstride, stride_t ystride, stride_t zstride)
{
    m_spec.auto_stride (xstride, ystride, zstride, format,
                        m_spec.nchannels, m_spec.tile_width, m_spec.tile_height);
    bool contiguous = (xstride == m_spec.nchannels*format.size() &&
                       ystride == xstride*m_spec.tile_width &&
                       (zstride == ystride*m_spec.tile_height || zstride == 0));
    if (contiguous && m_spec.format == format)  // Simple case
        return read_native_tile (x, y, z, data);

    // Complex case -- either changing data type or stride
    int tile_values = m_spec.tile_width * m_spec.tile_height * 
                      std::max(1,m_spec.tile_depth) * m_spec.nchannels;
    unsigned char *buf = (unsigned char *) alloca (m_spec.tile_bytes());
    bool ok = read_native_tile (x, y, z, buf);
    if (! ok)
        return false;
    // FIXME -- what happens when the last tile of a row or column extends
    // beyond the borders of the image buffer???
    ok = contiguous 
        ? convert_types (m_spec.format, buf, format, data, tile_values)
        : convert_image (m_spec.nchannels, m_spec.tile_width, m_spec.tile_height, m_spec.tile_depth, 
                         buf, m_spec.format, AutoStride, AutoStride, AutoStride,
                         data, format, xstride, ystride, zstride);
    if (! ok)
        error ("ImageInput::read_tile : no support for format %s",
               m_spec.format.c_str());
    return ok;
}



bool
ImageInput::read_image (TypeDesc format, void *data,
                        stride_t xstride, stride_t ystride, stride_t zstride,
                        OpenImageIO::ProgressCallback progress_callback,
                        void *progress_callback_data)
{
    m_spec.auto_stride (xstride, ystride, zstride, format, m_spec.nchannels,
                        m_spec.width, m_spec.height);
    bool ok = true;
    if (progress_callback)
        if (progress_callback (progress_callback_data, 0.0f))
            return ok;
    if (m_spec.tile_width) {
        // Tiled image

        // Locally allocate a single tile to gracefully deal with image
        // dimensions smaller than a tile, or if one of the tiles runs
        // past the right or bottom edge.  Then we copy from our tile to
        // the user data, only copying valid pixel ranges.
        size_t tilexstride = m_spec.nchannels * format.size();
        size_t tileystride = tilexstride * m_spec.tile_width;
        size_t tilezstride = tileystride * m_spec.tile_height;
        size_t tile_values = (size_t)m_spec.tile_width * (size_t)m_spec.tile_height *
            (size_t)std::max(1,m_spec.tile_depth) * m_spec.nchannels;
        boost::scoped_array<char> pels (new char [tile_values * format.size()]);
        for (int z = 0;  z < m_spec.depth;  z += m_spec.tile_depth)
            for (int y = 0;  y < m_spec.height;  y += m_spec.tile_height) {
                for (int x = 0;  x < m_spec.width && ok;  x += m_spec.tile_width) {
#if 0
                    ok &= read_tile (x+m_spec.x, y+m_spec.y, z+m_spec.z, format,
                                     (char *)data + z*zstride + y*ystride + x*xstride,
                                     xstride, ystride, zstride);
#endif
                    ok &= read_tile (x+m_spec.x, y+m_spec.y, z+m_spec.z,
                                     format, &pels[0]);
                    // Now copy out the scanlines
                    int ntz = std::min (z+m_spec.tile_depth, m_spec.depth) - z;
                    int nty = std::min (y+m_spec.tile_height, m_spec.height) - y;
                    int ntx = std::min (x+m_spec.tile_width, m_spec.width) - x;
                    for (int tz = 0;  tz < ntz;  ++tz) {
                        for (int ty = 0;  ty < nty;  ++ty) {
                            // FIXME -- doesn't work for non-contiguous scanlines
                            memcpy ((char *)data + x*xstride + (y+ty)*ystride + (z+tz)*zstride,
                                    &pels[ty*tileystride+tz*tilezstride],
                                    ntx*tilexstride);
                        }
                    }
//                    return ok; // DEBUG -- just try very first tile
                }
                if (progress_callback)
                    if (progress_callback (progress_callback_data, (float)y/m_spec.height))
                        return ok;
            }
    } else {
        // Scanline image
        for (int z = 0;  z < m_spec.depth;  ++z)
            for (int y = 0;  y < m_spec.height && ok;  ++y) {
                ok &= read_scanline (y+m_spec.y, z+m_spec.z, format,
                                     (char *)data + z*zstride + y*ystride,
                                     xstride);
                if (progress_callback && !(y & 0x0f))
                    if (progress_callback (progress_callback_data, (float)y/m_spec.height))
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
ImageInput::error (const char *format, ...)
{
    va_list ap;
    va_start (ap, format);
    m_errmessage = Strutil::vformat (format, ap);
    va_end (ap);
}

