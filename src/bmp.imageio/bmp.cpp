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

#include "bmp.h"


namespace bmp_pvt {


DibHeader*
DibHeader::return_dib_header (FILE *source) 
{
    int type = 0;
    fpos_t current_pos;
    fgetpos (source, &current_pos);
    fread (&type, 4, 1, source);
    fsetpos (source, &current_pos);
    // we identify what dib header is useb by checking its size
    const int V3_WINDOWS = 40;
    const int V1_OS2 = 12;
    switch (type) {
        case V3_WINDOWS : { return new V3Windows(source); break; }
        case V1_OS2     : { return new V1Os2(source); break; }
        default         : return NULL;
    }
}



bool
V3Windows::read_header (void) 
{
    size_t bytes = 0;
    bytes += fread(&size, 1, 4, m_source);
    bytes += fread(&width, 1, 4, m_source);
    bytes += fread(&height, 1, 4, m_source);
    bytes += fread(&planes, 1, 2, m_source);
    bytes += fread(&bpp, 1, 2, m_source);
    bytes += fread(&compression, 1, 4, m_source);
    bytes += fread(&raw_size, 1, 4, m_source);
    bytes += fread(&hres, 1, 4, m_source);
    bytes += fread(&vres, 1, 4, m_source);
    bytes += fread(&colors, 1, 4, m_source);
    bytes += fread(&important, 1, 4, m_source);

    return (bytes == 40);
}



bool
V1Os2::read_header (void) 
{
    size_t bytes = 0;
    bytes += fread(&size, 1, 4, m_source);
    bytes += fread(&width, 1, 2, m_source);
    bytes += fread(&height, 1, 2, m_source);
    bytes += fread(&planes, 1, 2, m_source);
    bytes += fread(&bpp, 1, 2, m_source);

    return (bytes == 12);
}

} /* end namespace bmp_pvt */

