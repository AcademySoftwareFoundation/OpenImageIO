// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <iterator>

#include <OpenImageIO/Imath.h>
#include <OpenImageIO/argparse.h>
#include <OpenImageIO/benchmark.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/imagecache.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/texture.h>
#include <OpenImageIO/timer.h>
#include <OpenImageIO/ustring.h>

using namespace OIIO;

using OIIO::_1;

static std::vector<ustring> filenames;
static std::string output_filename = "out.exr";
static bool verbose                = false;
static int nthreads                = 0;
static int minthreads              = 1;
static int threadtimes             = 0;
static int output_xres = 512, output_yres = 512;
static int nchannels_override     = 0;
static std::string dataformatname = "half";
static float sscale = 1, tscale = 1;
static float sblur = 0, tblur = -1;
static float width       = 1;
static float widthramp   = 0;
static float anisoaspect = 1.0;  // anisotropic aspect ratio
static std::string wrapmodes("periodic");
static std::string texcolorspace;
static int texcolortransform_id = 0;
static int anisomax             = TextureOpt().anisotropic;
static int iters                = 1;
static int autotile             = 0;
static bool automip             = false;
static bool dedup               = true;
static bool test_construction   = false;
static bool test_gettexels      = false;
static bool test_getimagespec   = false;
static bool filtertest          = false;
static std::shared_ptr<TextureSystem> texsys;
static std::string searchpath;
static bool batch         = false;
static bool nowarp        = false;
static bool tube          = false;
static bool use_handle    = false;
static bool use_bluenoise = false;
static float cachesize    = -1;
static int maxfiles       = -1;
static int mipmode        = int(TextureOpt::MipModeDefault);
static int interpmode     = int(TextureOpt::InterpSmartBicubic);
static int stochastic     = 0;
static float missing[4]   = { -1, 0, 0, 1 };
static float fill         = -1;  // -1 signifies unset
static float scalefactor  = 1.0f;
static Imath::V3f texoffset(0, 0, 0);
static bool nountiled              = false;
static bool nounmipped             = false;
static bool gray_to_rgb            = false;
static bool flip_t                 = false;
static bool resetstats             = false;
static bool testhash               = false;
static bool wedge                  = false;
static int ntrials                 = 1;
static int lowtrials               = 10;
static int testicwrite             = 0;
static bool test_derivs            = false;
static bool test_statquery         = false;
static bool invalidate_before_iter = true;
static bool close_before_iter      = false;
static bool runstats               = false;
static bool udim_tests             = false;
static bool do_gettextureinfo      = true;
static int subimage                = -1;
static std::string subimagename;
static Imath::M33f xform;
static std::string texoptions;
static std::string gtiname;
static std::string maketest_template;
static int maketest_res   = 2048;
static int maketest_chans = 4;
static int num_test_files = 0;
static std::vector<std::string> filenames_to_delete;
const int pieces_per_udim = 20;
static std::vector<TextureSystem::TextureHandle*> texture_handles;
void* dummyptr;
static const ImageBuf& bluenoiseimg(ImageBufAlgo::bluenoise_image());

typedef void (*Mapping2D)(const int&, const int&, float&, float&, float&,
                          float&, float&, float&);
typedef void (*Mapping3D)(const int&, const int&, Imath::V3f&, Imath::V3f&,
                          Imath::V3f&, Imath::V3f&);

typedef void (*MappingEnv)(const int&, const int&, Imath::V3f&, Imath::V3f&,
                           Imath::V3f&);

typedef void (*Mapping2DWide)(const Tex::IntWide&, const Tex::IntWide&,
                              Tex::FloatWide&, Tex::FloatWide&, Tex::FloatWide&,
                              Tex::FloatWide&, Tex::FloatWide&,
                              Tex::FloatWide&);
typedef void (*Mapping3DWide)(const Tex::IntWide&, const Tex::IntWide&,
                              Imath::Vec3<Tex::FloatWide>&,
                              Imath::Vec3<Tex::FloatWide>&,
                              Imath::Vec3<Tex::FloatWide>&,
                              Imath::Vec3<Tex::FloatWide>&);

typedef void (*MappingEnvWide)(const Tex::IntWide&, const Tex::IntWide&,
                               Imath::Vec3<Tex::FloatWide>&,
                               Imath::Vec3<Tex::FloatWide>&,
                               Imath::Vec3<Tex::FloatWide>&);



static void
getargs(int argc, const char* argv[])
{
    // clang-format off
    ArgParse ap;
    ap.usage ("testtex [options] inputfile");
    ap.arg("filename")
      .hidden()
      .action([&](cspan<const char*> argv){ filenames.emplace_back(argv[0]); });
    ap.arg("-v", &verbose)
      .help("Verbose status messages");

    ap.arg("-o %s:FILENAME", &output_filename)
      .help("Output test image");
    ap.arg("-d %s:TYPE", &dataformatname)
      .help("Set the output data format to one of: "
            "uint8, sint8, uint10, uint12, uint16, sint16, half, float, double");
    ap.arg("--res %d:WIDTH %d:HEIGHT", &output_xres, &output_yres)
      .help("Resolution of output test image");
    ap.arg("--nchannels %d:N", &nchannels_override)
      .help("Force number of channels to look up");
    ap.arg("--iters %d:N", &iters)
      .help("Iterations for time trials");
    ap.arg("--threads %d:N", &nthreads)
      .help("Number of threads (default 0 = #cores)");
    ap.arg("-t %d", &nthreads)
       .hidden();  // synonym
    ap.arg("--texoptions %s:OPTLIST", &texoptions)
      .help("Set extra TextureSystem options (name=val,name=val)");
    ap.arg("--blur %f:BLURSIZE", &sblur)
      .help("Add blur to texture lookup");
    ap.arg("--stblur %f:SBLUR %f:TBLUR", &sblur, &tblur)
      .help("Add blur (s, t) to texture lookup");
    ap.arg("--width %f:WIDTH", &width)
      .help("Multiply filter width of texture lookup");
    ap.arg("--widthramp %f:WIDTH", &widthramp)
      .help("If set, ramp to this width on the right side");
    ap.arg("--fill %f:FILLVAL", &fill)
      .help("Set fill value for missing channels");
    ap.arg("--wrap %s:MODE", &wrapmodes)
      .help("Set wrap mode (default, black, clamp, periodic, mirror, overscan)");
    ap.arg("--texcolorspace %s:NAME", &texcolorspace)
      .help("Set texture presumed color space");
    ap.arg("--anisoaspect %f:ASPECT", &anisoaspect)
      .help("Set anisotropic ellipse aspect ratio for threadtimes tests (default: 2.0)");
    ap.arg("--anisomax %d:MAX", &anisomax)
      .help(Strutil::fmt::format("Set max anisotropy (default: {})", anisomax));
    ap.arg("--mipmode %d:MODE", &mipmode)
      .help("Set mip mode (default: 0 = aniso)");
    ap.arg("--interpmode %d:MODE", &interpmode)
      .help("Set interp mode (default: 3 = smart bicubic)");
    ap.arg("--stochastic %d:MODE", &stochastic)
      .help("Set stochastic sampling mode (default: 0 = none)");
    ap.arg("--missing %f:R %f:G %f:B", &missing[0], &missing[1], &missing[2])
      .help("Specify missing texture color");
    ap.arg("--autotile %d:TILESIZE", &autotile)
      .help("Set auto-tile size for the image cache");
    ap.arg("--automip", &automip)
      .help("Set auto-MIPmap for the image cache");
    ap.arg("--batch", &batch)
      .help(Strutil::fmt::format("Use batched shading, batch size = {}", Tex::BatchWidth));
    ap.arg("--handle", &use_handle)
      .help("Use texture handle rather than name lookup");
    ap.arg("--searchpath %s:PATHLIST", &searchpath)
      .help("Search path for files (colon-separated directory list)");
    ap.arg("--filtertest", &filtertest)
      .help("Test the filter sizes");
    ap.arg("--nowarp", &nowarp)
      .help("Do not warp the image->texture mapping");
    ap.arg("--tube", &tube)
      .help("Make a tube projection");
    ap.arg("--ctr", &test_construction)
      .help("Test TextureOpt construction time");
    ap.arg("--gettexels", &test_gettexels)
      .help("Test TextureSystem::get_texels");
    ap.arg("--getimagespec", &test_getimagespec)
      .help("Test TextureSystem::get_imagespec");
    ap.arg("--gettextureinfo %s:NAME", &gtiname)
      .help("Test gettextureinfo, retrieving attrib 'NAME'");
    ap.arg("--offset %f:SOFF %f:TOFF %f:ROFF", &texoffset.x, &texoffset.y, &texoffset.z)
      .help("Offset texture coordinates");
    ap.arg("--scalest %f:SSCALE %f:TSCALE", &sscale, &tscale)
      .help("Scale texture lookups (s, t)");
    ap.arg("--cachesize %f:MB", &cachesize)
      .help("Set cache size, in MB");
    ap.arg("--nodedup %!", &dedup)
      .help("Turn off de-duplication");
    ap.arg("--scale %f:SCALEFACTOR", &scalefactor)
      .help("Scale intensities");
    ap.arg("--maxfiles %d:MAXFILES", &maxfiles)
      .help("Set maximum open files");
    ap.arg("--nountiled", &nountiled)
      .help("Reject untiled images");
    ap.arg("--nounmipped", &nounmipped)
      .help("Reject unmipped images");
    ap.arg("--graytorgb", &gray_to_rgb)
      .help("Convert gratscale textures to RGB");
    ap.arg("--flipt", &flip_t)
      .help("Flip direction of t coordinate");
    ap.arg("--derivs", &test_derivs)
      .help("Test returning derivatives of texture lookups");
    ap.arg("--resetstats", &resetstats)
      .help("Print and reset statistics on each iteration");
    ap.arg("--testhash", &testhash)
      .help("Test the tile hashing function");
    ap.arg("--threadtimes %d:MODE", &threadtimes)
      .help("Do thread timings (arg = workload profile)");
    ap.arg("--trials %d:N", &ntrials)
      .help("Number of trials for timings");
    ap.arg("--lowtrials %d:N", &lowtrials)
      .help("Optional lower number of trials for <= 2 thread timings");
    ap.arg("--wedge", &wedge)
      .help("Wedge test");
    ap.arg("--minthreads %d:N", &minthreads)
      .help("Minimum number of threads for wedges (default: 1)");
    ap.arg("--noinvalidate %!", &invalidate_before_iter)
      .help("Don't invalidate the cache before each --threadtimes trial");
    ap.arg("--closebeforeiter", &close_before_iter)
      .help("Close all handles before each --iter");
    ap.arg("--testicwrite %d:MODE", &testicwrite)
      .help("Test ImageCache write ability (1=seeded, 2=generated)");
    ap.arg("--teststatquery", &test_statquery)
      .help("Test queries of statistics");
    ap.arg("--runstats", &runstats)
      .help("Print runtime statistics");
    ap.arg("--maketests %d:NUMFILES %s:TEMPLATE", &num_test_files,  &maketest_template)
      .help("Make tests from a template (e.g., \"tmp/test{:04}.exr\")");
    ap.arg("--maketest-res %d:RES", &maketest_res)
      .help("Resolution for maketests (default: 2048)");
    ap.arg("--maketest-chans %d:NCHANS", &maketest_chans)
      .help("Channels for maketests (default: 4)");
    ap.arg("--udim", &udim_tests)
      .help("Do udim-oriented tests");
    ap.arg("--bluenoise", &use_bluenoise)
      .help("Use blue noise for stochastic choices");
    ap.arg("--no-gettextureinfo %!", &do_gettextureinfo)
      .help("Do not test gettextureinfo");
    ap.arg("--subimage %d:INDEX", &subimage)
      .help("Use the specified subimage (by index)");
    ap.arg("--subimagename %s:NAME", &subimagename)
      .help("Use the specified subimage (by name)");

    // clang-format on
    ap.parse(argc, argv);

    if (filenames.size() < 1 && !num_test_files && !test_construction
        && !test_getimagespec && !testhash) {
        std::cerr << "testtex: Must have at least one input file\n";
        ap.usage();
        exit(EXIT_FAILURE);
    }
}



