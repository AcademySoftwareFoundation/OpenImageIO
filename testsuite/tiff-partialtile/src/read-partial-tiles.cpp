// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// Regression test for a heap out-of-bounds write in the TIFF reader.
//
// A tiled TIFF whose dimensions are not an exact multiple of the tile size
// has partial edge tiles. ImageInput::read_native_tiles() into a buffer sized
// exactly to the requested rectangle must copy only the part of each tile
// that lies inside the rectangle. Copying whole tiles overruns the caller's
// buffer and scrambles the pixels of neighboring rows.
//
// This test reads rectangles that include partial edge tiles through the
// low-level read_native_tiles() API (both span and pointer flavors, with
// both the serial and the parallel-decompression code paths) and checks that
// the pixels match a full-image read and that trailing guard bytes are
// untouched.

#include <OpenImageIO/imageio.h>

#include <vector>

using namespace OIIO;


static int
check_rect(ImageInput& in, const std::vector<unsigned char>& full,
           const char* label, bool use_span, int xbegin, int xend, int ybegin,
           int yend)
{
    const ImageSpec& spec = in.spec();
    size_t pixelbytes     = spec.pixel_bytes(true);
    int rw = xend - xbegin, rh = yend - ybegin;
    size_t rowbytes = size_t(rw) * pixelbytes;
    size_t expected = rowbytes * size_t(rh);

    // Exact rectangle buffer plus trailing guard bytes.
    const size_t guard = 4096;
    std::vector<unsigned char> buf(expected + guard, 0xCC);
    bool ok = use_span
                  ? in.read_native_tiles(0, 0, xbegin, xend, ybegin, yend,
                                         as_writable_bytes(
                                             make_span(buf.data(), expected)))
                  : in.read_native_tiles(0, 0, xbegin, xend, ybegin, yend,
                                         spec.z, spec.z + 1, buf.data());
    if (!ok) {
        OIIO::print("{}: read_native_tiles failed: {}\n", label, in.geterror());
        return 1;
    }

    size_t smashed = 0;
    for (size_t i = expected; i < buf.size(); ++i)
        if (buf[i] != 0xCC)
            ++smashed;

    int baddiffs = 0;
    for (int y = 0; y < rh; ++y) {
        const unsigned char* trow = buf.data() + size_t(y) * rowbytes;
        const unsigned char* frow = full.data()
                                    + (size_t(ybegin - spec.y + y) * spec.width
                                       + (xbegin - spec.x))
                                          * pixelbytes;
        for (size_t i = 0; i < rowbytes; ++i)
            if (trow[i] != frow[i])
                ++baddiffs;
    }

    bool pass = (smashed == 0 && baddiffs == 0);
    OIIO::print("{} {}: rect {}x{} smashed={} baddiffs={} -> {}\n", label,
                use_span ? "span" : "ptr", rw, rh, smashed, baddiffs,
                pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}



static int
check_file(const char* filename, int multithread)
{
    OIIO::attribute("tiff:multithread", multithread);
    const char* label = multithread ? "parallel" : "serial";

    auto in = ImageInput::open(filename);
    if (!in) {
        OIIO::print("{}: open failed: {}\n", label, OIIO::geterror());
        return 1;
    }
    const ImageSpec& spec = in->spec();
    int W = spec.width, H = spec.height;

    std::vector<unsigned char> full(size_t(W) * H * spec.pixel_bytes(true));
    if (!in->read_image(0, 0, 0, spec.nchannels, TypeDesc::UNKNOWN,
                        full.data())) {
        OIIO::print("{}: read_image failed\n", label);
        return 1;
    }

    int err = 0;
    for (int s = 0; s < 2; ++s) {
        bool use_span = (s == 0);
        // Whole data window: partial tiles on the right and bottom edges.
        err += check_rect(*in, full, label, use_span, spec.x, spec.x + W,
                          spec.y, spec.y + H);
        // Right-edge column of tiles only.
        int tw = spec.tile_width, th = spec.tile_height;
        err += check_rect(*in, full, label, use_span,
                          spec.x + ((W - 1) / tw) * tw, spec.x + W, spec.y,
                          spec.y + std::min(th, H));
    }
    in->close();
    return err;
}



int
main(int argc, char** argv)
{
    if (argc < 2) {
        OIIO::print("usage: read-partial-tiles file.tif\n");
        return 2;
    }
    int err = 0;
    err += check_file(argv[1], 0);  // serial tile-by-tile path
    err += check_file(argv[1], 1);  // parallel zip decompression path
    return err ? 1 : 0;
}
