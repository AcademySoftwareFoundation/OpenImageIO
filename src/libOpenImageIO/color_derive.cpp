// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// Deriving the primaries of a color space from what its transforms do.

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <string>
#include <unordered_set>

#include <OpenImageIO/Imath.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/strutil.h>

#include "color_derive.h"

namespace OCIO = OCIO_NAMESPACE;


OIIO_NAMESPACE_BEGIN

namespace {

using Imath::M33d;
using Imath::V3d;

// Matrices here apply to column vectors.
V3d
apply(const M33d& m, const V3d& v)
{
    return V3d(m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
               m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
               m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z);
}



V3d
xy_to_XYZ(double x, double y)
{
    return V3d(x / y, 1.0, (1.0 - x - y) / y);
}



// The RGB to CIE XYZ matrix of primaries and white given as CIE xy.
M33d
rgb_to_XYZ(const double xy[8])
{
    const V3d r = xy_to_XYZ(xy[0], xy[1]);
    const V3d g = xy_to_XYZ(xy[2], xy[3]);
    const V3d b = xy_to_XYZ(xy[4], xy[5]);
    const M33d primaries(r.x, g.x, b.x, r.y, g.y, b.y, r.z, g.z, b.z);
    const V3d s = apply(primaries.inverse(), xy_to_XYZ(xy[6], xy[7]));
    return primaries * M33d(s.x, 0.0, 0.0, 0.0, s.y, 0.0, 0.0, 0.0, s.z);
}



// CIE XYZ of the white points a reading is tried under.
const V3d white_D65(0.95045592705167, 1.0, 1.08905775075988);
const V3d white_D60(0.95264607456985, 1.0, 1.00882518435159);  // ACES
const V3d white_DCI(0.89458689458689, 1.0, 0.95441595441595);
const V3d white_D50(0.96429567642956771, 1.0, 0.82510460251046047);

enum class Adaptation { Bradford, CAT02, None };

// Chromatic adaptation of CIE XYZ from white `from` to white `to`.
M33d
adapt(const V3d& from, const V3d& to, Adaptation method)
{
    static const M33d bradford(0.8951, 0.2664, -0.1614, -0.7502, 1.7135, 0.0367,
                               0.0389, -0.0685, 1.0296);
    static const M33d cat02(0.7328, 0.4296, -0.1624, -0.7036, 1.6975, 0.0061,
                            0.0030, 0.0136, 0.9834);
    if (method == Adaptation::None
        || (std::abs(from.x - to.x) <= 1e-12 && std::abs(from.y - to.y) <= 1e-12
            && std::abs(from.z - to.z) <= 1e-12))
        return M33d();
    const M33d& cone   = method == Adaptation::Bradford ? bradford : cat02;
    const V3d response = apply(cone, from);
    const V3d target   = apply(cone, to);
    return cone.inverse()
           * M33d(target.x / response.x, 0.0, 0.0, 0.0, target.y / response.y,
                  0.0, 0.0, 0.0, target.z / response.z)
           * cone;
}



// The interchange space a color space's matrices are read against:
// ACES2065-1 for a scene-referred space, CIE XYZ with a D65 white for a
// display-referred one.
struct Reference {
    M33d to_XYZ;
    M33d from_XYZ;
    V3d white;  // CIE XYZ
};

const Reference&
reference(bool display)
{
    static const Reference aces = [] {
        const double ap0[8] = { .7347, .2653, 0.0,    1.0,
                                .0001, -.077, .32168, .33767 };
        Reference r;
        r.to_XYZ   = rgb_to_XYZ(ap0);
        r.from_XYZ = r.to_XYZ.inverse();
        r.white    = apply(r.to_XYZ, V3d(1.0, 1.0, 1.0));
        return r;
    }();
    static const Reference xyz = [] {
        Reference r;
        r.white = white_D65;
        return r;
    }();
    return display ? xyz : aces;
}



// The coordinate rounded to six decimals, or to fewer when it is within 2e-7
// of that, so that equal primaries read through different matrices compare
// equal.
double
round_coordinate(double value)
{
    const double rounded = std::round(value * 1e6) / 1e6;
    for (double scale : { 1e5, 1e4, 1e3, 1e2 }) {
        const double coarser = std::round(rounded * scale) / scale;
        if (std::abs(rounded - coarser) <= 2e-7)
            return coarser + 0.0;  // No negative zero
    }
    return rounded + 0.0;
}



// The chromaticities (R G B W) that a linear RGB to CIE XYZ matrix gives, or
// false if one of them has no CIE xy (X + Y + Z is zero).
bool
chromaticities(const M33d& rgb_to_xyz, std::array<double, 8>& xy)
{
    const V3d basis[4] = { V3d(1.0, 0.0, 0.0), V3d(0.0, 1.0, 0.0),
                           V3d(0.0, 0.0, 1.0), V3d(1.0, 1.0, 1.0) };
    for (int i = 0; i < 4; ++i) {
        const V3d XYZ    = apply(rgb_to_xyz, basis[i]);
        const double sum = XYZ.x + XYZ.y + XYZ.z;
        if (!std::isfinite(sum) || std::abs(sum) < 1e-12)
            return false;
        xy[2 * i]     = round_coordinate(XYZ.x / sum);
        xy[2 * i + 1] = round_coordinate(XYZ.y / sum);
    }
    // A white with x equal to y is CIE XYZ's own, illuminant E.
    if (xy[6] == xy[7])
        xy[6] = xy[7] = 1.0 / 3.0;
    return true;
}



// Decimal places of a coordinate, up to nine.
int
decimal_places(double value)
{
    long long scaled = std::llround(std::abs(value) * 1e9);
    if (scaled == 0)
        return 0;
    int places = 9;
    for (; places > 0 && scaled % 10 == 0; --places)
        scaled /= 10;
    return places;
}



// How few decimals the primaries' coordinates have: 1 when none has any, 0
// when each has `cap` or more.
double
exactness(const std::array<double, 8>& xy, int cap)
{
    int total = 0;
    for (int i = 0; i < 6; ++i)
        total += std::min(decimal_places(xy[i]), cap);
    return 1.0 - double(total) / double(cap * 6);
}



// The gamut among `gamuts` that the matrix reproduces within 1e-5, reached
// under Bradford, CAT02 or no chromatic adaptation.
const float*
known_gamut(const M33d& rgb_to_reference, const Reference& ref,
            cspan<const float*> gamuts)
{
    const float* found = nullptr;
    double closest     = 1e-5;
    for (const float* gamut : gamuts) {
        // CIE XYZ taken as RGB has primaries on y = 0, and no such matrix.
        if (gamut[1] == 0.0f || gamut[3] == 0.0f || gamut[5] == 0.0f)
            continue;
        double xy[8];
        std::copy(gamut, gamut + 8, xy);
        const M33d to_XYZ = rgb_to_XYZ(xy);
        const V3d white   = apply(to_XYZ, V3d(1.0, 1.0, 1.0));
        for (Adaptation method :
             { Adaptation::Bradford, Adaptation::CAT02, Adaptation::None }) {
            const M33d candidate = ref.from_XYZ
                                   * adapt(white, ref.white, method) * to_XYZ;
            double residual      = 0.0;
            for (int r = 0; r < 3; ++r)
                for (int c = 0; c < 3; ++c)
                    residual = std::max(residual,
                                        std::abs(candidate[r][c]
                                                 - rgb_to_reference[r][c]));
            if (residual < closest) {
                closest = residual;
                found   = gamut;
            }
        }
    }
    return found;
}



// For primaries no known gamut matches: read them under each usual white
// point and adaptation, take the first reading whose coordinates are round
// enough, and otherwise the roundest, or failing that the first (D65,
// Bradford). False if no reading has CIE xy.
bool
roundest_reading(const M33d& rgb_to_reference, const Reference& ref,
                 std::array<double, 8>& best)
{
    const M33d rgb_to_XYZ = ref.to_XYZ * rgb_to_reference;
    double best_rank      = 0.0;
    bool found            = false;
    for (const V3d& white : { white_D65, white_D60, white_DCI, white_D50 }) {
        for (Adaptation method : { Adaptation::Bradford, Adaptation::CAT02 }) {
            std::array<double, 8> xy;
            if (!chromaticities(adapt(ref.white, white, method) * rgb_to_XYZ,
                                xy))
                continue;
            if (!found) {
                best  = xy;
                found = true;
            }
            if (exactness(xy, 6) > 0.6) {
                best = xy;
                return true;
            }
            const double rank = exactness(xy, 9);
            if (rank > best_rank && rank > 0.2) {
                best_rank = rank;
                best      = xy;
            }
        }
    }
    return found;
}



bool is_simple_transform(const OCIO::ConstTransformRcPtr& transform);



// The matrices that lead the conversion from the interchange space `role` to
// the color space `name`, multiplied into one: clamps and exposures (matrices
// that scale every channel alike) are skipped, and the first curve ends them.
// A conversion with no matrix mixing channels ahead of its first curve is in
// the interchange space's own primaries if every later step acts on each
// channel alone, and otherwise establishes none: this returns false.
bool
leading_matrices(const OCIO::Config& config, const char* role, const char* name,
                 M33d& matrices)
{
    OCIO::ConstGroupTransformRcPtr group;
    try {
        // Lossless optimization folds and cancels matrices without composing
        // LUTs, which only the curves after the matrices would use; over
        // OCIO's built-in configs it reads the same matrices as any heavier
        // optimization, much faster.
        group = config.getProcessor(role, name)
                    ->getOptimizedProcessor(OCIO::OptimizationFlags(
                        OCIO::OPTIMIZATION_LOSSLESS
                        & ~OCIO::OPTIMIZATION_SIMPLIFY_OPS))
                    ->createGroupTransform();
    } catch (const std::exception&) {
        return false;
    }
    matrices.makeIdentity();
    bool mixed  = false;  // A matrix ahead of the curves mixes channels
    bool curved = false;  // A curve came first
    for (int i = 0, n = group->getNumTransforms(); i < n; ++i) {
        const OCIO::ConstTransformRcPtr transform = group->getTransform(i);
        if (transform->getTransformType() == OCIO::TRANSFORM_TYPE_RANGE)
            continue;
        auto matrix = OCIO::DynamicPtrCast<const OCIO::MatrixTransform>(
            transform);
        if (!matrix) {
            if (mixed)
                break;
            if (!is_simple_transform(transform))
                return false;
            curved = true;
            continue;
        }
        double m[16];
        matrix->getMatrix(m);
        M33d step(m[0], m[1], m[2], m[4], m[5], m[6], m[8], m[9], m[10]);
        bool mixes = false;
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
                mixes |= r != c && std::abs(step[r][c]) > 1e-9;
        if (curved) {
            if (mixes)
                return false;
            continue;
        }
        if (!mixes && std::abs(step[1][1] - step[0][0]) <= 1e-9
            && std::abs(step[2][2] - step[0][0]) <= 1e-9)
            continue;
        if (matrix->getDirection() == OCIO::TRANSFORM_DIR_INVERSE)
            step.invert();
        matrices = step * matrices;
        mixed |= mixes;
    }
    return true;
}



// Whether a transform, other than a group, file or color space reference, is
// one whose matrices and curves describe a color encoding: not a 3D LUT,
// grade, look, view or rendering.
bool
is_simple_transform(const OCIO::ConstTransformRcPtr& transform)
{
    switch (transform->getTransformType()) {
    case OCIO::TRANSFORM_TYPE_BUILTIN: {
        auto builtin = OCIO::DynamicPtrCast<const OCIO::BuiltinTransform>(
            transform);
        const std::string style(builtin ? builtin->getStyle() : "");
        return style.rfind("ACES-OUTPUT", 0) != 0
               && style.rfind("ACES-LMT", 0) != 0;
    }
    case OCIO::TRANSFORM_TYPE_FIXED_FUNCTION: {
#if OCIO_VERSION_HEX >= 0x02040000
        auto ff = OCIO::DynamicPtrCast<const OCIO::FixedFunctionTransform>(
            transform);
        return ff
               && (ff->getStyle() == OCIO::FIXED_FUNCTION_LIN_TO_DOUBLE_LOG
                   || ff->getStyle() == OCIO::FIXED_FUNCTION_LIN_TO_GAMMA_LOG);
#else
        return false;
#endif
    }
    case OCIO::TRANSFORM_TYPE_MATRIX:
    case OCIO::TRANSFORM_TYPE_RANGE:
    case OCIO::TRANSFORM_TYPE_EXPONENT:
    case OCIO::TRANSFORM_TYPE_EXPONENT_WITH_LINEAR:
    case OCIO::TRANSFORM_TYPE_LOG:
    case OCIO::TRANSFORM_TYPE_LOG_AFFINE:
    case OCIO::TRANSFORM_TYPE_LOG_CAMERA:
    case OCIO::TRANSFORM_TYPE_ALLOCATION:
    case OCIO::TRANSFORM_TYPE_LUT1D:
    case OCIO::TRANSFORM_TYPE_GRADING_RGB_CURVE: return true;
    default: return false;
    }
}



bool
has_context_variable(const char* s)
{
    return s && (std::strchr(s, '$') || std::strchr(s, '%'));
}



// Whether a color space's transforms (from the reference if it has them,
// otherwise to it) include anything but simple ones, following color space
// references. Data and is-unique spaces count as complex, and so does a
// reference cycle.
bool
is_complex(const OCIO::Config& config, const OCIO::ConstColorSpaceRcPtr& cs,
           std::unordered_set<std::string>& visiting)
{
    if (!cs || cs->isData() || cs->hasCategory("is-unique")
        || has_context_variable(cs->getName()))
        return true;
    if (!visiting.insert(cs->getName()).second)
        return true;
    std::function<bool(const OCIO::ConstTransformRcPtr&)> complex_transform;
    complex_transform = [&](const OCIO::ConstTransformRcPtr& transform) {
        if (!transform)
            return false;
        switch (transform->getTransformType()) {
        // A file is simple only in a format for matrices, 1D LUTs or CLF
        // process lists; any other, such as a 3D LUT, is not.
        case OCIO::TRANSFORM_TYPE_FILE: {
            auto file = OCIO::DynamicPtrCast<const OCIO::FileTransform>(
                transform);
            const std::string ext
                = Filesystem::extension(file ? file->getSrc() : "", false);
            for (const char* simple : { "spimtx", "spi1d", "ctf", "clf" })
                if (Strutil::iequals(ext, simple))
                    return false;
            return true;
        }
        case OCIO::TRANSFORM_TYPE_GROUP: {
            auto group = OCIO::DynamicPtrCast<const OCIO::GroupTransform>(
                transform);
            for (int i = 0, n = group ? group->getNumTransforms() : 0; i < n;
                 ++i)
                if (complex_transform(group->getTransform(i)))
                    return true;
            return !group;
        }
        case OCIO::TRANSFORM_TYPE_COLORSPACE: {
            auto cst = OCIO::DynamicPtrCast<const OCIO::ColorSpaceTransform>(
                transform);
            if (!cst)
                return true;
            if (has_context_variable(cst->getSrc())
                || has_context_variable(cst->getDst()))
                return false;
            return is_complex(config, config.getColorSpace(cst->getSrc()),
                              visiting)
                   || is_complex(config, config.getColorSpace(cst->getDst()),
                                 visiting);
        }
        default: return !is_simple_transform(transform);
        }
    };
    auto from          = cs->getTransform(OCIO::COLORSPACE_DIR_FROM_REFERENCE);
    const bool complex = complex_transform(
        from ? from : cs->getTransform(OCIO::COLORSPACE_DIR_TO_REFERENCE));
    visiting.erase(cs->getName());
    return complex;
}



// The color space a transform chain ends in, when its last step is a
// ColorSpaceTransform, following groups and their inversion.
std::string
trailing_color_space(const OCIO::ConstTransformRcPtr& transform,
                     OCIO::TransformDirection direction)
{
    if (!transform)
        return {};
    const bool inverse = (direction == OCIO::TRANSFORM_DIR_INVERSE)
                         != (transform->getDirection()
                             == OCIO::TRANSFORM_DIR_INVERSE);
    if (auto cst = OCIO::DynamicPtrCast<const OCIO::ColorSpaceTransform>(
            transform)) {
        const char* end = inverse ? cst->getSrc() : cst->getDst();
        return end ? end : "";
    }
    auto group = OCIO::DynamicPtrCast<const OCIO::GroupTransform>(transform);
    if (!group || group->getNumTransforms() == 0)
        return {};
    return trailing_color_space(
        group->getTransform(inverse ? 0 : group->getNumTransforms() - 1),
        inverse ? OCIO::TRANSFORM_DIR_INVERSE : OCIO::TRANSFORM_DIR_FORWARD);
}



bool
derive(const OCIO::Config& config, const OCIO::ColorSpace& cs,
       cspan<const float*> gamuts, std::array<float, 8>& xy, int depth)
{
    if (cs.isData())
        return false;
    const bool display = cs.getReferenceSpaceType()
                         == OCIO::REFERENCE_SPACE_DISPLAY;
    const char* role   = display ? OCIO::ROLE_INTERCHANGE_DISPLAY
                                 : OCIO::ROLE_INTERCHANGE_SCENE;
    if (!config.hasRole(role))
        return false;

    // A space whose transforms are not simple but that ends in another
    // color space answers as that one.
    std::unordered_set<std::string> visiting;
    if (depth < 8
        && is_complex(config, config.getColorSpace(cs.getName()), visiting)) {
        std::string target;
        if (auto from = cs.getTransform(OCIO::COLORSPACE_DIR_FROM_REFERENCE))
            target = trailing_color_space(from, OCIO::TRANSFORM_DIR_FORWARD);
        else
            target = trailing_color_space(
                cs.getTransform(OCIO::COLORSPACE_DIR_TO_REFERENCE),
                OCIO::TRANSFORM_DIR_INVERSE);
        if (!target.empty() && target != cs.getName())
            if (auto other = config.getColorSpace(target.c_str()))
                if (derive(config, *other, gamuts, xy, depth + 1))
                    return true;
    }

    M33d reference_to_rgb;
    if (!leading_matrices(config, role, cs.getName(), reference_to_rgb)
        || std::abs(reference_to_rgb.determinant()) < 1e-12)
        return false;
    const M33d rgb_to_reference = reference_to_rgb.inverse();
    const Reference& ref        = reference(display);
    if (const float* gamut = known_gamut(rgb_to_reference, ref, gamuts)) {
        std::copy(gamut, gamut + 8, xy.begin());
        return true;
    }
    std::array<double, 8> read;
    if (!roundest_reading(rgb_to_reference, ref, read))
        return false;
    for (int i = 0; i < 8; ++i)
        xy[i] = float(read[i]);
    return true;
}

}  // namespace



bool
pvt::derive_chromaticities(const OCIO::Config& config,
                           const OCIO::ColorSpace& cs,
                           cspan<const float*> gamuts, std::array<float, 8>& xy)
{
    return derive(config, cs, gamuts, xy, 0);
}

OIIO_NAMESPACE_END
