/*
  Copyright 2008-2009 Larry Gritz and the other authors and contributors.
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
#include "psd_pvt.h"
#include <fstream>

OIIO_PLUGIN_NAMESPACE_BEGIN

namespace OIIO = OIIO_NAMESPACE;

namespace psd_pvt {



int
read_pascal_string (std::ifstream &inf, std::string &s, uint16_t mod_padding)
{
    s.clear();
    uint8_t length;
    int bytes = 0;
    if (inf.read ((char *)&length, 1)) {
        bytes = 1;
        if (length == 0) {
            if (inf.seekg (mod_padding - 1, std::ios::cur))
                bytes += mod_padding - 1;
        } else {
            s.resize (length);
            if (inf.read (&s[0], length)) {
                bytes += length;
                if (mod_padding > 0) {
                    for (int padded_length = length + 1; padded_length % mod_padding != 0; padded_length++) {
                        if (!inf.seekg(1, std::ios::cur))
                            break;

                        bytes++;
                    }
                }
            }
        }
    }
    return bytes;
}



const char *
PSDFileHeader::read (std::ifstream &inf)
{
    inf.read (signature, 4);
    inf.read ((char *)&version, 2);
    // skip reserved bytes
    inf.seekg (6, std::ios::cur);
    inf.read ((char *)&channels, 2);
    inf.read ((char *)&height, 4);
    inf.read ((char *)&width, 4);
    inf.read ((char *)&depth, 2);
    inf.read ((char *)&color_mode, 2);
    swap_endian ();

    if (!inf) return "read error";

    if (std::memcmp (signature, "8BPS", 4) != 0)
        return "invalid signature";

    if (version != 1 && version != 2)
        return "invalid version";

    if (channels < 1 || channels > 56)
        return "invalid channel count";

    if (height < 1 || ((version == 1 && height > 30000) || (version == 2 && height > 300000)))
        return "invalid image height";

    if (width < 1 || ((version == 1 && width > 30000) || (version == 2 && width > 300000)))
        return "invalid image width";

    if (depth != 1 && depth != 8 && depth != 16 && depth != 32)
        return "invalid depth";

    switch (color_mode) {
        case COLOR_MODE_BITMAP :
        case COLOR_MODE_GRAYSCALE :
        case COLOR_MODE_INDEXED :
        case COLOR_MODE_RGB :
        case COLOR_MODE_CMYK :
        case COLOR_MODE_MULTICHANNEL :
        case COLOR_MODE_DUOTONE :
        case COLOR_MODE_LAB :
            break;
        default:
            return "invalid color mode";
    }
    return NULL;
}



void
PSDFileHeader::swap_endian ()
{
    if (!OIIO::bigendian ()) {
        OIIO::swap_endian (&version);
        OIIO::swap_endian (&channels);
        OIIO::swap_endian (&height);
        OIIO::swap_endian (&width);
        OIIO::swap_endian (&depth);
        OIIO::swap_endian (&color_mode);
    }
}



const char *
PSDColorModeData::read (std::ifstream &inf, const PSDFileHeader &header)
{
    inf.read ((char *)&length, 4);
    //swap endian before interpreting length
    swap_endian ();
    data.resize (length);
    inf.read (&data[0], length);

    if (!inf)
        return "read error";

    if (header.color_mode == COLOR_MODE_INDEXED && length != 768)
        return "length should be 768 for indexed color mode";

    return NULL;
}



void
PSDColorModeData::swap_endian ()
{
    if (!OIIO::bigendian ())
        OIIO::swap_endian (&length);
}



} // psd_pvt namespace


OIIO_PLUGIN_NAMESPACE_END

