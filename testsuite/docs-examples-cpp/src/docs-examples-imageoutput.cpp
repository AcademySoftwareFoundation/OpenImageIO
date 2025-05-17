// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


///////////////////////////////////////////////////////////////////////////
// This file contains code examples from the ImageOutput chapter of the
// main OpenImageIO documentation.
//
// To add an additional test, replicate the section below. Change
// "example1" to a helpful short name that identifies the example.

// BEGIN-imageoutput-example1
#include <OpenImageIO/imageio.h>
using namespace OIIO;

void
example1()
{
    //
    // Example code fragment from the docs goes here.
    //
    // It probably should generate either some text output (which will show up
    // in "out.txt" that captures each test's output), or it should produce a
    // (small) image file that can be compared against a reference image that
    // goes in the ref/ subdirectory of this test.
    //
}
// END-imageoutput-example1

//
///////////////////////////////////////////////////////////////////////////



// BEGIN-imageoutput-simple
#include <OpenImageIO/imageio.h>
using namespace OIIO;

void
simple_write()
{
    const char* filename = "simple.tif";
    const int xres = 320, yres = 240, channels = 3;
    std::vector<unsigned char> pixels(xres * yres * channels);

    std::unique_ptr<ImageOutput> out = ImageOutput::create(filename);
    if (!out)
        return;  // error
    ImageSpec spec(xres, yres, channels, TypeDesc::UINT8);
    out->open(filename, spec);
    out->write_image(make_cspan(pixels));
    out->close();
}
// END-imageoutput-simple



void
scanlines_write()
{
    const char* filename = "scanlines.tif";
    const int xres = 320, yres = 240, channels = 3;

    std::unique_ptr<ImageOutput> out = ImageOutput::create(filename);
    if (!out)
        return;  // error

    // BEGIN-imageoutput-scanlines
    ImageSpec spec(xres, yres, channels, TypeDesc::UINT8);
    std::vector<unsigned char> scanline(xres * channels);
    out->open(filename, spec);
    for (int y = 0; y < yres; ++y) {
        // ... generate data in scanline[0..xres*channels-1] ...
        out->write_scanline(y, make_span(scanline));
    }
    out->close();
    // END-imageoutput-scanlines
}



void
tiles_write()
{
    const char* filename = "tiles.tif";
    const int xres = 320, yres = 240, channels = 3;
    const int tilesize = 64;

    // BEGIN-imageoutput-tiles-create
    std::unique_ptr<ImageOutput> out = ImageOutput::create(filename);
    if (!out)
        return;  // error: could not create output at all
    if (!out->supports("tiles")) {
        // Tiles are not supported
    }
    // END-imageoutput-tiles-create

    // BEGIN-imageoutput-tiles-make-spec-open
    ImageSpec spec(xres, yres, channels, TypeDesc::UINT8);
    spec.tile_width  = tilesize;
    spec.tile_height = tilesize;
    out->open(filename, spec);
    // END-imageoutput-tiles-make-spec-open

    // BEGIN-imageoutput-tiles
    std::vector<uint8_t> tile(tilesize * tilesize * spec.nchannels);
    for (int y = 0; y < yres; y += tilesize) {
        for (int x = 0; x < xres; x += tilesize) {
            out->write_tiles(x, std::min(x + spec.tile_width, spec.width), y,
                             std::min(y + spec.tile_height, spec.height), 0, 1,
                             make_span(tile));
            // ... process the pixels in tile[] ..
            // Watch out for "edge tiles" that are smaller than the full
            // tile size.
            // For example, if the image is 100x100 and the tile size is
            // 32x32, the last tile in each row will be 4x32, the bottom
            // row of tiles will be 32x4, and the very last
            // tile of the whole images will be 4x4.
        }
    }
    out->close();
    // END-imageoutput-tiles
}



int
main(int /*argc*/, char** /*argv*/)
{
    simple_write();
    scanlines_write();
    tiles_write();
    return 0;
}
