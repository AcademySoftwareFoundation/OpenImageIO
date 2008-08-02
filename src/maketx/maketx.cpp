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

#include <boost/filesystem.hpp>
#include <ImathMatrix.h>

#include "argparse.h"
#include "filesystem.h"
#include "fmath.h"
#include "strutil.h"
#include "imageio.h"
using namespace OpenImageIO;
#include "imagebuf.h"


// Basic runtime options
static std::string full_command_line;
static std::vector<std::string> filenames;
static std::string outputfilename;
static std::string dataformatname = "";
static std::string fileformatname = "tiff";
static float ingamma = 1.0f, outgamma = 1.0f;
static bool verbose = false;
static int tile[3] = { 64, 64, 1 };
static std::string channellist;
static bool updatemode = false;

// Conversion modes.  If none are true, we just make an ordinary texture.
static bool mipmapmode = false;
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
static std::string wrap = "black";
static std::string swrap;
static std::string twrap;
static bool noresize = false;
static float opaquewidth = 0;  // should be volume shadow epsilon
static Imath::M44f Mcam(0.0f), Mscr(0.0f);  // Initialize to 0
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
    full_command_line = ap.command_line ();

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
    if (optionsum == 0)
        mipmapmode = true;

    if (filenames.size() < 1) {
        std::cerr << "maketx ERROR: Must have at least one input filename specified.\n";
        ap.usage();
        exit (EXIT_FAILURE);
    }
//    std::cout << "Converting " << filenames[0] << " to " << outputfilename << "\n";
}



static std::string
datestring ()
{
    time_t now;
    time (&now);
    struct tm mytm;
    localtime_r (&now, &mytm);
    return Strutil::format ("%4d:%02d:%02d %2d:%02d:%02d",
                            mytm.tm_year+1900, mytm.tm_mon+1, mytm.tm_mday,
                            mytm.tm_hour, mytm.tm_min, mytm.tm_sec);
}



