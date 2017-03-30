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
#include <memory>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/deepdata.h>
#include "imageio_pvt.h"


OIIO_NAMESPACE_BEGIN
    using namespace pvt;



ImageInput::ImageInput ()
    : m_threads(0)
{
}



ImageInput::~ImageInput ()
{
}



// Default implementation of valid_file: try to do a full open.  If it
// succeeds, it's the right kind of file.  We assume that most plugins
// will override this with something smarter and much less expensive,
// like reading just the first few bytes of the file to check for magic
// numbers.
bool
ImageInput::valid_file (const std::string &filename) const
{
    ImageSpec tmpspec;
    bool ok = const_cast<ImageInput *>(this)->open (filename, tmpspec);
    if (ok)
        const_cast<ImageInput *>(this)->close ();
    return ok;
}



ImageInput *
ImageInput::open (const std::string &filename,
                  const ImageSpec *config)
{
    if (config == NULL) {
        // Without config, this is really just a call to create-with-open.
        return ImageInput::create (filename, true, std::string());
    }

    // With config, create without open, then try to open with config.
    ImageInput *in = ImageInput::create (filename, false, std::string());
    if (! in)
        return NULL;  // create() failed
    ImageSpec newspec;
    if (in->open (filename, newspec, *config))
        return in;   // creted fine, opened fine, return it

    // The open failed.  Transfer the error from 'in' to the global OIIO
    // error, delete the ImageInput we allocated, and return NULL.
    std::string err = in->geterror();
    if (err.size())
        pvt::error ("%s", err.c_str());
    delete in;
    return NULL;
}



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
        ASSERT (m_spec.channelformats.size() == (size_t)m_spec.nchannels);
        size_t offset = 0;
        for (int c = 0;  ok && c < m_spec.nchannels;  ++c) {
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
    return read_scanlines (ybegin, yend, z, 0, m_spec.nchannels,
                           format, data, xstride, ystride);
}



