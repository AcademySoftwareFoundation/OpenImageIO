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
#include <cstring>
#include <ctime>
#include <iostream>
#include <iterator>

#include <OpenEXR/ImathMatrix.h>
#include <OpenEXR/ImathVec.h>
#include <OpenEXR/half.h>

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/ustring.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/texture.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/timer.h>
#include <OpenImageIO/benchmark.h>
#include "../libtexture/imagecache_pvt.h"

using namespace OIIO;

using OIIO::_1;

static std::vector<ustring> filenames;
static std::string output_filename = "out.exr";
static bool verbose = false;
static int nthreads = 0;
static int threadtimes = 0;
static int output_xres = 512, output_yres = 512;
static int nchannels_override = 0;
static std::string dataformatname = "half";
static float sscale = 1, tscale = 1;
static float sblur = 0, tblur = -1;
static float width = 1;
static std::string wrapmodes ("periodic");
static int anisotropic = -1;
static int iters = 1;
static int autotile = 0;
static bool automip = false;
static bool dedup = true;
static bool test_construction = false;
static bool test_gettexels = false;
static bool test_getimagespec = false;
static bool filtertest = false;
static TextureSystem *texsys = NULL;
static std::string searchpath;
static bool batch = false;
static bool nowarp = false;
static bool tube = false;
static bool use_handle = false;
static float cachesize = -1;
static int maxfiles = -1;
static int mipmode = TextureOpt::MipModeDefault;
static int interpmode = TextureOpt::InterpSmartBicubic;
static float missing[4] = {-1, 0, 0, 1};
static float fill = -1;  // -1 signifies unset
static float scalefactor = 1.0f;
static Imath::V3f texoffset (0,0,0);
static bool nountiled = false;
static bool nounmipped = false;
static bool gray_to_rgb = false;
static bool flip_t = false;
static bool resetstats = false;
static bool testhash = false;
static bool wedge = false;
static int ntrials = 1;
static int testicwrite = 0;
static bool test_derivs = false;
static bool test_statquery = false;
static Imath::M33f xform;
static mutex error_mutex;
void *dummyptr;

typedef void (*Mapping2D)(const int&,const int&,float&,float&,float&,float&,float&,float&);
typedef void (*Mapping3D)(const int&,const int&,Imath::V3f&,Imath::V3f&,Imath::V3f&,Imath::V3f &);

typedef void (*Mapping2DWide)(const Tex::IntWide&,const Tex::IntWide&,
                              Tex::FloatWide&,Tex::FloatWide&,
                              Tex::FloatWide&,Tex::FloatWide&,
                              Tex::FloatWide&,Tex::FloatWide&);
typedef void (*Mapping3DWide)(const Tex::IntWide&,const Tex::IntWide&,
                              Imath::Vec3<Tex::FloatWide>&,
                              Imath::Vec3<Tex::FloatWide>&,
                              Imath::Vec3<Tex::FloatWide>&,
                              Imath::Vec3<Tex::FloatWide>&);



static int
parse_files (int argc, const char *argv[])
{
    for (int i = 0;  i < argc;  i++)
        filenames.emplace_back(argv[i]);
    return 0;
}



static void
getargs (int argc, const char *argv[])
{
    TextureOpt opt;  // to figure out defaults
    anisotropic = opt.anisotropic;

    bool help = false;
    ArgParse ap;
    ap.options ("Usage:  testtex [options] inputfile",
                  "%*", parse_files, "",
                  "--help", &help, "Print help message",
                  "-v", &verbose, "Verbose status messages",
                  "-o %s", &output_filename, "Output test image",
                  "-d %s", &dataformatname, "Set the output data format to one of:"
                        "uint8, sint8, uint10, uint12, uint16, sint16, half, float, double",
                  "--res %d %d", &output_xres, &output_yres,
                      "Resolution of output test image",
                  "--nchannels %d", &nchannels_override,
                      "Force number of channels to look up",
                  "--iters %d", &iters,
                      "Iterations for time trials",
                  "--threads %d", &nthreads, "Number of threads (default 0 = #cores)",
                  "-t %d", &nthreads, "",  // synonym
                  "--blur %f", &sblur, "Add blur to texture lookup",
                  "--stblur %f %f", &sblur, &tblur, "Add blur (s, t) to texture lookup",
                  "--width %f", &width, "Multiply filter width of texture lookup",
                  "--fill %f", &fill, "Set fill value for missing channels",
                  "--wrap %s", &wrapmodes, "Set wrap mode (default, black, clamp, periodic, mirror, overscan)",
                  "--aniso %d", &anisotropic,
                      Strutil::format("Set max anisotropy (default: %d)", anisotropic).c_str(),
                  "--mipmode %d", &mipmode, "Set mip mode (default: 0 = aniso)",
                  "--interpmode %d", &interpmode, "Set interp mode (default: 3 = smart bicubic)",
                  "--missing %f %f %f", &missing[0], &missing[1], &missing[2],
                        "Specify missing texture color",
                  "--autotile %d", &autotile, "Set auto-tile size for the image cache",
                  "--automip", &automip, "Set auto-MIPmap for the image cache",
                  "--batch", &batch,
                        Strutil::format("Use batched shading, batch size = %d", Tex::BatchWidth).c_str(),
                  "--handle", &use_handle, "Use texture handle rather than name lookup",
                  "--searchpath %s", &searchpath, "Search path for files",
                  "--filtertest", &filtertest, "Test the filter sizes",
                  "--nowarp", &nowarp, "Do not warp the image->texture mapping",
                  "--tube", &tube, "Make a tube projection",
                  "--ctr", &test_construction, "Test TextureOpt construction time",
                  "--gettexels", &test_gettexels, "Test TextureSystem::get_texels",
                  "--getimagespec", &test_getimagespec, "Test TextureSystem::get_imagespec",
                  "--offset %f %f %f", &texoffset[0], &texoffset[1], &texoffset[2], "Offset texture coordinates",
                  "--scalest %f %f", &sscale, &tscale, "Scale texture lookups (s, t)",
                  "--cachesize %f", &cachesize, "Set cache size, in MB",
                  "--nodedup %!", &dedup, "Turn off de-duplication",
                  "--scale %f", &scalefactor, "Scale intensities",
                  "--maxfiles %d", &maxfiles, "Set maximum open files",
                  "--nountiled", &nountiled, "Reject untiled images",
                  "--nounmipped", &nounmipped, "Reject unmipped images",
                  "--graytorgb", &gray_to_rgb, "Convert gratscale textures to RGB",
                  "--flipt", &flip_t, "Flip direction of t coordinate",
                  "--derivs", &test_derivs, "Test returning derivatives of texture lookups",
                  "--resetstats", &resetstats, "Print and reset statistics on each iteration",
                  "--testhash", &testhash, "Test the tile hashing function",
                  "--threadtimes %d", &threadtimes, "Do thread timings (arg = workload profile)",
                  "--trials %d", &ntrials, "Number of trials for timings",
                  "--wedge", &wedge, "Wedge test",
                  "--testicwrite %d", &testicwrite, "Test ImageCache write ability (1=seeded, 2=generated)",
                  "--teststatquery", &test_statquery, "Test queries of statistics",
                  NULL);
    if (ap.parse (argc, argv) < 0) {
        std::cerr << ap.geterror() << std::endl;
        ap.usage ();
        exit (EXIT_FAILURE);
    }
    if (help) {
        ap.usage ();
        exit (EXIT_FAILURE);
    }

    if (filenames.size() < 1 &&
          !test_construction && !test_getimagespec && !testhash) {
        std::cerr << "testtex: Must have at least one input file\n";
        ap.usage();
        exit (EXIT_FAILURE);
    }
}