static void
make_mipmap (void)
{
    if (filenames.size() != 1) {
        std::cerr << "maketx ERROR: Ordinary texture map requires exactly one filename\n";
        exit (EXIT_FAILURE);
    }

    if (! boost::filesystem::exists (filenames[0])) {
        std::cerr << "maketx ERROR: \"" << filenames[0] << "\" does not exist\n";
        exit (EXIT_FAILURE);
    }
    if (outputfilename.empty()) {
        std::string ext = boost::filesystem::extension (filenames[0]);
        int notextlen = (int) filenames[0].length() - (int) ext.length();
        outputfilename = std::string (filenames[0].begin(),
                                      filenames[0].begin() + notextlen);
        outputfilename += ".tx";
    }

    ImageBuf src (filenames[0]);
    if (! src.read()) {
        std::cerr 
            << "maketx ERROR: Could not find an ImageIO plugin to read \"" 
            << filenames[0] << "\" : " << src.error_message() << "\n";
        exit (EXIT_FAILURE);
    }

    // Copy the spec, with possible change in format
    ImageIOFormatSpec dstspec = src.spec();
    dstspec.set_format (src.spec().format);
    if (! dataformatname.empty()) {
        if (dataformatname == "uint8")
            dstspec.set_format (PT_UINT8);
        else if (dataformatname == "int8")
            dstspec.set_format (PT_INT8);
        else if (dataformatname == "uint16")
            dstspec.set_format (PT_UINT16);
        else if (dataformatname == "int16")
            dstspec.set_format (PT_INT16);
        else if (dataformatname == "half")
            dstspec.set_format (PT_HALF);
        else if (dataformatname == "float")
            dstspec.set_format (PT_FLOAT);
        else if (dataformatname == "double")
            dstspec.set_format (PT_DOUBLE);
    }

    // Make the output not a crop window
    dstspec.x = 0;
    dstspec.y = 0;
    dstspec.z = 0;
    dstspec.full_width = 0;
    dstspec.full_height = 0;
    dstspec.full_depth = 0;

    // Make the output tiled, regardless of input
    dstspec.tile_width  = tile[0];
    dstspec.tile_height = tile[1];
    dstspec.tile_depth  = tile[2];

    // Always use ZIP compression
    dstspec.attribute ("compression", "zip");

    dstspec.attribute ("DateTime", datestring());
    dstspec.attribute ("Software", full_command_line);
    dstspec.attribute ("textureformat", "Plain Texture");

    if (Mcam != Imath::M44f(0.0f))
        dstspec.attribute ("worldtocamera", PT_MATRIX, 1, &Mcam);
    if (Mscr != Imath::M44f(0.0f))
        dstspec.attribute ("worldtoscreen", PT_MATRIX, 1, &Mscr);

    // FIXME - check for valid strings in the wrap mode
    std::string wrapmodes = (swrap.size() ? swrap : wrap) + ',' + 
                            (twrap.size() ? twrap : wrap);
    dstspec.attribute ("wrapmodes", wrapmodes);
    dstspec.attribute ("fovcot", (float)src.spec().width / src.spec().height);

    // FIXME -- should we allow tile sizes to reduce if the image is
    // smaller than the tile size?  And when we do, should we also try
    // to make it bigger in the other direction to make the total tile
    // size more constant?

    dstspec.set_format (PT_FLOAT);
    if (! noresize) {
        dstspec.width = pow2roundup (dstspec.width);
        dstspec.height = pow2roundup (dstspec.height);
        dstspec.full_width = dstspec.width;
        dstspec.full_height = dstspec.height;
    }
    ImageBuf dst ("temp", dstspec);
    float *pel = (float *) alloca (dstspec.pixel_bytes());
    for (int y = 0;  y < dstspec.height;  ++y) {
        for (int x = 0;  x < dstspec.width;  ++x) {
            src.interppixel_NDC ((x+0.5f)/(float)dstspec.width,
                                 (y+0.5f)/(float)dstspec.height, pel);
            dst.setpixel (x, y, pel);
        }
    }

    // Find an ImageIO plugin that can open the output file, and open it
    ImageOutput *out = ImageOutput::create (fileformatname.c_str());
    if (! out) {
        std::cerr 
            << "maketx ERROR: Could not find an ImageIO plugin to write " 
            << fileformatname << " files:" << OpenImageIO::error_message() << "\n";
        exit (EXIT_FAILURE);
    }
    if (! out->supports ("tiles") || ! out->supports ("multiimage")) {
        std::cerr << "maketx ERROR: \"" << outputfilename
                  << "\" format does not support tiled, multires images\n";
        exit (EXIT_FAILURE);
    }
    ImageIOFormatSpec outspec = dstspec;
    outspec.set_format (src.spec().format);
    if (! out->open (outputfilename.c_str(), outspec)) {
        std::cerr << "maketx ERROR: Could not open \"" << outputfilename
                  << "\" : " << out->error_message() << "\n";
        exit (EXIT_FAILURE);
    }

    out->write_image (PT_FLOAT, dst.pixeladdr(0,0));
    while (dstspec.width > 1 || dstspec.height > 1) {
        ImageBuf tmp = dst;
        if (dstspec.width > 1)
            dstspec.width /= 2;
        if (dstspec.height > 1)
            dstspec.height /= 2;
        dst.alloc (dstspec);  // Realocate with new size
        for (int y = 0;  y < dstspec.height;  ++y) {
            for (int x = 0;  x < dstspec.width;  ++x) {
                tmp.interppixel_NDC ((x+0.5f)/(float)dstspec.width,
                                     (y+0.5f)/(float)dstspec.height, pel);
                dst.setpixel (x, y, pel);
            }
        }
        outspec = dst.spec();
        outspec.set_format (src.spec().format);
        if (! out->open (outputfilename.c_str(), outspec, true)) {
            std::cerr << "maketx ERROR: Could not append \"" << outputfilename
                      << "\" : " << out->error_message() << "\n";
            exit (EXIT_FAILURE);
        }
        out->write_image (PT_FLOAT, dst.pixeladdr(0,0));
    }

    out->close ();
    delete out;
}



int
main (int argc, char *argv[])
{
    getargs (argc, argv);

    if (mipmapmode) {
        make_mipmap ();
    } else if (shadowmode) {
    } else if (shadowcubemode) {
    } else if (volshadowmode) {
    } else if (envlatlmode) {
    } else if (envcubemode) {
    } else if (lightprobemode) {
    } else if (vertcrossmode) {
    } else if (latl2envcubemode) {
    }

    return 0;
}
