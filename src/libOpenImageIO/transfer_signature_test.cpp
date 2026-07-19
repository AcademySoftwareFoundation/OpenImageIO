// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// Unit tests for the pvt:: transfer-signature axis: the { identity, family,
// signature } transfer property, the identity -> family -> signature match
// order, and the pure numerical primitives (normalized slopes, per-encoding
// slope tolerance, white-gain tie-break). All config-free -- signatures are
// built directly from synthetic probe data, no OCIO needed.

#include <optional>
#include <string>
#include <vector>

#include "color_pvt.h"

#include <OpenImageIO/unittest.h>

#include "imageio_pvt.h"

using namespace OIIO;
using pvt::TransferFunctionSignature;
using pvt::TransferHint;
using pvt::TransferProperty;


// A synthetic "reference" signature: a plausible gamma-ish normalized slope
// profile (index 0 is the always-masked negative-clip slope) with a full
// 10-element value run that does not clip superwhite (values[8] != values[9]).
static TransferFunctionSignature
reference_sig(const std::string& encoding = "sdr-video")
{
    TransferFunctionSignature s;
    s.slopes   = { 5.0, 2.0, 1.5, 1.2, 1.0, 0.9, 0.8, 0.7, 0.6 };
    s.values   = { 0.0, 0.0, 0.01, 0.02, 0.05, 0.1, 0.18, 0.35, 0.7, 0.75 };
    s.encoding = encoding;
    return s;
}


static void
test_slope_tolerance()
{
    OIIO_CHECK_EQUAL(pvt::tf_slope_tolerance("log"), 0.05);
    OIIO_CHECK_EQUAL(pvt::tf_slope_tolerance("hdr-video"), 0.1);
    OIIO_CHECK_EQUAL(pvt::tf_slope_tolerance("sdr-video"), 0.02);
    OIIO_CHECK_EQUAL(pvt::tf_slope_tolerance(""), 0.02);       // default
    OIIO_CHECK_EQUAL(pvt::tf_slope_tolerance("whatever"), 0.02);
}


static void
test_clips_superwhite()
{
    OIIO_CHECK_EQUAL(pvt::tf_clips_superwhite({ 0.1, 0.5, 0.5 }), true);
    OIIO_CHECK_EQUAL(pvt::tf_clips_superwhite({ 0.1, 0.5, 0.6 }), false);
    OIIO_CHECK_EQUAL(pvt::tf_clips_superwhite({ 0.5 }), false);  // too short
}


static void
test_probe_axis()
{
    cspan<double> axis = pvt::tf_probe_axis();
    // 10 discriminating probes + (dark, bright) linearity pair.
    OIIO_CHECK_EQUAL(axis.size(), 12);
    OIIO_CHECK_EQUAL(axis[0], -0.005);
    OIIO_CHECK_EQUAL(axis[6], 0.18);   // mid-grey anchor
    OIIO_CHECK_EQUAL(axis[8], 1.0);    // reference white
    OIIO_CHECK_EQUAL(axis[9], 1.1);    // superwhite
    OIIO_CHECK_EQUAL(axis[10], 0.0625);  // linearity dark
    OIIO_CHECK_EQUAL(axis[11], 4.0);     // linearity bright
}


static void
test_normalized_slopes()
{
    // A linear ramp f(x) = x: every adjacent slope is 1, and normalizing by
    // the anchor slope (also 1) leaves all slopes at 1.
    cspan<double> axis = pvt::tf_probe_axis();
    std::vector<double> linear(axis.begin(), axis.begin() + 10);
    auto slopes = pvt::tf_normalized_slopes(linear);
    OIIO_CHECK_EQUAL(slopes.size(), 9);
    for (double s : slopes)
        OIIO_CHECK_EQUAL_THRESH(s, 1.0, 1e-9);

    // Flat curve -> degenerate anchor slope -> empty.
    OIIO_CHECK_EQUAL(
        pvt::tf_normalized_slopes(std::vector<double>(10, 0.5)).empty(), true);

    // Not a full 10-probe run -> empty.
    OIIO_CHECK_EQUAL(
        pvt::tf_normalized_slopes(std::vector<double>(9, 0.1)).empty(), true);
}


static void
test_signature_from_probes()
{
    cspan<double> axis = pvt::tf_probe_axis();

    // Linear ramp outputs (identity): bright == 64 * dark, so is_linear.
    std::vector<double> linear(axis.begin(), axis.end());  // 12 outputs
    auto lin = pvt::tf_signature_from_probes(linear);
    OIIO_CHECK_ASSERT(lin.has_value());
    OIIO_CHECK_EQUAL(lin->values.size(), 10);   // only the discriminating run
    OIIO_CHECK_EQUAL(lin->slopes.size(), 9);
    OIIO_CHECK_EQUAL(lin->is_linear, true);

    // Same slope run but a non-linear bright output -> is_linear false.
    std::vector<double> nonlin = linear;
    nonlin[11] = 3.0;  // bright, breaks the 64x ratio
    auto nl = pvt::tf_signature_from_probes(nonlin);
    OIIO_CHECK_ASSERT(nl.has_value());
    OIIO_CHECK_EQUAL(nl->is_linear, false);

    // Wrong-length span -> nullopt.
    OIIO_CHECK_EQUAL(
        pvt::tf_signature_from_probes(std::vector<double>(11, 0.1)).has_value(),
        false);
}