static void
initialize_opt (TextureOpt &opt, int nchannels)
{
    opt.sblur = sblur;
    opt.tblur = tblur >= 0.0f ? tblur : sblur;
    opt.rblur = sblur;
    opt.swidth = width;
    opt.twidth = width;
    opt.rwidth = width;
//    opt.nchannels = nchannels;
    opt.fill = (fill >= 0.0f) ? fill : 1.0f;
    if (missing[0] >= 0)
        opt.missingcolor = (float *)&missing;
    TextureOpt::parse_wrapmodes (wrapmodes.c_str(), opt.swrap, opt.twrap);
    opt.rwrap = opt.swrap;
    opt.anisotropic = anisotropic;
    opt.mipmode = (TextureOpt::MipMode) mipmode;
    opt.interpmode = (TextureOpt::InterpMode) interpmode;
}



static void
initialize_opt (TextureOptBatch &opt, int nchannels)
{
    using namespace Tex;
    FloatWide sb(sblur); sb.store (opt.sblur);
    FloatWide tb(tblur >= 0.0f ? tblur : sblur); tb.store (opt.tblur);
    sb.store (opt.rblur);
    FloatWide w (width);
    w.store (opt.swidth);
    w.store (opt.twidth);
    w.store (opt.rwidth);
    opt.fill = (fill >= 0.0f) ? fill : 1.0f;
    if (missing[0] >= 0)
        opt.missingcolor = (float *)&missing;
    Tex::parse_wrapmodes (wrapmodes.c_str(), opt.swrap, opt.twrap);
    opt.rwrap = opt.swrap;
    opt.anisotropic = anisotropic;
    opt.mipmode = MipMode(mipmode);
    opt.interpmode = InterpMode(interpmode);
}



static void
test_gettextureinfo (ustring filename)
{
    bool ok;

    int res[2] = {0};
    ok = texsys->get_texture_info (filename, 0, ustring("resolution"),
                                   TypeDesc(TypeDesc::INT,2), res);
    std::cout << "Result of get_texture_info resolution = " << ok << ' ' << res[0] << 'x' << res[1] << "\n";

    int chan = 0;
    ok = texsys->get_texture_info (filename, 0, ustring("channels"),
                                   TypeDesc::INT, &chan);
    std::cout << "Result of get_texture_info channels = " << ok << ' ' << chan << "\n";

    float fchan = 0;
    ok = texsys->get_texture_info (filename, 0, ustring("channels"),
                                   TypeDesc::FLOAT, &fchan);
    std::cout << "Result of get_texture_info channels = " << ok << ' ' << fchan << "\n";

    int dataformat = 0;
    ok = texsys->get_texture_info (filename, 0, ustring("format"),
                                   TypeDesc::INT, &dataformat);
    std::cout << "Result of get_texture_info data format = " << ok << ' '
              << TypeDesc((TypeDesc::BASETYPE)dataformat).c_str() << "\n";

    const char *datetime = NULL;
    ok = texsys->get_texture_info (filename, 0, ustring("DateTime"),
                                   TypeDesc::STRING, &datetime);
    std::cout << "Result of get_texture_info datetime = " << ok << ' '
              << (datetime ? datetime : "") << "\n";

    float avg[4];
    ok = texsys->get_texture_info (filename, 0, ustring("averagecolor"),
                                   TypeDesc(TypeDesc::FLOAT,4), avg);
    std::cout << "Result of get_texture_info averagecolor = " << (ok?"yes":"no\n");
    if (ok)
        std::cout << " " << avg[0] << ' ' << avg[1] << ' '
                  << avg[2] << ' ' << avg[3] << "\n";
    ok = texsys->get_texture_info (filename, 0, ustring("averagealpha"),
                                   TypeFloat, avg);
    std::cout << "Result of get_texture_info averagealpha = " << (ok?"yes":"no\n");
    if (ok)
        std::cout << " " << avg[0] << "\n";
    ok = texsys->get_texture_info (filename, 0, ustring("constantcolor"),
                                   TypeDesc(TypeDesc::FLOAT,4), avg);
    std::cout << "Result of get_texture_info constantcolor = " << (ok?"yes":"no\n");
    if (ok)
        std::cout << " " << avg[0] << ' ' << avg[1] << ' '
                  << avg[2] << ' ' << avg[3] << "\n";

    const char *texturetype = NULL;
    ok = texsys->get_texture_info (filename, 0, ustring("textureformat"),
                                   TypeDesc::STRING, &texturetype);
    std::cout << "Texture type is " << ok << ' '
              << (texturetype ? texturetype : "") << "\n";
    std::cout << "\n";
}



template<typename Float=float>
inline Imath::Vec2<Float>
warp (const Float& x, const Float& y, const Imath::M33f &xform)
{
    Imath::Vec2<Float> coord (x, y);
    xform.multVecMatrix (coord, coord);
    return coord;
}


template<typename Float=float>
inline Imath::Vec3<Float>
warp (const Float& x, const Float& y, const Float& z, const Imath::M33f &xform)
{
    Imath::Vec3<Float> coord (x, y, z);
    coord *= xform;
    return coord;
}


template<typename Float=float>
inline Imath::Vec2<Float>
warp_coord (const Float& x, const Float& y)
{
    Imath::Vec2<Float> coord = warp (x/output_xres, y/output_yres, xform);
    coord.x *= sscale;
    coord.y *= tscale;
    coord += Imath::Vec2<Float>(texoffset.x, texoffset.y);
    return coord;
}



// Just map pixels to [0,1] st space
template<typename Float=float, typename Int=int>
void
map_default (const Int& x, const Int& y, Float& s, Float& t,
             Float& dsdx, Float& dtdx, Float& dsdy, Float& dtdy)
{
    s = (Float(x)+0.5f)/output_xres * sscale + texoffset[0];
    t = (Float(y)+0.5f)/output_yres * tscale + texoffset[1];
    dsdx = 1.0f/output_xres * sscale;
    dtdx = 0.0f;
    dsdy = 0.0f;
    dtdy = 1.0f/output_yres * tscale;
}



template<typename Float=float, typename Int=int>
static void
map_warp (const Int& x, const Int& y, Float &s, Float &t,
          Float &dsdx, Float &dtdx, Float &dsdy, Float &dtdy)
{
    const Imath::Vec2<Float> coord  = warp_coord (Float(x)+0.5f, Float(y)+0.5f);
    const Imath::Vec2<Float> coordx = warp_coord (Float(x)+1.5f, Float(y)+0.5f);
    const Imath::Vec2<Float> coordy = warp_coord (Float(x)+0.5f, Float(y)+1.5f);
    s = coord[0];
    t = coord[1];
    dsdx = coordx[0] - coord[0];
    dtdx = coordx[1] - coord[1];
    dsdy = coordy[0] - coord[0];
    dtdy = coordy[1] - coord[1];
}



