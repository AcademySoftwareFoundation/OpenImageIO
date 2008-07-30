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

#include <ImathMatrix.h>

#include "argparse.h"
#include "imageio.h"
using namespace OpenImageIO;


// Basic runtime options
static std::vector<std::string> filenames;
static std::string outputfilename;
static std::string dataformatname = "";
static std::string fileformatname = "tiff";
static float ingamma = 1.0f, outgamma = 1.0f;
static bool verbose = false;
static int tile[3] = { 0, 0, 1 };
static std::string channellist;
static bool updatemode = false;

// Conversion modes.  If none are true, we just make an ordinary texture.
static bool shadowmode = false;
static bool shadowcubemode = false;
static bool volshadowmode = false;
static bool envlatlmode = false;
static bool envcubemode = false;
static bool lightprobemode = false;
static bool vertcrossmode = false;
static bool latl2envcubemode = false;

// Options controlling file metadata or mipmap creation
static float fov = 90;
static std::string wrap;
static std::string swrap = "black";
static std::string twrap = "black";
static bool noresize = false;
static float opaquewidth = 0;  // should be volume shadow epsilon
static Imath::M44f Mcam, Mscr;  // Initialize to identity
static bool separate = false;



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
    if (ap.parse ("Usage:  maketx [options] inputfile outputfile",
                  "%*", parse_files, "",
                  "--help", &help, "Print help message",
                  "-v", &verbose, "Verbose status messages",
                  "-o %s", &outputfilename, "Output directory or filename",
                  "-u", &updatemode, "Update mode",
                  "--format %s", &fileformatname, "Specify output format (default: tiff)",
                  "-d %s", &dataformatname, "Set the output data format to one of:\n"
                          "\t\t\tuint8, sint8, uint16, sint16, half, float",
                  "--tile %d %d", &tile[0], &tile[1], "Specify tile size",
                  "--separate", &separate, "Use planarconfig separate (default: contiguous)",
                  "--ingamma %f", &ingamma, "Specify gamma of input files (default: 1)",
                  "--outgamma %f", &outgamma, "Specify gamma of output files (default: 1)",
                  "--opaquewidth %f", &opaquewidth, "Set z fudge factor for volume shadows",
                  "--fov %f", &fov, "Field of view for envcube/shadcube/twofish",
                  "--wrap %s", &wrap, "Specify wrap mode (black, clamp, periodic, mirror)",
                  "--swrap %s", &swrap, "Specific s wrap mode separately",
                  "--twrap %s", &twrap, "Specific t wrap mode separately",
                  "--noresize", &noresize, "Do not resize textures to power of 2 resolution",
                  "--Mcamera %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
                          &Mcam[0][0], &Mcam[0][1], &Mcam[0][2], &Mcam[0][3], 
                          &Mcam[1][0], &Mcam[1][1], &Mcam[1][2], &Mcam[1][3], 
                          &Mcam[2][0], &Mcam[2][1], &Mcam[2][2], &Mcam[2][3], 
                          &Mcam[3][0], &Mcam[3][1], &Mcam[3][2], &Mcam[3][3], 
                          "Set the camera matrix",
                  "--Mscreen %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
                          &Mscr[0][0], &Mscr[0][1], &Mscr[0][2], &Mscr[0][3], 
                          &Mscr[1][0], &Mscr[1][1], &Mscr[1][2], &Mscr[1][3], 
                          &Mscr[2][0], &Mscr[2][1], &Mscr[2][2], &Mscr[2][3], 
                          &Mscr[3][0], &Mscr[3][1], &Mscr[3][2], &Mscr[3][3], 
                          "Set the camera matrix",
//FIXME           "-c %s", &channellist, "Restrict/shuffle channels",
//FIXME           "-debugdso"
//FIXME           "-note %s", &note, "Append a note to the image comments",
                  "<SEPARATOR>", "Basic modes (default is plain texture):",
                  "--shadow", &shadowmode, "Create shadow map",
                  "--shadcube", &shadowcubemode, "Create shadow cube (file order: px,nx,py,ny,pz,nz)",
                  "--volshad", &volshadowmode, "Create volume shadow map",
                  "--envlatl", &envlatlmode, "Create lat/long environment map",
                  "--envcube", &envcubemode, "Create cubic env map (file order: px,nx,py,ny,pz,nz)",
                  "--lightprobe", &lightprobemode, "Convert a lightprobe to cubic env map",
                  "--latl2envcube", &latl2envcubemode, "Convert a lat-long env map to a cubic env map",
                  "--vertcross", &vertcrossmode, "Convert a vertical cross layout to a cubic env map",
                  NULL) < 0) {
        std::cerr << ap.error_message() << std::endl;
        ap.usage ();
        exit (EXIT_FAILURE);
    }
    if (help) {
        ap.usage ();
        exit (EXIT_FAILURE);
    }

    int optionsum = ((int)shadowmode + (int)shadowcubemode + (int)volshadowmode +
                     (int)envlatlmode + (int)envcubemode +
                     (int)lightprobemode + (int)vertcrossmode +
                     (int)latl2envcubemode);
    if (optionsum > 1) {
        std::cerr << "maketx ERROR: At most one of the following options may be set:\n"
                  << "\t--shadow --shadcube --volshad --envlatl --envcube\n"
                  << "\t--lightprobe --vertcross --latl2envcube\n";
        ap.usage ();
        exit (EXIT_FAILURE);
    }

    if (filenames.size() < 1) {
        std::cerr << "maketx: Must have at least one input filename specified.\n";
        ap.usage();
        exit (EXIT_FAILURE);
    }
