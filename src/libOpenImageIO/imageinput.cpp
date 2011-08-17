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
#include <vector>

#include "dassert.h"
#include "typedesc.h"
#include "strutil.h"
#include "fmath.h"

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
ImageInput::read_scanlines (int ybegin, int yend, int z,
                            TypeDesc format, void *data,
                            stride_t xstride, stride_t ystride)
{
    yend = std::min (yend, spec().y+spec().height);
    size_t native_pixel_bytes = m_spec.pixel_bytes (true);
    imagesize_t native_scanline_bytes = m_spec.scanline_bytes (true);
    bool native = (format == TypeDesc::UNKNOWN);
    size_t pixel_bytes = native ? m_spec.pixel_bytes (native)
                                : (format.size()*m_spec.nchannels);
    if (native && xstride == AutoStride)
        xstride = pixel_bytes;
    stride_t zstride = AutoStride;
    m_spec.auto_stride (xstride, ystride, zstride, format, m_spec.nchannels,
                        m_spec.width, m_spec.height);
    bool contiguous = (xstride == (stride_t) native_pixel_bytes &&
                       ystride == (stride_t) native_scanline_bytes);
    // If user's format and strides are set up to accept the native data
    // layout, read the scanlines directly into the user's buffer.
    bool rightformat = (format == TypeDesc::UNKNOWN) ||
        (format == m_spec.format && m_spec.channelformats.empty());
    if (rightformat && contiguous)
        return read_native_scanlines (ybegin, yend, z, data);

    // No such luck.  Read scanlines in chunks.

    const imagesize_t limit = 16*1024*1024;   // Allocate 16 MB, or 1 scanline
    int chunk = std::max (1, int(limit / native_scanline_bytes));
    unsigned char *buf = new unsigned char [chunk * native_scanline_bytes];

    bool ok = true;
    int scanline_values = m_spec.width * m_spec.nchannels;
    for (;  ok && ybegin < yend;  ybegin += chunk) {
        int y1 = std::min (ybegin+chunk, yend);
        ok &= read_native_scanlines (ybegin, y1, z, data);
        if (! ok)
            break;

        int nscanlines = y1 - ybegin;
        int chunkvalues = scanline_values * nscanlines;
        if (m_spec.channelformats.empty()) {
            // No per-channel formats -- do the conversion in one shot
            if (contiguous)
                ok = convert_types (m_spec.format, buf, format, data, chunkvalues);
            else {
                ok = convert_image (m_spec.nchannels, m_spec.width, nscanlines, 1, 
                                    buf, m_spec.format, AutoStride, AutoStride, AutoStride,
                                    data, format, xstride, ystride, zstride);
            }
        } else {
            // Per-channel formats -- have to convert/copy channels individually
            size_t offset = 0;
            for (size_t c = 0;  ok && c < m_spec.channelformats.size();  ++c) {
                TypeDesc chanformat = m_spec.channelformats[c];
                ok = convert_image (1 /* channels */, m_spec.width, nscanlines, 1, 
                                    buf+offset, chanformat, 
                                    native_pixel_bytes, AutoStride, AutoStride,
                                    (char *)data + c*m_spec.format.size(),
                                    format, xstride, ystride, zstride);
                offset += chanformat.size ();
            }
        }
        if (! ok)
            error ("ImageInput::read_scanlines : no support for format %s",
                   m_spec.format.c_str());
        data = (char *)data + ystride*nscanlines;
    }
    delete [] buf;
    return ok;
}