static void
map_tube (const int& x, const int& y, float &s, float &t,
          float &dsdx, float &dtdx, float &dsdy, float &dtdy)
{
    float xt = (float(x)+0.5f)/output_xres - 0.5f;
    float dxt_dx = 1.0f/output_xres;
    float yt = (float(y)+0.5f)/output_yres - 0.5f;
    float dyt_dy = 1.0f/output_yres;
    float theta = atan2f (yt, xt);
    // See OSL's Dual2 for partial derivs of
    // atan2, hypot, and 1/x
    double denom = 1.0 / (xt*xt + yt*yt);
    double dtheta_dx = yt*dxt_dx * denom;
    double dtheta_dy = -xt*dyt_dy * denom;
    s = float(4.0 * theta / (2.0 * M_PI));
    dsdx = float(4.0 * dtheta_dx / (2.0 * M_PI));
    dsdy = float(4.0 * dtheta_dy / (2.0 * M_PI));
    double h = hypot(xt,yt);
    double dh_dx = xt*dxt_dx / h;
    double dh_dy = yt*dyt_dy / h;
    h *= M_SQRT2;
    dh_dx *= M_SQRT2; dh_dy *= M_SQRT2;
    double hinv = 1.0 / h;
    t = float(hinv);
    dtdx = float(hinv * (-hinv * dh_dx));
    dtdy = float(hinv * (-hinv * dh_dy));
}



// FIXME -- templatize map_tube. For now, just loop over scalar version.
static void
map_tube (const Tex::IntWide& x, const Tex::IntWide& y,
          Tex::FloatWide& s, Tex::FloatWide& t,
          Tex::FloatWide& dsdx, Tex::FloatWide& dtdx,
          Tex::FloatWide& dsdy, Tex::FloatWide& dtdy)
{
    for (int i = 0; i < Tex::BatchWidth; ++i)
        map_tube (x[i], y[i], s[i], t[i], dsdx[i], dtdx[i], dsdy[i], dtdy[i]);
}



// To test filters, we always sample at the center of the image, and
// keep the minor axis of the filter at 1/256, but we vary the
// eccentricity (i.e. major axis length) as we go left (1) to right
// (32), and vary the angle as we go top (0) to bottom (2pi).
//
// If filtering is correct, all pixels should sample from the same MIP
// level because they have the same minor axis (1/256), regardless of
// eccentricity or angle.  If we specify a texture that has a
// distinctive color at the 256-res level, and something totally
// different at the 512 and 128 levels, it should be easy to verify that
// we aren't over-filtering or under-filtering by selecting the wrong
// MIP level.  (Though of course, there are other kinds of mistakes we
// could be making, such as computing the wrong eccentricity or angle.)
static void
map_filtertest (const int& x, const int& y, float &s, float &t,
                float &dsdx, float &dtdx, float &dsdy, float &dtdy)
{
    float minoraxis = 1.0f/256;
    float majoraxis = minoraxis * lerp (1.0f, 32.0f, (float)x/(output_xres-1));
    float angle = (float)(2.0 * M_PI * (double)y/(output_yres-1));
    float sinangle, cosangle;
    sincos (angle, &sinangle, &cosangle);
    s = 0.5f;
    t = 0.5f;

    dsdx =  minoraxis * cosangle;
    dtdx =  minoraxis * sinangle;
    dsdy = -majoraxis * sinangle;
    dtdy =  majoraxis * cosangle;
}



// FIXME -- templatize map_filtertest. For now, just loop over scalar version.
static void
map_filtertest (const Tex::IntWide& x, const Tex::IntWide& y,
          Tex::FloatWide& s, Tex::FloatWide& t,
          Tex::FloatWide& dsdx, Tex::FloatWide& dtdx,
          Tex::FloatWide& dsdy, Tex::FloatWide& dtdy)
{
    for (int i = 0; i < Tex::BatchWidth; ++i)
        map_filtertest (x[i], y[i], s[i], t[i],
                        dsdx[i], dtdx[i], dsdy[i], dtdy[i]);
}



template<typename Float=float, typename Int=int>
void
map_default_3D (const Int& x, const Int& y,
                Imath::Vec3<Float> &P, Imath::Vec3<Float> &dPdx,
                Imath::Vec3<Float> &dPdy, Imath::Vec3<Float> &dPdz)
{
    P[0] = (Float(x)+0.5f)/output_xres * sscale;
    P[1] = (Float(y)+0.5f)/output_yres * tscale;
    P[2] = 0.5f * sscale;
    P += texoffset;
    dPdx[0] = 1.0f/output_xres * sscale;
    dPdx[1] = 0;
    dPdx[2] = 0;
    dPdy[0] = 0;
    dPdy[1] = 1.0f/output_yres * tscale;
    dPdy[2] = 0;
    dPdz.setValue (0,0,0);
}



template<typename Float=float, typename Int=int>
void
map_warp_3D (const Int& x, const Int& y,
             Imath::Vec3<Float> &P, Imath::Vec3<Float> &dPdx,
             Imath::Vec3<Float> &dPdy, Imath::Vec3<Float> &dPdz)
{
    Imath::Vec3<Float> coord = warp ((Float(x)/output_xres),
                                     (Float(y)/output_yres),
                                     Float(0.5), xform);
    coord.x *= sscale;
    coord.y *= tscale;
    coord += texoffset;
    Imath::Vec3<Float> coordx = warp ((Float(x+1)/output_xres),
                                      (Float(y)/output_yres),
                                      Float(0.5), xform);
    coordx.x *= sscale;
    coordx.y *= tscale;
    coordx += texoffset;
    Imath::Vec3<Float> coordy = warp ((Float(x)/output_xres),
                                      (Float(y+1)/output_yres),
                                      Float(0.5), xform);
    coordy.x *= sscale;
    coordy.y *= tscale;
    coordy += texoffset;
    P = coord;
    dPdx = coordx - coord;
    dPdy = coordy - coord;
    dPdz.setValue (0,0,0);
}



void
plain_tex_region (ImageBuf &image, ustring filename, Mapping2D mapping,
                  ImageBuf *image_ds, ImageBuf *image_dt, ROI roi)
{
    TextureSystem::Perthread *perthread_info = texsys->get_perthread_info ();
    TextureSystem::TextureHandle *texture_handle = texsys->get_texture_handle (filename);
    int nchannels = nchannels_override ? nchannels_override : image.nchannels();

    TextureOpt opt;
    initialize_opt (opt, nchannels);

    float *result = ALLOCA (float, std::max (3, nchannels));
    float *dresultds = test_derivs ? ALLOCA (float, nchannels) : NULL;
    float *dresultdt = test_derivs ? ALLOCA (float, nchannels) : NULL;
    for (ImageBuf::Iterator<float> p (image, roi);  ! p.done();  ++p) {
        float s, t, dsdx, dtdx, dsdy, dtdy;
        mapping (p.x(), p.y(), s, t, dsdx, dtdx, dsdy, dtdy);

        // Call the texture system to do the filtering.
        bool ok;
        if (use_handle)
            ok = texsys->texture (texture_handle, perthread_info, opt,
                                  s, t, dsdx, dtdx, dsdy, dtdy,
                                  nchannels, result, dresultds, dresultdt);
        else
            ok = texsys->texture (filename, opt,
                                  s, t, dsdx, dtdx, dsdy, dtdy,
                                  nchannels, result, dresultds, dresultdt);
        if (! ok) {
            std::string e = texsys->geterror ();
            if (! e.empty()) {
                lock_guard lock (error_mutex);
                std::cerr << "ERROR: " << e << "\n";
            }
        }

        // Save filtered pixels back to the image.
        for (int i = 0;  i < nchannels;  ++i)
            result[i] *= scalefactor;
        image.setpixel (p.x(), p.y(), result);
        if (test_derivs) {
            image_ds->setpixel (p.x(), p.y(), dresultds);
            image_dt->setpixel (p.x(), p.y(), dresultdt);
        }
    }
}



