// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md


#include "bmp_pvt.h"

#include <OpenImageIO/fmath.h>

OIIO_PLUGIN_NAMESPACE_BEGIN

namespace bmp_pvt {

/// Helper - write, with error detection
template<class T>
bool
fwrite(FILE* fd, const T* buf)
{
    size_t n = std::fwrite(buf, sizeof(T), 1, fd);
    return n == 1;
}

/// Helper - read, with error detection
template<class T>
bool
fread(FILE* fd, T* buf, size_t itemsize = sizeof(T))
{
    size_t n = std::fread(buf, itemsize, 1, fd);
    return n == 1;
}

bool
BmpFileHeader::read_header(FILE* fd)
{
    if (!fread(fd, &magic) || !fread(fd, &fsize) || !fread(fd, &res1)
        || !fread(fd, &res2) || !fread(fd, &offset)) {
        return false;
    }

    if (bigendian())
        swap_endian();
    return true;
}



bool
BmpFileHeader::write_header(FILE* fd)
{
    if (bigendian())
        swap_endian();

    if (!fwrite(fd, &magic) || !fwrite(fd, &fsize) || !fwrite(fd, &res1)
        || !fwrite(fd, &res2) || !fwrite(fd, &offset)) {
        return false;
    }

    return true;
}



bool
BmpFileHeader::isBmp() const
{
    switch (magic) {
    case MAGIC_BM:
    case MAGIC_BA:
    case MAGIC_CI:
    case MAGIC_CP:
    case MAGIC_PT: return true;
    }
    return false;
}



void
BmpFileHeader::swap_endian(void)
{
    OIIO::swap_endian(&magic);
    OIIO::swap_endian(&fsize);
    OIIO::swap_endian(&offset);
}



bool
DibInformationHeader::read_header(FILE* fd)
{
    if (!fread(fd, &size))
        return false;

    if (size == WINDOWS_V3 || size == WINDOWS_V4 || size == WINDOWS_V5) {
        if (!fread(fd, &width) || !fread(fd, &height) || !fread(fd, &cplanes)
            || !fread(fd, &bpp) || !fread(fd, &compression)
            || !fread(fd, &isize) || !fread(fd, &hres) || !fread(fd, &vres)
            || !fread(fd, &cpalete) || !fread(fd, &important)) {
            return false;
        }

        if (size == WINDOWS_V4 || size == WINDOWS_V5) {
            if (!fread(fd, &red_mask) || !fread(fd, &blue_mask)
                || !fread(fd, &green_mask) || !fread(fd, &alpha_mask)
                || !fread(fd, &cs_type) || !fread(fd, &red_x)
                || !fread(fd, &red_y) || !fread(fd, &red_z)
                || !fread(fd, &green_x) || !fread(fd, &green_y)
                || !fread(fd, &green_z) || !fread(fd, &blue_x)
                || !fread(fd, &blue_y) || !fread(fd, &blue_z)
                || !fread(fd, &gamma_x) || !fread(fd, &gamma_y)
                || !fread(fd, &gamma_z)) {
                return false;
            }
        }

        if (size == WINDOWS_V5) {
            if (!fread(fd, &intent) || !fread(fd, &profile_data)
                || !fread(fd, &profile_size) || !fread(fd, &reserved)) {
                return false;
            }
        }
    } else if (size == OS2_V1) {
        // some of theses fields are smaller then in WINDOWS_Vx headers,
        // so we use hardcoded counts
        width  = 0;
        height = 0;
        if (!fread(fd, &width, 2) || !fread(fd, &height, 2)
            || !fread(fd, &cplanes) || !fread(fd, &bpp)) {
            return false;
        }
    }
    if (bigendian())
        swap_endian();
    return true;
}



bool
DibInformationHeader::write_header(FILE* fd)
{
    if (bigendian())
        swap_endian();

    if (!fwrite(fd, &size) || !fwrite(fd, &width) || !fwrite(fd, &height)
        || !fwrite(fd, &cplanes) || !fwrite(fd, &bpp)
        || !fwrite(fd, &compression) || !fwrite(fd, &isize)
        || !fwrite(fd, &hres) || !fwrite(fd, &vres) || !fwrite(fd, &cpalete)
        || !fwrite(fd, &important)) {
        return false;
    }

    return (true);
}



void
DibInformationHeader::swap_endian()
{
    OIIO::swap_endian(&size);
    OIIO::swap_endian(&width);
    OIIO::swap_endian(&height);
    OIIO::swap_endian(&cplanes);
    OIIO::swap_endian(&bpp);
    OIIO::swap_endian(&compression);
    OIIO::swap_endian(&isize);
    OIIO::swap_endian(&hres);
    OIIO::swap_endian(&vres);
    OIIO::swap_endian(&cpalete);
    OIIO::swap_endian(&important);
}



}  // namespace bmp_pvt


OIIO_PLUGIN_NAMESPACE_END