static void
initialize_opt(TextureOpt& opt)
{
    opt.sblur  = sblur;
    opt.tblur  = tblur >= 0.0f ? tblur : sblur;
    opt.rblur  = sblur;
    opt.swidth = width;
    opt.twidth = width;
    opt.rwidth = width;
    opt.fill   = (fill >= 0.0f) ? fill : 1.0f;
    if (missing[0] >= 0)
        opt.missingcolor = (float*)&missing;
    TextureOpt::parse_wrapmodes(wrapmodes.c_str(), opt.swrap, opt.twrap);
    opt.rwrap       = opt.swrap;
    opt.anisotropic = anisomax;
    opt.mipmode     = (TextureOpt::MipMode)mipmode;
    opt.interpmode  = (TextureOpt::InterpMode)interpmode;
    if (subimage >= 0)
        opt.subimage = subimage;
    else if (!subimagename.empty())
        opt.subimagename = ustring(subimagename);
    opt.colortransformid = texcolortransform_id;
}



static void
initialize_opt(TextureOptBatch& opt)
{
    using namespace Tex;
    FloatWide sb(sblur);
    sb.store(opt.sblur);
    FloatWide tb(tblur >= 0.0f ? tblur : sblur);
    tb.store(opt.tblur);
    sb.store(opt.rblur);
    FloatWide w(width);
    w.store(opt.swidth);
    w.store(opt.twidth);
    w.store(opt.rwidth);
    opt.fill = (fill >= 0.0f) ? fill : 1.0f;
    if (missing[0] >= 0)
        opt.missingcolor = (float*)&missing;

#if OIIO_TEXTUREOPTBATCH_VERSION == 1
    // Current layout of TextureOptBatch_v1
    Wrap sw, tw;
    Tex::parse_wrapmodes(wrapmodes.c_str(), sw, tw);
    opt.swrap       = int(sw);
    opt.twrap       = int(tw);
    opt.rwrap       = opt.swrap;
    opt.anisotropic = anisomax;
    opt.mipmode     = int(mipmode);     // ideal: MipMode(mipmode);
    opt.interpmode  = int(interpmode);  // ideal: InterpMode(interpmode);
#else
    // Some day, maybe for TextureOptBatch_v2, we'd like to switch to this to
    // completely match the types and layout of TextureOpt.
    Tex::parse_wrapmodes(wrapmodes.c_str(), opt.swrap, opt.twrap);
    opt.rwrap       = opt.swrap;
    opt.anisotropic = anisomax;
    opt.mipmode     = MipMode(mipmode);
    opt.interpmode  = InterpMode(interpmode);
#endif


    if (subimage >= 0)
        opt.subimage = subimage;
    else if (!subimagename.empty())
        opt.subimagename = ustring(subimagename);
    opt.colortransformid = texcolortransform_id;
}



static void
test_gettextureinfo(ustring filename)
{
    bool ok;

    int res[3] = { 0 };

    ok = texsys->get_texture_info(filename, 0, ustring("resolution"),
                                  TypeDesc(TypeDesc::INT, 2), res);
    Strutil::print(
        "Result of get_texture_info resolution (as int[2]) = {} {}x{}\n", ok,
        res[0], res[1]);
    ok = texsys->get_texture_info(filename, 0, ustring("resolution"),
                                  TypeDesc(TypeDesc::INT, 3), res);
    Strutil::print(
        "Result of get_texture_info resolution (as int[3]) = {} {}x{}x{}\n", ok,
        res[0], res[1], res[2]);

    int chan = 0;

    ok = texsys->get_texture_info(filename, 0, ustring("channels"),
                                  TypeDesc::INT, &chan);
    Strutil::print("Result of get_texture_info channels = {} {}\n", ok, chan);

    float fchan = 0;

    ok = texsys->get_texture_info(filename, 0, ustring("channels"),
                                  TypeDesc::FLOAT, &fchan);
    Strutil::print("Result of get_texture_info channels = {} {:g}\n", ok,
                   fchan);

    int dataformat = 0;
    ok = texsys->get_texture_info(filename, 0, ustring("format"), TypeDesc::INT,
                                  &dataformat);
    Strutil::print("Result of get_texture_info data format = {} {}\n", ok,
                   TypeDesc((TypeDesc::BASETYPE)dataformat).c_str());

    int window[6];
    ok = texsys->get_texture_info(filename, 0, ustring("datawindow"),
                                  TypeDesc("int[4]"), window);
    Strutil::print(
        "Result of get_texture_info datawindow (as int[4]) = {} [{} {} {} {}]\n",
        ok, window[0], window[1], window[2], window[3]);
    ok = texsys->get_texture_info(filename, 0, ustring("datawindow"),
                                  TypeDesc("int[6]"), window);
    Strutil::print(
        "Result of get_texture_info datawindow (as int[6]) = {} [{} {} {} {} {} {}]\n",
        ok, window[0], window[1], window[2], window[3], window[4], window[5]);

    ok = texsys->get_texture_info(filename, 0, ustring("displaywindow"),
                                  TypeDesc("int[4]"), window);
    Strutil::print(
        "Result of get_texture_info displaywindow (as int[4]) = {} [{} {} {} {}]\n",
        ok, window[0], window[1], window[2], window[3]);
    ok = texsys->get_texture_info(filename, 0, ustring("displaywindow"),
                                  TypeDesc("int[6]"), window);
    Strutil::print(
        "Result of get_texture_info displaywindow (as int[6]) = {} [{} {} {} {} {} {}]\n",
        ok, window[0], window[1], window[2], window[3], window[4], window[5]);

    float avg[4];
    ok = texsys->get_texture_info(filename, 0, ustring("averagecolor"),
                                  TypeDesc(TypeDesc::FLOAT, 4), avg);
    Strutil::print("Result of get_texture_info averagecolor = {}",
                   ok ? "yes" : "no\n");
    if (ok)
        Strutil::print(" {:g} {:g} {:g} {:g}\n", avg[0], avg[1], avg[2],
                       avg[3]);
    ok = texsys->get_texture_info(filename, 0, ustring("averagealpha"),
                                  TypeFloat, avg);
    Strutil::print("Result of get_texture_info averagealpha = {}",
                   ok ? "yes" : "no\n");
    if (ok)
        Strutil::print(" {:g}\n", avg[0]);
    ok = texsys->get_texture_info(filename, 0, ustring("constantcolor"),
                                  TypeDesc(TypeDesc::FLOAT, 4), avg);
    Strutil::print("Result of get_texture_info constantcolor = {}",
                   ok ? "yes" : "no\n");
    if (ok)
        Strutil::print(" {:g} {:g} {:g} {:g}\n", avg[0], avg[1], avg[2],
                       avg[3]);
    ok = texsys->get_texture_info(filename, 0, ustring("constantalpha"),
                                  TypeFloat, avg);
    Strutil::print("Result of get_texture_info constantalpha = {}",
                   ok ? "yes" : "no\n");
    if (ok)
        Strutil::print(" {:g}\n", avg[0]);

    const char* texturetype = NULL;
    ok = texsys->get_texture_info(filename, 0, ustring("textureformat"),
                                  TypeDesc::STRING, &texturetype);
    Strutil::print("Texture type is {} {}\n", ok,
                   texturetype ? texturetype : "");
    Strutil::print("\n");
}



template<typename Float = float>
inline Imath::Vec2<Float>
warp(const Float& x, const Float& y, const Imath::M33f& xform)
{
    Imath::Vec2<Float> coord(x, y);
    xform.multVecMatrix(coord, coord);
    return coord;
}


template<typename Float = float>
inline Imath::Vec2<Float>
warp_coord(const Float& x, const Float& y)
{
    Imath::Vec2<Float> coord = warp(x / output_xres, y / output_yres, xform);
    coord.x *= sscale;
    coord.y *= tscale;
    coord += Imath::Vec2<Float>(texoffset.x, texoffset.y);
    return coord;
}