void
test_plain_texture (Mapping2D mapping)
{
    std::cout << "Testing 2d texture " << filenames[0] << ", output = "
              << output_filename << "\n";
    const int nchannels = 4;
    ImageSpec outspec (output_xres, output_yres, nchannels, TypeDesc::FLOAT);
    ImageBuf image (outspec);
    TypeDesc fmt (dataformatname);
    image.set_write_format (fmt);
    OIIO::ImageBufAlgo::zero (image);
    ImageBuf image_ds, image_dt;
    if (test_derivs) {
        image_ds.reset (outspec);
        image_ds.set_write_format (fmt);
        OIIO::ImageBufAlgo::zero (image_ds);
        image_dt.reset (outspec);
        OIIO::ImageBufAlgo::zero (image_dt);
        image_dt.set_write_format (fmt);
    }

    ustring filename = filenames[0];

    for (int iter = 0;  iter < iters;  ++iter) {
        if (iters > 1 && filenames.size() > 1) {
            // Use a different filename for each iteration
            int texid = std::min (iter, (int)filenames.size()-1);
            filename = (filenames[texid]);
            std::cout << "iter " << iter << " file " << filename << "\n";
        }

        ImageBufAlgo::parallel_image (get_roi(image.spec()), nthreads,
                std::bind(plain_tex_region, std::ref(image), filename, mapping,
                          test_derivs ? &image_ds : NULL,
                          test_derivs ? &image_dt : NULL, _1));
        if (resetstats) {
            std::cout << texsys->getstats(2) << "\n";
            texsys->reset_stats ();
        }
    }

    if (! image.write (output_filename)) 
        std::cerr << "Error writing " << output_filename 
                  << " : " << image.geterror() << "\n";
    if (test_derivs) {
        if (! image_ds.write (output_filename+"-ds.exr")) 
            std::cerr << "Error writing " << (output_filename+"-ds.exr")
                      << " : " << image_ds.geterror() << "\n";
        if (! image_dt.write (output_filename+"-dt.exr")) 
            std::cerr << "Error writing " << (output_filename+"-dt.exr")
                      << " : " << image_dt.geterror() << "\n";
    }
}



void
plain_tex_region_batch (ImageBuf &image, ustring filename, Mapping2DWide mapping,
                  ImageBuf *image_ds, ImageBuf *image_dt, const ROI roi)
{
    using namespace Tex;
    TextureSystem::Perthread *perthread_info = texsys->get_perthread_info ();
    TextureSystem::TextureHandle *texture_handle = texsys->get_texture_handle (filename);
    int nchannels_img = image.nchannels();
    int nchannels = nchannels_override ? nchannels_override : image.nchannels();
    DASSERT (image.spec().format == TypeDesc::FLOAT);
    DASSERT (image_ds == nullptr || image_ds->spec().format == TypeDesc::FLOAT);
    DASSERT (image_dt == nullptr || image_dt->spec().format == TypeDesc::FLOAT);

    TextureOptBatch opt;
    initialize_opt (opt, nchannels);

    int nc = std::max (3, nchannels);
    FloatWide *result = ALLOCA (FloatWide, nc);
    FloatWide *dresultds = test_derivs ? ALLOCA (FloatWide, nc) : nullptr;
    FloatWide *dresultdt = test_derivs ? ALLOCA (FloatWide, nc) : nullptr;
    for (int y = roi.ybegin; y < roi.yend; ++y) {
        for (int x = roi.xbegin; x < roi.xend; x += BatchWidth) {
            FloatWide s, t, dsdx, dtdx, dsdy, dtdy;
            mapping (IntWide::Iota(x), y, s, t, dsdx, dtdx, dsdy, dtdy);
            int npoints = std::min (BatchWidth, roi.xend - x);
            RunMask mask = RunMaskOn >> (BatchWidth - npoints);
            // Call the texture system to do the filtering.
            bool ok;
            if (use_handle)
                ok = texsys->texture (texture_handle, perthread_info, opt,
                                      mask, s.data(), t.data(),
                                      dsdx.data(), dtdx.data(),
                                      dsdy.data(), dtdy.data(),
                                      nchannels, (float *)result,
                                      (float *)dresultds, (float *)dresultdt);
            else
                ok = texsys->texture (filename, opt,
                                      mask, s.data(), t.data(),
                                      dsdx.data(), dtdx.data(),
                                      dsdy.data(), dtdy.data(),
                                      nchannels, (float *)result,
                                      (float *)dresultds, (float *)dresultdt);
            if (! ok) {
                std::string e = texsys->geterror ();
                if (! e.empty()) {
                    lock_guard lock (error_mutex);
                    std::cerr << "ERROR: " << e << "\n";
                }
            }
            // Save filtered pixels back to the image.
            for (int c = 0;  c < nchannels;  ++c)
                result[c] *= scalefactor;
            float *resultptr = (float *)image.pixeladdr (x, y);
            // FIXME: simplify by using SIMD scatter
            for (int c = 0;  c < nchannels;  ++c)
                for (int i = 0; i < npoints; ++i)
                    resultptr[c+i*nchannels_img] = result[c][i];
            if (test_derivs) {
                float *resultdsptr = (float *)image_ds->pixeladdr (x, y);
                float *resultdtptr = (float *)image_dt->pixeladdr (x, y);
                for (int c = 0;  c < nchannels;  ++c) {
                    for (int i = 0; i < npoints; ++i) {
                        resultdsptr[c+i*nchannels_img] = dresultds[c][i];
                        resultdtptr[c+i*nchannels_img] = dresultdt[c][i];
                    }
                }
            }
        }
    }
}



void
test_plain_texture_batch (Mapping2DWide mapping)
{
    std::cout << "Testing BATCHED 2d texture " << filenames[0] << ", output = "
              << output_filename << "\n";
    const int nchannels = 4;
    ImageSpec outspec (output_xres, output_yres, nchannels, TypeDesc::FLOAT);
    TypeDesc fmt (dataformatname);
    ImageBuf image (outspec);
    image.set_write_format (fmt);
    OIIO::ImageBufAlgo::zero (image);
    std::unique_ptr<ImageBuf> image_ds, image_dt;
    if (test_derivs) {
        image_ds.reset (new ImageBuf (outspec));
        image_ds->set_write_format (fmt);
        OIIO::ImageBufAlgo::zero (*image_ds);
        image_dt.reset (new ImageBuf (outspec));
        image_dt->set_write_format (fmt);
        OIIO::ImageBufAlgo::zero (*image_dt);
    }

    ustring filename = filenames[0];

    for (int iter = 0;  iter < iters;  ++iter) {
        if (iters > 1 && filenames.size() > 1) {
            // Use a different filename for each iteration
            int texid = std::min (iter, (int)filenames.size()-1);
            filename = (filenames[texid]);
            std::cout << "iter " << iter << " file " << filename << "\n";
        }

        ImageBufAlgo::parallel_image (get_roi(image.spec()), nthreads,
                [&](ROI roi){
                    plain_tex_region_batch (image, filename, mapping,
                                      image_ds.get(), image_dt.get(), roi);
                });
        if (resetstats) {
            std::cout << texsys->getstats(2) << "\n";
            texsys->reset_stats ();
        }
    }

    if (! image.write (output_filename))
        std::cerr << "Error writing " << output_filename
                  << " : " << image.geterror() << "\n";
    if (test_derivs) {
        if (! image_ds->write (output_filename+"-ds.exr"))
            std::cerr << "Error writing " << (output_filename+"-ds.exr")
                      << " : " << image_ds->geterror() << "\n";
        if (! image_dt->write (output_filename+"-dt.exr"))
            std::cerr << "Error writing " << (output_filename+"-dt.exr")
                      << " : " << image_dt->geterror() << "\n";
    }
}



