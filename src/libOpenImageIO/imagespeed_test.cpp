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

OIIO_NAMESPACE_USING;

static bool verbose = false;
static int iterations = 1;
static int ntrials = 1;
static ustring input_filename;
static std::vector<char> buffer;
static ImageCache *imagecache = NULL;
static ImageSpec spec;



static int
parse_files (int argc, const char *argv[])
{
    input_filename = ustring(argv[0]);
    return 0;
}



static void
getargs (int argc, char *argv[])
{
    bool help = false;
    ArgParse ap;
    ap.options ("imagespeed_test\n"
                OIIO_INTRO_STRING "\n"
                "Usage:  imagespeed_test [options]",
                "%*", parse_files, "",
                "--help", &help, "Print help message",
                "-v", &verbose, "Verbose mode",
//                "--threads %d", &numthreads, 
//                    ustring::format("Number of threads (default: %d)", numthreads).c_str(),
                "--iters %d", &iterations,
                    ustring::format("Number of iterations (default: %d)", iterations).c_str(),
                "--trials %d", &ntrials, "Number of trials",
//                "--wedge", &wedge, "Do a wedge test",
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
    ImageInput *in = ImageInput::open (input_filename.c_str());
    ASSERT (in);
    in->read_image (TypeDesc::TypeFloat, &buffer[0]);
    in->close ();
    delete in;
}



static void
time_read_imagebuf ()
{
    ImageBuf ib (input_filename.string(), imagecache);
    ib.read (0, 0, true, TypeDesc::TypeFloat);
    imagecache->invalidate_all (true);
}



static void
time_ic_get_pixels ()
{
    imagecache->get_pixels (input_filename, 0, 0, spec.x, spec.x+spec.width,
                            spec.y, spec.y+spec.height,
                            spec.z, spec.z+spec.depth,
                            TypeDesc::TypeFloat, &buffer[0]);
    imagecache->invalidate_all (true);
}



int
main (int argc, char **argv)
{
    getargs (argc, argv);

    if (input_filename.empty()) {
        std::cout << "Error: Must supply a filename.\n";
        return -1;
    }

    imagecache = ImageCache::create ();
    imagecache->attribute ("forcefloat", 1);

    // Allocate a buffer big enough (for floats)
    bool ok = imagecache->get_imagespec (input_filename, spec);
    ASSERT (ok);
    imagecache->invalidate_all (true);  // Don't hold anything
    buffer.resize (spec.image_pixels()*spec.nchannels*sizeof(float), 0);

    {
        double t = time_trial (time_read_image, ntrials);
        std::cout << "image_read speed: " << Strutil::timeintervalformat(t,2) << "\n";
    }

    {
        double t = time_trial (time_read_imagebuf, ntrials);
        std::cout << "ImageBuf read speed: " << Strutil::timeintervalformat(t,2) << "\n";
    }

    {
        double t = time_trial (time_ic_get_pixels, ntrials);
        std::cout << "ImageCache get_pixels speed: " << Strutil::timeintervalformat(t,2) << "\n";
    }

    imagecache->invalidate_all (true);  // Don't hold anything

    ImageCache::destroy (imagecache);
    return unit_test_failures;
}