// Just map pixels to [0,1] st space
template<typename Float = float, typename Int = int>
void
map_default(const Int& x, const Int& y, Float& s, Float& t, Float& dsdx,
            Float& dtdx, Float& dsdy, Float& dtdy)
{
    s    = (Float(x) + 0.5f) / output_xres * sscale + texoffset.x;
    t    = (Float(y) + 0.5f) / output_yres * tscale + texoffset.y;
    dsdx = 1.0f / output_xres * sscale;
    dtdx = 0.0f;
    dsdy = 0.0f;
    dtdy = 1.0f / output_yres * tscale;
}



template<typename Float = float, typename Int = int>
static void
map_warp(const Int& x, const Int& y, Float& s, Float& t, Float& dsdx,
         Float& dtdx, Float& dsdy, Float& dtdy)
{
    Imath::Vec2<Float> coord  = warp_coord(Float(x) + 0.5f, Float(y) + 0.5f);
    Imath::Vec2<Float> coordx = warp_coord(Float(x) + 1.5f, Float(y) + 0.5f)
                                - coord;
    Imath::Vec2<Float> coordy = warp_coord(Float(x) + 0.5f, Float(y) + 1.5f)
                                - coord;

    s    = coord.x;
    t    = coord.y;
    dsdx = coordx.x;
    dtdx = coordx.y;
    dsdy = coordy.x;
    dtdy = coordy.y;
}



static void
map_tube(const int& x, const int& y, float& s, float& t, float& dsdx,
         float& dtdx, float& dsdy, float& dtdy)
{
    float xt     = (float(x) + 0.5f) / output_xres - 0.5f;
    float dxt_dx = 1.0f / output_xres;
    float yt     = (float(y) + 0.5f) / output_yres - 0.5f;
    float dyt_dy = 1.0f / output_yres;
    float theta  = atan2f(yt, xt);
    // See OSL's Dual2 for partial derivs of
    // atan2, hypot, and 1/x
    double denom     = 1.0 / (xt * xt + yt * yt);
    double dtheta_dx = yt * dxt_dx * denom;
    double dtheta_dy = -xt * dyt_dy * denom;
    s                = float(4.0 * theta / (2.0 * M_PI));
    dsdx             = float(4.0 * dtheta_dx / (2.0 * M_PI));
    dsdy             = float(4.0 * dtheta_dy / (2.0 * M_PI));
    double h         = hypot(xt, yt);
    double dh_dx     = xt * dxt_dx / h;
    double dh_dy     = yt * dyt_dy / h;
    h *= M_SQRT2;
    dh_dx *= M_SQRT2;
    dh_dy *= M_SQRT2;
    double hinv = 1.0 / h;
    t           = float(hinv);
    dtdx        = float(hinv * (-hinv * dh_dx));
    dtdy        = float(hinv * (-hinv * dh_dy));
}



