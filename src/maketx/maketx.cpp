// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>

#include <OpenImageIO/Imath.h>
#include <OpenImageIO/argparse.h>
#include <OpenImageIO/color.h>
#include <OpenImageIO/dassert.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/filter.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagecache.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/timer.h>

using namespace OIIO;

#ifndef OPENIMAGEIO_METADATA_HISTORY_DEFAULT
#    define OPENIMAGEIO_METADATA_HISTORY_DEFAULT 0
#endif


// # FIXME: Refactor all statics into a struct

// Basic runtime options
static std::string full_command_line;
static std::vector<std::string> filenames;
static std::string outputfilename;
static bool verbose  = false;
static bool runstats = false;
static int nthreads  = 0;  // default: use #cores threads if available

// Conversion modes.  If none are true, we just make an ordinary texture.
static bool mipmapmode     = false;
static bool shadowmode     = false;
static bool envlatlmode    = false;
static bool envcubemode    = false;
static bool lightprobemode = false;
static bool bumpslopesmode = false;


static std::string
filter_help_string()
{
    std::string s("Select filter for resizing (choices:");
    for (int i = 0, e = Filter2D::num_filters(); i < e; ++i) {
        FilterDesc d;
        Filter2D::get_filterdesc(i, &d);
        s.append(" ");
        s.append(d.name);
    }
    s.append(", default=box)");
    return s;
}



static std::string
colortitle_help_string()
{
    std::string s("Color Management Options ");
    if (ColorConfig::supportsOpenColorIO()) {
        s += "(OpenColorIO enabled)";
    } else {
        s += "(OpenColorIO DISABLED)";
    }
    return s;
}



static std::string
colorconvert_help_string()
{
    std::string s
        = "Apply a color space conversion to the image. "
          "If the output color space is not the same bit depth "
          "as input color space, it is your responsibility to set the data format "
          "to the proper bit depth using the -d option. ";

    s += " (choices: ";
    ColorConfig colorconfig;
    if (colorconfig.has_error() || colorconfig.getNumColorSpaces() == 0) {
        s += "NONE";
    } else {
        for (int i = 0; i < colorconfig.getNumColorSpaces(); ++i) {
            if (i != 0)
                s += ", ";
            s += colorconfig.getColorSpaceNameByIndex(i);
        }
    }
    s += ")";
    return s;
}



// Concatenate the command line into one string, optionally filtering out
// verbose attribute commands. Escape control chars in the arguments, and
// double-quote any that contain spaces.
static std::string
command_line_string(int argc, char* argv[], bool sansattrib)
{
    std::string s;
    for (int i = 0; i < argc; ++i) {
        if (sansattrib) {
            // skip any filtered attributes
            if (!strcmp(argv[i], "--attrib") || !strcmp(argv[i], "-attrib")
                || !strcmp(argv[i], "--sattrib")
                || !strcmp(argv[i], "-sattrib")) {
                i += 2;  // also skip the following arguments
                continue;
            }
            if (!strcmp(argv[i], "--sansattrib")
                || !strcmp(argv[i], "-sansattrib")) {
                continue;
            }
        }
        std::string a = Strutil::escape_chars(argv[i]);
        // If the string contains spaces
        if (a.find(' ') != std::string::npos) {
            // double quote args with spaces
            s += '\"';
            s += a;
            s += '\"';
        } else {
            s += a;
        }
        if (i < argc - 1)
            s += ' ';
    }
    return s;
}



