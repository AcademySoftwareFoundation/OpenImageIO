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
#include <cstdarg>
#include <iostream>
#include <vector>

#include "dassert.h"
#include "typedesc.h"
#include "filesystem.h"
#include "plugin.h"
#include "thread.h"
#include "strutil.h"

#include "imageio.h"
#include "imageio_pvt.h"


OIIO_NAMESPACE_ENTER
{
    using namespace pvt;

bool
ImageOutput::write_scanline (int y, int z, TypeDesc format,
                             const void *data, stride_t xstride)
{
    // Default implementation: don't know how to write scanlines
    return false;
}



bool
ImageOutput::write_scanlines (int ybegin, int yend, int z,
                              TypeDesc format, const void *data,
                              stride_t xstride, stride_t ystride)
{
    // Default implementation: write each scanline individually
    stride_t native_pixel_bytes = (stride_t) m_spec.pixel_bytes (true);
    if (format == TypeDesc::UNKNOWN && xstride == AutoStride)
        xstride = native_pixel_bytes;
    stride_t zstride = AutoStride;
    m_spec.auto_stride (xstride, ystride, zstride, format, m_spec.nchannels,
                        m_spec.width, yend-ybegin);
    bool ok = true;
    for (int y = ybegin;  ok && y < yend;  ++y) {
        ok &= write_scanline (y, z, format, data, xstride);
        data = (char *)data + ystride;
    }
    return ok;
}



bool
ImageOutput::write_tile (int x, int y, int z, TypeDesc format,
                         const void *data, stride_t xstride,
                         stride_t ystride,
                         stride_t zstride)
{
    // Default implementation: don't know how to write tiles
    return false;
}



bool ImageOutput::write_tiles (int xbegin, int xend, int ybegin, int yend,
                               int zbegin, int zend, TypeDesc format,
                               const void *data, stride_t xstride,
                               stride_t ystride, stride_t zstride)
{
    if (! m_spec.valid_tile_range (xbegin, xend, ybegin, yend, zbegin, zend))
        return false;

    // Default implementation: write each tile individually
    stride_t native_pixel_bytes = (stride_t) m_spec.pixel_bytes (true);
    if (format == TypeDesc::UNKNOWN && xstride == AutoStride)
        xstride = native_pixel_bytes;
    m_spec.auto_stride (xstride, ystride, zstride, format, m_spec.nchannels,
                        xend-xbegin, yend-ybegin);

    bool ok = true;
    stride_t pixelsize = format.size() * m_spec.nchannels;
    std::vector<char> buf;
    for (int z = zbegin;  z < zend;  z += std::max(1,m_spec.tile_depth)) {
        int zd = std::min (zend-z, m_spec.tile_depth);
        for (int y = ybegin;  y < yend;  y += m_spec.tile_height) {
            char *tilestart = ((char *)data + (z-zbegin)*zstride
                               + (y-ybegin)*ystride);
            int yh = std::min (yend-y, m_spec.tile_height);
            for (int x = xbegin;  ok && x < xend;  x += m_spec.tile_width) {
                int xw = std::min (xend-x, m_spec.tile_width);
                // Full tiles are written directly into the user buffer, but
                // Partial tiles (such as at the image edge) are copied into
                // a padded buffer to stage them.
                if (xw == m_spec.tile_width && yh == m_spec.tile_height &&
                    zd == m_spec.tile_depth) {
                    ok &= write_tile (x, y, z, format, tilestart,
                                     xstride, ystride, zstride);
                } else {
                    buf.resize (pixelsize * m_spec.tile_pixels());
                    OIIO_NAMESPACE::copy_image (m_spec.nchannels, xw, yh, zd,
                                tilestart, pixelsize, xstride, ystride, zstride,
                                &buf[0], pixelsize, pixelsize*m_spec.tile_width,
                                pixelsize*m_spec.tile_pixels());
                    ok &= write_tile (x, y, z, format, &buf[0],
                                      pixelsize, pixelsize*m_spec.tile_width,
                                      pixelsize*m_spec.tile_pixels());
                }
                tilestart += m_spec.tile_width * xstride;
            }
        }
    }
    return ok;
}



bool
ImageOutput::write_rectangle (int xbegin, int xend, int ybegin, int yend,
                              int zbegin, int zend, TypeDesc format,
                              const void *data, stride_t xstride,
                              stride_t ystride, stride_t zstride)
{
    return false;
}



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
ImageOutput::error (const char *format, ...)
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



const void *
ImageOutput::to_native_scanline (TypeDesc format,
                                 const void *data, stride_t xstride,
                                 std::vector<unsigned char> &scratch)
{
    return to_native_rectangle (0, m_spec.width, 0, 1, 0, 1, format, data,
                                xstride, 0, 0, scratch);
}



const void *
ImageOutput::to_native_tile (TypeDesc format, const void *data,
                             stride_t xstride, stride_t ystride, stride_t zstride,
                             std::vector<unsigned char> &scratch)
{
    return to_native_rectangle (0, m_spec.tile_width, 0, m_spec.tile_height,
                                0, std::max(1,m_spec.tile_depth), format, data,
                                xstride, ystride, zstride, scratch);
}



const void *
ImageOutput::to_native_rectangle (int xbegin, int xend, int ybegin, int yend,
                                  int zbegin, int zend,
                                  TypeDesc format, const void *data,
                                  stride_t xstride, stride_t ystride, stride_t zstride,
                                  std::vector<unsigned char> &scratch)
{
    // native_pixel_bytes is the size of a pixel in the FILE, including
    // the per-channel format, if specified when the file was opened.
    stride_t native_pixel_bytes = (stride_t) m_spec.pixel_bytes (true);
    // perchanfile is true if the file has different per-channel formats
    bool perchanfile = m_spec.channelformats.size() && supports("channelformats");
    // It's an error to pass per-channel data formats to a writer that
    // doesn't support it.
    if (m_spec.channelformats.size() && !perchanfile)
        return NULL;
    // native_data is true if the user is passing data in the native format
    bool native_data = (format == TypeDesc::UNKNOWN ||
                        (format == m_spec.format && !perchanfile));
    // If the user is passing native data and they've left xstride set
    // to Auto, then we know it's the native pixel size.
    if (native_data && xstride == AutoStride)
        xstride = native_pixel_bytes;
    // Fill in the rest of the strides that haven't been set.
    m_spec.auto_stride (xstride, ystride, zstride, format,
                        m_spec.nchannels, xend-xbegin, yend-ybegin);

    // Compute width and height from the rectangle extents
    int width = xend - xbegin;
    int height = yend - ybegin;
    int depth = zend - zbegin;

    // Do the strides indicate that the data area is contiguous?
    bool contiguous;
    if (native_data) {
        // If it's native data, it had better be contiguous by the
        // file's definition.
        contiguous = (xstride == (stride_t)(m_spec.pixel_bytes(native_data)));
    } else {
        // If it's not native data, we only care if the user's buffer
        // is contiguous.
        contiguous = (xstride == (stride_t)(format.size()*m_spec.nchannels));
    }
    contiguous &= ((ystride == xstride*width || height == 1) &&
                   (zstride == ystride*height || depth == 1));

    if (native_data && contiguous) {
        // Data are already in the native format and contiguous
        // just return a ptr to the original data.
        return data;
    }

    imagesize_t rectangle_pixels = width * height * depth;
    imagesize_t rectangle_values = rectangle_pixels * m_spec.nchannels;
    imagesize_t rectangle_bytes = rectangle_pixels * native_pixel_bytes;

    // Cases to handle:
    // 1. File has per-channel data, user passes native data -- this has
    //    already returned above, since the data didn't need munging.
    // 2. File has per-channel data, user passes some other data type
    // 3. File has uniform data, user passes some other data type
    // 4. File has uniform data, user passes the right data -- note that
    //    this case already returned if the user data was contiguous

    // Handle the per-channel format case (#2) where the user is passing
    // a non-native buffer.
    if (perchanfile) {
        if (native_data) {
            ASSERT (contiguous && "Per-channel native output requires contiguous strides");
        }
        ASSERT (format != TypeDesc::UNKNOWN);
        ASSERT (m_spec.channelformats.size() == (size_t)m_spec.nchannels);
        scratch.resize (rectangle_bytes);
        size_t offset = 0;
        for (int c = 0;  c < m_spec.nchannels;  ++c) {
            TypeDesc chanformat = m_spec.channelformats[c];
            convert_image (1 /* channels */, width, height, depth,
                           (char *)data + c*format.size(), format,
                           xstride, ystride, zstride, 
                           &scratch[offset], chanformat,
                           native_pixel_bytes, AutoStride, AutoStride,
                           c == m_spec.alpha_channel ? 0 : -1,
                           c == m_spec.z_channel ? 0 : -1);
            offset += chanformat.size ();
        }
        return &scratch[0];
    }

    // The remaining code is where all channels in the file have the
    // same data type, which may or may not be what the user passed in
    // (cases #3 and #4 above).
    imagesize_t contiguoussize = contiguous ? 0 : rectangle_values * native_pixel_bytes;
    contiguoussize = (contiguoussize+3) & (~3); // Round up to 4-byte boundary
    DASSERT ((contiguoussize & 3) == 0);
    imagesize_t floatsize = rectangle_values * sizeof(float);
    scratch.resize (contiguoussize + floatsize + rectangle_bytes);

    // Force contiguity if not already present
    if (! contiguous) {
        data = contiguize (data, m_spec.nchannels, xstride, ystride, zstride,
                           (void *)&scratch[0], width, height, depth, format);
    }

    // Rather than implement the entire cross-product of possible
    // conversions, use float as an intermediate format, which generally
    // will always preserve enough precision.
    const float *buf;
    if (format == TypeDesc::FLOAT) {
        // Already in float format -- leave it as-is.
        buf = (float *)data;
    } else {
        // Convert to from 'format' to float.
        buf = convert_to_float (data, (float *)&scratch[contiguoussize],
                                rectangle_values, format);
    }
    
    // Convert from float to native format.
    return convert_from_float (buf, &scratch[contiguoussize+floatsize], 
                       rectangle_values, m_spec.quant_black, m_spec.quant_white,
                       m_spec.quant_min, m_spec.quant_max,
                       m_spec.format);
}



bool
ImageOutput::write_image (TypeDesc format, const void *data,
                          stride_t xstride, stride_t ystride, stride_t zstride,
                          ProgressCallback progress_callback,
                          void *progress_callback_data)
{
    bool native = (format == TypeDesc::UNKNOWN);
    stride_t pixel_bytes = native ? (stride_t) m_spec.pixel_bytes (native)
                                  : format.size() * m_spec.nchannels;
    if (xstride == AutoStride)
        xstride = pixel_bytes;
    m_spec.auto_stride (xstride, ystride, zstride, format,
                        m_spec.nchannels, m_spec.width, m_spec.height);

    if (supports ("rectangles")) {
        // Use a rectangle if we can
        return write_rectangle (0, m_spec.width, 0, m_spec.height, 0, m_spec.depth,
                                format, data, xstride, ystride, zstride);
    }

    bool ok = true;
    if (progress_callback && progress_callback (progress_callback_data, 0.0f))
        return ok;
    if (m_spec.tile_width && supports ("tiles")) {
        // Tiled image
        for (int z = 0;  z < m_spec.depth;  z += m_spec.tile_depth) {
            int zend = std::min (z+m_spec.z+m_spec.tile_depth,
                                 m_spec.z+m_spec.depth);
            for (int y = 0;  y < m_spec.height;  y += m_spec.tile_height) {
                int yend = std::min (y+m_spec.y+m_spec.tile_height,
                                     m_spec.y+m_spec.height);
                const char *d = (const char *)data + z*zstride + y*ystride;
                ok &= write_tiles (m_spec.x, m_spec.x+m_spec.width,
                                   y+m_spec.y, yend, z+m_spec.z, zend,
                                   format, d, xstride, ystride, zstride);
                if (progress_callback &&
                    progress_callback (progress_callback_data,
                                       (float)(z*m_spec.height+y)/(m_spec.height*m_spec.depth)))
                    return ok;
            }
        }
    } else {
        // Scanline image
        const int chunk = 256;
        for (int z = 0;  z < m_spec.depth;  ++z)
            for (int y = 0;  y < m_spec.height && ok;  y += chunk) {
                int yend = std::min (y+m_spec.y+chunk, m_spec.y+m_spec.height);
                const char *d = (const char *)data + z*zstride + y*ystride;
                ok &= write_scanlines (y+m_spec.y, yend, z+m_spec.z,
                                       format, d, xstride, ystride);
                if (progress_callback &&
                    progress_callback (progress_callback_data,
                                       (float)(z*m_spec.height+y)/(m_spec.height*m_spec.depth)))
                    return ok;
            }
    }
    if (progress_callback)
        progress_callback (progress_callback_data, 1.0f);

    return ok;
}



bool
ImageOutput::copy_image (ImageInput *in)
{
    if (! in) {
        error ("copy_image: no input supplied");
        return false;
    }

    // Make sure the images are compatible in size
    const ImageSpec &inspec (in->spec());
    if (inspec.width != spec().width || inspec.height != spec().height || 
        inspec.depth != spec().depth || inspec.nchannels != spec().nchannels) {
        error ("Could not copy %d x %d x %d channels to %d x %d x %d channels",
               inspec.width, inspec.height, inspec.nchannels,
               spec().width, spec().height, spec().nchannels);
        return false;
    }

    // in most cases plugins don't allow to copy 0x0 images
    // but there are some exceptions (like in FITS plugin)
    // when we want to do this. Because 0x0 means there is no image
    // data in the file, we simply return true so the application thought
    // that everything went right
    if (! spec().image_bytes())
        return true;

    // Naive implementation -- read the whole image and write it back out.
    // FIXME -- a smarter implementation would read scanlines or tiles at
    // a time, to minimize mem footprint.
    bool native = supports("channelformats") && inspec.channelformats.size();
    TypeDesc format = native ? TypeDesc::UNKNOWN : inspec.format;
    std::vector<char> pixels (inspec.image_bytes(native));
    bool ok = in->read_image (format, &pixels[0]);
    if (!ok)
        error ("%s", in->geterror().c_str());  // copy err from in to out
    if (ok)
        ok = write_image (format, &pixels[0]);
    return ok;
}

}
OIIO_NAMESPACE_EXIT