bool
ImageInput::read_native_scanlines (int ybegin, int yend, int z, void *data)
{
    // Base class implementation of read_native_scanlines just repeatedly
    // calls read_native_scanline, which is supplied by every plugin.
    // Only the hardcore ones will overload read_native_scanlines with
    // their own implementation.
    size_t ystride = m_spec.scanline_bytes (true);
    yend = std::min (yend, spec().y+spec().height);
    for (int y = ybegin;  y < yend;  ++y) {
        bool ok = read_native_scanline (y, z, data);
        if (! ok)
            return false;
        data = (char *)data + ystride;
    }
    return true;
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
    size_t tile_values = (size_t)m_spec.tile_pixels() * m_spec.nchannels;

    std::vector<char> buf (m_spec.tile_bytes(true));
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
ImageInput::read_tiles (int xbegin, int xend, int ybegin, int yend,
                        int zbegin, int zend, TypeDesc format, void *data,
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
                        xend-xbegin, yend-ybegin);
    // Do the strides indicate that the data area is contiguous?
    bool contiguous = (native_data && xstride == native_pixel_bytes) ||
        (!native_data && xstride == (stride_t)m_spec.pixel_bytes(false));
    contiguous &= (ystride == xstride*(xend-xbegin) &&
                   (zstride == ystride*(yend-ybegin) || (zend-zbegin) <= 1));

    int nxtiles = (xend - xbegin + m_spec.tile_width - 1) / m_spec.tile_width;
    int nytiles = (yend - ybegin + m_spec.tile_height - 1) / m_spec.tile_height;
    int nztiles = (zend - zbegin + m_spec.tile_depth - 1) / m_spec.tile_depth;

    // If user's format and strides are set up to accept the native data
    // layout, and we're asking for a whole number of tiles (no partial
    // tiles at the edges), then read the tile directly into the user's
    // buffer.
    if (native_data && contiguous &&
        (xend-xbegin) == nxtiles*m_spec.tile_width &&
        (yend-ybegin) == nytiles*m_spec.tile_height &&
        (zend-zbegin) == nztiles*m_spec.tile_depth) {
        return read_native_tiles (xbegin, xend, ybegin, yend, zbegin, zend,
                                  data);  // Simple case
    }

    // No such luck.  Just punt and read tiles individually.
    bool ok = true;
    stride_t pixelsize = native_data ? native_pixel_bytes
                                     : (format.size() * m_spec.nchannels);
    std::vector<char> buf;
    for (int z = zbegin;  z < zend;  z += std::max(1,m_spec.tile_depth)) {
        int zd = std::min (zend-z, m_spec.tile_depth);
        for (int y = ybegin;  y < yend;  y += m_spec.tile_height) {
            char *tilestart = ((char *)data + (z-zbegin)*zstride
                               + (y-ybegin)*ystride);
            int yh = std::min (yend-y, m_spec.tile_height);
            for (int x = xbegin;  ok && x < xend;  x += m_spec.tile_width) {
                int xw = std::min (xend-x, m_spec.tile_width);
                // Full tiles are read directly into the user buffer, but
                // partial tiles (such as at the image edge) are read into a
                // buffer and then copied.
                if (xw == m_spec.tile_width && yh == m_spec.tile_height &&
                    zd == m_spec.tile_depth) {
                    ok &= read_tile (x, y, z, format, tilestart,
                                     xstride, ystride, zstride);
                } else {
                    buf.resize (pixelsize * m_spec.tile_pixels());
                    ok &= read_tile (x, y, z, format, &buf[0],
                                     pixelsize, pixelsize*m_spec.tile_width,
                                     pixelsize*m_spec.tile_pixels());
                    if (ok)
                        copy_image (m_spec.nchannels, xw, yh, zd, &buf[0],
                                    pixelsize, pixelsize,
                                    pixelsize*m_spec.tile_width,
                                    pixelsize*m_spec.tile_pixels(),
                                    tilestart, xstride, ystride, zstride);
                }
                tilestart += m_spec.tile_width * xstride;
            }
        }
    }

    if (! ok)
        error ("ImageInput::read_tiles : no support for format %s",
               m_spec.format.c_str());
    return ok;
}



bool
ImageInput::read_native_tiles (int xbegin, int xend, int ybegin, int yend,
                               int zbegin, int zend, void *data)
{
    // Base class implementation of read_native_tiles just repeatedly
    // calls read_native_tile, which is supplied by every plugin that
    // supports tiles.  Only the hardcore ones will overload
    // read_native_tiles with their own implementation.
    stride_t pixel_bytes = (stride_t) m_spec.pixel_bytes (true);
    stride_t tileystride = pixel_bytes * m_spec.tile_width;
    stride_t tilezstride = tileystride * m_spec.tile_height;
    stride_t ystride = (xend-xbegin) * pixel_bytes;
    stride_t zstride = (yend-ybegin) * ystride;
    std::vector<char> pels (m_spec.tile_bytes(true));
    for (int z = zbegin;  z < zend;  z += m_spec.tile_depth) {
        for (int y = ybegin;  y < yend;  y += m_spec.tile_height) {
            for (int x = xbegin;  x < xend;  x += m_spec.tile_width) {
                bool ok = read_native_tile (x, y, z, &pels[0]);
                if (! ok)
                    return false;
                copy_image (m_spec.nchannels, m_spec.tile_width,
                            m_spec.tile_height, m_spec.tile_depth,
                            &pels[0], size_t(pixel_bytes),
                            pixel_bytes, tileystride, tilezstride,
                            (char *)data+ (z-zbegin)*zstride + 
                                (y-ybegin)*ystride + (x-xbegin)*pixel_bytes,
                            pixel_bytes, ystride, zstride);
            }
        }
    }
    return true;
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
        for (int z = 0;  z < m_spec.depth;  z += m_spec.tile_depth) {
            for (int y = 0;  y < m_spec.height;  y += m_spec.tile_height) {
                ok &= read_tiles (m_spec.x, m_spec.x+m_spec.width,
                                  y+m_spec.y, std::min (y+m_spec.y+m_spec.tile_height, m_spec.y+m_spec.height),
                                  z+m_spec.z, std::min (z+m_spec.z+m_spec.tile_depth, m_spec.z+m_spec.depth),
                                  format, (char *)data + z*zstride + y*ystride,
                                  xstride, ystride, zstride);
                if (progress_callback &&
                    progress_callback (progress_callback_data, (float)y/m_spec.height))
                    return ok;
            }
        }
    } else {
        // Scanline image -- rely on read_scanlines, in chunks of 64
        const int chunk = 256;
        for (int z = 0;  z < m_spec.depth;  ++z)
            for (int y = 0;  y < m_spec.height && ok;  y += chunk) {
                int yend = std::min (y+m_spec.y+chunk, m_spec.y+m_spec.height);
                ok &= read_scanlines (y+m_spec.y, yend, z+m_spec.z, format,
                                      (char *)data + z*zstride + y*ystride,
                                      xstride, ystride);
                if (progress_callback)
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
