/*
  Copyright 2011 Larry Gritz and the other authors and contributors.
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

#ifndef OPENIMAGEIO_PSD_PVT_H
#define OPENIMAGEIO_PSD_PVT_H

#include <iosfwd>
#include <map>
#include "imageio.h"
#include "fmath.h"
#include <boost/function.hpp>

OIIO_PLUGIN_NAMESPACE_BEGIN

namespace psd_pvt {

    enum ColorMode {
        ColorMode_Bitmap = 0,
        ColorMode_Grayscale = 1,
        ColorMode_Indexed = 2,
        ColorMode_RGB = 3,
        ColorMode_CMYK = 4,
        ColorMode_Multichannel = 7,
        ColorMode_Duotone = 8,
        ColorMode_Lab = 9
    };



    enum ThumbnailFormat {
        kJpegRGB = 1,
        kRawRGB = 0
    };



    struct FileHeader {
        char signature[4];
        uint16_t version;
        uint16_t channel_count;
        uint32_t height;
        uint32_t width;
        uint16_t depth;
        uint16_t color_mode;
    };



    struct ColorModeData {
        uint32_t length;
        std::streampos pos;
    };



    struct ImageResourceBlock {
        char signature[4];
        uint16_t id;
        std::string name;
        uint32_t length;
        std::streampos pos;
    };



    struct ResolutionInfo {
        float hRes;
        int16_t hResUnit;
        int16_t widthUnit;
        float vRes;
        int16_t vResUnit;
        int16_t heightUnit;
        
        enum ResolutionUnit {
            PixelsPerInch = 1,
            PixelsPerCentimeter = 2
        };

        enum Unit {
            Inches = 1,
            Centimeters = 2,
            Points = 3,
            Picas = 4,
            Columns = 5
        };

    };

};  // namespace PSD_pvt

OIIO_PLUGIN_NAMESPACE_END

#endif  // OPENIMAGEIO_PSD_PVT_H

