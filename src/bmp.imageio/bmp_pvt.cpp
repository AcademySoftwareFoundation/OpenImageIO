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
#include "bmp_pvt.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

namespace bmp_pvt {



bool
BmpFileHeader::read_header (FILE *fd)
{
    int byte_count = 0;
    byte_count += fread (&magic, 1, sizeof (magic), fd);
    byte_count += fread (&fsize, 1, sizeof (fsize), fd);
    byte_count += fread (&res1, 1, sizeof (res1), fd);
    byte_count += fread (&res2, 1, sizeof (res2), fd);
    byte_count += fread (&offset, 1, sizeof (offset), fd);

    if (bigendian ())
        swap_endian ();
    return (byte_count == BMP_HEADER_SIZE);
}



bool
BmpFileHeader::write_header (FILE *fd)
{
    if (bigendian ())
        swap_endian ();

    int byte_count = 0;
    byte_count += fwrite (&magic, 1, sizeof (magic), fd);
    byte_count += fwrite (&fsize, 1, sizeof (fsize), fd);
    byte_count += fwrite (&res1, 1, sizeof (res1), fd);
    byte_count += fwrite (&res2, 1, sizeof (res2), fd);
    byte_count += fwrite (&offset, 1, sizeof (offset), fd);
    return (byte_count == BMP_HEADER_SIZE);
}



bool
BmpFileHeader::isBmp () const
{
    switch (magic) {
        case MAGIC_BM:
        case MAGIC_BA:
        case MAGIC_CI:
        case MAGIC_CP:
        case MAGIC_PT:
            return true;
    }
    return false;
}



void
BmpFileHeader::swap_endian (void)
{
    OIIO::swap_endian (&magic);
    OIIO::swap_endian (&fsize);
    OIIO::swap_endian (&offset);
}



bool
DibInformationHeader::read_header (FILE *fd)
{
    int byte_count = 0;
    byte_count += fread (&size, 1, sizeof (size), fd);

    if (size == WINDOWS_V3 || size == WINDOWS_V4) {
        byte_count += fread (&width, 1, sizeof (width), fd);
        byte_count += fread (&height, 1, sizeof (height), fd);
        byte_count += fread (&cplanes, 1, sizeof (cplanes), fd);
        byte_count += fread (&bpp, 1, sizeof (bpp), fd);
        byte_count += fread (&compression, 1, sizeof (compression), fd);
        byte_count += fread (&isize, 1, sizeof (isize), fd);
        byte_count += fread (&hres, 1, sizeof (hres), fd);
        byte_count += fread (&vres, 1, sizeof (vres), fd);
        byte_count += fread (&cpalete, 1, sizeof (cpalete), fd);
        byte_count += fread (&important, 1, sizeof (important), fd);
        if (size == WINDOWS_V4) {
            byte_count += fread (&red_mask, 1, sizeof (red_mask), fd);
            byte_count += fread (&blue_mask, 1, sizeof (blue_mask), fd);
            byte_count += fread (&green_mask, 1, sizeof (green_mask), fd);
            byte_count += fread (&cs_type, 1, sizeof (cs_type), fd);
            byte_count += fread (&red_x, 1, sizeof (red_x), fd);
            byte_count += fread (&red_y, 1, sizeof (red_y), fd);
            byte_count += fread (&red_z, 1, sizeof (red_z), fd);
            byte_count += fread (&green_x, 1, sizeof (green_x), fd);
            byte_count += fread (&green_y, 1, sizeof (green_y), fd);
            byte_count += fread (&green_z, 1, sizeof (green_z), fd);
            byte_count += fread (&blue_x, 1, sizeof (blue_x), fd);
            byte_count += fread (&blue_y, 1, sizeof (blue_y), fd);
            byte_count += fread (&blue_z, 1, sizeof (blue_z), fd);
            byte_count += fread (&gamma_x, 1, sizeof (gamma_x), fd);
            byte_count += fread (&gamma_y, 1, sizeof (gamma_y), fd);
            byte_count += fread (&gamma_z, 1, sizeof (gamma_z), fd);
            int32_t dummy;
            byte_count += fread (&dummy, 1, sizeof (dummy), fd);
        }
    }
    else if (size == OS2_V1) {
        // this fileds are smaller then in WINDOWS_Vx headers,
        // so we use hardcoded counts
        byte_count += fread (&width, 1, 2, fd);
        byte_count += fread (&height, 1, 2, fd);
        byte_count += fread (&cplanes, 1, 2, fd);
        byte_count += fread (&bpp, 1, 2, fd);
        
    }
    if (bigendian ())
        swap_endian ();
    return (byte_count == size);
}



bool
DibInformationHeader::write_header (FILE *fd)
{
    if (bigendian ())
        swap_endian ();
    fwrite (&size, 1, sizeof(size), fd);
    fwrite (&width, 1, sizeof(width), fd);
    fwrite (&height, 1, sizeof(height), fd);
    fwrite (&cplanes, 1, sizeof(cplanes), fd);
    fwrite (&bpp, 1, sizeof(bpp), fd);
    fwrite (&compression, 1, sizeof(compression), fd);
    fwrite (&isize, 1, sizeof(isize), fd);
    fwrite (&hres, 1, sizeof(hres), fd);
    fwrite (&vres, 1, sizeof(vres), fd);
    fwrite (&cpalete, 1, sizeof(cpalete), fd);
    fwrite (&important, 1, sizeof(important), fd);
    return true;
}



void
DibInformationHeader::swap_endian ()
{
    OIIO::swap_endian (&size);
    OIIO::swap_endian (&width);
    OIIO::swap_endian (&height);
    OIIO::swap_endian (&cplanes);
    OIIO::swap_endian (&bpp);
    OIIO::swap_endian (&compression);
    OIIO::swap_endian (&isize);
    OIIO::swap_endian (&hres);
    OIIO::swap_endian (&vres);
    OIIO::swap_endian (&cpalete);
    OIIO::swap_endian (&important);
}



} // bmp_pvt namespace


OIIO_PLUGIN_NAMESPACE_END

