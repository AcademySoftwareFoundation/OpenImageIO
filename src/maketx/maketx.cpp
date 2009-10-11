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

/* This header have to be included before boost/regex.hpp header
   If it is included after, there is an error
   "undefined reference to CSHA1::Update (unsigned char const*, unsigned long)"
*/
#include "SHA1.h"

#include <boost/filesystem.hpp>
#include <ImathMatrix.h>

#include "argparse.h"
#include "dassert.h"
#include "filesystem.h"
#include "fmath.h"
#include "strutil.h"
#include "sysutil.h"
#include "timer.h"
#include "imageio.h"
using namespace OpenImageIO;
#include "imagebuf.h"
#include "sysutil.h"


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
static double stat_readtime = 0;
static double stat_writetime = 0;
static double stat_resizetime = 0;
static double stat_miptime = 0;

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
static bool nomipmap = false;
static bool embed_hash;


// forward decl
static void write_mipmap (ImageBuf &img, const ImageSpec &outspec_template,
                   std::string outputfilename, std::string outformat,
                   TypeDesc outputdatatype, bool mipmap);



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
                OPENIMAGEIO_INTRO_STRING "\n"
                "Usage:  maketx [options] file...",
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
                  "--nomipmap", &nomipmap, "Do not make multiple MIP-map levels",
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
                  "--hash", &embed_hash, "Embed SHA-1 hash of pixels in the header",
//FIXME           "-c %s", &channellist, "Restrict/shuffle channels",
//FIXME           "-debugdso"
//FIXME           "-note %s", &note, "Append a note to the image comments",
                  "<SEPARATOR>", "Basic modes (default is plain texture):",
                  "--shadow", &shadowmode, "Create shadow map",
                  "--shadcube", &shadowcubemode, "Create shadow cube (file order: px,nx,py,ny,pz,nz) (UNIMPLEMENTED)",
                  "--volshad", &volshadowmode, "Create volume shadow map (UNIMP)",
                  "--envlatl", &envlatlmode, "Create lat/long environment map (UNIMP)",
                  "--envcube", &envcubemode, "Create cubic env map (file order: px,nx,py,ny,pz,nz) (UNIMP)",
                  "--lightprobe", &lightprobemode, "Convert a lightprobe to cubic env map (UNIMP)",
                  "--latl2envcube", &latl2envcubemode, "Convert a lat-long env map to a cubic env map (UNIMP)",
                  "--vertcross", &vertcrossmode, "Convert a vertical cross layout to a cubic env map (UNIMP)",
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
    Sysutil::get_local_time (&t, &mytm);
    return Strutil::format ("%4d:%02d:%02d %2d:%02d:%02d",
                            mytm.tm_year+1900, mytm.tm_mon+1, mytm.tm_mday,
                            mytm.tm_hour, mytm.tm_min, mytm.tm_sec);
}