bool
ImageInput::read_scanlines (int ybegin, int yend, int z,
                            int chbegin, int chend,
                            TypeDesc format, void *data,
                            stride_t xstride, stride_t ystride)
{
    chend = clamp (chend, chbegin+1, m_spec.nchannels);
    int nchans = chend - chbegin;
    yend = std::min (yend, spec().y+spec().height);
    size_t native_pixel_bytes = m_spec.pixel_bytes (chbegin, chend, true);
    imagesize_t native_scanline_bytes = clamped_mult64 ((imagesize_t)m_spec.width,
                                                        (imagesize_t)native_pixel_bytes);
    bool native = (format == TypeDesc::UNKNOWN);
    size_t pixel_bytes = native ? native_pixel_bytes : format.size()*nchans;
    if (native && xstride == AutoStride)
        xstride = pixel_bytes;
    stride_t zstride = AutoStride;
    m_spec.auto_stride (xstride, ystride, zstride, format, nchans,
                        m_spec.width, m_spec.height);
    bool contiguous = (xstride == (stride_t) native_pixel_bytes &&
                       ystride == (stride_t) native_scanline_bytes);
    // If user's format and strides are set up to accept the native data
    // layout, read the scanlines directly into the user's buffer.
    bool rightformat = (format == TypeDesc::UNKNOWN) ||
        (format == m_spec.format && m_spec.channelformats.empty());
    if (rightformat && contiguous) {
        if (chbegin == 0 && chend == m_spec.nchannels)
            return read_native_scanlines (ybegin, yend, z, data);
        else
            return read_native_scanlines (ybegin, yend, z, chbegin, chend, data);
    }

    // No such luck.  Read scanlines in chunks.

    const imagesize_t limit = 16*1024*1024;   // Allocate 16 MB, or 1 scanline
    int chunk = std::max (1, int(limit / native_scanline_bytes));
    std::unique_ptr<char[]> buf (new char [chunk * native_scanline_bytes]);

    bool ok = true;
    int scanline_values = m_spec.width * nchans;
    for (;  ok && ybegin < yend;  ybegin += chunk) {
        int y1 = std::min (ybegin+chunk, yend);
        ok &= read_native_scanlines (ybegin, y1, z, chbegin, chend, &buf[0]);
        if (! ok)
            break;

        int nscanlines = y1 - ybegin;
        int chunkvalues = scanline_values * nscanlines;
        if (m_spec.channelformats.empty()) {
            // No per-channel formats -- do the conversion in one shot
            if (contiguous) {
                ok = convert_types (m_spec.format, &buf[0], format, data, chunkvalues);
            } else {
                ok = parallel_convert_image (nchans, m_spec.width, nscanlines, 1, 
                                    &buf[0], m_spec.format, AutoStride, AutoStride, AutoStride,
                                    data, format, xstride, ystride, zstride,
                                    -1 /*alpha*/, -1 /*z*/, threads());
            }
        } else {
            // Per-channel formats -- have to convert/copy channels individually
            size_t offset = 0;
            int n = 1;
            for (int c = 0;  ok && c < nchans; c += n) {
                TypeDesc chanformat = m_spec.channelformats[c+chbegin];
                // Try to do more than one channel at a time to improve
                // memory coherence, if there are groups of adjacent
                // channels needing the same data conversion.
                for (n = 1; c+n < nchans; ++n)
                    if (m_spec.channelformats[c+chbegin+n] != chanformat)
                        break;
                ok = parallel_convert_image (n /* channels */, m_spec.width, nscanlines, 1, 
                                    &buf[offset], chanformat,
                                    native_pixel_bytes, AutoStride, AutoStride,
                                    (char *)data + c*format.size(),
                                    format, xstride, ystride, zstride,
                                    -1 /*alpha*/, -1 /*z*/, threads());
                offset += n * chanformat.size ();
            }
        }
        if (! ok)
            error ("ImageInput::read_scanlines : no support for format %s",
                   m_spec.format.c_str());
        data = (char *)data + ystride*nscanlines;
    }
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
ImageInput::read_native_scanlines (int ybegin, int yend, int z,
                                   int chbegin, int chend, void *data)
{
    // All-channel case just reduces to the simpler read_native_scanlines.
    if (chbegin == 0 && chend >= m_spec.nchannels)
        return read_native_scanlines (ybegin, yend, z, data);

    // Base class implementation of read_native_scanlines (with channel
    // subset) just calls read_native_scanlines (all channels), and
    // copies the appropriate subset.
    size_t prefix_bytes = m_spec.pixel_bytes (0,chbegin,true);
    size_t subset_bytes = m_spec.pixel_bytes (chbegin,chend,true);
    size_t subset_ystride = m_spec.width * subset_bytes;

    size_t native_pixel_bytes = m_spec.pixel_bytes (true);
    size_t native_ystride = m_spec.width * native_pixel_bytes;
    std::unique_ptr<char[]> buf (new char [native_ystride]);
    yend = std::min (yend, spec().y+spec().height);
    for (int y = ybegin;  y < yend;  ++y) {
        bool ok = read_native_scanline (y, z, &buf[0]);
        if (! ok)
            return false;
        for (int x = 0;  x < m_spec.width;  ++x)
            memcpy ((char *)data + subset_bytes*x,
                    &buf[prefix_bytes+native_pixel_bytes*x], subset_bytes);
        data = (char *)data + subset_ystride;
    }
    return true;
}



bool 
ImageInput::read_tile (int x, int y, int z, TypeDesc format, void *data,
                       stride_t xstride, stride_t ystride, stride_t zstride)
{
    if (! m_spec.tile_width ||
        ((x-m_spec.x) % m_spec.tile_width) != 0 ||
        ((y-m_spec.y) % m_spec.tile_height) != 0 ||
        ((z-m_spec.z) % m_spec.tile_depth) != 0)
        return false;   // coordinates are not a tile corner

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

    std::unique_ptr<char[]> buf (new char [m_spec.tile_bytes(true)]);
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
    return read_tiles (xbegin, xend, ybegin, yend, zbegin, zend,
                       0, m_spec.nchannels, format, data,
                       xstride, ystride, zstride);
}




bool 
ImageInput::read_tiles (int xbegin, int xend, int ybegin, int yend,
                        int zbegin, int zend, 
                        int chbegin, int chend,
                        TypeDesc format, void *data,
                        stride_t xstride, stride_t ystride, stride_t zstride)
{
    if (! m_spec.valid_tile_range (xbegin, xend, ybegin, yend, zbegin, zend))
        return false;

    chend = clamp (chend, chbegin+1, m_spec.nchannels);
    int nchans = chend - chbegin;
    // native_pixel_bytes is the size of a pixel in the FILE, including
    // the per-channel format.
    stride_t native_pixel_bytes = (stride_t) m_spec.pixel_bytes (chbegin, chend, true);
    // perchanfile is true if the file has different per-channel formats
    bool perchanfile = m_spec.channelformats.size();
    // native_data is true if the user asking for data in the native format
    bool native_data = (format == TypeDesc::UNKNOWN ||
                        (format == m_spec.format && !perchanfile));
    if (format == TypeDesc::UNKNOWN && xstride == AutoStride)
        xstride = native_pixel_bytes;
    m_spec.auto_stride (xstride, ystride, zstride, format, nchans,
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
        if (chbegin == 0 && chend == m_spec.nchannels)
            return read_native_tiles (xbegin, xend, ybegin, yend, zbegin, zend,
                                      data);  // Simple case
        else
            return read_native_tiles (xbegin, xend, ybegin, yend, zbegin, zend,
                                      chbegin, chend, data);
    }

    // No such luck.  Just punt and read tiles individually.
    bool ok = true;
    stride_t pixelsize = native_data ? native_pixel_bytes 
                                     : (format.size() * nchans);
    stride_t full_pixelsize = native_data ? m_spec.pixel_bytes(true)
                                          : (format.size() * m_spec.nchannels);
    stride_t full_tilewidthbytes = full_pixelsize * m_spec.tile_width;
    stride_t full_tilewhbytes = full_tilewidthbytes * m_spec.tile_height;
    stride_t full_tilebytes = full_tilewhbytes * m_spec.tile_depth;
    size_t prefix_bytes = native_data ? m_spec.pixel_bytes (0,chbegin,true)
                                      : format.size() * chbegin;
    std::vector<char> buf;
    for (int z = zbegin;  z < zend;  z += std::max(1,m_spec.tile_depth)) {
        int zd = std::min (zend-z, m_spec.tile_depth);
        for (int y = ybegin;  y < yend;  y += m_spec.tile_height) {
            char *tilestart = ((char *)data + (z-zbegin)*zstride
                               + (y-ybegin)*ystride);
            int yh = std::min (yend-y, m_spec.tile_height);
            for (int x = xbegin;  ok && x < xend;  x += m_spec.tile_width) {
                int xw = std::min (xend-x, m_spec.tile_width);
                // Full tiles are read directly into the user buffer,
                // but partial tiles (such as at the image edge) or
                // partial channel subsets are read into a buffer and
                // then copied.
                if (xw == m_spec.tile_width && yh == m_spec.tile_height &&
                      zd == m_spec.tile_depth && !perchanfile &&
                      chbegin == 0 && chend == m_spec.nchannels) {
                    // Full tile, either native data or not needing
                    // per-tile data format conversion.
                    ok &= read_tile (x, y, z, format, tilestart,
                                     xstride, ystride, zstride);
                    if (! ok)
                        return false;
                } else {
                    buf.resize (full_tilebytes);
                    ok &= read_tile (x, y, z, format,
                                     &buf[0], full_pixelsize,
                                     full_tilewidthbytes, full_tilewhbytes);
                    if (ok)
                        copy_image (nchans, xw, yh, zd, &buf[prefix_bytes],
                                    pixelsize, full_pixelsize,
                                    full_tilewidthbytes, full_tilewhbytes,
                                    tilestart, xstride, ystride, zstride);
                    // N.B. It looks like read_tiles doesn't handle the
                    // per-channel data types case fully, but it does!
                    // The call to read_tile() above handles the case of
                    // per-channel data types, converting to to desired
                    // format, so all we have to do on our own is the
                    // copy_image.
                }
                tilestart += m_spec.tile_width * xstride;
            }
            if (! ok)
                break;
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
    if (! m_spec.valid_tile_range (xbegin, xend, ybegin, yend, zbegin, zend))
        return false;

    // Base class implementation of read_native_tiles just repeatedly
    // calls read_native_tile, which is supplied by every plugin that
    // supports tiles.  Only the hardcore ones will overload
    // read_native_tiles with their own implementation.
    stride_t pixel_bytes = (stride_t) m_spec.pixel_bytes (true);
    stride_t tileystride = pixel_bytes * m_spec.tile_width;
    stride_t tilezstride = tileystride * m_spec.tile_height;
    stride_t ystride = (xend-xbegin) * pixel_bytes;
    stride_t zstride = (yend-ybegin) * ystride;
    std::unique_ptr<char[]> pels (new char [m_spec.tile_bytes(true)]);
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
ImageInput::read_native_tiles (int xbegin, int xend, int ybegin, int yend,
                               int zbegin, int zend, 
                               int chbegin, int chend, void *data)
{
    chend = clamp (chend, chbegin+1, m_spec.nchannels);
    int nchans = chend - chbegin;

    // All-channel case just reduces to the simpler read_native_scanlines.
    if (chbegin == 0 && chend >= m_spec.nchannels)
        return read_native_tiles (xbegin, xend, ybegin, yend,
                                  zbegin, zend, data);

    if (! m_spec.valid_tile_range (xbegin, xend, ybegin, yend, zbegin, zend))
        return false;

    // Base class implementation of read_native_tiles just repeatedly
    // calls read_native_tile, which is supplied by every plugin that
    // supports tiles.  Only the hardcore ones will overload
    // read_native_tiles with their own implementation.

    stride_t native_pixel_bytes = (stride_t) m_spec.pixel_bytes (true);
    stride_t native_tileystride = native_pixel_bytes * m_spec.tile_width;
    stride_t native_tilezstride = native_tileystride * m_spec.tile_height;

    size_t prefix_bytes = m_spec.pixel_bytes (0,chbegin,true);
    size_t subset_bytes = m_spec.pixel_bytes (chbegin,chend,true);
    stride_t subset_ystride = (xend-xbegin) * subset_bytes;
    stride_t subset_zstride = (yend-ybegin) * subset_ystride;

    std::unique_ptr<char[]> pels (new char [m_spec.tile_bytes(true)]);
    for (int z = zbegin;  z < zend;  z += m_spec.tile_depth) {
        for (int y = ybegin;  y < yend;  y += m_spec.tile_height) {
            for (int x = xbegin;  x < xend;  x += m_spec.tile_width) {
                bool ok = read_native_tile (x, y, z, &pels[0]);
                if (! ok)
                    return false;
                copy_image (nchans, m_spec.tile_width,
                            m_spec.tile_height, m_spec.tile_depth,
                            &pels[prefix_bytes], subset_bytes,
                            native_pixel_bytes, native_tileystride,
                            native_tilezstride,
                            (char *)data+ (z-zbegin)*subset_zstride + 
                                (y-ybegin)*subset_ystride +
                                (x-xbegin)*subset_bytes,
                            subset_bytes, subset_ystride, subset_zstride);
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
    return read_image (0, -1, format, data, xstride, ystride, zstride,
                       progress_callback, progress_callback_data);
}



bool
ImageInput::read_image (int chbegin, int chend, TypeDesc format, void *data,
                        stride_t xstride, stride_t ystride, stride_t zstride,
                        ProgressCallback progress_callback,
                        void *progress_callback_data)
{
    if (chend < 0)
        chend = m_spec.nchannels;
    chend = clamp (chend, chbegin+1, m_spec.nchannels);
    int nchans = chend - chbegin;
    bool native = (format == TypeDesc::UNKNOWN);
    stride_t pixel_bytes = native ? (stride_t) m_spec.pixel_bytes (chbegin, chend, native)
                                  : (stride_t) (format.size()*nchans);
    if (native && xstride == AutoStride)
        xstride = pixel_bytes;
    m_spec.auto_stride (xstride, ystride, zstride, format, nchans,
                        m_spec.width, m_spec.height);
    bool ok = true;
    if (progress_callback)
        if (progress_callback (progress_callback_data, 0.0f))
            return ok;
    if (m_spec.tile_width) {
        // Tiled image
        for (int z = 0;  z < m_spec.depth;  z += m_spec.tile_depth) {
            for (int y = 0;  y < m_spec.height && ok;  y += m_spec.tile_height) {
                ok &= read_tiles (m_spec.x, m_spec.x+m_spec.width,
                                  y+m_spec.y, std::min (y+m_spec.y+m_spec.tile_height, m_spec.y+m_spec.height),
                                  z+m_spec.z, std::min (z+m_spec.z+m_spec.tile_depth, m_spec.z+m_spec.depth),
                                  chbegin, chend,
                                  format, (char *)data + z*zstride + y*ystride,
                                  xstride, ystride, zstride);
                if (progress_callback &&
                    progress_callback (progress_callback_data, (float)y/m_spec.height))
                    return ok;
            }
        }
    } else {
        // Scanline image -- rely on read_scanlines, in chunks of oiio_read_chunk
        int read_chunk = oiio_read_chunk;
        if (!read_chunk) {
            read_chunk = m_spec.height;
        }
        for (int z = 0;  z < m_spec.depth;  ++z)
            for (int y = 0;  y < m_spec.height && ok;  y += read_chunk) {
                int yend = std::min (y+m_spec.y+read_chunk, m_spec.y+m_spec.height);
                ok &= read_scanlines (y+m_spec.y, yend, z+m_spec.z,
                                      chbegin, chend, format,
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



bool
ImageInput::read_native_deep_scanlines (int ybegin, int yend, int z,
                                        int chbegin, int chend,
                                        DeepData &deepdata)
{
    return false;  // default: doesn't support deep images
}



bool
ImageInput::read_native_deep_tiles (int xbegin, int xend,
                                    int ybegin, int yend,
                                    int zbegin, int zend,
                                    int chbegin, int chend,
                                    DeepData &deepdata)
{
    return false;  // default: doesn't support deep images
}



bool
ImageInput::read_native_deep_image (DeepData &deepdata)
{
    if (m_spec.depth > 1) {
        error ("read_native_deep_image is not supported for volume (3D) images.");
        return false;
        // FIXME? - not implementing 3D deep images for now.  The only
        // format that supports deep images at this time is OpenEXR, and
        // it doesn't support volumes.
    }
    if (m_spec.tile_width) {
        // Tiled image
        return read_native_deep_tiles (m_spec.x, m_spec.x+m_spec.width,
                                       m_spec.y, m_spec.y+m_spec.height,
                                       m_spec.z, m_spec.z+m_spec.depth,
                                       0, m_spec.nchannels, deepdata);
    } else {
        // Scanline image
        return read_native_deep_scanlines (m_spec.y, m_spec.y+m_spec.height, 0,
                                           0, m_spec.nchannels, deepdata);
    }
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
ImageInput::append_error (const std::string& message) const
{
    ASSERT (m_errmessage.size() < 1024*1024*16 &&
            "Accumulated error messages > 16MB. Try checking return codes!");
    if (m_errmessage.size())
        m_errmessage += '\n';
    m_errmessage += message;
}

bool
ImageInput::read_native_tile (int x, int y, int z, void * data)
{
    return false;
}

OIIO_NAMESPACE_END
