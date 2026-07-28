// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// Transfer-signature axis for color-space search by characterization. A
// candidate's transfer property is the triple { identity, family, signature };
// a hint is matched against it in a fixed identity -> family -> signature
// order. The signature is behavioral: a fixed set of neutral-axis probe values
// pushed through the color space in the encode direction, reduced to adjacent
// slopes normalized by the mid-grey (0.18->0.50) anchor slope, so two spaces
// carrying the same curve through different gamut matrices compare equal.
//
// Everything here is pure and config-free: the caller runs the CPU processor
// over tf_probe_axis() and hands the outputs in, so these primitives unit-test
// without a live config. The probe set and tolerances are empirical, ported
// verbatim from the proven reference implementation.

#include "color_pvt.h"
#include "imageio_pvt.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

OIIO_NAMESPACE_BEGIN

namespace pvt {

namespace {

    // Neutral-axis probe values. Each is applied as R=G=B and the slope between
    // adjacent outputs identifies the curve shape; the values sit at the
    // characteristic inflection points of the known log and gamma curves:
    //
    //  -0.005  negative clip / passthrough / mirror detection
    //   0.0    black point
    //   0.002  camera-log toe (separates log curves from gamma)
    //   0.005  sRGB linear-toe break (~0.0031)
    //   0.01   D-Log vs LogC3 linear-break divergence
    //   0.05   low midtone
    //   0.18   mid-grey -- slope normalization anchor
    //   0.50   highlight
    //   1.0    reference white / SDR clip boundary
    //   1.1    superwhite -- SDR clipping vs log/HDR headroom
    constexpr double kTFProbes[] = { -0.005, 0.0,  0.002, 0.005, 0.01,
                                     0.05,   0.18, 0.5,   1.0,   1.1 };
    constexpr int kTFProbeCount  = int(std::size(kTFProbes));
    constexpr int kTFAnchorIndex = 6;  // 0.18: slope normalization anchor
    constexpr int kTFWhiteIndex  = 8;  // 1.0: white-gain tie-break probe

    // Scaled (dark, bright) linearity pair, 64x apart, mirroring OCIO's
    // isColorSpaceLinear probe: | f(64x) - 64 f(x) | <= max(abs, rel * |f(64x)|).
    // The absolute term matches OCIO's probe; the relative term absorbs inverse
    // LUT1D evaluation error for spaces authored to-reference-only.
    constexpr double kTFLinearityDark   = 0.0625;
    constexpr double kTFLinearityBright = 4.0;
    constexpr double kTFLinearityRatio  = 64.0;  // 4.0 / 0.0625
    constexpr double kTFLinearityAbsTol = 1e-5;
    constexpr double kTFLinearityRelTol = 1e-4;

    // Minimum fraction of within-tolerance slopes required to report a match.
    constexpr double kTFMinMatchScore = 0.8;

    // White-gain tie-break tolerance (relative): normalized slopes discard
    // constant gain, so a headroom-scaled curve (e.g. DCI-scaled gamma 2.6) would
    // otherwise be indistinguishable from its unscaled twin.
    constexpr double kWhiteGainTolerance = 0.01;


