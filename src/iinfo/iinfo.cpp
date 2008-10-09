/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
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


#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <iostream>
#include <iterator>

#include <boost/foreach.hpp>

#include "argparse.h"
#include "strutil.h"
#include "imageio.h"
using namespace OpenImageIO;


static bool verbose = false;
static bool sum = false;
static bool help = false;
static std::vector<std::string> filenames;



static void
print_info (const std::string &filename, ImageInput *input,
            ImageSpec &spec,
            bool verbose, bool sum, long long &totalsize)
{
    printf ("%s : %4d x %4d", filename.c_str(), 
            spec.width, spec.height);
    if (spec.depth > 1)
        printf (" x %4d", spec.depth);
    printf (", %d channel, %s%s", spec.nchannels,
            spec.format.c_str(),
            spec.depth > 1 ? " volume" : "");
    if (sum) {
        totalsize += spec.image_bytes();
        printf (" (%.2f MB)", (float)spec.image_bytes() / (1024.0*1024.0));
    }
    printf ("\n");

    if (verbose) {
        printf ("    channel list: ");
        for (int i = 0;  i < spec.nchannels;  ++i) {
            printf ("%s%s", spec.channelnames[i].c_str(),
                    (i == spec.nchannels-1) ? "" : ", ");
        }
        printf ("\n");
        if (spec.x || spec.y || spec.z) {
            printf ("    pixel data origin: x=%d, y=%d", spec.x, spec.y);
            if (spec.depth > 1)
                printf (", z=%d\n", spec.x, spec.y, spec.z);
            printf ("\n");
        }
        if (spec.full_x || spec.full_y || spec.full_z ||
            (spec.full_width != spec.width && spec.full_width != 0) || 
            (spec.full_height != spec.height && spec.full_height != 0) ||
            (spec.full_depth != spec.depth && spec.full_depth != 0)) {
            printf ("    full/display size: %d x %d",
                    spec.full_width, spec.full_height);
            if (spec.depth > 1)
                printf (" x %d", spec.full_depth);
            printf ("\n");
            printf ("    full/display origin: %d, %d",
                    spec.full_x, spec.full_y);
            if (spec.depth > 1)
                printf (", %d", spec.full_z);
            printf ("\n");
        }
        if (spec.tile_width) {
            printf ("    tile size: %d x %d",
                    spec.tile_width, spec.tile_height);
            if (spec.depth > 1)
                printf (" x %d", spec.tile_depth);
            printf ("\n");
        }
        const char *cspacename [] = { "unknown", "linear", "gamma %g", "sRGB" };
        printf ("    Color space: %s\n",
                Strutil::format(cspacename[(int)spec.linearity], spec.gamma).c_str());
        BOOST_FOREACH (const ImageIOParameter &p, spec.extra_attribs) {
            printf ("    %s: ", p.name().c_str());
            if (p.type() == TypeDesc::STRING)
                printf ("\"%s\"", *(const char **)p.data());
            else if (p.type() == TypeDesc::FLOAT)
                printf ("%g", *(const float *)p.data());
            else if (p.type() == TypeDesc::DOUBLE)
                printf ("%g", *(const float *)p.data());
            else if (p.type() == TypeDesc::INT)
                printf ("%d", *(const int *)p.data());
            else if (p.type() == TypeDesc::UINT)
                printf ("%d", *(const unsigned int *)p.data());
            else if (p.type() == TypeDesc::UINT16)
                printf ("%u", *(const unsigned short *)p.data());
            else if (p.type() == TypeDesc::INT16)
                printf ("%d", *(const short *)p.data());
            else
                printf ("<unknown data type>");
            printf ("\n");
        }
    }
}



static int
parse_files (int argc, const char *argv[])
{
    for (int i = 0;  i < argc;  i++)
        filenames.push_back (argv[i]);
    return 0;
}



int
main (int argc, const char *argv[])
{
    ArgParse ap (argc, argv);
    if (ap.parse ("Usage:  iinfo [options] filename...",
                  "%*", parse_files, "",
                  "--help", &help, "Print help message",
                  "-v", &verbose, "Verbose output",
                  "-s", &sum, "Sum the image sizes",
                  NULL) < 0) {
        std::cerr << ap.error_message() << std::endl;
        ap.usage ();
        return EXIT_FAILURE;
    }
    if (help) {
        ap.usage ();
        exit (EXIT_FAILURE);
    }

    long long totalsize = 0;
    BOOST_FOREACH (const std::string &s, filenames) {
        ImageInput *in = ImageInput::create (s.c_str(), "" /* searchpath */);
        if (! in) {
            std::cerr << OpenImageIO::error_message() << "\n";
            continue;
        }
        ImageSpec spec;
        if (in->open (s.c_str(), spec)) {
            print_info (s, in, spec, verbose, sum, totalsize);
            in->close ();
        } else {
            fprintf (stderr, "iinfo: Could not open \"%s\" : %s\n",
                     s.c_str(), in->error_message().c_str());
        }
        delete in;
    }

    if (sum) {
        double t = (double)totalsize / (1024.0*1024.0);
        if (t > 1024.0)
            printf ("Total size: %.2f GB\n", t/1024.0);
        else 
            printf ("Total size: %.2f MB\n", t);
    }

    return 0;
}
