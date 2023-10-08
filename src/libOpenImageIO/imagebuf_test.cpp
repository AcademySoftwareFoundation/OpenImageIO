// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <OpenImageIO/benchmark.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagecache.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/unittest.h>

#include <iostream>

using namespace OIIO;



inline int
test_wrap(wrap_impl wrap, int coord, int origin, int width)
{
    wrap(coord, origin, width);
    return coord;
}


void
test_wrapmodes()
{
    const int ori    = 0;
    const int w      = 4;
    static int val[] = { -7, -6, -5, -4, -3, -2, -1, 0, 1,
                         2,  3,  4,  5,  6,  7,  8,  9, -10 },
               cla[] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 3, 3, 3, 3, 3, 3 },
               per[] = { 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1 },
               mir[] = { 1, 2, 3, 3, 2, 1, 0, 0, 1, 2, 3, 3, 2, 1, 0, 0, 1 };

    for (int i = 0; val[i] > -10; ++i) {
        OIIO_CHECK_EQUAL(test_wrap(wrap_clamp, val[i], ori, w), cla[i]);
        OIIO_CHECK_EQUAL(test_wrap(wrap_periodic, val[i], ori, w), per[i]);
        OIIO_CHECK_EQUAL(test_wrap(wrap_periodic_pow2, val[i], ori, w), per[i]);
        OIIO_CHECK_EQUAL(test_wrap(wrap_mirror, val[i], ori, w), mir[i]);
    }
}



void
test_is_imageio_format_name()
{
    OIIO_CHECK_EQUAL(is_imageio_format_name(""), false);
    OIIO_CHECK_EQUAL(is_imageio_format_name("openexr"), true);
    OIIO_CHECK_EQUAL(is_imageio_format_name("OpEnExR"), true);
    OIIO_CHECK_EQUAL(is_imageio_format_name("tiff"), true);
    OIIO_CHECK_EQUAL(is_imageio_format_name("tiffx"), false);
    OIIO_CHECK_EQUAL(is_imageio_format_name("blort"), false);
}



// Test iterators
template<class ITERATOR>
void
iterator_read_test()
{
    const int WIDTH = 4, HEIGHT = 4, CHANNELS = 3;
    static float buf[HEIGHT][WIDTH][CHANNELS]
        = { { { 0, 0, 0 }, { 1, 0, 1 }, { 2, 0, 2 }, { 3, 0, 3 } },
            { { 0, 1, 4 }, { 1, 1, 5 }, { 2, 1, 6 }, { 3, 1, 7 } },
            { { 0, 2, 8 }, { 1, 2, 9 }, { 2, 2, 10 }, { 3, 2, 11 } },
            { { 0, 3, 12 }, { 1, 3, 13 }, { 2, 3, 14 }, { 3, 3, 15 } } };
    ImageSpec spec(WIDTH, HEIGHT, CHANNELS, TypeDesc::FLOAT);
    ImageBuf A(spec, buf);

    ITERATOR p(A);
    OIIO_CHECK_EQUAL(p[0], 0.0f);
    OIIO_CHECK_EQUAL(p[1], 0.0f);
    OIIO_CHECK_EQUAL(p[2], 0.0f);

    // Explicit position
    p.pos(2, 1);
    OIIO_CHECK_EQUAL(p.x(), 2);
    OIIO_CHECK_EQUAL(p.y(), 1);
    OIIO_CHECK_EQUAL(p[0], 2.0f);
    OIIO_CHECK_EQUAL(p[1], 1.0f);
    OIIO_CHECK_EQUAL(p[2], 6.0f);

    // Iterate a few times
    ++p;
    OIIO_CHECK_EQUAL(p.x(), 3);
    OIIO_CHECK_EQUAL(p.y(), 1);
    OIIO_CHECK_EQUAL(p[0], 3.0f);
    OIIO_CHECK_EQUAL(p[1], 1.0f);
    OIIO_CHECK_EQUAL(p[2], 7.0f);
    ++p;
    OIIO_CHECK_EQUAL(p.x(), 0);
    OIIO_CHECK_EQUAL(p.y(), 2);
    OIIO_CHECK_EQUAL(p[0], 0.0f);
    OIIO_CHECK_EQUAL(p[1], 2.0f);
    OIIO_CHECK_EQUAL(p[2], 8.0f);

    std::cout << "iterator_read_test result:";
    int i = 0;
    for (ITERATOR p(A); !p.done(); ++p, ++i) {
        if ((i % 4) == 0)
            std::cout << "\n    ";
        std::cout << "   " << p[0] << ' ' << p[1] << ' ' << p[2];
    }
    std::cout << "\n";
}



