/*
  Copyright 2012 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/


#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/unittest.h>
#include <OpenImageIO/benchmark.h>

#include <iostream>

using namespace OIIO;



inline int
test_wrap (wrap_impl wrap, int coord, int origin, int width)
{
    wrap (coord, origin, width);
    return coord;
}


void
test_wrapmodes ()
{
    const int ori = 0;
    const int w = 4;
    static int
        val[] = {-7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -10},
        cla[] = { 0,  0,  0,  0,  0,  0,  0, 0, 1, 2, 3, 3, 3, 3, 3, 3, 3},
        per[] = { 1,  2,  3,  0,  1,  2,  3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1},
        mir[] = { 1,  2,  3,  3,  2,  1,  0, 0, 1, 2, 3, 3, 2, 1, 0, 0, 1};

    for (int i = 0; val[i] > -10; ++i) {
        OIIO_CHECK_EQUAL (test_wrap (wrap_clamp, val[i], ori, w), cla[i]);
        OIIO_CHECK_EQUAL (test_wrap (wrap_periodic, val[i], ori, w), per[i]);
        OIIO_CHECK_EQUAL (test_wrap (wrap_periodic_pow2, val[i], ori, w), per[i]);
        OIIO_CHECK_EQUAL (test_wrap (wrap_mirror, val[i], ori, w), mir[i]);
    }
}



// Test iterators
template <class ITERATOR>
void iterator_read_test ()
{
    const int WIDTH = 4, HEIGHT = 4, CHANNELS = 3;
    static float buf[HEIGHT][WIDTH][CHANNELS] = {
        { {0,0,0},  {1,0,1},  {2,0,2},  {3,0,3} },
        { {0,1,4},  {1,1,5},  {2,1,6},  {3,1,7} },
        { {0,2,8},  {1,2,9},  {2,2,10}, {3,2,11} },
        { {0,3,12}, {1,3,13}, {2,3,14}, {3,3,15} }
    };
    ImageSpec spec (WIDTH, HEIGHT, CHANNELS, TypeDesc::FLOAT);
    ImageBuf A (spec, buf);

    ITERATOR p (A);
    OIIO_CHECK_EQUAL (p[0], 0.0f);
    OIIO_CHECK_EQUAL (p[1], 0.0f);
    OIIO_CHECK_EQUAL (p[2], 0.0f);

    // Explicit position
    p.pos (2, 1);
    OIIO_CHECK_EQUAL (p.x(), 2); OIIO_CHECK_EQUAL (p.y(), 1);
    OIIO_CHECK_EQUAL (p[0], 2.0f);
    OIIO_CHECK_EQUAL (p[1], 1.0f);
    OIIO_CHECK_EQUAL (p[2], 6.0f);

    // Iterate a few times
    ++p;
    OIIO_CHECK_EQUAL (p.x(), 3); OIIO_CHECK_EQUAL (p.y(), 1);
    OIIO_CHECK_EQUAL (p[0], 3.0f);
    OIIO_CHECK_EQUAL (p[1], 1.0f);
    OIIO_CHECK_EQUAL (p[2], 7.0f);
    ++p;
    OIIO_CHECK_EQUAL (p.x(), 0); OIIO_CHECK_EQUAL (p.y(), 2);
    OIIO_CHECK_EQUAL (p[0], 0.0f);
    OIIO_CHECK_EQUAL (p[1], 2.0f);
    OIIO_CHECK_EQUAL (p[2], 8.0f);

    std::cout << "iterator_read_test result:";
    int i = 0;
    for (ITERATOR p (A);  !p.done();  ++p, ++i) {
        if ((i % 4) == 0)
            std::cout << "\n    ";
        std::cout << "   " << p[0] << ' ' << p[1] << ' ' << p[2];
    }
    std::cout << "\n";
}



// Test iterators
template <class ITERATOR>
void iterator_wrap_test (ImageBuf::WrapMode wrap, std::string wrapname)
{
    const int WIDTH = 4, HEIGHT = 4, CHANNELS = 3;
    static float buf[HEIGHT][WIDTH][CHANNELS] = {
        { {0,0,0},  {1,0,1},  {2,0,2},  {3,0,3} },
        { {0,1,4},  {1,1,5},  {2,1,6},  {3,1,7} },
        { {0,2,8},  {1,2,9},  {2,2,10}, {3,2,11} },
        { {0,3,12}, {1,3,13}, {2,3,14}, {3,3,15} }
    };
    ImageSpec spec (WIDTH, HEIGHT, CHANNELS, TypeDesc::FLOAT);
    ImageBuf A (spec, buf);

    std::cout << "iterator_wrap_test " << wrapname << ":";
    int i = 0;
    int noutside = 0;
    for (ITERATOR p (A, ROI(-2, WIDTH+2, -2, HEIGHT+2), wrap);
         !p.done(); ++p, ++i) {
        if ((i % 8) == 0)
            std::cout << "\n    ";
        std::cout << "   " << p[0] << ' ' << p[1] << ' ' << p[2];
        // Check wraps
        if (! p.exists()) {
            ++noutside;
            if (wrap == ImageBuf::WrapBlack) {
                OIIO_CHECK_EQUAL (p[0], 0.0f);
                OIIO_CHECK_EQUAL (p[1], 0.0f);
                OIIO_CHECK_EQUAL (p[2], 0.0f);
            } else if (wrap == ImageBuf::WrapClamp) {
                ITERATOR q = p;
                q.pos (clamp (p.x(), 0, WIDTH-1), clamp (p.y(), 0, HEIGHT-1));
                OIIO_CHECK_EQUAL (p[0], q[0]);
                OIIO_CHECK_EQUAL (p[1], q[1]);
                OIIO_CHECK_EQUAL (p[2], q[2]);
            } else if (wrap == ImageBuf::WrapPeriodic) {
                ITERATOR q = p;
                q.pos (p.x() % WIDTH, p.y() % HEIGHT);
                OIIO_CHECK_EQUAL (p[0], q[0]);
                OIIO_CHECK_EQUAL (p[1], q[1]);
                OIIO_CHECK_EQUAL (p[2], q[2]);
            } else if (wrap == ImageBuf::WrapMirror) {
                ITERATOR q = p;
                int x = p.x(), y = p.y();
                wrap_mirror (x, 0, WIDTH);
                wrap_mirror (y, 0, HEIGHT);
                q.pos (x, y);
                OIIO_CHECK_EQUAL (p[0], q[0]);
                OIIO_CHECK_EQUAL (p[1], q[1]);
                OIIO_CHECK_EQUAL (p[2], q[2]);
            }
        }
    }
    std::cout << "\n";
    OIIO_CHECK_EQUAL (noutside, 48);  // Should be 48 wrapped pixels
}



// Tests ImageBuf construction from application buffer
void ImageBuf_test_appbuffer ()
{
    const int WIDTH = 8;
    const int HEIGHT = 8;
    const int CHANNELS = 3;
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
    ImageSpec spec (WIDTH, HEIGHT, CHANNELS, TypeDesc::FLOAT);
    ImageBuf A (spec, buf);

    // Make sure A now points to the buffer
    OIIO_CHECK_EQUAL ((void *)A.pixeladdr (0, 0, 0), (void *)buf);

    // write it
    A.write ("A_imagebuf_test.tif");

    // Read it back and make sure it matches the original
    ImageBuf B ("A_imagebuf_test.tif");
    for (int y = 0;  y < HEIGHT;  ++y)
        for (int x = 0;  x < WIDTH;  ++x)
            for (int c = 0;  c < WIDTH;  ++c)
                OIIO_CHECK_EQUAL (A.getchannel (x, y, 0, c),
                                  B.getchannel (x, y, 0, c));

    // Make sure we can write to the buffer
    float pix[CHANNELS] = { 0.0, 42.0, 0 };
    A.setpixel (3, 2, 0, pix);
    OIIO_CHECK_EQUAL (buf[2][3][1], 42.0);

    // Make sure we can copy-construct the ImageBuf and it points to the
    // same application buffer.
    ImageBuf C (A);
    OIIO_CHECK_EQUAL ((void *)A.pixeladdr(0,0,0), (void*)C.pixeladdr(0,0,0));
}



void test_open_with_config ()
{
    // N.B. This function must run after ImageBuf_test_appbuffer, which
    // writes "A.tif".
    ImageCache *ic = ImageCache::create(false);
    ImageSpec config;
    config.attribute ("oiio:DebugOpenConfig!", 1);
    ImageBuf A ("A_imagebuf_test.tif", 0, 0, ic, &config);
    OIIO_CHECK_EQUAL (A.spec().get_int_attribute("oiio:DebugOpenConfig!",0), 42);
    ic->destroy (ic);
}



void test_empty_iterator ()
{
    // Ensure that ImageBuf iterators over empty ROIs immediately appear
    // done
    ImageBuf A (ImageSpec (64, 64, 3, TypeDesc::FLOAT));
    ROI roi (10, 10, 20, 40, 0, 1);
    for (ImageBuf::Iterator<float> p (A, roi);  ! p.done();  ++p) {
        std::cout << "p is " << p.x() << ' ' << p.y() << ' ' << p.z() << "\n";
        OIIO_CHECK_ASSERT (0 && "should never execute this loop body");
    }
}



void
print (const ImageBuf &A)
{
    ASSERT (A.spec().format == TypeDesc::FLOAT);
    for (ImageBuf::ConstIterator<float> p (A);  ! p.done();  ++p) {
        std::cout << "   @" << p.x() << ',' << p.y() << "=(";
        for (int c = 0; c < A.nchannels(); ++c)
            std::cout << (c ? "," : "") << p[c];
        std::cout << ')' << (p.x() == A.xmax() ? "\n" : "");
    }
    std::cout << "\n";
}



void
test_set_get_pixels ()
{
    std::cout << "\nTesting set_pixels, get_pixels:\n";
    const int nchans = 3;
    ImageBuf A (ImageSpec (4, 4, nchans, TypeDesc::FLOAT));
    ImageBufAlgo::zero (A);
    std::cout << " Cleared:\n";
    print (A);
    float newdata[2*2*nchans] = { 1,2,3,  4,5,6,
                                  7,8,9,  10,11,12 };
    A.set_pixels (ROI(1,3,1,3), TypeDesc::FLOAT, newdata);
    std::cout << " After set:\n";
    print (A);
    float retrieved[2*2*nchans] = { 9,9,9, 9,9,9, 9,9,9, 9,9,9 };
    A.get_pixels (ROI(1, 3, 1, 3, 0, 1), TypeDesc::FLOAT, retrieved);
    OIIO_CHECK_ASSERT (0 == memcmp (retrieved, newdata, 2*2*nchans));
}



void
time_get_pixels ()
{
    std::cout << "\nTesting set_pixels, get_pixels:\n";
    Benchmarker bench;
    const int nchans = 4;
    const int xres = 2000, yres = 1000;
    ImageBuf A (ImageSpec (xres, yres, nchans, TypeDesc::FLOAT));
    ImageBufAlgo::zero (A);

    // bench.work (size_t(xres*yres*nchans));
    std::unique_ptr<float[]> fbuf (new float[xres*yres*nchans]);
    bench ("get_pixels 1Mpelx4 float[4]->float[4] ", [&](){
              A.get_pixels (A.roi(), TypeFloat, fbuf.get());
           });
    bench ("get_pixels 1Mpelx4 float[4]->float[3] ", [&](){
              ROI roi3 = A.roi();
              roi3.chend = 3;
              A.get_pixels (roi3, TypeFloat, fbuf.get());
           });

    std::unique_ptr<uint8_t[]> ucbuf (new uint8_t[xres*yres*nchans]);
    bench ("get_pixels 1Mpelx4 float[4]->uint8[4] ", [&](){
              A.get_pixels (A.roi(), TypeUInt8, ucbuf.get());
           });

    std::unique_ptr<uint16_t[]> usbuf (new uint16_t[xres*yres*nchans]);
    bench ("get_pixels 1Mpelx4 float[4]->uint16[4] ", [&](){
              A.get_pixels (A.roi(), TypeUInt8, usbuf.get());
           });
}



void
test_read_channel_subset ()
{
    std::cout << "\nTesting reading a channel subset\n";

    // FIrst, write a test image with 6 channels
    static float color6[] = { .6, .5, .4, .3, .2, .1 };
    ImageBuf A (ImageSpec (2, 2, 6, TypeDesc::FLOAT));
    ImageBufAlgo::fill (A, color6);
    A.write ("sixchans.tif");
    std::cout << " Start with image:\n";
    print (A);

    // Now read it back using the "channel range" option.
    ImageBuf B ("sixchans.tif");
    B.read (0 /*subimage*/, 0 /*mip*/, 2 /*chbegin*/, 5 /*chend*/,
            true /*force*/, TypeDesc::FLOAT);
    std::cout << " After reading channels [2,5), we have:\n";
    print (B);
    OIIO_CHECK_EQUAL (B.nativespec().nchannels, 6);
    OIIO_CHECK_EQUAL (B.spec().nchannels, 3);
    OIIO_CHECK_EQUAL (B.spec().format, TypeDesc::FLOAT);
    OIIO_CHECK_EQUAL (B.spec().channelnames[0], "B");
    OIIO_CHECK_EQUAL (B.spec().channelnames[1], "A");
    OIIO_CHECK_EQUAL (B.spec().channelnames[2], "channel4");
    for (ImageBuf::ConstIterator<float> p (B);  ! p.done();  ++p) {
        OIIO_CHECK_EQUAL (p[0], 0.4f);
        OIIO_CHECK_EQUAL (p[1], 0.3f);
        OIIO_CHECK_EQUAL (p[2], 0.2f);
    }
}



int
main (int argc, char **argv)
{
    test_wrapmodes ();

    // Lots of tests related to ImageBuf::Iterator
    test_empty_iterator ();
    iterator_read_test<ImageBuf::ConstIterator<float> > ();
    iterator_read_test<ImageBuf::Iterator<float> > ();

    iterator_wrap_test<ImageBuf::ConstIterator<float> > (ImageBuf::WrapBlack, "black");
    iterator_wrap_test<ImageBuf::ConstIterator<float> > (ImageBuf::WrapClamp, "clamp");
    iterator_wrap_test<ImageBuf::ConstIterator<float> > (ImageBuf::WrapPeriodic, "periodic");
    iterator_wrap_test<ImageBuf::ConstIterator<float> > (ImageBuf::WrapMirror, "mirror");

    ImageBuf_test_appbuffer ();
    test_open_with_config ();
    test_read_channel_subset ();

    test_set_get_pixels ();
    time_get_pixels ();

    Filesystem::remove ("A_imagebuf_test.tif");
    return unit_test_failures;
}
