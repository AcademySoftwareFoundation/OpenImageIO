// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO
#include "noproxy-iff_pvt.h"
#include <OpenImageIO/dassert.h>

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace iff_pvt;



bool
IffFileHeader::read_header(FILE* fd, std::string& err)
{
    uint8_t type[4];
    uint32_t size;
    uint32_t chunksize;
    uint32_t tbhdsize;
    uint32_t flags;
    uint16_t bytes;
    uint16_t prnum;
    uint16_t prden;

    // read FOR4 <size> CIMG.
    err.clear();
    for (;;) {
        // get type and length
        if (!read_typesize(fd, type, size)) {
            err = "could not read type/size @ L" OIIO_STRINGIZE(__LINE__);
            return false;
        }

        chunksize = align_size(size, 4);

        if (type[0] == 'F' && type[1] == 'O' && type[2] == 'R'
            && type[3] == '4') {
            // get type
            if (!fread(&type, 1, sizeof(type), fd)) {
                err = "could not read FDR4 type @ L" OIIO_STRINGIZE(__LINE__);
                return false;
            }

            // check if CIMG
            if (type[0] == 'C' && type[1] == 'I' && type[2] == 'M'
                && type[3] == 'G') {
                // read TBHD.
                for (;;) {
                    if (!read_typesize(fd, type, size)) {
                        err = "could not read CIMG length @ L" OIIO_STRINGIZE(
                            __LINE__);
                        return false;
                    }

                    chunksize = align_size(size, 4);

                    if (type[0] == 'T' && type[1] == 'B' && type[2] == 'H'
                        && type[3] == 'D') {
                        tbhdsize = size;

                        // test if table header size is correct
                        if (tbhdsize != 24 && tbhdsize != 32) {
                            err = "bad table header @ L" OIIO_STRINGIZE(
                                __LINE__);
                            return false;  // bad table header
                        }

                        // get width and height
                        if (!read(fd, width) || !read(fd, height)
                            || !read(fd, prnum) || !read(fd, prden)
                            || !read(fd, flags) || !read(fd, bytes)
                            || !read(fd, tiles) || !read(fd, compression)) {
                            err = "@ L" OIIO_STRINGIZE(__LINE__);
                            return false;
                        }

                        // get xy
                        if (tbhdsize == 32) {
                            if (!read(fd, x) || !read(fd, y)) {
                                err = "could not get xy @ L" OIIO_STRINGIZE(
                                    __LINE__);
                                return false;
                            }
                        } else {
                            x = 0;
                            y = 0;
                        }

                        // tiles
                        if (tiles == 0) {
                            err = "non-tiles not supported @ L" OIIO_STRINGIZE(
                                __LINE__);
                            return false;
                        }  // non-tiles not supported

                        // 0 no compression
                        // 1 RLE compression
                        // 2 QRL (not supported)
                        // 3 QR4 (not supported)
                        if (compression > 1) {
                            err = "only RLE compression is supported @ L" OIIO_STRINGIZE(
                                __LINE__);
                            return false;
                        }

                        // test format.
                        if (flags & RGBA) {
                            // test if black is set
                            OIIO_DASSERT(!(flags & BLACK));

                            // test for RGB channels.
                            if (flags & RGB)
                                pixel_channels = 3;

                            // test for alpha channel
                            if (flags & ALPHA)
                                pixel_channels++;

                            // test pixel bits
                            pixel_bits = bytes ? 16 : 8;
                        }

                        // Z format.
                        else if (flags & ZBUFFER) {
                            pixel_channels = 1;
                            pixel_bits     = 32;  // 32bit
                            // NOTE: Z_F32 support - not supported
                            OIIO_DASSERT(bytes == 0);
                        }

                        // read AUTH, DATE or FOR4

                        for (;;) {
                            // get type
                            if (!read_typesize(fd, type, size)) {
                                err = "could not read type/size @ L" OIIO_STRINGIZE(
                                    __LINE__);
                                return false;
                            }

                            chunksize = align_size(size, 4);

                            if (type[0] == 'A' && type[1] == 'U'
                                && type[2] == 'T' && type[3] == 'H') {
                                std::vector<char> str(chunksize);
                                if (!fread(&str[0], 1, chunksize, fd)) {
                                    err = "could not read author @ L" OIIO_STRINGIZE(
                                        __LINE__);
                                    return false;
                                }
                                author = std::string(&str[0], size);
                            } else if (type[0] == 'D' && type[1] == 'A'
                                       && type[2] == 'T' && type[3] == 'E') {
                                std::vector<char> str(chunksize);
                                if (!fread(&str[0], 1, chunksize, fd)) {
                                    err = "could not read date @ L" OIIO_STRINGIZE(
                                        __LINE__);
                                    return false;
                                }
                                date = std::string(&str[0], size);
                            } else if (type[0] == 'F' && type[1] == 'O'
                                       && type[2] == 'R' && type[3] == '4') {
                                if (!fread(&type, 1, sizeof(type), fd)) {
                                    err = "could not read FOR4 type @ L" OIIO_STRINGIZE(
                                        __LINE__);
                                    return false;
                                }

                                // check if CIMG
                                if (type[0] == 'T' && type[1] == 'B'
                                    && type[2] == 'M' && type[3] == 'P') {
                                    // tbmp position for later user in in
                                    // read_native_tile

                                    tbmp_start = ftell(fd);

                                    // read first RGBA block to detect tile size.

                                    for (unsigned int t = 0; t < tiles; t++) {
                                        if (!read_typesize(fd, type, size)) {
                                            err = "xxx @ L" OIIO_STRINGIZE(
                                                __LINE__);
                                            return false;
                                        }
                                        chunksize = align_size(size, 4);

                                        // check if RGBA
                                        if (type[0] == 'R' && type[1] == 'G'
                                            && type[2] == 'B'
                                            && type[3] == 'A') {
                                            // get tile coordinates.
                                            uint16_t xmin, xmax, ymin, ymax;
                                            if (!read(fd, xmin)
                                                || !read(fd, ymin)
                                                || !read(fd, xmax)
                                                || !read(fd, ymax)) {
                                                err = "xxx @ L" OIIO_STRINGIZE(
                                                    __LINE__);
                                                return false;
                                            }

                                            // check tile
                                            if (xmin > xmax || ymin > ymax
                                                || xmax >= width
                                                || ymax >= height) {
                                                err = "tile min/max nonsensical @ L" OIIO_STRINGIZE(
                                                    __LINE__);
                                                return false;
                                            }

                                            // set tile width and height
                                            tile_width  = xmax - xmin + 1;
                                            tile_height = ymax - ymin + 1;

                                            // done, return
                                            return true;
                                        }

                                        // skip to the next block.
                                        if (fseek(fd, chunksize, SEEK_CUR)) {
                                            err = "could not fseek @ L" OIIO_STRINGIZE(
                                                __LINE__);
                                            return false;
                                        }
                                    }
                                } else {
                                    // skip to the next block.
                                    if (fseek(fd, chunksize, SEEK_CUR)) {
                                        err = "could not fseek @ L" OIIO_STRINGIZE(
                                            __LINE__);
                                        return false;
                                    }
                                }
                            } else {
                                // skip to the next block.
                                if (fseek(fd, chunksize, SEEK_CUR)) {
                                    err = "could not fseek @ L" OIIO_STRINGIZE(
                                        __LINE__);
                                    return false;
                                }
                            }
                        }
                        // TBHD done, break
                        break;
                    }

                    // skip to the next block.
                    if (fseek(fd, chunksize, SEEK_CUR)) {
                        err = "could not fseek @ L" OIIO_STRINGIZE(__LINE__);
                        return false;
                    }
                }
            }
        }
        // skip to the next block.
        if (fseek(fd, chunksize, SEEK_CUR)) {
            err = "could not fseek @ L" OIIO_STRINGIZE(__LINE__);
            return false;
        }
    }
    err = "unknown error, ended early @ L" OIIO_STRINGIZE(__LINE__);
    return false;
}