void
tex3d_region (ImageBuf &image, ustring filename, Mapping3D mapping,
              ROI roi)
{
    TextureSystem::Perthread *perthread_info = texsys->get_perthread_info ();
    TextureSystem::TextureHandle *texture_handle = texsys->get_texture_handle (filename);
    int nchannels = nchannels_override ? nchannels_override : image.nchannels();

    TextureOpt opt;
    initialize_opt (opt, nchannels);
    opt.fill = (fill >= 0.0f) ? fill : 0.0f;
//    opt.swrap = opt.twrap = opt.rwrap = TextureOpt::WrapPeriodic;

    float *result = ALLOCA (float, nchannels);
    float *dresultds = test_derivs ? ALLOCA (float, nchannels) : NULL;
    float *dresultdt = test_derivs ? ALLOCA (float, nchannels) : NULL;
    float *dresultdr = test_derivs ? ALLOCA (float, nchannels) : NULL;
    for (ImageBuf::Iterator<float> p (image, roi);  ! p.done();  ++p) {
        Imath::V3f P, dPdx, dPdy, dPdz;
        mapping (p.x(), p.y(), P, dPdx, dPdy, dPdz);

        // Call the texture system to do the filtering.
        bool ok = texsys->texture3d (texture_handle, perthread_info, opt, 
                                     P, dPdx, dPdy, dPdz, nchannels,
                                     result, dresultds, dresultdt, dresultdr);
        if (! ok) {
            std::string e = texsys->geterror ();
            if (! e.empty()) {
                lock_guard lock (error_mutex);
                std::cerr << "ERROR: " << e << "\n";
            }
        }

        // Save filtered pixels back to the image.
        for (int i = 0;  i < nchannels;  ++i)
            result[i] *= scalefactor;
        image.setpixel (p.x(), p.y(), result);
    }
}



void
tex3d_region_batch (ImageBuf &image, ustring filename, Mapping3DWide mapping, ROI roi)
{
    using namespace Tex;
    TextureSystem::Perthread *perthread_info = texsys->get_perthread_info ();
    TextureSystem::TextureHandle *texture_handle = texsys->get_texture_handle (filename);
    int nchannels_img = image.nchannels();
    int nchannels = nchannels_override ? nchannels_override : image.nchannels();

    TextureOptBatch opt;
    initialize_opt (opt, nchannels);
    opt.fill = (fill >= 0.0f) ? fill : 0.0f;
//    opt.swrap = opt.twrap = opt.rwrap = TextureOpt::WrapPeriodic;

    FloatWide *result = ALLOCA (FloatWide, nchannels);
    FloatWide *dresultds = test_derivs ? ALLOCA (FloatWide, nchannels) : NULL;
    FloatWide *dresultdt = test_derivs ? ALLOCA (FloatWide, nchannels) : NULL;
    FloatWide *dresultdr = test_derivs ? ALLOCA (FloatWide, nchannels) : NULL;
    for (int y = roi.ybegin; y < roi.yend; ++y) {
        for (int x = roi.xbegin; x < roi.xend; x += BatchWidth) {
            Imath::Vec3<FloatWide> P, dPdx, dPdy, dPdz;
            mapping (IntWide::Iota(x), y, P, dPdx, dPdy, dPdz);
            int npoints = std::min (BatchWidth, roi.xend - x);
            RunMask mask = RunMaskOn >> (BatchWidth - npoints);

            // Call the texture system to do the filtering.
            if (y == 0 && x == 0)
                std::cout << "P = " << P << "\n";
            bool ok = texsys->texture3d (texture_handle, perthread_info, opt, mask,
                                         (float*)&P, (float*)&dPdx, (float*)&dPdy, (float*)&dPdz, nchannels,
                                         (float *)result, (float *)dresultds,
                                         (float *)dresultdt, (float *)dresultdr);
            if (! ok) {
                std::string e = texsys->geterror ();
                if (! e.empty()) {
                    lock_guard lock (error_mutex);
                    std::cerr << "ERROR: " << e << "\n";
                }
            }

            // Save filtered pixels back to the image.
            for (int c = 0;  c < nchannels;  ++c)
                result[c] *= scalefactor;
            float *resultptr = (float *)image.pixeladdr (x, y);
            // FIXME: simplify by using SIMD scatter
            for (int c = 0;  c < nchannels;  ++c)
                for (int i = 0; i < npoints; ++i)
                    resultptr[c+i*nchannels_img] = result[c][i];
        }
    }
}



void
test_texture3d (ustring filename, Mapping3D mapping)
{
    std::cout << "Testing 3d texture " << filename << ", output = "
              << output_filename << "\n";
    int nchannels = nchannels_override ? nchannels_override : 4;
    ImageSpec outspec (output_xres, output_yres, nchannels, TypeDesc::FLOAT);
    ImageBuf image (outspec);
    TypeDesc fmt (dataformatname);
    image.set_write_format (fmt);
    OIIO::ImageBufAlgo::zero (image);

    for (int iter = 0;  iter < iters;  ++iter) {
        // Trick: switch to second texture, if given, for second iteration
        if (iter && filenames.size() > 1)
            filename = filenames[1];
        ImageBufAlgo::parallel_image (get_roi(image.spec()), nthreads,
                std::bind(tex3d_region, std::ref(image), filename, mapping, _1));
    }

    if (! image.write (output_filename)) 
        std::cerr << "Error writing " << output_filename 
                  << " : " << image.geterror() << "\n";
}



void
test_texture3d_batch (ustring filename, Mapping3DWide mapping)
{
    std::cout << "Testing 3d texture " << filename << ", output = "
              << output_filename << "\n";
    int nchannels = nchannels_override ? nchannels_override : 4;
    ImageSpec outspec (output_xres, output_yres, nchannels, TypeDesc::FLOAT);
    ImageBuf image (outspec);
    TypeDesc fmt (dataformatname);
    image.set_write_format (fmt);
    OIIO::ImageBufAlgo::zero (image);

    for (int iter = 0;  iter < iters;  ++iter) {
        // Trick: switch to second texture, if given, for second iteration
        if (iter && filenames.size() > 1)
            filename = filenames[1];
        ImageBufAlgo::parallel_image (get_roi(image.spec()), nthreads,
                [&](ROI roi){
                    tex3d_region_batch (image, filename, mapping, roi);
                });
    }

    if (! image.write (output_filename)) 
        std::cerr << "Error writing " << output_filename 
                  << " : " << image.geterror() << "\n";
}



static void
test_shadow (ustring filename)
{
}



static void
test_environment (ustring filename)
{
}



static void
test_getimagespec_gettexels (ustring filename)
{
    ImageSpec spec;
    int miplevel = 0;
    if (! texsys->get_imagespec (filename, 0, spec)) {
        std::cerr << "Could not get spec for " << filename << "\n";
        std::string e = texsys->geterror ();
        if (! e.empty()) {
            lock_guard lock (error_mutex);
            std::cerr << "ERROR: " << e << "\n";
        }
        return;
    }

    if (! test_gettexels)
        return;

    int w = std::min (spec.width, output_xres);
    int h = std::min (spec.height, output_yres);
    int nchannels = nchannels_override ? nchannels_override : spec.nchannels;
    ImageSpec postagespec (w, h, nchannels, TypeDesc::FLOAT);
    ImageBuf buf (postagespec);
    TextureOpt opt;
    initialize_opt (opt, nchannels);
    std::vector<float> tmp (w*h*nchannels);
    int x = spec.x + spec.width/2 - w/2;
    int y = spec.y + spec.height/2 - h/2;
    for (int i = 0; i < iters; ++i) {
        bool ok = texsys->get_texels (filename, opt, miplevel,
                                      x, x+w, y, y+h, 0, 1, 0, nchannels,
                                      postagespec.format, &tmp[0]);
        if (! ok)
            std::cerr << texsys->geterror() << "\n";
    }
    for (int y = 0;  y < h;  ++y)
        for (int x = 0;  x < w;  ++x) {
            imagesize_t texoffset = (y*w + x) * spec.nchannels;
            buf.setpixel (x, y, &tmp[texoffset]);
        }
    TypeDesc fmt (dataformatname);
    if (fmt != TypeDesc::UNKNOWN)
        buf.set_write_format (fmt);
    buf.write (output_filename);
}



