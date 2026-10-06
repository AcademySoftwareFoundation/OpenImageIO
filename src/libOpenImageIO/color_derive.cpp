// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// Deriving a color space's properties from what its transforms do, for a
// color space that no known Color Interop ID describes.

#include <algorithm>
#include <array>
#include <cmath>

#include "color_derive.h"

namespace OCIO = OCIO_NAMESPACE;


OIIO_NAMESPACE_BEGIN

namespace {

using Mat3 = std::array<double, 9>;  // Row-major, applied to column vectors
using Vec3 = std::array<double, 3>;

Mat3
mat3_mul(const Mat3& a, const Mat3& b)
{
    Mat3 m {};
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            for (int k = 0; k < 3; ++k)
                m[3 * r + c] += a[3 * r + k] * b[3 * k + c];
    return m;
}

Vec3
mat3_apply(const Mat3& m, const Vec3& v)
{
    return { m[0] * v[0] + m[1] * v[1] + m[2] * v[2],
             m[3] * v[0] + m[4] * v[1] + m[5] * v[2],
             m[6] * v[0] + m[7] * v[1] + m[8] * v[2] };
}

Mat3
mat3_inverse(const Mat3& m)
{
    const Mat3 adjugate { m[4] * m[8] - m[5] * m[7], m[2] * m[7] - m[1] * m[8],
                          m[1] * m[5] - m[2] * m[4], m[5] * m[6] - m[3] * m[8],
                          m[0] * m[8] - m[2] * m[6], m[2] * m[3] - m[0] * m[5],
                          m[3] * m[7] - m[4] * m[6], m[1] * m[6] - m[0] * m[7],
                          m[0] * m[4] - m[1] * m[3] };
    const double det = m[0] * adjugate[0] + m[1] * adjugate[3]
                       + m[2] * adjugate[6];
    Mat3 inverse;
    for (int i = 0; i < 9; ++i)
        inverse[i] = adjugate[i] / det;
    return inverse;
}

// CIE XYZ, with Y = 1, of the CIE xy chromaticity (x, y).
Vec3
xy_to_XYZ(double x, double y)
{
    return { x / y, 1.0, (1.0 - x - y) / y };
}

// The RGB to CIE XYZ matrix of primaries and white given as CIE xy.
Mat3
rgb_to_XYZ(const float* xy)
{
    Mat3 m;
    for (int c = 0; c < 3; ++c) {
        const Vec3 primary = xy_to_XYZ(xy[2 * c], xy[2 * c + 1]);
        for (int r = 0; r < 3; ++r)
            m[3 * r + c] = primary[r];
    }
    const Vec3 scale = mat3_apply(mat3_inverse(m), xy_to_XYZ(xy[6], xy[7]));
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            m[3 * r + c] *= scale[c];
    return m;
}

// Chromatic adaptation of CIE XYZ from white `from` to white `to`, scaling
// the `cone` responses (von Kries).
Mat3
adaptation(const Mat3& cone, const Vec3& from, const Vec3& to)
{
    const Vec3 f = mat3_apply(cone, from), t = mat3_apply(cone, to);
    const Mat3 scale { t[0] / f[0], 0, 0, 0, t[1] / f[1], 0, 0, 0, t[2] / f[2] };
    return mat3_mul(mat3_inverse(cone), mat3_mul(scale, cone));
}

const Mat3 bradford_cone { 0.8951, 0.2664, -0.1614, -0.7502, 1.7135,
                           0.0367, 0.0389, -0.0685, 1.0296 };
const Mat3 cat02_cone { 0.7328, 0.4296, -0.1624, -0.7036, 1.6975,
                        0.0061, 0.0030, 0.0136,  0.9834 };

// Whether two matrices are the same up to a positive scale, which the gain
// of a curve ahead of a matrix introduces.
bool
same_up_to_scale(const Mat3& a, const Mat3& b)
{
    double norm_a = 0.0, norm_b = 0.0;
    for (int i = 0; i < 9; ++i) {
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    norm_a = std::sqrt(norm_a);
    norm_b = std::sqrt(norm_b);
    if (!(norm_a > 0.0 && norm_b > 0.0))
        return false;
    for (int i = 0; i < 9; ++i)
        if (!(std::abs(a[i] / norm_a - b[i] / norm_b) < 1.0e-5))
            return false;
    return true;
}

double
vec3_length(const Vec3& v)
{
    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

}  // namespace



void
pvt::derive_color_space_properties(const OCIO::Config& config,
                                   const OCIO::ColorSpace& cs,
                                   cspan<const float*> gamuts, const float* ap0,
                                   const float*& xy, float& gamma)
{
    if (cs.isData())
        return;
    const bool display = cs.getReferenceSpaceType()
                         == OCIO::REFERENCE_SPACE_DISPLAY;
    const char* role   = display ? OCIO::ROLE_INTERCHANGE_DISPLAY
                                 : OCIO::ROLE_INTERCHANGE_SCENE;
    OCIO::ConstProcessorRcPtr decode;
    OCIO::ConstCPUProcessorRcPtr cpu;
    try {
        if (!config.hasRole(role))
            return;
        decode = config.getProcessor(cs.getName(), role);
        // No approximate powers and no fused operations, so a power curve
        // reads back as its exponent.
        cpu = decode->getOptimizedCPUProcessor(OCIO::OPTIMIZATION_NONE);
    } catch (const std::exception&) {
        return;
    }
    auto decoded = [&](const Vec3& rgb) {
        float pixel[3] = { float(rgb[0]), float(rgb[1]), float(rgb[2]) };
        cpu->applyRGB(pixel);
        return Vec3 { pixel[0], pixel[1], pixel[2] };
    };
    // A shared per-channel curve f followed by a matrix M decodes white less
    // white with one channel lowered to `t` as (f(1) - f(t)) times that
    // channel's column of M: each such probe is a multiple of its column, by
    // the same factor in all three channels, and the columns add up to white
    // less black. Probing down from white keeps every decoded value positive,
    // so a decoder that clamps negatives still reads as a matrix. Anything
    // else, such as a 3D LUT or a rendering, establishes nothing.
    const Vec3 black = decoded({ 0.0, 0.0, 0.0 });
    const Vec3 white = decoded({ 1.0, 1.0, 1.0 });
    auto lowered     = [&](int channel, double t) {
        Vec3 rgb { 1.0, 1.0, 1.0 };
        rgb[channel]     = t;
        const Vec3 value = decoded(rgb);
        return Vec3 { white[0] - value[0], white[1] - value[1],
                      white[2] - value[2] };
    };
    Mat3 matrix;
    Vec3 excess { white[0] - black[0], white[1] - black[1],
                  white[2] - black[2] };
    double scale = 0.0;
    for (int c = 0; c < 3; ++c) {
        const Vec3 column = lowered(c, 0.0);
        for (int r = 0; r < 3; ++r) {
            matrix[3 * r + c] = column[r];
            excess[r] -= column[r];
        }
        scale = std::max(scale, vec3_length(column));
    }
    if (!(scale > 0.0) || !std::isfinite(scale))
        return;
    const double tolerance = 1.0e-5 * scale;
    if (!(vec3_length(excess) <= tolerance))
        return;
    // How far f(t) has risen from f(0) toward f(1), at two probes.
    const double probes[2] = { 0.18, 0.5 };
    double rises[2]        = {};
    for (int p = 0; p < 2; ++p) {
        for (int c = 0; c < 3; ++c) {
            Vec3 value   = lowered(c, probes[p]);
            double dot   = 0.0;
            double norm2 = 0.0;
            for (int r = 0; r < 3; ++r) {
                dot += value[r] * matrix[3 * r + c];
                norm2 += matrix[3 * r + c] * matrix[3 * r + c];
            }
            const double factor = dot / norm2;
            for (int r = 0; r < 3; ++r)
                value[r] -= factor * matrix[3 * r + c];
            if (!(vec3_length(value) <= tolerance)
                || (c > 0 && !(std::abs(1.0 - factor - rises[p]) <= 1.0e-5)))
                return;
            rises[p] = 1.0 - factor;
        }
    }

    // Name the matrix as a published gamut, reached through the interchange
    // with no chromatic adaptation, Bradford or CAT02.
    const Vec3 interchange_white = display ? xy_to_XYZ(0.3127, 0.3290)
                                           : xy_to_XYZ(ap0[6], ap0[7]);
    const Mat3 from_ap0          = mat3_inverse(rgb_to_XYZ(ap0));
    for (const float* gamut : gamuts) {
        if (!gamut[1] || !gamut[3] || !gamut[5] || !gamut[7])
            continue;  // CIE XYZ, which has no RGB primaries
        const Mat3 to_XYZ      = rgb_to_XYZ(gamut);
        const Vec3 gamut_white = xy_to_XYZ(gamut[6], gamut[7]);
        for (const Mat3* cone :
             { (const Mat3*)nullptr, &bradford_cone, &cat02_cone }) {
            Mat3 expected = cone ? mat3_mul(adaptation(*cone, gamut_white,
                                                       interchange_white),
                                            to_XYZ)
                                 : to_XYZ;
            if (!display)
                expected = mat3_mul(from_ap0, expected);
            if (same_up_to_scale(matrix, expected)) {
                xy = gamut;
                break;
            }
        }
        if (xy)
            break;
    }

    // A pure power passes through black and scales each probe by its power.
    // It is reported only as an exponent the transforms state, so exactly.
    if (!(vec3_length(black) <= tolerance && rises[0] > 0.0 && rises[1] > 0.0))
        return;
    const double power = std::log(rises[0]) / std::log(probes[0]);
    if (!(std::abs(std::log(rises[1]) / std::log(probes[1]) - power)
          <= 1.0e-4 * power))
        return;
    if (std::abs(power - 1.0) <= 1.0e-4) {
        gamma = 1.0f;
        return;
    }
    auto ops = decode->createGroupTransform();
    for (int i = 0, n = ops->getNumTransforms(); i < n; ++i) {
        auto exponent = OCIO::DynamicPtrCast<const OCIO::ExponentTransform>(
            ops->getTransform(i));
        if (!exponent)
            continue;
        double value[4];
        exponent->getValue(value);
        if (value[0] != value[1] || value[0] != value[2] || !(value[0] > 0.0))
            continue;
        for (double stated : { value[0], 1.0 / value[0] })
            if (std::abs(stated - power) <= 1.0e-4 * power) {
                gamma = float(stated);
                return;
            }
    }
}

OIIO_NAMESPACE_END
