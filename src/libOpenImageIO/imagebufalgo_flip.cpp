// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// Implementation of ImageBufAlgo::FLIP_diff() -- FLIP perceptual image
// difference.
//
// Our APIs are described in imagebufalgo.h, and will not be recapitulated
// here.
//
// FLIP algorithm is based on:
//
// * "FLIP: A Difference Evaluator for Alternating Images"
//   Pontus Andersson, Jim Nilsson, Tomas Akenine-Moller, Magnus Oskarsson,
//   Kalle Astrom, Mark D. Fairchild. High Performance Graphics 2020.
//
// * "Visualizing Errors in Rendered High Dynamic Range Images"
//   Pontus Andersson, Jim Nilsson, Peter Shirley, Tomas Akenine-Moller.
//   Eurographics 2021.
//
// Reference implementation: FLIP.h by Pontus Ebelin (Andersson) and Tomas
// Akenine-Moller (NVIDIA), BSD-3-Clause. We do not include that code; this
// is a re-implementation using OIIO idioms.
//
// The original FLIP paper (2020) is for LDR images. The core algorithm for
// LDR-FLIP transforms to YCxCz, applies separable spatial filters (single
// Gaussian for Y/Cx, sum-of-two-Gaussians for Cz), converts to CIELab with
// Hunt chromatic adjustment, computes HyAB color distance with piecewise
// remap, then applies a separable feature filter (1st+2nd Gaussian
// derivatives on Y channel) to produce edge/point feature differences. Final
// per-pixel error is color^(1-feature).
//
// The follow-up paper (2021) extends it to HDR, and works as follows:
//
// * Determine a number of exposure "stops" that we will sample.
// * For each sampled exposure, scale down the image values, tone map to
//   the 0-1 range, and run the LDR FLIP algorithm on that image.
// * The final HDR error image is the per-pixel maximum across all the
//   exposure sample LDR FLIP results.
//
// We do not change the core of LDR or HDR FLIP algorithm, and ensure that
// when parameters are set as they are in the NVIDIA reference tool, ours
// matches numerically. We do, however, differ from the interface and usage of
// NVIDIA's reference implementaton of a command line tool in several ways
// that we thought were needed for usefulness and robustness in a in a
// production environment.
//
// Obvious shortcomings (for our use) of the reference implementation
// follow, and discssion of how we chose to handle them.
//
// 1. No handling of alpha or additional channels, which are very common
//    for renderer output.
//
//    Now: This is still unimplemented -- at present, IBA::FLIP_diff requires
//    a 3-channel RGB image and no other channels are considered.
//
//    Future: Continue to ignore alpha? Maybe always use FLIP LDR for alpha?
//    Maybe automatically compare alpha and other non-color data by normal
//    normal numerical differences? Unclear if alpha and additional channels
//    should be handled in IBA::FLIP_diff or only at the oiiotool level.
//
// 2. It assumes inputs uses sRGB (Rec709) color primaries, which is mostly
//    not what's used in high-end rendering these days.
//
//    Now: FLIP_diff checks the color space of the input images and
//    automatically transforms to "lin_rec709_scene", so it will accept
//    anything we support, but this is less efficient than it could be.
//
//    Future: Actually, the algorithm immediately transforms from linear
//    rec709 to XYZ to YCxCz, so there's no reason we can't to from ACEScg or
//    anything else to XYZ without the rec709 intermediary, or even directly
//    to YCxCz if we're clever.
//
// 3. (a) Selection of LDR vs HDR based on whether the input images are PNG or
//    OpenEXR (the NVIDIA tool offers no other choices). (b) Wildly different
//    numerical error values for LDR vs HDR of the same numerical values in
//    the image. That is, even if the image contains no values > 1.0, the
//    errors aren't the same if you use the LDR vs HDR algorithm, because the
//    latter does tone mapping, and also will choose exposures based on the
//    image content.
//
// 4. The reference tool's automatic choice of exposures based on the content
//    of the image (specifically, using the median and max luminance of the
//    reference image), and therefore the meaning of the error images and
//    their numerical values is not the same from image to image, even within
//    the same shot. That makes it very hard to use for animation, or for
//    having a fixed error threshold that's likely to  work across multiple
//    HDR images.
//
// 5. The reference tool does allow one to bypass the automatic selection
//    of exposures, but only by directly providing the start and stop
//    exposure settings. This seems counter-intuitive and unlikely to be
//    wielded successfully in a production setting.
//
// Our remedies for 3-5 are as follows:
// * To combat the LDR vs HDR disparity of meaning, we default to using HDR
//   for all images, though it's still possible through options to select LDR
//   if one needs to exactly match the LDR paper of the NVIDIA reference
//   implementation. (Open question: should we automatically use LDR if the
//   input images are known to be in a display-referred color space, but
//   use the HDR algorithm for all scene-referred color space inputs, even
//   if they don't contain HDR values?)
// * Instead of explicit selection of exposure levels (still possible by
//   option), prefer user selection of maximum expected luminance, from
//   which we compute exposure. It seems much more likely that people will
//   know their max luminance of their shot/show, and they certainly don't
//   want it to vary by frame.
// * For now, we're assuming that the median luminance rather than being
//   computed from each individual image, is always 0.18 (middle grey), as
//   that seems likely to be a plausible guess that avoids it changing from
//   frame to frame. If needed, we may in the future also allow this to be
//   specified as an option.
// * To avoid placing the burden on people to know or guess the maximum
//   luminance, we default that to 2.0, which gives a result not too far away
//   from the LDR figures, allows a moderate amount of HDR values without too
//   much clipping or compression of bright values, and is probably a broadly
//   applicable guess that will be more than adequate for most production
//   shots. TODO: compare whether there are any real-world production uses
//   where the guess of 2.0 max luminance is worse than a more knowlegable
//   setting, in a way that makes any practical difference.
//
// For now, this is all marked as "experimental": The C++ API calls are within
// the `ImageBufAlgo::experimental` namespace, and the oiiotool `--flipdiff`
// command will issue an error if the `--experimental` flag is not also used.
// This should allow people to try it out, and even for us to backport it to
// 3.1, while making it very clear that it's not officially a part of the
// public API, and thus exempt from our usual promise not to break back
// compatibility except at major release boundaries. We intend to remove the
// experimental designation by the time 3.2 is released in Fall, 2026.
//
// List of things still to be done before it's taken out of experimental
// designation, and beyond:
//
// - [ ] Find a strategy for handling alpha, additional color channels,
//       additional non-color channels. (And: should that be handled by
//       the IBA function, or just be an oiiotool feature?)
// - [ ] Directly handle other input color spaces (but especially ACEScg)
//       without needing to pass through "lin_rec709_scene" and possibly clip
//       or compress the gamut in the process.
// - [ ] Determine if there is any merit to using LDR for images known to be
//       in display-referred color spaces, or if LDR mode is only for
//       compatibility with the reference/research code and not recommended by
//       us for production use.
// - [ ] Validate defaulting to maximum luminance 2.0 and median 0.18 as
//       applicable to the vast majority of production uses.
// - [ ] More idiomatic use of OIIO. The initial translation of the reference
//       implementation to OIIO (aided by Claude, but heavily modified by LG)
//       did a pretty good job of making the code idiomatically OpenImageIO,
//       but there are still places where it's using 1D vectors or arrays
//       with "longhand" loops, that should be using ImageBuf and existing
//       IBA functions (some of which are already heavily optimized, or could
//       be).
// - [ ] Benchmarking and optimization (time and memory). The initial
//       translation has a lot of clearly wasteful code that can be improved.
//       There's a lot of needless copying, redundant operations, unwise use
//       of std::vector, and no use of SIMD.


