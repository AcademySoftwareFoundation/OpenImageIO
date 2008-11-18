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

#include "argparse.h"
#include "imageio.h"
using namespace OpenImageIO;


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
static int quality = -1;



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
                          "\t\t\tuint8, sint8, uint16, sint16, half, float",
                  "-g %f", &gammaval, "Set gamma correction (default = 1)",
                  "--tile %d %d", &tile[0], &tile[1], "Output as a tiled image",
                  "--scanline", &scanline, "Output as a scanline image",
                  "--compression %s", &compression, "Set the compression method (default = same as input)",
                  "--quality %d", &quality, "Set the compression quality, 1-100",
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

    char *pixels = new char [outspec.image_bytes()];
    in->read_image (outspec.format, pixels);
    in->close ();
    delete in;
    in = NULL;
    out->write_image (outspec.format, pixels);
    out->close ();
    delete out;
    delete [] pixels;

    return 0;
}
