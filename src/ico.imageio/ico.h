/*
  Copyright 2009 Larry Gritz and the other authors and contributors.
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

#ifndef OPENIMAGEIO_ICO_H
#define OPENIMAGEIO_ICO_H

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>


OIIO_PLUGIN_NAMESPACE_BEGIN

namespace ICO_pvt {

// IneQuation was here

// Win32 (pre-Vista) ICO format as described in these documents:
// http://msdn.microsoft.com/en-us/library/ms997538.aspx
// http://en.wikipedia.org/wiki/ICO_(icon_image_file_format)
// http://msdn.microsoft.com/en-us/library/dd183376(VS.85).aspx
// ...plus some of my own magic.

/// Win32 DIB (Device-Independent Bitmap) header.
/// According to MSDN, only size, width, height, planes, bpp and len are
/// valid for ICOs.
struct ico_bitmapinfo {
    int32_t size;         ///< structure size in bytes (how about a sizeof?)
    int32_t width;
    int32_t height;
    int16_t planes;       ///< # of colour planes
    int16_t bpp;          ///< bits per pixel
    int32_t compression;  ///< unused: compression type
    int32_t len;          ///< image size in bytes; may be 0 for uncompressed bitmaps
    int32_t x_res;        ///< unused: resolution of target device in pixels per metre
    int32_t y_res;        ///< unused: resolution of target device in pixels per metre
    int32_t clrs_used;    ///< # of colours used (if using a palette)
    int32_t clrs_required; ///< # of colours required to display the bitmap; 0 = all of them
};

/// Icon palette entry. Attached at each
struct ico_palette_entry {
    int8_t b, g, r;
    int8_t reserved; // unused
};


struct ico_subimage {
    uint8_t width;        ///< 0 means 256 pixels
    uint8_t height;       ///< 0 means 256 pixels
    uint8_t numColours;   ///< 0 means >= 256
    uint8_t reserved;     ///< should always be 0
    uint16_t planes;      ///< # of colour planes
    uint16_t bpp;         ///< bits per pixel
    uint32_t len;         ///< size (in bytes) of bitmap data
    uint32_t ofs;         ///< offset to bitmap data
};


struct ico_header {
    int16_t reserved;     ///< should always be 0
    int16_t type;         ///< 1 is icon, 2 is cursor
    int16_t count;        ///< number of subimages in the file
};


}  // namespace ICO_pvt


OIIO_PLUGIN_NAMESPACE_END

#endif  // OPENIMAGEIO_ICO_H
