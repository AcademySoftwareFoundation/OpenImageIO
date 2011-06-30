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

#include "imageio.h"
#include "imageio_pvt.h"

OIIO_NAMESPACE_ENTER
{
    using namespace pvt;


bool 
ImageInput::read_scanline (int y, int z, TypeDesc format, void *data,
                           stride_t xstride)
{
    // native_pixel_bytes is the size of a pixel in the FILE, including
    // the per-channel format.
    stride_t native_pixel_bytes = (stride_t) m_spec.pixel_bytes (true);
    // perchanfile is true if the file has different per-channel formats
    bool perchanfile = m_spec.channelformats.size();
    // native_data is true if the user asking for data in the native format
    bool native_data = (format == TypeDesc::UNKNOWN ||
                        (format == m_spec.format && !perchanfile));
    if (native_data && xstride == AutoStride)
        xstride = native_pixel_bytes;
    else
        m_spec.auto_stride (xstride, format, m_spec.nchannels);
    // Do the strides indicate that the data area is contiguous?
    bool contiguous = (native_data && xstride == native_pixel_bytes) ||
        (!native_data && xstride == (stride_t)m_spec.pixel_bytes(false));

    // If user's format and strides are set up to accept the native data
    // layout, read the scanline directly into the user's buffer.
    if (native_data && contiguous)
        return read_native_scanline (y, z, data);

    // Complex case -- either changing data type or stride
    int scanline_values = m_spec.width * m_spec.nchannels;
    unsigned char *buf = (unsigned char *) alloca (m_spec.scanline_bytes(true));
    bool ok = read_native_scanline (y, z, buf);
    if (! ok)
        return false;
    if (! perchanfile) {
        // No per-channel formats -- do the conversion in one shot
        ok = contiguous 
            ? convert_types (m_spec.format, buf, format, data, scanline_values)
            : convert_image (m_spec.nchannels, m_spec.width, 1, 1, 
                             buf, m_spec.format, AutoStride, AutoStride, AutoStride,
                             data, format, xstride, AutoStride, AutoStride);
    } else {
        // Per-channel formats -- have to convert/copy channels individually
        if (native_data) {
            ASSERT (contiguous && "Per-channel native input requires contiguous strides");
        }
        ASSERT (format != TypeDesc::UNKNOWN);
        ASSERT (m_spec.channelformats.size() == (size_t)m_spec.nchannels);
        size_t offset = 0;
        for (int c = 0;  c < m_spec.nchannels;  ++c) {
            TypeDesc chanformat = m_spec.channelformats[c];
            ok = convert_image (1 /* channels */, m_spec.width, 1, 1, 
                                buf+offset, chanformat, 
                                native_pixel_bytes, AutoStride, AutoStride,
                                (char *)data + c*format.size(),
                                format, xstride, AutoStride, AutoStride);
            offset += chanformat.size ();
        }
    }

    if (! ok)
        error ("ImageInput::read_scanline : no support for format %s",
               m_spec.format.c_str());
    return ok;
}



bool 
ImageInput::read_tile (int x, int y, int z, TypeDesc format, void *data,
                       stride_t xstride, stride_t ystride, stride_t zstride)
{
    // native_pixel_bytes is the size of a pixel in the FILE, including
    // the per-channel format.
    stride_t native_pixel_bytes = (stride_t) m_spec.pixel_bytes (true);
    // perchanfile is true if the file has different per-channel formats
    bool perchanfile = m_spec.channelformats.size();
    // native_data is true if the user asking for data in the native format
    bool native_data = (format == TypeDesc::UNKNOWN ||
                        (format == m_spec.format && !perchanfile));
    if (format == TypeDesc::UNKNOWN && xstride == AutoStride)
        xstride = native_pixel_bytes;
    m_spec.auto_stride (xstride, ystride, zstride, format, m_spec.nchannels,
                        m_spec.tile_width, m_spec.tile_height);
    // Do the strides indicate that the data area is contiguous?
    bool contiguous = (native_data && xstride == native_pixel_bytes) ||
        (!native_data && xstride == (stride_t)m_spec.pixel_bytes(false));
    contiguous &= (ystride == xstride*m_spec.tile_width &&
                   (zstride == ystride*m_spec.tile_height || zstride == 0));

    // If user's format and strides are set up to accept the native data
    // layout, read the tile directly into the user's buffer.
    if (native_data && contiguous)
        return read_native_tile (x, y, z, data);  // Simple case

    // Complex case -- either changing data type or stride
    int tile_values = m_spec.tile_width * m_spec.tile_height * 
                      std::max(1,m_spec.tile_depth) * m_spec.nchannels;

    boost::scoped_array<char> buf (new char [m_spec.tile_bytes(true)]);
    bool ok = read_native_tile (x, y, z, &buf[0]);
    if (! ok)
        return false;
    if (! perchanfile) {
        // No per-channel formats -- do the conversion in one shot
        ok = contiguous 
            ? convert_types (m_spec.format, &buf[0], format, data, tile_values)
            : convert_image (m_spec.nchannels, m_spec.tile_width, m_spec.tile_height, m_spec.tile_depth, 
                             &buf[0], m_spec.format, AutoStride, AutoStride, AutoStride,
                             data, format, xstride, ystride, zstride);
    } else {
        // Per-channel formats -- have to convert/copy channels individually
        if (native_data) {
            ASSERT (contiguous && "Per-channel native input requires contiguous strides");
        }
        ASSERT (format != TypeDesc::UNKNOWN);
        ASSERT (m_spec.channelformats.size() == (size_t)m_spec.nchannels);
        size_t offset = 0;
        for (int c = 0;  c < m_spec.nchannels;  ++c) {
            TypeDesc chanformat = m_spec.channelformats[c];
            ok = convert_image (1 /* channels */, m_spec.tile_width,
                                m_spec.tile_height, m_spec.tile_depth,
                                &buf[offset], chanformat, 
                                native_pixel_bytes, AutoStride, AutoStride,
                                (char *)data + c*format.size(),
                                format, xstride, AutoStride, AutoStride);
            offset += chanformat.size ();
        }
    }

    if (! ok)
        error ("ImageInput::read_tile : no support for format %s",
               m_spec.format.c_str());
    return ok;
}



bool
ImageInput::read_image (TypeDesc format, void *data,
                        stride_t xstride, stride_t ystride, stride_t zstride,
                        ProgressCallback progress_callback,
                        void *progress_callback_data)
{
    bool native = (format == TypeDesc::UNKNOWN);
    stride_t pixel_bytes = native ? (stride_t) m_spec.pixel_bytes (native)
                                  : (stride_t) (format.size()*m_spec.nchannels);
    if (native && xstride == AutoStride)
        xstride = pixel_bytes;
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
        stride_t tilexstride = pixel_bytes;
        stride_t tileystride = tilexstride * m_spec.tile_width;
        stride_t tilezstride = tileystride * m_spec.tile_height;
        imagesize_t tile_pixels = m_spec.tile_pixels();
        std::vector<char> pels (tile_pixels * pixel_bytes);
        for (int z = 0;  z < m_spec.depth;  z += m_spec.tile_depth)
            for (int y = 0;  y < m_spec.height;  y += m_spec.tile_height) {
                for (int x = 0;  x < m_spec.width && ok;  x += m_spec.tile_width) {
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
    ASSERT (m_errmessage.size() < 1024*1024*16 &&
            "Accumulated error messages > 16MB. Try checking return codes!");
    if (m_errmessage.size())
        m_errmessage += '\n';
    m_errmessage += Strutil::vformat (format, ap);
    va_end (ap);
}

bool
ImageInput::read_native_tile (int x, int y, int z, void * data)
{
    return false;
}

}
OIIO_NAMESPACE_EXIT