// FIXME -- templatize map_tube. For now, just loop over scalar version.
static void
map_tube(const Tex::IntWide& x, const Tex::IntWide& y, Tex::FloatWide& s,
         Tex::FloatWide& t, Tex::FloatWide& dsdx, Tex::FloatWide& dtdx,
         Tex::FloatWide& dsdy, Tex::FloatWide& dtdy)
{
    for (int i = 0; i < Tex::BatchWidth; ++i)
        map_tube(x[i], y[i], s[i], t[i], dsdx[i], dtdx[i], dsdy[i], dtdy[i]);
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
map_filtertest(const int& x, const int& y, float& s, float& t, float& dsdx,
               float& dtdx, float& dsdy, float& dtdy)
{
    float minoraxis = 1.0f / 256;
    float majoraxis = minoraxis
                      * lerp(1.0f, 32.0f, (float)x / (output_xres - 1));
    float angle = (float)(2.0 * M_PI * (double)y / (output_yres - 1));
    float sinangle, cosangle;
    sincos(angle, &sinangle, &cosangle);
    s = 0.5f;
    t = 0.5f;

    dsdx = minoraxis * cosangle;
    dtdx = minoraxis * sinangle;
    dsdy = -majoraxis * sinangle;
    dtdy = majoraxis * cosangle;
}



// FIXME -- templatize map_filtertest. For now, just loop over scalar version.
static void
map_filtertest(const Tex::IntWide& x, const Tex::IntWide& y, Tex::FloatWide& s,
               Tex::FloatWide& t, Tex::FloatWide& dsdx, Tex::FloatWide& dtdx,
               Tex::FloatWide& dsdy, Tex::FloatWide& dtdy)
{
    for (int i = 0; i < Tex::BatchWidth; ++i)
        map_filtertest(x[i], y[i], s[i], t[i], dsdx[i], dtdx[i], dsdy[i],
                       dtdy[i]);
}



template<typename Float = float, typename Int = int>
void
map_default_3D(const Int& x, const Int& y, Imath::Vec3<Float>& P,
               Imath::Vec3<Float>& dPdx, Imath::Vec3<Float>& dPdy,
               Imath::Vec3<Float>& dPdz)
{
    P.x = (Float(x) + 0.5f) / output_xres * sscale;
    P.y = (Float(y) + 0.5f) / output_yres * tscale;
    P.z = 0.5f * sscale;
    P += texoffset;
    dPdx.x = 1.0f / output_xres * sscale;
    dPdx.y = 0;
    dPdx.z = 0;
    dPdy.x = 0;
    dPdy.y = 1.0f / output_yres * tscale;
    dPdy.z = 0;
    dPdz.setValue(0, 0, 0);
}



// Just map pixels to environment direction
void
map_env_latlong(const int& x, const int& y, Imath::Vec3<float>& R,
                Imath::Vec3<float>& dRdx, Imath::Vec3<float>& dRdy)
{
    float u         = (x + 0.5f) / output_xres;
    float v         = (y + 0.5f) / output_yres;
    float du_dx     = float(1.0f / output_xres);  // du_dy = 0
    float dv_dy     = float(1.0f / output_yres);  // dv_dx = 0
    float theta     = float(2.0 * M_PI) * u;
    float dtheta_dx = float(2.0 * M_PI) * du_dx;  // dtheta_dy = 0
    float phi       = float(M_PI) * v;
    float dphi_dy   = float(M_PI) * dv_dy;  // dphi_dx = 0

    R    = Imath::Vec3<float>(fast_sin(phi) * fast_sin(theta), fast_cos(phi),
                           -fast_sin(phi) * fast_cos(theta));
    dRdx = Imath::Vec3<float>(fast_sin(phi) * fast_cos(theta) * dtheta_dx,
                              float(0.0f),
                              fast_sin(phi) * fast_sin(theta) * dtheta_dx);
    dRdy = Imath::Vec3<float>(-fast_cos(phi) * fast_sin(theta) * dphi_dy,
                              -fast_sin(phi) * dphi_dy,
                              -fast_cos(phi) * fast_cos(theta) * dphi_dy);
}



// Turn v3f[lanes] into simd[3]
inline Imath::Vec3<Tex::FloatWide>
soa(const Imath::V3f v[])
{
    Imath::Vec3<Tex::FloatWide> result;
    for (int i = 0; i < Tex::BatchWidth; ++i) {
        result[0][i] = v[i].x;
        result[1][i] = v[i].y;
        result[2][i] = v[i].z;
    }
    return result;
}



// FIXME -- templatize map_env_latlong. For now, just loop over scalar version.
void
map_env_latlong(const Tex::IntWide& x, const Tex::IntWide& y,
                Imath::Vec3<Tex::FloatWide>& R,
                Imath::Vec3<Tex::FloatWide>& dRdx,
                Imath::Vec3<Tex::FloatWide>& dRdy)
{
    Imath::V3f r[Tex::BatchWidth], drdx[Tex::BatchWidth], drdy[Tex::BatchWidth];
    for (int i = 0; i < Tex::BatchWidth; ++i)
        map_env_latlong(x[i], y[i], r[i], drdx[i], drdy[i]);
    R    = soa(r);
    dRdx = soa(drdx);
    dRdy = soa(drdy);
}



void
plain_tex_region(ImageBuf& image, ustring filename, Mapping2D mapping,
                 ImageBuf* image_ds, ImageBuf* image_dt, ROI roi)
{
    TextureSystem::Perthread* perthread_info     = texsys->get_perthread_info();
    TextureSystem::TextureHandle* texture_handle = texsys->get_texture_handle(
        filename);
    int nchannels = nchannels_override ? nchannels_override : image.nchannels();

    TextureOpt opt;
    initialize_opt(opt);

    float* result    = OIIO_ALLOCA(float, std::max(3, nchannels));
    float* dresultds = test_derivs ? OIIO_ALLOCA(float, nchannels) : NULL;
    float* dresultdt = test_derivs ? OIIO_ALLOCA(float, nchannels) : NULL;
    for (ImageBuf::Iterator<float> p(image, roi); !p.done(); ++p) {
        float s, t, dsdx, dtdx, dsdy, dtdy;
        mapping(p.x(), p.y(), s, t, dsdx, dtdx, dsdy, dtdy);

        if (widthramp != 0.0f) {
            // If widthramp is set, we want to blend between the set width
            // and the ramp width from left to right.
            opt.swidth = OIIO::lerp(width, widthramp, s);
            opt.twidth = opt.swidth;
        }
        if (stochastic) {
            // Hash the pixel coords to get a pseudo-random variant
            if (use_bluenoise) {
                opt.rnd = bluenoiseimg.getchannel(p.x(), p.y(), 0, 0,
                                                  ImageBuf::WrapPeriodic);
            } else {
                constexpr float inv
                    = 1.0f / float(std::numeric_limits<uint32_t>::max());
                opt.rnd = bjhash::bjfinal(p.x(), p.y()) * inv;
            }
        }

        // Call the texture system to do the filtering.
        bool ok;
        if (use_handle)
            ok = texsys->texture(texture_handle, perthread_info, opt, s, t,
                                 dsdx, dtdx, dsdy, dtdy, nchannels, result,
                                 dresultds, dresultdt);
        else
            ok = texsys->texture(filename, opt, s, t, dsdx, dtdx, dsdy, dtdy,
                                 nchannels, result, dresultds, dresultdt);
        if (!ok) {
            std::string e = texsys->geterror();
            if (!e.empty())
                Strutil::print(std::cerr, "ERROR: {}\n", e);
        }

        // Save filtered pixels back to the image.
        for (int i = 0; i < nchannels; ++i)
            result[i] *= scalefactor;
        image.setpixel(p.x(), p.y(), make_span(result, nchannels));
        if (test_derivs) {
            image_ds->setpixel(p.x(), p.y(), make_span(dresultds, nchannels));
            image_dt->setpixel(p.x(), p.y(), make_span(dresultdt, nchannels));
        }
    }
}



void
test_plain_texture(Mapping2D mapping)
{
    Strutil::print("Testing 2d texture {}, output = {}\n", filenames[0],
                   output_filename);
    const int nchannels = 4;
    ImageSpec outspec(output_xres, output_yres, nchannels, TypeDesc::FLOAT);
    ImageBuf image(outspec);
    TypeDesc fmt(dataformatname);
    image.set_write_format(fmt);
    OIIO::ImageBufAlgo::zero(image);
    ImageBuf image_ds, image_dt;
    if (test_derivs) {
        image_ds.reset(outspec);
        image_ds.set_write_format(fmt);
        OIIO::ImageBufAlgo::zero(image_ds);
        image_dt.reset(outspec);
        OIIO::ImageBufAlgo::zero(image_dt);
        image_dt.set_write_format(fmt);
    }

    ustring filename = filenames[0];

    for (int iter = 0; iter < iters; ++iter) {
        if (iters > 1 && filenames.size() > 1) {
            // Use a different filename for each iteration
            int texid = iter % (int)filenames.size();
            filename  = (filenames[texid]);
            Strutil::print("iter {} file {}\n", iter, filename);
        }
        if (close_before_iter)
            texsys->close_all();

        ImageBufAlgo::parallel_image(
            get_roi(image.spec()), nthreads,
            std::bind(plain_tex_region, std::ref(image), filename, mapping,
                      test_derivs ? &image_ds : NULL,
                      test_derivs ? &image_dt : NULL, _1));
        if (resetstats) {
            Strutil::print("{}\n", texsys->getstats(2));
            texsys->reset_stats();
        }
    }

    if (!image.write(output_filename))
        Strutil::print(std::cerr, "Error writing {} : {}\n", output_filename,
                       image.geterror());
    if (test_derivs) {
        if (!image_ds.write(output_filename + "-ds.exr"))
            Strutil::print(std::cerr, "Error writing {} : {}\n",
                           (output_filename + "-ds.exr"), image_ds.geterror());
        if (!image_dt.write(output_filename + "-dt.exr"))
            Strutil::print(std::cerr, "Error writing {} : {}\n",
                           (output_filename + "-dt.exr"), image_dt.geterror());
    }
}



void
plain_tex_region_batch(ImageBuf& image, ustring filename, Mapping2DWide mapping,
                       ImageBuf* image_ds, ImageBuf* image_dt, const ROI roi)
{
    using namespace Tex;
    TextureSystem::Perthread* perthread_info     = texsys->get_perthread_info();
    TextureSystem::TextureHandle* texture_handle = texsys->get_texture_handle(
        filename);
    int nchannels_img = image.nchannels();
    int nchannels = nchannels_override ? nchannels_override : image.nchannels();
    OIIO_DASSERT(image.spec().format == TypeDesc::FLOAT);
    OIIO_DASSERT(image_ds == nullptr
                 || image_ds->spec().format == TypeDesc::FLOAT);
    OIIO_DASSERT(image_dt == nullptr
                 || image_dt->spec().format == TypeDesc::FLOAT);

    TextureOptBatch opt;
    initialize_opt(opt);

    int nc               = std::max(3, nchannels);
    FloatWide* result    = OIIO_ALLOCA(FloatWide, nc);
    FloatWide* dresultds = test_derivs ? OIIO_ALLOCA(FloatWide, nc) : nullptr;
    FloatWide* dresultdt = test_derivs ? OIIO_ALLOCA(FloatWide, nc) : nullptr;
    for (int y = roi.ybegin; y < roi.yend; ++y) {
        for (int x = roi.xbegin; x < roi.xend; x += BatchWidth) {
            FloatWide s, t, dsdx, dtdx, dsdy, dtdy;
            mapping(IntWide::Iota(x), y, s, t, dsdx, dtdx, dsdy, dtdy);

            if (widthramp != 0.0f) {
                // If widthramp is set, we want to blend between the set width
                // and the ramp width from left to right.
                for (int i = 0; i < BatchWidth; ++i) {
                    opt.swidth[i] = OIIO::lerp(width, widthramp, s[i]);
                    opt.twidth[i] = opt.swidth[i];
                }
            }
            if (stochastic) {
                // Hash the pixel coords to get a pseudo-random variant
                constexpr float inv
                    = 1.0f / float(std::numeric_limits<uint32_t>::max());
                for (int i = 0; i < BatchWidth; ++i) {
                    if (use_bluenoise)
                        opt.rnd[i]
                            = bluenoiseimg.getchannel(x + i, y, 0, 0,
                                                      ImageBuf::WrapPeriodic);
                    else
                        opt.rnd[i] = bjhash::bjfinal(x + i, y) * inv;
                }
            }

            int npoints  = std::min(BatchWidth, roi.xend - x);
            RunMask mask = RunMaskOn >> (BatchWidth - npoints);
            // Call the texture system to do the filtering.
            bool ok;
            if (use_handle)
                ok = texsys->texture(texture_handle, perthread_info, opt, mask,
                                     s.data(), t.data(), dsdx.data(),
                                     dtdx.data(), dsdy.data(), dtdy.data(),
                                     nchannels, (float*)result,
                                     (float*)dresultds, (float*)dresultdt);
            else
                ok = texsys->texture(filename, opt, mask, s.data(), t.data(),
                                     dsdx.data(), dtdx.data(), dsdy.data(),
                                     dtdy.data(), nchannels, (float*)result,
                                     (float*)dresultds, (float*)dresultdt);
            if (!ok) {
                std::string e = texsys->geterror();
                if (!e.empty())
                    Strutil::print(std::cerr, "ERROR: {}\n", e);
            }
            // Save filtered pixels back to the image.
            for (int c = 0; c < nchannels; ++c)
                result[c] *= scalefactor;
            float* resultptr = (float*)image.pixeladdr(x, y);
            // FIXME: simplify by using SIMD scatter
            for (int c = 0; c < nchannels; ++c)
                for (int i = 0; i < npoints; ++i)
                    resultptr[c + i * nchannels_img] = result[c][i];
            if (test_derivs) {
                float* resultdsptr = (float*)image_ds->pixeladdr(x, y);
                float* resultdtptr = (float*)image_dt->pixeladdr(x, y);
                for (int c = 0; c < nchannels; ++c) {
                    for (int i = 0; i < npoints; ++i) {
                        resultdsptr[c + i * nchannels_img] = dresultds[c][i];
                        resultdtptr[c + i * nchannels_img] = dresultdt[c][i];
                    }
                }
            }
        }
    }
}



void
test_plain_texture_batch(Mapping2DWide mapping)
{
    Strutil::print("Testing BATCHED 2d texture {}, output = {}\n", filenames[0],
                   output_filename);
    const int nchannels = 4;
    ImageSpec outspec(output_xres, output_yres, nchannels, TypeDesc::FLOAT);
    TypeDesc fmt(dataformatname);
    ImageBuf image(outspec);
    image.set_write_format(fmt);
    OIIO::ImageBufAlgo::zero(image);
    std::unique_ptr<ImageBuf> image_ds, image_dt;
    if (test_derivs) {
        image_ds.reset(new ImageBuf(outspec));
        image_ds->set_write_format(fmt);
        OIIO::ImageBufAlgo::zero(*image_ds);
        image_dt.reset(new ImageBuf(outspec));
        image_dt->set_write_format(fmt);
        OIIO::ImageBufAlgo::zero(*image_dt);
    }

    ustring filename = filenames[0];

    for (int iter = 0; iter < iters; ++iter) {
        if (iters > 1 && filenames.size() > 1) {
            // Use a different filename for each iteration
            int texid = iter % (int)filenames.size();
            filename  = (filenames[texid]);
            Strutil::print("iter {} file {}\n", iter, filename);
        }
        if (close_before_iter)
            texsys->close_all();

        ImageBufAlgo::parallel_image(
            get_roi(image.spec()), nthreads, [&](ROI roi) {
                plain_tex_region_batch(image, filename, mapping, image_ds.get(),
                                       image_dt.get(), roi);
            });
        if (resetstats) {
            Strutil::print("{}\n", texsys->getstats(2));
            texsys->reset_stats();
        }
    }

    if (!image.write(output_filename))
        Strutil::print(std::cerr, "Error writing {} : {}\n", output_filename,
                       image.geterror());
    if (test_derivs) {
        if (!image_ds->write(output_filename + "-ds.exr"))
            Strutil::print(std::cerr, "Error writing {} : {}\n",
                           (output_filename + "-ds.exr"), image_ds->geterror());
        if (!image_dt->write(output_filename + "-dt.exr"))
            Strutil::print(std::cerr, "Error writing {} : {}\n",
                           (output_filename + "-dt.exr"), image_dt->geterror());
    }
}



void
tex3d_region(ImageBuf& image, ustring filename, Mapping3D mapping, ROI roi)
{
    TextureSystem::Perthread* perthread_info     = texsys->get_perthread_info();
    TextureSystem::TextureHandle* texture_handle = texsys->get_texture_handle(
        filename);
    int nchannels = nchannels_override ? nchannels_override : image.nchannels();

    TextureOpt opt;
    initialize_opt(opt);
    opt.fill = (fill >= 0.0f) ? fill : 0.0f;
    //    opt.swrap = opt.twrap = opt.rwrap = TextureOpt::WrapPeriodic;

    float* result    = OIIO_ALLOCA(float, nchannels);
    float* dresultds = test_derivs ? OIIO_ALLOCA(float, nchannels) : NULL;
    float* dresultdt = test_derivs ? OIIO_ALLOCA(float, nchannels) : NULL;
    float* dresultdr = test_derivs ? OIIO_ALLOCA(float, nchannels) : NULL;
    for (ImageBuf::Iterator<float> p(image, roi); !p.done(); ++p) {
        Imath::V3f P, dPdx, dPdy, dPdz;
        mapping(p.x(), p.y(), P, dPdx, dPdy, dPdz);

        // Call the texture system to do the filtering.
        bool ok = use_handle
                      ? texsys->texture3d(texture_handle, perthread_info, opt,
                                          P, dPdx, dPdy, dPdz, nchannels,
                                          result, dresultds, dresultdt,
                                          dresultdr)
                      : texsys->texture3d(filename, opt, P, dPdx, dPdy, dPdz,
                                          nchannels, result, dresultds,
                                          dresultdt, dresultdr);
        if (!ok) {
            std::string e = texsys->geterror();
            if (!e.empty())
                Strutil::print(std::cerr, "ERROR: {}\n", e);
        }

        // Save filtered pixels back to the image.
        for (int i = 0; i < nchannels; ++i)
            result[i] *= scalefactor;
        image.setpixel(p.x(), p.y(), make_span(result, nchannels));
    }
}



void
tex3d_region_batch(ImageBuf& image, ustring filename, Mapping3DWide mapping,
                   ROI roi)
{
    using namespace Tex;
    TextureSystem::Perthread* perthread_info     = texsys->get_perthread_info();
    TextureSystem::TextureHandle* texture_handle = texsys->get_texture_handle(
        filename);
    int nchannels_img = image.nchannels();
    int nchannels = nchannels_override ? nchannels_override : image.nchannels();

    TextureOptBatch opt;
    initialize_opt(opt);
    opt.fill = (fill >= 0.0f) ? fill : 0.0f;
    //    opt.swrap = opt.twrap = opt.rwrap = TextureOpt::WrapPeriodic;

    FloatWide* result    = OIIO_ALLOCA(FloatWide, nchannels);
    FloatWide* dresultds = test_derivs ? OIIO_ALLOCA(FloatWide, nchannels)
                                       : NULL;
    FloatWide* dresultdt = test_derivs ? OIIO_ALLOCA(FloatWide, nchannels)
                                       : NULL;
    FloatWide* dresultdr = test_derivs ? OIIO_ALLOCA(FloatWide, nchannels)
                                       : NULL;
    for (int y = roi.ybegin; y < roi.yend; ++y) {
        for (int x = roi.xbegin; x < roi.xend; x += BatchWidth) {
            Imath::Vec3<FloatWide> P, dPdx, dPdy, dPdz;
            mapping(IntWide::Iota(x), y, P, dPdx, dPdy, dPdz);
            int npoints  = std::min(BatchWidth, roi.xend - x);
            RunMask mask = RunMaskOn >> (BatchWidth - npoints);

            // Call the texture system to do the filtering.
            bool ok
                = use_handle
                      ? texsys->texture3d(texture_handle, perthread_info, opt,
                                          mask, (float*)&P, (float*)&dPdx,
                                          (float*)&dPdy, (float*)&dPdz,
                                          nchannels, (float*)result,
                                          (float*)dresultds, (float*)dresultdt,
                                          (float*)dresultdr)
                      : texsys->texture3d(filename, opt, mask, (float*)&P,
                                          (float*)&dPdx, (float*)&dPdy,
                                          (float*)&dPdz, nchannels,
                                          (float*)result, (float*)dresultds,
                                          (float*)dresultdt, (float*)dresultdr);
            if (!ok) {
                std::string e = texsys->geterror();
                if (!e.empty())
                    Strutil::print(std::cerr, "ERROR: {}\n", e);
            }

            // Save filtered pixels back to the image.
            for (int c = 0; c < nchannels; ++c)
                result[c] *= scalefactor;
            float* resultptr = (float*)image.pixeladdr(x, y);
            // FIXME: simplify by using SIMD scatter
            for (int c = 0; c < nchannels; ++c)
                for (int i = 0; i < npoints; ++i)
                    resultptr[c + i * nchannels_img] = result[c][i];
        }
    }
}



void
test_texture3d(ustring filename, Mapping3D mapping)
{
    Strutil::print("Testing texture3d {}, output = {}\n", filename,
                   output_filename);
    int nchannels = nchannels_override ? nchannels_override : 4;
    ImageSpec outspec(output_xres, output_yres, nchannels, TypeDesc::FLOAT);
    ImageBuf image(outspec);
    TypeDesc fmt(dataformatname);
    image.set_write_format(fmt);
    OIIO::ImageBufAlgo::zero(image);

    for (int iter = 0; iter < iters; ++iter) {
        // Trick: switch to second texture, if given, for second iteration
        if (iter && filenames.size() > 1)
            filename = filenames[1];
        if (close_before_iter)
            texsys->close_all();
        ImageBufAlgo::parallel_image(get_roi(image.spec()), nthreads,
                                     std::bind(tex3d_region, std::ref(image),
                                               filename, mapping, _1));
    }

    if (!image.write(output_filename))
        Strutil::print(std::cerr, "Error writing {} : {}\n", output_filename,
                       image.geterror());
}



void
test_texture3d_batch(ustring filename, Mapping3DWide mapping)
{
    Strutil::print("Testing BATCHED texture3d {}, output = {}\n", filename,
                   output_filename);
    int nchannels = nchannels_override ? nchannels_override : 4;
    ImageSpec outspec(output_xres, output_yres, nchannels, TypeDesc::FLOAT);
    ImageBuf image(outspec);
    TypeDesc fmt(dataformatname);
    image.set_write_format(fmt);
    OIIO::ImageBufAlgo::zero(image);

    for (int iter = 0; iter < iters; ++iter) {
        // Trick: switch to second texture, if given, for second iteration
        if (iter && filenames.size() > 1)
            filename = filenames[1];
        if (close_before_iter)
            texsys->close_all();
        ImageBufAlgo::parallel_image(get_roi(image.spec()), nthreads,
                                     [&](ROI roi) {
                                         tex3d_region_batch(image, filename,
                                                            mapping, roi);
                                     });
    }

    if (!image.write(output_filename))
        Strutil::print(std::cerr, "Error writing {} : {}\n", output_filename,
                       image.geterror());
}



static void
test_shadow(ustring /*filename*/)
{
}



void
env_region(ImageBuf& image, ustring filename, MappingEnv mapping,
           ImageBuf* image_ds, ImageBuf* image_dt, ROI roi)
{
    TextureSystem::Perthread* perthread_info     = texsys->get_perthread_info();
    TextureSystem::TextureHandle* texture_handle = texsys->get_texture_handle(
        filename);
    int nchannels = nchannels_override ? nchannels_override : image.nchannels();

    TextureOpt opt;
    initialize_opt(opt);

    float* result    = OIIO_ALLOCA(float, nchannels);
    float* dresultds = test_derivs ? OIIO_ALLOCA(float, nchannels) : NULL;
    float* dresultdt = test_derivs ? OIIO_ALLOCA(float, nchannels) : NULL;
    for (ImageBuf::Iterator<float> p(image, roi); !p.done(); ++p) {
        Imath::V3f R, dRdx, dRdy;
        mapping(p.x(), p.y(), R, dRdx, dRdy);
        // if (p.x() == 0 && p.y() == 0)
        //     Strutil::print("R = {}\n", R);
        // Call the texture system to do the filtering.
        bool ok = use_handle
                      ? texsys->environment(texture_handle, perthread_info, opt,
                                            R, dRdx, dRdy, nchannels, result,
                                            dresultds, dresultdt)
                      : texsys->environment(filename, opt, R, dRdx, dRdy,
                                            nchannels, result, dresultds,
                                            dresultdt);
        if (!ok) {
            std::string e = texsys->geterror();
            if (!e.empty())
                Strutil::print(std::cerr, "ERROR: {}\n", e);
        }

        // Save filtered pixels back to the image.
        for (int i = 0; i < nchannels; ++i)
            result[i] *= scalefactor;
        image.setpixel(p.x(), p.y(), make_span(result, nchannels));
        if (image_ds)
            image_ds->setpixel(p.x(), p.y(), make_span(dresultds, nchannels));
        if (image_dt)
            image_dt->setpixel(p.x(), p.y(), make_span(dresultdt, nchannels));
    }
}



void
env_region_batch(ImageBuf& image, ustring filename, MappingEnvWide mapping,
                 ImageBuf* image_ds, ImageBuf* image_dt, ROI roi)
{
    using namespace Tex;
    TextureSystem::Perthread* perthread_info     = texsys->get_perthread_info();
    TextureSystem::TextureHandle* texture_handle = texsys->get_texture_handle(
        filename);
    int nchannels_img = image.nchannels();
    int nchannels = nchannels_override ? nchannels_override : image.nchannels();

    TextureOptBatch opt;
    initialize_opt(opt);

    FloatWide* result    = OIIO_ALLOCA(FloatWide, nchannels);
    FloatWide* dresultds = test_derivs ? OIIO_ALLOCA(FloatWide, nchannels)
                                       : nullptr;
    FloatWide* dresultdt = test_derivs ? OIIO_ALLOCA(FloatWide, nchannels)
                                       : nullptr;
    for (int y = roi.ybegin; y < roi.yend; ++y) {
        for (int x = roi.xbegin; x < roi.xend; x += BatchWidth) {
            Imath::Vec3<FloatWide> R, dRdx, dRdy;
            mapping(IntWide::Iota(x), y, R, dRdx, dRdy);
            int npoints  = std::min(BatchWidth, roi.xend - x);
            RunMask mask = RunMaskOn >> (BatchWidth - npoints);

            // Call the texture system to do the filtering.
            // if (y == 0 && x == 0)
            //     Strutil::print("R = {}\n", R);
            bool ok
                = use_handle
                      ? texsys->environment(texture_handle, perthread_info, opt,
                                            mask, (const float*)&R,
                                            (const float*)&dRdx,
                                            (const float*)&dRdy, nchannels,
                                            (float*)result, (float*)dresultds,
                                            (float*)dresultdt)
                      : texsys->environment(filename, opt, mask,
                                            (const float*)&R,
                                            (const float*)&dRdx,
                                            (const float*)&dRdy, nchannels,
                                            (float*)result, (float*)dresultds,
                                            (float*)dresultdt);

            if (!ok) {
                std::string e = texsys->geterror();
                if (!e.empty())
                    Strutil::print(std::cerr, "ERROR: {}\n", e);
            }

            // Save filtered pixels back to the image.
            for (int i = 0; i < nchannels; ++i)
                result[i] *= scalefactor;
            float* resultptr = (float*)image.pixeladdr(x, y);
            // FIXME: simplify by using SIMD scatter
            for (int i = 0; i < npoints; ++i)
                for (int c = 0; c < nchannels; ++c)
                    resultptr[c + i * nchannels_img] = result[c][i];
            if (test_derivs) {
                float* resultdsptr = (float*)image_ds->pixeladdr(x, y);
                float* resultdtptr = (float*)image_dt->pixeladdr(x, y);
                for (int c = 0; c < nchannels; ++c) {
                    for (int i = 0; i < npoints; ++i) {
                        resultdsptr[c + i * nchannels_img] = dresultds[c][i];
                        resultdtptr[c + i * nchannels_img] = dresultdt[c][i];
                    }
                }
            }
        }
    }
}



static void
test_environment(ustring filename, MappingEnv mapping)
{
    Strutil::sync::print("Testing environment {}, output = {}\n", filename,
                         output_filename);
    int nchannels = nchannels_override ? nchannels_override : 4;
    ImageSpec outspec(output_xres, output_yres, nchannels, TypeDesc::FLOAT);
    ImageBuf image(outspec);
    TypeDesc fmt(dataformatname);
    image.set_write_format(fmt);
    OIIO::ImageBufAlgo::zero(image);
    ImageBuf image_ds, image_dt;
    if (test_derivs) {
        image_ds.reset(outspec);
        image_ds.set_write_format(fmt);
        OIIO::ImageBufAlgo::zero(image_ds);
        image_dt.reset(outspec);
        image_dt.set_write_format(fmt);
        OIIO::ImageBufAlgo::zero(image_dt);
    }

    for (int iter = 0; iter < iters; ++iter) {
        // Trick: switch to second texture, if given, for second iteration
        if (iter && filenames.size() > 1)
            filename = filenames[1];
        if (close_before_iter)
            texsys->close_all();
        ImageBufAlgo::parallel_image(
            get_roi(image.spec()), nthreads, [&](ROI roi) {
                env_region(image, filename, mapping,
                           test_derivs ? &image_ds : nullptr,
                           test_derivs ? &image_dt : nullptr, roi);
            });
    }

    if (!image.write(output_filename))
        Strutil::print(std::cerr, "Error writing {} : {}\n", output_filename,
                       image.geterror());
    if (test_derivs) {
        if (!image_ds.write(output_filename + "-ds.exr"))
            Strutil::print(std::cerr, "Error writing {} : {}\n",
                           (output_filename + "-ds.exr"), image_ds.geterror());
        if (!image_dt.write(output_filename + "-dt.exr"))
            Strutil::print(std::cerr, "Error writing {} : {}\n",
                           (output_filename + "-dt.exr"), image_dt.geterror());
    }
}



static void
test_environment_batch(ustring filename, MappingEnvWide mapping)
{
    Strutil::sync::print("Testing BATCHED environment {}, output = {}\n",
                         filename, output_filename);
    int nchannels = nchannels_override ? nchannels_override : 4;
    ImageSpec outspec(output_xres, output_yres, nchannels, TypeDesc::FLOAT);
    ImageBuf image(outspec);
    TypeDesc fmt(dataformatname);
    image.set_write_format(fmt);
    OIIO::ImageBufAlgo::zero(image);
    ImageBuf image_ds, image_dt;
    if (test_derivs) {
        image_ds.reset(outspec);
        image_ds.set_write_format(fmt);
        OIIO::ImageBufAlgo::zero(image_ds);
        image_dt.reset(outspec);
        image_dt.set_write_format(fmt);
        OIIO::ImageBufAlgo::zero(image_dt);
    }

    for (int iter = 0; iter < iters; ++iter) {
        // Trick: switch to second texture, if given, for second iteration
        if (iter && filenames.size() > 1)
            filename = filenames[1];
        if (close_before_iter)
            texsys->close_all();
        ImageBufAlgo::parallel_image(
            get_roi(image.spec()), nthreads, [&](ROI roi) {
                env_region_batch(image, filename, mapping,
                                 test_derivs ? &image_ds : nullptr,
                                 test_derivs ? &image_dt : nullptr, roi);
            });
    }

    if (!image.write(output_filename))
        Strutil::print(std::cerr, "Error writing {} : {}\n", output_filename,
                       image.geterror());
    if (test_derivs) {
        if (!image_ds.write(output_filename + "-ds.exr"))
            Strutil::print(std::cerr, "Error writing {} : {}\n",
                           (output_filename + "-ds.exr"), image_ds.geterror());
        if (!image_dt.write(output_filename + "-dt.exr"))
            Strutil::print(std::cerr, "Error writing {} : {}\n",
                           (output_filename + "-dt.exr"), image_dt.geterror());
    }
}



static void
test_getimagespec_gettexels(ustring filename)
{
    ImageSpec spec;
    int miplevel = 0;
    if (!texsys->get_imagespec(filename, spec, 0)) {
        Strutil::print(std::cerr, "Could not get spec for {}\n", filename);
        std::string e = texsys->geterror();
        if (!e.empty())
            Strutil::print(std::cerr, "ERROR: {}\n", e);
        return;
    }

    if (!test_gettexels)
        return;

    int w         = std::min(spec.width, output_xres);
    int h         = std::min(spec.height, output_yres);
    int nchannels = nchannels_override ? nchannels_override : spec.nchannels;
    ImageSpec postagespec(w, h, nchannels, TypeDesc::FLOAT);
    ImageBuf buf(postagespec);
    TextureOpt opt;
    initialize_opt(opt);
    std::vector<float> tmp(w * h * nchannels);
    int x = spec.x + spec.width / 2 - w / 2;
    int y = spec.y + spec.height / 2 - h / 2;
    for (int i = 0; i < iters; ++i) {
        bool ok = texsys->get_texels(filename, opt, miplevel, x, x + w, y,
                                     y + h, 0, 1, 0, nchannels,
                                     postagespec.format, tmp.data());
        if (!ok)
            Strutil::print(std::cerr, "ERROR: {}\n", texsys->geterror());
    }
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            imagesize_t texoffset = (y * w + x) * spec.nchannels;
            buf.setpixel(x, y,
                         make_span(tmp.data() + texoffset, spec.nchannels));
        }
    TypeDesc fmt(dataformatname);
    if (fmt != TypeDesc::UNKNOWN)
        buf.set_write_format(fmt);
    buf.write(output_filename);
}



