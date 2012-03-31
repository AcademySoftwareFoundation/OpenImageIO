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
#include "thread.h"
#include "filter.h"

OIIO_NAMESPACE_USING


// # FIXME: Refactor all statics into a struct

// Basic runtime options
static std::string full_command_line;
static std::vector<std::string> filenames;
static std::string outputfilename;
static std::string dataformatname = "";
static std::string fileformatname = "";
static std::vector<std::string> mipimages;
//static float ingamma = 1.0f, outgamma = 1.0f;
static bool verbose = false;
static bool stats = false;
static int nthreads = 0;    // default: use #cores threads if available
static int tile[3] = { 64, 64, 1 };
static std::string channellist;
static bool updatemode = false;
static double stat_readtime = 0;
static double stat_writetime = 0;
static double stat_resizetime = 0;
static double stat_miptime = 0;
static double stat_colorconverttime = 0;
static bool checknan = false;
static std::string fixnan = "none"; // none, black, box3
static int found_nonfinite = 0;
static spin_mutex maketx_mutex;   // for anything that needs locking
static std::string filtername = "box";
static Filter2D *filter = NULL;

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
static float fovcot = 0.0f;
static std::string wrap = "black";
static std::string swrap;
static std::string twrap;
static bool doresize = false;
//static float opaquewidth = 0;  // should be volume shadow epsilon
static Imath::M44f Mcam(0.0f), Mscr(0.0f);  // Initialize to 0
static bool separate = false;
static bool nomipmap = false;
static bool embed_hash = false; // Ignored.
static bool prman_metadata = false;
static bool constant_color_detect = false;
static bool monochrome_detect = false;
static bool opaque_detect = false;
static int nchannels = -1;
static bool prman = false;
static bool oiio = false;
static bool src_samples_border = false; // are src edge samples on the border?

static bool unpremult = false;
static std::string incolorspace;
static std::string outcolorspace;
static ColorConfig colorconfig;


// forward decl
static void write_mipmap (ImageBuf &img, const ImageSpec &outspec_template,
                          std::string outputfilename, ImageOutput *out,
                          TypeDesc outputdatatype, bool mipmap);



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
                  "-v", &verbose, "Verbose status messages",
                  "-o %s", &outputfilename, "Output filename",
                  "--threads %d", &nthreads, "Number of threads (default: #cores)",
                  "-u", &updatemode, "Update mode",
                  "--format %s", &fileformatname, "Specify output file format (default: guess from extension)",
                  "--nchannels %d", &nchannels, "Specify the number of output image channels.",
                  "-d %s", &dataformatname, "Set the output data format to one of: "
                          "uint8, sint8, uint16, sint16, half, float",
                  "--tile %d %d", &tile[0], &tile[1], "Specify tile size",
                  "--separate", &separate, "Use planarconfig separate (default: contiguous)",
//                  "--ingamma %f", &ingamma, "Specify gamma of input files (default: 1)",
//                  "--outgamma %f", &outgamma, "Specify gamma of output files (default: 1)",
//                  "--opaquewidth %f", &opaquewidth, "Set z fudge factor for volume shadows",
                  "--fov %f", &fov, "Field of view for envcube/shadcube/twofish",
                  "--fovcot %f", &fovcot, "Override the frame aspect ratio. Default is width/height.",
                  "--wrap %s", &wrap, "Specify wrap mode (black, clamp, periodic, mirror)",
                  "--swrap %s", &swrap, "Specific s wrap mode separately",
                  "--twrap %s", &twrap, "Specific t wrap mode separately",
                  "--resize", &doresize, "Resize textures to power of 2 (default: no)",
                  "--noresize %!", &doresize, "Do not resize textures to power of 2 (deprecated)",
                  "--filter %s", &filtername, filter_help_string().c_str(),
                  "--nomipmap", &nomipmap, "Do not make multiple MIP-map levels",
                  "--checknan", &checknan, "Check for NaN/Inf values (abort if found)",
                  "--fixnan %s", &fixnan, "Attempt to fix NaN/Inf values in the image (options: none, black, box3)",
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
                  "--prman-metadata", &prman_metadata, "Add prman specific metadata",
                  "--constant-color-detect", &constant_color_detect, "Create 1-tile textures from constant color inputs",
                  "--monochrome-detect", &monochrome_detect, "Create 1-channel textures from monochrome inputs",
                  "--opaque-detect", &opaque_detect, "Drop alpha channel that is always 1.0",
                  "--stats", &stats, "Print runtime statistics",
                  "--mipimage %L", &mipimages, "Specify an individual MIP level",
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
                  "--colorconvert %s %s", &incolorspace, &outcolorspace,
                          colorconvert_help_string().c_str(),
                  "--unpremult", &unpremult, "Unpremultiply before color conversion, then premultiply "
                          "after the color conversion.  You'll probably want to use this flag "
                          "if your image contains an alpha channel.",
                  "<SEPARATOR>", "Configuration Presets",
                  "--prman", &prman, "Use PRMan-safe settings for tile size, planarconfig, and metadata.",
                  "--oiio", &oiio, "Use OIIO-optimized settings for tile size, planarconfig, metadata, and constant-color optimizations.",
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
    
    if (prman && oiio) {
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

    filter = setup_filter (filtername);
    if (! filter) {
        std::cerr << "maketx ERROR: could not make filter '" << filtername << "\n";
        exit (EXIT_FAILURE);
    }

    if (embed_hash && verbose) {
        std::cerr << "maketx WARNING: The --embed_hash option is deprecated, and no longer necessary.\n";
        std::cerr << "                 (Hashes are always computed.)\n";
    }
    
//    std::cout << "Converting " << filenames[0] << " to " << outputfilename << "\n";
}