static void
test_signatures_match()
{
    const auto ref = reference_sig();

    // Identical signatures match.
    OIIO_CHECK_EQUAL(pvt::transfer_signatures_match(ref, ref), true);

    // A profile differing at four of the eight compared slopes (index 0 is
    // masked) drops the score to 0.5 < 0.8 -> no match.
    auto diff   = ref;
    diff.slopes = { 5.0, 2.5, 2.0, 1.7, 1.5, 0.9, 0.8, 0.7, 0.6 };
    OIIO_CHECK_EQUAL(pvt::transfer_signatures_match(ref, diff), false);

    // Encoding selects the tolerance. A profile off by 0.03 at two indices
    // fails under sdr-video (tol 0.02, 6/8 = 0.75) but passes under log
    // (tol 0.05, 8/8). Encoding is taken from the first side that declares one.
    auto near     = ref;
    near.slopes   = { 5.0, 2.03, 1.53, 1.2, 1.0, 0.9, 0.8, 0.7, 0.6 };
    near.encoding = "";  // let the ref side pick the tolerance
    OIIO_CHECK_EQUAL(pvt::transfer_signatures_match(reference_sig("sdr-video"),
                                                    near),
                     false);
    OIIO_CHECK_EQUAL(pvt::transfer_signatures_match(reference_sig("log"), near),
                     true);

    // White-gain tie-break: identical slopes but a headroom-scaled white
    // (0.7 -> 1.05) is rejected even though the slope profiles agree.
    auto scaled      = ref;
    scaled.values[8] = 1.05;  // white probe
    scaled.values[9] = 1.1;   // keep superwhite distinct (no clip mask)
    OIIO_CHECK_EQUAL(pvt::transfer_signatures_match(ref, scaled), false);
}


// The heart of the slice: the identity -> family -> signature match order.
static void
test_match_order()
{
    const auto sig = reference_sig();
    auto sig_diff  = sig;
    sig_diff.slopes = { 5.0, 2.5, 2.0, 1.7, 1.5, 0.9, 0.8, 0.7, 0.6 };

    TransferProperty prop_identity;  // linear/identity space
    prop_identity.identity = true;

    TransferProperty prop_family_g24;
    prop_family_g24.family    = "g24";
    prop_family_g24.signature = sig;

    TransferProperty prop_family_g26;
    prop_family_g26.family    = "g26";
    prop_family_g26.signature = sig;

    TransferProperty prop_sig_only;  // family unidentified, signature probed
    prop_sig_only.signature = sig;

    TransferProperty prop_unknown;  // nothing derivable
    OIIO_CHECK_EQUAL(prop_unknown.known(), false);
    OIIO_CHECK_EQUAL(prop_identity.known(), true);
    OIIO_CHECK_EQUAL(prop_sig_only.known(), true);

    // --- identity wins first ---
    TransferHint hint_identity;
    hint_identity.identity = true;
    OIIO_CHECK_EQUAL(pvt::transfer_hint_matches(hint_identity, prop_identity),
                     true);
    // An identity hint does not match a non-identity space...
    OIIO_CHECK_EQUAL(pvt::transfer_hint_matches(hint_identity, prop_family_g24),
                     false);
    OIIO_CHECK_EQUAL(pvt::transfer_hint_matches(hint_identity, prop_unknown),
                     false);

    // --- family short-circuits when both families are known ---
    TransferHint hint_g24;
    hint_g24.family     = "g24";
    hint_g24.signatures = { sig_diff };  // would NOT match by signature
    // Same family -> match, even though the carried signature differs.
    OIIO_CHECK_EQUAL(pvt::transfer_hint_matches(hint_g24, prop_family_g24),
                     true);
    // Different family -> reject, even though the property's signature would
    // match the hint if we fell through (behavior families beat signature).
    TransferHint hint_g24_matchsig;
    hint_g24_matchsig.family     = "g24";
    hint_g24_matchsig.signatures = { sig };
    OIIO_CHECK_EQUAL(
        pvt::transfer_hint_matches(hint_g24_matchsig, prop_family_g26), false);

    // --- signature fallback when a family is unknown on either side ---
    // Hint carries family "g24" but candidate has no family -> compare signatures.
    OIIO_CHECK_EQUAL(pvt::transfer_hint_matches(hint_g24, prop_sig_only),
                     false);  // sig_diff mismatches
    OIIO_CHECK_EQUAL(
        pvt::transfer_hint_matches(hint_g24_matchsig, prop_sig_only), true);

    // Pure signature hint (no family): matches any candidate whose signature
    // agrees, family known or not.
    TransferHint hint_sig;
    hint_sig.signatures = { sig };
    OIIO_CHECK_EQUAL(pvt::transfer_hint_matches(hint_sig, prop_sig_only), true);
    OIIO_CHECK_EQUAL(pvt::transfer_hint_matches(hint_sig, prop_family_g24),
                     true);
    // An unknown candidate property never matches.
    OIIO_CHECK_EQUAL(pvt::transfer_hint_matches(hint_sig, prop_unknown), false);
}


int
main(int /*argc*/, char* /*argv*/[])
{
    test_slope_tolerance();
    test_clips_superwhite();
    test_probe_axis();
    test_normalized_slopes();
    test_signature_from_probes();
    test_signatures_match();
    test_match_order();
    return unit_test_failures;
}
