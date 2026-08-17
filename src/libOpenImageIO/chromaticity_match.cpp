// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// Pure chromaticity math for color-space search by characterization: the
// reserved-primaries table keyed by interop-id token, the AP0->XYZ(D65)
// matrix that turns a set of AP0-anchored RGB probes into chromaticity
// coordinates, and the coordinate rounding/snapping that is the *only* place
// numerical fuzz is absorbed. Once rounding lands, chromaticity equality is
// coordinate-exact -- a Chromaticities is a std::array, so callers compare
// with plain `==` / std::find (no epsilon, no dedicated compare helper).
//
// Everything here is pure and config-free: reserved_chromaticities_for_id
// takes an id string, and chromaticities_from_ap0_probes takes the already-
// probed AP0 RGB quartet, so both unit-test without a live OCIO config. The
// config-driving probe (colorspace -> interchange apply that produces those
// AP0 RGB values) lives with the search walk that consumes it.
//
// Single-hypothesis derivation: the matrix is Bradford-adapted from the ACES
// white to D65 only. Non-D65 / log-curve spaces and registry gamuts absent
// from the reserved table are not resolved here; the multi-hypothesis
// whitepoint/CAT sweep is a documented follow-on.

#include "color_pvt.h"
#include "imageio_pvt.h"

#include <OpenImageIO/strutil.h>

#include <array>
#include <cmath>
#include <utility>

OIIO_NAMESPACE_BEGIN