#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include <OpenImageIO/Imath.h>
#include <OpenImageIO/color.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/strutil.h>

using Imath::Color3f;



OIIO_NAMESPACE_3_1_BEGIN

namespace {

// Summary statistics optionally returned by FLIP.
struct FLIPResults {
    double meanerror = 0;    ///< Mean of error map in [0,1].
    float maxerror   = 0;    ///< Maximum pixel error.
    int maxx = 0, maxy = 0;  ///< Pixel coordinates of the maximum error.
    /// HDR-only fields; zero for LDR:
    float startExposure = 0.0f;   ///< Actual start exposure stop used.
    float stopExposure  = 0.0f;   ///< Actual stop exposure stop used.
    int numExposures    = 0;      ///< Number of exposure steps used.
    bool error          = false;  ///< True if computation failed.
};



// ---------------------------------------------------------------------------
// FLIP perceptual constants  (see FLIP paper and FLIP.h reference)
// ---------------------------------------------------------------------------

static constexpr float FLIP_gpc = 0.4f;    // piecewise remap knee position
static constexpr float FLIP_gpt = 0.95f;   // piecewise remap knee value
static constexpr float FLIP_gw  = 0.082f;  // feature filter width scale
static constexpr float FLIP_gqc = 0.7f;    // color difference exponent
static constexpr float FLIP_gqf = 0.5f;    // feature difference exponent

// GaussianConstants (from FLIP.h xGaussianConstants struct)
// Used to build the spatial CSF filter kernels.
static constexpr float GC_a1x = 1.0f, GC_b1x = 0.0047f;  // Y channel
static constexpr float GC_a1y = 1.0f, GC_b1y = 0.0053f;  // Cx channel
static constexpr float GC_a1z = 34.1f, GC_b1z = 0.04f;   // Cz component 1
static constexpr float GC_a2z = 13.5f, GC_b2z = 0.025f;  // Cz component 2

// D65 reference illuminant and its inverse
static constexpr float D65x  = 0.950428545f;
static constexpr float D65y  = 1.000000000f;
static constexpr float D65z  = 1.088900371f;
static constexpr float iD65x = 1.052156925f;
static constexpr float iD65y = 1.000000000f;
static constexpr float iD65z = 0.918357670f;

static constexpr float PI_F = 3.14159265358979f;

// Default PPD: 0.7m distance, 3840px wide, 0.7m monitor -> ~67.02 PPD
static constexpr float FLIP_ppd_default = 67.02f;


// ---------------------------------------------------------------------------
// Pixel-level color space conversions
// ---------------------------------------------------------------------------

// Linear Rec.709 RGB -> CIE XYZ (D65)
// Matrix from FLIP.h LinearRGBToXYZ (fractional form).
inline Color3f
linearRGB_to_XYZ(Color3f c)
{
    return Color3f(
        c[0] * (10135552.0f / 24577794.0f) + c[1] * (8788810.0f / 24577794.0f)
            + c[2] * (4435075.0f / 24577794.0f),
        c[0] * (2613072.0f / 12288897.0f) + c[1] * (8788810.0f / 12288897.0f)
            + c[2] * (887015.0f / 12288897.0f),
        c[0] * (1425312.0f / 73733382.0f) + c[1] * (8788810.0f / 73733382.0f)
            + c[2] * (70074185.0f / 73733382.0f));
}

// CIE XYZ -> linear Rec.709 RGB (D65)
inline Color3f
XYZ_to_linearRGB(Color3f c)
{
    return Color3f(
        c[0] * 3.241003275f + c[1] * (-1.537398934f) + c[2] * (-0.498615861f),
        c[0] * (-0.969224334f) + c[1] * 1.875930071f + c[2] * 0.041554224f,
        c[0] * 0.055639423f + c[1] * (-0.204011202f) + c[2] * 1.057148933f);
}

// CIE XYZ -> FLIP's YCxCz (luminance + two chroma channels, D65)
// This is NOT the same as standard CIELab; it is an intermediate for the
// spatial CSF filtering step.
inline Color3f
XYZ_to_YCxCz(Color3f c)
{
    float X = c[0] * iD65x;
    float Y = c[1] * iD65y;
    float Z = c[2] * iD65z;
    return Color3f(116.0f * Y - 16.0f, 500.0f * (X - Y), 200.0f * (Y - Z));
}

// FLIP's YCxCz -> CIE XYZ (D65)
inline Color3f
YCxCz_to_XYZ(Color3f c)
{
    float Y  = (c[0] + 16.0f) / 116.0f;
    float Cx = c[1] / 500.0f;
    float Cz = c[2] / 200.0f;
    return Color3f((Y + Cx) * D65x, Y * D65y, (Y - Cz) * D65z);
}

// CIE XYZ -> CIELab (D65)
inline Color3f
XYZ_to_CIELab(Color3f c)
{
    constexpr float delta     = 6.0f / 29.0f;
    constexpr float deltaCube = delta * delta * delta;
    constexpr float factor    = 1.0f / (3.0f * delta * delta);
    constexpr float term      = 4.0f / 29.0f;
    float X                   = c[0] * iD65x;
    float Y                   = c[1] * iD65y;
    float Z                   = c[2] * iD65z;
    auto f                    = [&](float t) {
        return t > deltaCube ? fast_cbrt(t) : factor * t + term;
    };
    float fx = f(X), fy = f(Y), fz = f(Z);
    return Color3f(116.0f * fy - 16.0f, 500.0f * (fx - fy), 200.0f * (fy - fz));
}

// Convert linear Rec.709 RGB to FLIP's YCxCz (combines the two steps above)
inline Color3f
linearRGB_to_YCxCz(Color3f c)
{
    return XYZ_to_YCxCz(linearRGB_to_XYZ(c));
}

// Hunt chromatic adaptation: scale chrominance by luminance.
// Input/output: CIELab. Returns (L, 0.01*L*a, 0.01*L*b).
inline Color3f
hunt_adjust(Color3f lab)
{
    return Color3f(lab[0], 0.01f * lab[0] * lab[1], 0.01f * lab[0] * lab[2]);
}

// HyAB distance between two Hunt-adjusted CIELab colors.
// = |L_a - L_b| + ||AB_a - AB_b||_2
inline float
hyab(Color3f a, Color3f b)
{
    float da = a[1] - b[1];
    float db = a[2] - b[2];
    return std::abs(a[0] - b[0]) + std::sqrt(da * da + db * db);
}

// Precompute the maximum possible HyAB^gqc color distance (green vs blue).
static float
flip_max_color_distance()
{
    Color3f greenLab = XYZ_to_CIELab(
        linearRGB_to_XYZ(Color3f(0.0f, 1.0f, 0.0f)));
    Color3f blueLab = XYZ_to_CIELab(
        linearRGB_to_XYZ(Color3f(0.0f, 0.0f, 1.0f)));
    Color3f gh = hunt_adjust(greenLab);
    Color3f bh = hunt_adjust(blueLab);
    return std::pow(hyab(gh, bh), FLIP_gqc);
}

// ---------------------------------------------------------------------------
// Spatial CSF filter kernel construction
// ---------------------------------------------------------------------------

// Gaussian in the alternative form used by FLIP's spatial filter.
// g(x^2) = a * sqrt(pi/b) * exp(-pi^2 * x^2 / b)
inline float
gauss_alt(float x2, float a, float b)
{
    return a * std::sqrt(PI_F / b) * std::exp(-(PI_F * PI_F) * x2 / b);
}

// Square-root variant needed for the sum-of-Gaussians (Cz) channel.
// gsqrt(x^2) = sqrt(a * sqrt(pi/b)) * exp(-pi^2 * x^2 / b)
inline float
gauss_sqrt_alt(float x2, float a, float b)
{
    return std::sqrt(a * std::sqrt(PI_F / b))
           * std::exp(-(PI_F * PI_F) * x2 / b);
}

// Radius for the spatial CSF filter given ppd.
static int
spatial_filter_radius(float ppd)
{
    float maxB = std::max({ GC_b1x, GC_b1y, GC_b1z, GC_b2z });
    return int(std::ceil(3.0f * std::sqrt(maxB / (2.0f * PI_F * PI_F)) * ppd));
}

struct SpatialKernels {
    int radius = 0;
    // Per-tap weights; width = 2*radius+1.
    std::vector<float> wY;    // Y channel (single Gaussian)
    std::vector<float> wCx;   // Cx channel (single Gaussian)
    std::vector<float> wCz1;  // Cz component 1 (sqrt Gaussian)
    std::vector<float> wCz2;  // Cz component 2 (sqrt Gaussian)
    int width() const { return 2 * radius + 1; }
};

static SpatialKernels
build_spatial_kernels(float ppd)
{
    SpatialKernels k;
    k.radius = spatial_filter_radius(ppd);
    int w    = k.width();
    float dX = 1.0f / ppd;

    k.wY.resize(w);
    k.wCx.resize(w);
    k.wCz1.resize(w);
    k.wCz2.resize(w);

    float sumY = 0, sumCx = 0, sumCz1 = 0, sumCz2 = 0;
    for (int i = 0; i < w; ++i) {
        float ix  = (i - k.radius) * dX;
        float ix2 = ix * ix;
        k.wY[i]   = gauss_alt(ix2, GC_a1x, GC_b1x);
        k.wCx[i]  = gauss_alt(ix2, GC_a1y, GC_b1y);
        k.wCz1[i] = gauss_sqrt_alt(ix2, GC_a1z, GC_b1z);
        k.wCz2[i] = gauss_sqrt_alt(ix2, GC_a2z, GC_b2z);
        sumY += k.wY[i];
        sumCx += k.wCx[i];
        sumCz1 += k.wCz1[i];
        sumCz2 += k.wCz2[i];
    }

    // Normalize: Y and Cx by sum; Cz by sqrt(sumCz1^2 + sumCz2^2)
    float normCz = 1.0f / std::sqrt(sumCz1 * sumCz1 + sumCz2 * sumCz2);
    for (int i = 0; i < w; ++i) {
        k.wY[i] /= sumY;
        k.wCx[i] /= sumCx;
        k.wCz1[i] *= normCz;
        k.wCz2[i] *= normCz;
    }
    return k;
}


// ---------------------------------------------------------------------------
// Feature (edge/point) filter kernel construction
// ---------------------------------------------------------------------------

struct FeatureKernel {
    int radius = 0;
    std::vector<float> wG;    // Gaussian
    std::vector<float> wDG;   // 1st derivative of Gaussian (normalized)
    std::vector<float> wDDG;  // 2nd derivative of Gaussian (normalized)
    int width() const { return 2 * radius + 1; }
};

static FeatureKernel
build_feature_kernel(float ppd)
{
    FeatureKernel k;
    float stdDev = 0.5f * FLIP_gw * ppd;
    k.radius     = int(std::ceil(3.0f * stdDev));
    int w        = k.width();

    k.wG.resize(w);
    k.wDG.resize(w);
    k.wDDG.resize(w);

    auto gauss1d = [&](float x) {
        return std::exp(-(x * x) / (2.0f * stdDev * stdDev));
    };

    float gSum = 0, dgPos = 0, dgNeg = 0, ddgPos = 0, ddgNeg = 0;
    for (int i = 0; i < w; ++i) {
        float x   = float(i - k.radius);
        float g   = gauss1d(x);
        float dg  = -x * g;
        float ddg = (x * x / (stdDev * stdDev) - 1.0f) * g;
        k.wG[i]   = g;
        k.wDG[i]  = dg;
        k.wDDG[i] = ddg;
        gSum += g;
        if (dg > 0)
            dgPos += dg;
        else
            dgNeg -= dg;
        if (ddg > 0)
            ddgPos += ddg;
        else
            ddgNeg -= ddg;
    }

    for (int i = 0; i < w; ++i) {
        k.wG[i] /= gSum;
        k.wDG[i] /= (k.wDG[i] >= 0.0f ? dgPos : dgNeg);
        k.wDDG[i] /= (k.wDDG[i] >= 0.0f ? ddgPos : ddgNeg);
    }
    return k;
}


// ---------------------------------------------------------------------------
// LDR-FLIP implementation
// ---------------------------------------------------------------------------

// Run the full LDR-FLIP pipeline.
// ref and test must be float images with 3 channels.
// Inputs are in linear Rec.709 RGB, values expected in [0,1].
// dst is set to a 1-channel float image of the same spatial extent as roi,
// with pixel values in [0,1].
// Returns false and sets dst error on failure.
static bool
LDR_FLIP(ImageBuf& dst, const ImageBuf& ref, const ImageBuf& test, float ppd,
         ROI roi, int nthreads)
{
    OIIO_ASSERT(ref.nchannels() >= 3);
    const int w = roi.width();
    const int h = roi.height();
    if (w == 0 || h == 0)
        return true;

    // ------------------------------------------------------------------
    // Step 1: Convert both images to YCxCz, clamping to [0,1] first.
    // Stored as flat float arrays [y*w + x]*3.
    // ------------------------------------------------------------------
    std::vector<float> refYCC(w * h * 3);
    std::vector<float> tstYCC(w * h * 3);

    ImageBufAlgo::parallel_image(
        ROI(roi.xbegin, roi.xend, roi.ybegin, roi.yend, 0, 1, 0, 1), nthreads,
        [&](ROI r) {
            ImageBuf::ConstIterator<float> rit(ref, r);
            ImageBuf::ConstIterator<float> tit(test, r);
            for (; !rit.done(); ++rit, ++tit) {
                int lx = rit.x() - roi.xbegin;
                int ly = rit.y() - roi.ybegin;
                Color3f rRGB(OIIO::clamp(rit[0], 0.0f, 1.0f),
                             OIIO::clamp(rit[1], 0.0f, 1.0f),
                             OIIO::clamp(rit[2], 0.0f, 1.0f));
                Color3f tRGB(OIIO::clamp(tit[0], 0.0f, 1.0f),
                             OIIO::clamp(tit[1], 0.0f, 1.0f),
                             OIIO::clamp(tit[2], 0.0f, 1.0f));
                Color3f rYCC    = linearRGB_to_YCxCz(rRGB);
                Color3f tYCC    = linearRGB_to_YCxCz(tRGB);
                int idx         = (ly * w + lx) * 3;
                refYCC[idx]     = rYCC[0];
                refYCC[idx + 1] = rYCC[1];
                refYCC[idx + 2] = rYCC[2];
                tstYCC[idx]     = tYCC[0];
                tstYCC[idx + 1] = tYCC[1];
                tstYCC[idx + 2] = tYCC[2];
            }
        });

    // ------------------------------------------------------------------
    // Step 2: Spatial CSF filter — separable, two kernel variants.
    //
    // Intermediate arrays (x-direction pass results):
    //   iRefY, iRefCx, iRefCz1, iRefCz2  [h*w floats each]
    //   iTstY, iTstCx, iTstCz1, iTstCz2
    // ------------------------------------------------------------------
    SpatialKernels sk = build_spatial_kernels(ppd);
    const int sr      = sk.radius;

    std::vector<float> iRefY(w * h, 0.0f), iRefCx(w * h, 0.0f);
    std::vector<float> iRefCz1(w * h, 0.0f), iRefCz2(w * h, 0.0f);
    std::vector<float> iTstY(w * h, 0.0f), iTstCx(w * h, 0.0f);
    std::vector<float> iTstCz1(w * h, 0.0f), iTstCz2(w * h, 0.0f);

    // x-direction pass
    ImageBufAlgo::parallel_image(
        ROI(roi.xbegin, roi.xend, roi.ybegin, roi.yend, 0, 1, 0, 1), nthreads,
        [&](ROI r) {
            for (int gy = r.ybegin; gy < r.yend; ++gy) {
                int ly = gy - roi.ybegin;
                for (int lx = 0; lx < w; ++lx) {
                    float ry = 0, rcx = 0, rcz1 = 0, rcz2 = 0;
                    float ty = 0, tcx = 0, tcz1 = 0, tcz2 = 0;
                    for (int ix = -sr; ix <= sr; ++ix) {
                        int xx    = OIIO::clamp(lx + ix, 0, w - 1);
                        int fi    = ix + sr;
                        int idx   = (ly * w + xx) * 3;
                        float rY  = refYCC[idx];
                        float rCx = refYCC[idx + 1];
                        float rCz = refYCC[idx + 2];
                        float tY  = tstYCC[idx];
                        float tCx = tstYCC[idx + 1];
                        float tCz = tstYCC[idx + 2];
                        ry += sk.wY[fi] * rY;
                        rcx += sk.wCx[fi] * rCx;
                        rcz1 += sk.wCz1[fi] * rCz;
                        rcz2 += sk.wCz2[fi] * rCz;
                        ty += sk.wY[fi] * tY;
                        tcx += sk.wCx[fi] * tCx;
                        tcz1 += sk.wCz1[fi] * tCz;
                        tcz2 += sk.wCz2[fi] * tCz;
                    }
                    int oi      = ly * w + lx;
                    iRefY[oi]   = ry;
                    iRefCx[oi]  = rcx;
                    iRefCz1[oi] = rcz1;
                    iRefCz2[oi] = rcz2;
                    iTstY[oi]   = ty;
                    iTstCx[oi]  = tcx;
                    iTstCz1[oi] = tcz1;
                    iTstCz2[oi] = tcz2;
                }
            }
        });

    // ------------------------------------------------------------------
    // Step 3: y-direction pass + compute color difference.
    // For each pixel: reconstruct full YCxCz, round-trip through linear
    // RGB (clamping), convert to CIELab + Hunt, compute HyAB^gqc,
    // apply piecewise remap to [0,1].
    // ------------------------------------------------------------------
    const float cmax   = flip_max_color_distance();  // precomputed once
    const float pccmax = FLIP_gpc * cmax;

    std::vector<float> colorDiff(w * h, 0.0f);

    ImageBufAlgo::parallel_image(
        ROI(roi.xbegin, roi.xend, roi.ybegin, roi.yend, 0, 1, 0, 1), nthreads,
        [&](ROI r) {
            for (int gy = r.ybegin; gy < r.yend; ++gy) {
                int ly = gy - roi.ybegin;
                for (int lx = 0; lx < w; ++lx) {
                    float rY = 0, rCx = 0, rCz1 = 0, rCz2 = 0;
                    float tY = 0, tCx = 0, tCz1 = 0, tCz2 = 0;
                    for (int iy = -sr; iy <= sr; ++iy) {
                        int yy = OIIO::clamp(ly + iy, 0, h - 1);
                        int fi = iy + sr;
                        int si = yy * w + lx;
                        rY += sk.wY[fi] * iRefY[si];
                        rCx += sk.wCx[fi] * iRefCx[si];
                        rCz1 += sk.wCz1[fi] * iRefCz1[si];
                        rCz2 += sk.wCz2[fi] * iRefCz2[si];
                        tY += sk.wY[fi] * iTstY[si];
                        tCx += sk.wCx[fi] * iTstCx[si];
                        tCz1 += sk.wCz1[fi] * iTstCz1[si];
                        tCz2 += sk.wCz2[fi] * iTstCz2[si];
                    }

                    // Reconstruct YCxCz, back to linear RGB, clamp, then to CIELab
                    Color3f rYCxCz(rY, rCx, rCz1 + rCz2);
                    Color3f tYCxCz(tY, tCx, tCz1 + tCz2);
                    Color3f rRGB = XYZ_to_linearRGB(YCxCz_to_XYZ(rYCxCz));
                    Color3f tRGB = XYZ_to_linearRGB(YCxCz_to_XYZ(tYCxCz));
                    rRGB[0]      = OIIO::clamp(rRGB[0], 0.0f, 1.0f);
                    rRGB[1]      = OIIO::clamp(rRGB[1], 0.0f, 1.0f);
                    rRGB[2]      = OIIO::clamp(rRGB[2], 0.0f, 1.0f);
                    tRGB[0]      = OIIO::clamp(tRGB[0], 0.0f, 1.0f);
                    tRGB[1]      = OIIO::clamp(tRGB[1], 0.0f, 1.0f);
                    tRGB[2]      = OIIO::clamp(tRGB[2], 0.0f, 1.0f);

                    Color3f rLab = hunt_adjust(
                        XYZ_to_CIELab(linearRGB_to_XYZ(rRGB)));
                    Color3f tLab = hunt_adjust(
                        XYZ_to_CIELab(linearRGB_to_XYZ(tRGB)));

                    float cd = std::pow(hyab(rLab, tLab), FLIP_gqc);

                    // Piecewise remap [0, cmax] -> [0, 1]
                    if (cd < pccmax)
                        cd = cd * FLIP_gpt / pccmax;
                    else
                        cd = FLIP_gpt
                             + (cd - pccmax) / (cmax - pccmax)
                                   * (1.0f - FLIP_gpt);

                    colorDiff[ly * w + lx] = cd;
                }
            }
        });

    // ------------------------------------------------------------------
    // Step 4: Feature (edge/point) filter — x-direction pass.
    // Operates on the Y channel of YCxCz, normalized to [0,1].
    // Intermediate per pixel: (dx, ddx, gauss_Y).
    // ------------------------------------------------------------------
    FeatureKernel fk = build_feature_kernel(ppd);
    const int fr     = fk.radius;

    std::vector<float> iFeatRefDx(w * h, 0.0f), iFeatRefDdx(w * h, 0.0f);
    std::vector<float> iFeatRefG(w * h, 0.0f);
    std::vector<float> iFeatTstDx(w * h, 0.0f), iFeatTstDdx(w * h, 0.0f);
    std::vector<float> iFeatTstG(w * h, 0.0f);

    constexpr float oneOver116     = 1.0f / 116.0f;
    constexpr float sixteenOver116 = 16.0f / 116.0f;

    ImageBufAlgo::parallel_image(
        ROI(roi.xbegin, roi.xend, roi.ybegin, roi.yend, 0, 1, 0, 1), nthreads,
        [&](ROI r) {
            for (int gy = r.ybegin; gy < r.yend; ++gy) {
                int ly = gy - roi.ybegin;
                for (int lx = 0; lx < w; ++lx) {
                    float dxR = 0, ddxR = 0, gR = 0;
                    float dxT = 0, ddxT = 0, gT = 0;
                    for (int ix = -fr; ix <= fr; ++ix) {
                        int xx = OIIO::clamp(lx + ix, 0, w - 1);
                        int fi = ix + fr;
                        // Y is channel 0 of YCxCz; normalize to [0,1]
                        float yR = refYCC[(ly * w + xx) * 3] * oneOver116
                                   + sixteenOver116;
                        float yT = tstYCC[(ly * w + xx) * 3] * oneOver116
                                   + sixteenOver116;
                        dxR += fk.wDG[fi] * yR;
                        ddxR += fk.wDDG[fi] * yR;
                        gR += fk.wG[fi] * yR;
                        dxT += fk.wDG[fi] * yT;
                        ddxT += fk.wDDG[fi] * yT;
                        gT += fk.wG[fi] * yT;
                    }
                    int oi          = ly * w + lx;
                    iFeatRefDx[oi]  = dxR;
                    iFeatRefDdx[oi] = ddxR;
                    iFeatRefG[oi]   = gR;
                    iFeatTstDx[oi]  = dxT;
                    iFeatTstDdx[oi] = ddxT;
                    iFeatTstG[oi]   = gT;
                }
            }
        });

    // ------------------------------------------------------------------
    // Step 5: Feature filter y-direction pass + final combine.
    // For each pixel: compute edge/point differences, then
    //   flip = colorDiff ^ (1 - featureDiff)
    // Write directly to the output array, then set_pixels into dst.
    // ------------------------------------------------------------------
    constexpr float normFactor = 1.0f / 1.4142135623730951f;  // 1/sqrt(2)

    std::vector<float> result(w * h, 0.0f);

    ImageBufAlgo::parallel_image(
        ROI(roi.xbegin, roi.xend, roi.ybegin, roi.yend, 0, 1, 0, 1), nthreads,
        [&](ROI r) {
            for (int gy = r.ybegin; gy < r.yend; ++gy) {
                int ly = gy - roi.ybegin;
                for (int lx = 0; lx < w; ++lx) {
                    float dxR = 0, ddxR = 0, dyR = 0, ddyR = 0;
                    float dxT = 0, ddxT = 0, dyT = 0, ddyT = 0;
                    for (int iy = -fr; iy <= fr; ++iy) {
                        int yy = OIIO::clamp(ly + iy, 0, h - 1);
                        int fi = iy + fr;
                        int si = yy * w + lx;
                        // (dx_x, ddx_x) filtered by Gaussian_y:
                        dxR += fk.wG[fi] * iFeatRefDx[si];
                        ddxR += fk.wG[fi] * iFeatRefDdx[si];
                        dxT += fk.wG[fi] * iFeatTstDx[si];
                        ddxT += fk.wG[fi] * iFeatTstDdx[si];
                        // Gaussian_x filtered by (dy_y, ddy_y):
                        dyR += fk.wDG[fi] * iFeatRefG[si];
                        ddyR += fk.wDDG[fi] * iFeatRefG[si];
                        dyT += fk.wDG[fi] * iFeatTstG[si];
                        ddyT += fk.wDDG[fi] * iFeatTstG[si];
                    }
                    float edgeR = std::sqrt(dxR * dxR + dyR * dyR);
                    float edgeT = std::sqrt(dxT * dxT + dyT * dyT);
                    float ptR   = std::sqrt(ddxR * ddxR + ddyR * ddyR);
                    float ptT   = std::sqrt(ddxT * ddxT + ddyT * ddyT);
                    float featDiff
                        = std::pow(normFactor
                                       * std::max(std::abs(edgeR - edgeT),
                                                  std::abs(ptR - ptT)),
                                   FLIP_gqf);
                    float flip          = std::pow(colorDiff[ly * w + lx],
                                                   1.0f - featDiff);
                    result[ly * w + lx] = flip;
                }
            }
        });

    // Write result to dst (1-channel float, same spatial origin as roi)
    ImageSpec outspec(w, h, 1, TypeFloat);
    outspec.x = roi.xbegin;
    outspec.y = roi.ybegin;
    dst.reset(outspec);
    ROI out_roi(roi.xbegin, roi.xend, roi.ybegin, roi.yend, 0, 1, 0, 1);
    dst.set_pixels(out_roi, TypeFloat, result.data());

    return !dst.has_error();
}



// ---------------------------------------------------------------------------
// HDR-FLIP helpers
// ---------------------------------------------------------------------------

// Apply a multiplicative exposure to an image (in place on a copy).
// Multiplies all channels by 2^level.
static void
apply_exposure(ImageBuf& img, float level)
{
    float m = std::pow(2.0f, level);
    ImageBufAlgo::parallel_image(get_roi(img.spec()), 0, [&](ROI roi) {
        for (ImageBuf::Iterator<float> it(img, roi); !it.done(); ++it) {
            it[0] = it[0] * m;
            it[1] = it[1] * m;
            it[2] = it[2] * m;
        }
    });
}


// Tonemap a single float3 channel triple.
// Coefficients layout: pixel = (c^2*a[0] + c*a[1] + a[2]) / (c^2*a[3] + c*a[4] + a[5])
inline float
tonemap_channel(float c, const float tc[6])
{
    float num = c * c * tc[0] + c * tc[1] + tc[2];
    float den = c * c * tc[3] + c * tc[4] + tc[5];
    return den > 0.0f ? num / den : 0.0f;
}


// Apply tone mapping to all pixels of img, in place (modifies the float copy).
static void
apply_tonemap(ImageBuf& img, string_view tonemapper)
{
    // Tonemapper coefficients from FLIP.h ToneMappingCoefficients
    // Reinhard is special-cased (uses luminance-based formula).
    static constexpr float tc_aces[6]
        = { 0.6f * 0.6f * 2.51f, 0.6f * 0.03f, 0.0f,
            0.6f * 0.6f * 2.43f, 0.6f * 0.59f, 0.14f };
    static constexpr float tc_hable[6] = { 0.231683f, 0.013791f, 0.0f,
                                           0.18f,     0.3f,      0.018f };

    ImageBufAlgo::parallel_image(get_roi(img.spec()), 0, [&](ROI roi) {
        for (ImageBuf::Iterator<float> it(img, roi); !it.done(); ++it) {
            float r = it[0], g = it[1], b = it[2];
            if (tonemapper == "reinhard") {
                // Reinhard: out = c / (1 + luminance)
                float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                float d   = 1.0f + lum;
                r /= d;
                g /= d;
                b /= d;
            } else if (tonemapper == "hable") {
                r = tonemap_channel(r, tc_hable);
                g = tonemap_channel(g, tc_hable);
                b = tonemap_channel(b, tc_hable);
            } else {
                // Default: ACES
                r = tonemap_channel(r, tc_aces);
                g = tonemap_channel(g, tc_aces);
                b = tonemap_channel(b, tc_aces);
            }
            it[0] = r;
            it[1] = g;
            it[2] = b;
        }
    });
}


// Clamp all three starting channels to [0,1].
static void
clamp_rgb(ImageBuf& img)
{
    ImageBufAlgo::parallel_image(get_roi(img.spec()), 0, [&](ROI roi) {
        for (ImageBuf::Iterator<float> it(img, roi); !it.done(); ++it) {
            it[0] = OIIO::clamp(float(it[0]), 0.0f, 1.0f);
            it[1] = OIIO::clamp(float(it[1]), 0.0f, 1.0f);
            it[2] = OIIO::clamp(float(it[2]), 0.0f, 1.0f);
        }
    });
}


// Second-degree equation solver: a*x^2 + b*x + c = 0
static void
solve_quadratic(float a, float b, float c, float& xMin, float& xMax)
{
    if (a == 0.0f) {
        xMin = xMax = (b != 0.0f ? -c / b : 0.0f);
        return;
    }
    float d1 = -0.5f * (b / a);
    float d2 = std::sqrt(d1 * d1 - c / a);
    xMin     = d1 - d2;
    xMax     = d1 + d2;
}



// Compute exposure range for HDR-FLIP given the max and median luminance of
// the pixels of the reference image, and the tone mapping curve to use.
static void
compute_exposures_from_max_and_median(string_view tonemapper,
                                      float max_luminance,
                                      float median_luminance,
                                      float& startExposure, float& stopExposure)
{
    // Choose the tonemapper coefficients' "peak" threshold solution.
    // For each tonemapper: find xMax where curve reaches t=0.85.
    static constexpr float tc_aces[6]
        = { 0.6f * 0.6f * 2.51f, 0.6f * 0.03f, 0.0f,
            0.6f * 0.6f * 2.43f, 0.6f * 0.59f, 0.14f };
    static constexpr float tc_hable[6] = { 0.231683f, 0.013791f, 0.0f,
                                           0.18f,     0.3f,      0.018f };
    static constexpr float tc_reinhard[6]
        = { 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f };

    const float* tc = (tonemapper == "reinhard") ? tc_reinhard
                      : (tonemapper == "hable")  ? tc_hable
                                                 : tc_aces;

    float t   = 0.85f;
    float a_q = tc[0] - t * tc[3];
    float b_q = tc[1] - t * tc[4];
    float c_q = tc[2] - t * tc[5];
    float xMin, xMax;
    solve_quadratic(a_q, b_q, c_q, xMin, xMax);

    startExposure = std::log2(xMax / max_luminance);
    stopExposure  = std::log2(xMax / median_luminance);
}



// Compute the maximum and median luminance from the image.
static void
compute_max_median_from_image(const ImageBuf& ref, string_view tonemapper,
                              float& max_luminance, float& median_luminance)
{
    // Gather luminance statistics from reference image
    max_luminance = -std::numeric_limits<float>::max();
    std::vector<float> luminances;
    luminances.reserve(ref.spec().width * ref.spec().height);

    OIIO_ASSERT(ref.nchannels() >= 3);
    for (ImageBuf::ConstIterator<float> it(ref); !it.done(); ++it) {
        float r   = it[0];
        float g   = it[1];
        float b   = it[2];
        float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        luminances.push_back(lum);
        max_luminance = std::max(max_luminance, lum);
    }

    size_t mid = luminances.size() / 2;
    std::nth_element(luminances.begin(), luminances.begin() + mid,
                     luminances.end());
    median_luminance = std::max(luminances[mid],
                                std::numeric_limits<float>::epsilon());
}



// Run the HDR-FLIP pipeline.
static bool
HDR_FLIP(ImageBuf& dst, ImageBuf* exposuremap, const ImageBuf& ref,
         const ImageBuf& test, float ppd, string_view tonemapper,
         float& startExposure, float& stopExposure, int& numExposures,
         ImageBufAlgo::KWArgs options, ROI roi, int nthreads)
{
    OIIO_ASSERT(ref.nchannels() >= 3);

    const int w = roi.width();
    const int h = roi.height();

    bool debug          = options.get_int("debug");
    float max_luminance = options.get_float("maxluminance", 2.0f);

    if (std::isfinite(startExposure) && std::isfinite(stopExposure)) {
        // If caller supplied explicit exposure, use them!
        if (debug)
            print("FLIP using supplied exposures {}, {}\n", startExposure,
                  stopExposure);
    } else if (max_luminance > 0.0f) {
        // Caller supplied a max luminance, we will use that to determine the
        // exposure range. Default to an assumption that the median luminance
        // is 18% gray.
        float median_luminance = options.get_float("medianluminance", 0.18f);
        compute_exposures_from_max_and_median(tonemapper, max_luminance,
                                              median_luminance, startExposure,
                                              stopExposure);
        if (debug)
            print(
                "FLIP supplied max = {}, median = {}, computed exposure {}, {}\n",
                max_luminance, median_luminance, startExposure, stopExposure);
    } else {
        // Auto-compute exposures if not supplied
        float max_luminance, median_luminance;
        compute_max_median_from_image(ref, tonemapper, max_luminance,
                                      median_luminance);
        float autoStart, autoStop;
        compute_exposures_from_max_and_median(tonemapper, max_luminance,
                                              median_luminance, autoStart,
                                              autoStop);
        if (std::isnan(startExposure))
            startExposure = autoStart;
        if (std::isnan(stopExposure))
            stopExposure = autoStop;
        if (debug)
            print(
                "FLIP image max = {}, median = {}, auto computed exposure {}, {}\n",
                max_luminance, median_luminance, startExposure, stopExposure);
    }
    if (startExposure > stopExposure) {
        dst.errorfmt(
            "FLIP_diff: startExposure ({}) must be <= stopExposure ({})",
            startExposure, stopExposure);
        return false;
    }
    if (numExposures <= 0) {
        numExposures = std::max(2,
                                int(std::ceil(stopExposure - startExposure)));
        if (debug)
            print("FLIP auto numExposures {}\n", numExposures);
    }

    // Initialize output images
    ImageSpec outspec(w, h, 1, TypeFloat);
    outspec.x = roi.xbegin;
    outspec.y = roi.ybegin;
    dst.reset(outspec);
    ImageBufAlgo::zero(dst);

    if (exposuremap) {
        exposuremap->reset(outspec);
        ImageBufAlgo::zero(*exposuremap);
    }

    // Exposure loop
    float step = (numExposures > 1)
                     ? (stopExposure - startExposure) / float(numExposures - 1)
                     : 0.0f;

    for (int i = 0; i < numExposures; ++i) {
        float exposure = startExposure + i * step;

        // Make float copies of ref and test, apply exposure + tonemap + clamp
        ImageBuf rCopy, tCopy;
        ImageBufAlgo::copy(rCopy, ref, TypeFloat);
        ImageBufAlgo::copy(tCopy, test, TypeFloat);
        apply_exposure(rCopy, exposure);
        apply_exposure(tCopy, exposure);
        apply_tonemap(rCopy, tonemapper);
        apply_tonemap(tCopy, tonemapper);
        clamp_rgb(rCopy);
        clamp_rgb(tCopy);

        // Run LDR-FLIP for this exposure
        ImageBuf tmp;
        if (!LDR_FLIP(tmp, rCopy, tCopy, ppd, roi, nthreads))
            return false;

        // Per-pixel max: if tmp > dst, update dst (and exposuremap)
        float exposureNorm = (numExposures > 1)
                                 ? float(i) / float(numExposures - 1)
                                 : 0.0f;
        ROI out_roi(roi.xbegin, roi.xend, roi.ybegin, roi.yend, 0, 1, 0, 1);
        ImageBufAlgo::parallel_image(out_roi, nthreads, [&](ROI r) {
            ImageBuf::ConstIterator<float> tit(tmp, r);
            ImageBuf::Iterator<float> dit(dst, r);
            if (exposuremap) {
                ImageBuf::Iterator<float> eit(*exposuremap, r);
                for (; !tit.done(); ++tit, ++dit, ++eit) {
                    float tv = tit[0];
                    if (tv > dit[0]) {
                        dit[0] = tv;
                        eit[0] = exposureNorm;
                    }
                }
            } else {
                for (; !tit.done(); ++tit, ++dit) {
                    float tv = tit[0];
                    if (tv > dit[0])
                        dit[0] = tv;
                }
            }
        });
    }

    return !dst.has_error();
}


// ---------------------------------------------------------------------------
// Stats accumulation
// ---------------------------------------------------------------------------

static void
accumulate_flip_stats(const ImageBuf& flipmap, FLIPResults& result, ROI roi)
{
    double sum   = 0.0;
    float maxval = 0.0;
    int maxx = roi.xbegin, maxy = roi.ybegin;

    for (ImageBuf::ConstIterator<float> it(flipmap, roi); !it.done(); ++it) {
        float v = it[0];
        sum += v;
        if (v > maxval) {
            maxval = v;
            maxx   = it.x();
            maxy   = it.y();
        }
    }

    uint64_t npixels = uint64_t(roi.width()) * uint64_t(roi.height());
    result.meanerror = npixels > 0 ? sum / double(npixels) : 0.0;
    result.maxerror  = maxval;
    // Squash very small values to 0 so tiny numerical precision differences
    // don't make near-zero look different from zero.
    if (std::abs(result.meanerror) < 1.0e-6f)
        result.meanerror = 0.0f;
    if (std::abs(result.maxerror) < 1.0e-6f)
        result.maxerror = 0.0f;
    result.maxx = maxx;
    result.maxy = maxy;
}


// ---------------------------------------------------------------------------
// Core FLIP dispatch: handles KWArgs, colorspace check, LDR vs HDR.
// ---------------------------------------------------------------------------

static bool
flip_impl(ImageBuf& dst, ImageBuf* exposuremap, FLIPResults* result,
          const ImageBuf& ref, const ImageBuf& test,
          ImageBufAlgo::KWArgs options, ROI roi, int nthreads)
{
    // Parse options
    float ppd           = options.get_float("ppd", FLIP_ppd_default);
    int hdr             = options.get_int("hdr", 1);
    string_view tmapper = options.get_string("tonemapper", "aces");
    float startExp      = options.get_float("startExposure",
                                            std::numeric_limits<float>::quiet_NaN());
    float stopExp       = options.get_float("stopExposure",
                                            std::numeric_limits<float>::quiet_NaN());
    int numExp          = options.get_int("numExposures", 0);

    // Validate inputs
    if (!ref.initialized() || !test.initialized()) {
        dst.errorfmt("FLIP_diff: uninitialized input image");
        return false;
    }
    if (roi.depth() > 1) {
        dst.errorfmt("FLIP_diff: 3D images are not supported");
        return false;
    }

    // Establish ROI: default to union of both images, clipped to 3 channels
    if (!roi.defined())
        roi = roi_union(get_roi(ref.spec()), get_roi(test.spec()));
    if (roi.chbegin + 3 > ref.nchannels()
        || roi.chbegin + 3 > test.nchannels()) {
        dst.errorfmt(
            "FLIP_diff: images must have at least 3 channels starting at channel {}",
            roi.chbegin);
        return false;
    }
    roi.chend = roi.chbegin + 3;  // std::max(roi.chend, roi.chbegin + 3);

#if 0
    // Integer pixel types force LDR
    if (hdr && !ref.spec().format.is_floating_point()) {
        // print(stderr, "FLIP warning: integer pixel type forces LDR mode\n");
        hdr = 0;
    }
    // FIXME: This is questionable! It seems to me that a half exr file and a
    // uint8 tiff file ought to have identical results if they contain the
    // same normalized values. Maybe we should decide based on whether it's a
    // display-referred color space?
#endif

    // Convert inputs to float working copies. Note that this also resets
    // their image origins to (0,0) and clips to 3 channels.
    ImageBuf refF, tstF;
    ImageBufAlgo::copy(refF, ref, TypeFloat, roi);
    // print(" pre-copy  ref  roi = {}\n", ref.roi());
    // print(" post-copy refF roi = {}\n", refF.roi());
    ImageBufAlgo::copy(tstF, test, TypeFloat, roi);
    // After copying, we expect 3 channels. Readjust roi.
    ROI orig_roi = roi;  // save original ROI, we'll need it later
    roi          = refF.roi();
    OIIO_CONTRACT_ASSERT(roi.nchannels() == 3);

    // Colorspace check: if tagged with a non-linear space, convert to
    // lin_rec709_scene (linear Rec.709, D65).
    auto check_and_convert = [&](ImageBuf& img, const ImageBuf& orig) -> bool {
        std::string cs = orig.spec().get_string_attribute("oiio:ColorSpace");
        if (cs.empty())
            return true;  // untagged: assume already lin_rec709_scene
        const ColorConfig& cc = ColorConfig::default_colorconfig();
        if (cc.equivalent(cs, "lin_rec709_scene"))
            return true;  // already linear Rec.709
        ImageBuf converted;
        if (!ImageBufAlgo::colorconvert(converted, img, cs, "lin_rec709_scene",
                                        true, "", "", {}, {}, nthreads)) {
            dst.errorfmt(
                "FLIP_diff: colorconvert from '{}' to 'lin_rec709_scene' failed: {}",
                cs, converted.geterror());
            return false;
        }
        img = std::move(converted);
        return true;
    };
    if (!check_and_convert(refF, ref) || !check_and_convert(tstF, test))
        return false;

    // Run FLIP
    ImageBuf flipmap;
    bool ok;
    if (hdr) {
        ok = HDR_FLIP(flipmap, exposuremap, refF, tstF, ppd, tmapper, startExp,
                      stopExp, numExp, options, roi, nthreads);
        if (ok && result) {
            result->startExposure = startExp;
            result->stopExposure  = stopExp;
            result->numExposures  = numExp;
        }
    } else {
        ok = LDR_FLIP(flipmap, refF, tstF, ppd, roi, nthreads);
    }
    if (!ok) {
        dst.errorfmt("FLIP_diff: {}", flipmap.geterror());
        return false;
    }

    // Accumulate stats into result
    if (result) {
        ROI flip_roi(roi.xbegin, roi.xend, roi.ybegin, roi.yend, 0, 1, 0, 1);
        accumulate_flip_stats(flipmap, *result, flip_roi);
    }

    dst = std::move(flipmap);

    // Restore original image offsets
    dst.specmod().x      = orig_roi.xbegin;
    dst.specmod().y      = orig_roi.ybegin;
    dst.specmod().full_x = ref.spec().full_x;
    dst.specmod().full_y = ref.spec().full_y;

    return !dst.has_error();
}


}  // namespace



