// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#ifndef OPENIMAGEIO_IV_UTILS_H
#define OPENIMAGEIO_IV_UTILS_H

#include <algorithm>
#include <string>
#include <vector>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/oiioversion.h>

OIIO_NAMESPACE_BEGIN

/// Round up to the next power of 2
/// TODO: This should be optimized to use bit arithmetic on the ieee float
/// representation.  Once optimized and tested, move to fmath.h

inline float
ceil2f(float f)
{
    float logval = logf(f) / logf(2.0f);
    logval += 1e-6f;  // add floating point slop. this supports [0.00012207,8192]
    return powf(2.0f, ceilf(logval));
}

/// Round down to the next power of 2
/// TODO: This should be optimized to use bit arithmetic on the ieee float
/// representation.  Once optimized and tested, move to fmath.h

inline float
floor2f(float f)
{
    float logval = logf(f) / logf(2.0f);
    logval -= 1e-6f;  // add floating point slop. this supports [0.00012207,8192]
    return powf(2.0f, floorf(logval));
}


// Probe whether all of the pixel data of the image named `filename` is
// readable, without reading the whole image: attempt to read only the
// last scanline (for scanline files) or the last tile (for tiled files),
// which is the most likely region to be missing from a file that was
// only partially written. This is cheap enough to use after a read that
// tolerates missing pixels (see the "oiio:missingcolor" input config
// option), only to find out whether the fill color was actually needed,
// so that the user can be notified. The file is probed strictly, without
// any config, so that unreadable regions fail rather than being filled.
//
// Returns true if the last scanline/tile could be read (and therefore it
// is likely that the entire pixel data block is intact), false if it
// could not (meaning that the file is probably truncated or otherwise
// partially written).
inline bool
image_data_readable(string_view filename, int subimage, int miplevel)
{
    auto in = ImageInput::open(filename);
    if (!in)
        return false;
    if (!in->seek_subimage(subimage, miplevel)) {
        in->close();
        return false;
    }
    ImageSpec spec = in->spec(subimage, miplevel);
    bool ok        = false;
    if (spec.tile_width > 0) {
        // Try to read the bottom-most, right-most tile.
        int tx = spec.x
                 + ((spec.width - 1) / spec.tile_width) * spec.tile_width;
        int ty = spec.y
                 + ((spec.height - 1) / spec.tile_height) * spec.tile_height;
        int tz = spec.z
                 + ((std::max(spec.depth - 1, 0))
                    / std::max(spec.tile_depth, 1))
                       * std::max(spec.tile_depth, 1);
        std::vector<char> buf(spec.tile_bytes());
        ok = in->read_tile(tx, ty, tz, spec.format, buf.data());
    } else {
        // Try to read the last scanline.
        int y = spec.y + spec.height - 1;
        std::vector<char> buf(spec.scanline_bytes());
        ok = in->read_scanlines(subimage, miplevel, y, y + 1, spec.z, 0,
                                spec.nchannels, TypeDesc::UNKNOWN, buf.data());
    }
    in->close();
    return ok;
}

OIIO_NAMESPACE_END

#endif  // OPENIMAGEIO_IV_UTILS_H
