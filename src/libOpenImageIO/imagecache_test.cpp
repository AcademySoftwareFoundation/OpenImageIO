// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagecache.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/unittest.h>

#include <iostream>

using namespace OIIO;


static ustring udimpattern;
static ustring checkertex;
static std::vector<ustring> files_to_delete;



static void
create_temp_textures()
{
    // Make a texture
    {
        std::string temp_dir = Filesystem::temp_directory_path();
        OIIO_ASSERT(temp_dir.size());
        udimpattern = ustring::fmtformat("{}/checkertex.<UDIM>.exr", temp_dir);
        checkertex  = ustring::fmtformat("{}/checkertex.1001.exr", temp_dir);
        ImageBuf check = ImageBuf(ImageSpec(256, 256, 3, TypeUInt8));
        ImageBufAlgo::checker(check, 16, 16, 1, { 0.0f, 0.0f, 0.0f },
                              { 1.0f, 1.0f, 1.0f }, 0, 0, 0);
        ImageSpec config;
        config.format = TypeHalf;
        ImageBufAlgo::make_texture(ImageBufAlgo::MakeTxTexture, check,
                                   checkertex, config);
        check.write(checkertex);
        files_to_delete.push_back(checkertex);
    }

    ustring badfile("badfile.exr");
    Filesystem::write_text_file(badfile, "blahblah");
    files_to_delete.push_back(badfile);
}



// Test various get_pixels error handling
static void
test_get_pixels_errors()
{
    Strutil::print("\nTesting get_pixels error handling\n");
    std::shared_ptr<ImageCache> ic = ImageCache::create();
    float fpixels[4 * 4 * 3];
    const int fpixelsize = 3 * sizeof(float);

    OIIO_CHECK_FALSE(ic->get_pixels(ustring("noexist.exr"), 0, 0, 0, 2, 0, 2, 0,
                                    1, TypeFloat, fpixels));
    OIIO_CHECK_ASSERT(ic->has_error());
    Strutil::print("get_pixels of non-existant file:\n  {}\n", ic->geterror());

    OIIO_CHECK_FALSE(ic->get_pixels(ustring("badfile.exr"), 0, 0, 0, 2, 0, 2, 0,
                                    1, TypeFloat, fpixels));
    OIIO_CHECK_ASSERT(ic->has_error());
    Strutil::print("get_pixels of badfile:\n  {}\n", ic->geterror());

    OIIO_CHECK_FALSE(
        ic->get_pixels(checkertex, 8, 0, 0, 2, 0, 2, 0, 1, TypeFloat, fpixels));
    Strutil::print("get_pixels of out-of-range subimage:\n  {}\n",
                   ic->geterror());

    OIIO_CHECK_FALSE(ic->get_pixels(checkertex, 0, 20, 0, 2, 0, 2, 0, 1,
                                    TypeFloat, fpixels));
    Strutil::print("get_pixels of out-of-range miplevel:\n  {}\n",
                   ic->geterror());

    OIIO_CHECK_FALSE(ic->get_pixels(udimpattern, 0, 0, 0, 2, 0, 2, 0, 1,
                                    TypeFloat, fpixels));
    Strutil::print("get_pixels of udim pattern:\n  {}\n", ic->geterror());

    // Check asking for out of range z
    memset(fpixels, 255, sizeof(fpixels));
    OIIO_CHECK_ASSERT(
        ic->get_pixels(checkertex, 0, 0, 0, 2, 0, 2, 1, 2, TypeFloat, fpixels));
    OIIO_CHECK_EQUAL(fpixels[0], 0.0f);
    // and again for non-contiguous strides
    memset(fpixels, 255, sizeof(fpixels));
    OIIO_CHECK_ASSERT(ic->get_pixels(checkertex, 0, 0, 0, 2, 0, 2, 1, 2, 0, 3,
                                     TypeFloat, fpixels, 2 * fpixelsize));
    OIIO_CHECK_EQUAL(fpixels[0], 0.0f);

    // Check asking for out of range y
    memset(fpixels, 255, sizeof(fpixels));
    OIIO_CHECK_ASSERT(ic->get_pixels(checkertex, 0, 0, 0, 2, 10000, 10001, 0, 1,
                                     TypeFloat, fpixels));
    OIIO_CHECK_EQUAL(fpixels[0], 0.0f);
    // and again for non-contiguous strides
    memset(fpixels, 255, sizeof(fpixels));
    OIIO_CHECK_ASSERT(ic->get_pixels(checkertex, 0, 0, 0, 2, 10000, 10001, 0, 1,
                                     0, 3, TypeFloat, fpixels, 2 * fpixelsize));
    OIIO_CHECK_EQUAL(fpixels[0], 0.0f);

    // Check asking for out of range x
    memset(fpixels, 255, sizeof(fpixels));
    OIIO_CHECK_ASSERT(ic->get_pixels(checkertex, 0, 0, 10000, 10001, 0, 2, 0, 1,
                                     TypeFloat, fpixels));
    OIIO_CHECK_EQUAL(fpixels[0], 0.0f);
}



