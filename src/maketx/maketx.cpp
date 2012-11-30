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
#include <limits>
#include <sstream>

#include <boost/version.hpp>
#include <boost/filesystem.hpp>
#include <OpenEXR/ImathMatrix.h>

#include "argparse.h"
#include "dassert.h"
#include "filesystem.h"
#include "fmath.h"
#include "strutil.h"
#include "sysutil.h"
#include "timer.h"
#include "imageio.h"
#include "imagebuf.h"
#include "imagebufalgo.h"
#include "maketexture.h"
#include "thread.h"
#include "filter.h"

OIIO_NAMESPACE_USING


static MaketxParams param;

// Basic runtime options
static std::string full_command_line;
static std::vector<std::string> filenames;
static bool updatemode = false;
static bool stats = false;
static bool embed_hash = true;  // Deprecated
static std::string filtername = "box";
ColorConfig colorconfig;

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


static std::string
filter_help_string ()
{
    std::string s ("Select filter for resizing (choices:");
    for (int i = 0, e = Filter2D::num_filters();  i < e;  ++i) {
        FilterDesc d;
        Filter2D::get_filterdesc (i, &d);
        s.append (" ");
        s.append (d.name);
    }
    s.append (", default=box)");
    return s;
}



static std::string
colortitle_help_string ()
{
    std::string s ("Color Management Options ");
    if(ColorConfig::supportsOpenColorIO()) {
        s += "(OpenColorIO enabled)";
    }
    else {
        s += "(OpenColorIO DISABLED)";
    }
    return s;
}



static std::string
colorconvert_help_string ()
{
    std::string s = "Apply a color space conversion to the image. "
    "If the output color space is not the same bit depth "
    "as input color space, it is your responsibility to set the data format "
    "to the proper bit depth using the -d option. ";
    
    s += " (choices: ";
    if (colorconfig.error() || colorconfig.getNumColorSpaces()==0) {
        s += "NONE";
    } else {
        for (int i=0; i < colorconfig.getNumColorSpaces(); ++i) {
            if (i!=0) s += ", ";
            s += colorconfig.getColorSpaceNameByIndex(i);
        }
    }
    s += ")";
    return s;
}