static TypeDesc
set_prman_options(TypeDesc out_dataformat)
{
    // Force planar image handling, and also emit prman metadata
    separate = true;
    prman_metadata = true;
    
    // 8-bit : 64x64
    if (out_dataformat == TypeDesc::UINT8 ||
        out_dataformat == TypeDesc::INT8) {
        tile[0] = 64;
        tile[1] = 64;
    }
    
    // 16-bit : 64x32
    // Force u16 -> s16
    // In prman's txmake (last tested in 15.0)
    // specifying -short creates a signed int representation
    if (out_dataformat == TypeDesc::UINT16) {
        out_dataformat = TypeDesc::INT16;
    }
    
    if (out_dataformat == TypeDesc::UINT16 ||
        out_dataformat == TypeDesc::INT16) {
        tile[0] = 64;
        tile[1] = 32;
    }
    
    // Float: 32x32
    // In prman's txmake (last tested in 15.0)
    // specifying -half or -float make 32x32 tile size
    if (out_dataformat == TypeDesc::HALF ||
        out_dataformat == TypeDesc::FLOAT ||
        out_dataformat == TypeDesc::DOUBLE) {
        tile[0] = 32;
        tile[1] = 32;
    }
    
    return out_dataformat;
}



static TypeDesc
set_oiio_options(TypeDesc out_dataformat)
{
    // Interleaved channels are faster to read
    separate = false;
    
    // Enable constant color optimizations
    constant_color_detect = true;
    
    // Force fixed tile-size across the board
    tile[0] = 64;
    tile[1] = 64;
    
    return out_dataformat;
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



// Run func over all pixels of dst, but split into separate threads for
// bands of the image.  Assumes that the calling profile of func is:
//     func (dst, src, xbegin, xend, ybegin, yend);
// Also assumes that every pixel processed is approximately the same
// cost, so it just divides the image space into equal-sized bands without
// worrying about any sophisticated load balancing.
template <class Func>
void
parallel_image (Func func, ImageBuf *dst, const ImageBuf *src, 
                int xbegin, int xend, int ybegin, int yend, int nthreads)
{
    const ImageSpec &dstspec (dst->spec());

    // Don't parallelize with too few pixels
    if (dstspec.image_pixels() < 1000)
        nthreads = 1;
    // nthreads < 1 means try to make enough threads to fill all cores
    if (nthreads < 1) {
#if (BOOST_VERSION >= 103500)
        nthreads = boost::thread::hardware_concurrency();
#else
        nthreads = 1;   // hardware_concurrency not supported in Boost < 1.35
#endif
    }

#if (BOOST_VERSION >= 103500)
    if (nthreads > 1) {
        boost::thread_group threads;
        int blocksize = std::max (1, ((xend-xbegin) + nthreads-1) / nthreads);
        for (int i = 0;  i < nthreads;  ++i) {
            int x0 = xbegin + i*blocksize;
            int x1 = std::min (xbegin + (i+1)*blocksize, xend);
//            std::cerr << "  launching " << x0 << ' ' << x1 << ' '
//                      << ybegin << ' ' << yend << "\n";
            threads.add_thread (new boost::thread (func, dst, src,
                                                   x0, x1,
                                                   ybegin, yend));
        }
        threads.join_all ();
    } else {
#endif // BOOST_VERSION
        func (dst, src, xbegin, xend, ybegin, yend);
#if (BOOST_VERSION >= 103500)
    }
#endif
}



// Copy src into dst, but only for the range [x0,x1) x [y0,y1).
static void
copy_block (ImageBuf *dst, const ImageBuf *src,
            int x0, int x1, int y0, int y1)
{
    const ImageSpec &dstspec (dst->spec());
    float *pel = (float *) alloca (dstspec.pixel_bytes());
    for (int y = y0;  y < y1;  ++y) {
        for (int x = x0;  x < x1;  ++x) {
            src->getpixel (x, y, pel);
            dst->setpixel (x, y, pel);
        }
    }
}



// Resize src into dst using a good quality filter, 
// for the pixel range [x0,x1) x [y0,y1).
static void
resize_block_HQ (ImageBuf *dst, const ImageBuf *src,
                 int x0, int x1, int y0, int y1)
{
    ImageBufAlgo::resize (*dst, *src, x0, x1, y0, y1, filter);
}



static void
interppixel_NDC_clamped (const ImageBuf &buf, float x, float y, float *pixel)
{

    int fx = buf.spec().full_x;
    int fy = buf.spec().full_y;
    int fw = buf.spec().full_width;
    int fh = buf.spec().full_height;
    x = static_cast<float>(fx) + x * static_cast<float>(fw);
    y = static_cast<float>(fy) + y * static_cast<float>(fh);

    const int maxchannels = 64;  // Reasonable guess
    float p[4][maxchannels];
    DASSERT (buf.spec().nchannels <= maxchannels && 
             "You need to increase maxchannels");
    int n = std::min (buf.spec().nchannels, maxchannels);
    x -= 0.5f;
    y -= 0.5f;
    int xtexel, ytexel;
    float xfrac, yfrac;
    xfrac = floorfrac (x, &xtexel);
    yfrac = floorfrac (y, &ytexel);
    // Clamp
    int xnext = Imath::clamp (xtexel+1, buf.xmin(), buf.xmax());
    int ynext = Imath::clamp (ytexel+1, buf.ymin(), buf.ymax());
    xtexel = std::max (xtexel, buf.xmin());
    ytexel = std::max (ytexel, buf.ymin());
    // Get the four texels
    buf.getpixel (xtexel, ytexel, p[0], n);
    buf.getpixel (xnext, ytexel, p[1], n);
    buf.getpixel (xtexel, ynext, p[2], n);
    buf.getpixel (xnext, ynext, p[3], n);
    if (envlatlmode) {
        // For latlong environment maps, in order to conserve energy, we
        // must weight the pixels by sin(t*PI) because pixels closer to
        // the pole are actually less area on the sphere. Doing this
        // wrong will tend to over-represent the high latitudes in
        // low-res MIP levels.  We fold the area weighting into our
        // linear interpolation by adjusting yfrac.
        float w0 = (1.0f - yfrac) * sinf ((float)M_PI * (ytexel+0.5f)/(float)fh);
        float w1 = yfrac * sinf ((float)M_PI * (ynext+0.5f)/(float)fh);
        yfrac = w0 / (w0 + w1);
    }
    // Bilinearly interpolate
    bilerp (p[0], p[1], p[2], p[3], xfrac, yfrac, n, pixel);
}



// Resize src into dst, relying on the linear interpolation of
// interppixel_NDC_full, for the pixel range [x0,x1) x [y0,y1).
static void
resize_block (ImageBuf *dst, const ImageBuf *src,
              int x0, int x1, int y0, int y1)
{
    const ImageSpec &dstspec (dst->spec());
    float *pel = (float *) alloca (dstspec.pixel_bytes());
    float xoffset = dstspec.full_x;
    float yoffset = dstspec.full_y;
    float xscale = 1.0f / (float)dstspec.full_width;
    float yscale = 1.0f / (float)dstspec.full_height;
    for (int y = y0;  y < y1;  ++y) {
        float t = (y+0.5f)*yscale + yoffset;
        for (int x = x0;  x < x1;  ++x) {
            float s = (x+0.5f)*xscale + xoffset;
            interppixel_NDC_clamped (*src, s, t, pel);
            dst->setpixel (x, y, pel);
        }
    }
}



// Copy src into dst, but only for the range [x0,x1) x [y0,y1).
static void
check_nan_block (ImageBuf* /*dst*/, const ImageBuf* src,
                 int x0, int x1, int y0, int y1)
{
    const ImageSpec &spec (src->spec());
    float *pel = (float *) alloca (spec.pixel_bytes());
    for (int y = y0;  y < y1;  ++y) {
        for (int x = x0;  x < x1;  ++x) {
            src->getpixel (x, y, pel);
            for (int c = 0;  c < spec.nchannels;  ++c) {
                if (! isfinite(pel[c])) {
                    spin_lock lock (maketx_mutex);
                    if (found_nonfinite < 3)
                        std::cerr << "maketx ERROR: Found " << pel[c] 
                                  << " at (x=" << x << ", y=" << y << ")\n";
                    ++found_nonfinite;
                    break;  // skip other channels, there's no point
                }
            }
        }
    }
}



static void
fix_latl_edges (ImageBuf &buf)
{
    ASSERT (envlatlmode && "only call fix_latl_edges for latlong maps");
    int n = buf.nchannels();
    float *left = ALLOCA (float, n);
    float *right = ALLOCA (float, n);

    // Make the whole first and last row be solid, since they are exactly
    // on the pole
    float wscale = 1.0f / (buf.spec().width);
    for (int j = 0;  j <= 1;  ++j) {
        int y = (j==0) ? buf.ybegin() : buf.yend()-1;
        // use left for the sum, right for each new pixel
        for (int c = 0;  c < n;  ++c)
            left[c] = 0.0f;
        for (int x = buf.xbegin();  x < buf.xend();  ++x) {
            buf.getpixel (x, y, right);
            for (int c = 0;  c < n;  ++c)
                left[c] += right[c];
        }
        for (int c = 0;  c < n;  ++c)
            left[c] += right[c];
        for (int c = 0;  c < n;  ++c)
            left[c] *= wscale;
        for (int x = buf.xbegin();  x < buf.xend();  ++x)
            buf.setpixel (x, y, left);
    }

    // Make the left and right match, since they are both right on the
    // prime meridian.
    for (int y = buf.ybegin();  y < buf.yend();  ++y) {
        buf.getpixel (buf.xbegin(), y, left);
        buf.getpixel (buf.xend()-1, y, right);
        for (int c = 0;  c < n;  ++c)
            left[c] = 0.5f * left[c] + 0.5f * right[c];
        buf.setpixel (buf.xbegin(), y, left);
        buf.setpixel (buf.xend()-1, y, left);
    }
}



static std::string
formatres (const ImageSpec &spec, bool extended=false)
{
    std::string s;
    s = Strutil::format("%dx%d", spec.width, spec.height);
    if (extended) {
        if (spec.x || spec.y)
            s += Strutil::format("%+d%+d", spec.x, spec.y);
        if (spec.width != spec.full_width || spec.height != spec.full_height ||
            spec.x != spec.full_x || spec.y != spec.full_y) {
            s += " (full/display window is ";
            s += Strutil::format("%dx%d", spec.full_width, spec.full_height);
            if (spec.full_x || spec.full_y)
                s += Strutil::format("%+d%+d", spec.full_x, spec.full_y);
            s += ")";
        }
    }
    return s;
}



static void
make_texturemap (const char *maptypename = "texture map")
{
    if (filenames.size() != 1) {
        std::cerr << "maketx ERROR: " << maptypename 
                  << " requires exactly one input filename\n";
        exit (EXIT_FAILURE);
    }

    if (! Filesystem::exists (filenames[0])) {
        std::cerr << "maketx ERROR: \"" << filenames[0] << "\" does not exist\n";
        exit (EXIT_FAILURE);
    }
    if (outputfilename.empty()) 
        outputfilename = Filesystem::replace_extension (filenames[0], ".tx");

    // When was the input file last modified?
    std::time_t in_time = boost::filesystem::last_write_time (filenames[0]);

    // When in update mode, skip making the texture if the output already
    // exists and has the same file modification time as the input file.
    if (updatemode && Filesystem::exists (outputfilename) &&
        (in_time == boost::filesystem::last_write_time (outputfilename))) {
        std::cout << "maketx: no update required for \"" 
                  << outputfilename << "\"\n";
        return;
    }


    // Find an ImageIO plugin that can open the output file, and open it
    std::string outformat = fileformatname.empty() ? outputfilename : fileformatname;
    ImageOutput *out = ImageOutput::create (outformat.c_str());
    if (! out) {
        std::cerr 
            << "maketx ERROR: Could not find an ImageIO plugin to write " 
            << outformat << " files:" << geterror() << "\n";
        exit (EXIT_FAILURE);
    }
    if (! out->supports ("tiles")) {
        std::cerr << "maketx ERROR: \"" << outputfilename
                  << "\" format does not support tiled images\n";
        exit (EXIT_FAILURE);
    }

    ImageBuf src (filenames[0]);
    src.init_spec (filenames[0], 0, 0); // force it to get the spec, not read

    // The cache might mess with the apparent data format.  But for the 
    // purposes of what we should output, figure it out now, before the
    // file has been read and cached.
    TypeDesc out_dataformat = src.spec().format;

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
    
    
    // We cannot compute the prman / oiio options until after out_dataformat
    // has been determined, as it's required (and can potentially change 
    // out_dataformat too!)
    
    if (prman) out_dataformat = set_prman_options (out_dataformat);
    else if (oiio) out_dataformat = set_oiio_options (out_dataformat);
    
    // Read the full file locally if it's less than 1 GB, otherwise
    // allow the ImageBuf to use ImageCache to manage memory.
    bool read_local = (src.spec().image_bytes() < size_t(1024*1024*1024));

    if (verbose)
        std::cout << "Reading file: " << filenames[0] << std::endl;
    Timer readtimer;
    if (! src.read (0, 0, read_local)) {
        std::cerr 
            << "maketx ERROR: Could not read \"" 
            << filenames[0] << "\" : " << src.geterror() << "\n";
        exit (EXIT_FAILURE);
    }
    stat_readtime += readtimer();
    
    // If requested - and we're a constant color - make a tiny texture instead
    std::vector<float> constantColor(src.nchannels());
    bool isConstantColor = ImageBufAlgo::isConstantColor (src, &constantColor[0]);
    
    if (isConstantColor && constant_color_detect) {
        ImageSpec newspec = src.spec();
        
        // Reset the image, to a new image, at the new size
        std::string name = src.name() + ".constant_color";
        src.reset(name, newspec);
        
        ImageBufAlgo::fill (src, &constantColor[0]);
        
        if (verbose) {
            std::cout << "  Constant color image detected. ";
            std::cout << "Creating " << newspec.width << "x" << newspec.height << " texture instead.\n";
        }
    }
    
    // If requested -- and alpha is 1.0 everywhere -- drop it.
    if (opaque_detect && src.spec().alpha_channel == src.nchannels()-1 &&
          nchannels < 0 &&
          ImageBufAlgo::isConstantChannel(src,src.spec().alpha_channel,1.0f)) {
        ImageBuf newsrc(src.name() + ".noalpha", src.spec());
        ImageBufAlgo::setNumChannels (newsrc, src, src.nchannels()-1);
        src = newsrc;
        if (verbose) {
            std::cout << "  Alpha==1 image detected. Dropping the alpha channel.\n";
        }
    }

    // If requested - and we're a monochrome image - drop the extra channels
    if (monochrome_detect && (src.nchannels() > 1) && nchannels < 0 &&
            ImageBufAlgo::isMonochrome(src)) {
        ImageBuf newsrc(src.name() + ".monochrome", src.spec());
        ImageBufAlgo::setNumChannels (newsrc, src, 1);
        src = newsrc;
        if (verbose) {
            std::cout << "  Monochrome image detected. Converting to single channel texture.\n";
        }
    }

    // If we've otherwise explicitly requested to write out a
    // specific number of channels, do it.
    if ((nchannels > 0) && (nchannels != src.nchannels())) {
        ImageBuf newsrc(src.name() + ".channels", src.spec());
        ImageBufAlgo::setNumChannels (newsrc, src, nchannels);
        src = newsrc;
        if (verbose) {
            std::cout << "  Overriding number of channels to " << nchannels << "\n";
        }
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
    bool orig_was_volume = srcspec.depth > 1 || srcspec.full_depth > 1;
    bool orig_was_crop = (srcspec.x > srcspec.full_x ||
                          srcspec.y > srcspec.full_y ||
                          srcspec.z > srcspec.full_z ||
                          srcspec.x+srcspec.width < srcspec.full_x+srcspec.full_width ||
                          srcspec.y+srcspec.height < srcspec.full_y+srcspec.full_height ||
                          srcspec.z+srcspec.depth < srcspec.full_z+srcspec.full_depth);
    bool orig_was_overscan = (srcspec.x < srcspec.full_x &&
                              srcspec.y < srcspec.full_y &&
                              srcspec.x+srcspec.width > srcspec.full_x+srcspec.full_width &&
                              srcspec.y+srcspec.height > srcspec.full_y+srcspec.full_height &&
                              (!orig_was_volume || (srcspec.z < srcspec.full_z &&
                                                    srcspec.z+srcspec.depth > srcspec.full_z+srcspec.full_depth)));
    // Make the output not a crop window
    if (orig_was_crop) {
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
    }
    if (orig_was_overscan) {
        swrap = "black";
        twrap = "black";
    }

    if ((dstspec.x < 0 || dstspec.y < 0 || dstspec.z < 0) &&
        (out && !out->supports("negativeorigin"))) {
        // User passed negative origin but the output format doesn't
        // support it.  Try to salvage the situation by shifting the
        // image into the positive range.
        if (dstspec.x < 0) {
            dstspec.full_x -= dstspec.x;
            dstspec.x = 0;
        }
        if (dstspec.y < 0) {
            dstspec.full_y -= dstspec.y;
            dstspec.y = 0;
        }
        if (dstspec.z < 0) {
            dstspec.full_z -= dstspec.z;
            dstspec.z = 0;
        }
    }

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
    
    if (shadowmode) {
        dstspec.attribute ("textureformat", "Shadow");
        if (prman_metadata)
            dstspec.attribute ("PixarTextureFormat", "Shadow");
    } else if (envlatlmode) {
        dstspec.attribute ("textureformat", "LatLong Environment");
        swrap = "periodic";
        twrap = "clamp";
        if (prman_metadata)
            dstspec.attribute ("PixarTextureFormat", "Latlong Environment");
    } else {
        dstspec.attribute ("textureformat", "Plain Texture");
        if(prman_metadata)
            dstspec.attribute ("PixarTextureFormat", "Plain Texture");
    }

    if (Mcam != Imath::M44f(0.0f))
        dstspec.attribute ("worldtocamera", TypeDesc::TypeMatrix, &Mcam);
    if (Mscr != Imath::M44f(0.0f))
        dstspec.attribute ("worldtoscreen", TypeDesc::TypeMatrix, &Mscr);

    // FIXME - check for valid strings in the wrap mode
    if (! shadowmode) {
        std::string wrapmodes = (swrap.size() ? swrap : wrap) + ',' + 
                                (twrap.size() ? twrap : wrap);
        dstspec.attribute ("wrapmodes", wrapmodes);
    }
    
    if(fovcot == 0.0f) {
        fovcot = static_cast<float>(srcspec.full_width) / 
            static_cast<float>(srcspec.full_height);
    }
    dstspec.attribute ("fovcot", fovcot);

    if (separate)
        dstspec.attribute ("planarconfig", "separate");
    else {
        dstspec.erase_attribute("planarconfig");
        dstspec.erase_attribute("tiff:planarconfig");
    }
    // FIXME -- should we allow tile sizes to reduce if the image is
    // smaller than the tile size?  And when we do, should we also try
    // to make it bigger in the other direction to make the total tile
    // size more constant?

    // If --checknan was used and it's a floating point image, check for
    // nonfinite (NaN or Inf) values and abort if they are found.
    if (checknan && (srcspec.format.basetype == TypeDesc::FLOAT ||
                     srcspec.format.basetype == TypeDesc::HALF ||
                     srcspec.format.basetype == TypeDesc::DOUBLE)) {
        found_nonfinite = false;
        parallel_image (check_nan_block, &src, &src,
                        dstspec.x, dstspec.x+dstspec.width,
                        dstspec.y, dstspec.y+dstspec.height, nthreads);
        if (found_nonfinite) {
            if (found_nonfinite > 3)
                std::cerr << "maketx ERROR: ...and Nan/Inf at "
                          << (found_nonfinite-3) << " other pixels\n";
            exit (EXIT_FAILURE);
        }
    }
    
    // Fix nans/infs (if requested
    ImageBufAlgo::NonFiniteFixMode fixmode = ImageBufAlgo::NONFINITE_NONE;
    if (fixnan.empty() || fixnan == "none") { }
    else if (fixnan == "black") { fixmode = ImageBufAlgo::NONFINITE_BLACK; }
    else if (fixnan == "box3") { fixmode = ImageBufAlgo::NONFINITE_BOX3; }
    else {
        std::cerr << "maketx ERROR: Unknown --fixnan mode " << " fixnan\n";
        exit (EXIT_FAILURE);
    }
    
    int pixelsFixed = 0;
    if (!ImageBufAlgo::fixNonFinite (src, src, fixmode, &pixelsFixed)) {
        std::cerr << "maketx ERROR: Error fixing nans/infs.\n";
        exit (EXIT_FAILURE);
    }
    
    if (verbose && pixelsFixed>0) {
        std::cout << "  Warning: " << pixelsFixed << " nan/inf pixels fixed.\n";
    }
    
    
    
    // Color convert the pixels, if needed, in place.  If a color
    // conversion is required we will promote the src to floating point
    // (or there wont be enough precision potentially).  Also,
    // independently color convert the constant color metadata
    ImageBuf * ccSrc = &src;    // Ptr to cc'd src image
    ImageBuf colorBuffer;
    if (!incolorspace.empty() && !outcolorspace.empty() && incolorspace != outcolorspace) {
        if (src.spec().format != TypeDesc::FLOAT) {
            ImageSpec floatSpec = src.spec();
            floatSpec.set_format(TypeDesc::FLOAT);
            colorBuffer.reset("bitdepth promoted", floatSpec);
            ccSrc = &colorBuffer;
        }
        
        Timer colorconverttimer;
        if (verbose) {
            std::cout << "  Converting from colorspace " << incolorspace 
                      << " to colorspace " << outcolorspace << std::endl;
        }
        
        if (colorconfig.error()) {
            std::cerr << "Error Creating ColorConfig\n";
            std::cerr << colorconfig.geterror() << std::endl;
            exit (EXIT_FAILURE);
        }
        
        ColorProcessor * processor = colorconfig.createColorProcessor (
            incolorspace.c_str(), outcolorspace.c_str());
        
        if (!processor || colorconfig.error()) {
            std::cerr << "Error Creating Color Processor." << std::endl;
            std::cerr << colorconfig.geterror() << std::endl;
            exit (EXIT_FAILURE);
        }
        
        if (unpremult && verbose)
            std::cout << "  Unpremulting image..." << std::endl;
        
        if (!ImageBufAlgo::colorconvert (*ccSrc, src, processor, unpremult)) {
            std::cerr << "Error applying color conversion to image.\n";
            exit (EXIT_FAILURE);
        }
        
        if (isConstantColor) {
            if (!ImageBufAlgo::colorconvert (&constantColor[0],
                static_cast<int>(constantColor.size()), processor, unpremult)) {
                std::cerr << "Error applying color conversion to constant color.\n";
                exit (EXIT_FAILURE);
            }
        }

        ColorConfig::deleteColorProcessor(processor);
        processor = NULL;
        stat_colorconverttime += colorconverttimer();
    }

    // Force float for the sake of the ImageBuf math
    dstspec.set_format (TypeDesc::FLOAT);

    // Handle resize to power of two, if called for
    if (doresize  &&  ! shadowmode) {
        dstspec.width = pow2roundup (dstspec.width);
        dstspec.height = pow2roundup (dstspec.height);
        dstspec.full_width = dstspec.width;
        dstspec.full_height = dstspec.height;
    }

    bool do_resize = false;
    // Resize if we're up-resing for pow2
    if (dstspec.width != srcspec.width || dstspec.height != srcspec.height ||
          dstspec.full_depth != srcspec.full_depth)
        do_resize = true;
    // resize if the original was a crop
    if (orig_was_crop)
        do_resize = true;
    // resize if we're converting from non-border sampling to border sampling
    if (envlatlmode && ! src_samples_border && 
        (Strutil::iequals(fileformatname,"openexr") ||
         Strutil::iends_with(outputfilename,".exr")))
        do_resize = true;

    if (do_resize && orig_was_overscan &&
        out && !out->supports("displaywindow")) {
        std::cerr << "maketx ERROR: format " << out->format_name()
                  << " does not support separate display windows,\n"
                  << "              which is necessary when combining resizing"
                  << " and an input image with overscan.";
        exit (EXIT_FAILURE);
    }

    Timer resizetimer;
    ImageBuf dst ("temp", dstspec);
    ImageBuf *toplevel = &dst;    // Ptr to top level of mipmap
    if (! do_resize) {
        // Don't need to resize
        if (dstspec.format == ccSrc->spec().format) {
            // Even more special case, no format change -- just use
            // the original copy.
            toplevel = ccSrc;
        } else {
            parallel_image (copy_block, &dst, ccSrc,
                            dstspec.x, dstspec.x+dstspec.width,
                            dstspec.y, dstspec.y+dstspec.height, nthreads);
        }
    } else {
        // Resize
        if (verbose)
            std::cout << "  Resizing image to " << dstspec.width 
                      << " x " << dstspec.height << std::endl;
        if (filtername == "box" && filter->width() == 1.0f)
            parallel_image (resize_block, &dst, ccSrc,
                            dstspec.x, dstspec.x+dstspec.width,
                            dstspec.y, dstspec.y+dstspec.height, nthreads);
        else
            parallel_image (resize_block_HQ, &dst, ccSrc,
                            dstspec.x, dstspec.x+dstspec.width,
                            dstspec.y, dstspec.y+dstspec.height, nthreads);
    }
    stat_resizetime += resizetimer();

    
    // Update the toplevel ImageDescription with the sha1 pixel hash and constant color
    std::string desc = dstspec.get_string_attribute ("ImageDescription");
    bool updatedDesc = false;
    
    // FIXME: We need to do real dictionary style partial updates on the
    //        ImageDescription. I.e., set one key without affecting the
    //        other keys. But in the meantime, just clear it out if
    //        it appears the incoming image was a maketx style texture.
    
    if ((desc.find ("SHA-1=") != std::string::npos) || 
        (desc.find ("ConstantColor=") != std::string::npos)) {
        desc = "";
    }
    
    // The hash is only computed for the top mipmap level of pixel data.
    // Thus, any additional information that will effect the lower levels
    // (such as filtering information) needs to be manually added into the
    // hash.
    std::ostringstream addlHashData;
    addlHashData << filter->name() << " ";
    addlHashData << filter->width() << " ";
    
    std::string hash_digest = ImageBufAlgo::computePixelHashSHA1 (*toplevel,
        addlHashData.str());
    if (hash_digest.length()) {
        if (desc.length())
            desc += " ";
        desc += "SHA-1=";
        desc += hash_digest;
        if (verbose)
            std::cout << "  SHA-1: " << hash_digest << std::endl;
        updatedDesc = true;
        dstspec.attribute ("oiio:SHA-1", hash_digest);
    }
    
    if (isConstantColor) {
        std::ostringstream os; // Emulate a JSON array
        os << "[";
        for (unsigned int i=0; i<constantColor.size(); ++i) {
            if (i!=0) os << ",";
            os << constantColor[i];
        }
        os << "]";
        
        if (desc.length())
            desc += " ";
        desc += "ConstantColor=";
        desc += os.str();
        if (verbose)
            std::cout << "  ConstantColor: " << os.str() << std::endl;
        updatedDesc = true;
        dstspec.attribute ("oiio:ConstantColor", os.str());
    }
    
    if (updatedDesc) {
        dstspec.attribute ("ImageDescription", desc);
    }

    // Write out, and compute, the mipmap levels for the speicifed image
    write_mipmap (*toplevel, dstspec, outputfilename,
                  out, out_dataformat, !shadowmode && !nomipmap);
    delete out;  // don't need it any more

    // If using update mode, stamp the output file with a modification time
    // matching that of the input file.
    if (updatemode)
        boost::filesystem::last_write_time (outputfilename, in_time);
}



static void
write_mipmap (ImageBuf &img, const ImageSpec &outspec_template,
              std::string outputfilename, ImageOutput *out,
              TypeDesc outputdatatype, bool mipmap)
{
    ImageSpec outspec = outspec_template;
    outspec.set_format (outputdatatype);

    if (mipmap && !out->supports ("multiimage") && !out->supports ("mipmap")) {
        std::cerr << "maketx ERROR: \"" << outputfilename
                  << "\" format does not support multires images\n";
        exit (EXIT_FAILURE);
    }

    if (! mipmap && ! strcmp (out->format_name(), "openexr")) {
        // Send hint to OpenEXR driver that we won't specify a MIPmap
        outspec.attribute ("openexr:levelmode", 0 /* ONE_LEVEL */);
    }

    if (mipmap && ! strcmp (out->format_name(), "openexr")) {
        outspec.attribute ("openexr:roundingmode", 0 /* ROUND_DOWN */);
    }

    // OpenEXR always uses border sampling for environment maps
    if ((envlatlmode || envcubemode) &&
            !strcmp(out->format_name(), "openexr")) {
        src_samples_border = true;
        outspec.attribute ("oiio:updirection", "y");
        outspec.attribute ("oiio:sampleborder", 1);
    }
    if (envlatlmode && src_samples_border)
        fix_latl_edges (img);

    Timer writetimer;
    if (! out->open (outputfilename.c_str(), outspec)) {
        std::cerr << "maketx ERROR: Could not open \"" << outputfilename
                  << "\" : " << out->geterror() << "\n";
        exit (EXIT_FAILURE);
    }

    // Write out the image
    if (verbose) {
        std::cout << "  Writing file: " << outputfilename << std::endl;
        std::cout << "  Filter \"" << filter->name() << "\" width = " 
                  << filter->width() << "\n";
        std::cout << "  Top level is " << formatres(outspec) << std::endl;
    }

    bool ok = true;
    ok &= img.write (out);
    stat_writetime += writetimer();

    if (mipmap) {  // Mipmap levels:
        if (verbose)
            std::cout << "  Mipmapping...\n" << std::flush;
        ImageBuf tmp;
        ImageBuf *big = &img, *small = &tmp;
        while (ok && (outspec.width > 1 || outspec.height > 1)) {
            Timer miptimer;
            ImageSpec smallspec;

            if (mipimages.size()) {
                // Special case -- the user specified a custom MIP level
                small->reset (mipimages[0]);
                small->read (0, 0, true, TypeDesc::FLOAT);
                smallspec = small->spec();
                if (smallspec.nchannels != outspec.nchannels) {
                    std::cout << "WARNING: Custom mip level \"" << mipimages[0]
                              << " had the wrong number of channels.\n";
                    ImageBuf *t = new ImageBuf (mipimages[0], smallspec);
                    ImageBufAlgo::setNumChannels(*t, *small, outspec.nchannels);
                    std::swap (t, small);
                    delete t;
                }
                smallspec.tile_width = outspec.tile_width;
                smallspec.tile_height = outspec.tile_height;
                smallspec.tile_depth = outspec.tile_depth;
                mipimages.erase (mipimages.begin());
            } else {
                // Resize a factor of two smaller
                smallspec = outspec;
                smallspec.width = big->spec().width;
                smallspec.height = big->spec().height;
                smallspec.depth = big->spec().depth;
                if (smallspec.width > 1)
                    smallspec.width /= 2;
                if (smallspec.height > 1)
                    smallspec.height /= 2;
                smallspec.full_width = smallspec.width;
                smallspec.full_height = smallspec.height;
                smallspec.full_depth = smallspec.depth;
                smallspec.set_format (TypeDesc::FLOAT);

                // Trick: to get the resize working properly, we reset
                // both display and pixel windows to match, and have 0
                // offset, AND doctor the big image to have its display
                // and pixel windows match.  Don't worry, the texture
                // engine doesn't care what the upper MIP levels have
                // for the window sizes, it uses level 0 to determine
                // the relatinship between texture 0-1 space (display
                // window) and the pixels.
                smallspec.x = 0;
                smallspec.y = 0;
                smallspec.full_x = 0;
                smallspec.full_y = 0;
                small->alloc (smallspec);  // Realocate with new size
                big->set_full (big->xbegin(), big->xend(), big->ybegin(),
                               big->yend(), big->zbegin(), big->zend());

                if (filtername == "box" && filter->width() == 1.0f)
                    parallel_image (resize_block, small, big,
                                    small->xbegin(), small->xend(),
                                    small->ybegin(), small->yend(),
                                    nthreads);
                else
                    parallel_image (resize_block_HQ, small, big,
                                    small->xbegin(), small->xend(),
                                    small->ybegin(), small->yend(),
                                    nthreads);
            }

            stat_miptime += miptimer();
            outspec = smallspec;
            outspec.set_format (outputdatatype);
            if (envlatlmode && src_samples_border)
                fix_latl_edges (*small);

            Timer writetimer;
            // If the format explicitly supports MIP-maps, use that,
            // otherwise try to simulate MIP-mapping with multi-image.
            bool ok = false;
            ImageOutput::OpenMode mode = out->supports ("mipmap") ?
                ImageOutput::AppendMIPLevel : ImageOutput::AppendSubimage;
            if (! out->open (outputfilename.c_str(), outspec, mode)) {
                std::cerr << "maketx ERROR: Could not append \"" << outputfilename
                          << "\" : " << out->geterror() << "\n";
                exit (EXIT_FAILURE);
            }
            ok &= small->write (out);
            stat_writetime += writetimer();
            if (verbose) {
                std::cout << "    " << formatres(smallspec) << std::endl;
            }
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

    OIIO_NAMESPACE::attribute ("threads", nthreads);
    if (stats) {
        ImageCache *ic = ImageCache::create ();  // get the shared one
        ic->attribute ("forcefloat", 1);   // Force float upon read
        ic->attribute ("max_memory_MB", 1024.0);  // 1 GB cache
        // N.B. This will apply to the default IC that any ImageBuf's get.
    }

    if (mipmapmode) {
        make_texturemap ("texture map");
    } else if (shadowmode) {
        make_texturemap ("shadow map");
    } else if (shadowcubemode) {
        std::cerr << "Shadow cubes currently unsupported\n";
    } else if (volshadowmode) {
        std::cerr << "Volume shadows currently unsupported\n";
    } else if (envlatlmode) {
        make_texturemap ("latlong environment map");
    } else if (envcubemode) {
        std::cerr << "Environment cubes currently unsupported\n";
    } else if (lightprobemode) {
        std::cerr << "Light probes currently unsupported\n";
    } else if (vertcrossmode) {
        std::cerr << "Vertcross currently unsupported\n";
    } else if (latl2envcubemode) {
        std::cerr << "Latlong->cube conversion currently unsupported\n";
    }

    if (verbose || stats) {
        std::cout << "maketx Runtime statistics (seconds):\n";
        double alltime = alltimer();
        std::cout << Strutil::format ("  total runtime:   %5.2f\n", alltime);
        std::cout << Strutil::format ("  file read:       %5.2f\n", stat_readtime);
        std::cout << Strutil::format ("  file write:      %5.2f\n", stat_writetime);
        std::cout << Strutil::format ("  initial resize:  %5.2f\n", stat_resizetime);
        std::cout << Strutil::format ("  mip computation: %5.2f\n", stat_miptime);
        std::cout << Strutil::format ("  color convert:   %5.2f\n", stat_colorconverttime);
        std::cout << Strutil::format ("  unaccounted:     %5.2f\n",
                                      alltime-stat_readtime-stat_writetime-stat_resizetime-stat_miptime);
        size_t kb = Sysutil::memory_used(true) / 1024;
        std::cout << Strutil::format ("maketx memory used: %5.1f MB\n",
                                      (double)kb/1024.0);
    }

    Filter2D::destroy (filter);

    if (stats) {
        ImageCache *ic = ImageCache::create ();  // get the shared one
        std::cout << "\n" << ic->getstats();
    }

    return 0;
}
