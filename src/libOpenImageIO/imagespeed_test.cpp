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


#include "imageio.h"
#include "imagebuf.h"
#include "imagebufalgo.h"
#include "sysutil.h"
#include "argparse.h"
#include "ustring.h"
#include "strutil.h"
#include "timer.h"
#include "unittest.h"

#include <iostream>
#include <vector>

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

OIIO_NAMESPACE_USING;

static bool verbose = false;
static int iterations = 1;
static int ntrials = 1;
static int numthreads = 0;
static int autotile_size = 64;
static bool iter_only = false;
static std::vector<ustring> input_filename;
static std::vector<char> buffer;
static ImageCache *imagecache = NULL;
static imagesize_t total_image_pixels = 0;



static int
parse_files (int argc, const char *argv[])
{
    input_filename.push_back (ustring(argv[0]));
    return 0;
}



static void
getargs (int argc, char *argv[])
{
    bool help = false;
    ArgParse ap;
    ap.options ("imagespeed_test\n"
                OIIO_INTRO_STRING "\n"
                "Usage:  imagespeed_test [options] filename...",
                "%*", parse_files, "",
                "--help", &help, "Print help message",
                "-v", &verbose, "Verbose mode",
                "--threads %d", &numthreads, 
                    ustring::format("Number of threads (default: %d)", numthreads).c_str(),
                "--iters %d", &iterations,
                    ustring::format("Number of iterations (default: %d)", iterations).c_str(),
                "--trials %d", &ntrials, "Number of trials",
                "--autotile %d", &autotile_size, 
                    ustring::format("Autotile size (when used; default: %d)", autotile_size).c_str(),
                "--iteronly", &iter_only, "Run iteration tests only (not read tests)",
                NULL);
    if (ap.parse (argc, (const char**)argv) < 0) {
        std::cerr << ap.geterror() << std::endl;
        ap.usage ();
        exit (EXIT_FAILURE);
    }
    if (help) {
        ap.usage ();
        exit (EXIT_FAILURE);
    }
}



static void
time_read_image ()
{
    BOOST_FOREACH (ustring filename, input_filename) {
        ImageInput *in = ImageInput::open (filename.c_str());
        ASSERT (in);
        in->read_image (TypeDesc::TypeFloat, &buffer[0]);
        in->close ();
        delete in;
    }
}



static void
time_read_scanline_at_a_time ()
{
    BOOST_FOREACH (ustring filename, input_filename) {
        ImageInput *in = ImageInput::open (filename.c_str());
        ASSERT (in);
        const ImageSpec &spec (in->spec());
        size_t pixelsize = spec.nchannels * sizeof(float);
        imagesize_t scanlinesize = spec.width * pixelsize;
        for (int y = 0; y < spec.height;  ++y) {
            in->read_scanline (y+spec.y, 0, TypeDesc::TypeFloat,
                               &buffer[scanlinesize*y]);
        }
        in->close ();
        delete in;
    }
}



static void
time_read_64_scanlines_at_a_time ()
{
    BOOST_FOREACH (ustring filename, input_filename) {
        ImageInput *in = ImageInput::open (filename.c_str());
        ASSERT (in);
        const ImageSpec &spec (in->spec());
        size_t pixelsize = spec.nchannels * sizeof(float);
        imagesize_t scanlinesize = spec.width * pixelsize;
        for (int y = 0; y < spec.height;  y += 64) {
            in->read_scanlines (y+spec.y, std::min(y+spec.y+64, spec.y+spec.height),
                                0, TypeDesc::TypeFloat, &buffer[scanlinesize*y]);
        }
        in->close ();
        delete in;
    }
}



static void
time_read_imagebuf ()
{
    imagecache->invalidate_all (true);
    BOOST_FOREACH (ustring filename, input_filename) {
        ImageBuf ib (filename.string(), imagecache);
        ib.read (0, 0, true, TypeDesc::TypeFloat);
    }
}



static void
time_ic_get_pixels ()
{
    imagecache->invalidate_all (true);
    BOOST_FOREACH (ustring filename, input_filename) {
        const ImageSpec spec = (*imagecache->imagespec (filename));
        imagecache->get_pixels (filename, 0, 0, spec.x, spec.x+spec.width,
                                spec.y, spec.y+spec.height,
                                spec.z, spec.z+spec.depth,
                                TypeDesc::TypeFloat, &buffer[0]);
    }
}



static void
test_read (const std::string &explanation,
           void (*func)(), int autotile=64, int autoscanline=1)
{
    imagecache->invalidate_all (true);  // Don't hold anything
    imagecache->attribute ("autotile", autotile);
    imagecache->attribute ("autoscanline", autoscanline);
    double t = time_trial (func, ntrials);
    double rate = double(total_image_pixels) / t;
    std::cout << "  " << explanation << ": "
              << Strutil::timeintervalformat(t,2) 
              << " = " << Strutil::format("%5.1f",rate/1.0e6) << " Mpel/s\n";
}