static void
test_hash ()
{
    std::vector<size_t> fourbits (1<<4, 0);
    std::vector<size_t> eightbits (1<<8, 0);
    std::vector<size_t> sixteenbits (1<<16, 0);
    std::vector<size_t> highereightbits (1<<8, 0);

    const size_t iters = 1000000;
    const int res = 4*1024;  // Simulate tiles from a 4k image
    const int tilesize = 64;
    const int nfiles = iters / ((res/tilesize)*(res/tilesize));
    std::cout << "Testing hashing with " << nfiles << " files of "
              << res << 'x' << res << " with " << tilesize << 'x' << tilesize
              << " tiles:\n";

    ImageCache *imagecache = ImageCache::create ();

    // Set up the ImageCacheFiles outside of the timing loop
    using OIIO::pvt::ImageCacheImpl;
    using OIIO::pvt::ImageCacheFile;
    using OIIO::pvt::ImageCacheFileRef;
    std::vector<ImageCacheFileRef> icf;
    for (int f = 0;  f < nfiles;  ++f) {
        ustring filename = ustring::format ("%06d.tif", f);
        icf.push_back (new ImageCacheFile(*(ImageCacheImpl *)imagecache, NULL, filename));
    }

    // First, just try to do raw timings of the hash
    Timer timer;
    size_t i = 0, hh = 0;
    for (int f = 0;  f < nfiles;  ++f) {
        for (int y = 0;  y < res;  y += tilesize) {
            for (int x = 0;  x < res;  x += tilesize, ++i) {
                OIIO::pvt::TileID id (*icf[f], 0, 0, x, y, 0);
                size_t h = id.hash();
                hh += h;
            }
        }
    }
    std::cout << "hh = " << hh << "\n";
    double time = timer();
    double rate = (i/1.0e6) / time;
    std::cout << "Hashing rate: " << Strutil::format ("%3.2f", rate)
              << " Mhashes/sec\n";

    // Now, check the quality of the hash by looking at the low 4, 8, and
    // 16 bits and making sure that they divide into hash buckets fairly
    // evenly.
    i = 0;
    for (int f = 0;  f < nfiles;  ++f) {
        for (int y = 0;  y < res;  y += tilesize) {
            for (int x = 0;  x < res;  x += tilesize, ++i) {
                OIIO::pvt::TileID id (*icf[f], 0, 0, x, y, 0);
                size_t h = id.hash();
                ++ fourbits[h & 0xf];
                ++ eightbits[h & 0xff];
                ++ highereightbits[(h>>24) & 0xff];
                ++ sixteenbits[h & 0xffff];
                // if (i < 16) std::cout << Strutil::format("%llx\n", h);
            }
        }
    }

    size_t min, max;
    min = std::numeric_limits<size_t>::max();
    max = 0;
    for (int i = 0;  i < 16;  ++i) {
        if (fourbits[i] < min) min = fourbits[i];
        if (fourbits[i] > max) max = fourbits[i];
    }
    std::cout << "4-bit hash buckets range from "
              << min << " to " << max << "\n";

    min = std::numeric_limits<size_t>::max();
    max = 0;
    for (int i = 0;  i < 256;  ++i) {
        if (eightbits[i] < min) min = eightbits[i];
        if (eightbits[i] > max) max = eightbits[i];
    }
    std::cout << "8-bit hash buckets range from "
              << min << " to " << max << "\n";

    min = std::numeric_limits<size_t>::max();
    max = 0;
    for (int i = 0;  i < 256;  ++i) {
        if (highereightbits[i] < min) min = highereightbits[i];
        if (highereightbits[i] > max) max = highereightbits[i];
    }
    std::cout << "higher 8-bit hash buckets range from "
              << min << " to " << max << "\n";

    min = std::numeric_limits<size_t>::max();
    max = 0;
    for (int i = 0;  i < (1<<16);  ++i) {
        if (sixteenbits[i] < min) min = sixteenbits[i];
        if (sixteenbits[i] > max) max = sixteenbits[i];
    }
    std::cout << "16-bit hash buckets range from "
              << min << " to " << max << "\n";

    std::cout << "\n";

    ImageCache::destroy (imagecache);
}



static const char *workload_names[] = {
    /*0*/ "None",
    /*1*/ "Everybody accesses the same spot in one file (handles)",
    /*2*/ "Everybody accesses the same spot in one file",
    /*3*/ "Coherent access, one file, each thread in similar spots",
    /*4*/ "Coherent access, one file, each thread in different spots",
    /*5*/ "Coherent access, many files, each thread in similar spots",
    /*6*/ "Coherent access, many files, each thread in different spots",
    /*7*/ "Coherent access, many files, partially overlapping texture sets",
    /*8*/ "Coherent access, many files, partially overlapping texture sets, no extra busy work",
    NULL
};



void
do_tex_thread_workout (int iterations, int mythread)
{
    int nfiles = (int) filenames.size();
    float s = 0.1f, t = 0.1f;
    int nchannels = nchannels_override ? nchannels_override : 3;
    float *result = ALLOCA (float, nchannels);
    TextureOpt opt;
    initialize_opt (opt, nchannels);
    float *dresultds = test_derivs ? ALLOCA (float, nchannels) : NULL;
    float *dresultdt = test_derivs ? ALLOCA (float, nchannels) : NULL;
    TextureSystem::Perthread *perthread_info = texsys->get_perthread_info ();
    int pixel, whichfile = 0;

    std::vector<TextureSystem::TextureHandle *> texture_handles;
    for (auto f : filenames)
        texture_handles.emplace_back (texsys->get_texture_handle(f));

    ImageSpec spec0;
    texsys->get_imagespec (filenames[0], 0, spec0);
    // Compute a filter size that's between the first and second MIP levels.
    float fw = (1.0f / spec0.width) * 1.5f;
    float fh = (1.0f / spec0.height) * 1.5f;
    float dsdx = fw, dtdx = 0.0f, dsdy = 0.0f, dtdy = fh;

    for (int i = 0;  i < iterations;  ++i) {
        pixel = i;
        bool ok = false;
        // Several different texture access patterns
        switch (threadtimes) {
        case 1:
            // Workload 1: Speed of light: Static texture access (same
            // texture coordinates all the time, one file), with handles
            // and per-thread data already queried only once rather than
            // per-call.
            ok = texsys->texture (texture_handles[0], perthread_info, opt, s, t,
                                  dsdx, dtdx, dsdy, dtdy, nchannels,
                                  result, dresultds, dresultdt);
            break;
        case 2:
            // Workload 2: Static texture access, with filenames.
            ok = texsys->texture (filenames[0], opt, s, t,
                                  dsdx, dtdx, dsdy, dtdy, nchannels,
                                  result, dresultds, dresultdt);
            break;
        case 3:
        case 4:
            // Workload 3: One file, coherent texture coordinates.
            //
            // Workload 4: Each thread starts with a different texture
            // coordinate offset, so likely are not simultaneously
            // accessing the very same tile as the other threads.
            if (threadtimes == 4)
                pixel += 57557*mythread;
            break;
        case 5:
        case 6:
            // Workload 5: Coherent texture coordinates, but access
            // a series of textures at each coordinate.
            //
            // Workload 6: Each thread starts with a different texture
            // coordinate offset, so likely are not simultaneously
            // accessing the very same tile as the other threads.
            whichfile = i % nfiles;
            pixel = i / nfiles;
            if (threadtimes == 6)
                pixel += 57557*mythread;
            break;
        case 7:
        case 8:
            // Workload 7: Coherent texture coordinates, but access
            // a series of textures at each coordinate, which partially
            // overlap with other threads.
            {
            int file = i % 8;
            if (file < 2)        // everybody accesses the first 2 files
                whichfile = std::min (file, nfiles-1);
            else                 // and a slowly changing set of 6 others
                whichfile = (file+11*mythread+i/1000) % nfiles;
            pixel = i / nfiles;
            pixel += 57557*mythread;
            }
            break;
        default:
            ASSERT_MSG (0, "Unkonwn thread work pattern %d", threadtimes);
        }
        if (! ok) {
            s = (((2*pixel) % spec0.width) + 0.5f) / spec0.width;
            t = (((2*((2*pixel) / spec0.width)) % spec0.height) + 0.5f) / spec0.height;
            if (use_handle)
                ok = texsys->texture (texture_handles[whichfile],
                                      perthread_info, opt, s, t,
                                      dsdx, dtdx, dsdy, dtdy, nchannels,
                                      result, dresultds, dresultdt);
            else
                ok = texsys->texture (filenames[whichfile], opt, s, t,
                                      dsdx, dtdx, dsdy, dtdy, nchannels,
                                      result, dresultds, dresultdt);
        }
        if (! ok) {
            lock_guard lock (error_mutex);
            std::cerr << "Unexpected error: " << texsys->geterror() << "\n";
            return;
        }
        // Do some pointless work, to simulate that in a real app, there
        // would be operations interspersed with texture accesses.
        if (threadtimes != 8 /* skip on this test */) {
            for (int j = 0;  j < 30;  ++j)
                for (int c = 0;  c < nchannels;  ++c)
                    result[c] = cosf (result[c]);
        }
    }
    // Force the compiler to not optimize away the "other work"
    for (int c = 0;  c < nchannels;  ++c)
        ASSERT (! isnan(result[c]));
}



