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

#include "softimage_pvt.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

// Obligatory material to make this a recognizeable imageio plugin
OIIO_PLUGIN_EXPORTS_BEGIN

DLLEXPORT int softimage_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_PLUGIN_EXPORTS_END



namespace softimage_pvt {


void
PicFileHeader::swap_endian()
{
    OIIO::swap_endian (&magic);
    OIIO::swap_endian (&width);
    OIIO::swap_endian (&height);
    OIIO::swap_endian (&version);
    OIIO::swap_endian (&ratio);
    OIIO::swap_endian (&fields);
}



bool
PicFileHeader::read_header (FILE* fd)
{
    int byte_count = 0;
    byte_count += fread (this, 1, sizeof (PicFileHeader), fd);
    
    // Check if we're running on a little endian processor
    if (littleendian ())
        swap_endian();
        
    return (byte_count == sizeof (PicFileHeader));
}



std::vector<int>
ChannelPacket::channels () const
{
    std::vector<int> chanMap;

    // Check for the channels and add them to the chanMap
    if (channelCode & RED_CHANNEL)
        chanMap.push_back (0);
    if (channelCode & GREEN_CHANNEL)
        chanMap.push_back (1);
    if (channelCode & BLUE_CHANNEL)
        chanMap.push_back (2);
    if (channelCode & ALPHA_CHANNEL)
        chanMap.push_back (3);
    
    return chanMap;
}


}; // namespace softimage_pvt

OIIO_PLUGIN_NAMESPACE_END