static const char* workload_names[] = {
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
do_tex_thread_workout(int iterations, int mythread)
{
    int nfiles = (int)filenames.size();
    float s = 0.1f, t = 0.1f;
    int nchannels = nchannels_override ? nchannels_override : 3;
    float* result = OIIO_ALLOCA(float, nchannels);
    memset(result, 0, sizeof(float) * nchannels);
    TextureOpt opt;
    initialize_opt(opt);
    float* dresultds = test_derivs ? OIIO_ALLOCA(float, nchannels) : NULL;
    float* dresultdt = test_derivs ? OIIO_ALLOCA(float, nchannels) : NULL;
    TextureSystem::Perthread* perthread_info = texsys->get_perthread_info();
    int pixel, whichfile = 0;

    ImageSpec spec0;
    if (texsys->is_udim(filenames[0])) {
        auto th = texsys->resolve_udim(filenames[0], 0.5f, 0.5f);
        if (!th || !texsys->get_imagespec(th, nullptr, spec0, 0)) {
            Strutil::print(std::cerr, "Unexpected error with {}: {}\n",
                           filenames[0], texsys->geterror());
        }
    } else {
        bool ok = texsys->get_imagespec(filenames[0], spec0, 0);
        if (!ok) {
            Strutil::print(std::cerr, "Unexpected error: {}\n",
                           texsys->geterror());
            return;
        }
    }
    // Compute a filter size that's between the second and third MIP levels.
    float fw   = (1.0f / spec0.width) * 1.5f * 2.0;
    float fh   = (1.0f / spec0.height) * 1.5f * 2.0;
    float dsdx = fw, dtdx = 0.0f, dsdy = 0.0f, dtdy = fh;
    if (anisoaspect > 1.001f) {
        // Make an ellipse 30 degrees off horizontal, with given aspect
        float xs = sqrtf(3.0) / 2.0, ys = 0.5f;
        dsdx = fw * xs * anisoaspect;
        dtdx = fh * ys * anisoaspect;
        dsdy = fw * ys;
        dtdy = -fh * xs;
    }

    for (int i = 0; i < iterations; ++i) {
        pixel   = i;
        bool ok = false;
        // Several different texture access patterns
        switch (threadtimes) {
        case 1:
            // Workload 1: Speed of light: Static texture access (same
            // texture coordinates all the time, one file), with handles
            // and per-thread data already queried only once rather than
            // per-call.
            ok = texsys->texture(texture_handles[0], perthread_info, opt, s, t,
                                 dsdx, dtdx, dsdy, dtdy, nchannels, result,
                                 dresultds, dresultdt);
            break;
        case 2:
            // Workload 2: Static texture access, with filenames.
            ok = texsys->texture(filenames[0], opt, s, t, dsdx, dtdx, dsdy,
                                 dtdy, nchannels, result, dresultds, dresultdt);
            break;
        case 3:
        case 4:
            // Workload 3: One file, coherent texture coordinates.
            //
            // Workload 4: Each thread starts with a different texture
            // coordinate offset, so likely are not simultaneously
            // accessing the very same tile as the other threads.
            if (threadtimes == 4)
                pixel += 57557 * mythread;
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
            pixel     = i / nfiles;
            if (threadtimes == 6)
                pixel += 57557 * mythread;
            break;
        case 7:
        case 8:
            // Workload 7: Coherent texture coordinates, but access
            // a series of textures at each coordinate, which partially
            // overlap with other threads.
            {
                int file = i % 8;
                if (file < 2)  // everybody accesses the first 2 files
                    whichfile = std::min(file, nfiles - 1);
                else  // and a slowly changing set of 6 others
                    whichfile = (file + 11 * mythread + i / 1000) % nfiles;
                pixel = i / nfiles;
                pixel += 57557 * mythread;
            }
            break;
        default:
            OIIO_ASSERT_MSG(0, "Unknown thread work pattern %d", threadtimes);
        }
        if (!ok && spec0.width && spec0.height) {
            s = (((2 * pixel) % spec0.width) + 0.5f) / spec0.width;
            t = (((2 * ((2 * pixel) / spec0.width)) % spec0.height) + 0.5f)
                / spec0.height;
            if (udim_tests) {
                s *= 10.0f;
                t *= float((pieces_per_udim + 9) / 10);
            }
            if (use_handle)
                ok = texsys->texture(texture_handles[whichfile], perthread_info,
                                     opt, s, t, dsdx, dtdx, dsdy, dtdy,
                                     nchannels, result, dresultds, dresultdt);
            else
                ok = texsys->texture(filenames[whichfile], opt, s, t, dsdx,
                                     dtdx, dsdy, dtdy, nchannels, result,
                                     dresultds, dresultdt);
        }
        if (!ok) {
            Strutil::print(std::cerr, "Unexpected error: {}\n",
                           texsys->geterror());
            return;
        }
        // Do some pointless work, to simulate that in a real app, there
        // would be operations interspersed with texture accesses.
        if (threadtimes != 8 /* skip on this test */) {
            for (int j = 0; j < 30; ++j)
                for (int c = 0; c < nchannels; ++c)
                    result[c] = cosf(result[c]);
        }
    }
    // Force the compiler to not optimize away the "other work"
    for (int c = 0; c < nchannels; ++c)
        OIIO_ASSERT(!isnan(result[c]));
}



// Launch numthreads threads each of which performs a workout of texture
// accesses.
void
launch_tex_threads(int numthreads, int iterations)
{
    if (invalidate_before_iter)
        texsys->invalidate_all(true);
    OIIO::thread_group threads;
    for (int i = 0; i < numthreads; ++i) {
        threads.create_thread(std::bind(do_tex_thread_workout, iterations, i));
    }
    OIIO_ASSERT((int)threads.size() == numthreads);
    threads.join_all();
}



class GridImageInput final : public ImageInput {
public:
    GridImageInput()
        : m_miplevel(-1)
    {
    }
    ~GridImageInput() override { close(); }
    const char* format_name(void) const final { return "grid"; }
    bool valid_file(const std::string& /*filename*/) const final
    {
        return true;
    }
    bool open(const std::string& /*name*/, ImageSpec& newspec) final
    {
        bool ok = seek_subimage(0, 0);
        newspec = spec();
        return ok;
    }
    bool close() override { return true; }
    int current_miplevel(void) const final { return m_miplevel; }
    bool seek_subimage(int subimage, int miplevel) final
    {
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
        m_spec             = ImageSpec(res, res, 3, TypeDesc::FLOAT);
        m_spec.tile_width  = std::min(64, res);
        m_spec.tile_height = std::min(64, res);
        m_spec.tile_depth  = 1;
        m_miplevel         = miplevel;
        return true;
    }
    bool read_native_scanline(int /*subimage*/, int /*miplevel*/, int /*y*/,
                              int /*z*/, void* /*data*/) final
    {
        return false;
    }
    bool read_native_tile(int subimage, int miplevel, int xbegin, int ybegin,
                          int zbegin, void* data) final
    {
        lock_guard lock(*this);
        if (!seek_subimage(subimage, miplevel))
            return false;
        float* tile = (float*)data;
        for (int z = zbegin, zend = z + m_spec.tile_depth; z < zend; ++z)
            for (int y = ybegin, yend = y + m_spec.tile_height; y < yend; ++y)
                for (int x = xbegin, xend = x + m_spec.tile_width; x < xend;
                     ++x) {
                    tile[0] = float(x) / m_spec.width;
                    tile[2] = float(y) / m_spec.height;
                    tile[1] = (((x / 16) & 1) == ((y / 16) & 1))
                                  ? 1.0f / (m_miplevel + 1)
                                  : 0.05f;
                    tile += m_spec.nchannels;
                }
        return true;
    }

private:
    int m_miplevel;
};



ImageInput*
make_grid_input()
{
    return new GridImageInput;
}



void
test_icwrite(int testicwrite)
{
    Strutil::print("Testing IC write, mode {}\n", testicwrite);

    // The global "shared" ImageCache will be the same one the
    // TextureSystem uses.
    std::shared_ptr<ImageCache> ic = ImageCache::create();

    // Set up the fake file and add it
    int tw = 64, th = 64;  // tile width and height
    int nc = nchannels_override ? nchannels_override : 3;  // channels
    ImageSpec spec(512, 512, nc, TypeDesc::FLOAT);
    spec.depth       = 1;
    spec.tile_width  = tw;
    spec.tile_height = th;
    spec.tile_depth  = 1;
    ustring filename(filenames[0]);
    bool ok = ic->add_file(filename, make_grid_input);
    if (!ok) {
        Strutil::print("ic->add_file error: {}\n", ic->geterror());
        OIIO_ASSERT(ok);
    }

    // Now add all the tiles if it's a seeded map
    // testicwrite == 1 means to seed the first MIP level using add_tile.
    // testicwrite == 2 does not use add_tile, but instead will rely on
    // the make_grid_input custom ImageInput that constructs a pattern
    // procedurally.
    if (testicwrite == 1) {
        std::vector<float> tile(spec.tile_pixels() * spec.nchannels);
        for (int ty = 0; ty < spec.height; ty += th) {
            for (int tx = 0; tx < spec.width; tx += tw) {
                // Construct a tile
                for (int y = 0; y < th; ++y)
                    for (int x = 0; x < tw; ++x) {
                        int index = (y * tw + x) * nc;
                        int xx = x + tx, yy = y + ty;
                        tile[index + 0] = float(xx) / spec.width;
                        tile[index + 1] = float(yy) / spec.height;
                        tile[index + 2] = (!(xx % 10) || !(yy % 10)) ? 1.0f
                                                                     : 0.0f;
                    }
                bool ok = ic->add_tile(filename, 0, 0, tx, ty, 0, 0, -1,
                                       TypeDesc::FLOAT, &tile[0]);
                if (!ok) {
                    Strutil::print(std::cerr, "ic->add_tile error: {}\n",
                                   ic->geterror());
                    return;
                }
            }
        }
    }
}



// Return a repeatable hash-based pseudo-random value uniform on [0,1).
// It's a hash, so it's completely deterministic, based on x,y,z,c,seed.
// But it can be used in similar ways to a PRNG.
OIIO_FORCEINLINE float
hashrand(int x, int y, int z, int c, int seed)
{
    const uint32_t magic = 0xfffff;
    uint32_t xu(x), yu(y), zu(z), cu(c), seedu(seed);
    using bjhash::bjfinal;
    uint32_t h = bjfinal(bjfinal(xu, yu, zu), cu, seedu) & magic;
    return h * (1.0f / (magic + 1));
}



static void
make_temp_noise_file(string_view filename, int seed)
{
    ImageSpec spec(maketest_res, maketest_res, maketest_chans,
                   Filesystem::extension(filename) == ".exr" ? TypeHalf
                                                             : TypeUInt16);
    ImageBuf buf(spec);
    float c0[4] = { hashrand(1, 0, 0, 0, seed + 23 * 0),
                    hashrand(0, 1, 0, 0, seed + 23 * 0),
                    hashrand(0, 0, 1, 0, seed + 23 * 0), 1.0f };
    float c1[4] = { hashrand(1, 0, 0, 0, seed + 23 * 1),
                    hashrand(0, 1, 0, 0, seed + 23 * 1),
                    hashrand(0, 0, 1, 0, seed + 23 * 1), 1.0f };
    float c2[4] = { hashrand(1, 0, 0, 0, seed + 23 * 2),
                    hashrand(0, 1, 0, 0, seed + 23 * 2),
                    hashrand(0, 0, 1, 0, seed + 23 * 2), 1.0f };
    float c3[4] = { hashrand(1, 0, 0, 0, seed + 23 * 3),
                    hashrand(0, 1, 0, 0, seed + 23 * 3),
                    hashrand(0, 0, 1, 0, seed + 23 * 3), 1.0f };
    ImageBufAlgo::fill(buf, cspan<float>(c0), cspan<float>(c1),
                       cspan<float>(c2), cspan<float>(c3));
    ImageSpec config;
    ImageBufAlgo::make_texture(ImageBufAlgo::MakeTxTexture, buf, filename,
                               config);
    filenames_to_delete.push_back(filename);
}



// If asked to make our own test files, do it now
static void
make_test_files()
{
    Timer timer;
    int n      = 0;
    int pieces = udim_tests ? pieces_per_udim : 1;
    for (int i = 0; n < num_test_files; ++i) {
        std::string filename = Strutil::fmt::format(maketest_template, i);
        bool do_print
            = (num_test_files <= 10 || i <= 4 || i >= num_test_files - 5
               || (num_test_files < 200 && (i % 10) == 0) || (i % 100) == 0);
        if (do_print) {
            Strutil::print("Temp file {}: {}\n", i, filename);
            fflush(stdout);
        }
        for (int u = 0; u < pieces && n < num_test_files; ++u) {
            std::string fn
                = Strutil::replace(filename, "<UDIM>",
                                   Strutil::fmt::format("{:04d}", u + 1001));
            if (udim_tests && do_print)
                Strutil::print("    {}\n", fn);
            if (!Filesystem::exists(fn))
                make_temp_noise_file(fn, i + u * 19);
            ++n;
        }
        filenames.emplace_back(filename);
    }
    if (runstats)
        Strutil::print("Created {} test files in {}\n\n",
                       filenames_to_delete.size(),
                       Strutil::timeintervalformat(timer()));
    fflush(stdout);
}



int
main(int argc, const char* argv[])
{
    Filesystem::convert_native_arguments(argc, argv);
    getargs(argc, argv);

    // environment variable TESTTEX_BATCH can force batch mode
    string_view testtex_batch = Sysutil::getenv("TESTTEX_BATCH");
    if (testtex_batch.size())
        batch = Strutil::from_string<int>(testtex_batch);

    OIIO::attribute("threads", nthreads);

    texsys = TextureSystem::create();
    Strutil::sync::print("Created texture system\n");
    if (texoptions.size())
        texsys->attribute("options", texoptions);
    texsys->attribute("autotile", autotile);
    texsys->attribute("automip", (int)automip);
    texsys->attribute("deduplicate", (int)dedup);
    if (cachesize >= 0)
        texsys->attribute("max_memory_MB", cachesize);
    else
        texsys->getattribute("max_memory_MB", TypeFloat, &cachesize);
    if (maxfiles >= 0)
        texsys->attribute("max_open_files", maxfiles);
    if (searchpath.length())
        texsys->attribute("searchpath", searchpath);
    if (nountiled)
        texsys->attribute("accept_untiled", 0);
    if (nounmipped)
        texsys->attribute("accept_unmipped", 0);
    texsys->attribute("gray_to_rgb", gray_to_rgb);
    texsys->attribute("flip_t", flip_t);
    texsys->attribute("stochastic", stochastic);
    texcolortransform_id
        = std::max(0, texsys->get_colortransform_id(ustring(texcolorspace),
                                                    ustring("scene_linear")));
    if (texcolortransform_id > 0)
        print("Treating texture as if it is in colorspace {}\n", texcolorspace);

    if (test_construction) {
        Timer t;
        for (int i = 0; i < 1000000000; ++i) {
            TextureOpt opt;
            dummyptr = &opt;  // This forces the optimizer to keep the loop
        }
        Strutil::print("TextureOpt construction: {} ns\n", t());
        TextureOpt canonical, copy;
        t.reset();
        t.start();
        for (int i = 0; i < 1000000000; ++i) {
            copy     = canonical;
            dummyptr = &copy;  // This forces the optimizer to keep the loop
        }
        Strutil::print("TextureOpt copy: {} ns\n", t());
    }

    if (maketest_template.size()
        && Strutil::contains(maketest_template, "<UDIM>"))
        udim_tests = true;

    if (num_test_files > 0) {
        make_test_files();
    }

    if (testicwrite && filenames.size()) {
        test_icwrite(testicwrite);
    }

    if (test_getimagespec) {
        ImageSpec spec;
        for (int i = 0; i < iters; ++i) {
            texsys->get_imagespec(filenames[0], spec, 0);
        }
        iters = 0;
    }

    if (gtiname.size()) {
        const char* attrib = nullptr;
        bool result        = texsys->get_texture_info(filenames[0], 0,
                                                      ustring(gtiname), TypeString,
                                                      &attrib);
        if (result)
            Strutil::print("Image \"{}\" attrib \"{}\" = \"{}\"\n",
                           filenames[0], gtiname, attrib);
        else
            Strutil::print("Image \"{}\" attrib \"{}\" -> not found\n",
                           filenames[0], gtiname, attrib);
    }

    if (test_gettexels) {
        test_getimagespec_gettexels(filenames[0]);
        iters = 0;
    }

    if (testhash) {
        TextureSystem::unit_test_hash();
    }

    Imath::M33f scale;
    scale.scale(Imath::V2f(0.3, 0.3));
    Imath::M33f rot;
    rot.rotate(radians(25.0f));
    Imath::M33f trans;
    trans.translate(Imath::V2f(0.75f, 0.25f));
    Imath::M33f persp(2, 0, 0, 0, 0.8, -0.55, 0, 0, 1);
    xform = persp * rot * trans * scale;
    xform.invert();

    for (auto f : filenames) {
        texture_handles.emplace_back(texsys->get_texture_handle(f));
        // Strutil::print("tex {} -> {:p}\n", f, (void*)texture_handles.back());
    }

    if (threadtimes) {
        // If the --iters flag was used, do that number of iterations total
        // (divided among the threads). If not supplied (iters will be 1),
        // then use a large constant *per thread*.
        const int iterations = iters > 1 ? iters : 2000000;
        Strutil::print("Workload: {}\n", workload_names[threadtimes]);
        Strutil::print("texture cache size = {} MB\n", cachesize);
        Strutil::print("hw threads = {}\n", Sysutil::hardware_concurrency());
        Strutil::print("times are best of {} trials\n\n", ntrials);
        Strutil::print("threads  time (s)   speedup efficiency\n");
        Strutil::print("-------- -------- --------- ----------\n");

        if (nthreads == 0)
            nthreads = Sysutil::hardware_concurrency();
        static int threadcounts[] = { 1,  2,  4,  8,   12,   16,
                                      24, 32, 64, 128, 1024, 1 << 30 };
        float single_thread_time  = 0.0f;
        for (int i = 0; threadcounts[i] <= nthreads; ++i) {
            if (threadcounts[i] < minthreads)
                continue;
            int nt    = wedge ? threadcounts[i] : nthreads;
            int its   = iters > 1 ? (std::max(1, iters / nt)) : iterations;
            int tries = nt <= 2 ? std::min(lowtrials, ntrials) : ntrials;
            double range;
            float t = (float)time_trial(std::bind(launch_tex_threads, nt, its),
                                        tries, 1, &range);
            if (single_thread_time == 0.0f)
                single_thread_time = t * nt;
            float speedup    = single_thread_time / t;
            float efficiency = speedup / nt;
            Strutil::print(
                "{:3}     {:8.2f}   {:6.1f}x  {:6.1f}%    range {:.2f}\t({} iters/thread)\n",
                nt, t, speedup, efficiency * 100.0f, range, its);
            fflush(stdout);
            if (!wedge)
                break;  // don't loop if we're not wedging
        }
        Strutil::print("\n");
    } else if (iters > 0 && filenames.size()) {
        ustring filename(filenames[0]);
        if (do_gettextureinfo)
            test_gettextureinfo(filenames[0]);
        const char* texturetype = "Plain Texture";
        texsys->get_texture_info(filename, 0, ustring("texturetype"),
                                 TypeDesc::STRING, &texturetype);
        Timer timer;
        if (!strcmp(texturetype, "Plain Texture")) {
            if (batch) {
                if (nowarp)
                    test_plain_texture_batch(map_default);
                else if (tube)
                    test_plain_texture_batch(map_tube);
                else if (filtertest)
                    test_plain_texture_batch(map_filtertest);
                else
                    test_plain_texture_batch(map_warp);

            } else {
                if (nowarp)
                    test_plain_texture(map_default);
                else if (tube)
                    test_plain_texture(map_tube);
                else if (filtertest)
                    test_plain_texture(map_filtertest);
                else
                    test_plain_texture(map_warp);
            }
        }
        if (!strcmp(texturetype, "Volume Texture")) {
            if (batch) {
                test_texture3d_batch(filename, map_default_3D);
            } else {
                test_texture3d(filename, map_default_3D);
            }
        }
        if (!strcmp(texturetype, "Shadow")) {
            test_shadow(filename);
        }
        if (!strcmp(texturetype, "Environment")) {
            if (batch) {
                test_environment_batch(filename, map_env_latlong);
            } else {
                test_environment(filename, map_env_latlong);
            }
        }
        test_getimagespec_gettexels(filename);
        if (runstats || verbose)
            Strutil::print("Time: {}\n", Strutil::timeintervalformat(timer()));
    }

    if (test_statquery) {
        Strutil::print("Testing statistics queries:\n");
        int total_files = 0;
        texsys->getattribute("total_files", total_files);
        Strutil::print("  Total files: {}\n", total_files);
        std::vector<ustring> all_filenames(total_files);
        Strutil::print("{}\n", TypeDesc(TypeDesc::STRING, total_files));
        texsys->getattribute("all_filenames",
                             TypeDesc(TypeDesc::STRING, total_files),
                             &all_filenames[0]);
        for (int i = 0; i < total_files; ++i) {
            int timesopened   = 0;
            int64_t bytesread = 0;
            float iotime      = 0.0f;
            int64_t data_size = 0, file_size = 0;
            texsys->get_texture_info(all_filenames[i], 0,
                                     ustring("stat:timesopened"), TypeDesc::INT,
                                     &timesopened);
            texsys->get_texture_info(all_filenames[i], 0,
                                     ustring("stat:bytesread"), TypeDesc::INT64,
                                     &bytesread);
            texsys->get_texture_info(all_filenames[i], 0,
                                     ustring("stat:iotime"), TypeDesc::FLOAT,
                                     &iotime);
            texsys->get_texture_info(all_filenames[i], 0,
                                     ustring("stat:image_size"),
                                     TypeDesc::INT64, &data_size);
            texsys->get_texture_info(all_filenames[i], 0,
                                     ustring("stat:file_size"), TypeDesc::INT64,
                                     &file_size);
            Strutil::print(
                "  {}: {}  opens={}, read={}, time={}, data={}, file={}\n", i,
                all_filenames[i], timesopened, Strutil::memformat(bytesread),
                Strutil::timeintervalformat(iotime, 2),
                Strutil::memformat(data_size), Strutil::memformat(file_size));
        }
    }

    if (runstats || verbose) {
        Strutil::print("Memory use: {}\n",
                       Strutil::memformat(Sysutil::memory_used(true)));
        Strutil::print("{}\n", texsys->getstats(verbose ? 2 : 1));
    }
    TextureSystem::destroy(texsys);

    if (verbose)
        Strutil::print("\nustrings: {}\n\n", ustring::getstats(false));

    // Delete any temporary files we created
    for (auto&& f : filenames_to_delete) {
        std::string err;
        Filesystem::remove(f, err);
    }
    shutdown();
    return 0;
}
