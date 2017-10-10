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

#include <OpenEXR/ImathMatrix.h>

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/dassert.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/timer.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/thread.h>
#include <OpenImageIO/filter.h>

using namespace OIIO;


// # FIXME: Refactor all statics into a struct

// Basic runtime options
static std::string full_command_line;
static std::vector<std::string> filenames;
static std::string outputfilename;
static bool verbose = false;
static bool runstats = false;
static int nthreads = 0;    // default: use #cores threads if available

// Conversion modes.  If none are true, we just make an ordinary texture.
static bool mipmapmode = false;
static bool shadowmode = false;
static bool envlatlmode = false;
static bool envcubemode = false;
static bool lightprobemode = false;



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
    ColorConfig colorconfig;
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



static int
parse_files (int argc, const char *argv[])
{
    for (int i = 0;  i < argc;  i++)
        filenames.emplace_back(argv[i]);
    return 0;
}



// Concatenate the command line into one string, optionally filtering out
// verbose attribute commands.
static std::string
command_line_string (int argc, char * argv[], bool sansattrib)
{
    std::string s;
    for (int i = 0;  i < argc;  ++i) {
        if (sansattrib) {
            // skip any filtered attributes
            if (!strcmp(argv[i], "--attrib") || !strcmp(argv[i], "-attrib") ||
                !strcmp(argv[i], "--sattrib") || !strcmp(argv[i], "-sattrib")) {
                i += 2;  // also skip the following arguments
                continue;
            }
            if (!strcmp(argv[i], "--sansattrib") || !strcmp(argv[i], "-sansattrib")) {
                continue;
            }
        }
        if (strchr (argv[i], ' ')) {  // double quote args with spaces
            s += '\"';
            s += argv[i];
            s += '\"';
        } else {
            s += argv[i];
        }
        if (i < argc-1)
            s += ' ';
    }
    return s;
}