static void
getargs(int argc, char* argv[], ImageSpec& configspec)
{
    // Basic runtime options
    std::string dataformatname = "";
    std::string fileformatname = "";
    std::vector<std::string> mipimages;
    int tile[3] = { 64, 64, 1 };  // FIXME if we ever support volume MIPmaps
    std::string compression = "zip";
    bool updatemode         = false;
    bool checknan           = false;
    std::string fixnan;  // none, black, box3
    bool set_full_to_pixels        = false;
    bool do_highlight_compensation = false;
    std::string filtername;
    // Options controlling file metadata or mipmap creation
    float fovcot     = 0.0f;
    std::string wrap = "black";
    std::string swrap;
    std::string twrap;
    bool doresize = false;
    Imath::M44f Mcam(0.0f), Mscr(0.0f), MNDC(0.0f);  // Initialize to 0
    bool separate              = false;
    bool nomipmap              = false;
    bool prman_metadata        = false;
    bool constant_color_detect = false;
    bool monochrome_detect     = false;
    bool opaque_detect         = false;
    bool compute_average       = true;
    int nchannels              = -1;
    bool prman                 = false;
    bool oiio                  = false;
    bool ignore_unassoc        = false;  // ignore unassociated alpha tags
    bool unpremult             = false;
    bool sansattrib            = false;
    float sharpen              = 0.0f;
    float uvslopes_scale       = 0.0f;
    bool cdf                   = false;
    float cdfsigma             = 1.0f / 6;
    int cdfbits                = 8;
#if OPENIMAGEIO_METADATA_HISTORY_DEFAULT
    bool metadata_history = Strutil::from_string<int>(
        getenv("OPENIMAGEIO_METADATA_HISTORY", "1"));
#else
    bool metadata_history = Strutil::from_string<int>(
        getenv("OPENIMAGEIO_METADATA_HISTORY"));
#endif
    std::string incolorspace;
    std::string outcolorspace;
    std::string colorconfigname;
    std::string channelnames;
    std::string bumpformat = "auto";
    std::string handed;
    std::vector<std::string> string_attrib_names, string_attrib_values;
    std::vector<std::string> any_attrib_names, any_attrib_values;
    filenames.clear();

    // clang-format off
    ArgParse ap;
    ap.intro("maketx -- convert images to tiled, MIP-mapped textures\n"
             OIIO_INTRO_STRING)
      .usage("maketx [options] file...")
      .add_version(OIIO_VERSION_STRING);

    ap.arg("filename")
      .hidden()
      .action([&](cspan<const char*> argv){ filenames.emplace_back(argv[0]); });
    ap.arg("-v", &verbose)
      .help("Verbose status messages");
    ap.arg("-o %s:FILENAME", &outputfilename)
      .help("Output filename");
    ap.arg("--threads %d:NUMTHREADS", &nthreads)
      .help("Number of threads (default: #cores)");
    ap.arg("-u", &updatemode)
      .help("Update mode");
    ap.arg("--format %s:FILEFORMAT", &fileformatname)
      .help("Specify output file format (default: guess from extension)");
    ap.arg("--nchannels %d:N", &nchannels)
      .help("Specify the number of output image channels.");
    ap.arg("--chnames %s:CHANNELNAMES", &channelnames)
      .help("Rename channels (comma-separated)");
    ap.arg("-d %s:TYPE", &dataformatname)
      .help("Set the output data format to one of: uint8, sint8, uint16, sint16, half, float");
    ap.arg("--tile %d:WIDTH %d:HEIGHT", &tile[0], &tile[1])
      .help("Specify tile size");
    ap.arg("--separate", &separate)
      .help("Use planarconfig separate (default: contiguous)");
    ap.arg("--compression %s:NAME", &compression)
      .help("Set the compression method (default = zip, if possible)");
    ap.arg("--fovcot %f:FOVCAT", &fovcot)
      .help("Override the frame aspect ratio. Default is width/height.");
    ap.arg("--wrap %s:WRAP", &wrap)
      .help("Specify wrap mode (black, clamp, periodic, mirror)");
    ap.arg("--swrap %s:WRAP", &swrap)
      .help("Specific s wrap mode separately");
    ap.arg("--twrap %s:WRAP", &twrap)
      .help("Specific t wrap mode separately");
    ap.arg("--resize", &doresize)
      .help("Resize textures to power of 2 (default: no)");
    ap.arg("--filter %s:FILTERNAME", &filtername)
      .help(filter_help_string());
    ap.arg("--hicomp", &do_highlight_compensation)
      .help("Compress HDR range before resize, expand after.");
    ap.arg("--sharpen %f:SHARPEN", &sharpen)
      .help("Sharpen MIP levels (default = 0.0 = no)");
    ap.arg("--nomipmap", &nomipmap)
      .help("Do not make multiple MIP-map levels");
    ap.arg("--checknan", &checknan)
      .help("Check for NaN/Inf values (abort if found)");
    ap.arg("--fixnan %s:STRATEGY", &fixnan)
      .help("Attempt to fix NaN/Inf values in the image (options: none, black, box3)");
    ap.arg("--fullpixels", &set_full_to_pixels)
      .help("Set the 'full' image range to be the pixel data window");
    ap.arg("--Mcamera %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
                          &Mcam[0][0], &Mcam[0][1], &Mcam[0][2], &Mcam[0][3],
                          &Mcam[1][0], &Mcam[1][1], &Mcam[1][2], &Mcam[1][3],
                          &Mcam[2][0], &Mcam[2][1], &Mcam[2][2], &Mcam[2][3],
                          &Mcam[3][0], &Mcam[3][1], &Mcam[3][2], &Mcam[3][3])
      .help("Set the camera matrix");
    ap.arg("--Mscreen %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
                          &Mscr[0][0], &Mscr[0][1], &Mscr[0][2], &Mscr[0][3],
                          &Mscr[1][0], &Mscr[1][1], &Mscr[1][2], &Mscr[1][3],
                          &Mscr[2][0], &Mscr[2][1], &Mscr[2][2], &Mscr[2][3],
                          &Mscr[3][0], &Mscr[3][1], &Mscr[3][2], &Mscr[3][3])
      .help("Set the screen matrix");
    ap.arg("--MNDC %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
                          &MNDC[0][0], &MNDC[0][1], &MNDC[0][2], &MNDC[0][3],
                          &MNDC[1][0], &MNDC[1][1], &MNDC[1][2], &MNDC[1][3],
                          &MNDC[2][0], &MNDC[2][1], &MNDC[2][2], &MNDC[2][3],
                          &MNDC[3][0], &MNDC[3][1], &MNDC[3][2], &MNDC[3][3])
      .help("Set the NDC matrix");
    ap.arg("--prman-metadata", &prman_metadata)
      .help("Add prman specific metadata");
    ap.arg("--attrib %L:NAME %L:VALUE", &any_attrib_names, &any_attrib_values)
      .help("Sets metadata attribute (name, value)");
    ap.arg("--sattrib %L:NAME %L:VALUE", &string_attrib_names, &string_attrib_values)
      .help("Sets string metadata attribute (name, value)");
    ap.arg("--sansattrib", &sansattrib)
      .help("Write command line into Software & ImageHistory but remove --sattrib and --attrib options");
    ap.arg("--history", &metadata_history)
      .help("Write full command line into Exif:ImageHistory, Software metadata attributes");
    ap.arg("--no-history %!", &metadata_history)
      .help("Do not write full command line into Exif:ImageHistory, Software metadata attributes");
    ap.arg("--constant-color-detect", &constant_color_detect)
      .help("Create 1-tile textures from constant color inputs");
    ap.arg("--monochrome-detect", &monochrome_detect)
      .help("Create 1-channel textures from monochrome inputs");
    ap.arg("--opaque-detect", &opaque_detect)
      .help("Drop alpha channel that is always 1.0");
    ap.arg("--no-compute-average %!", &compute_average)
      .help("Don't compute and store average color");
    ap.arg("--ignore-unassoc", &ignore_unassoc)
      .help("Ignore unassociated alpha tags in input (don't autoconvert)");
    ap.arg("--runstats", &runstats)
      .help("Print runtime statistics");
    ap.arg("--mipimage %L:FILENAME", &mipimages)
      .help("Specify an individual MIP level");
    ap.arg("--cdf", &cdf)
      .help("Store the forward and inverse Gaussian CDF as a lookup-table. The variance is set by cdfsigma (1/6 by default), and the number of buckets \
              in the lookup table is determined by cdfbits (8 bit - 256 buckets by default)");
    ap.arg("--cdfsigma %f:N", &cdfsigma)
      .help("Specify the Gaussian sigma parameter when writing the forward and inverse Gaussian CDF data. The default vale is 1/6 (0.1667)");
    ap.arg("--cdfbits %d:N", &cdfbits)
      .help("Specify the number of bits used to store the forward and inverse Gaussian CDF. The default value is 8 bits");

    ap.separator("Basic modes (default is plain texture):");
    ap.arg("--shadow", &shadowmode)
      .help("Create shadow map");
    ap.arg("--envlatl", &envlatlmode)
      .help("Create lat/long environment map");
    ap.arg("--lightprobe", &lightprobemode)
      .help("Create lat/long environment map from a light probe");
    ap.arg("--bumpslopes", &bumpslopesmode)
      .help("Create a 6 channels bump-map with height, derivatives and square derivatives from an height or a normal map");
    ap.arg("--uvslopes_scale %f:VALUE", &uvslopes_scale)
      .help("If specified, compute derivatives for --bumpslopes in UV space rather than in texel space and divide them by a scale factor. 0=disable by default, only valid for height maps.");
    ap.arg("--bumpformat %s:NAME", &bumpformat)
      .help("Specify the interpretation of a 3-channel input image for --bumpslopes: \"height\", \"normal\" or \"auto\" (default).");
//                  "--envcube", &envcubemode, "Create cubic env map (file order: px, nx, py, ny, pz, nz) (UNIMP)",
    ap.arg("--handed %s:STRING", &handed)
      .help("Specify the handedness of a vector or normal map: \"left\", \"right\", or \"\" (default).");

    ap.separator(colortitle_help_string());
    ap.arg("--colorconfig %s:FILENAME", &colorconfigname)
      .help("Explicitly specify an OCIO configuration file");
    ap.arg("--colorconvert %s:IN %s:OUT", &incolorspace, &outcolorspace)
      .help(colorconvert_help_string());
    ap.arg("--unpremult", &unpremult)
      .help("Unpremultiply before color conversion, then premultiply "
            "after the color conversion.  You'll probably want to use this flag "
            "if your image contains an alpha channel.");

    ap.separator("Configuration Presets");
    ap.arg("--prman", &prman)
      .help("Use PRMan-safe settings for tile size, planarconfig, and metadata.");
    ap.arg("--oiio", &oiio)
      .help("Use OIIO-optimized settings for tile size, planarconfig, metadata.");

    // clang-format on
    ap.parse(argc, (const char**)argv);
    if (filenames.empty()) {
        ap.briefusage();
        std::cout << "\nFor detailed help: maketx --help\n";
        exit(EXIT_SUCCESS);
    }

    int optionsum = ((int)shadowmode + (int)envlatlmode + (int)envcubemode
                     + (int)lightprobemode)
                    + (int)bumpslopesmode;
    if (optionsum > 1) {
        std::cerr
            << "maketx ERROR: At most one of the following options may be set:\n"
            << "\t--shadow --envlatl --envcube --lightprobe\n";
        exit(EXIT_FAILURE);
    }
    if (optionsum == 0)
        mipmapmode = true;

    if (prman && oiio) {
        std::cerr
            << "maketx ERROR: '--prman' compatibility, and '--oiio' optimizations are mutually exclusive.\n";
        std::cerr
            << "\tIf you'd like both prman and oiio compatibility, you should choose --prman\n";
        std::cerr << "\t(at the expense of oiio-specific optimizations)\n";
        exit(EXIT_FAILURE);
    }

    if (filenames.size() != 1) {
        std::cerr << "maketx ERROR: requires exactly one input filename\n";
        exit(EXIT_FAILURE);
    }


    //    std::cout << "Converting " << filenames[0] << " to " << outputfilename << "\n";

    // Figure out which data format we want for output
    if (!dataformatname.empty()) {
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
            std::cerr << "maketx ERROR: unknown data format \""
                      << dataformatname << "\"\n";
            exit(EXIT_FAILURE);
        }
    }

    configspec.tile_width  = tile[0];
    configspec.tile_height = tile[1];
    configspec.tile_depth  = tile[2];
    configspec.attribute("compression", compression);
    if (fovcot != 0.0f)
        configspec.attribute("fovcot", fovcot);
    configspec.attribute("planarconfig", separate ? "separate" : "contig");
    if (Mcam != Imath::M44f(0.0f))
        configspec.attribute("worldtocamera", TypeMatrix, &Mcam);
    if (Mscr != Imath::M44f(0.0f))
        configspec.attribute("worldtoscreen", TypeMatrix, &Mscr);
    if (MNDC != Imath::M44f(0.0f))
        configspec.attribute("worldtoNDC", TypeMatrix, &MNDC);
    std::string wrapmodes = (swrap.size() ? swrap : wrap) + ','
                            + (twrap.size() ? twrap : wrap);
    configspec.attribute("wrapmodes", wrapmodes);

    configspec.attribute("maketx:verbose", verbose);
    configspec.attribute("maketx:runstats", runstats);
    configspec.attribute("maketx:resize", doresize);
    configspec.attribute("maketx:nomipmap", nomipmap);
    configspec.attribute("maketx:updatemode", updatemode);
    configspec.attribute("maketx:constant_color_detect", constant_color_detect);
    configspec.attribute("maketx:monochrome_detect", monochrome_detect);
    configspec.attribute("maketx:opaque_detect", opaque_detect);
    configspec.attribute("maketx:compute_average", compute_average);
    configspec.attribute("maketx:unpremult", unpremult);
    configspec.attribute("maketx:incolorspace", incolorspace);
    configspec.attribute("maketx:outcolorspace", outcolorspace);
    configspec.attribute("maketx:colorconfig", colorconfigname);
    configspec.attribute("maketx:checknan", checknan);
    configspec.attribute("maketx:fixnan", fixnan);
    configspec.attribute("maketx:set_full_to_pixels", set_full_to_pixels);
    configspec.attribute("maketx:highlightcomp",
                         (int)do_highlight_compensation);
    configspec.attribute("maketx:sharpen", sharpen);
    if (filtername.size())
        configspec.attribute("maketx:filtername", filtername);
    configspec.attribute("maketx:nchannels", nchannels);
    configspec.attribute("maketx:channelnames", channelnames);
    if (fileformatname.size())
        configspec.attribute("maketx:fileformatname", fileformatname);
    configspec.attribute("maketx:prman_metadata", prman_metadata);
    configspec.attribute("maketx:oiio_options", oiio);
    configspec.attribute("maketx:prman_options", prman);
    if (mipimages.size())
        configspec.attribute("maketx:mipimages", Strutil::join(mipimages, ";"));
    if (bumpslopesmode)
        configspec.attribute("maketx:bumpformat", bumpformat);
    if (handed.size())
        configspec.attribute("handed", handed);
    configspec.attribute("maketx:cdf", cdf);
    configspec.attribute("maketx:cdfsigma", cdfsigma);
    configspec.attribute("maketx:cdfbits", cdfbits);

    std::string cmdline = command_line_string(argc, argv, sansattrib);
    cmdline = Strutil::fmt::format("OpenImageIO {} : {}", OIIO_VERSION_STRING,
                                   metadata_history ? cmdline
                                                    : SHA1(cmdline).digest());
    configspec.attribute("Software", cmdline);
    configspec.attribute("maketx:full_command_line", cmdline);

    // Add user-specified string attributes
    for (size_t i = 0; i < string_attrib_names.size(); ++i) {
        configspec.attribute(string_attrib_names[i], string_attrib_values[i]);
    }

    // Add user-specified "any" attributes -- try to deduce the type
    for (size_t i = 0; i < any_attrib_names.size(); ++i) {
        string_view s = any_attrib_values[i];
        // Does it parse as an int (and nothing more?)
        int ival;
        if (Strutil::parse_int(s, ival)) {
            Strutil::skip_whitespace(s);
            if (!s.size()) {
                configspec.attribute(any_attrib_names[i], ival);
                continue;
            }
        }
        s = any_attrib_values[i];
        // Does it parse as a float (and nothing more?)
        float fval;
        if (Strutil::parse_float(s, fval)) {
            Strutil::skip_whitespace(s);
            if (!s.size()) {
                configspec.attribute(any_attrib_names[i], fval);
                continue;
            }
        }
        // OK, treat it like a string
        configspec.attribute(any_attrib_names[i], any_attrib_values[i]);
    }

    if (ignore_unassoc) {
        configspec.attribute("maketx:ignore_unassoc", (int)ignore_unassoc);
        auto ic = ImageCache::create();  // get the shared one
        ic->attribute("unassociatedalpha", (int)ignore_unassoc);
    }

    if (bumpslopesmode)
        configspec.attribute("maketx:uvslopes_scale", uvslopes_scale);
}



