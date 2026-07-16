// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// Regression test for a heap out-of-bounds write in the OpenEXR readers.
//
// A tiled OpenEXR whose dimensions are not an exact multiple of the tile size
// exposes an edge-tile range whose width is less than a whole number of tiles.
// ImageInput::read_native_tiles() must place the decoded rows into the
// caller's buffer using the requested-rectangle row stride, not a stride
// padded up to a whole number of tiles. Getting this wrong overruns the
// caller buffer (and/or scrambles the pixels) for the partial edge tile.
//
// This test reads such an edge rectangle through the low-level
// read_native_tiles() API into a buffer sized exactly to the rectangle and
// checks both that the pixels match a full-image read and that trailing guard
// bytes are untouched. It exercises both the classic and the OpenEXR-Core
// based readers.

#include <OpenImageIO/imageio.h>

#include <cstdlib>
#include <vector>

using namespace OIIO;


static int
check_reader(const char* filename, int use_core)
{
    OIIO::attribute("openexr:core", use_core);
    const char* label = use_core ? "core" : "classic";

    auto in = ImageInput::open(filename);
    if (!in) {
        OIIO::print("{}: open failed\n", label);
        return 1;
    }
    const ImageSpec& spec = in->spec();
    int W = spec.width, H = spec.height, C = spec.nchannels;

    // Work in the file's native pixel format, comparing raw bytes so the test
    // is agnostic to the channel type. Full image via read_image, as ground
    // truth.
    size_t pixelbytes = spec.pixel_bytes(true);
    std::vector<unsigned char> full(size_t(W) * H * pixelbytes);
    if (!in->read_image(0, 0, 0, C, TypeDesc::UNKNOWN, full.data())) {
        OIIO::print("{}: read_image failed\n", label);
        return 1;
    }

    // The right-edge column of tiles: [lastxtile*tw, W) x [0, tile_height).
    int tw = spec.tile_width, th = spec.tile_height;
    int xbegin = spec.x + ((W - 1) / tw) * tw;
    int xend   = spec.x + W;
    int ybegin = spec.y;
    int yend   = spec.y + std::min(th, H);
    int rw = xend - xbegin, rh = yend - ybegin;

    size_t rowbytes = size_t(rw) * pixelbytes;
    size_t expected = rowbytes * size_t(rh);

    // Exact rectangle buffer plus trailing guard bytes.
    const size_t guard = 256;
    std::vector<unsigned char> buf(expected + guard, 0xCC);
    bool ok = in->read_native_tiles(0, 0, xbegin, xend, ybegin, yend, spec.z,
                                    spec.z + 1, 0, C, buf.data());
    if (!ok) {
        OIIO::print("{}: read_native_tiles failed: {}\n", label,
                    in->geterror());
        return 1;
    }

    // Guard bytes must be pristine.
    size_t smashed = 0;
    for (size_t i = expected; i < buf.size(); ++i)
        if (buf[i] != 0xCC)
            ++smashed;

    // Each rectangle row must match the corresponding slice of the full image.
    int baddiffs = 0;
    for (int y = 0; y < rh; ++y) {
        const unsigned char* trow = buf.data() + size_t(y) * rowbytes;
        const unsigned char* frow = full.data()
                                    + (size_t(ybegin - spec.y + y) * W
                                       + (xbegin - spec.x))
                                          * pixelbytes;
        for (size_t i = 0; i < rowbytes; ++i)
            if (trow[i] != frow[i])
                ++baddiffs;
    }

    OIIO::print("{}: rect {}x{} smashed={} baddiffs={} -> {}\n", label, rw, rh,
                smashed, baddiffs,
                (smashed == 0 && baddiffs == 0) ? "PASS" : "FAIL");
    in->close();
    return (smashed == 0 && baddiffs == 0) ? 0 : 1;
}


int
main(int argc, char** argv)
{
    if (argc < 2) {
        OIIO::print("usage: read-partial-tiles file.exr\n");
        return 2;
    }
    int err = 0;
    err += check_reader(argv[1], 0);  // classic reader
    err += check_reader(argv[1], 1);  // OpenEXR-Core reader
    return err ? 1 : 0;
}