//    std::cout << "Converting " << filenames[0] << " to " << outputfilename << "\n";
}



int
main (int argc, char *argv[])
{
    getargs (argc, argv);

    // Find an ImageIO plugin that can open the input file, and open it.
    ImageInput *in = ImageInput::create (filenames[0].c_str(), "" /* searchpath */);
    if (! in) {
        std::cerr 
            << "maketx ERROR: Could not find an ImageIO plugin to read \"" 
            << filenames[0] << "\" : " << OpenImageIO::error_message() << "\n";
        exit (0);
    }
    ImageIOFormatSpec inspec;
    if (! in->open (filenames[0].c_str(), inspec)) {
        std::cerr << "maketx ERROR: Could not open \"" << filenames[0]
                  << "\" : " << in->error_message() << "\n";
        delete in;
        exit (0);
    }

    // Copy the spec, with possible change in format
    ImageIOFormatSpec outspec = inspec;
    outspec.set_format (inspec.format);
    if (! dataformatname.empty()) {
        if (dataformatname == "uint8")
            outspec.set_format (PT_UINT8);
        else if (dataformatname == "int8")
            outspec.set_format (PT_INT8);
        else if (dataformatname == "uint16")
            outspec.set_format (PT_UINT16);
        else if (dataformatname == "int16")
            outspec.set_format (PT_INT16);
        else if (dataformatname == "half")
            outspec.set_format (PT_HALF);
        else if (dataformatname == "float")
            outspec.set_format (PT_FLOAT);
        else if (dataformatname == "double")
            outspec.set_format (PT_DOUBLE);
    }

    if (tile[0]) {
        outspec.tile_width = tile[0];
        outspec.tile_height = tile[1];
        outspec.tile_depth = tile[2];
    }

    // Find an ImageIO plugin that can open the output file, and open it
    ImageOutput *out = ImageOutput::create (filenames[1].c_str());
    if (! out) {
        std::cerr 
            << "maketx ERROR: Could not find an ImageIO plugin to write \"" 
            << filenames[1] << "\" :" << OpenImageIO::error_message() << "\n";
        exit (0);
    }
    if (! out->open (filenames[1].c_str(), outspec)) {
        std::cerr << "maketx ERROR: Could not open \"" << filenames[1]
                  << "\" : " << out->error_message() << "\n";
        exit (0);
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
