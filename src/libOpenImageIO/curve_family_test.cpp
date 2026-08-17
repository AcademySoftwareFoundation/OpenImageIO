// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// Unit tests for the pvt:: curve-family normalization helpers
// (family_token / family_name + pass-through/mirror discrimination).
// Pure functions -- no config or OCIO needed.

#include <string>
#include <vector>

#include "color_pvt.h"

#include <OpenImageIO/unittest.h>

#include "imageio_pvt.h"

using namespace OIIO;


static void
test_family_token()
{
    // Current (_tx / bare mirror) and legacy (_scene / _display) spellings of
    // one family all reduce to the same token.
    OIIO_CHECK_EQUAL(pvt::family_token("crv_g24_tx"), "g24");
    OIIO_CHECK_EQUAL(pvt::family_token("crv_g24"), "g24");
    OIIO_CHECK_EQUAL(pvt::family_token("crv_g24_scene"), "g24");
    OIIO_CHECK_EQUAL(pvt::family_token("crv_g24_display"), "g24");
    OIIO_CHECK_EQUAL(pvt::family_token("crv_srgb_tx"), "srgb");
    OIIO_CHECK_EQUAL(pvt::family_token("crv_srgb_display"), "srgb");
    OIIO_CHECK_EQUAL(pvt::family_token("crv_arrilogc3"), "arrilogc3");
    OIIO_CHECK_EQUAL(pvt::family_token("crv_dcdm_display"), "dcdm");
    OIIO_CHECK_EQUAL(pvt::family_token("crv_lin"), "lin");

    // Degenerate: a bare suffix is strictly-not-longer, so it passes through;
    // empty maps to empty.
    OIIO_CHECK_EQUAL(pvt::family_token("_tx"), "_tx");
    OIIO_CHECK_EQUAL(pvt::family_token(""), "");

    // A name without the crv_ prefix still normalizes its suffix.
    OIIO_CHECK_EQUAL(pvt::family_token("g24_tx"), "g24");
}


static void
test_family_name()
{
    // Same suffix strip, but the crv_ prefix is preserved -- for comparing two
    // matched catalog names for family equality.
    OIIO_CHECK_EQUAL(pvt::family_name("crv_g24_tx"), "crv_g24");
    OIIO_CHECK_EQUAL(pvt::family_name("crv_g24"), "crv_g24");
    OIIO_CHECK_EQUAL(pvt::family_name("crv_g24_scene"), "crv_g24");
    OIIO_CHECK_EQUAL(pvt::family_name("crv_g24_display"), "crv_g24");
    OIIO_CHECK_EQUAL(pvt::family_name("crv_srgb_tx"), "crv_srgb");
    OIIO_CHECK_EQUAL(pvt::family_name("crv_srgb_display"), "crv_srgb");
    OIIO_CHECK_EQUAL(pvt::family_name("crv_arrilogc3"), "crv_arrilogc3");
    OIIO_CHECK_EQUAL(pvt::family_name("crv_dcdm_display"), "crv_dcdm");
    OIIO_CHECK_EQUAL(pvt::family_name("crv_lin"), "crv_lin");
    OIIO_CHECK_EQUAL(pvt::family_name("_tx"), "_tx");
    OIIO_CHECK_EQUAL(pvt::family_name(""), "");

    // family_token(x) == family_name(x) with the crv_ stripped.
    OIIO_CHECK_EQUAL("crv_" + pvt::family_token("crv_srgb_tx"),
                     pvt::family_name("crv_srgb_tx"));
}


static void
test_passthrough_mirror()
{
    // A catalog carrying paired families (both twins), pass-through-only, and
    // state-neutral curves.
    const std::vector<std::string> catalog {
        "crv_g24_tx",    "crv_g24",   // paired: _tx + suffixless mirror
        "crv_srgb_tx",   "crv_srgb",  // paired
        "crv_g18_tx",                 // pass-through only, no twin
        "crv_pq",        "crv_dcdm",  // state-neutral: no _tx companion
        "crv_arrilogc3",              // state-neutral (log)
    };

    // Pass-through: _tx suffix (and legacy _scene).
    OIIO_CHECK_EQUAL(pvt::curve_is_passthrough("crv_g24_tx"), true);
    OIIO_CHECK_EQUAL(pvt::curve_is_passthrough("crv_srgb_tx"), true);
    OIIO_CHECK_EQUAL(pvt::curve_is_passthrough("crv_g18_tx"), true);
    OIIO_CHECK_EQUAL(pvt::curve_is_passthrough("crv_g24_scene"), true);
    OIIO_CHECK_EQUAL(pvt::curve_is_passthrough("crv_g24"), false);

    // Mirror: suffixless twin found via companion `_tx` lookup, or legacy
    // `_display` suffix (no catalog needed for the suffix path).
    OIIO_CHECK_EQUAL(pvt::curve_is_mirror("crv_g24", catalog), true);
    OIIO_CHECK_EQUAL(pvt::curve_is_mirror("crv_srgb", catalog), true);
    OIIO_CHECK_EQUAL(pvt::curve_is_mirror("crv_g24_display", {}), true);
    OIIO_CHECK_EQUAL(pvt::curve_is_mirror("crv_g24_tx", catalog), false);

    // State-neutral: bare curves with no `_tx` companion are neither variant
    // (accepted-as-is behavior; visible to both states).
    OIIO_CHECK_EQUAL(pvt::curve_is_passthrough("crv_pq"), false);
    OIIO_CHECK_EQUAL(pvt::curve_is_mirror("crv_pq", catalog), false);
    OIIO_CHECK_EQUAL(pvt::curve_is_passthrough("crv_dcdm"), false);
    OIIO_CHECK_EQUAL(pvt::curve_is_mirror("crv_dcdm", catalog), false);
    OIIO_CHECK_EQUAL(pvt::curve_is_passthrough("crv_arrilogc3"), false);
    OIIO_CHECK_EQUAL(pvt::curve_is_mirror("crv_arrilogc3", catalog), false);

    // Degenerate bare suffixes classify as neither (strictly-longer guard).
    OIIO_CHECK_EQUAL(pvt::curve_is_passthrough("_tx"), false);
    OIIO_CHECK_EQUAL(pvt::curve_is_mirror("_display", {}), false);
}


int
main(int /*argc*/, char* /*argv*/[])
{
    test_family_token();
    test_family_name();
    test_passthrough_mirror();
    return unit_test_failures;
}
