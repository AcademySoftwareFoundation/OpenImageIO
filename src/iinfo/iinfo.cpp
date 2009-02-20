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
print_info (const std::string &filename, size_t namefieldlength, 
            ImageInput *input, ImageSpec &spec,
            bool verbose, bool sum, long long &totalsize)
{
    int padlen = std::max (0, (int)namefieldlength - (int)filename.length());
    std::string padding (padlen, ' ');
    printf ("%s%s : %4d x %4d", filename.c_str(), padding.c_str(),
            spec.width, spec.height);
    if (spec.depth > 1)
        printf (" x %4d", spec.depth);
    printf (", %d channel, %s%s", spec.nchannels,
            spec.format.c_str(),
            spec.depth > 1 ? " volume" : "");
    printf (" %s", input->format_name());
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
            TypeDesc element = p.type().elementtype();
            int n = p.type().numelements() * p.nvalues();
            if (element == TypeDesc::STRING) {
                for (int i = 0;  i < n;  ++i)
                    printf ("\"%s\"", ((const char **)p.data())[i]);
            } else if (element == TypeDesc::FLOAT) {
                for (int i = 0;  i < n;  ++i)
                    printf ("%s%g", (i ? ", " : ""), ((const float *)p.data())[i]);
            } else if (element == TypeDesc::DOUBLE) {
                for (int i = 0;  i < n;  ++i)
                    printf ("%s%g", (i ? ", " : ""), ((const double *)p.data())[i]);
            } else if (element == TypeDesc::INT) {
                for (int i = 0;  i < n;  ++i)
                    printf ("%s%d", (i ? ", " : ""), ((const int *)p.data())[i]);
            } else if (element == TypeDesc::UINT) {
                for (int i = 0;  i < n;  ++i)
                    printf ("%s%d", (i ? ", " : ""), ((const unsigned int *)p.data())[i]);
            } else if (element == TypeDesc::UINT16) {
                for (int i = 0;  i < n;  ++i)
                    printf ("%s%u", (i ? ", " : ""), ((const unsigned short *)p.data())[i]);
            } else if (element == TypeDesc::INT16) {
                for (int i = 0;  i < n;  ++i)
                    printf ("%s%d", (i ? ", " : ""), ((const short *)p.data())[i]);
            } else if (element == TypeDesc::TypeMatrix) {
                const float *m = (const float *)p.data();
                for (int i = 0;  i < n;  ++i, m += 16)
                    printf ("%g %g %g %g %g %g %g %g %g %g %g %g %g %g %g %g ",
                            m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], 
                            m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]);
            }
            else {
                printf ("<unknown data type> (base %d, agg %d vec %d)",
                        p.type().basetype, p.type().aggregate,
                        p.type().vecsemantics);
            }
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
    ArgParse ap;
    ap.options ("Usage:  iinfo [options] filename...",
                "%*", parse_files, "",
                "--help", &help, "Print help message",
                "-v", &verbose, "Verbose output",
                "-s", &sum, "Sum the image sizes",
                NULL);
    if (ap.parse(argc, argv) < 0) {
        std::cerr << ap.error_message() << std::endl;
        ap.usage ();
        return EXIT_FAILURE;
    }
    if (help) {
        ap.usage ();
        exit (EXIT_FAILURE);
    }

    // Find the longest filename
    size_t longestname = 0;
    BOOST_FOREACH (const std::string &s, filenames)
        longestname = std::max (longestname, s.length());
    longestname = std::min (longestname, (size_t)40);

    long long totalsize = 0;
    BOOST_FOREACH (const std::string &s, filenames) {
        ImageInput *in = ImageInput::create (s.c_str(), "" /* searchpath */);
        if (! in) {
            std::cerr << OpenImageIO::error_message() << "\n";
            continue;
        }
        ImageSpec spec;
        if (in->open (s.c_str(), spec)) {
            print_info (s, longestname, in, spec, verbose, sum, totalsize);
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