// Tests various ways for the subset of channels to be cached in a
// many-channel image.
void
test_get_pixels_cachechannels(int chbegin = 0, int chend = 4,
                              int cache_chbegin = 0, int cache_chend = -1)
{
    std::cout << "\nTesting IC get_pixels of chans [" << chbegin << "," << chend
              << ") with cache range [" << cache_chbegin << "," << cache_chend
              << "):\n";
    auto imagecache = ImageCache::create(false);

    // Create a 10 channel file
    ustring filename("tenchannels.tif");
    const int nchans = 10;
    ImageBuf A(ImageSpec(64, 64, nchans, TypeDesc::FLOAT));
    const float pixelvalue[nchans] = { 0.0f, 0.1f, 0.2f, 0.3f, 0.4f,
                                       0.5f, 0.6f, 0.7f, 0.8f, 0.9f };
    ImageBufAlgo::fill(A, cspan<float>(pixelvalue));
    A.write(filename);
    files_to_delete.push_back(filename);

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
    auto imagecache = ImageCache::create(false /*not shared*/);

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
}



void
test_custom_threadinfo()
{
    Strutil::print("\nTesting creating/destroying custom IC and thread info\n");
    auto imagecache = ImageCache::create(true);
    auto threadinfo = imagecache->create_thread_info();
    OIIO_CHECK_ASSERT(threadinfo != nullptr);
    imagecache->destroy_thread_info(threadinfo);
    imagecache->close_all();
}



void
test_tileptr()
{
    Strutil::print("\nTesting tile ptr things\n");
    auto imagecache        = ImageCache::create();
    auto hand              = imagecache->get_image_handle(checkertex);
    ImageCache::Tile* tile = imagecache->get_tile(hand, nullptr, 0, 0, 4, 4, 0);
    OIIO_CHECK_ASSERT(tile != nullptr);
    Strutil::print("tile @ 4,4 format {} ROI {}\n",
                   imagecache->tile_format(tile), imagecache->tile_roi(tile));
    OIIO_CHECK_EQUAL(imagecache->tile_format(tile), TypeHalf);
    OIIO_CHECK_EQUAL(imagecache->tile_roi(tile),
                     ROI(0, 256, 0, 256, 0, 1, 0, 3));
    TypeDesc tileformat;
    OIIO_CHECK_ASSERT(imagecache->tile_pixels(tile, tileformat) != nullptr);
    OIIO_CHECK_ASSERT(tileformat == TypeHalf);

    // Some error handling cases
    OIIO_CHECK_ASSERT(imagecache->get_tile(nullptr, nullptr, 0, 0, 4, 4, 0)
                      == nullptr);  // null tile ptr
    OIIO_CHECK_ASSERT(imagecache->get_tile(hand, nullptr, 1, 0, 400, 400, 0)
                      == nullptr);  // nonexistent tile

    imagecache->release_tile(tile);
}



