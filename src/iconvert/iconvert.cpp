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
#include <utime.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <boost/tokenizer.hpp>

#include "argparse.h"
#include "imageio.h"
using namespace OpenImageIO;


static std::string uninitialized = "uninitialized \001 HHRU dfvAS: efjl";
static std::string dataformatname = "";
static float gammaval = 1.0f;
static bool depth = false;
static bool verbose = false;
static std::vector<std::string> filenames;
static int tile[3] = { 0, 0, 1 };
static bool scanline = false;
static bool zfile = false;
static std::string channellist;
static std::string compression;
static bool no_copy_image = false;
static int quality = -1;
static bool adjust_time = false;
static std::string caption = uninitialized;
static std::string keywords = uninitialized;
static bool clear_keywords = false;



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
    ArgParse ap (argc, (const char **)argv);
    if (ap.parse ("Usage:  iconvert [options] inputfile outputfile",
                  "%*", parse_files, "",
                  "--help", &help, "Print help message",
                  "-v", &verbose, "Verbose status messages",
                  "-d %s", &dataformatname, "Set the output data format to one of:\n"
                          "\t\t\tuint8, sint8, uint16, sint16, half, float, double",
                  "-g %f", &gammaval, "Set gamma correction (default = 1)",
                  "--tile %d %d", &tile[0], &tile[1], "Output as a tiled image",
                  "--scanline", &scanline, "Output as a scanline image",
                  "--compression %s", &compression, "Set the compression method (default = same as input)",
                  "--quality %d", &quality, "Set the compression quality, 1-100",
                  "--no-copy-image", &no_copy_image, "Do not use ImageOutput copy_image functionality (dbg)",
                  "--adjust-time", &adjust_time, "Adjust file times to match DateTime metadata",
                  "--caption %s", &caption, "Set caption (ImageDescription)",
                  "--keyword %s", &keywords, "Add a keyword",
                  "--clear-keywords", &clear_keywords, "Clear keywords",
//FIXME           "-z", &zfile, "Treat input as a depth file",
//FIXME           "-c %s", &channellist, "Restrict/shuffle channels",
                  NULL) < 0) {
        std::cerr << ap.error_message() << std::endl;
        ap.usage ();
        exit (EXIT_FAILURE);
    }
    if (help) {
        ap.usage ();
        exit (EXIT_FAILURE);
    }

    if (filenames.size() != 2) {
        std::cerr << "iconvert: Must have both an input and output filename specified.\n";
        ap.usage();
        exit (EXIT_FAILURE);
    }
    std::cout << "Converting " << filenames[0] << " to " << filenames[1] << "\n";
}



