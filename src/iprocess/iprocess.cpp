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
#include <iostream>
#include <iterator>
#include <vector>
#include <string>

#include <boost/tokenizer.hpp>
#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>

#include "argparse.h"
#include "imageio.h"
#include "imagebuf.h"
#include "sysutil.h"

using namespace OpenImageIO;
using namespace OpenImageIO::ImageBufAlgo;

static std::string uninitialized = "uninitialized \001 HHRU dfvAS: efjl";
//static std::string dataformatname = "";
//static float gammaval = 1.0f;
//static bool depth = false;
//static bool verbose = false;
static std::vector<std::string> filenames;
static std::string outputname;
//static int tile[3] = { 0, 0, 1 };
//static bool scanline = false;
//static bool zfile = false;
//static std::string channellist;
//static std::string compression;
//static bool no_copy_image = false;
//static int quality = -1;
//static bool adjust_time = false;
//static std::string caption = uninitialized;
//static std::vector<std::string> keywords;
//static bool clear_keywords = false;
//static std::vector<std::string> attribnames, attribvals;
//static int orientation = 0;
//static bool rotcw = false, rotccw = false, rot180 = false;
//static bool sRGB = false;
//static bool separate = false, contig = false;
static std::string crop_type;
static int crop_xmin = 0, crop_xmax = 0, crop_ymin = 0, crop_ymax = 0;
static bool do_add = false;
static ImageBuf img;



static int
parse_files (int argc, const char *argv[])
{
    for (int i = 0;  i < argc;  i++)
        filenames.push_back (argv[i]);
    return 0;
}



static void
getargs (int argc, char *argv[])
{
    bool help = false;
    ArgParse ap;
    ap.options ("Usage:  iprocess [options] inputfile... -o outputfile\n",
                "%*", parse_files, "",
                "--help", &help, "Print help message",
//                "-v", &verbose, "Verbose status messages",
                "-o %s", &outputname, "Set output filename",
                "<SEPARATOR>", "Image operations:",
                "--add", &do_add, "Add two images",
		"--crop %s %d %d %d %d", &crop_type, &crop_xmin, &crop_xmax,
                    &crop_ymin, &crop_ymax, "Crop an image (type, xmin, xmax, ymin, ymax)\n\t\t\t\ttype = black|white|trans|window|cut",
                "<SEPARATOR>", "Output options:",
//                "-d %s", &dataformatname, "Set the output data format to one of:\n"
//                        "\t\t\tuint8, sint8, uint16, sint16, half, float, double",
//                "-g %f", &gammaval, "Set gamma correction (default = 1)",
//                "--tile %d %d", &tile[0], &tile[1], "Output as a tiled image",
//                "--scanline", &scanline, "Output as a scanline image",
//                "--compression %s", &compression, "Set the compression method (default = same as input)",
//                "--quality %d", &quality, "Set the compression quality, 1-100",
//                "--no-copy-image", &no_copy_image, "Do not use ImageOutput copy_image functionality (dbg)",
//                "--adjust-time", &adjust_time, "Adjust file times to match DateTime metadata",
//                "--caption %s", &caption, "Set caption (ImageDescription)",
//                "--keyword %L", &keywords, "Add a keyword",
//                "--clear-keywords", &clear_keywords, "Clear keywords",
//                "--attrib %L %L", &attribnames, &attribvals, "Set a string attribute (name, value)",
//                "--orientation %d", &orientation, "Set the orientation",
//                "--rotcw", &rotcw, "Rotate 90 deg clockwise",
//                "--rotccw", &rotccw, "Rotate 90 deg counter-clockwise",
//                "--rot180", &rot180, "Rotate 180 deg",
//                "--sRGB", &sRGB, "This file is in sRGB color space",
//                "--separate", &separate, "Force planarconfig separate",
//                "--contig", &contig, "Force planarconfig contig",
//FIXME         "-z", &zfile, "Treat input as a depth file",
//FIXME         "-c %s", &channellist, "Restrict/shuffle channels",
                NULL);
    if (ap.parse(argc, (const char**)argv) < 0) {
	std::cerr << ap.error_message() << std::endl;
        ap.usage ();
        exit (EXIT_FAILURE);
    }
    if (help) {
        ap.usage ();
        exit (EXIT_FAILURE);
    }

    if (filenames.size() == 0) {
        std::cerr << "iprocess: Must have at least one input filename\n";
        ap.usage();
        exit (EXIT_FAILURE);
    }
    if (outputname.empty()) {
        std::cerr << "iprocess: Must have an output filename\n";
        ap.usage();
        exit (EXIT_FAILURE);
    }
//    if (((int)rotcw + (int)rotccw + (int)rot180 + (orientation>0)) > 1) {
//        std::cerr << "iprocess: more than one of --rotcw, --rotccw, --rot180, --orientation\n";
//        ap.usage();
//        exit (EXIT_FAILURE);
//    }
}