static void
getargs (int argc, char *argv[], ImageSpec &configspec)
{
    bool help = false;
    // Basic runtime options
    std::string dataformatname = "";
    std::string fileformatname = "";
    std::vector<std::string> mipimages;
    int tile[3] = { 64, 64, 1 };  // FIXME if we ever support volume MIPmaps
    std::string compression = "zip";
    bool updatemode = false;
    bool checknan = false;
    std::string fixnan; // none, black, box3
    bool set_full_to_pixels = false;
    bool do_highlight_compensation = false;
    std::string filtername;
    // Options controlling file metadata or mipmap creation
    float fovcot = 0.0f;
    std::string wrap = "black";
    std::string swrap;
    std::string twrap;
    bool doresize = false;
    Imath::M44f Mcam(0.0f), Mscr(0.0f);  // Initialize to 0
    bool separate = false;
    bool nomipmap = false;
    bool prman_metadata = false;
    bool constant_color_detect = false;
    bool monochrome_detect = false;
    bool opaque_detect = false;
    bool compute_average = true;
    int nchannels = -1;
    bool prman = false;
    bool oiio = false;
    bool ignore_unassoc = false;  // ignore unassociated alpha tags
    bool unpremult = false;
    bool sansattrib = false;
    float sharpen = 0.0f;
    std::string incolorspace;
    std::string outcolorspace;
    std::string colorconfigname;
    std::string channelnames;
    std::vector<std::string> string_attrib_names, string_attrib_values;
    std::vector<std::string> any_attrib_names, any_attrib_values;
    filenames.clear();

    ArgParse ap;
    ap.options ("maketx -- convert images to tiled, MIP-mapped textures\n"
                OIIO_INTRO_STRING "\n"
                "Usage:  maketx [options] file...",
                  "%*", parse_files, "",
                  "--help", &help, "Print help message",
                  "-v", &verbose, "Verbose status messages",
                  "-o %s", &outputfilename, "Output filename",
                  "--threads %d", &nthreads, "Number of threads (default: #cores)",
                  "-u", &updatemode, "Update mode",
                  "--format %s", &fileformatname, "Specify output file format (default: guess from extension)",
                  "--nchannels %d", &nchannels, "Specify the number of output image channels.",
                  "--chnames %s", &channelnames, "Rename channels (comma-separated)",
                  "-d %s", &dataformatname, "Set the output data format to one of: "
                          "uint8, sint8, uint16, sint16, half, float",
                  "--tile %d %d", &tile[0], &tile[1], "Specify tile size",
                  "--separate", &separate, "Use planarconfig separate (default: contiguous)",
                  "--compression %s", &compression, "Set the compression method (default = zip, if possible)",
                  "--fovcot %f", &fovcot, "Override the frame aspect ratio. Default is width/height.",
                  "--wrap %s", &wrap, "Specify wrap mode (black, clamp, periodic, mirror)",
                  "--swrap %s", &swrap, "Specific s wrap mode separately",
                  "--twrap %s", &twrap, "Specific t wrap mode separately",
                  "--resize", &doresize, "Resize textures to power of 2 (default: no)",
                  "--noresize %!", &doresize, "Do not resize textures to power of 2 (deprecated)",
                  "--filter %s", &filtername, filter_help_string().c_str(),
                  "--hicomp", &do_highlight_compensation,
                          "Compress HDR range before resize, expand after.",
                  "--sharpen %f", &sharpen, "Sharpen MIP levels (default = 0.0 = no)",
                  "--nomipmap", &nomipmap, "Do not make multiple MIP-map levels",
                  "--checknan", &checknan, "Check for NaN/Inf values (abort if found)",
                  "--fixnan %s", &fixnan, "Attempt to fix NaN/Inf values in the image (options: none, black, box3)",
                  "--fullpixels", &set_full_to_pixels, "Set the 'full' image range to be the pixel data window",
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
                          "Set the screen matrix",
                  "--prman-metadata", &prman_metadata, "Add prman specific metadata",
                  "--attrib %L %L", &any_attrib_names, &any_attrib_values, "Sets metadata attribute (name, value)",
                  "--sattrib %L %L", &string_attrib_names, &string_attrib_values, "Sets string metadata attribute (name, value)",
                  "--sansattrib", &sansattrib, "Write command line into Software & ImageHistory but remove --sattrib and --attrib options",
                  "--constant-color-detect", &constant_color_detect, "Create 1-tile textures from constant color inputs",
                  "--monochrome-detect", &monochrome_detect, "Create 1-channel textures from monochrome inputs",
                  "--opaque-detect", &opaque_detect, "Drop alpha channel that is always 1.0",
                  "--no-compute-average %!", &compute_average, "Don't compute and store average color",
                  "--ignore-unassoc", &ignore_unassoc, "Ignore unassociated alpha tags in input (don't autoconvert)",
                  "--runstats", &runstats, "Print runtime statistics",
                  "--stats", &runstats, "", // DEPRECATED 1.6
                  "--mipimage %L", &mipimages, "Specify an individual MIP level",
                  "<SEPARATOR>", "Basic modes (default is plain texture):",
                  "--shadow", &shadowmode, "Create shadow map",
                  "--envlatl", &envlatlmode, "Create lat/long environment map",
                  "--lightprobe", &lightprobemode, "Create lat/long environment map from a light probe",
//                  "--envcube", &envcubemode, "Create cubic env map (file order: px, nx, py, ny, pz, nz) (UNIMP)",
                  "<SEPARATOR>", colortitle_help_string().c_str(),
                  "--colorconfig %s", &colorconfigname, "Explicitly specify an OCIO configuration file",
                  "--colorconvert %s %s", &incolorspace, &outcolorspace,
                          colorconvert_help_string().c_str(),
                  "--unpremult", &unpremult, "Unpremultiply before color conversion, then premultiply "
                          "after the color conversion.  You'll probably want to use this flag "
                          "if your image contains an alpha channel.",
                  "<SEPARATOR>", "Configuration Presets",
                  "--prman", &prman, "Use PRMan-safe settings for tile size, planarconfig, and metadata.",
                  "--oiio", &oiio, "Use OIIO-optimized settings for tile size, planarconfig, metadata.",
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
    if (filenames.empty()) {
        ap.briefusage ();
        std::cout << "\nFor detailed help: maketx --help\n";
        exit (EXIT_SUCCESS);
    }

    int optionsum = ((int)shadowmode + (int)envlatlmode + (int)envcubemode +
                     (int)lightprobemode);
    if (optionsum > 1) {
        std::cerr << "maketx ERROR: At most one of the following options may be set:\n"
                  << "\t--shadow --envlatl --envcube --lightprobe\n";
        exit (EXIT_FAILURE);
    }
    if (optionsum == 0)
        mipmapmode = true;
    
    if (prman && oiio) {
        std::cerr << "maketx ERROR: '--prman' compatibility, and '--oiio' optimizations are mutually exclusive.\n";
        std::cerr << "\tIf you'd like both prman and oiio compatibility, you should choose --prman\n";
        std::cerr << "\t(at the expense of oiio-specific optimizations)\n";
        exit (EXIT_FAILURE);
    }

    if (filenames.size() != 1) {
        std::cerr << "maketx ERROR: requires exactly one input filename\n";
        exit (EXIT_FAILURE);
    }


//    std::cout << "Converting " << filenames[0] << " to " << outputfilename << "\n";

    // Figure out which data format we want for output
    if (! dataformatname.empty()) {
        if (dataformatname == "uint8")
            configspec.format = TypeDesc::UINT8;
        else if (dataformatname == "int8" || dataformatname == "sint8")
            configspec.format = TypeDesc::INT8;
        else if (dataformatname == "uint16")
            configspec.format = TypeDesc::UINT16;
        else if (dataformatname == "int16" || dataformatname == "sint16")
            configspec.format = TypeDesc::INT16;
        else if (dataformatname == "half")
            configspec.format = TypeDesc::HALF;
        else if (dataformatname == "float")
            configspec.format = TypeDesc::FLOAT;
        else if (dataformatname == "double")
            configspec.format = TypeDesc::DOUBLE;
        else {
            std::cerr << "maketx ERROR: unknown data format \"" << dataformatname << "\"\n";
            exit (EXIT_FAILURE);
        }
    }

    configspec.tile_width  = tile[0];
    configspec.tile_height = tile[1];
    configspec.tile_depth  = tile[2];
    configspec.attribute ("compression", compression);
    if (fovcot != 0.0f)
        configspec.attribute ("fovcot", fovcot);
    configspec.attribute ("planarconfig", separate ? "separate" : "contig");
    if (Mcam != Imath::M44f(0.0f))
        configspec.attribute ("worldtocamera", TypeMatrix, &Mcam);
    if (Mscr != Imath::M44f(0.0f))
        configspec.attribute ("worldtoscreen", TypeMatrix, &Mscr);
    std::string wrapmodes = (swrap.size() ? swrap : wrap) + ',' + 
                            (twrap.size() ? twrap : wrap);
    configspec.attribute ("wrapmodes", wrapmodes);

    configspec.attribute ("maketx:verbose", verbose);
    configspec.attribute ("maketx:runstats", runstats);
    configspec.attribute ("maketx:resize", doresize);
    configspec.attribute ("maketx:nomipmap", nomipmap);
    configspec.attribute ("maketx:updatemode", updatemode);
    configspec.attribute ("maketx:constant_color_detect", constant_color_detect);
    configspec.attribute ("maketx:monochrome_detect", monochrome_detect);
    configspec.attribute ("maketx:opaque_detect", opaque_detect);
    configspec.attribute ("maketx:compute_average", compute_average);
    configspec.attribute ("maketx:unpremult", unpremult);
    configspec.attribute ("maketx:incolorspace", incolorspace);
    configspec.attribute ("maketx:outcolorspace", outcolorspace);
    configspec.attribute ("maketx:colorconfig", colorconfigname);
    configspec.attribute ("maketx:checknan", checknan);
    configspec.attribute ("maketx:fixnan", fixnan);
    configspec.attribute ("maketx:set_full_to_pixels", set_full_to_pixels);
    configspec.attribute ("maketx:highlightcomp", (int)do_highlight_compensation);
    configspec.attribute ("maketx:sharpen", sharpen);
    if (filtername.size())
        configspec.attribute ("maketx:filtername", filtername);
    configspec.attribute ("maketx:nchannels", nchannels);
    configspec.attribute ("maketx:channelnames", channelnames);
    if (fileformatname.size())
        configspec.attribute ("maketx:fileformatname", fileformatname);
    configspec.attribute ("maketx:prman_metadata", prman_metadata);
    configspec.attribute ("maketx:oiio_options", oiio);
    configspec.attribute ("maketx:prman_options", prman);
    if (mipimages.size())
        configspec.attribute ("maketx:mipimages", Strutil::join(mipimages,";"));

    std::string cmdline = Strutil::format ("OpenImageIO %s : %s",
                                     OIIO_VERSION_STRING,
                                     command_line_string (argc, argv, sansattrib));
    configspec.attribute ("Software", cmdline);
    configspec.attribute ("maketx:full_command_line", cmdline);

    // Add user-specified string attributes
    for (size_t i = 0; i < string_attrib_names.size(); ++i) {
        configspec.attribute (string_attrib_names[i], string_attrib_values[i]);
    }

    // Add user-specified "any" attributes -- try to deduce the type
    for (size_t i = 0; i < any_attrib_names.size(); ++i) {
        string_view s = any_attrib_values[i];
        // Does it parse as an int (and nothing more?)
        int ival;
        if (Strutil::parse_int(s,ival)) {
            Strutil::skip_whitespace(s);
            if (! s.size()) {
                configspec.attribute (any_attrib_names[i], ival);
                continue;
            }
        }
        s = any_attrib_values[i];
        // Does it parse as a float (and nothing more?)
        float fval;
        if (Strutil::parse_float(s,fval)) {
            Strutil::skip_whitespace(s);
            if (! s.size()) {
                configspec.attribute (any_attrib_names[i], fval);
                continue;
            }
        }
        // OK, treat it like a string
        configspec.attribute (any_attrib_names[i], any_attrib_values[i]);
    }

    if (ignore_unassoc) {
        configspec.attribute ("maketx:ignore_unassoc", (int)ignore_unassoc);
        ImageCache *ic = ImageCache::create ();  // get the shared one
        ic->attribute ("unassociatedalpha", (int)ignore_unassoc);
    }
}




int
main (int argc, char *argv[])
{
    Timer alltimer;

    // Globally force classic "C" locale, and turn off all formatting
    // internationalization, for the entire maketx application.
    std::locale::global (std::locale::classic());

    ImageSpec configspec;
    Filesystem::convert_native_arguments (argc, (const char **)argv);
    getargs (argc, argv, configspec);

    OIIO::attribute ("threads", nthreads);

    // N.B. This will apply to the default IC that any ImageBuf's get.
    ImageCache *ic = ImageCache::create ();  // get the shared one
    ic->attribute ("forcefloat", 1);   // Force float upon read
    ic->attribute ("max_memory_MB", 1024.0);  // 1 GB cache

    ImageBufAlgo::MakeTextureMode mode = ImageBufAlgo::MakeTxTexture;
    if (shadowmode)
        mode = ImageBufAlgo::MakeTxShadow;
    if (envlatlmode)
        mode = ImageBufAlgo::MakeTxEnvLatl;
    if (lightprobemode)
        mode = ImageBufAlgo::MakeTxEnvLatlFromLightProbe;
    bool ok = ImageBufAlgo::make_texture (mode, filenames[0],
                                          outputfilename, configspec,
                                          &std::cout);
    if (runstats)
        std::cout << "\n" << ic->getstats();

    return ok ? 0 : EXIT_FAILURE;
}
