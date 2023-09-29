// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>

#include "bmp_pvt.h"


OIIO_PLUGIN_NAMESPACE_BEGIN

namespace bmp_pvt {

/// Helper - write, with error detection
template<class T>
bool
fwrite(Filesystem::IOProxy* fd, const T* buf)
{
    return fd->write(buf, sizeof(T)) == sizeof(T);
}

/// Helper - read, with error detection
template<class T>
bool
fread(Filesystem::IOProxy* fd, T* buf, size_t itemsize = sizeof(T))
{
    return fd->read(buf, itemsize) == itemsize;
}

bool
BmpFileHeader::read_header(Filesystem::IOProxy* fd)
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
BmpFileHeader::write_header(Filesystem::IOProxy* fd)
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
    default: return false;
    }
}



void
BmpFileHeader::swap_endian(void)
{
    OIIO::swap_endian(&magic);
    OIIO::swap_endian(&fsize);
    OIIO::swap_endian(&offset);
}



bool
DibInformationHeader::read_header(Filesystem::IOProxy* fd)
{
    if (!fread(fd, &size))
        return false;

    if (size == WINDOWS_V3 || size == WINDOWS_V4 || size == WINDOWS_V5
        || size == UNDOCHEADER52 || size == UNDOCHEADER56) {
        if (!fread(fd, &width) || !fread(fd, &height) || !fread(fd, &cplanes)
            || !fread(fd, &bpp) || !fread(fd, &compression)
            || !fread(fd, &isize) || !fread(fd, &hres) || !fread(fd, &vres)
            || !fread(fd, &cpalete) || !fread(fd, &important)) {
            return false;
        }

        if ((size == WINDOWS_V3 && bpp == 16 && compression == 3)
            || size == WINDOWS_V4 || size == WINDOWS_V5 || size == UNDOCHEADER52
            || size == UNDOCHEADER56) {
            if (!fread(fd, &red_mask) || !fread(fd, &green_mask)
                || !fread(fd, &blue_mask)) {
                return false;
            }
            if (size != UNDOCHEADER52 && !fread(fd, &alpha_mask)) {
                return false;
            }
        }

        if (size == WINDOWS_V4 || size == WINDOWS_V5) {
            if (!fread(fd, &cs_type) || !fread(fd, &red_x) || !fread(fd, &red_y)
                || !fread(fd, &red_z) || !fread(fd, &green_x)
                || !fread(fd, &green_y) || !fread(fd, &green_z)
                || !fread(fd, &blue_x) || !fread(fd, &blue_y)
                || !fread(fd, &blue_z) || !fread(fd, &gamma_x)
                || !fread(fd, &gamma_y) || !fread(fd, &gamma_z)) {
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
        // some of these fields are smaller than in WINDOWS_Vx headers,
        // so we read into 16 bit ints and copy.
        uint16_t width16  = 0;
        uint16_t height16 = 0;
        if (!fread(fd, &width16) || !fread(fd, &height16)
            || !fread(fd, &cplanes) || !fread(fd, &bpp)) {
            return false;
        }
        width  = width16;
        height = height16;
    }
    if (bigendian())
        swap_endian();
    return true;
}



bool
DibInformationHeader::write_header(Filesystem::IOProxy* fd)
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

    return true;
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