// Test iterators
template<class ITERATOR>
void
iterator_wrap_test(ImageBuf::WrapMode wrap, std::string wrapname)
{
    const int WIDTH = 4, HEIGHT = 4, CHANNELS = 3;
    static float buf[HEIGHT][WIDTH][CHANNELS]
        = { { { 0, 0, 0 }, { 1, 0, 1 }, { 2, 0, 2 }, { 3, 0, 3 } },
            { { 0, 1, 4 }, { 1, 1, 5 }, { 2, 1, 6 }, { 3, 1, 7 } },
            { { 0, 2, 8 }, { 1, 2, 9 }, { 2, 2, 10 }, { 3, 2, 11 } },
            { { 0, 3, 12 }, { 1, 3, 13 }, { 2, 3, 14 }, { 3, 3, 15 } } };
    ImageSpec spec(WIDTH, HEIGHT, CHANNELS, TypeDesc::FLOAT);
    ImageBuf A(spec, buf);

    std::cout << "iterator_wrap_test " << wrapname << ":";
    int i        = 0;
    int noutside = 0;
    for (ITERATOR p(A, ROI(-2, WIDTH + 2, -2, HEIGHT + 2), wrap); !p.done();
         ++p, ++i) {
        if ((i % 8) == 0)
            std::cout << "\n    ";
        std::cout << "   " << p[0] << ' ' << p[1] << ' ' << p[2];
        // Check wraps
        if (!p.exists()) {
            ++noutside;
            if (wrap == ImageBuf::WrapBlack) {
                OIIO_CHECK_EQUAL(p[0], 0.0f);
                OIIO_CHECK_EQUAL(p[1], 0.0f);
                OIIO_CHECK_EQUAL(p[2], 0.0f);
            } else if (wrap == ImageBuf::WrapClamp) {
                ITERATOR q = p;
                q.pos(clamp(p.x(), 0, WIDTH - 1), clamp(p.y(), 0, HEIGHT - 1));
                OIIO_CHECK_EQUAL(p[0], q[0]);
                OIIO_CHECK_EQUAL(p[1], q[1]);
                OIIO_CHECK_EQUAL(p[2], q[2]);
            } else if (wrap == ImageBuf::WrapPeriodic) {
                ITERATOR q = p;
                q.pos(p.x() % WIDTH, p.y() % HEIGHT);
                OIIO_CHECK_EQUAL(p[0], q[0]);
                OIIO_CHECK_EQUAL(p[1], q[1]);
                OIIO_CHECK_EQUAL(p[2], q[2]);
            } else if (wrap == ImageBuf::WrapMirror) {
                ITERATOR q = p;
                int x = p.x(), y = p.y();
                wrap_mirror(x, 0, WIDTH);
                wrap_mirror(y, 0, HEIGHT);
                q.pos(x, y);
                OIIO_CHECK_EQUAL(p[0], q[0]);
                OIIO_CHECK_EQUAL(p[1], q[1]);
                OIIO_CHECK_EQUAL(p[2], q[2]);
            }
        }
    }
    std::cout << "\n";
    OIIO_CHECK_EQUAL(noutside, 48);  // Should be 48 wrapped pixels
}



