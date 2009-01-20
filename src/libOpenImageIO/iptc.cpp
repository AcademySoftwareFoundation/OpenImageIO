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


#include <iostream>

#include "imageio.h"
using namespace OpenImageIO;

#define DEBUG_IPTC_READ  0
#define DEBUG_IPTC_WRITE 0


namespace OpenImageIO {


bool
decode_iptc_iim (const void *iptc, int length, ImageSpec &spec)
{
    const unsigned char *buf = (const unsigned char *) iptc;

#if DEBUG_IPTC_READ
    std::cerr << "IPTC dump:\n";
    for (int i = 0;  i < 100;  ++i) {
        if (buf[i] >= ' ')
            std::cerr << (char)buf[i] << ' ';
        std::cerr << "(" << (int)(unsigned char)buf[i] << ") ";
    }
    std::cerr << "\n";
#endif

    std::string keywords;

    // Now there are a series of data blocks.  Each one starts with 1C
    // 02, then a single byte indicating the tag type, then 2 byte (big
    // endian) giving the tag length, then the data itself.  This
    // repeats until we've used up the whole segment buffer, or I guess
    // until we don't find another 1C 02 tag start.
    while (length > 0 && buf[0] == 0x1c && (buf[1] == 0x02 || buf[1] == 0x01)) {
        int firstbyte = buf[0], secondbyte = buf[1];
        int tagtype = buf[2];
        int tagsize = (buf[3] << 8) + buf[4];
        buf += 5;
        length -= 5;

#if DEBUG_IPTC_READ
        std::cerr << "iptc tag " << tagtype << ":\n";
        for (int i = 0;  i < tagsize;  ++i) {
            if (buf[i] >= ' ')
                std::cerr << (char)buf[i] << ' ';
            std::cerr << "(" << (int)(unsigned char)buf[i] << ") ";
        }
        std::cerr << "\n";
#endif

        if (secondbyte != 0x02) {
            buf += tagsize;
            length -= tagsize;
            continue;
        }

        std::string s ((const char *)buf, tagsize);

        switch (tagtype) {
        case 25:
            if (keywords.length())
                keywords += std::string (", ");
            keywords += s;
            break;
        case 30:
            spec.attribute ("IPTC:Instructions", s);
            break;
        case 80:
            spec.attribute ("Artist", s);
            spec.attribute ("IPTC:Creator", s);
            break;
        case 85:
            spec.attribute ("IPTC:AuthorsPosition", s);
            break;
        case 90:
            spec.attribute ("IPTC:City", s);
            break;
        case 95:
            spec.attribute ("IPTC:State", s);
            break;
        case 101:
            spec.attribute ("IPTC:Country", s);
            break;
        case 105:
            spec.attribute ("IPTC:Headline", s);
            break;
        case 110:
            spec.attribute ("IPTC:Provider", s);
            break;
        case 115:
            spec.attribute ("IPTC:Source", s);
            break;
        case 116:
            spec.attribute ("Copyright", s);
            break;
        case 120:
            spec.attribute ("IPTC:Caption_Abstract", s);
            spec.attribute ("ImageDescription", s);
            break;
        case 122:
            spec.attribute ("IPTC:CaptionWriter", s);
            break;
        }

        buf += tagsize;
        length -= tagsize;
    }

    if (keywords.length())
        spec.attribute ("keywords", keywords);
}


};  // namespace Jpeg_imageio_pvt