bool
IffOutput::write_header(IffFileHeader& header)
{
    // write 'FOR4' type, with 0 length for now (to reserve it)
    if (!(write_str("FOR4") && write_int(0)))
        return false;

    // write 'CIMG' type
    if (!write_str("CIMG"))
        return false;

    // write 'TBHD' type
    if (!write_str("TBHD"))
        return false;

    // 'TBHD' length, 32 bytes
    if (!write_int(32))
        return false;

    if (!write_int(header.width) || !write_int(header.height))
        return false;

    // write prnum and prden (pixel aspect ratio? -- FIXME)
    if (!write_short(1) || !write_short(1))  //NOSONAR
        return false;

    // write flags and channels
    if (!write_int(header.pixel_channels == 3 ? RGB : RGBA)
        || !write_short(header.pixel_bits == 8 ? 0 : 1)
        || !write_short(header.tiles))
        return false;

    // write compression
    // 0 no compression
    // 1 RLE compression
    // 2 QRL (not supported)
    // 3 QR4 (not supported)
    if (!write_int(header.compression))
        return false;

    // write x and y
    if (!write_int(header.x) || !write_int(header.y))
        return false;

    // Write metadata
    write_meta_string("AUTH", header.author);
    write_meta_string("DATE", header.date);

    // for4 position for later user in close
    header.for4_start = ftell(m_fd);

    // write 'FOR4' type, with 0 length to reserve it for now
    if (!write_str("FOR4") || !write_int(0))
        return false;

    // write 'TBMP' type
    if (!write_str("TBMP"))
        return false;

    return true;
}



OIIO_PLUGIN_NAMESPACE_END
