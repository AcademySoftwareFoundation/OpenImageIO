/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2008 Larry Gritz
// 
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
// 
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 
// (this is the MIT license)
/////////////////////////////////////////////////////////////////////////////


#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <iostream>
#include <iterator>

#include <boost/foreach.hpp>

#include "imageio.h"
using namespace OpenImageIO;


static bool verbose = false;
static bool sum = false;
static std::vector<std::string> filenames;



static void
usage (void)
{
    std::cout << 
        "Usage:  iinfo [options] filename...\n"
        "    --help                      Print this help message\n"
        "    -v [--verbose]              Verbose output\n"
        "    -s                          Sum the image sizes\n";
        ;
}



static void
getargs (int argc, char *argv[])
{
    for (int i = 1;  i < argc;  ++i) {
        if (! strcmp (argv[i], "-h") || ! strcmp (argv[i], "--help")) {
            usage();
            exit (0);
        }
        if (! strcmp (argv[i], "-v") || ! strcmp (argv[i], "--verbose")) {
            verbose = true;
            continue;
        }
        if (! strcmp (argv[i], "-s")) {
            sum = true;
            continue;
        }
        filenames.push_back (argv[i]);
    }
}



static void
print_info (const std::string &filename, ImageInput *input,
            ImageIOFormatSpec &spec,
            bool verbose, bool sum, long long &totalsize)
{
    printf ("%s : %s %s %4d x %4d", filename.c_str(), 
            ParamBaseTypeNameString(spec.format),
            spec.depth > 1 ? "volume" : "image",
            spec.width, spec.height);
    if (spec.depth > 1)
        printf (" x %4d", spec.depth);
    printf (", %d channel%s", spec.nchannels, spec.nchannels > 1 ? "s" : "");
    if (sum) {
        totalsize += spec.image_bytes();
        printf (" (%.2f MB)", (float)spec.image_bytes() / (1024.0*1024.0));
    }
    printf ("\n");

    if (verbose) {
        if (1 || spec.x || spec.y || spec.z) {
            printf ("    offset: x=%d, y=%d", spec.x, spec.y);
            if (spec.depth > 1)
                printf (", z=%d\n", spec.x, spec.y, spec.z);
            printf ("\n");
        }
        if (spec.full_width != spec.width || spec.full_height != spec.height ||
            spec.full_depth != spec.depth) {
            printf ("    full (uncropped) size: %4d x %d",
                    spec.full_width, spec.full_height);
            if (spec.depth > 1)
                printf (" x %d", spec.full_depth);
            printf ("\n");
        }
        if (spec.tile_width) {
            printf ("    tile size: %d x %d",
                    spec.tile_width, spec.tile_height);
            if (spec.depth > 1)
                printf (" x %d", spec.tile_depth);
            printf ("\n");
        }
        BOOST_FOREACH (const ImageIOParameter &p, spec.extra_params) {
            printf ("    %s: ", p.name.c_str());
            if (p.type == PT_STRING)
                printf ("\"%s\"", *(const char **)p.data());
            else if (p.type == PT_FLOAT)
                printf ("%g", *(const float *)p.data());
            else if (p.type == PT_INT)
                printf ("%d", *(const int *)p.data());
            else if (p.type == PT_UINT)
                printf ("%ud", *(const unsigned int *)p.data());
            else
                printf ("<unknown data type>");
            printf ("\n");
        }
    }
}



int
main (int argc, char *argv[])
{
    getargs (argc, argv);

    long long totalsize = 0;
    BOOST_FOREACH (const std::string &s, filenames) {
        ImageInput *in = ImageInput::create (s.c_str(), "" /* searchpath */);
        if (! in) {
            std::cerr << OpenImageIO::error_message() << "\n";
            continue;
        }
        ImageIOFormatSpec spec;
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