// ---------------------------------------------------------------------------
// Public API implementations
// ---------------------------------------------------------------------------

bool
ImageBufAlgo::experimental::FLIP_diff(ImageBuf& dst, const ImageBuf& ref,
                                      const ImageBuf& test, KWArgs options,
                                      ROI roi, int nthreads)
{
    FLIPResults flipresults;
    bool ok = flip_impl(dst, nullptr, &flipresults, ref, test, options, roi,
                        nthreads);
    if (ok) {
        // Store the results as attributes on the output image.
        ImageSpec& spec(dst.specmod());
        spec.attribute("FLIP:meanerror", float(flipresults.meanerror));
        spec.attribute("FLIP:maxerror", float(flipresults.maxerror));
        spec.attribute("FLIP:maxx", flipresults.maxx);
        spec.attribute("FLIP:maxy", flipresults.maxy);
        if (options.get_int("hdr", 1)) {
            spec.attribute("FLIP:startExposure", flipresults.startExposure);
            spec.attribute("FLIP:stopExposure", flipresults.stopExposure);
            spec.attribute("FLIP:numExposures", flipresults.numExposures);
        }
    }
    return ok;
}


ImageBuf
ImageBufAlgo::experimental::FLIP_diff(const ImageBuf& ref, const ImageBuf& test,
                                      KWArgs options, ROI roi, int nthreads)
{
    ImageBuf dst;
    (void)FLIP_diff(dst, ref, test, options, roi, nthreads);
    // Ignoring error return is ok here because the error is reported in dst
    return dst;
}

OIIO_NAMESPACE_3_1_END