int
main(int argc, char* argv[])
{
    Timer alltimer;

    // Helpful for debugging to make sure that any crashes dump a stack
    // trace.
    Sysutil::setup_crash_stacktrace("stdout");

    // Globally force classic "C" locale, and turn off all formatting
    // internationalization, for the entire maketx application.
    std::locale::global(std::locale::classic());

    ImageSpec configspec;
    Filesystem::convert_native_arguments(argc, (const char**)argv);
    getargs(argc, argv, configspec);

    OIIO::attribute("threads", nthreads);

    // N.B. This will apply to the default IC that any ImageBuf's get.
    auto ic = ImageCache::create();          // get the shared one
    ic->attribute("forcefloat", 1);          // Force float upon read
    ic->attribute("max_memory_MB", 1024.0);  // 1 GB cache

    ImageBufAlgo::MakeTextureMode mode = ImageBufAlgo::MakeTxTexture;
    if (shadowmode)
        mode = ImageBufAlgo::MakeTxShadow;
    if (envlatlmode)
        mode = ImageBufAlgo::MakeTxEnvLatl;
    if (lightprobemode)
        mode = ImageBufAlgo::MakeTxEnvLatlFromLightProbe;
    if (bumpslopesmode)
        mode = ImageBufAlgo::MakeTxBumpWithSlopes;

    bool ok = ImageBufAlgo::make_texture(mode, filenames[0], outputfilename,
                                         configspec, &std::cout);
    if (!ok)
        std::cout << "make_texture ERROR: " << OIIO::geterror() << "\n";
    if (runstats)
        std::cout << "\n" << ic->getstats();

    shutdown();
    return ok ? 0 : EXIT_FAILURE;
}