namespace pvt {

double
round_chromaticity_coord(double value)
{
    // Round to 6 decimals, then snap to the nearest coarser 5/4/3/2-digit
    // grid if it lands within snapTol. First (finest) grid within tolerance
    // wins. This absorbs OCIO chromaticity floats that come back as e.g.
    // 0.329999998; downstream equality is exact on the result.
    const double rounded            = std::round(value * 1.0e6) / 1.0e6;
    static constexpr double snaptol = 2.0e-7;  // 2 * 10^-(6+1)
    for (int digits : { 5, 4, 3, 2 }) {
        const double factor    = std::pow(10.0, digits);
        const double candidate = std::round(rounded * factor) / factor;
        if (std::abs(rounded - candidate) <= snaptol)
            return candidate;
    }
    return rounded;
}


std::optional<Chromaticities>
reserved_chromaticities_for_id(string_view interop_id)
{
    if (interop_id.empty())
        return {};

    // Reserved (R,G,B,W) xy primaries for well-known interop-id tokens, per
    // the CIF registry reserved tables. Probed as an "_<token>_" substring of
    // the lowered id (so a token must be a *complete* gamut component, not an
    // arbitrary fragment), first match wins -- table order is readability
    // only.
    static const std::pair<const char*, Chromaticities> reserved[] = {
        // clang-format off
        { "bmdwg5",            {{ {{0.7177215, 0.3171181}}, {{0.228041, 0.861569}},
                                  {{0.1005841, -0.0820452}}, {{0.312717, 0.3290312}} }} },
        { "wg5",               {{ {{0.7177215, 0.3171181}}, {{0.228041, 0.861569}},
                                  {{0.1005841, -0.0820452}}, {{0.312717, 0.3290312}} }} },
        { "sgamut3venice",     {{ {{0.74046426, 0.27936437}}, {{0.08924115, 0.89380953}},
                                  {{0.11048824, -0.05257933}}, {{0.3127, 0.329}} }} },
        { "sgamut3cinevenice", {{ {{0.77590187, 0.27450239}}, {{0.1886829, 0.82868494}},
                                  {{0.10133738, -0.08918752}}, {{0.3127, 0.329}} }} },
        { "rec2020",           {{ {{0.708, 0.292}}, {{0.170, 0.797}},
                                  {{0.131, 0.046}}, {{0.3127, 0.329}} }} },
        { "rec601pal",         {{ {{0.64, 0.33}}, {{0.29, 0.60}},
                                  {{0.15, 0.06}}, {{0.3127, 0.329}} }} },
        { "rec601",            {{ {{0.63, 0.34}}, {{0.31, 0.595}},
                                  {{0.155, 0.07}}, {{0.3127, 0.329}} }} },
        { "rec709",            {{ {{0.64, 0.33}}, {{0.30, 0.60}},
                                  {{0.15, 0.06}}, {{0.3127, 0.329}} }} },
        { "p3d65",             {{ {{0.68, 0.32}}, {{0.265, 0.69}},
                                  {{0.15, 0.06}}, {{0.3127, 0.329}} }} },
        { "ap0",               {{ {{0.7347, 0.2653}}, {{0.0, 1.0}},
                                  {{0.0001, -0.077}}, {{0.32168, 0.33767}} }} },
        { "ap1",               {{ {{0.713, 0.293}}, {{0.165, 0.830}},
                                  {{0.128, 0.044}}, {{0.32168, 0.33767}} }} },
        // clang-format on
    };

    const std::string lowered = Strutil::lower(interop_id);
    for (const auto& [token, chroma] : reserved) {
        const std::string probe = std::string("_") + token + "_";
        if (lowered.find(probe) != std::string::npos)
            return chroma;
    }
    // AdobeRGB is matched only on the exact id or an "_adobergb_" token, never
    // via the substring loop, because many ids carry "rgb" incidentally.
    if (lowered == "adobergb"
        || lowered.find("_adobergb_") != std::string::npos)
        return Chromaticities { { { { 0.64, 0.33 } },
                                  { { 0.21, 0.71 } },
                                  { { 0.15, 0.06 } },
                                  { { 0.3127, 0.329 } } } };
    return {};
}


std::optional<Chromaticities>
chromaticities_from_ap0_probes(cspan<float> ap0_rgb)
{
    if (ap0_rgb.size() != 12)
        return {};

    // AP0 (ACES2065-1) RGB -> CIE XYZ, Bradford-adapted from the ACES white
    // (0.32168, 0.33767) to D65 (XYZ 0.95045592705167, 1, 1.08905775075988).
    // Hardcoded to match OCIO's builtin transform constants closely enough
    // that probe-derived coordinates land exactly on reference values after
    // round_chromaticity_coord. Single hypothesis: D65 whitepoint + Bradford
    // CAT only -- the multi-hypothesis whitepoint/CAT sweep is a follow-on.
    static const double kAp0ToXyzD65[3][3] = {
        { 0.93827984927725538, -0.0044514458123613527, 0.016627523586776195 },
        { 0.33736889078783672, 0.72952156669026558, -0.0668904574781026 },
        { 0.0011739508496858876, -0.0037107064020525725, 1.0915945063122463 },
    };

    // Four probes -- pure R, G, B, W pushed through colorspace -> AP0
    // interchange by the caller. A transfer curve that maps 0->0 and 1->1
    // drops out, and xy is scale-invariant, so pure gain is harmless.
    Chromaticities out;
    for (int i = 0; i < 4; ++i) {
        const float* rgb = ap0_rgb.data() + i * 3;
        double xyz[3];
        for (int r = 0; r < 3; ++r)
            xyz[r] = kAp0ToXyzD65[r][0] * double(rgb[0])
                     + kAp0ToXyzD65[r][1] * double(rgb[1])
                     + kAp0ToXyzD65[r][2] * double(rgb[2]);
        const double sum = xyz[0] + xyz[1] + xyz[2];
        if (!std::isfinite(sum) || std::abs(sum) < 1.0e-12)
            return {};
        out[i] = { { round_chromaticity_coord(xyz[0] / sum),
                     round_chromaticity_coord(xyz[1] / sum) } };
    }
    // An equal-energy white rounds to x == y; normalize to exact (1/3, 1/3).
    if (out[3][0] == out[3][1])
        out[3] = { { 1.0 / 3.0, 1.0 / 3.0 } };
    return out;
}

}  // namespace pvt

OIIO_NAMESPACE_END