static Filter2D *
setup_filter (const std::string &filtername)
{
    // Figure out the recommended filter width for the named filter
    float filterwidth = 1.0f;
    for (int i = 0, e = Filter2D::num_filters();  i < e;  ++i) {
        FilterDesc d;
        Filter2D::get_filterdesc (i, &d);
        if (filtername == d.name) {
            filterwidth = d.width;
            break;
        }
    }

    Filter2D *filter = Filter2D::create (filtername, filterwidth, filterwidth);

    return filter;
}



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
    ap.options ("maketx -- convert images to tiled, MIP-mapped textures\n"
                OIIO_INTRO_STRING "\n"
                "Usage:  maketx [options] file...",
                  "%*", parse_files, "",
                  "--help", &help, "Print help message",
                  "-v", &param.verbose, "Verbose status messages",
                  "-o %s", &param.outputfilename, "Output filename",
                  "--threads %d", &param.nthreads, "Number of threads (default: #cores)",
                  "-u", &updatemode, "Update mode",
                  "--format %s", &param.fileformatname, "Specify output file format (default: guess from extension)",
                  "--nchannels %d", &param.nchannels, "Specify the number of output image channels.",
                  "-d %s", &param.dataformatname, "Set the output data format to one of: "
                          "uint8, sint8, uint16, sint16, half, float",
                  "--tile %d %d", &param.tile_width, &param.tile_height, "Specify tile size",
                  "--separate", &param.separate, "Use planarconfig separate (default: contiguous)",
//                  "--ingamma %f", &ingamma, "Specify gamma of input files (default: 1)",
//                  "--outgamma %f", &outgamma, "Specify gamma of output files (default: 1)",
//                  "--opaquewidth %f", &opaquewidth, "Set z fudge factor for volume shadows",
                  "--fov %f", &param.fov, "Field of view for envcube/shadcube/twofish",
                  "--fovcot %f", &param.fovcot, "Override the frame aspect ratio. Default is width/height.",
                  "--wrap %s", &param.wrap, "Specify wrap mode (black, clamp, periodic, mirror)",
                  "--swrap %s", &param.swrap, "Specific s wrap mode separately",
                  "--twrap %s", &param.twrap, "Specific t wrap mode separately",
                  "--resize", &param.pow2resize, "Resize textures to power of 2 (default: no)",
                  "--noresize %!", &param.pow2resize, "Do not resize textures to power of 2 (deprecated)",
                  "--filter %s", &filtername, filter_help_string().c_str(),
                  "--nomipmap", &param.nomipmap, "Do not make multiple MIP-map levels",
                  "--checknan", &param.checknan, "Check for NaN/Inf values (abort if found)",
                  "--fixnan %s", &param.fixnan, "Attempt to fix NaN/Inf values in the image (options: none, black, box3)",
                  "--Mcamera %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
                          &param.Mcam[0][0], &param.Mcam[0][1], &param.Mcam[0][2], &param.Mcam[0][3], 
                          &param.Mcam[1][0], &param.Mcam[1][1], &param.Mcam[1][2], &param.Mcam[1][3], 
                          &param.Mcam[2][0], &param.Mcam[2][1], &param.Mcam[2][2], &param.Mcam[2][3], 
                          &param.Mcam[3][0], &param.Mcam[3][1], &param.Mcam[3][2], &param.Mcam[3][3], 
                          "Set the camera matrix",
                  "--Mscreen %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
                          &param.Mscr[0][0], &param.Mscr[0][1], &param.Mscr[0][2], &param.Mscr[0][3], 
                          &param.Mscr[1][0], &param.Mscr[1][1], &param.Mscr[1][2], &param.Mscr[1][3], 
                          &param.Mscr[2][0], &param.Mscr[2][1], &param.Mscr[2][2], &param.Mscr[2][3], 
                          &param.Mscr[3][0], &param.Mscr[3][1], &param.Mscr[3][2], &param.Mscr[3][3], 
                          "Set the camera matrix",
                  "--hash", &embed_hash, "Embed SHA-1 hash of pixels in the header",
                  "--prman-metadata", &param.prman_metadata, "Add prman specific metadata",
                  "--constant-color-detect", &param.constant_color_detect, "Create 1-tile textures from constant color inputs",
                  "--monochrome-detect", &param.monochrome_detect, "Create 1-channel textures from monochrome inputs",
                  "--opaque-detect", &param.opaque_detect, "Drop alpha channel that is always 1.0",
                  "--stats", &stats, "Print runtime statistics",
                  "--mipimage %L", &param.mipimages, "Specify an individual MIP level",
//FIXME           "-c %s", &channellist, "Restrict/shuffle channels",
//FIXME           "-debugdso"
//FIXME           "-note %s", &note, "Append a note to the image comments",
                  "<SEPARATOR>", "Basic modes (default is plain texture):",
                  "--shadow", &shadowmode, "Create shadow map",
//                  "--shadcube", &shadowcubemode, "Create shadow cube (file order: px,nx,py,ny,pz,nz) (UNIMPLEMENTED)",
//                  "--volshad", &volshadowmode, "Create volume shadow map (UNIMP)",
                  "--envlatl", &envlatlmode, "Create lat/long environment map",
                  "--envcube", &envcubemode, "Create cubic env map (file order: px, nx, py, ny, pz, nz) (UNIMP)",
//                  "--lightprobe", &lightprobemode, "Convert a lightprobe to cubic env map (UNIMP)",
//                  "--latl2envcube", &latl2envcubemode, "Convert a lat-long env map to a cubic env map (UNIMP)",
//                  "--vertcross", &vertcrossmode, "Convert a vertical cross layout to a cubic env map (UNIMP)",
                  "<SEPARATOR>", colortitle_help_string().c_str(),
                  "--colorconvert %s %s", &param.incolorspace, &param.outcolorspace,
                          colorconvert_help_string().c_str(),
                  "--unpremult", &param.unpremult, "Unpremultiply before color conversion, then premultiply "
                          "after the color conversion.  You'll probably want to use this flag "
                          "if your image contains an alpha channel.",
                  "<SEPARATOR>", "Configuration Presets",
                  "--prman", &param.prman, "Use PRMan-safe settings for tile size, planarconfig, and metadata.",
                  "--oiio", &param.oiio, "Use OIIO-optimized settings for tile size, planarconfig, metadata, and constant-color optimizations.",
                  NULL);
    if (ap.parse (argc, (const char**)argv) < 0) {
        std::cerr << ap.geterror() << std::endl;
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
    
    if (param.prman && param.oiio) {
        std::cerr << "maketx ERROR: '--prman' compatibility, and '--oiio' optimizations are mutually exclusive.\n";
        std::cerr << "\tIf you'd like both prman and oiio compatibility, you should choose --prman\n";
        std::cerr << "\t(at the expense of oiio-specific optimizations)\n";
        ap.usage ();
        exit (EXIT_FAILURE);
    }

    if (filenames.size() < 1) {
        std::cerr << "maketx ERROR: Must have at least one input filename specified.\n";
        ap.usage();
        exit (EXIT_FAILURE);
    }

    param.filter = setup_filter (filtername);
    if (! param.filter) {
        std::cerr << "maketx ERROR: could not make filter '" << filtername << "\n";
        exit (EXIT_FAILURE);
    }

    if (param.verbose) {
        std::cerr << "maketx WARNING: The --embed_hash option is deprecated, and no longer necessary.\n";
        std::cerr << "                 (Hashes are always computed.)\n";
    }
    
//    std::cout << "Converting " << filenames[0] << " to " << param.outputfilename << "\n";
}