static bool
DateTime_to_time_t (const char *datetime, time_t &timet)
{
    int year, month, day, hour, min, sec;
    int r = sscanf (datetime, "%d:%d:%d %d:%d:%d",
                    &year, &month, &day, &hour, &min, &sec);
    // printf ("%d  %d:%d:%d %d:%d:%d\n", r, year, month, day, hour, min, sec);
    if (r != 6)
        return false;
    struct tm tmtime;
    time_t now;
    Sysutil::get_local_time (&now, &tmtime); // fill in defaults
    tmtime.tm_sec = sec;
    tmtime.tm_min = min;
    tmtime.tm_hour = hour;
    tmtime.tm_mday = day;
    tmtime.tm_mon = month-1;
    tmtime.tm_year = year-1900;
    timet = mktime (&tmtime);
    return true;
}



// Utility: split semicolon-separated list
static void
split_list (const std::string &list, std::vector<std::string> &items)
{
    typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
    boost::char_separator<char> sep(";");
    tokenizer tokens (list, sep);
    for (tokenizer::iterator tok_iter = tokens.begin();
         tok_iter != tokens.end(); ++tok_iter) {
        std::string t = *tok_iter;
        while (t.length() && t[0] == ' ')
            t.erase (t.begin());
        if (t.length())
            items.push_back (t);
    }
}



// Utility: join list into a single semicolon-separated string
static std::string
join_list (const std::vector<std::string> &items)
{
    std::string s;
    for (size_t i = 0;  i < items.size();  ++i) {
        if (i > 0)
            s += "; ";
        s += items[i];
    }
    return s;
}




static bool
read_input (const std::string &filename, ImageBuf &img, int subimage=0)
{
    if (img.subimage() >= 0 && img.subimage() == subimage)
        return true;

    if (img.init_spec (filename) && 
        img.read (subimage, false, TypeDesc::FLOAT))
        return true;

    std::cerr << "iprocess ERROR: Could not read " << filename << ":\n\t"
              << img.error_message() << "\n";
    return false;
}


int
main (int argc, char *argv[])
{
    getargs (argc, argv);

    bool ok = true;

    if (crop_type.size()) {
        std::cout << "Cropping " << filenames[0] << " to  " << outputname << "\n";
        if (filenames.size() != 1) {
            std::cerr << "iprocess: --crop needs one input filename\n";
            exit (EXIT_FAILURE);
        }
        ImageBuf A;
        if (! read_input (filenames[0], A)) {
            std::cerr << "iprocess: read error: " << A.error_message() << "\n";
            return EXIT_FAILURE;
        }
        ImageBuf out;
        CropOptions opt = CROP_CUT;
        if (crop_type == "white")
            opt = CROP_WHITE;
        else if (crop_type == "black")
            opt = CROP_BLACK;
        else if (crop_type == "trans")
            opt = CROP_TRANS;
        else if (crop_type == "window")
            opt = CROP_WINDOW;
        else if (crop_type == "cut")
            opt = CROP_CUT;
        else {
            std::cerr << "iprocess: crop needs a 'type' of white, black, trans, window, or cut\n";
            return EXIT_FAILURE;
        }
        crop (out, A, crop_xmin, crop_xmax+1, crop_ymin, crop_ymax+1, opt);
	std::cout << "finished cropping\n";
        out.save (outputname);
    }
    if (do_add) {
	std::cout << "Adding " << filenames[0] << " and " << filenames[1] 
		<< " result will be saved at " << outputname << "\n";
	if (filenames.size() != 2) {
	   std::cerr << "iprocess: --add needs two input filenames\n";
	   exit (EXIT_FAILURE);
	}
	ImageBuf A, B;
        if (! read_input (filenames[0], A)) {
            std::cerr << "iprocess: read error: " << A.error_message() << "\n";
            return EXIT_FAILURE;
        }
        if (! read_input (filenames[0], B)) {
            std::cerr << "iprocess: read error: " << B.error_message() << "\n";
            return EXIT_FAILURE;
        }
        ImageBuf out;
        add (out, A, B);

        out.save (outputname);
    }//do add


    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
