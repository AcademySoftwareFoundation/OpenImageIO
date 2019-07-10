// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md


#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagecache.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/unittest.h>

#include <iostream>

using namespace OIIO;



// Tests various ways for the subset of channels to be cached in a
// many-channel image.
void
test_get_pixels_cachechannels(int chbegin = 0, int chend = 4,
                              int cache_chbegin = 0, int cache_chend = -1)
{
    std::cout << "\nTesting IC get_pixels of chans [" << chbegin << "," << chend
              << ") with cache range [" << cache_chbegin << "," << cache_chend
              << "):\n";
    ImageCache* imagecache = ImageCache::create(false /*not shared*/);

    // Create a 10 channel file
    ustring filename("tenchannels.tif");
    const int nchans = 10;
    ImageBuf A(ImageSpec(64, 64, nchans, TypeDesc::FLOAT));
    const float pixelvalue[nchans] = { 0.0f, 0.1f, 0.2f, 0.3f, 0.4f,
                                       0.5f, 0.6f, 0.7f, 0.8f, 0.9f };
    ImageBufAlgo::fill(A, pixelvalue);
    A.write(filename);

    // Retrieve 2 pixels of [chbegin,chend), make sure we got the right values
    float p[2 * nchans] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    OIIO_CHECK_ASSERT(imagecache->get_pixels(filename, 0, 0, 0, 2, 0, 1, 0,
                                             1,  // pixel range
                                             chbegin, chend, TypeDesc::FLOAT, p,
                                             AutoStride, AutoStride, AutoStride,
                                             cache_chbegin, cache_chend));
    int nc = chend - chbegin;
    for (int x = 0; x < 2; ++x) {
        for (int c = 0; c < nc; ++c) {
            std::cout << ' ' << p[x * nc + c];
            OIIO_CHECK_EQUAL(p[x * nc + c], pixelvalue[c + chbegin]);
        }
        std::cout << "\n";
    }
    for (int c = 2 * nc; c < 2 * nchans; ++c)
        OIIO_CHECK_EQUAL(p[c], -1.0f);

    ImageCache::destroy(imagecache);
}



// Wimple wrapper to return a raw "null" ImageInput*.
static ImageInput*
NullInputCreator()
{
    // Note: we can't create it directly, but we can ask for a managed
    // pointer and then release the raw pointer from it.
    return ImageInput::create("0.null").release();
}


// Test the ability to add an application buffer to make it appear as if
// it's an image in the cache.
void
test_app_buffer()
{
    ImageCache* imagecache = ImageCache::create(false /*not shared*/);

    // Add a file entry with a "null" ImageInput proxy configured to look
    // like a 2x2 RGB float image.
    ustring fooname("foo");
    const int xres = 2, yres = 2, chans = 3;
    TypeDesc imgtype = TypeDesc::FLOAT;
    ImageSpec config(xres, yres, chans, imgtype);
    config.tile_width  = xres;
    config.tile_height = yres;
    config.attribute("null:force", 1);  // necessary because no .null extension
    bool fadded = imagecache->add_file(fooname, NullInputCreator, &config);
    OIIO_CHECK_ASSERT(fadded);

    // Make sure it got added correctly
    ImageSpec retrieved_spec;
    imagecache->get_imagespec(fooname, retrieved_spec);
    OIIO_CHECK_EQUAL(retrieved_spec.width, xres);
    OIIO_CHECK_EQUAL(retrieved_spec.height, yres);
    OIIO_CHECK_EQUAL(retrieved_spec.format, imgtype);

    // Here's our image of data, in our own buffer.
    static float pixels[yres][xres][chans] = { { { 0, 0, 0 }, { 0, 1, 0 } },
                                               { { 1, 0, 0 }, { 1, 1, 0 } } };
    // Add a proxy tile that points to -- but does not copy -- the image.
    bool ok = imagecache->add_tile(fooname, 0 /* subimage */, 0 /* miplevel */,
                                   0, 0, 0,         /* origin */
                                   0, chans,        /* channel range */
                                   imgtype, pixels, /* the buffer */
                                   AutoStride, AutoStride,
                                   AutoStride, /* strides */
                                   false /* DO NOT COPY THE PIXELS! */);
    OIIO_CHECK_ASSERT(ok);

    // Check that we can retrieve the tile.
    ImageCache::Tile* tile = imagecache->get_tile(fooname, 0, 0, 0, 0, 0);
    OIIO_CHECK_ASSERT(tile != nullptr);
    imagecache->release_tile(tile);  // de-refcount what we got from get_tile

    // Check that the tile's pixels appear to actually be our own buffer
    TypeDesc format;
    const void* pels = imagecache->tile_pixels(tile, format);
    OIIO_CHECK_EQUAL(pels, pixels);
    OIIO_CHECK_EQUAL(format, TypeDesc::FLOAT);

    // Check that retrieving the pixel (as would be done by the texture
    // system) returns the right color. This would work for texture calls
    // and whatnot as well.
    float testpixel[3] = { -1, -1, -1 };
    imagecache->get_pixels(fooname, 0, 0, 1, 2, 1, 2, 0, 1, 0, 3,
                           TypeDesc::FLOAT, testpixel);
    OIIO_CHECK_EQUAL(testpixel[0], pixels[1][1][0]);
    OIIO_CHECK_EQUAL(testpixel[1], pixels[1][1][1]);
    OIIO_CHECK_EQUAL(testpixel[2], pixels[1][1][2]);

    ImageCache::destroy(imagecache);
}



int
main(int argc, char** argv)
{
    test_get_pixels_cachechannels(0, 10);
    test_get_pixels_cachechannels(0, 4);
    test_get_pixels_cachechannels(0, 4, 0, 6);
    test_get_pixels_cachechannels(0, 4, 0, 4);
    test_get_pixels_cachechannels(6, 9);
    test_get_pixels_cachechannels(6, 9, 6, 9);

    test_app_buffer();

    return unit_test_failures;
}
