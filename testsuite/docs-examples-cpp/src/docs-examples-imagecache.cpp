// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


///////////////////////////////////////////////////////////////////////////
// This file contains code examples from the ImageCache chapter of the
// main OpenImageIO documentation.
//
// To add an additional test, replicate the section below. Change
// "example1" to a helpful short name that identifies the example.

// BEGIN-imagecache-example1
#include <OpenImageIO/imagecache.h>
#include <iostream>
#include <vector>
using namespace OIIO;

void
example1()
{
    // Create an image cache and set some options
    auto cache = ImageCache::create();
    cache->attribute("max_memory_MB", 500.0f);
    cache->attribute("autotile", 64);

    ustring filename("tahoe.tif");

    // Get information about a file
    ImageSpec spec;
    bool ok = cache->get_imagespec(filename, spec);
    if (ok)
        std::cout << "resolution is " << spec.width << "x" << spec.height
                  << "\n";

    // Get a block of pixels from a file.
    const int xbegin = 0, xend = 4, ybegin = 0, yend = 4;
    const int nchannels = spec.nchannels;
    ROI roi(xbegin, xend, ybegin, yend, 0, 1, 0, nchannels);
    std::vector<float> pixels(roi.width() * roi.height() * nchannels);
    image_span<float> pixelspan(pixels.data(), nchannels, roi.width(),
                                roi.height());
    cache->get_pixels(filename, 0, 0, roi, pixelspan);

    // Request and hold a tile, do some work with its pixels, then release
    ImageCache::Tile* tile = cache->get_tile(filename, 0, 0, 0, 0, 0);
    if (tile) {
        // The tile won't be freed until we release it, so this is safe:
        TypeDesc format;
        const void* p = cache->tile_pixels(tile, format);
        // Now p points to the raw pixels of the tile, whose data format
        // is given by 'format'.
        (void)p;
        cache->release_tile(tile);
        // Now cache is permitted to free the tile when needed
    }

    // Note that all files were referenced by name, we never had to open
    // or close any files, and all the resource and memory management
    // was automatic.

    ImageCache::destroy(cache);
}
// END-imagecache-example1

//
///////////////////////////////////////////////////////////////////////////



int
main(int /*argc*/, char** /*argv*/)
{
    // Each example function needs to get called here, or it won't execute
    // as part of the test.
    example1();
    return 0;
}
