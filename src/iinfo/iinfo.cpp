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
#include <boost/regex.hpp>

#include "argparse.h"
#include "strutil.h"
#include "imageio.h"
using namespace OpenImageIO;


static bool verbose = false;
static bool sum = false;
static bool help = false;
static std::vector<std::string> filenames;
static std::string metamatch;
static bool filenameprefix = false;
static boost::regex field_re;
static bool subimages = false;



// prints basic info (resolution, width, height, depth, channels, data format,
// and format name) about given subimage.
static void
print_info_subimage (int current, int max_subimages, ImageSpec &spec,
                      ImageInput *input)
{
    if ( ! input->seek_subimage (current, spec) )
        return;

    if (subimages && max_subimages != 1 && (metamatch.empty() ||
          boost::regex_search ("resolution, width, height, depth, channels",
                                 field_re))) {
        printf (" subimage %2d: ", current);
        printf ("%4d x %4d", spec.width, spec.height);
        if (spec.depth > 1)
            printf (" x %4d", spec.depth);
        printf (", %d channel, %s%s", spec.nchannels, spec.format.c_str(),
                 spec.depth > 1 ? " volume" : "");
        printf (" %s\n", input->format_name());
    }
}



static void
print_info (const std::string &filename, size_t namefieldlength, 
            ImageInput *input, ImageSpec &spec,
            bool verbose, bool sum, long long &totalsize)
{
    bool printed = false;
    int padlen = std::max (0, (int)namefieldlength - (int)filename.length());
    std::string padding (padlen, ' ');

    // checking how many subimages are stored in the file
    int num_of_subimages = 1;
    if ( input->seek_subimage (1, spec)) {
        // mayby we should do this more gently?
        while (input->seek_subimage (num_of_subimages, spec))
            ++num_of_subimages;
        input->seek_subimage (0, spec);
    }
    if (metamatch.empty() ||
        boost::regex_search ("resolution, width, height, depth, channels", field_re)) {
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
        // we prints info about how many subimages are stored in file
        // only when we have more then one subimage
        if ( ! verbose && num_of_subimages != 1)
            printf (" (%d subimages)", num_of_subimages);
        printf ("\n");
        // we print basic info about subimages when only the option '-a'
        // was used and the file store more then one subimage
        if (subimages && ! verbose && num_of_subimages != 1) {
            for (int i = 0; i < num_of_subimages; ++i) 
                print_info_subimage (i, num_of_subimages, spec, input);
        }
        printed = true;
    }

    if (verbose) {
        // info about num of subimages and their resolutions
        if (num_of_subimages != 1) {
            printf ("    %d subimages: ", num_of_subimages);
            for (int i = 0; i < num_of_subimages; ++i) {
                input->seek_subimage (i, spec);
                printf ("%dx%d ", spec.width, spec.height);
            }
            printf ("\n");
        }
        // if the '-a' flag is not set we print info
        // about first subimage only
        if ( ! subimages)
            num_of_subimages = 1;
        for (int i = 0; i < num_of_subimages; ++i) {
            print_info_subimage (i, num_of_subimages, spec, input);
            if (metamatch.empty() ||
                    boost::regex_search ("channels", field_re) ||
                    boost::regex_search ("channel list", field_re)) {
                if (filenameprefix)
                    printf ("%s : ", filename.c_str());
                printf ("    channel list: ");
                for (int i = 0;  i < spec.nchannels;  ++i) {
                    printf ("%s%s", spec.channelnames[i].c_str(),
                            (i == spec.nchannels-1) ? "" : ", ");
                }
                printf ("\n");
                printed = true;
            }
            if (spec.x || spec.y || spec.z) {
                if (metamatch.empty() ||
                        boost::regex_search ("pixel data origin", field_re)) {
                    if (filenameprefix)
                        printf ("%s : ", filename.c_str());
                    printf ("    pixel data origin: x=%d, y=%d", spec.x, spec.y);
                    if (spec.depth > 1)
                        printf (", z=%d", spec.z);
                    printf ("\n");
                    printed = true;
                }
            }
            if (spec.full_x || spec.full_y || spec.full_z ||
                (spec.full_width != spec.width && spec.full_width != 0) || 
                (spec.full_height != spec.height && spec.full_height != 0) ||
                (spec.full_depth != spec.depth && spec.full_depth != 0)) {
                if (metamatch.empty() ||
                        boost::regex_search ("full/display size", field_re)) {
                    if (filenameprefix)
                        printf ("%s : ", filename.c_str());
                    printf ("    full/display size: %d x %d",
                            spec.full_width, spec.full_height);
                    if (spec.depth > 1)
                        printf (" x %d", spec.full_depth);
                    printf ("\n");
                    printed = true;
                }
                if (metamatch.empty() ||
                       boost::regex_search ("full/display origin", field_re)) {
                    if (filenameprefix)
                        printf ("%s : ", filename.c_str());
                    printf ("    full/display origin: %d, %d",
                            spec.full_x, spec.full_y);
                    if (spec.depth > 1)
                        printf (", %d", spec.full_z);
                    printf ("\n");
                    printed = true;
                }
            }
            if (spec.tile_width) {
                if (metamatch.empty() ||
                        boost::regex_search ("tile", field_re)) {
                    if (filenameprefix)
                        printf ("%s : ", filename.c_str());
                    printf ("    tile size: %d x %d",
                            spec.tile_width, spec.tile_height);
                    if (spec.depth > 1)
                        printf (" x %d", spec.tile_depth);
                    printf ("\n");
                    printed = true;
                }
            }
            if (metamatch.empty() ||
                    boost::regex_search ("Color space", field_re)) {
                if (filenameprefix)
                    printf ("%s : ", filename.c_str());
                const char *cspacename [] = { "unknown", "linear", "gamma %g", "sRGB" };
                printf ("    Color space: %s\n",
                        Strutil::format(cspacename[(int)spec.linearity], spec.gamma).c_str());
                printed = true;
            }

            BOOST_FOREACH (const ImageIOParameter &p, spec.extra_attribs) {
                if (! metamatch.empty() &&
                    ! boost::regex_search (p.name().c_str(), field_re))
                    continue;
                std::string s = spec.metadata_val (p, true);
                if (filenameprefix)
                    printf ("%s : ", filename.c_str());
                printf ("    %s: %s\n", p.name().c_str(), s.c_str());
                printed = true;
            }

            if (! printed && !metamatch.empty()) {
                if (filenameprefix)
                    printf ("%s : ", filename.c_str());
                printf ("    %s: <unknown>\n", metamatch.c_str());
            }
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
                "-m %s", &metamatch, "Metadata names to print (default: all)",
                "-f", &filenameprefix, "Prefix each line with the filename",
                "-s", &sum, "Sum the image sizes",
                "-a", &subimages, "Print info about all subimages",
                NULL);
    if (ap.parse(argc, argv) < 0 || filenames.empty()) {
        std::cerr << ap.error_message() << std::endl;
        ap.usage ();
        return EXIT_FAILURE;
    }
    if (help) {
        ap.usage ();
        exit (EXIT_FAILURE);
    }

    if (! metamatch.empty())
        field_re.assign (metamatch,
                         boost::regex::extended | boost::regex_constants::icase);

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