// Launch numthreads threads each of which performs a workout of texture
// accesses.
void
launch_tex_threads (int numthreads, int iterations)
{
    texsys->invalidate_all (true);
    OIIO::thread_group threads;
    for (int i = 0;  i < numthreads;  ++i) {
        threads.create_thread (std::bind(do_tex_thread_workout,iterations,i));
    }
    ASSERT ((int)threads.size() == numthreads);
    threads.join_all ();
}



class GridImageInput final : public ImageInput {
public:
    GridImageInput () : m_miplevel(-1) { }
    virtual ~GridImageInput () { close(); }
    virtual const char * format_name (void) const { return "grid"; }
    virtual bool valid_file (const std::string &filename) const { return true; }
    virtual bool open (const std::string &name, ImageSpec &newspec) {
        return seek_subimage (0, 0, newspec);
    }
    virtual bool close () { return true; }
    virtual int current_miplevel (void) const { return m_miplevel; }
    virtual bool seek_subimage (int subimage, int miplevel, ImageSpec &newspec) {
        if (subimage > 0)
            return false;
        if (miplevel > 0 && automip /* if automip is on, don't generate MIP */)
            return false;
        if (miplevel == m_miplevel)
            return true;
        int res = 512;
        res >>= miplevel;
        if (res == 0)
            return false;
        m_spec = ImageSpec (res, res, 3, TypeDesc::FLOAT);
        m_spec.tile_width = std::min (64, res);
        m_spec.tile_height = std::min (64, res);
        m_spec.tile_depth = 1;
        newspec = m_spec;
        m_miplevel = miplevel;
        return true;
    }
    virtual bool read_native_scanline (int y, int z, void *data) { return false; }
    virtual bool read_native_tile (int xbegin, int ybegin, int zbegin, void *data) {
        float *tile = (float *)data;
        for (int z = zbegin, zend = z+m_spec.tile_depth; z < zend; ++z)
            for (int y = ybegin, yend = y+m_spec.tile_height; y < yend; ++y)
                for (int x = xbegin, xend = x+m_spec.tile_width; x < xend; ++x) {
                    tile[0] = float(x)/m_spec.width;
                    tile[2] = float(y)/m_spec.height;
                    tile[1] = (((x/16)&1) == ((y/16)&1)) ? 1.0f/(m_miplevel+1) : 0.05f;
                    tile += m_spec.nchannels;
                }
        return true;
    }
private:
    int m_miplevel;
};



ImageInput *make_grid_input () { return new GridImageInput; }



void
test_icwrite (int testicwrite)
{
    std::cout << "Testing IC write, mode " << testicwrite << "\n";

    // The global "shared" ImageCache will be the same one the
    // TextureSystem uses.
    ImageCache *ic = ImageCache::create ();

    // Set up the fake file ane add it
    int tw = 64, th = 64;  // tile width and height
    int nc = nchannels_override ? nchannels_override : 3;  // channels
    ImageSpec spec (512, 512, nc, TypeDesc::FLOAT);
    spec.depth = 1;
    spec.tile_width = tw;
    spec.tile_height = th;
    spec.tile_depth = 1;
    ustring filename (filenames[0]);
    bool ok = ic->add_file (filename, make_grid_input);
    if (! ok)
        std::cout << "ic->add_file error: " << ic->geterror() << "\n";
    ASSERT (ok);

    // Now add all the tiles if it's a seeded map
    // testicwrite == 1 means to seed the first MIP level using add_tile.
    // testicwrite == 2 does not use add_tile, but instead will rely on
    // the make_grid_input custom ImageInput that constructs a pattern
    // procedurally.
    if (testicwrite == 1) {
        std::vector<float> tile (spec.tile_pixels() * spec.nchannels);
        for (int ty = 0;  ty < spec.height;  ty += th) {
            for (int tx = 0;  tx < spec.width;  tx += tw) {
                // Construct a tile
                for (int y = 0; y < th; ++y)
                    for (int x = 0; x < tw; ++x) {
                        int index = (y*tw + x) * nc;
                        int xx = x+tx, yy = y+ty;
                        tile[index+0] = float(xx)/spec.width;
                        tile[index+1] = float(yy)/spec.height;
                        tile[index+2] = (!(xx%10) || !(yy%10)) ? 1.0f : 0.0f;
                    }
                bool ok = ic->add_tile (filename, 0, 0, tx, ty, 0, 0, -1,
                                        TypeDesc::FLOAT, &tile[0]);
                if (! ok) {
                    lock_guard lock (error_mutex);
                    std::cout << "ic->add_tile error: " << ic->geterror() << "\n";
                }
                ASSERT (ok);
            }
        }
    }
}



