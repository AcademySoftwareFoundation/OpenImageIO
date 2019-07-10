// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md
#include "iff_pvt.h"
#include <OpenImageIO/dassert.h>

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace iff_pvt;



bool
IffFileHeader::read_header(FILE* fd)
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
    for (;;) {
        // get type
        if (!fread(&type, 1, sizeof(type), fd) ||
            // get length
            !fread(&size, 1, sizeof(size), fd))
            return false;

        if (littleendian())
            swap_endian(&size);

        chunksize = align_size(size, 4);

        if (type[0] == 'F' && type[1] == 'O' && type[2] == 'R'
            && type[3] == '4') {
            // get type
            if (!fread(&type, 1, sizeof(type), fd))
                return false;

            // check if CIMG
            if (type[0] == 'C' && type[1] == 'I' && type[2] == 'M'
                && type[3] == 'G') {
                // read TBHD.
                for (;;) {
                    // get type
                    if (!fread(&type, 1, sizeof(type), fd) ||
                        // get length
                        !fread(&size, 1, sizeof(size), fd))
                        return false;

                    if (littleendian())
                        swap_endian(&size);

                    chunksize = align_size(size, 4);

                    if (type[0] == 'T' && type[1] == 'B' && type[2] == 'H'
                        && type[3] == 'D') {
                        tbhdsize = size;

                        // test if table header size is correct
                        if (tbhdsize != 24 && tbhdsize != 32)
                            return false;  // bad table header size

                        // get width and height
                        if (!fread(&width, 1, sizeof(width), fd)
                            || !fread(&height, 1, sizeof(height), fd) ||

                            // get prnum and prdeb
                            !fread(&prnum, 1, sizeof(prnum), fd)
                            || !fread(&prden, 1, sizeof(prden), fd) ||

                            // get flags, bytes, tiles and compressed
                            !fread(&flags, 1, sizeof(flags), fd)
                            || !fread(&bytes, 1, sizeof(bytes), fd)
                            || !fread(&tiles, 1, sizeof(tiles), fd)
                            || !fread(&compression, 1, sizeof(compression), fd))
                            return false;

                        // get xy
                        if (tbhdsize == 32) {
                            if (!fread(&x, 1, sizeof(x), fd)
                                || !fread(&y, 1, sizeof(y), fd))
                                return false;
                        } else {
                            x = 0;
                            y = 0;
                        }

                        // swap endianness
                        if (littleendian()) {
                            swap_endian(&width);
                            swap_endian(&height);
                            swap_endian(&prnum);
                            swap_endian(&prden);
                            swap_endian(&flags);
                            swap_endian(&bytes);
                            swap_endian(&tiles);
                            swap_endian(&compression);
                        }

                        // tiles
                        if (tiles == 0)
                            return false;  // non-tiles not supported

                        // 0 no compression
                        // 1 RLE compression
                        // 2 QRL (not supported)
                        // 3 QR4 (not supported)
                        if (compression > 1)
                            return false;

                        // test format.
                        if (flags & RGBA) {
                            // test if black is set
                            DASSERT(!(flags & BLACK));

                            // test for RGB channels.
                            if (flags & RGB) {
                                pixel_channels = 3;
                            }

                            // test for alpha channel
                            if (flags & ALPHA) {
                                pixel_channels++;
                            }

                            // test pixel bits
                            if (!bytes) {
                                pixel_bits = 8;  // 8bit
                            } else {
                                pixel_bits = 16;  // 16bit
                            }
                        }

                        // Z format.
                        else if (flags & ZBUFFER) {
                            pixel_channels = 1;
                            pixel_bits     = 32;  // 32bit
                            // NOTE: Z_F32 support - not supported
                            DASSERT(bytes == 0);
                        }

                        // read AUTH, DATE or FOR4

                        for (;;) {
                            // get type
                            if (!fread(&type, 1, sizeof(type), fd) ||
                                // get length
                                !fread(&size, 1, sizeof(size), fd))
                                return false;

                            if (littleendian())
                                swap_endian(&size);

                            chunksize = align_size(size, 4);

                            if (type[0] == 'A' && type[1] == 'U'
                                && type[2] == 'T' && type[3] == 'H') {
                                std::vector<char> str(chunksize);
                                if (!fread(&str[0], 1, chunksize, fd))
                                    return false;

                                // get author
                                author = std::string(&str[0], size);
                            } else if (type[0] == 'D' && type[1] == 'A'
                                       && type[2] == 'T' && type[3] == 'E') {
                                std::vector<char> str(chunksize);
                                if (!fread(&str[0], 1, chunksize, fd))
                                    return false;

                                // get date
                                date = std::string(&str[0], size);
                            } else if (type[0] == 'F' && type[1] == 'O'
                                       && type[2] == 'R' && type[3] == '4') {
                                // get type
                                if (!fread(&type, 1, sizeof(type), fd))
                                    return false;

                                // check if CIMG
                                if (type[0] == 'T' && type[1] == 'B'
                                    && type[2] == 'M' && type[3] == 'P') {
                                    // tbmp position for later user in in
                                    // read_native_tile

                                    tbmp_start = ftell(fd);

                                    // read first RGBA block to detect tile size.

                                    for (unsigned int t = 0; t < tiles; t++) {
                                        // get type
                                        if (!fread(&type, 1, sizeof(type), fd)
                                            ||
                                            // get length
                                            !fread(&size, 1, sizeof(size), fd))
                                            return false;

                                        if (littleendian())
                                            swap_endian(&size);

                                        chunksize = align_size(size, 4);

                                        // check if RGBA
                                        if (type[0] == 'R' && type[1] == 'G'
                                            && type[2] == 'B'
                                            && type[3] == 'A') {
                                            // get tile coordinates.
                                            uint16_t xmin, xmax, ymin, ymax;
                                            if (!fread(&xmin, 1, sizeof(xmin),
                                                       fd)
                                                || !fread(&ymin, 1,
                                                          sizeof(ymin), fd)
                                                || !fread(&xmax, 1,
                                                          sizeof(xmax), fd)
                                                || !fread(&ymax, 1,
                                                          sizeof(ymax), fd))
                                                return false;

                                            // swap endianness
                                            if (littleendian()) {
                                                swap_endian(&xmin);
                                                swap_endian(&ymin);
                                                swap_endian(&xmax);
                                                swap_endian(&ymax);
                                            }

                                            // check tile
                                            if (xmin > xmax || ymin > ymax
                                                || xmax >= width
                                                || ymax >= height) {
                                                return false;
                                            }

                                            // set tile width and height
                                            tile_width  = xmax - xmin + 1;
                                            tile_height = ymax - ymin + 1;

                                            // done, return
                                            return true;
                                        }

                                        // skip to the next block.
                                        if (fseek(fd, chunksize, SEEK_CUR))
                                            return false;
                                    }
                                } else {
                                    // skip to the next block.
                                    if (fseek(fd, chunksize, SEEK_CUR))
                                        return false;
                                }
                            } else {
                                // skip to the next block.
                                if (fseek(fd, chunksize, SEEK_CUR))
                                    return false;
                            }
                        }
                        // TBHD done, break
                        break;
                    }

                    // skip to the next block.
                    if (fseek(fd, chunksize, SEEK_CUR))
                        return false;
                }
            }
        }
        // skip to the next block.
        if (fseek(fd, chunksize, SEEK_CUR))
            return false;
    }

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
    if (!write_short(1) || !write_short(1))
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
