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
static std::string fileformatname = "";
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
    if (ap.parse ("Usage:  maketx [options] file...",
                  "%*", parse_files, "",
                  "--help", &help, "Print help message",
                  "-v", &verbose, "Verbose status messages",
                  "-o %s", &outputfilename, "Output filename",
                  "-u", &updatemode, "Update mode",
                  "--format %s", &fileformatname, "Specify output format (default: guess from extension)",
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
                  "--shadow", &shadowmode, "Create shadow map (UNIMPLEMENTED)",
                  "--shadcube", &shadowcubemode, "Create shadow cube (file order: px,nx,py,ny,pz,nz) (UNIMP)",
                  "--volshad", &volshadowmode, "Create volume shadow map (UNIMP)",
                  "--envlatl", &envlatlmode, "Create lat/long environment map (UNIMP)",
                  "--envcube", &envcubemode, "Create cubic env map (file order: px,nx,py,ny,pz,nz) (UNIMP)",
                  "--lightprobe", &lightprobemode, "Convert a lightprobe to cubic env map (UNIMP)",
                  "--latl2envcube", &latl2envcubemode, "Convert a lat-long env map to a cubic env map (UNIMP)",
                  "--vertcross", &vertcrossmode, "Convert a vertical cross layout to a cubic env map (UNIMP)",
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
datestring (time_t t)
{
    struct tm mytm;
    localtime_r (&t, &mytm);
    return Strutil::format ("%4d:%02d:%02d %2d:%02d:%02d",
                            mytm.tm_year+1900, mytm.tm_mon+1, mytm.tm_mday,
                            mytm.tm_hour, mytm.tm_min, mytm.tm_sec);
}



static void
make_mipmap (void)
{
    if (filenames.size() != 1) {
        std::cerr << "maketx ERROR: Ordinary texture map requires exactly one input filename\n";
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

    // When was the input file last modified?
    std::time_t in_time = boost::filesystem::last_write_time (filenames[0]);

    // When in update mode, skip making the texture if the output already
    // exists and has the same file modification time as the input file.
    if (updatemode && boost::filesystem::exists (outputfilename) &&
        (in_time == boost::filesystem::last_write_time (outputfilename))) {
        std::cout << "maketx: no update required for \"" 
                  << outputfilename << "\"\n";
        return;
    }

    ImageBuf src (filenames[0]);
    if (! src.read()) {
        std::cerr 
            << "maketx ERROR: Could not find an ImageIO plugin to read \"" 
            << filenames[0] << "\" : " << src.error_message() << "\n";
        exit (EXIT_FAILURE);
    }

    // Copy the spec, with possible change in format
    ImageSpec dstspec = src.spec();
    dstspec.set_format (src.spec().format);
    if (! dataformatname.empty()) {
        if (dataformatname == "uint8")
            dstspec.set_format (TypeDesc::UINT8);
        else if (dataformatname == "int8")
            dstspec.set_format (TypeDesc::INT8);
        else if (dataformatname == "uint16")
            dstspec.set_format (TypeDesc::UINT16);
        else if (dataformatname == "int16")
            dstspec.set_format (TypeDesc::INT16);
        else if (dataformatname == "half")
            dstspec.set_format (TypeDesc::HALF);
        else if (dataformatname == "float")
            dstspec.set_format (TypeDesc::FLOAT);
        else if (dataformatname == "double")
            dstspec.set_format (TypeDesc::DOUBLE);
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
    // dstspec.attribute ("compression", "zip");
    // Ugh, the line above seems to trigger a bug in the tiff library.
    // Maybe a bug in libtiff zip compression for tiles?  So let's
    // stick to the default compression.

    // Put a DateTime in the out file, either now, or matching the date
    // stamp of the input file (if update mode).
    time_t date;
    if (updatemode)
        date = in_time;  // update mode: use the time stamp of the input
    else
        time (&date);    // not update: get the time now
    dstspec.attribute ("DateTime", datestring(date));

    dstspec.attribute ("Software", full_command_line);
    dstspec.attribute ("textureformat", "Plain Texture");

    if (Mcam != Imath::M44f(0.0f))
        dstspec.attribute ("worldtocamera", PT_MATRIX, &Mcam);
    if (Mscr != Imath::M44f(0.0f))
        dstspec.attribute ("worldtoscreen", PT_MATRIX, &Mscr);

    // FIXME - check for valid strings in the wrap mode
    std::string wrapmodes = (swrap.size() ? swrap : wrap) + ',' + 
                            (twrap.size() ? twrap : wrap);
    dstspec.attribute ("wrapmodes", wrapmodes);
    dstspec.attribute ("fovcot", (float)src.spec().width / src.spec().height);

    // FIXME -- should we allow tile sizes to reduce if the image is
    // smaller than the tile size?  And when we do, should we also try
    // to make it bigger in the other direction to make the total tile
    // size more constant?

    dstspec.set_format (TypeDesc::FLOAT);
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
    std::string outformat = fileformatname.empty() ? outputfilename : fileformatname;
    ImageOutput *out = ImageOutput::create (outformat.c_str());
    if (! out) {
        std::cerr 
            << "maketx ERROR: Could not find an ImageIO plugin to write " 
            << outformat << " files:" << OpenImageIO::error_message() << "\n";
        exit (EXIT_FAILURE);
    }
    if (! out->supports ("tiles") || ! out->supports ("multiimage")) {
        std::cerr << "maketx ERROR: \"" << outputfilename
                  << "\" format does not support tiled, multires images\n";
        exit (EXIT_FAILURE);
    }
    ImageSpec outspec = dstspec;
    outspec.set_format (src.spec().format);
    if (! out->open (outputfilename.c_str(), outspec)) {
        std::cerr << "maketx ERROR: Could not open \"" << outputfilename
                  << "\" : " << out->error_message() << "\n";
        exit (EXIT_FAILURE);
    }

    bool ok = true;
    ok &= out->write_image (TypeDesc::FLOAT, dst.pixeladdr(0,0));
    while (ok && (dstspec.width > 1 || dstspec.height > 1)) {
        ImageBuf tmp = dst;
        if (dstspec.width > 1)
            dstspec.width /= 2;
        if (dstspec.height > 1)
            dstspec.height /= 2;
        dstspec.full_width  = dstspec.width;
        dstspec.full_height = dstspec.height;
        dstspec.full_depth  = dstspec.depth;
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
        ok &= out->write_image (TypeDesc::FLOAT, dst.pixeladdr(0,0));
    }

    if (ok)
        ok &= out->close ();
    delete out;

    if (! ok) {
        std::cerr << "maketx ERROR writing \"" << outputfilename
                  << "\" : " << out->error_message() << "\n";
        exit (EXIT_FAILURE);
    }

    // If using update mode, stamp the output file with a modification time
    // matching that of the input file.
    if (updatemode)
        boost::filesystem::last_write_time (outputfilename, in_time);
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