    // Compare two normalized slope profiles under the per-encoding tolerance:
    // index 0 (the negative-clip slope) is always masked; the last index is
    // masked when either side clips superwhite; pass when at least
    // kTFMinMatchScore of the compared slopes agree.
    bool slope_profiles_match(cspan<double> a, cspan<double> avalues,
                              cspan<double> b, cspan<double> bvalues,
                              string_view encoding)
    {
        const int n = int(std::min(a.size(), b.size()));
        if (n == 0)
            return false;
        const double tol     = tf_slope_tolerance(encoding);
        const bool clipsuper = tf_clips_superwhite(avalues)
                               || tf_clips_superwhite(bvalues);
        int masked = 0;
        int within = 0;
        for (int i = 0; i < n; ++i) {
            if (i == 0 || (i == n - 1 && clipsuper)) {
                ++masked;
                continue;
            }
            if (std::abs(a[i] - b[i]) <= tol)
                ++within;
        }
        const int compared = n - masked;
        return compared > 0
               && double(within) / double(compared) >= kTFMinMatchScore;
    }

}  // namespace


cspan<double>
tf_probe_axis()
{
    // 10 discriminating probes + the (dark, bright) scaled-linearity pair,
    // built once from the constants so there is a single source of truth.
    static const std::array<double, kTFProbeCount + 2> axis = [] {
        std::array<double, kTFProbeCount + 2> a {};
        std::copy(std::begin(kTFProbes), std::end(kTFProbes), a.begin());
        a[kTFProbeCount]     = kTFLinearityDark;
        a[kTFProbeCount + 1] = kTFLinearityBright;
        return a;
    }();
    return axis;
}


double
tf_slope_tolerance(string_view encoding)
{
    if (encoding == "log")
        return 0.05;
    if (encoding == "hdr-video")
        return 0.1;
    return 0.02;  // sdr-video and default
}


bool
tf_clips_superwhite(cspan<double> values)
{
    return values.size() >= 2
           && std::abs(values.back() - values[values.size() - 2]) < 1e-6;
}


std::vector<double>
tf_normalized_slopes(cspan<double> values)
{
    if (values.size() != size_t(kTFProbeCount))
        return {};
    std::vector<double> slopes(kTFProbeCount - 1);
    for (int i = 0; i < kTFProbeCount - 1; ++i)
        slopes[i] = (values[i + 1] - values[i])
                    / (kTFProbes[i + 1] - kTFProbes[i]);
    const double ref = slopes[kTFAnchorIndex];
    if (std::abs(ref) < 1e-12)
        return {};
    for (double& s : slopes)
        s /= ref;
    return slopes;
}


std::optional<TransferFunctionSignature>
tf_signature_from_probes(cspan<double> probe_outputs)
{
    if (probe_outputs.size() != size_t(kTFProbeCount) + 2)
        return {};
    std::vector<double> values(probe_outputs.begin(),
                               probe_outputs.begin() + kTFProbeCount);
    auto slopes = tf_normalized_slopes(values);
    if (slopes.empty())
        return {};

    TransferFunctionSignature sig;
    sig.slopes          = std::move(slopes);
    const double dark   = probe_outputs[kTFProbeCount];
    const double bright = probe_outputs[kTFProbeCount + 1];
    sig.is_linear       = std::abs(bright - kTFLinearityRatio * dark)
                    <= std::max(kTFLinearityAbsTol,
                                kTFLinearityRelTol * std::abs(bright));
    sig.values = std::move(values);
    return sig;
}


bool
transfer_signatures_match(const TransferFunctionSignature& a,
                          const TransferFunctionSignature& b)
{
    // Encoding (slope tolerance) comes from whichever side declares one.
    const std::string& encoding = !a.encoding.empty() ? a.encoding : b.encoding;
    if (!slope_profiles_match(a.slopes, a.values, b.slopes, b.values, encoding))
        return false;
    if (a.values.size() <= size_t(kTFWhiteIndex)
        || b.values.size() <= size_t(kTFWhiteIndex))
        return true;
    const double aw    = a.values[kTFWhiteIndex];
    const double bw    = b.values[kTFWhiteIndex];
    const double scale = std::max({ std::abs(aw), std::abs(bw), 1e-12 });
    return std::abs(aw - bw) / scale <= kWhiteGainTolerance;
}


bool
transfer_hint_matches(const TransferHint& hint,
                      const TransferProperty& property)
{
    if (hint.identity && property.identity)
        return true;
    // Both families identified: family equality short-circuits (behavior
    // families beat signature comparison).
    if (!hint.family.empty() && !property.family.empty())
        return hint.family == property.family;
    if (!property.signature)
        return false;
    return std::any_of(hint.signatures.begin(), hint.signatures.end(),
                       [&](const TransferFunctionSignature& signature) {
                           return transfer_signatures_match(signature,
                                                            *property.signature);
                       });
}

}  // namespace pvt

OIIO_NAMESPACE_END