static void
test_imagespec()
{
    Strutil::print("\nTesting imagespec retrieval\n");
    auto ic = ImageCache::create();

    {  // basic get_imagespec()
        ImageSpec spec;
        OIIO_CHECK_ASSERT(ic->get_imagespec(checkertex, spec));
        OIIO_CHECK_EQUAL(spec.width, 256);
    }
    {  // basic get_imagespec() with handle
        auto hand = ic->get_image_handle(checkertex);
        ImageSpec spec;
        OIIO_CHECK_ASSERT(ic->get_imagespec(hand, nullptr, spec));
        OIIO_CHECK_EQUAL(spec.width, 256);
    }

    {  // get_imagespec() for nonexistant file
        ImageSpec spec;
        OIIO_CHECK_FALSE(ic->get_imagespec(ustring("noexist.exr"), spec));
        OIIO_CHECK_ASSERT(ic->has_error());
        Strutil::print("get_imagespec() of non-existant file:\n  {}\n",
                       ic->geterror());
    }
    {  // imagespec() for nonexistant file
        const ImageSpec* spec = ic->imagespec(ustring("noexist.exr"));
        OIIO_CHECK_ASSERT(spec == nullptr && ic->has_error());
        Strutil::print("imagespec() of non-existant file:\n  {}\n",
                       ic->geterror());
    }
    {  // imagespec() for null handle
        const ImageSpec* spec = ic->imagespec(nullptr, nullptr);
        OIIO_CHECK_ASSERT(spec == nullptr && ic->has_error());
        Strutil::print("imagespec(handle) of non-existant file:\n  {}\n",
                       ic->geterror());
    }
    {  // imagespec() for out of range subimage
        const ImageSpec* spec = ic->imagespec(checkertex, 10);
        OIIO_CHECK_ASSERT(spec == nullptr && ic->has_error());
        Strutil::print("imagespec() out-of-range subimage:\n  {}\n",
                       ic->geterror());
    }
}



static void
test_get_cache_dimensions()
{
    Strutil::print("\nTesting cache dimensions retrieval\n");
    auto ic = ImageCache::create();

    {  // basic get_cache_dimensions()
        ImageSpec spec;
        OIIO_CHECK_ASSERT(ic->get_cache_dimensions(checkertex, spec));
        OIIO_CHECK_EQUAL(spec.width, 256);
    }
    {  // basic get_cache_dimensions() with handle
        auto hand = ic->get_image_handle(checkertex);
        ImageSpec spec;
        OIIO_CHECK_ASSERT(ic->get_cache_dimensions(hand, nullptr, spec));
        OIIO_CHECK_EQUAL(spec.width, 256);
    }

    {  // get_cache_dimensions() for nonexistant file
        ImageSpec spec;
        OIIO_CHECK_FALSE(
            ic->get_cache_dimensions(ustring("noexist.exr"), spec));
        OIIO_CHECK_ASSERT(ic->has_error());
        Strutil::print("get_cache_dimensions() of non-existant file:\n  {}\n",
                       ic->geterror());
    }
    {  // get_cache_dimensions() for null handle
        ImageSpec spec;
        const bool valid = ic->get_cache_dimensions(nullptr, nullptr, spec);
        OIIO_CHECK_ASSERT(!valid && ic->has_error());
        Strutil::print(
            "get_cache_dimensions(handle) of non-existant file:\n  {}\n",
            ic->geterror());
    }
    {  // get_cache_dimensions() for out of range subimage
        ImageSpec spec;
        const bool valid = ic->get_cache_dimensions(checkertex, spec, 10);
        OIIO_CHECK_ASSERT(!valid && ic->has_error());
        Strutil::print("get_cache_dimensions() out-of-range subimage:\n  {}\n",
                       ic->geterror());
    }
    {  // get_cache_dimensions() for out of range mip level
        ImageSpec spec;
        const bool valid = ic->get_cache_dimensions(checkertex, spec, 0, 100);
        OIIO_CHECK_ASSERT(!valid && ic->has_error());
        Strutil::print("get_cache_dimensions() out-of-range miplevel:\n  {}\n",
                       ic->geterror());
    }
}



int
main(int /*argc*/, char* /*argv*/[])
{
    create_temp_textures();

    test_get_pixels_cachechannels(0, 10);
    test_get_pixels_cachechannels(0, 4);
    test_get_pixels_cachechannels(0, 4, 0, 6);
    test_get_pixels_cachechannels(0, 4, 0, 4);
    test_get_pixels_cachechannels(6, 9);
    test_get_pixels_cachechannels(6, 9, 6, 9);

    test_app_buffer();
    test_tileptr();
    test_get_pixels_errors();
    test_custom_threadinfo();
    test_imagespec();
    test_get_cache_dimensions();

    auto ic = ImageCache::create();
    Strutil::print("\n\n{}\n", ic->getstats(5));
    ic->reset_stats();

    for (auto f : files_to_delete)
        Filesystem::remove(f);
    return unit_test_failures;
}