static bool
DateTime_to_time_t (const char *datetime, struct timeval times[2])
{
    int year, month, day, hour, min, sec;
    int r = sscanf (datetime, "%d:%d:%d %d:%d:%d",
                    &year, &month, &day, &hour, &min, &sec);
    // printf ("%d  %d:%d:%d %d:%d:%d\n", r, year, month, day, hour, min, sec);
    if (r != 6)
        return false;
    struct tm tmtime;
    time_t now;
    localtime_r (&now, &tmtime);  // fill in defaults
    tmtime.tm_sec = sec;
    tmtime.tm_min = min;
    tmtime.tm_hour = hour;
    tmtime.tm_mday = day;
    tmtime.tm_mon = month-1;
    tmtime.tm_year = year-1900;
    time_t t = mktime (&tmtime);

    times[0].tv_sec = t; // new access time
    times[0].tv_usec = 0;
    times[1].tv_sec = t; // new modification time
    times[1].tv_usec = 0;

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



int
main (int argc, char *argv[])
{
    getargs (argc, argv);

    // Find an ImageIO plugin that can open the input file, and open it.
    ImageInput *in = ImageInput::create (filenames[0].c_str(), "" /* searchpath */);
    if (! in) {
        std::cerr 
            << "iconvert ERROR: Could not find an ImageIO plugin to read \"" 
            << filenames[0] << "\" : " << OpenImageIO::error_message() << "\n";
        exit (EXIT_FAILURE);
    }
    ImageSpec inspec;
    if (! in->open (filenames[0].c_str(), inspec)) {
        std::cerr << "iconvert ERROR: Could not open \"" << filenames[0]
                  << "\" : " << in->error_message() << "\n";
        delete in;
        exit (EXIT_FAILURE);
    }

    // Copy the spec, with possible change in format
    ImageSpec outspec = inspec;
    outspec.set_format (inspec.format);
    if (! dataformatname.empty()) {
        if (dataformatname == "uint8")
            outspec.set_format (TypeDesc::UINT8);
        else if (dataformatname == "int8")
            outspec.set_format (TypeDesc::INT8);
        else if (dataformatname == "uint16")
            outspec.set_format (TypeDesc::UINT16);
        else if (dataformatname == "int16")
            outspec.set_format (TypeDesc::INT16);
        else if (dataformatname == "half")
            outspec.set_format (TypeDesc::HALF);
        else if (dataformatname == "float")
            outspec.set_format (TypeDesc::FLOAT);
        else if (dataformatname == "double")
            outspec.set_format (TypeDesc::DOUBLE);
    }
    outspec.gamma = gammaval;

    if (tile[0]) {
        outspec.tile_width = tile[0];
        outspec.tile_height = tile[1];
        outspec.tile_depth = tile[2];
    }
    if (scanline) {
        outspec.tile_width = 0;
        outspec.tile_height = 0;
        outspec.tile_depth = 0;
    }
    if (! compression.empty()) {
        outspec.attribute ("compression", compression);
    }

    if (quality > 0)
        outspec.attribute ("CompressionQuality", quality);

    if (caption != uninitialized)
        outspec.attribute ("ImageDescription", caption);

    if (clear_keywords || keywords != uninitialized) {
        std::string kw;
        ImageIOParameter *p = outspec.find_attribute ("Keywords", TypeDesc::TypeString);

        if (p)
            kw = *(const char **)p->data();
        if (clear_keywords)
            kw = "";
        if (keywords != uninitialized && !keywords.empty()) {
            std::vector<std::string> items;
            split_list (kw, items);
            bool dup = false;
            for (size_t i = 0;  i < items.size();  ++i)
                dup |= (items[i] == keywords);
            if (! dup) {
                if (kw.length() > 1)
                    kw += "; ";
                kw += keywords;
            }
        }
        outspec.attribute ("Keywords", kw);
    }

    // Find an ImageIO plugin that can open the output file, and open it
    ImageOutput *out = ImageOutput::create (filenames[1].c_str());
    if (! out) {
        std::cerr 
            << "iconvert ERROR: Could not find an ImageIO plugin to write \"" 
            << filenames[1] << "\" :" << OpenImageIO::error_message() << "\n";
        exit (EXIT_FAILURE);
    }
    if (! out->open (filenames[1].c_str(), outspec)) {
        std::cerr << "iconvert ERROR: Could not open \"" << filenames[1]
                  << "\" : " << out->error_message() << "\n";
        exit (EXIT_FAILURE);
    }

    if (! no_copy_image) {
        out->copy_image (in);
    } else {
        // Need to do it by hand for some reason.  Future expansion in which
        // only a subset of channels are copied, or some such.
        std::vector<char> pixels (outspec.image_bytes());
        in->read_image (outspec.format, &pixels[0]);
        out->write_image (outspec.format, &pixels[0]);
    }

    out->close ();
    delete out;
    in->close ();
    delete in;


    // If user requested, try to adjust the file's modification time to
    // the creation time indicated by the file's DateTime metadata.
    if (adjust_time) {
        struct timeval times[2];
        ImageIOParameter *p = outspec.find_attribute ("DateTime", TypeDesc::TypeString);
        if (p && DateTime_to_time_t (*(const char **)p->data(), times))
            utimes (filenames[1].c_str(), times);
    }

    return 0;
}
