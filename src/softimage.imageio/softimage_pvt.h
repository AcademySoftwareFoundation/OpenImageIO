/*
OpenImageIO and all code, documentation, and other materials contained
therein are:

Copyright 2010 Larry Gritz and the other authors and contributors.
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

#ifndef OPENIMAGEIO_SOFTIMAGE_H
#define OPENIMAGEIO_SOFTIMAGE_H

#include <cstdio>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/filesystem.h>

OIIO_PLUGIN_NAMESPACE_BEGIN



namespace softimage_pvt
{
    class PicFileHeader
    {
    public:
        // Read pic header from file
        bool read_header (FILE *fd);
        
        // PIC header
        uint32_t magic; // Softimage magic number
        float version; // Storage format - 1 is RLE, 0 is RAW
        char comment[80]; // Comment
        char id[4]; // ID - should be PICT
        uint16_t width; // X size in pixels
        uint16_t height; // Y size in pixels
        float ratio; // Pixel aspect ratio
        uint16_t fields; // The scanline setting - No Pictures, Odd, Even or every
        uint16_t pad; // unused

    private:
        void swap_endian();
    }; // class PicFileHeader



    class ChannelPacket
    {
    public:
        //channel packet contains info on the image data
        ChannelPacket() { chained = 0; }
        // !brief  get a list of the channels contained in this channel packet
        std::vector<int> channels() const;
        uint8_t chained; //0 if this is the last channel packet
        uint8_t size; //Number of bits per pixel per channel
        uint8_t type; //Data encoding and type
        uint8_t channelCode; //bitset for channels
    }; // class ChannelPacket



    enum channelCodes
    {
        RED_CHANNEL  = 0x80,
        GREEN_CHANNEL = 0x40,
        BLUE_CHANNEL = 0x20,
        ALPHA_CHANNEL = 0x10
    }; // enum channelCodes



    enum encoding
    {
        UNCOMPRESSED,
        PURE_RUN_LENGTH,
        MIXED_RUN_LENGTH
    }; // enum encoding

} //namespace softimage_pvt

OIIO_PLUGIN_NAMESPACE_END

#endif // OPENIMAGEIO_PIC_H