static void
make_texturemap (const char *maptypename = "texture map")
{
    if (filenames.size() != 1) {
        std::cerr << "maketx ERROR: " << maptypename 
                  << " requires exactly one input filename\n";
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
    src.init_spec (filenames[0]);  // force it to get the spec, not to read

    // The cache might mess with the apparent data format.  But for the 
    // purposes of what we should output, figure it out now, before the
    // file has been read and cached.
    TypeDesc out_dataformat = src.spec().format;

    if (verbose)
        std::cout << "Reading file: " << filenames[0] << std::endl;
    Timer readtimer;
    if (! src.read()) {
        std::cerr 
            << "maketx ERROR: Could not find an ImageIO plugin to read \"" 
            << filenames[0] << "\" : " << src.geterror() << "\n";
        exit (EXIT_FAILURE);
    }
    stat_readtime += readtimer();

    std::string hash_digest;
    if (embed_hash) {
        CSHA1 sha;
        sha.Reset ();
        // Do one scanline at a time, to keep to < 2^32 bytes each
        imagesize_t scanline_bytes = src.spec().scanline_bytes();
        ASSERT (scanline_bytes < std::numeric_limits<unsigned int>::max());
        std::vector<unsigned char> tmp (scanline_bytes);
        for (int y = src.ymin();  y <= src.ymax();  ++y) {
            src.copy_pixels (src.xbegin(), src.xend(), y, y+1,
                             src.spec().format, &tmp[0]);
            sha.Update (&tmp[0], (unsigned int) scanline_bytes);
        }
        sha.Final ();
        sha.ReportHashStl (hash_digest, CSHA1::REPORT_HEX_SHORT);
        if (verbose)
            std::cout << "  SHA-1: " << hash_digest << "\n";
    }

    // Figure out which data format we want for output
    if (! dataformatname.empty()) {
        if (dataformatname == "uint8")
            out_dataformat = TypeDesc::UINT8;
        else if (dataformatname == "int8" || dataformatname == "sint8")
            out_dataformat = TypeDesc::INT8;
        else if (dataformatname == "uint16")
            out_dataformat = TypeDesc::UINT16;
        else if (dataformatname == "int16" || dataformatname == "sint16")
            out_dataformat = TypeDesc::INT16;
        else if (dataformatname == "half")
            out_dataformat = TypeDesc::HALF;
        else if (dataformatname == "float")
            out_dataformat = TypeDesc::FLOAT;
        else if (dataformatname == "double")
            out_dataformat = TypeDesc::DOUBLE;
    }

    if (shadowmode) {
        // Some special checks for shadow maps
        if (src.spec().nchannels != 1) {
            std::cerr << "maketx ERROR: shadow maps require 1-channel images,\n"
                      << "\t\"" << filenames[0] << "\" is " 
                      << src.spec().nchannels << " channels\n";
            exit (EXIT_FAILURE);
        }
        // Shadow maps only make sense for floating-point data.
        if (out_dataformat != TypeDesc::FLOAT &&
              out_dataformat != TypeDesc::HALF &&
              out_dataformat != TypeDesc::DOUBLE)
            out_dataformat = TypeDesc::FLOAT;
    }

    // Copy the input spec
    const ImageSpec &srcspec = src.spec();
    ImageSpec dstspec = srcspec;

    // Make the output not a crop window
    dstspec.x = 0;
    dstspec.y = 0;
    dstspec.z = 0;
    dstspec.width = srcspec.full_width;
    dstspec.height = srcspec.full_height;
    dstspec.depth = srcspec.full_depth;
    dstspec.full_x = 0;
    dstspec.full_y = 0;
    dstspec.full_z = 0;
    dstspec.full_width = dstspec.width;
    dstspec.full_height = dstspec.height;
    dstspec.full_depth = dstspec.depth;
    bool orig_was_crop = (srcspec.x != 0 || srcspec.y != 0 || srcspec.z != 0 ||
                          srcspec.full_width != srcspec.width ||
                          srcspec.full_height != srcspec.height ||
                          srcspec.full_depth != srcspec.depth);

    // Make the output tiled, regardless of input
    dstspec.tile_width  = tile[0];
    dstspec.tile_height = tile[1];
    dstspec.tile_depth  = tile[2];

    // Always use ZIP compression
    dstspec.attribute ("compression", "zip");
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

    if (hash_digest.length()) {
        std::string desc = dstspec.get_string_attribute ("ImageDescription");
        if (desc.length())
            desc += " ";
        desc += "SHA-1=";
        desc += hash_digest;
        dstspec.attribute ("ImageDescription", desc);
    }

    if (shadowmode)
        dstspec.attribute ("textureformat", "Shadow");
    else
        dstspec.attribute ("textureformat", "Plain Texture");

    if (Mcam != Imath::M44f(0.0f))
        dstspec.attribute ("worldtocamera", PT_MATRIX, &Mcam);
    if (Mscr != Imath::M44f(0.0f))
        dstspec.attribute ("worldtoscreen", PT_MATRIX, &Mscr);

    // FIXME - check for valid strings in the wrap mode
    if (! shadowmode) {
        std::string wrapmodes = (swrap.size() ? swrap : wrap) + ',' + 
                                (twrap.size() ? twrap : wrap);
        dstspec.attribute ("wrapmodes", wrapmodes);
    }
    dstspec.attribute ("fovcot", (float)srcspec.full_width / srcspec.full_height);

    if (separate)
        dstspec.attribute ("planarconfig", "separate");

    // FIXME -- should we allow tile sizes to reduce if the image is
    // smaller than the tile size?  And when we do, should we also try
    // to make it bigger in the other direction to make the total tile
    // size more constant?

    // Force float for the sake of the ImageBuf math
    dstspec.set_format (TypeDesc::FLOAT);
    if (! noresize  &&  ! shadowmode) {
        dstspec.width = pow2roundup (dstspec.width);
        dstspec.height = pow2roundup (dstspec.height);
        dstspec.full_width = dstspec.width;
        dstspec.full_height = dstspec.height;
    }
    Timer resizetimer;
    ImageBuf dst ("temp", dstspec);
    ImageBuf *toplevel = &dst;    // Ptr to top level of mipmap
    float *pel = (float *) alloca (dstspec.pixel_bytes());
    if (dstspec.width == srcspec.width && 
        dstspec.height == srcspec.height &&
        dstspec.depth == srcspec.depth && ! orig_was_crop) {
        // Special case: don't need to resize
        if (dstspec.format == srcspec.format) {
            // Even more special case, no format change -- just use
            // the original copy.
            toplevel = &src;
        } else {
            for (int y = 0;  y < dstspec.height;  ++y) {
                for (int x = 0;  x < dstspec.width;  ++x) {
                    src.getpixel (x, y, pel);
                    dst.setpixel (x, y, pel);
                }
            }
        }
    } else {
        // General case: resize
        if (verbose)
            std::cout << "  Resizing image to " << dstspec.width 
                      << " x " << dstspec.height << std::endl;
        for (int y = 0;  y < dstspec.height;  ++y) {
            for (int x = 0;  x < dstspec.width;  ++x) {
                src.interppixel_NDC_full ((x+0.5f)/(float)dstspec.width,
                                          (y+0.5f)/(float)dstspec.height, pel);
                dst.setpixel (x, y, pel);
            }
        }
    }
    stat_resizetime += resizetimer();

    std::string outformat = fileformatname.empty() ? outputfilename : fileformatname;
    write_mipmap (*toplevel, dstspec, outputfilename, outformat, out_dataformat,
                  !shadowmode && !nomipmap);

    // If using update mode, stamp the output file with a modification time
    // matching that of the input file.
    if (updatemode)
        boost::filesystem::last_write_time (outputfilename, in_time);
}



static void
write_mipmap (ImageBuf &img, const ImageSpec &outspec_template,
              std::string outputfilename, std::string outformat,
              TypeDesc outputdatatype, bool mipmap)
{
    ImageSpec outspec = outspec_template;
    outspec.set_format (outputdatatype);

    // Find an ImageIO plugin that can open the output file, and open it
    Timer writetimer;
    ImageOutput *out = ImageOutput::create (outformat.c_str());
    if (! out) {
        std::cerr 
            << "maketx ERROR: Could not find an ImageIO plugin to write " 
            << outformat << " files:" << OpenImageIO::geterror() << "\n";
        exit (EXIT_FAILURE);
    }
    if (! out->supports ("tiles")) {
        std::cerr << "maketx ERROR: \"" << outputfilename
                  << "\" format does not support tiled images\n";
        exit (EXIT_FAILURE);
    }
    if (mipmap && ! out->supports ("multiimage")) {
        std::cerr << "maketx ERROR: \"" << outputfilename
                  << "\" format does not support multires images\n";
        exit (EXIT_FAILURE);
    }

    if (! mipmap && ! strcmp (out->format_name(), "openexr")) {
        // Send hint to OpenEXR driver that we won't specify a MIPmap
        outspec.attribute ("openexr:levelmode", 0 /* ONE_LEVEL */);
    }

    if (! out->open (outputfilename.c_str(), outspec)) {
        std::cerr << "maketx ERROR: Could not open \"" << outputfilename
                  << "\" : " << out->geterror() << "\n";
        exit (EXIT_FAILURE);
    }

    // Write out the image
    bool ok = true;
    ok &= img.write (out);
    stat_writetime += writetimer();

    if (mipmap) {  // Mipmap levels:
        if (verbose)
            std::cout << "  Mipmapping...\n";
        size_t pixelsize = outspec.nchannels * sizeof(float);
        float *pel = (float *) alloca (pixelsize);
        float *in0 = (float *) alloca (4 * pixelsize);
        float *in1 = in0 + outspec.nchannels;
        float *in2 = in1 + outspec.nchannels;
        float *in3 = in2 + outspec.nchannels;
        ImageBuf tmp;
        ImageBuf *big = &img, *small = &tmp;
        while (ok && (outspec.width > 1 || outspec.height > 1)) {
            Timer miptimer;
            // Resize a factor of two smaller
            ImageSpec smallspec = outspec;
            smallspec.width = big->spec().width;
            smallspec.height = big->spec().height;
            smallspec.depth = big->spec().depth;
            if (smallspec.width > 1)
                smallspec.width /= 2;
            if (smallspec.height > 1)
                smallspec.height /= 2;
            smallspec.full_width  = smallspec.width;
            smallspec.full_height = smallspec.height;
            smallspec.full_depth  = smallspec.depth;
            smallspec.set_format (TypeDesc::FLOAT);
            small->alloc (smallspec);  // Realocate with new size
            if ((smallspec.width & 1) == 0 && (smallspec.height & 1) == 0) {
                // Special case -- both dimensions are even resolutions, so
                // we can just average groups of four pixels.
                for (int y = 0;  y < smallspec.height;  ++y) {
                    for (int x = 0;  x < smallspec.width;  ++x) {
                        big->getpixel (2*x, 2*y, in0);
                        big->getpixel (2*x+1, 2*y, in1);
                        big->getpixel (2*x, 2*y+1, in2);
                        big->getpixel (2*x+1, 2*y+1, in3);
                        for (int c = 0;  c < outspec.nchannels;  ++c)
                            pel[c] = 0.25f * (in0[c] + in1[c] + in2[c] + in3[c]);
                        small->setpixel (x, y, pel);
                    }
                }
            } else {
                for (int y = 0;  y < smallspec.height;  ++y) {
                    for (int x = 0;  x < smallspec.width;  ++x) {
                        big->interppixel_NDC ((x+0.5f)/(float)smallspec.width,
                                              (y+0.5f)/(float)smallspec.height, pel);
                        small->setpixel (x, y, pel);
                    }
                }
            }
            stat_miptime += miptimer();
            outspec = smallspec;
            outspec.set_format (outputdatatype);
            Timer writetimer;
            if (! out->open (outputfilename.c_str(), outspec, true)) {
                std::cerr << "maketx ERROR: Could not append \"" << outputfilename
                          << "\" : " << out->geterror() << "\n";
                exit (EXIT_FAILURE);
            }
            ok &= small->write (out);
            stat_writetime += writetimer();
            if (verbose)
                std::cout << "    " << smallspec.width << 'x' 
                          << smallspec.height << "\n";
            std::swap (big, small);
        }
    }

    if (verbose)
        std::cout << "  Wrote file: " << outputfilename << std::endl;
    writetimer.reset ();
    writetimer.start ();
    if (ok)
        ok &= out->close ();
    stat_writetime += writetimer ();
    delete out;

    if (! ok) {
        std::cerr << "maketx ERROR writing \"" << outputfilename
                  << "\" : " << out->geterror() << "\n";
        exit (EXIT_FAILURE);
    }

}



int
main (int argc, char *argv[])
{
    Timer alltimer;
    getargs (argc, argv);

    if (mipmapmode) {
        make_texturemap ("texture map");
    } else if (shadowmode) {
        make_texturemap ("shadow map");
    } else if (shadowcubemode) {
        std::cerr << "Shadow cubes currently unsupported\n";
    } else if (volshadowmode) {
        std::cerr << "Volume shadows currently unsupported\n";
    } else if (envlatlmode) {
        std::cerr << "Latlong environment maps currently unsupported\n";
    } else if (envcubemode) {
        std::cerr << "Environment cubes currently unsupported\n";
    } else if (lightprobemode) {
        std::cerr << "Light probes currently unsupported\n";
    } else if (vertcrossmode) {
        std::cerr << "Vertcross currently unsupported\n";
    } else if (latl2envcubemode) {
        std::cerr << "Latlong->cube conversion currently unsupported\n";
    }

    if (verbose) {
        std::cout << "maketx Runtime statistics (seconds):\n";
        double alltime = alltimer();
        std::cout << Strutil::format ("  total runtime:   %5.2f\n", alltime);
        std::cout << Strutil::format ("  file read:       %5.2f\n", stat_readtime);
        std::cout << Strutil::format ("  file write:      %5.2f\n", stat_writetime);
        std::cout << Strutil::format ("  initial resize:  %5.2f\n", stat_resizetime);
        std::cout << Strutil::format ("  mip computation: %5.2f\n", stat_miptime);
        std::cout << Strutil::format ("  unaccounted:     %5.2f\n",
                                      alltime-stat_readtime-stat_writetime-stat_resizetime-stat_miptime);
        size_t kb = Sysutil::memory_used(true) / 1024;
        std::cout << Strutil::format ("maketx memory used: %5.1f MB\n",
                                      (double)kb/1024.0);
    }

    return 0;
}