static float
time_loop_pixels_1D (ImageBuf &ib, int iters)
{
    ASSERT (ib.localpixels() && ib.pixeltype() == TypeDesc::TypeFloat);
    const ImageSpec &spec (ib.spec());
    imagesize_t npixels = spec.image_pixels();
    int nchannels = spec.nchannels;
    double sum = 0.0f;
    for (int i = 0;  i < iters;  ++i) {
        const float *f = (const float *) ib.pixeladdr (spec.x, spec.y, spec.z);
        ASSERT (f);
        for (imagesize_t p = 0;  p < npixels;  ++p) {
            sum += f[0];
            f += nchannels;
        }
    }
    // std::cout << float(sum/npixels/iters) << "\n";
    return float(sum/npixels/iters);
}



static float
time_loop_pixels_3D (ImageBuf &ib, int iters)
{
    ASSERT (ib.localpixels() && ib.pixeltype() == TypeDesc::TypeFloat);
    const ImageSpec &spec (ib.spec());
    imagesize_t npixels = spec.image_pixels();
    int nchannels = spec.nchannels;
    double sum = 0.0f;
    for (int i = 0;  i < iters;  ++i) {
        const float *f = (const float *) ib.pixeladdr (spec.x, spec.y, spec.z);
        ASSERT (f);
        for (int z = spec.z, ze = spec.z+spec.depth; z < ze; ++z) {
            for (int y = spec.y, ye = spec.y+spec.height; y < ye; ++y) {
                for (int x = spec.x, xe = spec.x+spec.width; x < xe; ++x) {
                    sum += f[0];
                    f += nchannels;
                }
            }
        }
    }
    // std::cout << float(sum/npixels/iters) << "\n";
    return float(sum/npixels/iters);
}



static float
time_loop_pixels_3D_getchannel (ImageBuf &ib, int iters)
{
    ASSERT (ib.pixeltype() == TypeDesc::TypeFloat);
    const ImageSpec &spec (ib.spec());
    imagesize_t npixels = spec.image_pixels();
    double sum = 0.0f;
    for (int i = 0;  i < iters;  ++i) {
        for (int z = spec.z, ze = spec.z+spec.depth; z < ze; ++z) {
            for (int y = spec.y, ye = spec.y+spec.height; y < ye; ++y) {
                for (int x = spec.x, xe = spec.x+spec.width; x < xe; ++x) {
                    sum += ib.getchannel (x, y, 0);
                }
            }
        }
    }
    // std::cout << float(sum/npixels/iters) << "\n";
    return float(sum/npixels/iters);
}



static float
time_iterate_pixels (ImageBuf &ib, int iters)
{
    ASSERT (ib.pixeltype() == TypeDesc::TypeFloat);
    const ImageSpec &spec (ib.spec());
    imagesize_t npixels = spec.image_pixels();
    double sum = 0.0f;
    for (int i = 0;  i < iters;  ++i) {
        for (ImageBuf::ConstIterator<float,float> p (ib);  !p.done();  ++p) {
            sum += p[0];
        }
    }
    // std::cout << float(sum/npixels/iters) << "\n";
    return float(sum/npixels/iters);
}



static float
time_iterate_pixels_slave_pos (ImageBuf &ib, int iters)
{
    ASSERT (ib.pixeltype() == TypeDesc::TypeFloat);
    const ImageSpec &spec (ib.spec());
    imagesize_t npixels = spec.image_pixels();
    double sum = 0.0f;
    for (int i = 0;  i < iters;  ++i) {
        ImageBuf::ConstIterator<float,float> slave (ib);
        for (ImageBuf::ConstIterator<float,float> p (ib);  !p.done();  ++p) {
            slave.pos (p.x(), p.y());
            sum += p[0];
        }
    }
    // std::cout << float(sum/npixels/iters) << "\n";
    return float(sum/npixels/iters);
}



static float
time_iterate_pixels_slave_incr (ImageBuf &ib, int iters)
{
    ASSERT (ib.pixeltype() == TypeDesc::TypeFloat);
    const ImageSpec &spec (ib.spec());
    imagesize_t npixels = spec.image_pixels();
    double sum = 0.0f;
    for (int i = 0;  i < iters;  ++i) {
        ImageBuf::ConstIterator<float,float> slave (ib);
        for (ImageBuf::ConstIterator<float,float> p (ib);  !p.done();  ++p) {
            sum += p[0];
            ++slave;
        }
    }
    // std::cout << float(sum/npixels/iters) << "\n";
    return float(sum/npixels/iters);
}