int
main (int argc, const char *argv[])
{
    Filesystem::convert_native_arguments (argc, argv);
    getargs (argc, argv);

    // environment variable TESTTEX_BATCH can force batch mode
    string_view testtex_batch = Sysutil::getenv("TESTTEX_BATCH");
    if (testtex_batch.size())
        batch = Strutil::from_string<int>(testtex_batch);

    OIIO::attribute ("threads", nthreads);

    texsys = TextureSystem::create ();
    std::cout << "Created texture system\n";
    texsys->attribute ("statistics:level", 2);
    texsys->attribute ("autotile", autotile);
    texsys->attribute ("automip", (int)automip);
    texsys->attribute ("deduplicate", (int)dedup);
    if (cachesize >= 0)
        texsys->attribute ("max_memory_MB", cachesize);
    else
        texsys->getattribute ("max_memory_MB", TypeFloat, &cachesize);
    if (maxfiles >= 0)
        texsys->attribute ("max_open_files", maxfiles);
    if (searchpath.length())
        texsys->attribute ("searchpath", searchpath);
    if (nountiled)
        texsys->attribute ("accept_untiled", 0);
    if (nounmipped)
        texsys->attribute ("accept_unmipped", 0);
    texsys->attribute ("gray_to_rgb", gray_to_rgb);
    texsys->attribute ("flip_t", flip_t);

    if (test_construction) {
        Timer t;
        for (int i = 0;  i < 1000000000;  ++i) {
            TextureOpt opt;
            dummyptr = &opt;  // This forces the optimizer to keep the loop
        }
        std::cout << "TextureOpt construction: " << t() << " ns\n";
        TextureOpt canonical, copy;
        t.reset();
        t.start();
        for (int i = 0;  i < 1000000000;  ++i) {
            memcpy (&copy, &canonical, sizeof(TextureOpt));
            dummyptr = &copy;  // This forces the optimizer to keep the loop
        }
        std::cout << "TextureOpt memcpy: " << t() << " ns\n";
    }

    if (testicwrite && filenames.size()) {
        test_icwrite (testicwrite);
    }

    if (test_getimagespec) {
        ImageSpec spec;
        for (int i = 0;  i < iters;  ++i) {
            texsys->get_imagespec (filenames[0], 0, spec);
        }
        iters = 0;
    }

    if (test_gettexels) {
        test_getimagespec_gettexels (filenames[0]);
        iters = 0;
    }

    if (testhash) {
        test_hash ();
    }

    Imath::M33f scale;  scale.scale (Imath::V2f (0.3, 0.3));
    Imath::M33f rot;    rot.rotate (radians(25.0f));
    Imath::M33f trans;  trans.translate (Imath::V2f (0.75f, 0.25f));
    Imath::M33f persp (2, 0, 0,
                       0, 0.8, -0.55,
                       0, 0, 1);
    xform = persp * rot * trans * scale;
    xform.invert();

    if (threadtimes) {
        // If the --iters flag was used, do that number of iterations total
        // (divided among the threads). If not supplied (iters will be 1),
        // then use a large constant *per thread*.
        const int iterations = iters>1 ? iters : 2000000;
        std::cout << "Workload: " << workload_names[threadtimes] << "\n";
        std::cout << "texture cache size = " << cachesize << " MB\n";
        std::cout << "hw threads = " << Sysutil::hardware_concurrency() << "\n";
        std::cout << "times are best of " << ntrials << " trials\n\n";
        std::cout << "threads  time (s)   speedup efficiency\n";
        std::cout << "-------- -------- --------- ----------\n";

        if (nthreads == 0)
            nthreads = Sysutil::hardware_concurrency();
        static int threadcounts[] = { 1, 2, 4, 8, 12, 16, 24, 32, 64, 128, 1024, 1<<30 };
        float single_thread_time = 0.0f;
        for (int i = 0; threadcounts[i] <= nthreads; ++i) {
            int nt = wedge ? threadcounts[i] : nthreads;
            int its = iters>1 ? (std::max (1, iters/nt)) : iterations; // / nt;
            double range;
            double t = time_trial (std::bind(launch_tex_threads,nt,its),
                                   ntrials, &range);
            if (nt == 1)
                single_thread_time = (float)t;
            float speedup = (single_thread_time /*/nt*/) / (float)t;
            float efficiency = (single_thread_time / nt) / float(t);
            std::cout << Strutil::format ("%3d     %8.2f   %6.1fx  %6.1f%%    range %.2f\t(%d iters/thread)\n",
                                          nt, t, speedup, efficiency*100.0f, range, its);
            std::cout.flush();
            if (! wedge)
                break;    // don't loop if we're not wedging
        }
        std::cout << "\n";

    } else if (iters > 0 && filenames.size()) {
        ustring filename (filenames[0]);
        test_gettextureinfo (filenames[0]);
        const char *texturetype = "Plain Texture";
        texsys->get_texture_info (filename, 0, ustring("texturetype"),
                                  TypeDesc::STRING, &texturetype);
        Timer timer;
        if (! strcmp (texturetype, "Plain Texture")) {
            if (batch) {
                if (nowarp)
                    test_plain_texture_batch (map_default);
                else if (tube)
                    test_plain_texture_batch (map_tube);
                else if (filtertest)
                    test_plain_texture_batch (map_filtertest);
                else
                    test_plain_texture_batch (map_warp);

            } else {
                if (nowarp)
                    test_plain_texture (map_default);
                else if (tube)
                    test_plain_texture (map_tube);
                else if (filtertest)
                    test_plain_texture (map_filtertest);
                else
                    test_plain_texture (map_warp);
            }
        }
        if (! strcmp (texturetype, "Volume Texture")) {
            if (batch) {
                if (nowarp)
                    test_texture3d_batch (filename, map_default_3D);
                else
                    test_texture3d_batch (filename, map_warp_3D);
            } else {
                if (nowarp)
                    test_texture3d (filename, map_default_3D);
                else
                    test_texture3d (filename, map_warp_3D);
            }
        }
        if (! strcmp (texturetype, "Shadow")) {
            test_shadow (filename);
        }
        if (! strcmp (texturetype, "Environment")) {
            test_environment (filename);
        }
        test_getimagespec_gettexels (filename);
        std::cout << "Time: " << Strutil::timeintervalformat (timer()) << "\n";
    }

    if (test_statquery) {
        std::cout << "Testing statistics queries:\n";
        int total_files = 0;
        texsys->getattribute ("total_files", total_files);
        std::cout << "  Total files: " << total_files << "\n";
        std::vector<ustring> all_filenames (total_files);
        std::cout << TypeDesc(TypeDesc::STRING,total_files) << "\n";
        texsys->getattribute ("all_filenames", TypeDesc(TypeDesc::STRING,total_files), &all_filenames[0]);
        for (int i = 0; i < total_files; ++i) {
            int timesopened = 0;
            int64_t bytesread = 0;
            float iotime = 0.0f;
            int64_t data_size = 0, file_size = 0;
            texsys->get_texture_info (all_filenames[i], 0, ustring("stat:timesopened"),
                                      TypeDesc::INT, &timesopened);
            texsys->get_texture_info (all_filenames[i], 0, ustring("stat:bytesread"),
                                      TypeDesc::INT64, &bytesread);
            texsys->get_texture_info (all_filenames[i], 0, ustring("stat:iotime"),
                                      TypeDesc::FLOAT, &iotime);
            texsys->get_texture_info (all_filenames[i], 0, ustring("stat:image_size"),
                                      TypeDesc::INT64, &data_size);
            texsys->get_texture_info (all_filenames[i], 0, ustring("stat:file_size"),
                                      TypeDesc::INT64, &file_size);
            std::cout << Strutil::format ("  %d: %s  opens=%d, read=%s, time=%s, data=%s, file=%s\n",
                                          i, all_filenames[i], timesopened,
                                          Strutil::memformat(bytesread),
                                          Strutil::timeintervalformat(iotime,2),
                                          Strutil::memformat(data_size),
                                          Strutil::memformat(file_size));
        }
    }

    std::cout << "Memory use: "
              << Strutil::memformat (Sysutil::memory_used(true)) << "\n";
    TextureSystem::destroy (texsys);

    std::cout << "\nustrings: " << ustring::getstats(false) << "\n\n";
    return 0;
}