int
main (int argc, char *argv[])
{
    Timer alltimer;
    getargs (argc, argv);

    OIIO::attribute ("threads", param.nthreads);
    if (stats) {
        ImageCache *ic = ImageCache::create ();  // get the shared one
        ic->attribute ("forcefloat", 1);   // Force float upon read
        ic->attribute ("max_memory_MB", 1024.0);  // 1 GB cache
        // N.B. This will apply to the default IC that any ImageBuf's get.
    }

    if (mipmapmode) {
        param.conversionmode = MaketxParams::MIPMAP;
    } else if (shadowmode) {
        param.conversionmode = MaketxParams::SHADOW;
    } else if (shadowcubemode) {
        std::cerr << "Shadow cubes currently unsupported\n";
    } else if (volshadowmode) {
        std::cerr << "Volume shadows currently unsupported\n";
    } else if (envlatlmode) {
        param.conversionmode = MaketxParams::ENVLATLONG;
    } else if (envcubemode) {
        std::cerr << "Environment cubes currently unsupported\n";
    } else if (lightprobemode) {
        std::cerr << "Light probes currently unsupported\n";
    } else if (vertcrossmode) {
        std::cerr << "Vertcross currently unsupported\n";
    } else if (latl2envcubemode) {
        std::cerr << "Latlong->cube conversion currently unsupported\n";
    }
  
    if (filenames.size() != 1) {
        std::cerr << "maketx ERROR: Requires exactly one input filename\n";
        exit (EXIT_FAILURE);
    }
    
    if (! Filesystem::exists (filenames[0])) {
        std::cerr << "maketx ERROR: \"" << filenames[0] << "\" does not exist\n";
        exit (EXIT_FAILURE);
    }
    if (param.outputfilename.empty())
        param.outputfilename = Filesystem::replace_extension (filenames[0], ".tx");
    
    // When was the input file last modified?
    std::time_t in_time = Filesystem::last_write_time (filenames[0]);
    
    // When in update mode, skip making the texture if the output already
    // exists and has the same file modification time as the input file.
    MaketxStats stat;
    if (updatemode && Filesystem::exists (param.outputfilename) &&
        (in_time == Filesystem::last_write_time (param.outputfilename))) {
        std::cout << "maketx: no update required for \""
                  << param.outputfilename << "\"\n";
    } else {
        // Find an ImageIO plugin that can open the output file, and open it
        std::string outformat = param.fileformatname.empty() ?
                                param.outputfilename : param.fileformatname;
        ImageOutput *out = ImageOutput::create (outformat.c_str());
        if (! out) {
            std::cerr
                << "maketx ERROR: Could not find an ImageIO plugin to write "
                << outformat << " files:" << geterror() << "\n";
            return false;
        }
        if (! out->supports ("tiles")) {
            std::cerr << "maketx ERROR: \"" << param.outputfilename
                      << "\" format does not support tiled images\n";
            return false;
        }
        
        ImageBuf src(filenames[0]);
        src.init_spec (filenames[0], 0, 0); // force it to get the spec, not read
        if (!make_texturemap(src, out, param, &stat))
            exit (EXIT_FAILURE);

        delete out;

        // If using update mode, stamp the output file with a modification time
        // matching that of the input file.
        if (updatemode)
            Filesystem::last_write_time (param.outputfilename, in_time);
    }
    
    if (param.verbose || stats) {
        std::cout << "maketx Runtime statistics (seconds):\n";
        double alltime = alltimer();
        std::cout << Strutil::format ("  total runtime:   %5.2f\n", alltime);
        std::cout << Strutil::format ("  file read:       %5.2f\n", stat.readtime);
        std::cout << Strutil::format ("  file write:      %5.2f\n", stat.writetime);
        std::cout << Strutil::format ("  initial resize:  %5.2f\n", stat.resizetime);
        std::cout << Strutil::format ("  mip computation: %5.2f\n", stat.miptime);
        std::cout << Strutil::format ("  color convert:   %5.2f\n", stat.colorconverttime);
        std::cout << Strutil::format ("  unaccounted:     %5.2f\n",
                                      alltime-stat.readtime-stat.writetime-stat.resizetime-stat.miptime);
        size_t kb = Sysutil::memory_used(true) / 1024;
        std::cout << Strutil::format ("maketx memory used: %5.1f MB\n",
                                      (double)kb/1024.0);
    }

    Filter2D::destroy (param.filter);

    if (stats) {
        std::cout << "\n" << ic->getstats();
    }

    return 0;
}