static void
test_pixel_iteration (const std::string &explanation,
                      float (*func)(ImageBuf&,int),
                      bool preload, int iters=100, int autotile=64)
{
    imagecache->invalidate_all (true);  // Don't hold anything
    // Force the whole image to be read at once
    imagecache->attribute ("autotile", autotile);
    imagecache->attribute ("autoscanline", 1);
    ImageBuf ib (input_filename[0].string(), imagecache);
    ib.read (0, 0, preload, TypeDesc::TypeFloat);
    double t = time_trial (boost::bind(func,boost::ref(ib),iters), ntrials);
    double rate = double(ib.spec().image_pixels()) / (t/iters);
    std::cout << "  " << explanation << ": "
              << Strutil::timeintervalformat(t/iters,3) 
              << " = " << Strutil::format("%5.1f",rate/1.0e6) << " Mpel/s\n";
}



int
main (int argc, char **argv)
{
    getargs (argc, argv);
    if (input_filename.size() == 0) {
        std::cout << "Error: Must supply a filename.\n";
        return -1;
    }

    OIIO::attribute ("threads", numthreads);

    imagecache = ImageCache::create ();
    imagecache->attribute ("forcefloat", 1);

    // Allocate a buffer big enough (for floats)
    bool all_scanline = true;
    total_image_pixels = 0;
    imagesize_t maxpelchans = 0;
    for (size_t i = 0;  i < input_filename.size();  ++i) {
        ImageSpec spec;
        if (! imagecache->get_imagespec (input_filename[i], spec, 0, 0, true)) {
            std::cout << "File \"" << input_filename[i] << "\" could not be opened.\n";
            return -1;
        }
        total_image_pixels += spec.image_pixels();
        maxpelchans = std::max (maxpelchans, spec.image_pixels()*spec.nchannels);
        all_scanline &= (spec.tile_width == 0);
    }
    imagecache->invalidate_all (true);  // Don't hold anything

    if (! iter_only) {
        std::cout << "Timing various ways of reading images:\n";
        buffer.resize (maxpelchans*sizeof(float), 0);
        test_read ("read_image                                   ",
                   time_read_image, 0, 0);
        if (all_scanline) {
            test_read ("read_scanline (1 at a time)                  ",
                       time_read_scanline_at_a_time, 0, 0);
            test_read ("read_scanlines (64 at a time)                ",
                       time_read_64_scanlines_at_a_time, 0, 0);
        }
        test_read ("ImageBuf read                                ",
                   time_read_imagebuf, 0, 0);
        test_read ("ImageCache get_pixels                        ",
                   time_ic_get_pixels, 0, 0);
        test_read ("ImageBuf read (autotile)                     ",
                   time_read_imagebuf, autotile_size, 0);
        test_read ("ImageCache get_pixels (autotile)             ",
                   time_ic_get_pixels, autotile_size, 0);
        if (all_scanline) {  // don't bother for tiled images
            test_read ("ImageBuf read (autotile+autoscanline)        ",
                       time_read_imagebuf, autotile_size, 1);
            test_read ("ImageCache get_pixels (autotile+autoscanline)",
                       time_ic_get_pixels, autotile_size, 1);
        }
        if (verbose)
            std::cout << "\n" << imagecache->getstats(2) << "\n";
        std::cout << "\n";
    }

    const int iters = 64;
    std::cout << "Timing ways of iterating over an image:\n";

    test_pixel_iteration ("Loop pointers on loaded image (\"1D\")    ",
                          time_loop_pixels_1D, true, iters);
    test_pixel_iteration ("Loop pointers on loaded image (\"3D\")    ",
                          time_loop_pixels_3D, true, iters);
    test_pixel_iteration ("Loop + getchannel on loaded image (\"3D\")",
                          time_loop_pixels_3D_getchannel, true, iters/32);
    test_pixel_iteration ("Loop + getchannel on cached image (\"3D\")",
                          time_loop_pixels_3D_getchannel, false, iters/32);
    test_pixel_iteration ("Iterate over a loaded image             ",
                          time_iterate_pixels, true, iters);
    test_pixel_iteration ("Iterate over a cache image              ",
                          time_iterate_pixels, false, iters);
    test_pixel_iteration ("Iterate over a loaded image (pos slave)",
                          time_iterate_pixels_slave_pos, true, iters);
    test_pixel_iteration ("Iterate over a cache image (pos slave) ",
                          time_iterate_pixels_slave_pos, false, iters);
    test_pixel_iteration ("Iterate over a loaded image (incr slave)",
                          time_iterate_pixels_slave_incr, true, iters);
    test_pixel_iteration ("Iterate over a cache image (incr slave) ",
                          time_iterate_pixels_slave_incr, false, iters);

    if (verbose)
        std::cout << "\n" << imagecache->getstats(2) << "\n";

    ImageCache::destroy (imagecache);
    return unit_test_failures;
}