// Tests ImageBuf construction from application buffer
void
ImageBuf_test_appbuffer()
{
    const int WIDTH    = 8;
    const int HEIGHT   = 8;
    const int CHANNELS = 3;
    // clang-format off
    float buf[HEIGHT][WIDTH][CHANNELS] = {
        { {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {1,0,0}, {0,0,0}, {0,0,0}, {0,0,0} },
        { {0,0,0}, {0,0,0}, {0,0,0}, {1,0,0}, {0,0,0}, {1,0,0}, {0,0,0}, {0,0,0} },
        { {0,0,0}, {0,0,0}, {1,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {1,0,0}, {0,0,0} },
        { {0,0,0}, {1,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {1,0,0} },
        { {0,0,0}, {0,0,0}, {1,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {1,0,0}, {0,0,0} },
        { {0,0,0}, {0,0,0}, {0,0,0}, {1,0,0}, {0,0,0}, {1,0,0}, {0,0,0}, {0,0,0} },
        { {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {1,0,0}, {0,0,0}, {0,0,0}, {0,0,0} },
        { {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0} }
    };
    // clang-format on
    ImageSpec spec(WIDTH, HEIGHT, CHANNELS, TypeDesc::FLOAT);
    ImageBuf A(spec, buf);

    // Make sure A now points to the buffer
    OIIO_CHECK_EQUAL((void*)A.pixeladdr(0, 0, 0), (void*)buf);

    // write it
    A.write("A_imagebuf_test.tif");

    // Read it back and make sure it matches the original
    ImageBuf B("A_imagebuf_test.tif");
    for (int y = 0; y < HEIGHT; ++y)
        for (int x = 0; x < WIDTH; ++x)
            for (int c = 0; c < WIDTH; ++c)
                OIIO_CHECK_EQUAL(A.getchannel(x, y, 0, c),
                                 B.getchannel(x, y, 0, c));

    // Make sure we can write to the buffer
    float pix[CHANNELS] = { 0.0, 42.0, 0 };
    A.setpixel(3, 2, 0, pix);
    OIIO_CHECK_EQUAL(buf[2][3][1], 42.0);

    // Make sure we can copy-construct the ImageBuf and it points to the
    // same application buffer.
    ImageBuf C(A);
    OIIO_CHECK_EQUAL((void*)A.pixeladdr(0, 0, 0), (void*)C.pixeladdr(0, 0, 0));

    // Test that channel and pixel strides work
    OIIO_CHECK_EQUAL((float*)A.pixeladdr(0, 0, 0, 1),
                     (float*)A.pixeladdr(0, 0, 0) + 1);
    OIIO_CHECK_EQUAL(A.pixel_stride(), (stride_t)sizeof(float) * CHANNELS);
}



void
ImageBuf_test_appbuffer_strided()
{
    Strutil::print("Testing strided app buffers\n");

    // Make a 16x16 x 3chan float buffer, fill with zero
    const int res = 16, nchans = 3;
    float mem[res][res][nchans];
    memset(mem, 0, res * res * nchans * sizeof(float));

    // Wrap the whole buffer, fill with green
    ImageBuf wrapped(ImageSpec(res, res, nchans, TypeFloat), mem);
    const float green[nchans] = { 0.0f, 1.0f, 0.0f };
    ImageBufAlgo::fill(wrapped, green);
    float color[nchans] = { -1, -1, -1 };
    OIIO_CHECK_ASSERT(ImageBufAlgo::isConstantColor(wrapped, 0.0f, color)
                      && color[0] == 0.0f && color[1] == 1.0f
                      && color[2] == 0.0f);

    // Do a strided wrap in the interior: a 3x3 image with extra spacing
    // between pixels and rows, and fill it with red.
    ImageBuf strided(ImageSpec(3, 3, nchans, TypeFloat), &mem[4][4][0],
                     2 * nchans * sizeof(float) /* every other pixel */,
                     2 * res * nchans * sizeof(float) /* ever other line */);
    const float red[nchans] = { 1.0f, 0.0f, 0.0f };
    ImageBufAlgo::fill(strided, red);

    // The strided IB ought to look all-red
    OIIO_CHECK_ASSERT(ImageBufAlgo::isConstantColor(strided, 0.0f, color)
                      && color[0] == 1.0f && color[1] == 0.0f
                      && color[2] == 0.0f);

    // The wrapped IB ought NOT to look like one color
    OIIO_CHECK_ASSERT(!ImageBufAlgo::isConstantColor(wrapped, 0.0f, color));

    // Write both to disk and make sure they are what we think they are
    {
        strided.write("stridedfill.tif", TypeUInt8);
        ImageBuf test("stridedfill.tif");  // read it back
        float color[nchans] = { -1, -1, -1 };
        OIIO_CHECK_ASSERT(ImageBufAlgo::isConstantColor(test, 0.0f, color)
                          && color[0] == 1.0f && color[1] == 0.0f
                          && color[2] == 0.0f);
    }
    {
        wrapped.write("wrappedfill.tif", TypeUInt8);
        ImageBuf test("wrappedfill.tif");  // read it back
        // Slightly tricky test because of the strides
        for (int y = 0; y < res; ++y) {
            for (int x = 0; x < res; ++x) {
                float pixel[nchans];
                test.getpixel(x, y, pixel);
                if ((x == 4 || x == 6 || x == 8)
                    && (y == 4 || y == 6 || y == 8)) {
                    OIIO_CHECK_ASSERT(cspan<float>(pixel) == cspan<float>(red));
                } else {
                    OIIO_CHECK_ASSERT(cspan<float>(pixel)
                                      == cspan<float>(green));
                }
            }
        }
    }
}



void
test_open_with_config()
{
    // N.B. This function must run after ImageBuf_test_appbuffer, which
    // writes "A.tif".
    ImageCache* ic = ImageCache::create(false);
    ImageSpec config;
    config.attribute("oiio:DebugOpenConfig!", 1);
    ImageBuf A("A_imagebuf_test.tif", 0, 0, ic, &config);
    OIIO_CHECK_EQUAL(A.spec().get_int_attribute("oiio:DebugOpenConfig!", 0),
                     42);
    // Clear A because it would be unwise to let the ImageBuf outlive the
    // custom ImageCache we passed it to use.
    A.clear();
    ic->destroy(ic);
}



void
test_empty_iterator()
{
    // Ensure that ImageBuf iterators over empty ROIs immediately appear
    // done
    ImageBuf A(ImageSpec(64, 64, 3, TypeDesc::FLOAT));
    ROI roi(10, 10, 20, 40, 0, 1);
    for (ImageBuf::Iterator<float> p(A, roi); !p.done(); ++p) {
        std::cout << "p is " << p.x() << ' ' << p.y() << ' ' << p.z() << "\n";
        OIIO_CHECK_ASSERT(0 && "should never execute this loop body");
    }
}



void
print(const ImageBuf& A)
{
    OIIO_DASSERT(A.spec().format == TypeDesc::FLOAT);
    for (ImageBuf::ConstIterator<float> p(A); !p.done(); ++p) {
        std::cout << "   @" << p.x() << ',' << p.y() << "=(";
        for (int c = 0; c < A.nchannels(); ++c)
            std::cout << (c ? "," : "") << p[c];
        std::cout << ')' << (p.x() == A.xmax() ? "\n" : "");
    }
    std::cout << "\n";
}



void
test_set_get_pixels()
{
    std::cout << "\nTesting set_pixels, get_pixels:\n";
    const int nchans = 3;
    ImageBuf A(ImageSpec(4, 4, nchans, TypeDesc::FLOAT));
    ImageBufAlgo::zero(A);
    std::cout << " Cleared:\n";
    print(A);
    float newdata[2 * 2 * nchans] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
    A.set_pixels(ROI(1, 3, 1, 3), TypeDesc::FLOAT, newdata);
    std::cout << " After set:\n";
    print(A);
    float retrieved[2 * 2 * nchans] = { 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9 };
    A.get_pixels(ROI(1, 3, 1, 3, 0, 1), TypeDesc::FLOAT, retrieved);
    OIIO_CHECK_ASSERT(0 == memcmp(retrieved, newdata, 2 * 2 * nchans));
}



void
time_get_pixels()
{
    std::cout << "\nTesting set_pixels, get_pixels:\n";
    Benchmarker bench;
    const int nchans = 4;
    const int xres = 2000, yres = 1000;
    ImageBuf A(ImageSpec(xres, yres, nchans, TypeDesc::FLOAT));
    ImageBufAlgo::zero(A);

    // bench.work (size_t(xres*yres*nchans));
    std::unique_ptr<float[]> fbuf(new float[xres * yres * nchans]);
    bench("get_pixels 1Mpelx4 float[4]->float[4] ",
          [&]() { A.get_pixels(A.roi(), TypeFloat, fbuf.get()); });
    bench("get_pixels 1Mpelx4 float[4]->float[3] ", [&]() {
        ROI roi3   = A.roi();
        roi3.chend = 3;
        A.get_pixels(roi3, TypeFloat, fbuf.get());
    });

    std::unique_ptr<uint8_t[]> ucbuf(new uint8_t[xres * yres * nchans]);
    bench("get_pixels 1Mpelx4 float[4]->uint8[4] ",
          [&]() { A.get_pixels(A.roi(), TypeUInt8, ucbuf.get()); });

    std::unique_ptr<uint16_t[]> usbuf(new uint16_t[xres * yres * nchans]);
    bench("get_pixels 1Mpelx4 float[4]->uint16[4] ",
          [&]() { A.get_pixels(A.roi(), TypeUInt8, usbuf.get()); });
}



void
test_read_channel_subset()
{
    std::cout << "\nTesting reading a channel subset\n";

    // FIrst, write a test image with 6 channels
    static float color6[] = { 0.6f, 0.5f, 0.4f, 0.3f, 0.2f, 0.1f };
    ImageBuf A(ImageSpec(2, 2, 6, TypeDesc::FLOAT));
    ImageBufAlgo::fill(A, color6);
    A.write("sixchans.tif");
    std::cout << " Start with image:\n";
    print(A);

    // Now read it back using the "channel range" option.
    ImageBuf B("sixchans.tif");
    B.read(0 /*subimage*/, 0 /*mip*/, 2 /*chbegin*/, 5 /*chend*/,
           true /*force*/, TypeDesc::FLOAT);
    std::cout << " After reading channels [2,5), we have:\n";
    print(B);
    OIIO_CHECK_EQUAL(B.nativespec().nchannels, 6);
    OIIO_CHECK_EQUAL(B.spec().nchannels, 3);
    OIIO_CHECK_EQUAL(B.spec().format, TypeDesc::FLOAT);
    OIIO_CHECK_EQUAL(B.spec().channelnames[0], "B");
    OIIO_CHECK_EQUAL(B.spec().channelnames[1], "A");
    OIIO_CHECK_EQUAL(B.spec().channelnames[2], "channel4");
    for (ImageBuf::ConstIterator<float> p(B); !p.done(); ++p) {
        OIIO_CHECK_EQUAL(p[0], 0.4f);
        OIIO_CHECK_EQUAL(p[1], 0.3f);
        OIIO_CHECK_EQUAL(p[2], 0.2f);
    }
}



void
test_roi()
{
    std::cout << "Testing ROI functions for ImageSpec and ImageBuf\n";
    ROI datawin(10, 640, 20, 480, 0, 1, 0, 3);
    ROI displaywin(0, 512, 30, 100, 0, 1, 0, 3);
    ROI initroi(0, 256, 0, 300, 0, 1, 0, 3);

    // Test roi set and retrieve on an ImageSpec
    ImageSpec spec(256, 300, 3);
    OIIO_CHECK_EQUAL(spec.roi(), initroi);
    OIIO_CHECK_EQUAL(spec.roi_full(), initroi);
    spec.set_roi(datawin);
    spec.set_roi_full(displaywin);
    OIIO_CHECK_EQUAL(spec.roi(), datawin);
    OIIO_CHECK_EQUAL(spec.roi_full(), displaywin);

    // Test roi set and retrieve on an ImageSBuf
    ImageBuf buf((ImageSpec(datawin)));
    OIIO_CHECK_EQUAL(buf.roi(), datawin);
    OIIO_CHECK_EQUAL(buf.roi_full(), datawin);
    buf.set_roi_full(displaywin);
    OIIO_CHECK_EQUAL(buf.roi(), datawin);
    OIIO_CHECK_EQUAL(buf.roi_full(), displaywin);

    OIIO_CHECK_ASSERT(buf.contains_roi(datawin));
    OIIO_CHECK_ASSERT(buf.contains_roi(ROI(100, 110, 100, 110, 0, 1, 0, 2)));
    OIIO_CHECK_ASSERT(
        !buf.contains_roi(ROI(0, 640, 0, 480, 0, 1, 0, 3)));  // outside xy
    OIIO_CHECK_ASSERT(
        !buf.contains_roi(ROI(10, 640, 20, 480, 1, 2, 0, 3)));  // outside z
    OIIO_CHECK_ASSERT(
        !buf.contains_roi(ROI(10, 640, 20, 480, 0, 1, 0, 4)));  // outside ch
}



// Test what happens when we read, replace the image on disk, then read
// again.
void
test_write_over()
{
    // Write two images
    {
        ImageBuf img(ImageSpec(16, 16, 3, TypeUInt8));
        ImageBufAlgo::fill(img, { 0.0f, 1.0f, 0.0f });
        img.write("tmp-green.tif");
        Sysutil::usleep(1000000);  // make sure times are different
        ImageBufAlgo::fill(img, { 1.0f, 0.0f, 0.0f });
        img.write("tmp-red.tif");
    }

    // Read the image
    float pixel[3];
    ImageBuf A("tmp-green.tif");
    A.getpixel(4, 4, pixel);
    OIIO_CHECK_ASSERT(pixel[0] == 0 && pixel[1] == 1 && pixel[2] == 0);
    A.reset();  // make sure A isn't held open, we're about to remove it

    // Replace the green image with red, under the nose of the ImageBuf.
    Filesystem::remove("tmp-green.tif");
    Filesystem::copy("tmp-red.tif", "tmp-green.tif");

    // Read the image again -- different ImageBuf.
    // We expect it to have the new color, not have the underlying
    // ImageCache misremember the old color!
    ImageBuf B("tmp-green.tif");
    B.getpixel(4, 4, pixel);
    OIIO_CHECK_ASSERT(pixel[0] == 1 && pixel[1] == 0 && pixel[2] == 0);
    B.reset();  // make sure B isn't held open, we're about to remove it

    Filesystem::remove("tmp-green.tif");
}



static void
test_uncaught_error()
{
    ImageBuf buf;
    buf.error("Boo!");
    // buf exists scope and is destroyed without anybody retrieving the error.
}



void
test_mutable_iterator_with_imagecache()
{
    // Make 4x4 1-channel float source image, value 0.5, write it.
    char srcfilename[] = "tmp_f1.exr";
    ImageSpec fsize1(4, 4, 1, TypeFloat);
    ImageBuf src(fsize1);
    ImageBufAlgo::fill(src, 0.5f);
    src.write(srcfilename);

    ImageBuf buf(srcfilename, 0, 0, ImageCache::create());
    // Using the cache, it should look tiled
    OIIO_CHECK_EQUAL(buf.spec().tile_width, buf.spec().width);

    // Make a mutable iterator, even though it's an image file reference.
    // Merely establishing the iterator ought to read the file and make the
    // buffer writeable.
    ImageBuf::Iterator<float> it(buf);
    OIIO_CHECK_EQUAL(buf.spec().tile_width, 0);  // should look untiled
    OIIO_CHECK_ASSERT(buf.localpixels());        // should look local
    for (; !it.done(); ++it)
        it[0] = 1.0f;

    ImageCache::create()->invalidate(ustring(srcfilename));
    Filesystem::remove(srcfilename);
}



void
time_iterators()
{
    print("Timing iterator operations:\n");
    const int rez = 4096, nchans = 4;
    ImageSpec spec(rez, rez, nchans, TypeFloat);
    ImageBuf img(spec);
    ImageBufAlgo::fill(img, { 0.25f, 0.5f, 0.75f, 1.0f });

    Benchmarker bench;
    double sum = 0.0f;
    bench("Read traversal with ConstIterator", [&]() {
        sum = 0.0f;
        for (ImageBuf::ConstIterator<float> it(img); !it.done(); ++it) {
            for (int c = 0; c < nchans; ++c)
                sum += it[c];
        }
    });
    OIIO_CHECK_EQUAL(sum, 2.5 * rez * rez);
    bench("Read traversal with Iterator", [&]() {
        sum = 0.0f;
        for (ImageBuf::Iterator<float> it(img); !it.done(); ++it) {
            for (int c = 0; c < nchans; ++c)
                sum += it[c];
        }
    });
    OIIO_CHECK_EQUAL(sum, 2.5 * rez * rez);
    bench("Read traversal with pointer", [&]() {
        sum             = 0.0f;
        const float* it = (const float*)img.localpixels();
        for (int y = 0; y < rez; ++y)
            for (int x = 0; x < rez; ++x, it += 4) {
                for (int c = 0; c < nchans; ++c)
                    sum += it[c];
            }
    });
    OIIO_CHECK_EQUAL(sum, 2.5 * rez * rez);
    bench("Write traversal with Iterator", [&]() {
        ImageBuf::Iterator<float> it(img);
        for (ImageBuf::Iterator<float> it(img); !it.done(); ++it) {
            for (int c = 0; c < nchans; ++c)
                it[c] = 0.5f;
        }
    });
    bench("Write traversal with pointer", [&]() {
        float* it = (float*)img.localpixels();
        for (int y = 0; y < rez; ++y)
            for (int x = 0; x < rez; ++x, it += 4) {
                for (int c = 0; c < nchans; ++c)
                    it[c] = 0.5f;
            }
    });
}



int
main(int /*argc*/, char* /*argv*/[])
{
    // Some miscellaneous things that aren't strictly ImageBuf, but this is
    // as good a place to verify them as any.
    test_wrapmodes();
    test_is_imageio_format_name();
    test_roi();

    // Lots of tests related to ImageBuf::Iterator
    test_empty_iterator();
    iterator_read_test<ImageBuf::ConstIterator<float>>();
    iterator_read_test<ImageBuf::Iterator<float>>();

    iterator_wrap_test<ImageBuf::ConstIterator<float>>(ImageBuf::WrapBlack,
                                                       "black");
    iterator_wrap_test<ImageBuf::ConstIterator<float>>(ImageBuf::WrapClamp,
                                                       "clamp");
    iterator_wrap_test<ImageBuf::ConstIterator<float>>(ImageBuf::WrapPeriodic,
                                                       "periodic");
    iterator_wrap_test<ImageBuf::ConstIterator<float>>(ImageBuf::WrapMirror,
                                                       "mirror");
    test_mutable_iterator_with_imagecache();
    time_iterators();

    ImageBuf_test_appbuffer();
    ImageBuf_test_appbuffer_strided();
    test_open_with_config();
    test_read_channel_subset();

    test_set_get_pixels();
    time_get_pixels();

    test_write_over();

    test_uncaught_error();

    Filesystem::remove("A_imagebuf_test.tif");
    return unit_test_failures;
}
