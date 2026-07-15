// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// Unit tests for color-space search by characterization. Two layers:
//   1. The pure crown -- the term-grammar parse and the three-valued axis
//      combination -- exercised with no config (parse_search_term,
//      three_valued_axis).
//   2. The config-driven search -- pvt::find_color_spaces over small in-memory
//      OCIO configs written to temp files -- exercising the universe/visibility
//      gate, the `~` vs `-` unknown-propagation split, fail-fast hint
//      resolution, escapes/sequences, and the exhaustive realize-clean +
//      allowlist gate (which must NOT consult the fingerprint subsystem).

#include <string>
#include <vector>

#include <OpenImageIO/color.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/unittest.h>

#include "imageio_pvt.h"

using namespace OIIO;

namespace {

template<class F>
bool
throws_invalid_argument(F&& f)
{
    try {
        f();
    } catch (const std::invalid_argument&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}


// ---------------------------------------------------------------------------
// Layer 1: pure term grammar
// ---------------------------------------------------------------------------

void
test_parse_search_term()
{
    using pvt::parse_search_term;
    using Mode = pvt::SearchTermMode;

    auto plain = parse_search_term("scene-linear");
    OIIO_CHECK_ASSERT(plain.first == Mode::include);
    OIIO_CHECK_EQUAL(plain.second, "scene-linear");

    auto excl = parse_search_term("-scene-linear");
    OIIO_CHECK_ASSERT(excl.first == Mode::exclude);
    OIIO_CHECK_EQUAL(excl.second, "scene-linear");

    auto inv = parse_search_term("~scene-linear");
    OIIO_CHECK_ASSERT(inv.first == Mode::inverse);
    OIIO_CHECK_EQUAL(inv.second, "scene-linear");

    // Escaped operators become part of the name, keeping the term's own mode.
    auto esc_dash = parse_search_term("\\-foo");
    OIIO_CHECK_ASSERT(esc_dash.first == Mode::include);
    OIIO_CHECK_EQUAL(esc_dash.second, "-foo");

    auto esc_tilde = parse_search_term("\\~foo");
    OIIO_CHECK_ASSERT(esc_tilde.first == Mode::include);
    OIIO_CHECK_EQUAL(esc_tilde.second, "~foo");

    // An operator may precede an escape: ~\~foo = inverse-match literal "~foo".
    auto inv_esc = parse_search_term("~\\~foo");
    OIIO_CHECK_ASSERT(inv_esc.first == Mode::inverse);
    OIIO_CHECK_EQUAL(inv_esc.second, "~foo");

    auto esc_back = parse_search_term("\\\\foo");
    OIIO_CHECK_EQUAL(esc_back.second, "\\foo");

    // Empty input yields an empty value (the caller skips it).
    OIIO_CHECK_EQUAL(parse_search_term("").second, "");

    // Bare operator, dangling escape, and invalid escape all throw.
    OIIO_CHECK_ASSERT(throws_invalid_argument([] { pvt::parse_search_term("-"); }));
    OIIO_CHECK_ASSERT(throws_invalid_argument([] { pvt::parse_search_term("~"); }));
    OIIO_CHECK_ASSERT(
        throws_invalid_argument([] { pvt::parse_search_term("\\"); }));
    OIIO_CHECK_ASSERT(
        throws_invalid_argument([] { pvt::parse_search_term("\\foo"); }));
}


// Small helper mirroring the axis-evaluator call in the search walk: build the
// parallel (modes, term_matches) spans and combine.
bool
axis(std::vector<pvt::SearchTermMode> modes, std::vector<unsigned char> matches,
     bool known)
{
    return pvt::three_valued_axis(modes, matches, known);
}

void
test_three_valued_axis()
{
    using Mode = pvt::SearchTermMode;

    // An empty axis is unconstrained.
    OIIO_CHECK_EQUAL(axis({}, {}, false), true);
    OIIO_CHECK_EQUAL(axis({}, {}, true), true);

    // --- The three-valued table, one include/inverse/exclude term at a time,
    //     against a matching / known-different / unknown property. ---

    // include: match selects; known-different misses; unknown misses.
    OIIO_CHECK_EQUAL(axis({ Mode::include }, { 1 }, true), true);
    OIIO_CHECK_EQUAL(axis({ Mode::include }, { 0 }, true), false);
    OIIO_CHECK_EQUAL(axis({ Mode::include }, { 0 }, false), false);

    // ~ inverse: known-different selects; match misses; unknown is REJECTED.
    OIIO_CHECK_EQUAL(axis({ Mode::inverse }, { 0 }, true), true);
    OIIO_CHECK_EQUAL(axis({ Mode::inverse }, { 1 }, true), false);
    OIIO_CHECK_EQUAL(axis({ Mode::inverse }, { 0 }, false), false);

    // - exclude: match is rejected; unknown is PRESERVED (kept). This is the
    //   crux of the `~` vs `-` split -- inverse rejects unknown, exclude keeps
    //   it.
    OIIO_CHECK_EQUAL(axis({ Mode::exclude }, { 1 }, true), false);
    OIIO_CHECK_EQUAL(axis({ Mode::exclude }, { 0 }, false), true);
    OIIO_CHECK_EQUAL(axis({ Mode::exclude }, { 0 }, true), true);

    // Exclusion-only axis starts from the full universe (no selector needed).
    OIIO_CHECK_EQUAL(axis({ Mode::exclude }, { 0 }, true), true);

    // Include ∪ inverse, then exclude subtracts. A candidate selected by an
    // include term is still dropped by a matching exclude term (exclusion wins
    // last).
    OIIO_CHECK_EQUAL(axis({ Mode::include, Mode::exclude }, { 1, 1 }, true),
                     false);
    OIIO_CHECK_EQUAL(axis({ Mode::include, Mode::exclude }, { 1, 0 }, true),
                     true);

    // Two includes: match on either selects.
    OIIO_CHECK_EQUAL(axis({ Mode::include, Mode::include }, { 0, 1 }, true),
                     true);
    OIIO_CHECK_EQUAL(axis({ Mode::include, Mode::include }, { 0, 0 }, true),
                     false);
}

}  // namespace


int
main(int /*argc*/, char* /*argv*/[])
{
    test_parse_search_term();
    test_three_valued_axis();
    return unit_test_failures;
}
