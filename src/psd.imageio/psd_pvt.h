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

#ifndef OPENIMAGEIO_PSD_PVT_H
#define OPENIMAGEIO_PSD_PVT_H

#include <iosfwd>
#include "imageio.h"
#include "fmath.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

namespace psd_pvt {

    enum PSDColorMode {
        COLOR_MODE_BITMAP = 0,
        COLOR_MODE_GRAYSCALE = 1,
        COLOR_MODE_INDEXED = 2,
        COLOR_MODE_RGB = 3,
        COLOR_MODE_CMYK = 4,
        COLOR_MODE_MULTICHANNEL = 7,
        COLOR_MODE_DUOTONE = 8,
        COLOR_MODE_LAB = 9
    };

    int read_pascal_string (std::ifstream &inf, std::string &s, uint16_t mod_padding);

    class PSDFileHeader {
     public:
        const char *read (std::ifstream &inf);

        char signature[4];
        uint16_t version;
        uint16_t channels;
        uint32_t height;
        uint32_t width;
        uint16_t depth;
        uint16_t color_mode;

     private:
        void swap_endian ();
    };

    class PSDColorModeData {
     public:
        const char *read (std::ifstream &inf, const PSDFileHeader &header);

        uint32_t length;
        std::string data;

     private:
        void swap_endian ();
    };

};  // namespace PSD_pvt

OIIO_PLUGIN_NAMESPACE_END

#endif  // OPENIMAGEIO_PSD_PVT_H

