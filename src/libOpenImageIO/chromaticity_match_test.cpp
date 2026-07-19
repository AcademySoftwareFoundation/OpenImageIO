// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// Unit tests for the pvt:: pure chromaticity math (reserved-primaries table,
// AP0->XYZ(D65) probe solve, coordinate rounding/snapping). Pure functions --
// no config or OCIO needed.

#include <array>
#include <optional>
#include <vector>

#include "color_pvt.h"

#include <OpenImageIO/unittest.h>

#include "imageio_pvt.h"

using namespace OIIO;


static void
test_round_chromaticity_coord()
{
    // The single fuzz-absorbing step: round to 6 decimals, then snap to a
    // coarser grid within 2e-7. 0.329999998 -> 0.330000 (6) -> snaps to 0.33.
    OIIO_CHECK_EQUAL(pvt::round_chromaticity_coord(0.329999998), 0.33);
    OIIO_CHECK_EQUAL(pvt::round_chromaticity_coord(0.3127), 0.3127);
    OIIO_CHECK_EQUAL(pvt::round_chromaticity_coord(0.150000012), 0.15);
    // No coarser grid within tol -> the 6-decimal rounding stands.
    OIIO_CHECK_EQUAL(pvt::round_chromaticity_coord(0.734789), 0.734789);
    // Exactly representable coarse values are unchanged.
    OIIO_CHECK_EQUAL(pvt::round_chromaticity_coord(0.06), 0.06);
}


static void
test_reserved_chromaticities_for_id()
{
    // Probed as "_<token>_" against the lowered id: a full interop id
    // resolves, but a bare or partial fragment does not.
    auto ap1 = pvt::reserved_chromaticities_for_id("lin_ap1_scene");
    OIIO_CHECK_ASSERT(ap1.has_value());
    OIIO_CHECK_EQUAL((*ap1)[0][0], 0.713);
    OIIO_CHECK_EQUAL((*ap1)[3][1], 0.33767);  // ACES white y

    auto rec709 = pvt::reserved_chromaticities_for_id("srgb_rec709_display");
    OIIO_CHECK_ASSERT(rec709.has_value());
    OIIO_CHECK_EQUAL((*rec709)[1][1], 0.60);  // green y

    // A bare component fragment is not a full id -> no "_token_" match. (The
    // search axis resolves bare "ap1" through a separate component path.)
    OIIO_CHECK_ASSERT(!pvt::reserved_chromaticities_for_id("ap1").has_value());
    OIIO_CHECK_ASSERT(!pvt::reserved_chromaticities_for_id("p3").has_value());
    OIIO_CHECK_ASSERT(!pvt::reserved_chromaticities_for_id("").has_value());

    // AdobeRGB matches only exactly / via _adobergb_, never through the
    // substring loop -- and a rec709 id must not accidentally hit it.
    auto adobe = pvt::reserved_chromaticities_for_id("adobergb");
    OIIO_CHECK_ASSERT(adobe.has_value());
    OIIO_CHECK_EQUAL((*adobe)[1][0], 0.21);  // green x, adobergb-specific
    OIIO_CHECK_ASSERT(
        pvt::reserved_chromaticities_for_id("g22_adobergb_display").has_value());
    auto plain709 = pvt::reserved_chromaticities_for_id("lin_rec709_scene");
    OIIO_CHECK_ASSERT(plain709.has_value());
    OIIO_CHECK_EQUAL((*plain709)[1][0], 0.30);  // rec709 green x, not adobergb's

    // Case-insensitive.
    OIIO_CHECK_ASSERT(
        pvt::reserved_chromaticities_for_id("LIN_AP1_SCENE").has_value());
}


static void
test_chromaticities_from_ap0_probes()
{
    // AP0 white probe (1,1,1) through the D65-adapted matrix lands on D65
    // (0.3127, 0.329) after rounding; R/G/B probes carry the other three.
    const std::vector<float> unit { 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1 };
    auto c = pvt::chromaticities_from_ap0_probes(unit);
    OIIO_CHECK_ASSERT(c.has_value());
    OIIO_CHECK_EQUAL((*c)[3][0], 0.3127);  // white x
    OIIO_CHECK_EQUAL((*c)[3][1], 0.329);   // white y
    OIIO_CHECK_EQUAL_THRESH((*c)[0][0], 0.734855, 1e-6);  // AP0 red x (D65-adapted)

    // Illuminant-E white probe (equal XYZ) rounds to x == y and is snapped to
    // exact (1/3, 1/3).
    const std::vector<float> eq {
        1, 0, 0, 0, 1, 0, 0, 0, 1,
        1.0540976150737138f, 0.9674863678639096f, 0.9182462840112028f
    };
    auto e = pvt::chromaticities_from_ap0_probes(eq);
    OIIO_CHECK_ASSERT(e.has_value());
    OIIO_CHECK_EQUAL((*e)[3][0], 1.0 / 3.0);
    OIIO_CHECK_EQUAL((*e)[3][1], 1.0 / 3.0);

    // Wrong span length and a degenerate (all-zero) probe both decline.
    const std::vector<float> shortspan { 1, 0, 0 };
    OIIO_CHECK_ASSERT(!pvt::chromaticities_from_ap0_probes(shortspan).has_value());
    const std::vector<float> zero(12, 0.0f);
    OIIO_CHECK_ASSERT(!pvt::chromaticities_from_ap0_probes(zero).has_value());
}


int
main(int /*argc*/, char* /*argv*/[])
{
    test_round_chromaticity_coord();
    test_reserved_chromaticities_for_id();
    test_chromaticities_from_ap0_probes();
    return unit_test_failures;
}
