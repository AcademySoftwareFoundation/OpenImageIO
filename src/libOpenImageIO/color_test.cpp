// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <unordered_set>
#include <vector>

#include "color_pvt.h"
#include <OpenImageIO/argparse.h>
#include <OpenImageIO/benchmark.h>
#include <OpenImageIO/color.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/simd.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/timer.h>
#include <OpenImageIO/typedesc.h>

#include <OpenImageIO/unittest.h>

#include "imageio_pvt.h"


using namespace OIIO;
using namespace simd;


// Aid for things that are too short to benchmark accurately
#define REP10(x) x, x, x, x, x, x, x, x, x, x

static int iterations   = 1000000;
static int ntrials      = 5;
static bool verbose     = false;
static bool bench_mode  = false;
static bool bench_child = false;



static void
getargs(int argc, char* argv[])
{
    ArgParse ap;
    // clang-format off
    ap.intro("color_test\n" OIIO_INTRO_STRING)
      .usage("color_test [options]");

    ap.arg("-v", &verbose)
      .help("Verbose mode");
    ap.arg("--iters %d", &iterations)
      .help(Strutil::fmt::format("Number of iterations (default: {})", iterations));
    ap.arg("--trials %d", &ntrials)
      .help("Number of trials");
    ap.arg("--bench", &bench_mode)
      .help("Run the interop engine's cold/warm benchmark phases (timings + "
            "cardinality counts; not a pass/fail gate)");
    ap.arg("--bench-child-construct", &bench_child)
      .help("Internal: used by --bench to measure ColorConfig construction "
            "in a fresh subprocess; do not use directly");
    // clang-format on

    ap.parse(argc, (const char**)argv);
}



static void
test_sRGB_conversion()
{
    Benchmarker bench;

    OIIO_CHECK_EQUAL_THRESH(linear_to_sRGB(0.0f), 0.0f, 1.0e-6);
    OIIO_CHECK_EQUAL_THRESH(linear_to_sRGB(1.0f), 1.0f, 1.0e-6);
    OIIO_CHECK_EQUAL_THRESH(linear_to_sRGB(0.5f), 0.735356983052449f, 1.0e-6);

    OIIO_CHECK_EQUAL_THRESH(sRGB_to_linear(0.0f), 0.0f, 1.0e-6);
    OIIO_CHECK_EQUAL_THRESH(sRGB_to_linear(1.0f), 1.0f, 1.0e-6);
    OIIO_CHECK_EQUAL_THRESH(sRGB_to_linear(0.5f), 0.214041140482232f, 1.0e-6);

    // Check the SIMD versions, too
    OIIO_CHECK_SIMD_EQUAL_THRESH(linear_to_sRGB(vfloat4(0.0f)), vfloat4(0.0f),
                                 1.0e-5);
    OIIO_CHECK_SIMD_EQUAL_THRESH(linear_to_sRGB(vfloat4(1.0f)), vfloat4(1.0f),
                                 1.0e-5);
    OIIO_CHECK_SIMD_EQUAL_THRESH(linear_to_sRGB(vfloat4(0.5f)),
                                 vfloat4(0.735356983052449f), 1.0e-5);

    OIIO_CHECK_SIMD_EQUAL_THRESH(sRGB_to_linear(vfloat4(0.0f)), vfloat4(0.0f),
                                 1.0e-5);
    OIIO_CHECK_SIMD_EQUAL_THRESH(sRGB_to_linear(vfloat4(1.0f)), vfloat4(1.0f),
                                 1.0e-5);
    OIIO_CHECK_SIMD_EQUAL_THRESH(sRGB_to_linear(vfloat4(0.5f)),
                                 vfloat4(0.214041140482232f), 1.0e-5);

    float fval = 0.5f;
    clobber(fval);
    vfloat4 vfval(fval);
    clobber(vfval);
    bench("sRGB_to_linear",
          [&]() { return DoNotOptimize(sRGB_to_linear(fval)); });
    bench("linear_to_sRGB",
          [&]() { return DoNotOptimize(sRGB_to_linear(fval)); });
    bench.work(4);
    bench("sRGB_to_linear simd",
          [&]() { return DoNotOptimize(sRGB_to_linear(vfval)); });
    bench("linear_to_sRGB simd",
          [&]() { return DoNotOptimize(sRGB_to_linear(vfval)); });
}



static void
test_Rec709_conversion()
{
    Benchmarker bench;

    OIIO_CHECK_EQUAL_THRESH(linear_to_Rec709(0.0f), 0.0f, 1.0e-6);
    OIIO_CHECK_EQUAL_THRESH(linear_to_Rec709(1.0f), 1.0f, 1.0e-6);
    OIIO_CHECK_EQUAL_THRESH(linear_to_Rec709(0.5f), 0.705515089922121f, 1.0e-6);

    OIIO_CHECK_EQUAL_THRESH(Rec709_to_linear(0.0f), 0.0f, 1.0e-6);
    OIIO_CHECK_EQUAL_THRESH(Rec709_to_linear(1.0f), 1.0f, 1.0e-6);
    OIIO_CHECK_EQUAL_THRESH(Rec709_to_linear(0.5f), 0.259589400506286f, 1.0e-6);

    float fval = 0.5f;
    clobber(fval);
    bench("Rec709_to_linear",
          [&]() { return DoNotOptimize(Rec709_to_linear(fval)); });
    bench("linear_to_Rec709",
          [&]() { return DoNotOptimize(Rec709_to_linear(fval)); });
}



static void
test_interop_identities_config()
{
    int nspaces = OIIO::pvt::interop_identities_config_size();
    OIIO_CHECK_GT(nspaces, 0);

    // With OCIO >= 2.5 (0x02050000) the config is built on OCIO's own builtin
    // studio config, so it must still resolve a studio-native identity -- that
    // is, the registry is the studio config's superset, hence its space count
    // is at least the studio baseline -- and it must also resolve OIIO's own
    // additions layered on top.
    if (ColorConfig::OpenColorIO_version_hex() >= 0x02050000) {
        OIIO_CHECK_ASSERT(
            OIIO::pvt::interop_identities_config_resolves("ACES2065-1"));
        OIIO_CHECK_ASSERT(OIIO::pvt::interop_identities_config_resolves(
            "oiio:lin_p3d60_display"));
    }
}



static void
test_interop_id_grammar()
{
    using OIIO::pvt::InteropIdForm;
    using OIIO::pvt::is_utility_interop_id;
    using OIIO::pvt::is_valid_interop_id;
    using OIIO::pvt::parse_interop_id;
    using OIIO::pvt::sanitize_id_token;
    using OIIO::pvt::strip_leftmost_namespace;

    // Validity + form, per the CIF Annex B grammar (4 legal forms; 3+
    // colons is always invalid).
    OIIO_CHECK_ASSERT(is_valid_interop_id("lin_ap0_scene"));
    OIIO_CHECK_EQUAL((int)parse_interop_id("lin_ap0_scene").form,
                     (int)InteropIdForm::BASE);

    // "local:srgb" is an ordinary INNER_BASE id at the grammar layer --
    // the grammar has zero knowledge of "local" as special; that's a
    // question one layer up (resolution code checking
    // form == OUTER_INNER_BASE && inner == "local").
    {
        auto parts = parse_interop_id("local:srgb");
        OIIO_CHECK_ASSERT(is_valid_interop_id("local:srgb"));
        OIIO_CHECK_EQUAL((int)parts.form, (int)InteropIdForm::INNER_BASE);
        OIIO_CHECK_EQUAL(parts.inner, "local");
        OIIO_CHECK_EQUAL(parts.base, "srgb");
    }

    {
        auto parts = parse_interop_id("show1-config:local:srgb");
        OIIO_CHECK_ASSERT(is_valid_interop_id("show1-config:local:srgb"));
        OIIO_CHECK_EQUAL((int)parts.form, (int)InteropIdForm::OUTER_INNER_BASE);
        OIIO_CHECK_EQUAL(parts.outer, "show1-config");
        OIIO_CHECK_EQUAL(parts.inner, "local");
        OIIO_CHECK_EQUAL(parts.base, "srgb");
    }

    {
        auto parts = parse_interop_id("my-studio::srgb");
        OIIO_CHECK_ASSERT(is_valid_interop_id("my-studio::srgb"));
        OIIO_CHECK_EQUAL((int)parts.form, (int)InteropIdForm::OUTER_BLANK_BASE);
        OIIO_CHECK_EQUAL(parts.outer, "my-studio");
        OIIO_CHECK_ASSERT(parts.inner.empty());
        OIIO_CHECK_EQUAL(parts.base, "srgb");
    }

    OIIO_CHECK_FALSE(is_valid_interop_id(""));
    OIIO_CHECK_FALSE(is_valid_interop_id(":base"));
    OIIO_CHECK_FALSE(is_valid_interop_id(":inner:base"));
    OIIO_CHECK_FALSE(is_valid_interop_id("a:b:c:d"));
    // Validation never folds case or sanitizes.
    OIIO_CHECK_FALSE(is_valid_interop_id("Lin_AP0_Scene"));
    OIIO_CHECK_FALSE(is_valid_interop_id("caf\xc3\xa9"));   // "café"
    OIIO_CHECK_FALSE(is_valid_interop_id("\xe4\xb8\xad"));  // "中"
    OIIO_CHECK_FALSE(is_valid_interop_id("outer::"));
    OIIO_CHECK_FALSE(is_valid_interop_id("outer:"));
    OIIO_CHECK_FALSE(is_valid_interop_id("lin_ap0_scene:"));

    // Sanitization (Annex C, 5-step precedence).
    OIIO_CHECK_EQUAL(sanitize_id_token("lin_ap0_scene"), "lin_ap0_scene");
    OIIO_CHECK_EQUAL(sanitize_id_token("ACEScg"), "acescg");
    OIIO_CHECK_EQUAL(sanitize_id_token("sRGB - Texture"), "srgb_-_texture");
    OIIO_CHECK_EQUAL(sanitize_id_token("a{b}c"), "a(b)c");
    OIIO_CHECK_EQUAL(sanitize_id_token("a<b>c"), "a(b)c");
    OIIO_CHECK_EQUAL(sanitize_id_token("a,b"), "a.b");
    OIIO_CHECK_EQUAL(sanitize_id_token("a;b"), "a|b");
    OIIO_CHECK_EQUAL(sanitize_id_token("a:b"), "a|b");
    OIIO_CHECK_EQUAL(sanitize_id_token("a'b\"c"), "a#b#c");
    OIIO_CHECK_EQUAL(sanitize_id_token("a\\b"), "a/b");
    OIIO_CHECK_EQUAL(sanitize_id_token("a!b=c@d"), "a*b*c*d");
    // Non-ASCII: one '^' per whole UTF-8 code point, never per byte.
    {
        std::string cafe = "caf\xc3\xa9";  // "café", 2-byte 'é'
        std::string got  = sanitize_id_token(cafe);
        OIIO_CHECK_EQUAL(got, "caf^");
        OIIO_CHECK_EQUAL(got.size(), size_t(4));
    }
    {
        std::string zhong = "\xe4\xb8\xad";  // "中", 3-byte code point
        std::string got   = sanitize_id_token(zhong);
        OIIO_CHECK_EQUAL(got, "^");
        OIIO_CHECK_EQUAL(got.size(), size_t(1));
    }
    OIIO_CHECK_EQUAL(sanitize_id_token("a\xe4\xb8\xad"
                                       "b"),
                     "a^b");

    // Namespace stripping: pure substring op, independent of validity,
    // never assumes the result is itself a valid id.
    OIIO_CHECK_EQUAL(strip_leftmost_namespace("a:b:c"), "b:c");
    OIIO_CHECK_EQUAL(strip_leftmost_namespace("a::c"), ":c");
    OIIO_CHECK_EQUAL(strip_leftmost_namespace("a"), "a");
    // Load-bearing: the blank-inner leading colon is retained, so the
    // result is NOT "srgb".
    OIIO_CHECK_EQUAL(strip_leftmost_namespace("my-studio::srgb"), ":srgb");
    OIIO_CHECK_NE(strip_leftmost_namespace("my-studio::srgb"),
                  std::string("srgb"));

    // Utility tokens: case-sensitive exact membership, no grammar
    // involvement.
    OIIO_CHECK_ASSERT(is_utility_interop_id("data"));
    OIIO_CHECK_ASSERT(is_utility_interop_id("unknown"));
    OIIO_CHECK_ASSERT(is_utility_interop_id("bypass"));
    OIIO_CHECK_FALSE(is_utility_interop_id("Data"));

    // The one marker classifier: utility tokens case-sensitive, the
    // deliberate unknown-marker family case-insensitive, everything else
    // (including empty) an ordinary definite claim.
    using OIIO::pvt::classify_interop_marker;
    using OIIO::pvt::InteropMarker;
    using OIIO::pvt::is_unknown_marker;
    OIIO_CHECK_EQUAL((int)classify_interop_marker("data"),
                     (int)InteropMarker::UtilityData);
    OIIO_CHECK_EQUAL((int)classify_interop_marker("bypass"),
                     (int)InteropMarker::UtilityBypass);
    OIIO_CHECK_EQUAL((int)classify_interop_marker("unknown"),
                     (int)InteropMarker::BareUnknown);
    OIIO_CHECK_EQUAL((int)classify_interop_marker("ocio:unknown"),
                     (int)InteropMarker::OcioUnknown);
    OIIO_CHECK_EQUAL((int)classify_interop_marker("oiio:unknown"),
                     (int)InteropMarker::OiioUnknown);
    OIIO_CHECK_EQUAL((int)classify_interop_marker("error:unknown"),
                     (int)InteropMarker::ErrorUnknown);
    OIIO_CHECK_EQUAL((int)classify_interop_marker("OIIO:Unknown"),
                     (int)InteropMarker::OiioUnknown);
    OIIO_CHECK_EQUAL((int)classify_interop_marker("Data"),
                     (int)InteropMarker::Definite);
    OIIO_CHECK_EQUAL((int)classify_interop_marker("Unknown"),
                     (int)InteropMarker::Definite);
    OIIO_CHECK_EQUAL((int)classify_interop_marker("lin_ap0_scene"),
                     (int)InteropMarker::Definite);
    OIIO_CHECK_EQUAL((int)classify_interop_marker(""),
                     (int)InteropMarker::Definite);
    OIIO_CHECK_ASSERT(is_unknown_marker("error:unknown"));
    OIIO_CHECK_ASSERT(is_unknown_marker("OCIO:UNKNOWN"));
    OIIO_CHECK_FALSE(is_unknown_marker("unknown"));
    OIIO_CHECK_FALSE(is_unknown_marker(""));
}



static void
test_registry_invariants()
{
    using OIIO::pvt::interop_identities_config_names;
    using OIIO::pvt::interop_identities_config_resolves;
    using OIIO::pvt::InteropIdForm;
    using OIIO::pvt::is_utility_interop_id;
    using OIIO::pvt::parse_interop_id;
    using OIIO::pvt::strip_leftmost_namespace;

    std::vector<std::string> names = interop_identities_config_names();
    OIIO_CHECK_GT(names.size(), size_t(0));

    // Invariant 1: every registry entry's `name:` equals its `interop_id:`
    // in the source config (verified at authoring time -- both fields are
    // set to the identical value for every entry). At the OCIO API level
    // that invariant means every declared name must resolve to itself:
    // count mismatches across the whole registry and expect 0.
    int name_mismatches = 0;
    for (const auto& name : names)
        if (!interop_identities_config_resolves(name))
            ++name_mismatches;
    OIIO_CHECK_EQUAL(name_mismatches, 0);

    // Invariant 2: every namespaced entry's bare stripped form resolves
    // through OCIO's own alias resolution to the same color space -- no
    // registry-side code needed. Proof it's an alias and not a coincidental
    // separate entry: the stripped form does not itself appear in the
    // config's own declared-name list.
    std::unordered_set<std::string> declared_names(names.begin(), names.end());
    int namespaced_checked = 0;
    for (const auto& name : names) {
        auto parts = parse_interop_id(name);
        if (parts.form != InteropIdForm::INNER_BASE)
            continue;  // not an "inner:base" namespaced entry
        std::string bare = strip_leftmost_namespace(name);
        OIIO_CHECK_ASSERT(interop_identities_config_resolves(name));
        OIIO_CHECK_ASSERT(interop_identities_config_resolves(bare));
        OIIO_CHECK_ASSERT(declared_names.find(bare) == declared_names.end());
        ++namespaced_checked;
    }
    OIIO_CHECK_GT(namespaced_checked, 0);

    // Invariant 5: `data` is a config entry (isdata: true); the utility
    // tokens `unknown`/`bypass` are pure grammar-layer strings, never
    // registry lookups.
    OIIO_CHECK_ASSERT(interop_identities_config_resolves("data"));
    OIIO_CHECK_FALSE(interop_identities_config_resolves("unknown"));
    OIIO_CHECK_FALSE(interop_identities_config_resolves("bypass"));
    OIIO_CHECK_ASSERT(is_utility_interop_id("unknown"));
    OIIO_CHECK_ASSERT(is_utility_interop_id("bypass"));

    // The DCDM family: the XYZ form (g26_xyzd65_display, alias dcdm_xyzd65) and
    // the P3 form (dcdm_p3d65_display, = g26_p3d65 colorimetry + the DCI white
    // headroom) both resolve as registry spaces. dcdm_p3d65_display is the P3
    // DCDM identity the write-canonical conversion adds; g26_xyzd65_display is
    // the conversion TARGET, which must be a real space for the mapping to run.
    OIIO_CHECK_ASSERT(interop_identities_config_resolves("g26_xyzd65_display"));
    OIIO_CHECK_ASSERT(interop_identities_config_resolves("dcdm_xyzd65"));
    OIIO_CHECK_ASSERT(interop_identities_config_resolves("g26_p3d65_display"));
    OIIO_CHECK_ASSERT(interop_identities_config_resolves("dcdm_p3d65_display"));

    // Cross-check against the CIF wiki's published Color Interop IDs:
    // https://github.com/AcademySoftwareFoundation/ColorInterop/wiki/Registered-Color-Interop-IDs
    // A representative subset of the published IDs; extend to the full
    // list if it is ever vendored.
    static const char* published_ids[] = {
        "lin_ap0_scene",
        "lin_rec709_scene",
        "lin_p3d65_scene",
        "lin_rec2020_scene",
        "lin_adobergb_scene",
        "srgb_rec709_display",
        "g24_rec709_display",
        "g22_rec709_display",
        "lin_rec709_display",
        "lin_p3d65_display",
        "lin_p3d60_display",      // oiio: alias, bare
        "lin_ciexyzd65_display",  // ocio: alias, bare
        "data",
    };
    for (const char* id : published_ids)
        OIIO_CHECK_ASSERT(interop_identities_config_resolves(id));
}



// Namespace-tolerant round-trip over the built-in interop identities registry:
// for every canonical (grammar-valid) interop id the registry declares, if it
// resolves to a real space in the active config, deriving that space's id back
// must return the SAME id -- exactly, or up to removing one leftmost namespace
// from a single side (never both, the rule resolve() itself uses). A config
// author may declare a namespaced form of a published id, and declared ids are
// authoritative; one-sided namespace stripping preserves that identity. Swept
// over ocio://default and, on OCIO >= 2.5, the builtin studio config.
static void
test_registry_round_trip()
{
    using OIIO::pvt::interop_identities_config_names;
    using OIIO::pvt::is_valid_interop_id;
    using OIIO::pvt::strip_leftmost_namespace;

    if (!ColorConfig::supportsOpenColorIO())
        return;
    // ocio:// built-in configs require OCIO >= 2.2.
    if (ColorConfig::OpenColorIO_version_hex() < 0x02020000)
        return;

    const std::vector<std::string> ids = interop_identities_config_names();
    OIIO_CHECK_GT(ids.size(), size_t(0));

    // One-sided-strip round-trip predicate: got equals id, or exactly one side
    // loses one leftmost namespace to reach the other -- never both.
    auto round_trips = [&](const std::string& got, const std::string& id) {
        return got == id || strip_leftmost_namespace(got) == id
               || got == strip_leftmost_namespace(id);
    };

    // equivalent() resolves names first, so a color interop ID and a native
    // config name for the same encoding are equivalent. "lin_ap1_scene" (a
    // CIID) and "ACEScg" (ocio://default's native name) denote the same space.
    {
        ColorConfig cc("ocio://default");
        if (!cc.has_error() && cc.getNumColorSpaces() > 0)
            OIIO_CHECK_ASSERT(cc.equivalent("lin_ap1_scene", "ACEScg"));
    }

    const bool have_studio = ColorConfig::OpenColorIO_version_hex()
                             >= 0x02050000;
    std::vector<std::string> configs = { "ocio://default" };
    if (have_studio)
        configs.emplace_back("ocio://studio-config-latest");

    for (const std::string& cfgname : configs) {
        ColorConfig cc(cfgname);
        if (cc.has_error() || cc.getNumColorSpaces() == 0)
            continue;  // built-in config unavailable in this OCIO build

        for (const std::string& id : ids) {
            // Only the grammar-valid entries are canonical interop ids; the
            // registry also carries the studio config's human-readable space
            // names (e.g. "ACEScg", "sRGB - Display"), which are not ids.
            if (!is_valid_interop_id(id))
                continue;
            std::string resolved(cc.resolve(id));
            if (cc.getColorSpaceIndex(resolved) < 0)
                continue;  // id did not land on a real space in this config
            std::string got(OIIO::pvt::derive_color_interop_id(cc, resolved));
            if (round_trips(got, id))
                continue;

            // The only non-one-sided landing observed: id and got are two
            // DIFFERENT vendor-namespaced forms of the same published id
            // (registry's "oiio:applelog_rec2020_scene" vs the studio config's
            // declared "ocio:applelog_rec2020_scene"), bridged by resolve()'s
            // value-based fingerprint tier rather than by namespace. resolve()'s
            // one-sided rule cannot relate two distinct namespaces, so this is
            // deliberately NOT a round-trip identity -- but get_color_interop_id
            // is correctly returning the config's own declared (authoritative)
            // form. Require it really is that case (identical bare tails) so a
            // genuinely new exception still fails loudly.

            // Below OCIO 2.5 authored interop_id keys are dropped at parse,
            // so only the value-based fingerprint tier can land an id -- and
            // it cannot separate value-identical pairs. OCIO 2.4's
            // ocio://default declares "sRGB Encoded P3-D65 - Texture", the
            // same math as the registry's srgb_p3d65_display (they differ
            // only in referredness, which pixel probes cannot see), so the
            // sweep lands srgbe_p3d65_display on its sibling. Accept
            // exactly that class there -- equivalent() proves the
            // value-identity -- and nothing else: a non-equivalent
            // mislanding still fails loudly. Declared ids disambiguate at
            // 2.5+.
            if (!have_studio && cc.equivalent(id, got))
                continue;
            OIIO_CHECK_EQUAL(strip_leftmost_namespace(got),
                             strip_leftmost_namespace(id));
        }
    }

    // Explicit live case: on the OCIO >= 2.5 studio config, registry id
    // "g24_rec709_scene" resolves to a space the studio config declares as
    // "ocio:g24_rec709_scene", so the round trip passes only via the
    // stripped-form arm (one leftmost namespace removed from the derived id),
    // never as an exact match.
    if (have_studio) {
        ColorConfig studio("ocio://studio-config-latest");
        if (!studio.has_error() && studio.getNumColorSpaces() > 0) {
            std::string resolved(studio.resolve("g24_rec709_scene"));
            OIIO_CHECK_GE(studio.getColorSpaceIndex(resolved), 0);
            std::string got(
                OIIO::pvt::derive_color_interop_id(studio, resolved));
            OIIO_CHECK_NE(got, std::string("g24_rec709_scene"));
            OIIO_CHECK_EQUAL(strip_leftmost_namespace(got),
                             std::string("g24_rec709_scene"));
        }
    }
}



// The internal ColorConfig::get_builtin_interop_ids() lookup must be an
// exact-set match for the canonical `interop_id:` set declared in the
// embedded interop identities registry source (NOT the composite parsed
// config, whose declared names diverge from the canonical id set under
// OCIO >= 2.5's studio-config overlay), and its storage must be stable for
// the life of the process.
static void
test_builtin_interop_ids_sync()
{
    using OIIO::pvt::embedded_interop_identities_ids;

    std::vector<std::string> registry = embedded_interop_identities_ids();
    OIIO_CHECK_GT(registry.size(), size_t(0));

    std::unordered_set<std::string> registry_set(registry.begin(),
                                                 registry.end());
    cspan<string_view> all = ColorConfig::get_builtin_interop_ids();
    std::unordered_set<std::string> all_set;
    for (string_view id : all)
        all_set.emplace(id);

    OIIO_CHECK_EQUAL(all_set.size(), registry_set.size());
    for (const auto& id : registry_set) {
        if (all_set.count(id) != 1)
            Strutil::print("  registry id missing from builtin ids: {}\n", id);
        OIIO_CHECK_ASSERT(all_set.count(id) == 1);
    }
    for (const auto& id : all_set) {
        if (registry_set.count(id) != 1)
            Strutil::print("  builtin id not in registry: {}\n", id);
        OIIO_CHECK_ASSERT(registry_set.count(id) == 1);
    }

    // Process-lifetime storage: repeated calls return the same data.
    OIIO_CHECK_ASSERT(ColorConfig::get_builtin_interop_ids().data()
                      == all.data());
    OIIO_CHECK_EQUAL(ColorConfig::get_builtin_interop_ids().size(), all.size());
}



// The legacy static CICP/interop-id
// table (color_ocio.cpp's `color_interop_ids[]`) must not drift from the
// registry that is its single source of truth for id spelling. The table
// spells its ids as plain string literals (they are data rows), so this
// runtime check is the whole guarantee: every entry except the "unknown"
// utility token, which the registry deliberately omits, must resolve in
// the registry set -- a typo or a registry rename fails here.
static void
test_legacy_table_registry_sync()
{
    using OIIO::pvt::embedded_interop_identities_ids;
    using OIIO::pvt::legacy_interop_id_table_names;

    std::vector<std::string> registry = embedded_interop_identities_ids();
    OIIO_CHECK_GT(registry.size(), size_t(0));
    std::unordered_set<std::string> registry_set(registry.begin(),
                                                 registry.end());

    std::vector<std::string> table = legacy_interop_id_table_names();
    OIIO_CHECK_GT(table.size(), size_t(0));
    int unknown_count = 0;
    for (const auto& id : table) {
        // "unknown" is the one table entry the registry deliberately does
        // not declare (utility token, not an identity). "data" is also a
        // utility token but IS a registry entry, so it takes the normal
        // registry-membership path below.
        if (id == "unknown") {
            ++unknown_count;
            continue;
        }
        if (registry_set.count(id) != 1)
            Strutil::print("  table id not in registry: {}\n", id);
        OIIO_CHECK_ASSERT(registry_set.count(id) == 1);
    }
    OIIO_CHECK_EQUAL(unknown_count, 1);
}



// Exercise the color-space classification pass: the simple-transform
// allowlist and the lazy per-space analysis that sets the classification
// bits. Uses a minimal generated OCIO config. CDL and ACES-OUTPUT builtins
// stand in for the general "complex transform" class (which also covers
// LUT3D/LOOK); a matrix+TF space and a context-varying-but-resolvable
// ColorSpaceTransform reference cover the simple cases.
static void
test_color_space_classification()
{
    using OIIO::pvt::color_space_analysis_flags;
    using OIIO::pvt::color_space_analyzed;
    namespace P = OIIO::pvt;

    if (!ColorConfig::supportsOpenColorIO())
        return;

    static const char* config_yaml = R"(ocio_profile_version: 2.1

environment:
  SHOT_CS: matrix_tf_space
search_path: ""
roles:
  scene_linear: ref
  default: ref

displays:
  disp:
    - !<View> {name: main, colorspace: ref}

colorspaces:
  - !<ColorSpace>
    name: ref

  - !<ColorSpace>
    name: rawdata
    isdata: true

  - !<ColorSpace>
    name: matrix_tf_space
    from_scene_reference: !<GroupTransform>
      children:
        - !<MatrixTransform> {matrix: [2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1]}
        - !<ExponentTransform> {value: [2.2, 2.2, 2.2, 1]}

  - !<ColorSpace>
    name: cdl_space
    from_scene_reference: !<CDLTransform> {slope: [0.9, 1.1, 1.0]}

  - !<ColorSpace>
    name: aces_output_space
    from_scene_reference: !<BuiltinTransform> {style: ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - SDR-VIDEO_1.0}

  - !<ColorSpace>
    name: camera_log
    to_scene_reference: !<ColorSpaceTransform> {src: $SHOT_CS, dst: ref}
)";

    std::string config_path = Filesystem::temp_directory_path()
                              + "/oiio_color_test_classify.ocio";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(config_path, config_yaml));

    ColorConfig cc(config_path);
    OIIO_CHECK_ASSERT(!cc.has_error());

    // Fully lazy: constructing the ColorConfig runs no classification.
    OIIO_CHECK_FALSE(color_space_analyzed(cc, "matrix_tf_space"));

    // Matrix + transfer function: a simple, context-invariant space. The
    // first flags query is what triggers the lazy analysis.
    int mflags = color_space_analysis_flags(cc, "matrix_tf_space");
    OIIO_CHECK_ASSERT(mflags & P::ColorSpaceIsSimple);
    OIIO_CHECK_ASSERT(mflags & P::ColorSpaceIsContextInvariant);
    OIIO_CHECK_FALSE(mflags & P::ColorSpaceHasComplexTransform);
    // ...and now it has been analyzed.
    OIIO_CHECK_ASSERT(color_space_analyzed(cc, "matrix_tf_space"));

    // Complex transforms (CDL, ACES-OUTPUT) are rejected by the allowlist.
    int cdlflags = color_space_analysis_flags(cc, "cdl_space");
    OIIO_CHECK_ASSERT(cdlflags & P::ColorSpaceHasComplexTransform);
    OIIO_CHECK_FALSE(cdlflags & P::ColorSpaceIsSimple);
    int acesflags = color_space_analysis_flags(cc, "aces_output_space");
    OIIO_CHECK_ASSERT(acesflags & P::ColorSpaceHasComplexTransform);
    OIIO_CHECK_FALSE(acesflags & P::ColorSpaceIsSimple);

    // A data space is flagged is_data and is never a matching candidate.
    int dflags = color_space_analysis_flags(cc, "rawdata");
    OIIO_CHECK_ASSERT(dflags & P::ColorSpaceIsData);
    OIIO_CHECK_ASSERT(dflags & P::ColorSpaceShouldSkipMatching);
    OIIO_CHECK_FALSE(dflags & P::ColorSpaceIsSimple);

    // A space whose transform references a context variable is not context
    // invariant, but (resolving through the context) is still simple.
    int cflags = color_space_analysis_flags(cc, "camera_log");
    OIIO_CHECK_FALSE(cflags & P::ColorSpaceIsContextInvariant);
    OIIO_CHECK_ASSERT(cflags & P::ColorSpaceIsSimple);

    // Unknown names classify as nothing.
    OIIO_CHECK_EQUAL(color_space_analysis_flags(cc, "no_such_space"), 0);
    OIIO_CHECK_FALSE(color_space_analyzed(cc, "no_such_space"));

    Filesystem::remove(config_path);
}



// Exercise color space fingerprinting: the probe protocol, the exact
// tolerance-gated matcher (including the scene-vs-display reference-kind gate),
// byte-reproducibility, and deterministic sorted iteration. Uses OCIO's
// built-in default config, which carries the aces_interchange role this slice
// assumes is resolved.
static void
test_color_space_fingerprint()
{
    using OIIO::pvt::color_space_fingerprint;
    using OIIO::pvt::color_space_fingerprint_order;
    using OIIO::pvt::color_space_fingerprints_match;
    using OIIO::pvt::ColorSpaceFingerprint;

    if (!ColorConfig::supportsOpenColorIO())
        return;
    // ocio:// built-in configs require OCIO >= 2.2.
    if (ColorConfig::OpenColorIO_version_hex() < 0x02020000)
        return;

    ColorConfig cc("ocio://default");
    if (cc.has_error() || cc.getNumColorSpaces() == 0)
        return;  // built-in configs unavailable in this OCIO build

    // A space and one of its aliases are the same OCIO color space, so their
    // fingerprints are byte-identical -- and match within tolerance.
    ColorSpaceFingerprint ap0      = color_space_fingerprint(cc, "ACES2065-1");
    ColorSpaceFingerprint ap0alias = color_space_fingerprint(cc, "lin_ap0");
    OIIO_CHECK_ASSERT(ap0.computed());
    OIIO_CHECK_ASSERT(ap0alias.computed());
    OIIO_CHECK_ASSERT(color_space_fingerprints_match(ap0, ap0alias));
    OIIO_CHECK_ASSERT(ap0.values == ap0alias.values);  // byte-identical

    // The same space fingerprinted twice yields byte-identical floats
    // (OPTIMIZATION_NONE + reused probe config).
    ColorSpaceFingerprint ap0again = color_space_fingerprint(cc, "ACES2065-1");
    OIIO_CHECK_ASSERT(ap0.values == ap0again.values);

    // Two distinct scene spaces (lin_ap0 vs an sRGB-encoded space) do NOT
    // match.
    ColorSpaceFingerprint srgb = color_space_fingerprint(cc, "sRGB - Texture");
    OIIO_CHECK_ASSERT(srgb.computed());
    OIIO_CHECK_ASSERT(srgb.reference_kind == ap0.reference_kind);
    OIIO_CHECK_FALSE(color_space_fingerprints_match(ap0, srgb));

    // A scene space and a display space never compare equal: the reference-kind
    // gate rejects them before any float comparison.
    ColorSpaceFingerprint disp = color_space_fingerprint(cc, "sRGB - Display");
    if (disp.computed()) {
        OIIO_CHECK_ASSERT(disp.reference_kind != ap0.reference_kind);
        OIIO_CHECK_FALSE(color_space_fingerprints_match(ap0, disp));
    }

    // Unknown names produce an empty (uncomputed) fingerprint.
    OIIO_CHECK_FALSE(color_space_fingerprint(cc, "no_such_space").computed());

    // The bulk pass iterates the classification's sorted simple-space cache, so
    // the fingerprinted names come back in deterministic sorted order.
    std::vector<std::string> order = color_space_fingerprint_order(cc);
    OIIO_CHECK_ASSERT(!order.empty());
    OIIO_CHECK_ASSERT(std::is_sorted(order.begin(), order.end()));
    OIIO_CHECK_ASSERT(order == color_space_fingerprint_order(cc));  // stable
}



// The identification tier's hardest documented discrimination case: sRGB's
// PIECEWISE transfer curve vs a PURE gamma-2.2 power curve, over identical
// (Rec.709/D65) primaries and the same display reference. Everything except
// the transfer function is held equal, so the only thing separating the two
// fingerprints is the curve -- and the shipped absolute tolerance (5e-3, see
// kFingerprintAbsTolerance) must separate them in BOTH directions. This is the
// number Doug Walker's "what does your tolerance conflate?" question wants: an
// executable statement of what the tolerance provably does NOT conflate.
//
// The separation is dominated by the probe's dark neutral (linear ~3.4%),
// where the two curves are furthest apart; the diffuse-white and black probe
// pixels agree exactly, which is precisely why the probe cannot be a
// white-point-only check.
static void
test_transfer_curve_discrimination()
{
    using OIIO::pvt::color_space_fingerprint;
    using OIIO::pvt::color_space_fingerprints_match;
    using OIIO::pvt::ColorSpaceFingerprint;

    // The documented identification gate. Kept as a literal here (rather than
    // reaching into color_ocio_pvt.h) so a silent widening of the shipped
    // constant fails this test instead of moving with it.
    const float kDocumentedTolerance = 5e-3f;

    if (!ColorConfig::supportsOpenColorIO())
        return;
    if (ColorConfig::OpenColorIO_version_hex() < 0x02020000)
        return;

    // Two display-referred spaces over the SAME XYZ-D65 -> Rec.709 matrix.
    // srgb_disp uses the sRGB piecewise curve (ExponentWithLinear, gamma 2.4 +
    // 0.055 offset); g22_disp uses a pure 2.2 power law. Nothing else differs.
    static const char* yaml = R"(ocio_profile_version: 2.1
strictparsing: false
search_path: ""
roles:
  default: ACEScg
  scene_linear: ACEScg
  aces_interchange: ACES2065-1
displays:
  disp:
    - !<View> {name: srgb, colorspace: srgb_disp}
    - !<View> {name: g22, colorspace: g22_disp}
colorspaces:
  - !<ColorSpace>
    name: ACES2065-1
    encoding: scene-linear
  - !<ColorSpace>
    name: ACEScg
    encoding: scene-linear
    to_scene_reference: !<MatrixTransform> {matrix: [0.6954522414, 0.1406786965, 0.1638690622, 0, 0.0447945634, 0.8596711185, 0.0955343182, 0, -0.0055258826, 0.0040252103, 1.0015006723, 0, 0, 0, 0, 1]}
display_colorspaces:
  - !<ColorSpace>
    name: srgb_disp
    encoding: sdr-video
    from_display_reference: !<GroupTransform>
      children:
        - !<MatrixTransform> {matrix: [3.2409699419, -1.5373831776, -0.4986107603, 0, -0.9692436363, 1.8759675015, 0.0415550574, 0, 0.0556300797, -0.2039769589, 1.0569715142, 0, 0, 0, 0, 1]}
        - !<ExponentWithLinearTransform> {gamma: 2.4, offset: 0.055, direction: inverse}
  - !<ColorSpace>
    name: g22_disp
    encoding: sdr-video
    from_display_reference: !<GroupTransform>
      children:
        - !<MatrixTransform> {matrix: [3.2409699419, -1.5373831776, -0.4986107603, 0, -0.9692436363, 1.8759675015, 0.0415550574, 0, 0.0556300797, -0.2039769589, 1.0569715142, 0, 0, 0, 0, 1]}
        - !<ExponentTransform> {value: 2.2, style: mirror, direction: inverse}
)";
    std::string path        = Filesystem::temp_directory_path()
                       + "/oiio_transfer_discrimination.ocio";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(path, yaml));
    ColorConfig cc(path);
    OIIO_CHECK_ASSERT(!cc.has_error());

    ColorSpaceFingerprint srgb = color_space_fingerprint(cc, "srgb_disp");
    ColorSpaceFingerprint g22  = color_space_fingerprint(cc, "g22_disp");
    OIIO_CHECK_ASSERT(srgb.computed());
    OIIO_CHECK_ASSERT(g22.computed());
    if (srgb.computed() && g22.computed()) {
        // Same reference kind and probe layout: the reference-kind gate cannot
        // be what separates them -- only the float comparison can.
        OIIO_CHECK_EQUAL(srgb.reference_kind, g22.reference_kind);
        OIIO_CHECK_EQUAL(srgb.values.size(), g22.values.size());

        // NOT confused, in both directions.
        OIIO_CHECK_FALSE(color_space_fingerprints_match(srgb, g22));
        OIIO_CHECK_FALSE(color_space_fingerprints_match(g22, srgb));

        // And the separation is real, not marginal: report the largest
        // disagreement over the six identity probe pixels (the 24 floats the
        // matcher actually compares) and require it to clear the documented
        // gate. The number belongs in the P1-4 PR body.
        float worst        = 0.0f;
        size_t worst_i     = 0;
        const size_t bound = std::min<size_t>(24, srgb.values.size());
        for (size_t i = 0; i < bound; ++i) {
            const float d = std::abs(srgb.values[i] - g22.values[i]);
            if (d > worst) {
                worst   = d;
                worst_i = i;
            }
        }
        Strutil::print(
            "transfer-curve discrimination: sRGB vs gamma-2.2, max identity-probe "
            "separation {:.6f} at float {} (probe pixel {}, channel {}); "
            "documented tolerance {:.6f}; margin {:.2f}x\n",
            worst, worst_i, worst_i / 4, worst_i % 4, kDocumentedTolerance,
            worst / kDocumentedTolerance);
        OIIO_CHECK_ASSERT(worst > kDocumentedTolerance);
    }

    // End to end, through the tier that actually consumes the comparison: each
    // space must derive its OWN registry identity, never its neighbour's. The
    // registry carries both srgb_rec709_display and g22_rec709_display, so a
    // tolerance too loose to separate the curves would show up here as two
    // spaces claiming one id (whichever the deterministic walk reached first).
    const std::string srgb_id(
        OIIO::pvt::derive_color_interop_id(cc, "srgb_disp"));
    const std::string g22_id(
        OIIO::pvt::derive_color_interop_id(cc, "g22_disp"));
    Strutil::print("  derived ids: srgb_disp -> '{}', g22_disp -> '{}'\n",
                   srgb_id, g22_id);
    OIIO_CHECK_ASSERT(srgb_id != g22_id);
    OIIO_CHECK_FALSE(srgb_id == "g22_rec709_display");
    OIIO_CHECK_FALSE(g22_id == "srgb_rec709_display");

    Filesystem::remove(path);
}



// Exercise the config interoperability check: a config carrying the
// aces_interchange role is interoperable and does not warn; a config whose
// scene reference is positively identifiable (known alias/name, or OCIO
// builtin identification) is repaired -- the interopified copy binds the
// interchange role; a stripped config whose reference CANNOT be positively
// identified is NOT repaired (fail-don't-guess: never fabricate an AP0
// equivalence), and warns exactly once per config structure. The whole thing
// is lazy -- constructing a ColorConfig runs none of it.
static void
test_config_interoperability()
{
    using OIIO::pvt::color_config_interchange_name;
    using OIIO::pvt::color_config_interop_computed;
    using OIIO::pvt::color_config_interop_warned;
    using OIIO::pvt::color_config_interopified_cache_off;
    using OIIO::pvt::color_config_interopified_resolves_scene_interchange;
    using OIIO::pvt::color_config_is_interoperable;

    if (!ColorConfig::supportsOpenColorIO())
        return;

    // A config that declares an aces_interchange role is interoperable.
    static const char* interop_yaml = R"(ocio_profile_version: 2.1
search_path: ""
roles:
  default: ref
  scene_linear: ref
  aces_interchange: ref
displays:
  disp:
    - !<View> {name: main, colorspace: ref}
colorspaces:
  - !<ColorSpace>
    name: ref
)";
    // A config that resolves no scene interchange at all -- but does have a
    // scene-referred identity (reference) space to anchor a repair on.
    static const char* stripped_yaml = R"(ocio_profile_version: 2.1
search_path: ""
roles:
  default: ref
  scene_linear: ref
displays:
  disp:
    - !<View> {name: main, colorspace: ref}
colorspaces:
  - !<ColorSpace>
    name: ref

  - !<ColorSpace>
    name: log_space
    from_scene_reference: !<ExponentTransform> {value: [2.2, 2.2, 2.2, 1]}
)";

    std::string interop_path = Filesystem::temp_directory_path()
                               + "/oiio_color_test_interop.ocio";
    std::string stripped_path = Filesystem::temp_directory_path()
                                + "/oiio_color_test_stripped.ocio";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(interop_path, interop_yaml));
    OIIO_CHECK_ASSERT(
        Filesystem::write_text_file(stripped_path, stripped_yaml));

    // --- Interoperable config ---------------------------------------------
    {
        ColorConfig cc(interop_path);
        OIIO_CHECK_ASSERT(!cc.has_error());
        // Fully lazy: construction ran no interop bootstrap.
        OIIO_CHECK_FALSE(color_config_interop_computed(cc));

        OIIO_CHECK_ASSERT(color_config_is_interoperable(cc));
        // ...and querying it is what triggered the bootstrap.
        OIIO_CHECK_ASSERT(color_config_interop_computed(cc));
        // The aces_interchange role points at "ref".
        OIIO_CHECK_EQUAL(color_config_interchange_name(cc), "ref");
        // An interoperable config never warns.
        OIIO_CHECK_FALSE(color_config_interop_warned(cc));
    }

    // --- Non-interoperable config -----------------------------------------
    {
        ColorConfig cc(stripped_path);
        OIIO_CHECK_ASSERT(!cc.has_error());
        OIIO_CHECK_FALSE(color_config_interop_computed(cc));

        // The original config resolves no scene interchange.
        OIIO_CHECK_FALSE(color_config_is_interoperable(cc));
        OIIO_CHECK_ASSERT(color_config_interop_computed(cc));
        OIIO_CHECK_ASSERT(color_config_interchange_name(cc).empty());
        // The bootstrap warning is debug-gated + recorded in-memory only --
        // it must NOT pollute the ColorConfig error string (R4): a
        // non-interoperable config that nobody has tried to cross-config-
        // convert with is otherwise perfectly healthy.
        OIIO_CHECK_FALSE(cc.has_error());

        // The in-memory interopified copy is NOT repaired: "ref" is a bare
        // transformless space that nothing positively identifies as AP0
        // (no role, no known alias, no builtin identification), and the
        // bridge must never fabricate that equivalence. The copy still
        // exists (processor cache off), but resolves no scene interchange.
        OIIO_CHECK_FALSE(
            color_config_interopified_resolves_scene_interchange(cc));
        OIIO_CHECK_ASSERT(color_config_interopified_cache_off(cc));

        // It warned exactly once: this instance emitted the warning, and a
        // second query is silent (the lazy gate ran the bootstrap only once).
        OIIO_CHECK_ASSERT(color_config_interop_warned(cc));
        OIIO_CHECK_FALSE(color_config_is_interoperable(cc));
        OIIO_CHECK_ASSERT(color_config_interop_warned(cc));
        // Still no error string, even after two failed bootstrap queries.
        OIIO_CHECK_FALSE(cc.has_error());

        // A second ColorConfig over the same (structurally identical) config
        // independently discovers it is non-interoperable during its OWN
        // ensure_interop() and reports its OWN `warned` observable
        // accordingly -- that is decoupled from the process-global guard
        // that throttles the printed debug line to once per structural
        // config id (only one of the two Impls "wins" that dedup claim, but
        // both must observably report having been warned).
        ColorConfig cc2(stripped_path);
        OIIO_CHECK_FALSE(color_config_is_interoperable(cc2));
        OIIO_CHECK_ASSERT(color_config_interop_warned(cc2));
        // ...and its own copy likewise resolves no scene interchange.
        OIIO_CHECK_FALSE(
            color_config_interopified_resolves_scene_interchange(cc2));
    }

    // --- Positively identifiable reference: repair IS performed -------------
    // The reference space is transformless but NAMED as a known scene
    // interchange alias ("ACES2065-1"), so the identification is positive and
    // the interopified copy may bind the interchange role to it.
    {
        static const char* identifiable_yaml = R"(ocio_profile_version: 2.1
search_path: ""
roles:
  default: ACES2065-1
  scene_linear: ACES2065-1
displays:
  disp:
    - !<View> {name: main, colorspace: ACES2065-1}
colorspaces:
  - !<ColorSpace>
    name: ACES2065-1
)";
        std::string identifiable_path        = Filesystem::temp_directory_path()
                                        + "/oiio_color_test_identifiable.ocio";
        OIIO_CHECK_ASSERT(
            Filesystem::write_text_file(identifiable_path, identifiable_yaml));
        ColorConfig cc(identifiable_path);
        OIIO_CHECK_ASSERT(!cc.has_error());
        // Alias discovery finds "ACES2065-1" even without the role...
        OIIO_CHECK_ASSERT(color_config_is_interoperable(cc));
        OIIO_CHECK_EQUAL(color_config_interchange_name(cc), "ACES2065-1");
        // ...and the interopified copy binds the role to it.
        OIIO_CHECK_ASSERT(
            color_config_interopified_resolves_scene_interchange(cc));
        OIIO_CHECK_FALSE(color_config_interop_warned(cc));
        Filesystem::remove(identifiable_path);
    }

    Filesystem::remove(interop_path);
    Filesystem::remove(stripped_path);
}



// Exercise the cross-config processor chokepoint (pvt::cross_config_probe, a
// wrapper over OCIO's two-config GetProcessorFromConfigs): a route bridged
// between two structurally distinct configs that share the aces_interchange
// role reproduces the destination config's own transform (probe pixel agrees
// within 1e-6); a config with no interchange role fails with a null processor
// and the OCIO role message set on the destination; and a context key/value
// pair smoke-drives the context-aware overload.
static void
test_cross_config_processor()
{
    using OIIO::pvt::cross_config_probe;

    if (!ColorConfig::supportsOpenColorIO())
        return;
    // Two-config GetProcessorFromConfigs / the interchange-role machinery needs
    // OCIO >= 2.3.
    if (ColorConfig::OpenColorIO_version_hex() < 0x02030000)
        return;

    // Source config: the scene interchange (aces_interchange -> ref) is the
    // route's source endpoint.
    static const char* src_yaml = R"(ocio_profile_version: 2.1
search_path: ""
roles:
  default: ref
  scene_linear: ref
  aces_interchange: ref
colorspaces:
  - !<ColorSpace>
    name: ref
)";
    // Destination config: shares the interchange (ref) and adds a gamma space
    // reachable from it. Structurally distinct from src (it has g22), so the
    // route genuinely crosses configs.
    static const char* dst_yaml = R"(ocio_profile_version: 2.1
search_path: ""
roles:
  default: ref
  scene_linear: ref
  aces_interchange: ref
colorspaces:
  - !<ColorSpace>
    name: ref

  - !<ColorSpace>
    name: g22
    from_scene_reference: !<ExponentTransform> {value: [2.2, 2.2, 2.2, 1]}
)";
    // A config with no scene interchange role at all: unbridgeable.
    static const char* noninterop_yaml = R"(ocio_profile_version: 2.1
search_path: ""
roles:
  default: ref
  scene_linear: ref
colorspaces:
  - !<ColorSpace>
    name: ref
)";

    std::string src_path = Filesystem::temp_directory_path()
                           + "/oiio_color_test_xconfig_src.ocio";
    std::string dst_path = Filesystem::temp_directory_path()
                           + "/oiio_color_test_xconfig_dst.ocio";
    std::string non_path = Filesystem::temp_directory_path()
                           + "/oiio_color_test_xconfig_non.ocio";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(src_path, src_yaml));
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(dst_path, dst_yaml));
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(non_path, noninterop_yaml));

    ColorConfig src_cc(src_path);
    ColorConfig dst_cc(dst_path);
    ColorConfig non_cc(non_path);
    OIIO_CHECK_ASSERT(!src_cc.has_error());
    OIIO_CHECK_ASSERT(!dst_cc.has_error());
    OIIO_CHECK_ASSERT(!non_cc.has_error());

    const float probe[3] = { 0.18f, 0.42f, 0.73f };

    // --- Success: cross-config route equals the destination's own transform --
    {
        auto got = cross_config_probe(src_cc, "ref", dst_cc, "g22", probe);
        OIIO_CHECK_EQUAL(got.size(), size_t(3));
        OIIO_CHECK_ASSERT(!dst_cc.has_error());

        // Independent reference: the destination config's own ref->g22
        // processor, applied to the same probe pixel.
        auto ref = dst_cc.createColorProcessor("ref", "g22");
        OIIO_CHECK_ASSERT(ref.get() != nullptr);
        float expected[3] = { probe[0], probe[1], probe[2] };
        if (ref)
            ref->apply(expected);
        if (got.size() == 3)
            for (int c = 0; c < 3; ++c)
                OIIO_CHECK_EQUAL_THRESH(got[c], expected[c], 1e-6f);

        // Second, INDEPENDENT anchor: a hand-computed value that does not
        // come from any OCIO/OIIO code path at all (the check above still
        // only cross-checks two OCIO entry points against each other, both
        // evaluating the identical "g22" transform -- a bug in how that
        // transform is applied would agree with itself either way). "g22" is
        // authored as a from-scene-reference ExponentTransform{2.2}, and OCIO
        // applies a color space's from-reference transform in its authored
        // (forward) direction when building the reference-to-space half of a
        // ref->g22 conversion; the forward exponent op is a plain per-channel
        // powf(max(0,in), 2.2) (see OpenColorIO's ExponentOpCPU::apply).
        // Hand-computing that directly, with no config/processor involved:
        float hand_expected[3];
        for (int c = 0; c < 3; ++c)
            hand_expected[c] = powf(probe[c], 2.2f);
        if (got.size() == 3)
            for (int c = 0; c < 3; ++c)
                OIIO_CHECK_EQUAL_THRESH(got[c], hand_expected[c], 1e-4f);
    }

    // --- Missing-role failure: empty result + OCIO role message set on dst ---
    {
        auto got = cross_config_probe(src_cc, "ref", non_cc, "ref", probe);
        OIIO_CHECK_ASSERT(got.empty());
        OIIO_CHECK_ASSERT(non_cc.has_error());
        std::string err = non_cc.geterror();
        OIIO_CHECK_ASSERT(Strutil::contains(err, "aces_interchange"));
    }

    // --- Context-aware overload smoke: a key/value pair builds a processor ---
    {
        auto got = cross_config_probe(src_cc, "ref", dst_cc, "g22", probe,
                                      "LUT", "identity");
        OIIO_CHECK_EQUAL(got.size(), size_t(3));
        OIIO_CHECK_ASSERT(!dst_cc.has_error());
    }

    Filesystem::remove(src_path);
    Filesystem::remove(dst_path);
    Filesystem::remove(non_path);
}



// Exercise the cross-config conversion route in ColorConfig::createColorProcessor:
// when a requested color space is absent from the current config
// but is a registry-known interop identity, and the config is color-
// interoperable (natively or via in-memory repair), the conversion routes
// through the built-in interop identities config instead of erroring on the
// name. The gate consults the interoperability state, not bare name-presence;
// OCIO strict parsing restores today's hard-error behavior; and non-strict
// parsing falls back to a pass-through so the pipeline continues.
static void
test_cross_config_conversion()
{
    using OIIO::pvt::identities_route_probe;

    if (!ColorConfig::supportsOpenColorIO())
        return;
    // Two-config GetProcessorFromConfigs / the interchange-role machinery needs
    // OCIO >= 2.3.
    if (ColorConfig::OpenColorIO_version_hex() < 0x02030000)
        return;
    // The route bridges the local AP0 reference to a registry AP1 (ACEScg)
    // identity; skip if this build's identities config doesn't carry it.
    if (!OIIO::pvt::interop_identities_config_resolves("lin_ap1_scene"))
        return;

    // An interoperable config (aces_interchange -> ap0, a transformless scene
    // reference) that LACKS the registry-known scene space "lin_ap1_scene".
    // Non-strict parsing.
    static const char* interop_yaml = R"(ocio_profile_version: 2.1
strictparsing: false
search_path: ""
roles:
  default: ap0
  scene_linear: ap0
  aces_interchange: ap0
colorspaces:
  - !<ColorSpace>
    name: ap0
)";
    // Same config, but with OCIO strict parsing enabled.
    static const char* interop_strict_yaml = R"(ocio_profile_version: 2.1
strictparsing: true
search_path: ""
roles:
  default: ap0
  scene_linear: ap0
  aces_interchange: ap0
colorspaces:
  - !<ColorSpace>
    name: ap0
)";
    // A NON-interoperable config: its only color space has a from-reference
    // transform, so there is no transformless scene reference to anchor a
    // repair on, and no interchange alias resolves -- the interopified copy
    // resolves no scene interchange (gate stays closed). Non-strict parsing.
    static const char* noninterop_yaml = R"(ocio_profile_version: 2.1
strictparsing: false
search_path: ""
roles:
  default: enc
  scene_linear: enc
colorspaces:
  - !<ColorSpace>
    name: enc
    from_scene_reference: !<ExponentTransform> {value: [2.2, 2.2, 2.2, 1]}
)";

    std::string interop_path = Filesystem::temp_directory_path()
                               + "/oiio_color_test_xconv_interop.ocio";
    std::string strict_path = Filesystem::temp_directory_path()
                              + "/oiio_color_test_xconv_strict.ocio";
    std::string non_path = Filesystem::temp_directory_path()
                           + "/oiio_color_test_xconv_non.ocio";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(interop_path, interop_yaml));
    OIIO_CHECK_ASSERT(
        Filesystem::write_text_file(strict_path, interop_strict_yaml));
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(non_path, noninterop_yaml));

    const float probe[3] = { 0.18f, 0.42f, 0.73f };

    // --- Success: registry-known name absent locally routes via the bridge ----
    {
        ColorConfig cc(interop_path);
        OIIO_CHECK_ASSERT(!cc.has_error());

        auto handle = cc.createColorProcessor("ap0", "lin_ap1_scene");
        OIIO_CHECK_ASSERT(handle.get() != nullptr);
        // A real AP0->AP1 transform, not a pass-through no-op.
        if (handle)
            OIIO_CHECK_FALSE(handle->isNoOp());
        OIIO_CHECK_FALSE(cc.has_error());

        // The public bridge reproduces the direct chokepoint route against the
        // identities config (probe-pixel agreement, abs 1e-6/channel).
        float got[3] = { probe[0], probe[1], probe[2] };
        if (handle)
            handle->apply(got);
        auto ref = identities_route_probe(cc, "ap0", "lin_ap1_scene", probe);
        OIIO_CHECK_EQUAL(ref.size(), size_t(3));
        if (ref.size() == 3)
            for (int c = 0; c < 3; ++c)
                OIIO_CHECK_EQUAL_THRESH(got[c], ref[c], 1e-6f);
    }

    // --- Reverse direction (registry src -> local dst): the foreign endpoint
    //     may be either side. "lin_ap1_scene" (registry) -> "ap0" (local) is the
    //     inverse of the block above and must bridge symmetrically. ------------
    {
        ColorConfig cc(interop_path);
        OIIO_CHECK_ASSERT(!cc.has_error());

        // identities_route_probe models only the local-src -> registry-dst
        // direction, so the real, non-identity registry-src -> local-dst
        // processor is the demonstration here.
        auto handle = cc.createColorProcessor("lin_ap1_scene", "ap0");
        OIIO_CHECK_ASSERT(handle.get() != nullptr);
        if (handle)
            OIIO_CHECK_FALSE(handle->isNoOp());  // real AP1->AP0 transform
        OIIO_CHECK_FALSE(cc.has_error());
    }

    // --- Both endpoints registry-known and locally absent: the route runs
    //     registry -> registry (both drawn from the identities config).
    //     "lin_ap0_scene" -> "lin_ap1_scene" is a real AP0->AP1 transform. -----
    if (OIIO::pvt::interop_identities_config_resolves("lin_ap0_scene")) {
        ColorConfig cc(interop_path);
        OIIO_CHECK_ASSERT(!cc.has_error());

        // The route runs registry -> registry (both from the identities
        // config); the local->registry identities_route_probe reference does
        // not model that path, so the real, non-identity processor is the
        // demonstration here.
        auto handle = cc.createColorProcessor("lin_ap0_scene", "lin_ap1_scene");
        OIIO_CHECK_ASSERT(handle.get() != nullptr);
        if (handle)
            OIIO_CHECK_FALSE(handle->isNoOp());  // real AP0->AP1 transform
        OIIO_CHECK_FALSE(cc.has_error());
    }

    // --- Case 2 (CIID with a local equivalent): a valid registry CIID the
    //     config defines locally (here as an alias) resolves same-config and
    //     never touches the cross-config bridge. "lin_ap0_scene" aliases the
    //     local "ap0", so the conversion to "ap0" is a plain local no-op. ------
    {
        static const char* alias_yaml = R"(ocio_profile_version: 2.1
strictparsing: false
search_path: ""
roles:
  default: ap0
  scene_linear: ap0
  aces_interchange: ap0
colorspaces:
  - !<ColorSpace>
    name: ap0
    aliases: [lin_ap0_scene]
)";
        std::string alias_path        = Filesystem::temp_directory_path()
                                 + "/oiio_color_test_xconv_alias.ocio";
        OIIO_CHECK_ASSERT(Filesystem::write_text_file(alias_path, alias_yaml));
        ColorConfig cc(alias_path);
        OIIO_CHECK_ASSERT(!cc.has_error());
        // resolve() maps the CIID to its local equivalent -- the case-2 test.
        OIIO_CHECK_EQUAL(cc.resolve("lin_ap0_scene"), "ap0");
        auto handle = cc.createColorProcessor("lin_ap0_scene", "ap0");
        OIIO_CHECK_ASSERT(handle.get() != nullptr);
        if (handle)
            OIIO_CHECK_ASSERT(handle->isNoOp());  // same-config, not bridged
        OIIO_CHECK_FALSE(cc.has_error());
        Filesystem::remove(alias_path);
    }

    // --- Zero behavior change: a name this config defines still resolves -------
    {
        ColorConfig cc(interop_path);
        auto handle = cc.createColorProcessor("ap0", "ap0");
        OIIO_CHECK_ASSERT(handle.get() != nullptr);  // local no-op, unchanged
        OIIO_CHECK_FALSE(cc.has_error());
    }

    // --- Case 4 (unresolvable): a name that is neither local nor registry-known
    //     is a genuine unknown -- the bridge declines and today's hard error
    //     stands (no pass-through masking a typo). ------------------------------
    {
        ColorConfig cc(interop_path);
        auto handle = cc.createColorProcessor("ap0", "no_such_space_xyzzy");
        OIIO_CHECK_ASSERT(handle.get() == nullptr);
        OIIO_CHECK_ASSERT(cc.has_error());
        (void)cc.geterror();
    }

    // --- Gate respects interop state: a non-interoperable config does not
    //     bridge a registry-known name (no real cross-config transform) --------
    {
        ColorConfig cc(non_path);
        OIIO_CHECK_ASSERT(!cc.has_error());
        // Confirm the fixture is non-interoperable and its repair is unusable.
        // This triggers the lazy bootstrap (and its once-per-config warning).
        OIIO_CHECK_FALSE(OIIO::pvt::color_config_is_interoperable(cc));
        OIIO_CHECK_FALSE(
            OIIO::pvt::color_config_interopified_resolves_scene_interchange(cc));
        // R4/scope (c): the bootstrap warning never sets the error string --
        // has_error() stays false until a cross-config route is attempted.
        OIIO_CHECK_FALSE(cc.has_error());

        auto handle = cc.createColorProcessor("enc", "lin_ap1_scene");
        // Non-strict parsing: the gate is closed, so no bridge is built; the
        // route falls back to a pass-through (identity -- pixels unchanged).
        OIIO_CHECK_ASSERT(handle.get() != nullptr);
        float passthru[3] = { probe[0], probe[1], probe[2] };
        if (handle)
            handle->apply(passthru);
        for (int c = 0; c < 3; ++c)
            OIIO_CHECK_EQUAL_THRESH(passthru[c], probe[c], 1e-6f);
        OIIO_CHECK_ASSERT(cc.has_error());
        std::string err = cc.geterror();
        OIIO_CHECK_ASSERT(Strutil::contains(err, "not color-interoperable"));

        // ...and the fallback did NOT reproduce a real bridge route: since the
        // config resolves no interchange, the direct chokepoint route also
        // fails to build a processor.
        auto ref = identities_route_probe(cc, "enc", "lin_ap1_scene", probe);
        OIIO_CHECK_ASSERT(ref.empty());
    }

    // --- Strict-off fallback: reconciliation failure continues with a
    //     pass-through and records a why + how-to-fix message -----------------
    {
        ColorConfig cc(non_path);
        auto handle = cc.createColorProcessor("enc", "lin_ap1_scene");
        OIIO_CHECK_ASSERT(handle.get() != nullptr);  // non-null fallback
        float passthru[3] = { probe[0], probe[1], probe[2] };
        if (handle)
            handle->apply(passthru);  // pass-through: pixels unchanged
        for (int c = 0; c < 3; ++c)
            OIIO_CHECK_EQUAL_THRESH(passthru[c], probe[c], 1e-6f);
        OIIO_CHECK_ASSERT(cc.has_error());
        std::string err = cc.geterror();
        // Narration recorded on the error string: what failed and how to fix.
        OIIO_CHECK_ASSERT(Strutil::contains(err, "lin_ap1_scene"));
        OIIO_CHECK_ASSERT(Strutil::contains(err, "aces_interchange role"));
    }

    // --- Lenient-fallback outcome travels WITH the processor: a cache hit of
    //     the fallback behaves exactly like its first computation, and IBA
    //     metadata never claims the conversion that didn't happen ------------
    {
        ColorConfig cc(non_path);
        auto h1 = cc.createColorProcessor("enc", "lin_ap1_scene");
        OIIO_CHECK_ASSERT(h1.get() != nullptr);
        OIIO_CHECK_ASSERT(cc.has_error());
        (void)cc.geterror();  // consume (clears the shared error string)

        // Cache hit: the per-call outcome must be identical to the first
        // computation -- the continue-message is re-signaled, not lost.
        auto h2 = cc.createColorProcessor("enc", "lin_ap1_scene");
        OIIO_CHECK_ASSERT(h2.get() != nullptr);
        OIIO_CHECK_ASSERT(cc.has_error());
        std::string err2 = cc.geterror();
        OIIO_CHECK_ASSERT(Strutil::contains(err2, "aces_interchange role"));

        // IBA honesty on BOTH calls: no pixels moved, so the output keeps the
        // true (source) color space -- including when the fallback processor
        // comes from the cache with the shared error string clean.
        ImageBuf src(ImageSpec(2, 2, 3, TypeDesc::FLOAT));
        ImageBufAlgo::fill(src, { 0.25f, 0.5f, 0.75f });
        src.specmod().set_colorspace("enc");
        ImageBuf d1 = ImageBufAlgo::colorconvert(src, "enc", "lin_ap1_scene",
                                                 true, "", "", &cc);
        OIIO_CHECK_ASSERT(!d1.has_error());
        OIIO_CHECK_EQUAL(d1.spec().get_string_attribute("oiio:ColorSpace"),
                         "enc");
        (void)cc.geterror();  // clear again: the cached path must not depend
                              // on leftover shared error state
        ImageBuf d2 = ImageBufAlgo::colorconvert(src, "enc", "lin_ap1_scene",
                                                 true, "", "", &cc);
        OIIO_CHECK_ASSERT(!d2.has_error());
        OIIO_CHECK_EQUAL(d2.spec().get_string_attribute("oiio:ColorSpace"),
                         "enc");
    }

    // --- Strict parsing: hard error (today's behavior) with why + how-to-fix --
    {
        ColorConfig cc(strict_path);
        OIIO_CHECK_ASSERT(!cc.has_error());
        // Even though the config is interoperable (the bridge COULD resolve the
        // name), strict parsing suppresses the bridge and restores the hard
        // error.
        OIIO_CHECK_ASSERT(OIIO::pvt::color_config_is_interoperable(cc));

        auto handle = cc.createColorProcessor("ap0", "lin_ap1_scene");
        OIIO_CHECK_ASSERT(handle.get() == nullptr);  // hard error, no processor
        OIIO_CHECK_ASSERT(cc.has_error());
        std::string err = cc.geterror();
        OIIO_CHECK_ASSERT(Strutil::contains(err, "strict parsing"));
        OIIO_CHECK_ASSERT(Strutil::contains(err, "registry-known interop"));
    }

    Filesystem::remove(interop_path);
    Filesystem::remove(strict_path);
    Filesystem::remove(non_path);
}



// A DISPLAY-referred registry CIID (e.g. srgb_rec709_display) as a --colorconvert
// endpoint against a config that defines NO display space. It can never have a
// local equivalent, yet the registry lowers it colorimetrically to the scene
// reference through its own default view transform; so the cross-config bridge
// routes it through the scene interchange (spec 10 B2 fallback). Unlike a scene
// CIID, it is NOT subject to the strict-parsing hard-error opt-out -- it bridges
// under both strict and non-strict parsing. The conversion is colorimetric: white
// stays white, mid-gray follows the inverse sRGB EOTF, no tonescale/blow-up.
static void
test_cross_config_display_ciid_convert()
{
    if (!ColorConfig::supportsOpenColorIO())
        return;
    if (ColorConfig::OpenColorIO_version_hex() < 0x02030000)
        return;
    if (!OIIO::pvt::interop_identities_config_resolves("srgb_rec709_display"))
        return;

    // An interoperable, scene-only config (aces_interchange -> ACES2065-1,
    // ACEScg via matrix) with NO display space. Two variants: default OCIO
    // strict parsing, and explicit non-strict.
    auto yaml = [](bool strict) {
        return Strutil::fmt::format(R"(ocio_profile_version: 2.1
strictparsing: {}
search_path: ""
roles:
  default: ACEScg
  scene_linear: ACEScg
  aces_interchange: ACES2065-1
colorspaces:
  - !<ColorSpace>
    name: ACES2065-1
    encoding: scene-linear
  - !<ColorSpace>
    name: ACEScg
    encoding: scene-linear
    to_scene_reference: !<MatrixTransform> {{matrix: [0.6954522414, 0.1406786965, 0.1638690622, 0, 0.0447945634, 0.8596711185, 0.0955343182, 0, -0.0055258826, 0.0040252103, 1.0015006723, 0, 0, 0, 0, 1]}}
)",
                                    strict ? "true" : "false");
    };

    for (bool strict : { true, false }) {
        std::string path = Filesystem::temp_directory_path()
                           + (strict ? "/oiio_xconv_disp_strict.ocio"
                                     : "/oiio_xconv_disp_nonstrict.ocio");
        OIIO_CHECK_ASSERT(Filesystem::write_text_file(path, yaml(strict)));
        ColorConfig cc(path);
        OIIO_CHECK_ASSERT(!cc.has_error());

        auto handle = cc.createColorProcessor("srgb_rec709_display", "ACEScg");
        OIIO_CHECK_ASSERT(handle.get() != nullptr);  // bridges under BOTH modes
        if (handle) {
            OIIO_CHECK_FALSE(
                handle->isNoOp());  // a real colorimetric transform

            // Colorimetric: display white -> scene white (no blow-up), matrix
            // preserves neutral (equal channels stay equal).
            float white[3] = { 1.0f, 1.0f, 1.0f };
            handle->apply(white);
            for (int c = 0; c < 3; ++c)
                OIIO_CHECK_EQUAL_THRESH(white[c], 1.0f, 2e-3f);

            // Mid-gray 0.5 follows the inverse sRGB EOTF (~0.214), NOT a
            // tonescale and NOT a pass-through (which would leave 0.5).
            float mid[3] = { 0.5f, 0.5f, 0.5f };
            handle->apply(mid);
            for (int c = 0; c < 3; ++c) {
                OIIO_CHECK_EQUAL_THRESH(mid[c], 0.214f, 3e-3f);
                OIIO_CHECK_ASSERT(mid[c] < 0.49f);  // definitely not a no-op
            }
        }
        // No error recorded on a clean bridge success (either parsing mode).
        OIIO_CHECK_FALSE(cc.has_error());
        Filesystem::remove(path);
    }
}



// Step 2 (spec 10 B2): the display interchange. interopify synthesizes a
// colorimetric cie_xyz_d65_interchange on the in-memory copy, and a
// display-referred CIID PREFERS it -- enabling a colorimetric display->display
// route (a display CIID to a display-referred space in the user's config) that
// the scene anchor cannot express by a single matrix. The local display space
// here uses P3-D65 primaries (distinct from the sRGB CIID) so resolve() cannot
// map the CIID to it locally, forcing the cross-config bridge.
static void
test_cross_config_display_interchange()
{
    using OIIO::pvt::interopified_display_interchange_probe;

    if (!ColorConfig::supportsOpenColorIO())
        return;
    if (ColorConfig::OpenColorIO_version_hex() < 0x02030000)
        return;
    if (!OIIO::pvt::interop_identities_config_resolves("srgb_rec709_display"))
        return;

    // Interoperable config with a P3-D65 display space and NO cie_xyz_d65
    // interchange of its own -- interopify must synthesize one.
    static const char* yaml = R"(ocio_profile_version: 2.1
strictparsing: false
search_path: ""
roles:
  default: ACEScg
  scene_linear: ACEScg
  aces_interchange: ACES2065-1
displays:
  P3:
    - !<View> {name: Raw, colorspace: my_p3_display}
colorspaces:
  - !<ColorSpace>
    name: ACES2065-1
    encoding: scene-linear
  - !<ColorSpace>
    name: ACEScg
    encoding: scene-linear
    to_scene_reference: !<MatrixTransform> {matrix: [0.6954522414, 0.1406786965, 0.1638690622, 0, 0.0447945634, 0.8596711185, 0.0955343182, 0, -0.0055258826, 0.0040252103, 1.0015006723, 0, 0, 0, 0, 1]}
display_colorspaces:
  - !<ColorSpace>
    name: my_p3_display
    encoding: sdr-video
    from_display_reference: !<GroupTransform>
      children:
        - !<MatrixTransform> {matrix: [2.49349691194143, -0.931383617919124, -0.402710784450717, 0, -0.829488969561575, 1.76266406031835, 0.0236246858419436, 0, 0.0358458302437845, -0.0761723892680418, 0.956884524007688, 0, 0, 0, 0, 1]}
        - !<ExponentTransform> {value: 2.2, style: mirror, direction: inverse}
)";
    std::string path        = Filesystem::temp_directory_path()
                       + "/oiio_xconv_disp_interchange.ocio";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(path, yaml));
    ColorConfig cc(path);
    OIIO_CHECK_ASSERT(!cc.has_error());

    // --- (c) The synthesized cie_xyz_d65_interchange is colorimetric: XYZ-D65
    //     white -> scene white, matrix-only (no tonescale/offset). ------------
    {
        // XYZ-D65 white (the CIE white point). A colorimetric anchor maps it to
        // the scene space's white (1,1,1).
        const float xyz_white[3] = { 0.95047f, 1.0f, 1.08883f };
        auto w = interopified_display_interchange_probe(cc, "ACEScg",
                                                        xyz_white);
        OIIO_CHECK_EQUAL(w.size(), size_t(3));
        if (w.size() == 3)
            for (int c = 0; c < 3; ++c)
                OIIO_CHECK_EQUAL_THRESH(w[c], 1.0f, 2e-3f);

        // Matrix-only (linear, no offset): scaling the input scales the output.
        const float xyz_half[3] = { 0.475235f, 0.5f, 0.544415f };
        auto h = interopified_display_interchange_probe(cc, "ACEScg", xyz_half);
        OIIO_CHECK_EQUAL(h.size(), size_t(3));
        if (w.size() == 3 && h.size() == 3)
            for (int c = 0; c < 3; ++c)
                OIIO_CHECK_EQUAL_THRESH(h[c], 0.5f * w[c], 2e-3f);
    }

    // --- (b) display CIID -> a display-referred space in the user's config,
    //     bridged through the display interchange, colorimetric (white->white,
    //     neutral stays neutral, no tonescale/blow-up). ----------------------
    {
        auto handle = cc.createColorProcessor("srgb_rec709_display",
                                              "my_p3_display");
        OIIO_CHECK_ASSERT(handle.get() != nullptr);
        if (handle) {
            OIIO_CHECK_FALSE(handle->isNoOp());  // real cross-config transform

            float white[3] = { 1.0f, 1.0f, 1.0f };
            handle->apply(white);
            for (int c = 0; c < 3; ++c) {
                OIIO_CHECK_EQUAL_THRESH(white[c], 1.0f, 3e-3f);  // white->white
                OIIO_CHECK_ASSERT(white[c] < 1.05f);             // no blow-up
            }

            // Mid-gray stays neutral (equal channels) and bounded -- an
            // encoding change, never a tonescale.
            float mid[3] = { 0.5f, 0.5f, 0.5f };
            handle->apply(mid);
            OIIO_CHECK_EQUAL_THRESH(mid[0], mid[1], 2e-3f);
            OIIO_CHECK_EQUAL_THRESH(mid[1], mid[2], 2e-3f);
            OIIO_CHECK_ASSERT(mid[0] > 0.0f && mid[0] < 1.0f);
        }
        OIIO_CHECK_FALSE(cc.has_error());
    }

    Filesystem::remove(path);
}



// Exercise the cross-config DISPLAY route in ColorConfig::createDisplayTransform:
// when the INPUT color space is absent from the current config
// but is a registry-known interop identity, and the config defines the requested
// display/view, the display transform routes the foreign source through the
// built-in interop identities config into this config's display/view -- the same
// strict/lenient/narration contract as the color-space route. On bridge failure,
// the input is deliberately NOT reinterpreted as scene_linear; instead the
// strict-aware fallback applies: strict OFF -> pass-through (pixels UNCHANGED, NOT
// reinterpreted as scene_linear); strict ON -> hard error.
static void
test_cross_config_display()
{
    using OIIO::pvt::identities_display_route_probe;

    if (!ColorConfig::supportsOpenColorIO())
        return;
    // The two-config display-view GetProcessorFromConfigs overload needs
    // OCIO >= 2.3.
    if (ColorConfig::OpenColorIO_version_hex() < 0x02030000)
        return;
    // The route bridges a registry AP1 (ACEScg) identity into the config's
    // display/view; skip if this build's identities config doesn't carry it.
    if (!OIIO::pvt::interop_identities_config_resolves("lin_ap1_scene"))
        return;

    // An interoperable config (aces_interchange -> ap0) that defines a display/
    // view locally but LACKS the registry-known scene space "lin_ap1_scene".
    static const char* interop_yaml = R"(ocio_profile_version: 2.1
strictparsing: false
search_path: ""
roles:
  default: ap0
  scene_linear: ap0
  aces_interchange: ap0
displays:
  disp:
    - !<View> {name: view1, colorspace: g22}
colorspaces:
  - !<ColorSpace>
    name: ap0

  - !<ColorSpace>
    name: g22
    from_scene_reference: !<ExponentTransform> {value: [2.2, 2.2, 2.2, 1]}
)";
    // Same config, but with OCIO strict parsing enabled.
    static const char* strict_yaml = R"(ocio_profile_version: 2.1
strictparsing: true
search_path: ""
roles:
  default: ap0
  scene_linear: ap0
  aces_interchange: ap0
displays:
  disp:
    - !<View> {name: view1, colorspace: g22}
colorspaces:
  - !<ColorSpace>
    name: ap0

  - !<ColorSpace>
    name: g22
    from_scene_reference: !<ExponentTransform> {value: [2.2, 2.2, 2.2, 1]}
)";
    // A NON-interoperable config that still defines a display/view. Its spaces
    // are all from-reference (gamma) with no transformless scene reference to
    // anchor a repair, so the interopified copy resolves no scene interchange
    // (gate stays closed). The view color space "out" is a REAL transform from
    // scene_linear, so a scene_linear->display transform is non-identity -- this
    // is what makes the trap-1 regression assertion meaningful.
    static const char* non_yaml = R"(ocio_profile_version: 2.1
strictparsing: false
search_path: ""
roles:
  default: enc
  scene_linear: enc
displays:
  disp:
    - !<View> {name: view1, colorspace: out}
colorspaces:
  - !<ColorSpace>
    name: enc
    from_scene_reference: !<ExponentTransform> {value: [2.2, 2.2, 2.2, 1]}

  - !<ColorSpace>
    name: out
    from_scene_reference: !<ExponentTransform> {value: [3.0, 3.0, 3.0, 1]}
)";

    std::string interop_path = Filesystem::temp_directory_path()
                               + "/oiio_color_test_xdisp_interop.ocio";
    std::string strict_path = Filesystem::temp_directory_path()
                              + "/oiio_color_test_xdisp_strict.ocio";
    std::string non_path = Filesystem::temp_directory_path()
                           + "/oiio_color_test_xdisp_non.ocio";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(interop_path, interop_yaml));
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(strict_path, strict_yaml));
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(non_path, non_yaml));

    const float probe[3] = { 0.18f, 0.42f, 0.73f };

    // --- Success: registry-known input absent locally routes via the display
    //     bridge, reproducing the direct chokepoint route (abs 1e-6/channel) ----
    {
        ColorConfig cc(interop_path);
        OIIO_CHECK_ASSERT(!cc.has_error());

        auto handle = cc.createDisplayTransform("disp", "view1",
                                                "lin_ap1_scene");
        OIIO_CHECK_ASSERT(handle.get() != nullptr);
        // A real AP1->display transform, not a pass-through no-op.
        if (handle)
            OIIO_CHECK_FALSE(handle->isNoOp());
        OIIO_CHECK_FALSE(cc.has_error());

        float got[3] = { probe[0], probe[1], probe[2] };
        if (handle)
            handle->apply(got);
        auto ref = identities_display_route_probe(cc, "lin_ap1_scene", "disp",
                                                  "view1", probe);
        OIIO_CHECK_EQUAL(ref.size(), size_t(3));
        if (ref.size() == 3)
            for (int c = 0; c < 3; ++c)
                OIIO_CHECK_EQUAL_THRESH(got[c], ref[c], 1e-6f);
    }

    // --- Zero behavior change: a local input space resolves as before ---------
    {
        ColorConfig cc(interop_path);
        auto handle = cc.createDisplayTransform("disp", "view1", "ap0");
        OIIO_CHECK_ASSERT(handle.get() != nullptr);
        OIIO_CHECK_FALSE(cc.has_error());
    }

    // --- Trap-1 regression, strict OFF: the foreign input is NOT silently
    //     treated as scene_linear -- the route falls back to a pass-through
    //     (pixels UNCHANGED) and records a why + how-to-fix message. THIS is the
    //     test of the slice. --------------------------------------------------
    {
        ColorConfig cc(non_path);
        OIIO_CHECK_ASSERT(!cc.has_error());
        OIIO_CHECK_FALSE(OIIO::pvt::color_config_is_interoperable(cc));
        OIIO_CHECK_FALSE(
            OIIO::pvt::color_config_interopified_resolves_scene_interchange(cc));

        auto handle = cc.createDisplayTransform("disp", "view1",
                                                "lin_ap1_scene");
        OIIO_CHECK_ASSERT(handle.get() != nullptr);  // non-null fallback
        float passthru[3] = { probe[0], probe[1], probe[2] };
        if (handle)
            handle->apply(passthru);
        // Pixels unchanged: the input was NOT reinterpreted as scene_linear.
        for (int c = 0; c < 3; ++c)
            OIIO_CHECK_EQUAL_THRESH(passthru[c], probe[c], 1e-6f);
        OIIO_CHECK_ASSERT(cc.has_error());
        std::string err = cc.geterror();
        OIIO_CHECK_ASSERT(Strutil::contains(err, "not color-interoperable"));
        OIIO_CHECK_ASSERT(Strutil::contains(err, "display transform"));

        // Prove the pass-through is meaningful, not a coincidental identity: had
        // the source been reinterpreted as scene_linear (the role space "enc"),
        // the display transform WOULD have changed the pixels.
        ColorConfig cc2(non_path);
        auto trap = cc2.createDisplayTransform("disp", "view1", "enc");
        OIIO_CHECK_ASSERT(trap.get() != nullptr);
        float trapped[3] = { probe[0], probe[1], probe[2] };
        if (trap)
            trap->apply(trapped);
        bool trap_changes = false;
        for (int c = 0; c < 3; ++c)
            if (std::abs(trapped[c] - probe[c]) > 1e-4f)
                trap_changes = true;
        OIIO_CHECK_ASSERT(trap_changes);
    }

    // --- Trap-1 regression, strict ON: hard error (today's behavior) ----------
    {
        ColorConfig cc(strict_path);
        OIIO_CHECK_ASSERT(!cc.has_error());
        OIIO_CHECK_ASSERT(OIIO::pvt::color_config_is_interoperable(cc));

        auto handle = cc.createDisplayTransform("disp", "view1",
                                                "lin_ap1_scene");
        OIIO_CHECK_ASSERT(handle.get() == nullptr);  // hard error, no processor
        OIIO_CHECK_ASSERT(cc.has_error());
        std::string err = cc.geterror();
        OIIO_CHECK_ASSERT(Strutil::contains(err, "strict parsing"));
        OIIO_CHECK_ASSERT(Strutil::contains(err, "registry-known interop"));
    }

    Filesystem::remove(interop_path);
    Filesystem::remove(strict_path);
    Filesystem::remove(non_path);
}



// Set/clear an environment variable portably (the same idiom OIIO uses
// elsewhere, e.g. src/iv/ivmain.cpp).
static void
set_env_var(const char* name, const char* value)
{
#ifdef _MSC_VER
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}
static void
unset_env_var(const char* name)
{
#ifdef _MSC_VER
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}



// Exercise the process-global flyweight fingerprint cache: a repeated lookup is
// a hit (the cache does not grow); a context-invariant space collapses to one
// bucket across two different contexts of the same structural config, while a
// context-sensitive space keeps a bucket per context; a different structural
// config keys separately (old entries orphan, no crash); the bulk warm pass
// populates one entry per simple space; and reset empties it.
static void
test_color_space_fingerprint_cache()
{
    using OIIO::pvt::color_space_fingerprint_cache_reset;
    using OIIO::pvt::color_space_fingerprint_cache_size;
    using OIIO::pvt::color_space_fingerprint_cached;
    using OIIO::pvt::color_space_fingerprint_order;
    using OIIO::pvt::color_space_fingerprint_warm;
    using OIIO::pvt::ColorSpaceFingerprint;

    if (!ColorConfig::supportsOpenColorIO())
        return;
    // The interchange role + IdentifyBuiltinColorSpace path needs OCIO >= 2.2.
    if (ColorConfig::OpenColorIO_version_hex() < 0x02020000)
        return;

    // Interoperable config with a declared context variable, one context-
    // invariant simple space (matrix_inv) and one context-sensitive simple
    // space (ctx_space, which resolves $CTX_CS). Written to two distinct paths
    // with identical content so both share one structural cache id but each
    // loads its own context.
    static const char* cfg_yaml = R"(ocio_profile_version: 2.1
environment:
  CTX_CS: gamma_a
search_path: ""
roles:
  default: ref
  scene_linear: ref
  aces_interchange: ref
displays:
  disp:
    - !<View> {name: main, colorspace: ref}
colorspaces:
  - !<ColorSpace>
    name: ref

  - !<ColorSpace>
    name: gamma_a
    from_scene_reference: !<ExponentTransform> {value: [2.2, 2.2, 2.2, 1]}

  - !<ColorSpace>
    name: gamma_b
    from_scene_reference: !<ExponentTransform> {value: [1.8, 1.8, 1.8, 1]}

  - !<ColorSpace>
    name: matrix_inv
    from_scene_reference: !<MatrixTransform> {matrix: [2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1]}

  - !<ColorSpace>
    name: ctx_space
    to_scene_reference: !<ColorSpaceTransform> {src: $CTX_CS, dst: ref}
)";
    // A structurally different config, for the distinct-key check.
    static const char* other_yaml = R"(ocio_profile_version: 2.1
search_path: ""
roles:
  default: base
  scene_linear: base
  aces_interchange: base
displays:
  disp:
    - !<View> {name: main, colorspace: base}
colorspaces:
  - !<ColorSpace>
    name: base

  - !<ColorSpace>
    name: doubler
    from_scene_reference: !<MatrixTransform> {matrix: [3, 0, 0, 0, 0, 3, 0, 0, 0, 0, 3, 0, 0, 0, 0, 1]}
)";

    std::string dir    = Filesystem::temp_directory_path();
    std::string path_a = dir + "/oiio_color_test_fpcache_a.ocio";
    std::string path_b = dir + "/oiio_color_test_fpcache_b.ocio";
    std::string path_o = dir + "/oiio_color_test_fpcache_other.ocio";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(path_a, cfg_yaml));
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(path_b, cfg_yaml));
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(path_o, other_yaml));

    color_space_fingerprint_cache_reset();
    OIIO_CHECK_EQUAL(color_space_fingerprint_cache_size(), size_t(0));

    // --- All of cc1's work happens under CTX_CS=gamma_a --------------------
    set_env_var("CTX_CS", "gamma_a");
    ColorConfig cc1(path_a);
    OIIO_CHECK_ASSERT(!cc1.has_error());

    // (1) Same-name lookup twice: the first is a miss that publishes one entry,
    // the second is a hit that returns the identical fingerprint and does not
    // grow the cache.
    ColorSpaceFingerprint m1 = color_space_fingerprint_cached(cc1,
                                                              "matrix_inv");
    OIIO_CHECK_ASSERT(m1.computed());
    OIIO_CHECK_EQUAL(color_space_fingerprint_cache_size(), size_t(1));
    ColorSpaceFingerprint m1b = color_space_fingerprint_cached(cc1,
                                                               "matrix_inv");
    OIIO_CHECK_EQUAL(color_space_fingerprint_cache_size(), size_t(1));  // hit
    OIIO_CHECK_ASSERT(m1.values == m1b.values);

    // Cache the context-sensitive space under cc1's context (gamma_a).
    ColorSpaceFingerprint s1 = color_space_fingerprint_cached(cc1, "ctx_space");
    OIIO_CHECK_ASSERT(s1.computed());
    OIIO_CHECK_EQUAL(color_space_fingerprint_cache_size(), size_t(2));

    // --- cc2 is a second context (gamma_b) of the SAME structural config ---
    set_env_var("CTX_CS", "gamma_b");
    ColorConfig cc2(path_b);
    OIIO_CHECK_ASSERT(!cc2.has_error());

    // (2a) The context-invariant space collapses to the single bucket cc1
    // already populated: querying it through cc2 is a hit (no growth).
    ColorSpaceFingerprint m2 = color_space_fingerprint_cached(cc2,
                                                              "matrix_inv");
    OIIO_CHECK_EQUAL(color_space_fingerprint_cache_size(),
                     size_t(2));  // collapsed
    OIIO_CHECK_ASSERT(m1.values == m2.values);

    // (2b) The context-sensitive space does NOT collapse: cc2's different
    // context keys a separate bucket. This is airtight given (2a): matrix_inv
    // collapsing proved cc1 and cc2 share one structural config id, so the only
    // thing that can grow the cache here is ctx_space's differing context id.
    ColorSpaceFingerprint s2 = color_space_fingerprint_cached(cc2, "ctx_space");
    OIIO_CHECK_EQUAL(color_space_fingerprint_cache_size(),
                     size_t(3));  // new bucket
    OIIO_CHECK_ASSERT(s2.computed());
    // And the VALUES differ: each instance probes under its OWN current
    // context (gamma_a's 2.2 curve vs gamma_b's 1.8 curve), even though both
    // share one process-memoized structural probe copy. The cache key's
    // context id is exactly the context the probe ran under.
    OIIO_CHECK_ASSERT(s1.values != s2.values);

    // (3) A structurally different config keys separately; the earlier entries
    // just orphan (content-addressed, no eviction, no crash).
    {
        ColorConfig cco(path_o);
        OIIO_CHECK_ASSERT(!cco.has_error());
        size_t before           = color_space_fingerprint_cache_size();
        ColorSpaceFingerprint d = color_space_fingerprint_cached(cco,
                                                                 "doubler");
        if (d.computed())
            OIIO_CHECK_EQUAL(color_space_fingerprint_cache_size(), before + 1);
    }

    // (5) The bulk warm pass populates exactly one entry per simple color space
    // (the same deterministic set color_space_fingerprint_order reports).
    set_env_var("CTX_CS", "gamma_a");
    std::vector<std::string> simple = color_space_fingerprint_order(cc1);
    color_space_fingerprint_cache_reset();
    OIIO_CHECK_EQUAL(color_space_fingerprint_cache_size(), size_t(0));
    size_t warmed = color_space_fingerprint_warm(cc1);
    OIIO_CHECK_ASSERT(warmed > 0);
    OIIO_CHECK_EQUAL(warmed, simple.size());
    OIIO_CHECK_EQUAL(color_space_fingerprint_cache_size(), warmed);

    // (4) Reset empties the cache.
    color_space_fingerprint_cache_reset();
    OIIO_CHECK_EQUAL(color_space_fingerprint_cache_size(), size_t(0));

    unset_env_var("CTX_CS");
    Filesystem::remove(path_a);
    Filesystem::remove(path_b);
    Filesystem::remove(path_o);
}



// Exercise the enriched ColorConfig::resolve() read-side tiers: the
// stripped-namespace retry, the config-local "<config>:local:<base>" form,
// the explicit interop_id attribute match (one-side-stripped only, never
// both), the data/bypass utility-token ranking (and "unknown"'s deliberate
// exclusion from it), and the registry-equivalence (fingerprint) tier --
// plus the historical passthrough-on-total-miss regression guard. Small
// hand-built OCIO configs, same pattern as test_color_space_classification /
// test_config_interoperability, isolate each tier so one config's fixtures
// can't accidentally satisfy a different tier's assertion.
static void
test_interop_resolve()
{
    if (!ColorConfig::supportsOpenColorIO())
        return;

    // OCIO >= 2.5 is required for the `interop_id:` color space attribute
    // (native getInteropID()); tiers that depend on it are gated below.
    const bool has_interop_id_attr = ColorConfig::OpenColorIO_version_hex()
                                     >= 0x02050000;

    // ---- Base fixture: stripped-namespace, config-local, literal-unknown,
    // and total-miss passthrough. No interop_id attributes -- safe to parse
    // on any linked OCIO version. -----------------------------------------
    static const char* base_yaml = R"(ocio_profile_version: 2.1
name: resolvetest
search_path: ""
roles:
  default: ref
  scene_linear: ref
  aces_interchange: ref
displays:
  disp:
    - !<View> {name: main, colorspace: ref}
colorspaces:
  - !<ColorSpace>
    name: ref

  - !<ColorSpace>
    name: gamma24_space
    aliases: [g24_rec709_scene]
    from_scene_reference: !<ExponentTransform> {value: [2.4, 2.4, 2.4, 1]}

  - !<ColorSpace>
    name: local_target
    aliases: [my_local_alias, unknown]
    from_scene_reference: !<ExponentTransform> {value: [1.8, 1.8, 1.8, 1]}
)";
    std::string base_path        = Filesystem::temp_directory_path()
                            + "/oiio_color_test_resolve_base.ocio";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(base_path, base_yaml));
    {
        ColorConfig cc(base_path);
        OIIO_CHECK_ASSERT(!cc.has_error());

        // Tier 1a': stripped-namespace retry -- the full string
        // "myapp:g24_rec709_scene" matches no name/alias/role, but stripping
        // the one leftmost namespace reaches the real alias.
        OIIO_CHECK_EQUAL(cc.resolve("myapp:g24_rec709_scene"), "gamma24_space");

        // Tier 1a'': config-local "<config>:local:<base>" form, matched
        // against names/aliases only. A hit through an alias...
        OIIO_CHECK_EQUAL(cc.resolve("resolvetest:local:my_local_alias"),
                         "local_target");
        // ...and a miss when the base names nothing in this config (proves
        // the tier doesn't fall back to a fuzzy match).
        OIIO_CHECK_EQUAL(cc.resolve("resolvetest:local:no_such_space"),
                         "resolvetest:local:no_such_space");

        // "unknown" is a literal name/alias lookup only -- never routed
        // through the ranked data-space search. Reachable here because
        // local_target happens to carry it as a literal alias (ordinary
        // tier 1a), not because of any utility-token machinery.
        OIIO_CHECK_EQUAL(cc.resolve("unknown"), "local_target");

        // Regression guard: a name that matches nothing in any tier is
        // still passed through unchanged (main's historical behavior).
        OIIO_CHECK_EQUAL(cc.resolve("totally_unrecognized_id"),
                         "totally_unrecognized_id");

        // The failover overload: a miss yields the caller's failover (an
        // empty one making "not recognized" distinguishable from a name
        // that resolves to itself), while a hit is unaffected by it.
        OIIO_CHECK_EQUAL(cc.resolve("totally_unrecognized_id", ""), "");
        OIIO_CHECK_EQUAL(cc.resolve("totally_unrecognized_id", "sentinel"),
                         "sentinel");
        OIIO_CHECK_EQUAL(cc.resolve("resolvetest:local:my_local_alias", ""),
                         "local_target");
        // "local_target" resolves to itself -- a hit, not a passthrough.
        OIIO_CHECK_EQUAL(cc.resolve("local_target", ""), "local_target");
    }
    Filesystem::remove(base_path);

    // ---- Uppercase fixture: an OCIO name/alias lookup is case-insensitive,
    // so a literal (capitalized) "Unknown"/"Bypass" color space is reachable
    // via tier 1a's pre-existing OCIO lookup regardless of the CIF grammar's
    // lowercase-only validity rule (is_valid_interop_id, already covered by
    // test_interop_id_grammar) -- resolve() is not gated on id validity, by
    // design, so it doesn't re-derive that grammar-level invariant. What IS
    // decisive and worth guarding here: the new utility-ranking tier
    // (resolve_data_utility) never even runs for these, because tier 1a's
    // OCIO-native lookup already satisfied the query first.
    {
        static const char* upper_yaml = R"(ocio_profile_version: 2.1
search_path: ""
roles:
  default: ref
  scene_linear: ref
colorspaces:
  - !<ColorSpace>
    name: ref

  - !<ColorSpace>
    name: Uppercase_Utility
    aliases: [Unknown, Bypass]
    isdata: true
)";
        std::string upper_path        = Filesystem::temp_directory_path()
                                 + "/oiio_color_test_resolve_upper.ocio";
        OIIO_CHECK_ASSERT(Filesystem::write_text_file(upper_path, upper_yaml));
        ColorConfig cc(upper_path);
        OIIO_CHECK_ASSERT(!cc.has_error());
        OIIO_CHECK_EQUAL(cc.resolve("unknown"), "Uppercase_Utility");
        OIIO_CHECK_EQUAL(cc.resolve("bypass"), "Uppercase_Utility");
        Filesystem::remove(upper_path);
    }

    // ---- Real "Raw" data space: a config with a data space literally named
    // "Raw" alongside other spaces is NOT the synthetic one-space
    // OCIO::Config::CreateRaw() config, so the utility-token ranking must treat
    // its "Raw" as a valid target -- "bypass"/"data" resolve to it. (The
    // synthetic-raw skip is now keyed on the config's single-colorspace shape,
    // not the name alone.) No interop_id attribute -- safe on any OCIO version.
    {
        static const char* raw_yaml = R"(ocio_profile_version: 2.1
search_path: ""
roles:
  default: ref
  scene_linear: ref
colorspaces:
  - !<ColorSpace>
    name: ref

  - !<ColorSpace>
    name: Raw
    isdata: true
)";
        std::string raw_path        = Filesystem::temp_directory_path()
                               + "/oiio_color_test_resolve_raw.ocio";
        OIIO_CHECK_ASSERT(Filesystem::write_text_file(raw_path, raw_yaml));
        ColorConfig cc(raw_path);
        OIIO_CHECK_ASSERT(!cc.has_error());
        OIIO_CHECK_EQUAL(cc.resolve("bypass"), "Raw");
        OIIO_CHECK_EQUAL(cc.resolve("data"), "Raw");
        Filesystem::remove(raw_path);
    }

    if (has_interop_id_attr) {
        // ---- Explicit interop_id attribute: safe directions -- exactly
        // one side stripped -- still match. ---------------------------
        static const char* safe_yaml = R"(ocio_profile_version: 2.1
search_path: ""
roles:
  default: ref
  scene_linear: ref
colorspaces:
  - !<ColorSpace>
    name: ref

  - !<ColorSpace>
    name: attr_bare_y
    interop_id: "y"
    from_scene_reference: !<ExponentTransform> {value: [1.5, 1.5, 1.5, 1]}

  - !<ColorSpace>
    name: attr_ns_z
    interop_id: "app2:z"
    from_scene_reference: !<ExponentTransform> {value: [1.6, 1.6, 1.6, 1]}
)";
        std::string safe_path        = Filesystem::temp_directory_path()
                                + "/oiio_color_test_resolve_safe.ocio";
        OIIO_CHECK_ASSERT(Filesystem::write_text_file(safe_path, safe_yaml));
        {
            ColorConfig cc(safe_path);
            OIIO_CHECK_ASSERT(!cc.has_error());
            // Query-side stripped: bare attribute "y" matches namespaced
            // query "app:y".
            OIIO_CHECK_EQUAL(cc.resolve("app:y"), "attr_bare_y");
            // Attribute-side stripped: namespaced attribute "app2:z"
            // matches bare query "z".
            OIIO_CHECK_EQUAL(cc.resolve("z"), "attr_ns_z");
        }
        Filesystem::remove(safe_path);

        // ---- Explicit interop_id attribute: the both-sides-stripped
        // cross-namespace false positive is rejected. -------------------
        static const char* reject_yaml = R"(ocio_profile_version: 2.1
search_path: ""
roles:
  default: ref
  scene_linear: ref
colorspaces:
  - !<ColorSpace>
    name: ref

  - !<ColorSpace>
    name: attr_oiio_x
    interop_id: "oiio:x"
    from_scene_reference: !<ExponentTransform> {value: [1.7, 1.7, 1.7, 1]}
)";
        std::string reject_path        = Filesystem::temp_directory_path()
                                  + "/oiio_color_test_resolve_reject.ocio";
        OIIO_CHECK_ASSERT(
            Filesystem::write_text_file(reject_path, reject_yaml));
        {
            ColorConfig cc(reject_path);
            OIIO_CHECK_ASSERT(!cc.has_error());
            // "oiio:x" and "ocio:x" both strip to "x", but neither raw side
            // matches -- a miss, not a false positive.
            OIIO_CHECK_EQUAL(cc.resolve("ocio:x"), "ocio:x");
        }
        Filesystem::remove(reject_path);

        // ---- Reserved `local` namespace: a declared interop_id attribute
        // whose leftmost segment is `local` is never matched by the
        // attribute tier -- otherwise a grammar-legal "local:x" declaration
        // would poach OTHER configs' private "<config>:local:x" IDs via the
        // stripped-attribute match. The genuine config-local tier and
        // ordinary declared attributes are unaffected.
        static const char* localns_yaml = R"(ocio_profile_version: 2.1
name: localns
search_path: ""
roles:
  default: ref
  scene_linear: ref
colorspaces:
  - !<ColorSpace>
    name: ref

  - !<ColorSpace>
    name: poacher
    interop_id: "local:x"
    from_scene_reference: !<ExponentTransform> {value: [1.9, 1.9, 1.9, 1]}

  - !<ColorSpace>
    name: inner_target
    aliases: [xbase]
    from_scene_reference: !<ExponentTransform> {value: [2.0, 2.0, 2.0, 1]}

  - !<ColorSpace>
    name: normal_attr
    interop_id: "app9:q"
    from_scene_reference: !<ExponentTransform> {value: [2.1, 2.1, 2.1, 1]}
)";
        std::string localns_path        = Filesystem::temp_directory_path()
                                   + "/oiio_color_test_resolve_localns.ocio";
        OIIO_CHECK_ASSERT(
            Filesystem::write_text_file(localns_path, localns_yaml));
        {
            ColorConfig cc(localns_path);
            OIIO_CHECK_ASSERT(!cc.has_error());
            // Another config's config-local ID strips to "local:x", which
            // equals the declared attribute -- but the reserved-namespace
            // exclusion makes it a total miss, not a poach.
            OIIO_CHECK_EQUAL(cc.resolve("othercfg:local:x"),
                             "othercfg:local:x");
            // The bare declared form itself is unreachable too.
            OIIO_CHECK_EQUAL(cc.resolve("local:x"), "local:x");
            // The genuine config-local tier still resolves for THIS config.
            OIIO_CHECK_EQUAL(cc.resolve("localns:local:xbase"), "inner_target");
            // Ordinary declared attributes are unaffected (attribute-side
            // strip still matches).
            OIIO_CHECK_EQUAL(cc.resolve("q"), "normal_attr");
        }
        Filesystem::remove(localns_path);

        // ---- Utility-token ranking: rank 0 (self-identity via interop_id)
        // short-circuits for both "bypass" and "data". --------------------
        static const char* rank_full_yaml = R"(ocio_profile_version: 2.1
search_path: ""
roles:
  default: ref
  scene_linear: ref
colorspaces:
  - !<ColorSpace>
    name: ref

  - !<ColorSpace>
    name: bypass_named
    interop_id: bypass
    isdata: true

  - !<ColorSpace>
    name: data_named
    interop_id: data
    isdata: true

  - !<ColorSpace>
    name: plain_data_space
    isdata: true
)";
        std::string rank_full_path        = Filesystem::temp_directory_path()
                                     + "/oiio_color_test_resolve_rank_full.ocio";
        OIIO_CHECK_ASSERT(
            Filesystem::write_text_file(rank_full_path, rank_full_yaml));
        {
            ColorConfig cc(rank_full_path);
            OIIO_CHECK_ASSERT(!cc.has_error());
            OIIO_CHECK_EQUAL(cc.resolve("bypass"), "bypass_named");
            OIIO_CHECK_EQUAL(cc.resolve("data"), "data_named");
            // "unknown" is never ranked -- no literal "unknown" name/alias
            // exists here, so it's a total miss even though data spaces do.
            OIIO_CHECK_EQUAL(cc.resolve("unknown"), "unknown");
        }
        Filesystem::remove(rank_full_path);

        // ---- Utility-token ranking: without a self-identified space, a
        // plain data space (rank 1) beats one identified as the OTHER
        // token (rank 2). The "data" query's mirror case runs through the
        // identical ranking code path (data_space_identifies_as / rank
        // computation are symmetric in token/other), so one direction is
        // sufficient coverage here.
        static const char* rank_partial_yaml = R"(ocio_profile_version: 2.1
search_path: ""
roles:
  default: ref
  scene_linear: ref
colorspaces:
  - !<ColorSpace>
    name: ref

  - !<ColorSpace>
    name: data_named
    interop_id: data
    isdata: true

  - !<ColorSpace>
    name: plain_data_space
    isdata: true
)";
        std::string rank_partial_path
            = Filesystem::temp_directory_path()
              + "/oiio_color_test_resolve_rank_partial.ocio";
        OIIO_CHECK_ASSERT(
            Filesystem::write_text_file(rank_partial_path, rank_partial_yaml));
        {
            ColorConfig cc(rank_partial_path);
            OIIO_CHECK_ASSERT(!cc.has_error());
            // No space here self-identifies as "bypass"; data_named
            // identifies as the OTHER token (rank 2), so the plain data
            // space (rank 1) wins.
            OIIO_CHECK_EQUAL(cc.resolve("bypass"), "plain_data_space");
        }
        Filesystem::remove(rank_partial_path);
    }

    // ---- Registry equivalence (tier 2): a query that names no local
    // space, but is fingerprint-identical to a registry identity, resolves
    // to this config's OWN equivalent space -- never a cross-config
    // processor. Both this config's "aces_interchange" anchor and the
    // queried space are identity (no transform), matching the registry's
    // own AP0 reference space (ACES2065-1) exactly. A utility token stays
    // an automatic miss even though this config is otherwise interoperable
    // and reaches this tier. -------------------------------------------
    {
        static const char* registry_yaml = R"(ocio_profile_version: 2.1
search_path: ""
roles:
  default: my_ap0_ref
  scene_linear: my_ap0_ref
  aces_interchange: my_ap0_ref
colorspaces:
  - !<ColorSpace>
    name: my_ap0_ref

  - !<ColorSpace>
    name: another_ap0_identity_space
)";
        std::string registry_path        = Filesystem::temp_directory_path()
                                    + "/oiio_color_test_resolve_registry.ocio";
        OIIO_CHECK_ASSERT(
            Filesystem::write_text_file(registry_path, registry_yaml));
        ColorConfig cc(registry_path);
        OIIO_CHECK_ASSERT(!cc.has_error());

        // Neither space is named or aliased "lin_ap0_scene" -- only a
        // registry fingerprint match can reach one via that id. Both
        // "my_ap0_ref" and "another_ap0_identity_space" are identity (no
        // transform), so both are genuinely fingerprint-equivalent; the
        // tier walks the config's simple spaces in deterministic sorted
        // order and returns the first match, which alphabetically is
        // "another_ap0_identity_space".
        OIIO_CHECK_EQUAL(cc.resolve("lin_ap0_scene"),
                         "another_ap0_identity_space");

        // A utility token has no registry fingerprint and must not attempt
        // one, even on a config that is otherwise interoperable and would
        // reach tier 2.
        OIIO_CHECK_EQUAL(cc.resolve("data"), "data");
        OIIO_CHECK_EQUAL(cc.resolve("bypass"), "bypass");
        OIIO_CHECK_EQUAL(cc.resolve("unknown"), "unknown");

        Filesystem::remove(registry_path);
    }
}



// Exercise the cheap, OCIO-free ICC byte-inspection primitives
// (is_icc_profile / icc_profile_identifier / icc_embedded_cicp).
static void
test_icc_utils()
{
    using OIIO::pvt::icc_embedded_cicp;
    using OIIO::pvt::icc_profile_identifier;
    using OIIO::pvt::is_icc_profile;

    Strutil::print("Testing ICC identification primitives\n");

    // Build a minimal structurally-valid ICC blob: 128-byte header with the
    // 'acsp' signature at byte 36 and `version` in header byte 8, a
    // big-endian tag count at 128, then `tagcount` 12-byte tag entries.
    auto make_icc = [](uint8_t version, uint32_t tagcount) {
        std::vector<uint8_t> blob(132 + size_t(tagcount) * 12, 0);
        memcpy(blob.data() + 36, "acsp", 4);
        blob[8]   = version;
        blob[131] = uint8_t(tagcount);  // BE tag count (< 256 here)
        return blob;
    };

    // ---- is_icc_profile: the sole header gate ----------------------------
    OIIO_CHECK_ASSERT(!is_icc_profile(cspan<uint8_t>()));
    std::vector<uint8_t> junk(200, 0x42);
    OIIO_CHECK_ASSERT(!is_icc_profile(junk));
    std::vector<uint8_t> tiny(make_icc(2, 0));
    tiny.resize(131);  // one byte short of header + tag count
    OIIO_CHECK_ASSERT(!is_icc_profile(tiny));
    OIIO_CHECK_ASSERT(is_icc_profile(make_icc(2, 0)));
    OIIO_CHECK_EQUAL(icc_profile_identifier(junk), "");

    // ---- identifier, v2: XXH64 over raw bytes, 16 lowercase hex. ---------
    auto v2                = make_icc(2, 0);
    const std::string v2id = icc_profile_identifier(v2);
    OIIO_CHECK_EQUAL(v2id.size(), 16);
    OIIO_CHECK_EQUAL(icc_profile_identifier(v2), v2id);  // deterministic
    auto v2tampered = v2;
    for (size_t i = 84; i < 100; ++i)
        v2tampered[i] = 0xAB;
    const std::string v2tamperedid = icc_profile_identifier(v2tampered);
    OIIO_CHECK_EQUAL(v2tamperedid.size(), 16);
    OIIO_CHECK_ASSERT(v2tamperedid != v2id);  // raw bytes differ -> id differs

    // ---- identifier, v4: byte-exact contract. The embedded Profile ID
    // field (bytes 84-99) is NEVER trusted as identity -- two different
    // bodies sharing one embedded ID must not collide, and identical bytes
    // must agree. ----------------------------------------------------------
    auto v4 = make_icc(4, 0);
    OIIO_CHECK_EQUAL(icc_profile_identifier(v4).size(), 16);
    for (size_t i = 84; i < 100; ++i)
        v4[i] = uint8_t(i - 84);
    const std::string v4id = icc_profile_identifier(v4);
    OIIO_CHECK_EQUAL(v4id.size(), 16);  // hash, not the embedded hex field
    OIIO_CHECK_EQUAL(icc_profile_identifier(v4), v4id);  // same bytes, same id
    // A body change with an UNCHANGED embedded Profile ID (the stale/forged
    // ID scenario) must change the identifier.
    auto v4body = v4;
    v4body[100] = 0x7F;
    OIIO_CHECK_ASSERT(icc_profile_identifier(v4body) != v4id);

    // ---- embedded cicpTag reader ----------------------------------------
    // Well-formed v4 cicp tag: entry at 132, tag data at 144 = 'cicp' + 4
    // reserved zero bytes + (P,T,M,R).
    auto make_cicp_icc = [&](uint8_t p, uint8_t t, uint8_t m, uint8_t r) {
        auto blob = make_icc(4, 1);
        blob.resize(156, 0);
        memcpy(blob.data() + 132, "cicp", 4);
        blob[139] = 144;  // BE tag offset
        blob[143] = 12;   // BE tag size
        memcpy(blob.data() + 144, "cicp", 4);
        blob[152] = p;
        blob[153] = t;
        blob[154] = m;
        blob[155] = r;
        return blob;
    };
    int cicp[4] = { -1, -1, -1, -1 };
    OIIO_CHECK_ASSERT(icc_embedded_cicp(make_cicp_icc(1, 13, 0, 1), cicp));
    OIIO_CHECK_EQUAL(cicp[0], 1);
    OIIO_CHECK_EQUAL(cicp[1], 13);
    OIIO_CHECK_EQUAL(cicp[2], 0);
    OIIO_CHECK_EQUAL(cicp[3], 1);

    // No cicp tag; v2 profile; junk: all false, cicp untouched.
    int untouched[4] = { -1, -1, -1, -1 };
    OIIO_CHECK_ASSERT(!icc_embedded_cicp(make_icc(4, 0), untouched));
    auto v2cicp = make_cicp_icc(1, 13, 0, 1);
    v2cicp[8]   = 2;  // v2: cicpTag is an ICC.1:2022 (v4) construct
    OIIO_CHECK_ASSERT(!icc_embedded_cicp(v2cicp, untouched));
    OIIO_CHECK_ASSERT(!icc_embedded_cicp(junk, untouched));

    // Malformed flavors: wrong tag size, non-zero reserved bytes, range
    // flag > 1, out-of-bounds offset.
    auto badsize = make_cicp_icc(1, 13, 0, 1);
    badsize[143] = 16;  // size != 12
    OIIO_CHECK_ASSERT(!icc_embedded_cicp(badsize, untouched));
    auto badresv = make_cicp_icc(1, 13, 0, 1);
    badresv[148] = 1;  // reserved must be zero
    OIIO_CHECK_ASSERT(!icc_embedded_cicp(badresv, untouched));
    OIIO_CHECK_ASSERT(!icc_embedded_cicp(make_cicp_icc(1, 13, 0, 2),
                                         untouched));  // range > 1
    auto badoffset = make_cicp_icc(1, 13, 0, 1);
    badoffset[139] = 200;  // tag data beyond the blob
    OIIO_CHECK_ASSERT(!icc_embedded_cicp(badoffset, untouched));
    for (int v : untouched)
        OIIO_CHECK_EQUAL(v, -1);
}



// ---------------------------------------------------------------------------
// ICC identification fixtures, built in-memory (port of the proven POC
// fixture generator). The rXYZ/gXYZ/bXYZ colorant matrices are the
// primaries' NPM Bradford-adapted from D65 to the ICC PCS illuminant D50
// (dst = the header illuminant XYZ 0.9642/1.0/0.8249, NOT xy-derived D50),
// which is the exact inverse of the hardcoded D50->D65 adaptation OCIO's
// ICC reader composes in on decode -- so the decoded fixture recovers its
// nominal D65 primaries. The matrix values are precomputed s15Fixed16
// integers; the construction math is not repeated here.
// ---------------------------------------------------------------------------

namespace icc_fixture {

static void
be16(std::vector<uint8_t>& v, uint16_t x)
{
    v.push_back(uint8_t(x >> 8));
    v.push_back(uint8_t(x));
}

static void
be32(std::vector<uint8_t>& v, uint32_t x)
{
    v.push_back(uint8_t(x >> 24));
    v.push_back(uint8_t(x >> 16));
    v.push_back(uint8_t(x >> 8));
    v.push_back(uint8_t(x));
}

static void
tag4(std::vector<uint8_t>& v, const char* sig)
{
    v.insert(v.end(), sig, sig + 4);
}

// 'XYZ ' tag from three s15Fixed16 raw integers.
static std::vector<uint8_t>
xyz_tag(int32_t x, int32_t y, int32_t z)
{
    std::vector<uint8_t> t;
    tag4(t, "XYZ ");
    be32(t, 0);
    be32(t, uint32_t(x));
    be32(t, uint32_t(y));
    be32(t, uint32_t(z));
    return t;
}

// 'curv' tag: gamma (one u8Fixed8 entry) or a full u16 table.
static std::vector<uint8_t>
curv_gamma(float g)
{
    std::vector<uint8_t> t;
    tag4(t, "curv");
    be32(t, 0);
    be32(t, 1);
    be16(t, uint16_t(std::lround(g * 256.0f)));
    return t;
}

static std::vector<uint8_t>
curv_srgb_table(int n = 1024)
{
    std::vector<uint8_t> t;
    tag4(t, "curv");
    be32(t, 0);
    be32(t, uint32_t(n));
    for (int i = 0; i < n; ++i) {
        double x = double(i) / (n - 1);
        double y = x <= 0.04045 ? x / 12.92
                                : std::pow((x + 0.055) / 1.055, 2.4);
        be16(t,
             uint16_t(std::lround(std::min(std::max(y, 0.0), 1.0) * 65535.0)));
    }
    return t;
}

// ICC v2 'desc' (textDescription) tag, ASCII record only.
static std::vector<uint8_t>
desc_tag(const char* text)
{
    std::vector<uint8_t> t;
    tag4(t, "desc");
    be32(t, 0);
    const size_t len = strlen(text) + 1;  // include NUL
    be32(t, uint32_t(len));
    t.insert(t.end(), text, text + len);
    t.insert(t.end(), 8 + 3 + 67, 0);  // unicode + scriptcode + mac records
    return t;
}

// D50 header illuminant / wtpt as s15Fixed16 (0.9642, 1.0, 0.8249).
static const int32_t kD50[3] = { 63190, 65536, 54061 };

// Assemble header + tag table + 4-byte-aligned bodies; profile_size at 0.
static std::vector<uint8_t>
assemble(const std::vector<std::pair<const char*, std::vector<uint8_t>>>& tags)
{
    std::vector<uint8_t> h(128, 0);
    memcpy(h.data() + 4, "oici", 4);                   // CMM
    h[8] = 2, h[9] = 0x40;                             // version 2.4
    memcpy(h.data() + 12, "mntr", 4);                  // device class
    memcpy(h.data() + 16, "RGB ", 4);                  // data color space
    memcpy(h.data() + 20, "XYZ ", 4);                  // PCS
    h[24] = 0x07, h[25] = 0xEA, h[27] = 7, h[29] = 9;  // fixed date
    memcpy(h.data() + 36, "acsp", 4);                  // signature
    std::vector<uint8_t> illum;
    be32(illum, uint32_t(kD50[0]));
    be32(illum, uint32_t(kD50[1]));
    be32(illum, uint32_t(kD50[2]));
    std::copy(illum.begin(), illum.end(), h.begin() + 68);

    const uint32_t n = uint32_t(tags.size());
    uint32_t offset  = 128 + 4 + 12 * n;
    std::vector<uint8_t> table, body;
    be32(table, n);
    for (const auto& [sig, data] : tags) {
        tag4(table, sig);
        be32(table, offset);
        be32(table, uint32_t(data.size()));
        body.insert(body.end(), data.begin(), data.end());
        const size_t pad = (4 - data.size() % 4) % 4;
        body.insert(body.end(), pad, 0);
        offset += uint32_t(data.size() + pad);
    }
    std::vector<uint8_t> blob = std::move(h);
    blob.insert(blob.end(), table.begin(), table.end());
    blob.insert(blob.end(), body.begin(), body.end());
    blob[0] = uint8_t(blob.size() >> 24);
    blob[1] = uint8_t(blob.size() >> 16);
    blob[2] = uint8_t(blob.size() >> 8);
    blob[3] = uint8_t(blob.size());
    return blob;
}

// Standard sRGB: Rec.709 primaries / D65, Bradford-adapted to D50
// (s15Fixed16 columns R,G,B), tabulated sRGB EOTF.
static std::vector<uint8_t>
srgb_profile()
{
    auto trc = curv_srgb_table();
    return assemble({
        { "desc", desc_tag("oiio sRGB v2 fixture") },
        { "rXYZ", xyz_tag(28576, 14581, 912) },
        { "gXYZ", xyz_tag(25239, 46983, 6361) },
        { "bXYZ", xyz_tag(9375, 3972, 46787) },
        { "wtpt", xyz_tag(kD50[0], kD50[1], kD50[2]) },
        { "rTRC", trc },
        { "gTRC", trc },
        { "bTRC", trc },
    });
}

// Decodable but nonstandard: wide-gamut primaries, gamma 1.8 -- matches no
// registry identity.
static std::vector<uint8_t>
wide_profile()
{
    auto trc = curv_gamma(1.8f);
    return assemble({
        { "desc", desc_tag("oiio custom wide-gamut g1.8 fixture") },
        { "rXYZ", xyz_tag(53445, 19596, -193) },
        { "gXYZ", xyz_tag(10487, 47072, 629) },
        { "bXYZ", xyz_tag(-742, -1131, 53624) },
        { "wtpt", xyz_tag(kD50[0], kD50[1], kD50[2]) },
        { "rTRC", trc },
        { "gTRC", trc },
        { "bTRC", trc },
    });
}

// cLUT-only profile: A2B0 (lut8Type), no matrix/TRC tags -> OCIO's
// matrix/TRC reader cannot build a transform and must refuse it.
static std::vector<uint8_t>
clut_profile()
{
    std::vector<uint8_t> a2b;
    tag4(a2b, "mft1");
    be32(a2b, 0);
    a2b.push_back(3);  // in channels
    a2b.push_back(3);  // out channels
    a2b.push_back(2);  // grid points
    a2b.push_back(0);
    for (int v : { 1, 0, 0, 0, 1, 0, 0, 0, 1 })  // identity matrix s15f16
        be32(a2b, uint32_t(v * 65536));
    for (int i = 0; i < 3; ++i)  // input tables
        a2b.insert(a2b.end(), { 0x00, 0xFF });
    for (int r : { 0, 255 })  // 2^3 CLUT grid, 3 outputs
        for (int g : { 0, 255 })
            for (int b : { 0, 255 })
                a2b.insert(a2b.end(), { uint8_t(r), uint8_t(g), uint8_t(b) });
    for (int i = 0; i < 3; ++i)  // output tables
        a2b.insert(a2b.end(), { 0x00, 0xFF });
    return assemble({
        { "desc", desc_tag("oiio cLUT A2B fixture") },
        { "wtpt", xyz_tag(kD50[0], kD50[1], kD50[2]) },
        { "A2B0", a2b },
    });
}

}  // namespace icc_fixture



static void
test_identify_icc()
{
    using OIIO::pvt::icc_profile_identifier;
    using OIIO::pvt::identify_icc_profile;

    if (!ColorConfig::supportsOpenColorIO())
        return;

    Strutil::print("Testing ICC profile identification\n");
    ColorConfig config("ocio://default");

    // Non-ICC bytes: empty id, not decodable (invalid input, not a color
    // answer).
    {
        std::vector<uint8_t> junk(200, 0x42);
        auto r = identify_icc_profile(config, junk);
        OIIO_CHECK_EQUAL(r.id, "");
        OIIO_CHECK_EQUAL(r.decodable, false);
    }

    // Standard sRGB profile: decodes and fingerprint-matches the registry
    // sRGB display identity -- the result must carry srgb_rec709_display
    // semantics (caller-local name or the bare CIID) and must NOT be an
    // "icc:" token (identify-first: no token for a matched profile).
    const auto srgb = icc_fixture::srgb_profile();
    {
        auto r = identify_icc_profile(config, srgb);
        OIIO_CHECK_EQUAL(r.decodable, true);
        OIIO_CHECK_ASSERT(!r.id.empty());
        OIIO_CHECK_ASSERT(!Strutil::starts_with(r.id, "icc:"));
        const bool srgb_semantics
            = r.id == "srgb_rec709_display"
              || OIIO::pvt::derive_color_interop_id(config, r.id)
                     == "srgb_rec709_display";
        OIIO_CHECK_ASSERT(srgb_semantics);
        if (!srgb_semantics)
            Strutil::print("  (identified as '{}')\n", r.id);
    }

    // Decodable but nonstandard profile: no registry identity matches, so
    // the answer is the bare deterministic "icc:<identifier>" token
    // (16-hex XXH64 for a v2 profile). Idempotent across calls.
    const auto wide = icc_fixture::wide_profile();
    {
        const std::string token = "icc:" + icc_profile_identifier(wide);
        OIIO_CHECK_EQUAL(token.size(), 4 + 16);
        auto r = identify_icc_profile(config, wide);
        OIIO_CHECK_EQUAL(r.decodable, true);
        OIIO_CHECK_EQUAL(r.id, token);
        auto again = identify_icc_profile(config, wide);
        OIIO_CHECK_EQUAL(again.id, token);
    }

    // cLUT/AToB profile: structurally ICC but OCIO's matrix/TRC reader
    // refuses it -> bare token, decodable false.
    const auto clut = icc_fixture::clut_profile();
    {
        auto r = identify_icc_profile(config, clut);
        OIIO_CHECK_EQUAL(r.decodable, false);
        OIIO_CHECK_EQUAL(r.id, "icc:" + icc_profile_identifier(clut));
    }

    // Distinct profiles stay distinct across interleaved identifications:
    // the content-unique virtual filename keeps OCIO's process-global file
    // hash cache from handing one profile's processor to another (the
    // classic collision would "decode" the cLUT as the previously-seen
    // sRGB).
    {
        auto r1 = identify_icc_profile(config, srgb);
        auto r2 = identify_icc_profile(config, clut);
        auto r3 = identify_icc_profile(config, srgb);
        OIIO_CHECK_EQUAL(r2.decodable, false);
        OIIO_CHECK_EQUAL(r1.decodable, true);
        OIIO_CHECK_EQUAL(r3.id, r1.id);
        OIIO_CHECK_ASSERT(r1.id != r2.id);
    }
}



static void
test_mastering_volume()
{
    using OIIO::pvt::derive_mastering_volume;
    using OIIO::pvt::MasteringDisplayVolume;

    if (!ColorConfig::supportsOpenColorIO())
        return;
    // The fixture declares interop_id attributes (OCIO >= 2.5) and 2.5
    // builtin styles.
    if (ColorConfig::OpenColorIO_version_hex() < 0x02050000)
        return;

    Strutil::print("Testing mastering display volume derivation\n");

    // Identity 3D LUT for the pure-LUT ODT fixtures.
    std::string lut_path = Filesystem::temp_directory_path()
                           + "/oiio_mdcv_identity.spi3d";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(lut_path,
                                                  "SPILUT 1.0\n"
                                                  "3 3\n"
                                                  "2 2 2\n"
                                                  "0 0 0 0.0 0.0 0.0\n"
                                                  "0 0 1 0.0 0.0 1.0\n"
                                                  "0 1 0 0.0 1.0 0.0\n"
                                                  "0 1 1 0.0 1.0 1.0\n"
                                                  "1 0 0 1.0 0.0 0.0\n"
                                                  "1 0 1 1.0 0.0 1.0\n"
                                                  "1 1 0 1.0 1.0 0.0\n"
                                                  "1 1 1 1.0 1.0 1.0\n"));

    // mDCV fixture (ported from the proven POC): ACES builtin HDR/SDR
    // views, custom RangeTransform views for the probe path, v1-style
    // colorspace-based views with GroupTransform nesting, gamma-2.6
    // theatrical and PQ-DCDM flavors for the cinema-anchor cases, and
    // pure-LUT ODTs (tagged and untagged) for the identity tier.
    static const char* mdcv_yaml = R"(ocio_profile_version: 2.5
name: mdcv-fixture
search_path: .
roles:
  aces_interchange: ACES2065-1
  cie_xyz_d65_interchange: CIE-XYZ-D65
  default: ACES2065-1
  scene_linear: ACES2065-1
file_rules:
  - !<Rule> {name: Default, colorspace: default}
displays:
  Rec2100PQ:
    - !<View> {name: HDR 1000 nit P3 lim, view_transform: HDR-1000-P3lim, display_colorspace: ST2084-P3-D65}
    - !<View> {name: SDR Video, view_transform: SDR-Video, display_colorspace: sRGB - Display}
    - !<View> {name: Custom Clamp SDRish, view_transform: Custom-Clamp-1, display_colorspace: ST2084-P3-D65}
    - !<View> {name: Custom Clamp HDRish, view_transform: Custom-Clamp-10, display_colorspace: ST2084-P3-D65}
  LegacyHDR:
    - !<View> {name: Output HDR Video, colorspace: Output - HDR Video 2020}
  LegacySDR:
    - !<View> {name: Output sRGB, colorspace: Output - SDR Video}
  LegacyCinema:
    - !<View> {name: Output DCI, colorspace: Output - SDR Cinema DCI}
    - !<View> {name: Output D60, colorspace: Output - SDR Cinema D60}
  DCDMPQ:
    - !<View> {name: PQ DCDM Clamp, view_transform: Custom-Clamp-10, display_colorspace: ST2084-DCDM}
  CinemaVT:
    - !<View> {name: DCI VT, view_transform: SDR-Cinema-DCI-VT, display_colorspace: G2.6-P3-DCI}
  LegacyLUT:
    - !<View> {name: Film LUT, colorspace: Output - Film LUT}
    - !<View> {name: Mystery LUT, colorspace: Output - Mystery LUT}
    - !<View> {name: Lin P3DCI LUT, colorspace: Output - Lin P3DCI LUT}
default_view_transform: SDR-Video
view_transforms:
  - !<ViewTransform>
    name: HDR-1000-P3lim
    from_scene_reference: !<BuiltinTransform> {style: ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - HDR-VIDEO-1000nit-15nit-P3lim_1.1}
  - !<ViewTransform>
    name: SDR-Video
    from_scene_reference: !<BuiltinTransform> {style: ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - SDR-VIDEO_1.0}
  - !<ViewTransform>
    name: Custom-Clamp-1
    from_scene_reference: !<RangeTransform> {min_in_value: 0., max_in_value: 1., min_out_value: 0., max_out_value: 1.}
  - !<ViewTransform>
    name: Custom-Clamp-10
    from_scene_reference: !<RangeTransform> {min_in_value: 0., max_in_value: 10., min_out_value: 0., max_out_value: 10.}
  - !<ViewTransform>
    name: SDR-Cinema-DCI-VT
    from_scene_reference: !<BuiltinTransform> {style: ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - SDR-CINEMA-D60sim-DCI_1.0}
display_colorspaces:
  - !<ColorSpace>
    name: CIE-XYZ-D65
    encoding: display-linear
    isdata: false
  - !<ColorSpace>
    name: ST2084-P3-D65
    encoding: hdr-video
    isdata: false
    from_display_reference: !<BuiltinTransform> {style: DISPLAY - CIE-XYZ-D65_to_ST2084-P3-D65}
  - !<ColorSpace>
    name: sRGB - Display
    encoding: sdr-video
    isdata: false
    from_display_reference: !<BuiltinTransform> {style: DISPLAY - CIE-XYZ-D65_to_sRGB}
  - !<ColorSpace>
    name: G2.6-P3-DCI
    encoding: sdr-video
    isdata: false
    from_display_reference: !<BuiltinTransform> {style: DISPLAY - CIE-XYZ-D65_to_G2.6-P3-DCI-BFD}
  - !<ColorSpace>
    name: ST2084-DCDM
    encoding: hdr-video
    isdata: false
    from_display_reference: !<BuiltinTransform> {style: DISPLAY - CIE-XYZ-D65_to_ST2084-DCDM-D65}
colorspaces:
  - !<ColorSpace>
    name: ACES2065-1
    encoding: scene-linear
    isdata: false
  - !<ColorSpace>
    name: Output - HDR Video 2020
    encoding: hdr-video
    isdata: false
    from_scene_reference: !<GroupTransform>
      children:
        - !<BuiltinTransform> {style: ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - HDR-VIDEO-1000nit-15nit-REC2020lim_1.1}
        - !<BuiltinTransform> {style: DISPLAY - CIE-XYZ-D65_to_REC.2100-PQ}
  - !<ColorSpace>
    name: Output - SDR Video
    encoding: sdr-video
    isdata: false
    from_scene_reference: !<GroupTransform>
      children:
        - !<BuiltinTransform> {style: ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - SDR-VIDEO_1.0}
        - !<BuiltinTransform> {style: DISPLAY - CIE-XYZ-D65_to_sRGB}
  - !<ColorSpace>
    name: Output - Film LUT
    encoding: sdr-video
    isdata: false
    interop_id: srgb_rec709_display
    from_scene_reference: !<FileTransform> {src: oiio_mdcv_identity.spi3d, interpolation: best}
  - !<ColorSpace>
    name: Output - Mystery LUT
    encoding: sdr-video
    isdata: false
    from_scene_reference: !<FileTransform> {src: oiio_mdcv_identity.spi3d, interpolation: best}
  - !<ColorSpace>
    name: Output - Lin P3DCI LUT
    encoding: sdr-video
    isdata: false
    interop_id: oiio:lin_p3dci_display
    from_scene_reference: !<FileTransform> {src: oiio_mdcv_identity.spi3d, interpolation: best}
  - !<ColorSpace>
    name: Output - SDR Cinema DCI
    encoding: sdr-video
    isdata: false
    from_scene_reference: !<GroupTransform>
      children:
        - !<BuiltinTransform> {style: ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - SDR-CINEMA-D60sim-DCI_1.0}
        - !<BuiltinTransform> {style: DISPLAY - CIE-XYZ-D65_to_G2.6-P3-DCI-BFD}
  - !<ColorSpace>
    name: Output - SDR Cinema D60
    encoding: sdr-video
    isdata: false
    from_scene_reference: !<GroupTransform>
      children:
        - !<BuiltinTransform> {style: ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - SDR-CINEMA-D60sim-DCI_1.0}
        - !<BuiltinTransform> {style: DISPLAY - CIE-XYZ-D65_to_G2.6-P3-D60-BFD}
)";
    std::string config_path      = Filesystem::temp_directory_path()
                              + "/oiio_mdcv_fixture.ocio";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(config_path, mdcv_yaml));
    ColorConfig config(config_path);
    OIIO_CHECK_ASSERT(!config.has_error());

    static const float kRec709[4][2]  = { { 0.64f, 0.33f },
                                          { 0.30f, 0.60f },
                                          { 0.15f, 0.06f },
                                          { 0.3127f, 0.329f } };
    static const float kP3D65[4][2]   = { { 0.68f, 0.32f },
                                          { 0.265f, 0.69f },
                                          { 0.15f, 0.06f },
                                          { 0.3127f, 0.329f } };
    static const float kRec2020[4][2] = { { 0.708f, 0.292f },
                                          { 0.170f, 0.797f },
                                          { 0.131f, 0.046f },
                                          { 0.3127f, 0.329f } };
    auto check_primaries              = [](const MasteringDisplayVolume& vol,
                              const float want[4][2], float tol) {
        for (int i = 0; i < 4; ++i) {
            OIIO_CHECK_EQUAL_THRESH(vol.primaries[i][0], want[i][0], tol);
            OIIO_CHECK_EQUAL_THRESH(vol.primaries[i][1], want[i][1], tol);
        }
    };

    // Tier 1, view_transform-based: nominal peak + limiting gamut from the
    // ACES-OUTPUT style table.
    {
        MasteringDisplayVolume vol;
        OIIO_CHECK_ASSERT(derive_mastering_volume(config, "Rec2100PQ",
                                                  "HDR 1000 nit P3 lim", vol));
        OIIO_CHECK_EQUAL(vol.max_luminance, 1000.0);
        OIIO_CHECK_EQUAL(vol.min_luminance, 0.0);
        OIIO_CHECK_ASSERT(Strutil::contains(vol.style, "P3lim"));
        check_primaries(vol, kP3D65, 1e-6f);
    }

    // Tier 1, v1-style with the builtin nested inside a GroupTransform.
    {
        MasteringDisplayVolume vol;
        OIIO_CHECK_ASSERT(derive_mastering_volume(config, "LegacyHDR",
                                                  "Output HDR Video", vol));
        OIIO_CHECK_EQUAL(vol.max_luminance, 1000.0);
        OIIO_CHECK_ASSERT(Strutil::contains(vol.style, "REC2020lim"));
        check_primaries(vol, kRec2020, 1e-6f);
    }

    // Tier 1, tokenless SDR-VIDEO: ACES defines it as Rec.709 / 100 nit,
    // in both the view_transform-based and v1-style flavors.
    for (auto&& [d, v] :
         { std::pair<const char*, const char*> { "Rec2100PQ", "SDR Video" },
           { "LegacySDR", "Output sRGB" } }) {
        MasteringDisplayVolume vol;
        OIIO_CHECK_ASSERT(derive_mastering_volume(config, d, v, vol));
        OIIO_CHECK_EQUAL(vol.max_luminance, 100.0);
        OIIO_CHECK_EQUAL(vol.min_luminance, 0.0);
        check_primaries(vol, kRec709, 1e-6f);
    }

    // Tier 2 probe, SDR-ish clamp (XYZ Y=1 -> 100 nits): primaries are the
    // display ENCODING gamut; provenance style is empty (no ACES builtin
    // anywhere in the custom view).
    {
        MasteringDisplayVolume vol;
        OIIO_CHECK_ASSERT(derive_mastering_volume(config, "Rec2100PQ",
                                                  "Custom Clamp SDRish", vol));
        OIIO_CHECK_EQUAL(vol.max_luminance, 100.0);
        OIIO_CHECK_ASSERT(vol.min_luminance < 1e-6);
        OIIO_CHECK_EQUAL(vol.style, "");
        check_primaries(vol, kP3D65, 1e-4f);
    }

    // Tier 2 probe, HDR-ish clamp (Y=10 -> 1000 nits, snapped to nominal).
    {
        MasteringDisplayVolume vol;
        OIIO_CHECK_ASSERT(derive_mastering_volume(config, "Rec2100PQ",
                                                  "Custom Clamp HDRish", vol));
        OIIO_CHECK_EQUAL(vol.max_luminance, 1000.0);
        check_primaries(vol, kP3D65, 1e-4f);
    }

    // Tier 3, v1-style DCI: the ACES style parses no gamut (DCI white has
    // no table entry), so the DISPLAY tail decodes INVERSE. The BFD builtin
    // bakes a Bradford DCI->D65 adaptation (white lands at D65) and the
    // gamma-2.6 family anchors at the 48-nit projector calibration white:
    // the D60sim white sits at Y_rel 0.883 -> ~42.4 cd/m^2, between
    // nominals so reported raw.
    {
        MasteringDisplayVolume vol;
        OIIO_CHECK_ASSERT(
            derive_mastering_volume(config, "LegacyCinema", "Output DCI", vol));
        OIIO_CHECK_EQUAL_THRESH(vol.max_luminance, 0.8828 * 48.0, 0.05);
        OIIO_CHECK_ASSERT(vol.min_luminance < 1e-5);
        OIIO_CHECK_EQUAL(vol.style, "DISPLAY - CIE-XYZ-D65_to_G2.6-P3-DCI-BFD");
        OIIO_CHECK_EQUAL_THRESH(vol.primaries[3][0], 0.3127f, 1e-4f);
        OIIO_CHECK_EQUAL_THRESH(vol.primaries[3][1], 0.329f, 1e-4f);
        OIIO_CHECK_EQUAL_THRESH(vol.primaries[0][0], 0.68f, 0.01f);
    }

    // Tier 3, same shape re-encoded with the G2.6-P3-D60-BFD tail: a
    // 48-nit theatrical encoding whose style carries NO DCI/DCDM token.
    // The anchor is classified from the leading encoding family
    // (G2.6-P3-*), not device-token matching -- the same ~42.4 cd/m^2, not
    // a 2x-overstated 88.3.
    {
        MasteringDisplayVolume vol;
        OIIO_CHECK_ASSERT(
            derive_mastering_volume(config, "LegacyCinema", "Output D60", vol));
        OIIO_CHECK_EQUAL_THRESH(vol.max_luminance, 0.8828 * 48.0, 0.05);
        OIIO_CHECK_EQUAL(vol.style, "DISPLAY - CIE-XYZ-D65_to_G2.6-P3-D60-BFD");
    }

    // Tier 2, PQ in the DCDM XYZ container: pure PQ decodes to absolute
    // nits/100 -- the "DCDM" token must NOT drag it to the 48-nit cinema
    // anchor (which would understate luminance 2.08x). Clamp at Y=10 ->
    // 1000 nits on the nominal; encoding gamut is the raw XYZ container
    // axes with white at illuminant E.
    {
        MasteringDisplayVolume vol;
        OIIO_CHECK_ASSERT(
            derive_mastering_volume(config, "DCDMPQ", "PQ DCDM Clamp", vol));
        OIIO_CHECK_EQUAL(vol.max_luminance, 1000.0);
        OIIO_CHECK_ASSERT(vol.min_luminance < 1e-5);
        OIIO_CHECK_EQUAL_THRESH(vol.primaries[3][0], 1.0f / 3.0f, 1e-4f);
        OIIO_CHECK_EQUAL_THRESH(vol.primaries[3][1], 1.0f / 3.0f, 1e-4f);
    }

    // Tier 2, view_transform-based DCI cinema: the ACES style parses a
    // 48-nit peak but no gamut token, so it falls to the CST probe. Cinema
    // is detected from the display colorspace's structural evidence (its
    // DCI DISPLAY-builtin tail), anchoring at 48: the CST exactly cancels
    // the display colorspace's forward builtin, so the measured peak is
    // the raw ACES SDR-CINEMA-D60sim-DCI output, ~42.375 cd/m^2. The
    // provenance is the unparseable ACES style tier 1 found.
    {
        MasteringDisplayVolume vol;
        OIIO_CHECK_ASSERT(
            derive_mastering_volume(config, "CinemaVT", "DCI VT", vol));
        OIIO_CHECK_EQUAL_THRESH(vol.max_luminance, 42.375443, 1e-3);
        OIIO_CHECK_ASSERT(vol.min_luminance < 1e-5);
        OIIO_CHECK_ASSERT(
            Strutil::contains(vol.style, "SDR-CINEMA-D60sim-DCI"));
        OIIO_CHECK_EQUAL_THRESH(vol.primaries[3][0], 0.3127f, 1e-4f);
        OIIO_CHECK_EQUAL_THRESH(vol.primaries[0][0], 0.68f, 0.01f);
    }

    // Tier 4, pure-LUT ODT tagged with a LINEAR P3DCI display identity:
    // the decode comes from the REGISTRY definition of the id. The 1e5
    // drive saturates the identity LUT to code (1,1,1); decoded as linear
    // P3DCI that is the display white at relative luminance 1.0, and the
    // display-linear + p3dci identity anchors at the 48-nit cinema peak.
    {
        MasteringDisplayVolume vol;
        OIIO_CHECK_ASSERT(
            derive_mastering_volume(config, "LegacyLUT", "Lin P3DCI LUT", vol));
        OIIO_CHECK_EQUAL(vol.max_luminance, 48.0);
        OIIO_CHECK_EQUAL(vol.min_luminance, 0.0);
        OIIO_CHECK_EQUAL(vol.style, "oiio:lin_p3dci_display");
        OIIO_CHECK_EQUAL_THRESH(vol.primaries[3][0], 0.3127f, 1e-4f);
        OIIO_CHECK_EQUAL_THRESH(vol.primaries[0][0], 0.68f, 0.01f);
    }

    // Tier 4, pure-LUT ODT tagged srgb_rec709_display: registry decode,
    // video anchor.
    {
        MasteringDisplayVolume vol;
        OIIO_CHECK_ASSERT(
            derive_mastering_volume(config, "LegacyLUT", "Film LUT", vol));
        OIIO_CHECK_EQUAL(vol.max_luminance, 100.0);
        OIIO_CHECK_EQUAL(vol.min_luminance, 0.0);
        OIIO_CHECK_EQUAL(vol.style, "srgb_rec709_display");
        check_primaries(vol, kRec709, 1e-4f);
    }

    // Tier 5: same LUT with no identity, no DISPLAY tail, no parseable
    // style -- code->nits is underdetermined; honestly no record.
    {
        MasteringDisplayVolume vol;
        OIIO_CHECK_FALSE(
            derive_mastering_volume(config, "LegacyLUT", "Mystery LUT", vol));
    }

    // Defaults: no display/view resolves to the first declared display and
    // its default view (the 1000-nit P3-limited volume).
    {
        MasteringDisplayVolume vol;
        OIIO_CHECK_ASSERT(derive_mastering_volume(config, "", "", vol));
        OIIO_CHECK_EQUAL(vol.max_luminance, 1000.0);
        check_primaries(vol, kP3D65, 1e-6f);
    }

    // Unknown display or view: no record.
    {
        MasteringDisplayVolume vol;
        OIIO_CHECK_FALSE(derive_mastering_volume(config, "NoSuchDisplay",
                                                 "NoSuchView", vol));
        OIIO_CHECK_FALSE(
            derive_mastering_volume(config, "Rec2100PQ", "NoSuchView", vol));
    }
}



static void
test_interop_derive()
{
    using OIIO::pvt::derive_color_interop_id;
    using OIIO::pvt::sanitize_id_token;

    if (!ColorConfig::supportsOpenColorIO())
        return;

    // The `interop_id:` color space attribute (native getInteropID()) needs
    // OCIO >= 2.5; gate the fixtures that declare it.
    const bool has_interop_id_attr = ColorConfig::OpenColorIO_version_hex()
                                     >= 0x02050000;

    // ---- Base fixture (no interop_id attribute -- safe on any linked OCIO
    // version): isData sub-case, registry fingerprint match, no-match falling
    // through to a generated local id, and an unresolvable query. The config
    // name and the generated space's name both carry spaces/mixed case, so a
    // passing local-id assertion also proves both segments are sanitized
    // independently. ------------------------------------------------------
    static const char* named_yaml = R"(ocio_profile_version: 2.1
name: "MyDerive Config"
search_path: ""
roles:
  default: ref
  scene_linear: ref
  aces_interchange: ref
colorspaces:
  - !<ColorSpace>
    name: ref

  - !<ColorSpace>
    name: implicit_data_space
    isdata: true

  - !<ColorSpace>
    name: registry_equivalent_space

  - !<ColorSpace>
    name: My Unmatched Curve
    aliases: [my_unmatched_curve]
    from_scene_reference: !<ExponentTransform> {value: [1.8, 1.8, 1.8, 1]}
)";
    std::string named_path        = Filesystem::temp_directory_path()
                             + "/oiio_color_test_derive_named.ocio";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(named_path, named_yaml));
    {
        ColorConfig cc(named_path);
        OIIO_CHECK_ASSERT(!cc.has_error());

        // Step 1, utility sub-case: a data space with no declared interop_id
        // resolves to "data" -- before any fingerprint tier runs, even though
        // this identity space would ALSO fingerprint-match the registry's
        // lin_ap0_scene identity. (This tier is also part of the cheap public
        // lookup.)
        OIIO_CHECK_EQUAL(derive_color_interop_id(cc, "implicit_data_space"),
                         "data");
        OIIO_CHECK_EQUAL(cc.get_color_interop_id("implicit_data_space"),
                         "data");

        // Step 2: an identity-transform space with no declared id and no
        // registry-precluding classification is genuinely fingerprint-
        // equivalent to the built-in registry's "lin_ap0_scene" identity
        // (also an identity transform) -- returns the REGISTRY identity's own
        // id, not the query's own name. Fingerprinting is derive-only: the
        // cheap public lookup must NOT identify this space.
        OIIO_CHECK_EQUAL(derive_color_interop_id(cc,
                                                 "registry_equivalent_space"),
                         "lin_ap0_scene");
        OIIO_CHECK_EQUAL(cc.get_color_interop_id("registry_equivalent_space"),
                         "");

        // Step 2 miss -> step 3: no registry scene-side entry is a bare
        // gamma-exponent curve, so this space has no fingerprint match; the
        // config has a name and the query resolves to a real space, so a
        // config-local id is generated. Both segments are sanitized
        // independently per the CIF grammar (spaces -> '_', lowercased) --
        // built here via the landed pvt::sanitize_id_token so this assertion
        // exercises the same function the production code calls, rather than
        // a hand-typed guess at its output shape.
        std::string expected_local = sanitize_id_token("MyDerive Config")
                                     + ":local:"
                                     + sanitize_id_token("My Unmatched Curve");
        OIIO_CHECK_EQUAL(derive_color_interop_id(cc, "my_unmatched_curve"),
                         expected_local);
        // Local-id manufacture is derive-only: the cheap public lookup never
        // manufactures an id.
        OIIO_CHECK_EQUAL(cc.get_color_interop_id("my_unmatched_curve"), "");

        // Step 3 precondition: the query itself must resolve to a real space
        // -- a config-local id is never generated for a name this config
        // doesn't know, even though the config has a name.
        OIIO_CHECK_EQUAL(derive_color_interop_id(cc, "no_such_space_at_all"),
                         "");

        // The bare "unknown" token with no backing config space is a
        // cannot-determine: the derivation omits (returns empty), never
        // emits bare "unknown". (The cheap public lookup still answers the
        // literal utility token from the static table.)
        OIIO_CHECK_EQUAL(derive_color_interop_id(cc, "unknown"), "");
        OIIO_CHECK_EQUAL(cc.get_color_interop_id("unknown"), "unknown");
    }
    Filesystem::remove(named_path);

    // ---- Config-declared unknown fixture: a space NAMED "unknown" (with no
    // contradicting declared interop_id) is the config's own declaration of
    // unknownness -- the derivation emits the "ocio:unknown" marker, never
    // bare "unknown". Bare "unknown" on disk is reserved for a user's own
    // explicitly-set colorInteropID attribute, which writers emit verbatim.
    {
        static const char* unknown_yaml = R"(ocio_profile_version: 2.1
name: unknowncfg
search_path: ""
roles:
  default: ref
  scene_linear: ref
  aces_interchange: ref
colorspaces:
  - !<ColorSpace>
    name: ref

  - !<ColorSpace>
    name: unknown
    from_scene_reference: !<ExponentTransform> {value: [2.0, 2.0, 2.0, 1]}
)";
        std::string unknown_path        = Filesystem::temp_directory_path()
                                   + "/oiio_color_test_derive_unknown.ocio";
        OIIO_CHECK_ASSERT(
            Filesystem::write_text_file(unknown_path, unknown_yaml));
        ColorConfig cc(unknown_path);
        OIIO_CHECK_ASSERT(!cc.has_error());
        OIIO_CHECK_EQUAL(derive_color_interop_id(cc, "unknown"),
                         "ocio:unknown");

        // Marker-vs-marker precedence (ADR-0020). THIS config is the shape
        // that used to corrupt the signal: it contains a space literally
        // NAMED "unknown", so resolve()'s CIF strip-leftmost fall-back turned
        // "error:unknown" into bare "unknown", which then hit the
        // config-declared branch above and answered "ocio:unknown" -- a
        // resolution FAILURE silently relabeled as a config DECLARATION.
        // Same corruption for OIIO's own synthetic treatment marker.
        // An incoming marker is terminal: it derives to itself, unchanged.
        OIIO_CHECK_EQUAL(derive_color_interop_id(cc, "error:unknown"),
                         "error:unknown");
        OIIO_CHECK_EQUAL(derive_color_interop_id(cc, "oiio:unknown"),
                         "oiio:unknown");
        OIIO_CHECK_EQUAL(derive_color_interop_id(cc, "ocio:unknown"),
                         "ocio:unknown");
        // Case-insensitive per the marker vocabulary, and canonically spelled
        // on the way out so callers can compare against the literal.
        OIIO_CHECK_EQUAL(derive_color_interop_id(cc, "ERROR:Unknown"),
                         "error:unknown");
        Filesystem::remove(unknown_path);
    }

    // ---- Unnamed-config fixture: the same "no registry match" curve space,
    // but the config has no `name:` set. Step 3 requires a non-empty config
    // name, so this is empty -- and since nothing earlier in the cascade
    // matches either, this doubles as the decisive "total miss returns
    // empty" guard: the legacy static id/CICP table (step 2.5) never fires
    // as a guessed default here, and no other tier steps in to fill the gap.
    // -----------------------------------------------------------------------
    {
        static const char* unnamed_yaml = R"(ocio_profile_version: 2.1
search_path: ""
roles:
  default: ref
  scene_linear: ref
  aces_interchange: ref
colorspaces:
  - !<ColorSpace>
    name: ref

  - !<ColorSpace>
    name: My Unmatched Curve
    aliases: [my_unmatched_curve]
    from_scene_reference: !<ExponentTransform> {value: [1.8, 1.8, 1.8, 1]}
)";
        std::string unnamed_path        = Filesystem::temp_directory_path()
                                   + "/oiio_color_test_derive_unnamed.ocio";
        OIIO_CHECK_ASSERT(
            Filesystem::write_text_file(unnamed_path, unnamed_yaml));
        ColorConfig cc(unnamed_path);
        OIIO_CHECK_ASSERT(!cc.has_error());
        OIIO_CHECK_EQUAL(derive_color_interop_id(cc, "my_unmatched_curve"), "");
        Filesystem::remove(unnamed_path);
    }

    // ---- Sanitizer-collision fixture: two distinct spaces ("Foo Bar" and
    // "foo_bar") whose names sanitize to the SAME token. Serializing a
    // config-local id for either would be ambiguous -- resolution could not
    // uniquely reverse it -- so step 3 must OMIT (never-guess) rather than
    // emit a lossy id, and read-side resolution of the colliding token must
    // refuse to pick a winner. A third, collision-free space proves the
    // guard doesn't disturb the ordinary step-3 path. ----------------------
    {
        static const char* collide_yaml = R"(ocio_profile_version: 2.1
name: collide_cfg
search_path: ""
roles:
  default: ref
  scene_linear: ref
  aces_interchange: ref
colorspaces:
  - !<ColorSpace>
    name: ref

  - !<ColorSpace>
    name: Foo Bar
    from_scene_reference: !<ExponentTransform> {value: [1.7, 1.7, 1.7, 1]}

  - !<ColorSpace>
    name: foo_bar
    from_scene_reference: !<ExponentTransform> {value: [1.9, 1.9, 1.9, 1]}

  - !<ColorSpace>
    name: Solo Space
    from_scene_reference: !<ExponentTransform> {value: [2.1, 2.1, 2.1, 1]}
)";
        std::string collide_path        = Filesystem::temp_directory_path()
                                   + "/oiio_color_test_derive_collide.ocio";
        OIIO_CHECK_ASSERT(
            Filesystem::write_text_file(collide_path, collide_yaml));
        ColorConfig cc(collide_path);
        OIIO_CHECK_ASSERT(!cc.has_error());
        // Both colliding spaces omit -- neither may claim the shared token.
        OIIO_CHECK_EQUAL(derive_color_interop_id(cc, "Foo Bar"), "");
        OIIO_CHECK_EQUAL(derive_color_interop_id(cc, "foo_bar"), "");
        // The collision-free space still gets its config-local id.
        OIIO_CHECK_EQUAL(derive_color_interop_id(cc, "Solo Space"),
                         "collide_cfg:local:solo_space");
        // Read side: the ambiguous token resolves to NEITHER space (the
        // total-miss passthrough), while the unique token still resolves.
        OIIO_CHECK_EQUAL(cc.resolve("collide_cfg:local:foo_bar"),
                         "collide_cfg:local:foo_bar");
        OIIO_CHECK_EQUAL(cc.resolve("collide_cfg:local:solo_space"),
                         "Solo Space");
        Filesystem::remove(collide_path);
    }

    if (has_interop_id_attr) {
        // ---- Declared interop_id precedence: the single most important
        // regression vector for step 1 -- an explicit, author-declared
        // interop_id is unconditionally authoritative, beating even a
        // fingerprint match that a same-shaped identity space would
        // otherwise win at step 2. (Some implementations exercise this vector
        // across a (strict, explicitUnknown) flag matrix; OIIO's
        // get_color_interop_id(string_view) takes no such flags -- there is
        // nothing else to vary -- so one config proves the same precedence.)
        // -------------------------------------------------------------
        static const char* declared_yaml = R"(ocio_profile_version: 2.1
name: gatedcfg
search_path: ""
roles:
  default: ref
  scene_linear: ref
  aces_interchange: ref
colorspaces:
  - !<ColorSpace>
    name: ref

  - !<ColorSpace>
    name: declared_explicit_space
    interop_id: "custom:explicit_id"

  - !<ColorSpace>
    name: declared_unknown_space
    interop_id: "unknown"
)";
        std::string declared_path        = Filesystem::temp_directory_path()
                                    + "/oiio_color_test_derive_declared.ocio";
        OIIO_CHECK_ASSERT(
            Filesystem::write_text_file(declared_path, declared_yaml));
        ColorConfig cc(declared_path);
        OIIO_CHECK_ASSERT(!cc.has_error());
        // Would fingerprint-match "lin_ap0_scene" (identity transform) if the
        // declared attribute didn't win first. The declared tier is shared:
        // both the derive cascade and the cheap public lookup honor it.
        OIIO_CHECK_EQUAL(derive_color_interop_id(cc, "declared_explicit_space"),
                         "custom:explicit_id");
        OIIO_CHECK_EQUAL(cc.get_color_interop_id("declared_explicit_space"),
                         "custom:explicit_id");
        // A declared interop_id of literally "unknown" is the config-side
        // declaration of unknownness: the derivation emits the
        // "ocio:unknown" marker rather than bare "unknown".
        OIIO_CHECK_EQUAL(derive_color_interop_id(cc, "declared_unknown_space"),
                         "ocio:unknown");
        Filesystem::remove(declared_path);
    }
}



// True-cold construction helper: re-exec'd as a fresh subprocess by
// run_bench_phases() below (see comment there for why). Prints
// "construct_ms <value>" and exits -- no other tests run.
static int
bench_child_construct_and_exit()
{
    Timer timer;
    ColorConfig cc("ocio://default");
    std::cout << Strutil::fmt::format("construct_ms {:.6f}\n",
                                      timer() * 1000.0);
    return cc.has_error() ? 1 : 0;
}



// --bench mode: cold/warm phase timings and cardinality counts for the
// color space fingerprint engine, feeding the numbers a design write-up
// needs. Not a pass/fail gate -- prints only, asserts nothing about perf.
// Uses OCIO's built-in default config (ocio:// requires OCIO >= 2.2).
static void
run_bench_phases()
{
    using OIIO::pvt::color_space_analysis_flags;
    using OIIO::pvt::color_space_fingerprint_cache_reset;
    using OIIO::pvt::color_space_fingerprint_cached;
    using OIIO::pvt::color_space_fingerprint_order;

    if (!ColorConfig::supportsOpenColorIO()
        || ColorConfig::OpenColorIO_version_hex() < 0x02020000) {
        std::cout << "--bench: OCIO built-in configs unavailable, skipping.\n";
        return;
    }

    // The same probe name test_color_space_fingerprint() already relies on
    // being present in ocio://default.
    static const char* probe_name = "ACES2065-1";

    // Phase 1: load_ms -- cold ColorConfig construction, before this process
    // has touched any interop machinery. Re-measured at the end (after every
    // phase below has run) to show construction cost stays flat regardless
    // of how much interop work has happened elsewhere in-process -- the
    // "zero construction cost" evidence for the fully-lazy claim.
    Timer t_load;
    ColorConfig cc("ocio://default");
    double load_ms = t_load() * 1000.0;
    if (cc.has_error() || cc.getNumColorSpaces() == 0) {
        std::cout << "--bench: built-in config unavailable, skipping.\n";
        return;
    }
    std::cout << Strutil::fmt::format("load_ms                : {:10.4f}\n",
                                      load_ms);

    // Phase 2: simple_catalog_ms -- the first classification query for any
    // name triggers the config-wide "simple color space" catalog scan
    // (cached thereafter). This is the cold-classify number.
    Timer t_catalog;
    color_space_analysis_flags(cc, probe_name);
    double simple_catalog_ms = t_catalog() * 1000.0;
    std::cout << Strutil::fmt::format("simple_catalog_ms      : {:10.4f}\n",
                                      simple_catalog_ms);

    // Phase 3: cold_resolve_ms / warm_resolve_ms -- first-vs-second
    // fingerprint-cache lookup for one name.
    color_space_fingerprint_cache_reset();
    Timer t_cold_resolve;
    color_space_fingerprint_cached(cc, probe_name);
    double cold_resolve_ms = t_cold_resolve() * 1000.0;
    Timer t_warm_resolve;
    color_space_fingerprint_cached(cc, probe_name);
    double warm_resolve_ms = t_warm_resolve() * 1000.0;
    std::cout << Strutil::fmt::format("cold_resolve_ms        : {:10.4f}\n",
                                      cold_resolve_ms);
    std::cout << Strutil::fmt::format("warm_resolve_ms        : {:10.4f}\n",
                                      warm_resolve_ms);

    // Phase 4: fingerprint_vector_ms -- the bulk all-simple-spaces
    // fingerprint pass (uncached; the classification catalog is already
    // warm from phase 2, so this isolates fingerprint compute cost).
    Timer t_vector;
    std::vector<std::string> order = color_space_fingerprint_order(cc);
    double fingerprint_vector_ms   = t_vector() * 1000.0;
    std::cout << Strutil::fmt::format(
        "fingerprint_vector_ms  : {:10.4f}  (simple_candidate_count={}, "
        "fingerprint_vector_count={})\n",
        fingerprint_vector_ms, order.size(), order.size());

    // Re-measure load_ms after all the interop machinery above has run, to
    // show it hasn't grown.
    Timer t_load_after;
    ColorConfig cc_after("ocio://default");
    double load_ms_after = t_load_after() * 1000.0;
    (void)cc_after;
    std::cout << Strutil::fmt::format(
        "load_ms (after interop): {:10.4f}  (baseline load_ms={:.4f})\n",
        load_ms_after, load_ms);

    // True-cold construction: a same-process measurement understates the
    // fully-lazy claim, since process-level OCIO/OS caches (file reads,
    // etc.) persist across ColorConfig instances within this same run --
    // spawn a fresh subprocess per config so construct_ms isn't
    // contaminated by that.
    std::string cmd = "\"" + Sysutil::this_program_path()
                      + "\" --bench-child-construct";
#ifdef _MSC_VER
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    double subprocess_construct_ms = -1.0;
    if (pipe) {
        char line[256];
        if (fgets(line, sizeof(line), pipe))
            sscanf(line, "construct_ms %lf", &subprocess_construct_ms);
#ifdef _MSC_VER
        _pclose(pipe);
#else
        pclose(pipe);
#endif
    }
    std::cout << Strutil::fmt::format(
        "construct_ms (true cold, subprocess): {:10.4f}\n",
        subprocess_construct_ms);
}



// Exercise the built-in color interop ID <-> CICP table via the public
// ColorConfig API (the table itself is a private, static array in
// color_ocio.cpp, so it can only be reached through get_color_interop_id()
// and get_cicp()).
static void
test_color_interop_ids()
{
    const ColorConfig& cc = ColorConfig::default_colorconfig();

    // A representative sample of built-in interop IDs should resolve to
    // themselves (case-insensitively) and, where the table records a CICP
    // correspondence, get_cicp() should return the expected 4 values.
    OIIO_CHECK_EQUAL(cc.get_color_interop_id("srgb_rec709_scene"),
                     "srgb_rec709_scene");
    OIIO_CHECK_EQUAL(cc.get_color_interop_id("SRGB_REC709_SCENE"),
                     "srgb_rec709_scene");
    OIIO_CHECK_EQUAL(cc.get_color_interop_id("lin_ap1_scene"), "lin_ap1_scene");
    OIIO_CHECK_EQUAL(cc.get_color_interop_id("g24_rec709_display"),
                     "g24_rec709_display");
    OIIO_CHECK_EQUAL(cc.get_color_interop_id("data"), "data");
    OIIO_CHECK_EQUAL(cc.get_color_interop_id("unknown"), "unknown");

    // Unknown names (and the empty string) return an empty interop ID.
    OIIO_CHECK_EQUAL(cc.get_color_interop_id("not_a_real_interop_id"), "");
    OIIO_CHECK_EQUAL(cc.get_color_interop_id(""), "");

    // Entries that carry a CICP mapping: primaries (cicp[0]) and transfer
    // (cicp[1]) round-trip through get_cicp() / get_color_interop_id(cicp).
    cspan<int> cicp = cc.get_cicp("srgb_rec709_scene");
    OIIO_CHECK_EQUAL(cicp.size(), 4);
    if (cicp.size() == 4) {
        OIIO_CHECK_EQUAL(cicp[0], 1);   // CICPPrimaries::Rec709
        OIIO_CHECK_EQUAL(cicp[1], 13);  // CICPTransfer::sRGB
        OIIO_CHECK_EQUAL(cicp[2], 1);   // CICPMatrix::BT709
        // The reverse lookup matches on (primaries, transfer) alone, and
        // srgb_rec709_display is listed ahead of srgb_rec709_scene, so the
        // shared Rec709/sRGB tuple resolves display-referred.
        OIIO_CHECK_EQUAL(cc.get_color_interop_id(cicp.data()),
                         "srgb_rec709_display");
    }

    // Entries with no CICP mapping (e.g. AP1/AP0 scene-linear, "data",
    // "unknown") return an empty span from get_cicp().
    OIIO_CHECK_EQUAL(cc.get_cicp("lin_ap1_scene").size(), 0);
    OIIO_CHECK_EQUAL(cc.get_cicp("data").size(), 0);
    OIIO_CHECK_EQUAL(cc.get_cicp("unknown").size(), 0);

    // get_color_interop_id(cicp) picks the first table match by
    // (primaries, transfer) alone -- matrix and range are not part of the
    // lookup key. srgb_rec709_display is listed ahead of srgb_rec709_scene,
    // so the shared Rec709/sRGB tuple resolves display-referred.
    const int rec709_srgb_cicp[4] = { 1, 13, 1, 1 };
    OIIO_CHECK_EQUAL(cc.get_color_interop_id(rec709_srgb_cicp),
                     "srgb_rec709_display");
}



// The CICP tuple with Rec.709 primaries (1) and the IEC 61966-2-1 / sRGB
// transfer (13) describes display-referred sRGB. The reverse CICP -> interop-ID
// lookup (matched on primaries + transfer) must therefore resolve it to the
// display-referred identity, not the scene-referred one.
static void
test_cicp_interop_id()
{
    ColorConfig config;
    const int cicp_srgb[4] = { 1, 13, 0, 1 };
    OIIO_CHECK_EQUAL(config.get_color_interop_id(cicp_srgb),
                     string_view("srgb_rec709_display"));
    // Forward mapping round-trips to a Rec.709 / sRGB tuple.
    cspan<int> cicp = config.get_cicp("srgb_rec709_display");
    OIIO_CHECK_ASSERT(cicp.size() >= 2 && cicp[0] == 1 && cicp[1] == 13);
}



// Regression guard: the internal copy_config() must preserve a config's
// explicit default view transform name across an editable copy. OCIO < 2.3.1's
// createEditableCopy() drops it; without the restore, interopify_config's
// display bridge would see no default view transform and synthesize a
// scene_to_display_bridge that shadows a config's own. The pvt probe runs the
// real copy_config() on a two-view-transform config whose explicit default is
// the non-first one (so a dropped default is observable, not masked by OCIO's
// first-VT implicit default). Passes on all OCIO versions -- natively on
// >= 2.3.1, via the workaround below it.
static void
test_copy_config_default_view_transform()
{
    OIIO_CHECK_ASSERT(
        OIIO::pvt::copy_config_preserves_default_view_transform());
}



// Concurrency stress for the process-global color caches. Many threads race
// the SHARED lazy state -- the registry fingerprint index (a C++11 magic-static
// built once on first registry-equivalence resolve), the flyweight
// ColorSpaceFingerprint cache, and the interopify structural memo -- through
// resolve(), get_color_interop_id(), equivalent(), and the pvt fingerprint
// cache. Runs FIRST in main() so the caches are genuinely cold and the workers'
// opening iterations are the true first touch (the prime suspect for a race).
// Under ThreadSanitizer this section is the payload; without TSan it is a cheap
// smoke test that the shared caches survive concurrent use. All threads spin on
// a start gate so they hit the cold caches simultaneously.
static void
test_thread_stress()
{
    using OIIO::pvt::color_space_fingerprint_cache_reset;
    using OIIO::pvt::color_space_fingerprint_cached;
    using OIIO::pvt::ColorSpaceFingerprint;

    if (!ColorConfig::supportsOpenColorIO()
        || ColorConfig::OpenColorIO_version_hex() < 0x02020000)
        return;

    // cc1: built-in ACES config (registry-rich). cc2: a small hand-built
    // config. Both constructed here, single-threaded -- the workers race the
    // shared process-global caches, not per-config construction.
    ColorConfig cc1("ocio://default");
    if (cc1.has_error())
        return;  // built-in config unavailable on this OCIO build

    static const char* cfg2_yaml = R"(ocio_profile_version: 2.1
search_path: ""
roles:
  default: ref
  scene_linear: ref
  aces_interchange: ref
displays:
  disp:
    - !<View> {name: main, colorspace: ref}
colorspaces:
  - !<ColorSpace>
    name: ref

  - !<ColorSpace>
    name: gamma22
    from_scene_reference: !<ExponentTransform> {value: [2.2, 2.2, 2.2, 1]}

  - !<ColorSpace>
    name: doubler
    from_scene_reference: !<MatrixTransform> {matrix: [2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1]}
)";
    std::string cfg2_path        = Filesystem::temp_directory_path()
                            + "/oiio_color_test_stress2.ocio";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(cfg2_path, cfg2_yaml));
    ColorConfig cc2(cfg2_path);
    OIIO_CHECK_ASSERT(!cc2.has_error());

    // cc3: an interoperable scene+display config that LACKS every registry CIID
    // the workers request, so a createColorProcessor with a registry endpoint
    // can never resolve locally and MUST take the cross-config bridge. It carries
    // aces_interchange (scene bridge) and a P3-D65 display space but NO
    // cie_xyz_d65 interchange (interopify must synthesize the display interchange).
    // Sharing ONE cc3 across all workers is deliberate: it races both the
    // per-Impl ensure_interop() lazy publish AND the process-global interopify_config
    // memo (s_memo, first-writer-wins, keyed by structural config id) on their
    // genuinely cold first touch -- the state the prior stress predates. Gated on
    // OCIO >= 2.3 (two-config display bridge) and registry CIID availability.
    const bool do_xconfig = ColorConfig::OpenColorIO_version_hex() >= 0x02030000
                            && OIIO::pvt::interop_identities_config_resolves(
                                "lin_ap1_scene")
                            && OIIO::pvt::interop_identities_config_resolves(
                                "srgb_rec709_display");
    static const char* cfg3_yaml = R"(ocio_profile_version: 2.1
strictparsing: false
search_path: ""
roles:
  default: ACEScg
  scene_linear: ACEScg
  aces_interchange: ACES2065-1
displays:
  P3:
    - !<View> {name: Raw, colorspace: my_p3_display}
colorspaces:
  - !<ColorSpace>
    name: ACES2065-1
    encoding: scene-linear
  - !<ColorSpace>
    name: ACEScg
    encoding: scene-linear
    to_scene_reference: !<MatrixTransform> {matrix: [0.6954522414, 0.1406786965, 0.1638690622, 0, 0.0447945634, 0.8596711185, 0.0955343182, 0, -0.0055258826, 0.0040252103, 1.0015006723, 0, 0, 0, 0, 1]}
display_colorspaces:
  - !<ColorSpace>
    name: my_p3_display
    encoding: sdr-video
    from_display_reference: !<GroupTransform>
      children:
        - !<MatrixTransform> {matrix: [2.49349691194143, -0.931383617919124, -0.402710784450717, 0, -0.829488969561575, 1.76266406031835, 0.0236246858419436, 0, 0.0358458302437845, -0.0761723892680418, 0.956884524007688, 0, 0, 0, 0, 1]}
        - !<ExponentTransform> {value: 2.2, style: mirror, direction: inverse}
)";
    std::string cfg3_path        = Filesystem::temp_directory_path()
                            + "/oiio_color_test_stress3.ocio";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(cfg3_path, cfg3_yaml));
    ColorConfig cc3(cfg3_path);
    OIIO_CHECK_ASSERT(!cc3.has_error());

    std::vector<std::string> names1 = cc1.getColorSpaceNames();
    std::vector<std::string> names2 = cc2.getColorSpaceNames();

    // CIID strings that drive resolve()'s registry-equivalence tier; constant
    // regardless of config, so resolve() reaches the registry fingerprint index.
    static const char* ciids[] = {
        "lin_ap1_scene", "srgb_rec709_scene",   "g24_rec709_display",
        "data",          "srgb_rec709_display", "unknown",
    };

    // Cold start: nothing has warmed the shared caches. The workers are the
    // first touch.
    color_space_fingerprint_cache_reset();

    const int nthreads = 12;
    const int iters    = 300;
    std::atomic<int> ready { 0 };
    std::atomic<bool> go { false };

    auto worker = [&](int tid) {
        ready.fetch_add(1);
        while (!go.load())
            std::this_thread::yield();  // all workers hit the cold caches together
        for (int it = 0; it < iters; ++it) {
            const ColorConfig& cc                 = (tid & 1) ? cc2 : cc1;
            const std::vector<std::string>& names = (tid & 1) ? names2 : names1;

            // resolve() -- registry-equivalence tier -> registry index bootstrap
            for (const char* id : ciids)
                (void)cc.resolve(id);

            if (!names.empty()) {
                const std::string& nm = names[(size_t)(it + tid) % names.size()];
                // fingerprint + registry + interopify memo
                (void)cc.get_color_interop_id(nm);
                // flyweight fingerprint cache: first-insert / publish / hit
                ColorSpaceFingerprint fp = color_space_fingerprint_cached(cc,
                                                                          nm);
                (void)fp;
            }

            // equivalent() -- each side is resolve()d first
            if (names.size() >= 2)
                (void)cc.equivalent(names[0], names[1]);

            // cross-config: two distinct configs pushing the same shared index
            (void)cc1.get_color_interop_id("ACEScg");
            (void)cc2.get_color_interop_id("doubler");

            // Cross-config createColorProcessor bridge on the SHARED cc3: cold
            // first touch of ensure_interop() + the process-global interopify
            // memo + bootstrap_display_interchange, hit concurrently by every
            // worker. Both scene CIIDs (via aces_interchange) and a display CIID
            // (via the synthesized display interchange) exercise reconcile_cross_
            // config / reconcile_cross_config_display. Handles are discarded; the
            // point is the shared construction/caching, not the transform.
            if (do_xconfig) {
                (void)cc3.createColorProcessor("ACEScg", "lin_ap1_scene");
                (void)cc3.createColorProcessor("lin_ap0_scene", "ACEScg");
                (void)cc3.createColorProcessor("srgb_rec709_display", "ACEScg");
                (void)cc3.createColorProcessor("srgb_rec709_display",
                                               "my_p3_display");
            }
        }
    };

    std::vector<std::thread> pool;
    pool.reserve(nthreads);
    for (int t = 0; t < nthreads; ++t)
        pool.emplace_back(worker, t);
    while (ready.load() < nthreads)
        std::this_thread::yield();
    go.store(true);
    for (auto& th : pool)
        th.join();

    // Reaching here without a TSan report, deadlock, or crash is the pass.
    OIIO_CHECK_ASSERT(true);
    Filesystem::remove(cfg2_path);
    Filesystem::remove(cfg3_path);
}



// The internal cheap characterization facade: ColorSpaceInfo +
// ColorConfig::get_color_space_info (scalar and batch). Field semantics
// (direct facts computed; derivable facts uncomputed with no cached data;
// range never guessed), the error convention, batch order/duplicates, and
// the never-derives guarantee (no fingerprint work, no cache publication).
static void
test_color_space_info()
{
    using OIIO::pvt::characterization_cache_reset;
    using OIIO::pvt::characterization_cache_size;
    using OIIO::pvt::color_space_fingerprint_cache_size;
    using F = ColorSpaceInfoField;

    // A default-constructed info is the one honest "nothing" object -- no
    // OCIO needed for that check.
    {
        ColorSpaceInfo none;
        OIIO_CHECK_FALSE(none.valid());
        OIIO_CHECK_EQUAL(none.name(), "");
        OIIO_CHECK_EQUAL(none.encoding(), "");
        OIIO_CHECK_ASSERT(none.chromaticities().empty());
        OIIO_CHECK_EQUAL(int(none.transfer_function_kind()),
                         int(ColorTransferFunctionKind::Undetermined));
        OIIO_CHECK_FALSE(none.computed(F::ImageState));
        OIIO_CHECK_FALSE(none.available(F::ImageState));
        OIIO_CHECK_FALSE(none.derived(F::ImageState));
    }

    if (!ColorConfig::supportsOpenColorIO())
        return;

    // Fixture: a scene space named for a static-table interop id (the cheap
    // table tier identifies it with no interop_id attribute, so this is
    // OCIO-version-independent), a data space, a display space, and a space
    // with no authored facts at all.
    static const char* config_yaml = R"(ocio_profile_version: 2.1
name: infocfg
search_path: ""
roles:
  default: ref
  scene_linear: ref
displays:
  disp:
    - !<View> {name: main, colorspace: screen}
display_colorspaces:
  - !<ColorSpace>
    name: screen
    encoding: sdr-video
    from_display_reference: !<ExponentTransform> {value: [2.4, 2.4, 2.4, 1]}
colorspaces:
  - !<ColorSpace>
    name: ref

  - !<ColorSpace>
    name: srgb_rec709_scene
    aliases: [my_srgb]
    encoding: sdr-video
    from_scene_reference: !<ExponentTransform> {value: [2.2, 2.2, 2.2, 1]}

  - !<ColorSpace>
    name: rawdata
    isdata: true

  - !<ColorSpace>
    name: plain_space
    from_scene_reference: !<ExponentTransform> {value: [1.8, 1.8, 1.8, 1]}
)";
    std::string config_path        = Filesystem::temp_directory_path()
                              + "/oiio_color_test_info.ocio";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(config_path, config_yaml));
    ColorConfig cc(config_path);
    OIIO_CHECK_ASSERT(!cc.has_error());

    characterization_cache_reset();
    const size_t fp_cache_before = color_space_fingerprint_cache_size();

    // Direct facts of a fully described scene space, queried by alias (the
    // record reports the canonical name).
    {
        ColorSpaceInfo info = cc.get_color_space_info("my_srgb");
        OIIO_CHECK_ASSERT(info.valid());
        OIIO_CHECK_ASSERT(!cc.has_error());
        OIIO_CHECK_EQUAL(info.name(), "srgb_rec709_scene");
        OIIO_CHECK_ASSERT(info.computed(F::ImageState)
                          && info.available(F::ImageState));
        OIIO_CHECK_EQUAL(info.image_state(), "scene");
        OIIO_CHECK_ASSERT(info.computed(F::ColorInteropID)
                          && info.available(F::ColorInteropID));
        OIIO_CHECK_EQUAL(info.color_interop_id(), "srgb_rec709_scene");
        OIIO_CHECK_ASSERT(info.computed(F::Encoding)
                          && info.available(F::Encoding));
        OIIO_CHECK_EQUAL(info.encoding(), "sdr-video");
        // Direct facts are direct, not behavioral derivations.
        OIIO_CHECK_FALSE(info.derived(F::ImageState));
        OIIO_CHECK_FALSE(info.derived(F::ColorInteropID));
        OIIO_CHECK_FALSE(info.derived(F::Encoding));
        // Range: attempted, but nothing registers one -- a stable negative,
        // never a guessed "full".
        OIIO_CHECK_ASSERT(info.computed(F::Range));
        OIIO_CHECK_FALSE(info.available(F::Range));
        OIIO_CHECK_EQUAL(info.range(), "");
        // Derivable fields with no cached derived data: not attempted (the
        // cheap getter never silently derives).
        OIIO_CHECK_FALSE(info.computed(F::EqualityID));
        OIIO_CHECK_FALSE(info.computed(F::Chromaticities));
        OIIO_CHECK_FALSE(info.computed(F::TransferFunction));
        OIIO_CHECK_ASSERT(info.chromaticities().empty());
        OIIO_CHECK_EQUAL(int(info.transfer_function_kind()),
                         int(ColorTransferFunctionKind::Undetermined));
    }

    // A display space reports state "display"; a data space's state is
    // honestly undetermined and its cheap id is the "data" utility token; a
    // space with no authored facts has an unavailable id and encoding.
    {
        ColorSpaceInfo screen = cc.get_color_space_info("screen");
        OIIO_CHECK_ASSERT(screen.valid());
        OIIO_CHECK_EQUAL(screen.image_state(), "display");

        ColorSpaceInfo data = cc.get_color_space_info("rawdata");
        OIIO_CHECK_ASSERT(data.valid());
        OIIO_CHECK_ASSERT(data.computed(F::ImageState));
        OIIO_CHECK_FALSE(data.available(F::ImageState));
        OIIO_CHECK_EQUAL(data.color_interop_id(), "data");

        ColorSpaceInfo plain = cc.get_color_space_info("plain_space");
        OIIO_CHECK_ASSERT(plain.valid());
        OIIO_CHECK_ASSERT(plain.computed(F::ColorInteropID));
        OIIO_CHECK_FALSE(plain.available(F::ColorInteropID));
        OIIO_CHECK_ASSERT(plain.computed(F::Encoding));
        OIIO_CHECK_FALSE(plain.available(F::Encoding));
    }

    // Error convention, scalar: unknown name -> invalid record + the
    // ColorConfig error state.
    {
        ColorSpaceInfo bad = cc.get_color_space_info("no_such_space_xyzzy");
        OIIO_CHECK_FALSE(bad.valid());
        OIIO_CHECK_ASSERT(cc.has_error());
        std::string err = cc.geterror();
        OIIO_CHECK_ASSERT(Strutil::contains(err, "unknown color space")
                          && Strutil::contains(err, "no_such_space_xyzzy"));
    }

    // Batch: input order and duplicates preserved, one record per input.
    {
        std::vector<std::string> names { "plain_space", "rawdata", "my_srgb",
                                         "plain_space" };
        std::vector<ColorSpaceInfo> infos = cc.get_color_space_infos(names);
        OIIO_CHECK_ASSERT(!cc.has_error());
        OIIO_CHECK_EQUAL(infos.size(), 4);
        if (infos.size() == 4) {
            OIIO_CHECK_EQUAL(infos[0].name(), "plain_space");
            OIIO_CHECK_EQUAL(infos[1].name(), "rawdata");
            OIIO_CHECK_EQUAL(infos[2].name(), "srgb_rec709_scene");
            OIIO_CHECK_EQUAL(infos[3].name(), "plain_space");
        }
        // An empty input span is an empty batch, not "all spaces".
        OIIO_CHECK_ASSERT(
            cc.get_color_space_infos(cspan<std::string>()).empty());
        OIIO_CHECK_ASSERT(!cc.has_error());
    }

    // Error convention, batch: any invalid input fails the whole batch with
    // one INDEXED error and an empty result.
    {
        std::vector<std::string> names { "ref", "bogus_name", "plain_space" };
        std::vector<ColorSpaceInfo> infos = cc.get_color_space_infos(names);
        OIIO_CHECK_ASSERT(infos.empty());
        OIIO_CHECK_ASSERT(cc.has_error());
        std::string err = cc.geterror();
        OIIO_CHECK_ASSERT(Strutil::contains(err, "get_color_space_infos[1]")
                          && Strutil::contains(err, "bogus_name"));
    }

    // The never-derives guarantee: none of the calls above woke the
    // fingerprint engine or published a characterization record.
    OIIO_CHECK_EQUAL(color_space_fingerprint_cache_size(), fp_cache_before);
    OIIO_CHECK_EQUAL(characterization_cache_size(), 0);

    Filesystem::remove(config_path);
}



// The internal field-selective characterization engine
// (pvt::characterize_color_space) and its cache: full derivation on request,
// publication of successful AND negative attempts, cache merge into later
// cheap getters, no-retry of settled fields, and immutable snapshots.
static void
test_characterize_color_space()
{
    using OIIO::pvt::characterization_cache_reset;
    using OIIO::pvt::characterization_cache_size;
    using OIIO::pvt::characterize_color_space;
    using CF = OIIO::pvt::CharacterizationField;
    using F  = ColorSpaceInfoField;

    if (!ColorConfig::supportsOpenColorIO())
        return;
    // ocio:// built-in configs require OCIO >= 2.2.
    if (ColorConfig::OpenColorIO_version_hex() < 0x02020000)
        return;
    ColorConfig cc("ocio://default");
    if (cc.has_error() || cc.getNumColorSpaces() == 0)
        return;  // built-in configs unavailable in this OCIO build

    characterization_cache_reset();

    // Snapshot the cheap view before any derivation.
    ColorSpaceInfo before = cc.get_color_space_info("sRGB - Texture");
    OIIO_CHECK_ASSERT(before.valid());
    OIIO_CHECK_FALSE(before.computed(F::EqualityID));
    OIIO_CHECK_FALSE(before.computed(F::Chromaticities));
    OIIO_CHECK_FALSE(before.computed(F::TransferFunction));
    // Cheap gets never publish.
    OIIO_CHECK_EQUAL(characterization_cache_size(), 0);

    // Full derivation through the engine: every field attempted.
    auto rec = characterize_color_space(cc, "sRGB - Texture", CF::All);
    OIIO_CHECK_ASSERT(rec.valid());
    // Equality identity by fingerprint equivalence against the registry.
    OIIO_CHECK_ASSERT(rec.computed(CF::EqualityID));
    OIIO_CHECK_ASSERT(rec.available(CF::EqualityID)
                      && rec.derived(CF::EqualityID));
    OIIO_CHECK_EQUAL(rec.equality_id, "srgb_rec709_scene");
    // Chromaticities: 8 floats, RGBW xy.
    OIIO_CHECK_ASSERT(rec.available(CF::Chromaticities));
    OIIO_CHECK_EQUAL(rec.chromaticities.size(), 8);
    // Transfer: a recognized named family.
    OIIO_CHECK_ASSERT(rec.available(CF::TransferFunction));
    OIIO_CHECK_EQUAL(int(rec.transfer_kind),
                     int(ColorTransferFunctionKind::Named));
    OIIO_CHECK_EQUAL(rec.transfer_function, "srgb");
    // The attempt was published.
    OIIO_CHECK_EQUAL(characterization_cache_size(), 1);

    // A later cheap get merges the cached derived facts...
    ColorSpaceInfo after = cc.get_color_space_info("sRGB - Texture");
    OIIO_CHECK_ASSERT(after.available(F::EqualityID)
                      && after.derived(F::EqualityID));
    OIIO_CHECK_EQUAL(after.equality_id(), "srgb_rec709_scene");
    OIIO_CHECK_ASSERT(after.available(F::Chromaticities));
    OIIO_CHECK_EQUAL(after.chromaticities().size(), 8);
    OIIO_CHECK_EQUAL(after.transfer_function(), "srgb");
    // ...and the merge itself published nothing new.
    OIIO_CHECK_EQUAL(characterization_cache_size(), 1);
    // Previously returned snapshots are immutable: the pre-derive object
    // still reports its fields unattempted.
    OIIO_CHECK_FALSE(before.computed(F::EqualityID));
    OIIO_CHECK_FALSE(before.computed(F::TransferFunction));

    // Negative results are results: a data space derives no equality id,
    // chromaticities, or transfer -- computed but unavailable -- and the
    // negative record is cached so the space is not re-probed every query.
    if (cc.getColorSpaceIndex("Raw") >= 0) {
        auto draw = characterize_color_space(cc, "Raw", CF::All);
        OIIO_CHECK_ASSERT(draw.valid());
        OIIO_CHECK_EQUAL(draw.color_interop_id, "data");
        OIIO_CHECK_ASSERT(draw.computed(CF::EqualityID));
        OIIO_CHECK_FALSE(draw.available(CF::EqualityID));
        OIIO_CHECK_ASSERT(draw.computed(CF::Chromaticities));
        OIIO_CHECK_FALSE(draw.available(CF::Chromaticities));
        OIIO_CHECK_ASSERT(draw.computed(CF::TransferFunction));
        OIIO_CHECK_FALSE(draw.available(CF::TransferFunction));
        const size_t published = characterization_cache_size();
        // Settled fields (positive or negative) are not retried: a repeat
        // full characterization adds no cache entries.
        auto again = characterize_color_space(cc, "Raw", CF::All);
        OIIO_CHECK_EQUAL(characterization_cache_size(), published);
        OIIO_CHECK_ASSERT(again.computed(CF::Chromaticities));
        OIIO_CHECK_FALSE(again.available(CF::Chromaticities));
        // The cheap getter sees the cached negative as computed-but-
        // unavailable -- a stable answer, not an error.
        ColorSpaceInfo raw = cc.get_color_space_info("Raw");
        OIIO_CHECK_ASSERT(raw.computed(F::Chromaticities));
        OIIO_CHECK_FALSE(raw.available(F::Chromaticities));
        OIIO_CHECK_ASSERT(!cc.has_error());
    }

    characterization_cache_reset();
}



// The internal derive facade: ColorConfig::derive_color_space_info (scalar and
// batch). Complete-record semantics on an uncharacterizable space
// (computed-but-unavailable, never an error), range never guessed even under
// derive, batch order/duplicates and the indexed error, cache publication
// observable through the cheap getter, and two-context derive isolation.
static void
test_derive_color_space_info()
{
    using OIIO::pvt::characterization_cache_reset;
    using F = ColorSpaceInfoField;

    if (!ColorConfig::supportsOpenColorIO())
        return;

    characterization_cache_reset();

    // --- A NON-interoperable config (no aces_interchange): probes cannot
    // run, so derive produces stable negatives -- computed but unavailable
    // -- on a valid record, with no error.
    {
        static const char* config_yaml = R"(ocio_profile_version: 2.1
name: derivecfg
search_path: ""
roles:
  default: ref
  scene_linear: ref
displays:
  disp:
    - !<View> {name: main, colorspace: ref}
colorspaces:
  - !<ColorSpace>
    name: ref

  - !<ColorSpace>
    name: srgb_rec709_scene
    encoding: sdr-video
    from_scene_reference: !<ExponentTransform> {value: [2.2, 2.2, 2.2, 1]}

  - !<ColorSpace>
    name: plain_space
    from_scene_reference: !<ExponentTransform> {value: [1.8, 1.8, 1.8, 1]}
)";
        std::string config_path        = Filesystem::temp_directory_path()
                                  + "/oiio_color_test_derive.ocio";
        OIIO_CHECK_ASSERT(
            Filesystem::write_text_file(config_path, config_yaml));
        ColorConfig cc(config_path);
        OIIO_CHECK_ASSERT(!cc.has_error());

        // The uncharacterizable space: every field attempted, the derivable
        // ones honestly unavailable. That is a complete record, not an
        // error.
        ColorSpaceInfo plain = cc.derive_color_space_info("plain_space");
        OIIO_CHECK_ASSERT(plain.valid());
        OIIO_CHECK_ASSERT(!cc.has_error());
        for (F f :
             { F::EqualityID, F::ColorInteropID, F::Encoding, F::ImageState,
               F::Range, F::Chromaticities, F::TransferFunction })
            OIIO_CHECK_ASSERT(plain.computed(f));
        OIIO_CHECK_FALSE(plain.available(F::EqualityID));
        OIIO_CHECK_FALSE(plain.available(F::Chromaticities));
        OIIO_CHECK_FALSE(plain.available(F::TransferFunction));
        // Range is never guessed, derive path included: nothing registers a
        // genuine per-space range, so the full attempt is a stable negative.
        OIIO_CHECK_FALSE(plain.available(F::Range));
        OIIO_CHECK_EQUAL(plain.range(), "");

        // A table-identified space still derives its reserved-table
        // chromaticities without any probe (registry association, not a
        // guess); the transfer probe itself stays unavailable here.
        ColorSpaceInfo srgb = cc.derive_color_space_info("srgb_rec709_scene");
        OIIO_CHECK_ASSERT(srgb.valid());
        OIIO_CHECK_EQUAL(srgb.color_interop_id(), "srgb_rec709_scene");
        OIIO_CHECK_ASSERT(srgb.available(F::Chromaticities)
                          && srgb.derived(F::Chromaticities));
        OIIO_CHECK_EQUAL(srgb.chromaticities().size(), 8);
        OIIO_CHECK_ASSERT(srgb.computed(F::TransferFunction));

        // Batch: order and duplicates preserved; per-field failure is not a
        // batch failure.
        {
            std::vector<std::string> names { "plain_space", "srgb_rec709_scene",
                                             "plain_space" };
            std::vector<ColorSpaceInfo> infos = cc.derive_color_space_infos(
                names);
            OIIO_CHECK_ASSERT(!cc.has_error());
            OIIO_CHECK_EQUAL(infos.size(), 3);
            if (infos.size() == 3) {
                OIIO_CHECK_EQUAL(infos[0].name(), "plain_space");
                OIIO_CHECK_EQUAL(infos[1].name(), "srgb_rec709_scene");
                OIIO_CHECK_EQUAL(infos[2].name(), "plain_space");
            }
            OIIO_CHECK_ASSERT(
                cc.derive_color_space_infos(cspan<std::string>()).empty());
            OIIO_CHECK_ASSERT(!cc.has_error());
        }

        // Batch error: validate-all-first, one indexed error, empty result.
        {
            std::vector<std::string> names { "ref", "bogus_name",
                                             "plain_space" };
            std::vector<ColorSpaceInfo> infos = cc.derive_color_space_infos(
                names);
            OIIO_CHECK_ASSERT(infos.empty());
            OIIO_CHECK_ASSERT(cc.has_error());
            std::string err = cc.geterror();
            OIIO_CHECK_ASSERT(
                Strutil::contains(err, "derive_color_space_infos[1]")
                && Strutil::contains(err, "bogus_name"));
        }

        // Scalar error convention.
        {
            ColorSpaceInfo bad = cc.derive_color_space_info("no_such_space");
            OIIO_CHECK_FALSE(bad.valid());
            OIIO_CHECK_ASSERT(cc.has_error());
            std::string err = cc.geterror();
            OIIO_CHECK_ASSERT(Strutil::contains(err, "derive_color_space_info")
                              && Strutil::contains(err, "no_such_space"));
        }

        Filesystem::remove(config_path);
    }

    // --- Cache publication is observable through the PUBLIC surface: a
    // derive fills the fields, a later cheap get sees them (the cheap
    // getter itself still derives nothing).
    if (ColorConfig::OpenColorIO_version_hex() >= 0x02020000) {
        ColorConfig cc("ocio://default");
        if (!cc.has_error() && cc.getNumColorSpaces() > 0) {
            characterization_cache_reset();
            ColorSpaceInfo cheap = cc.get_color_space_info("sRGB - Texture");
            OIIO_CHECK_ASSERT(cheap.valid());
            OIIO_CHECK_FALSE(cheap.computed(F::EqualityID));

            ColorSpaceInfo full = cc.derive_color_space_info("sRGB - Texture");
            OIIO_CHECK_ASSERT(full.valid());
            OIIO_CHECK_ASSERT(full.available(F::EqualityID)
                              && full.derived(F::EqualityID));
            OIIO_CHECK_EQUAL(full.equality_id(), "srgb_rec709_scene");
            OIIO_CHECK_ASSERT(full.available(F::Chromaticities));
            OIIO_CHECK_EQUAL(full.transfer_function(), "srgb");
            // Complete record, range still honestly absent.
            OIIO_CHECK_ASSERT(full.computed(F::Range));
            OIIO_CHECK_FALSE(full.available(F::Range));

            ColorSpaceInfo seen = cc.get_color_space_info("sRGB - Texture");
            OIIO_CHECK_ASSERT(seen.available(F::EqualityID));
            OIIO_CHECK_EQUAL(seen.equality_id(), "srgb_rec709_scene");
            OIIO_CHECK_EQUAL(seen.transfer_function(), "srgb");
            // The earlier cheap snapshot is immutable.
            OIIO_CHECK_FALSE(cheap.computed(F::EqualityID));
        }
    }

    // --- Two-context derive isolation: a context-sensitive space derived
    // under context A publishes into A's bucket only; a cheap get under
    // context B sees none of it. (The interchange role + the OCIO context
    // API used here need OCIO >= 2.2.)
    if (ColorConfig::OpenColorIO_version_hex() >= 0x02020000) {
        static const char* ctx_yaml = R"(ocio_profile_version: 2.1
environment:
  CTX_CS: ref
search_path: ""
roles:
  default: ref
  scene_linear: ref
  aces_interchange: ref
displays:
  disp:
    - !<View> {name: main, colorspace: ref}
colorspaces:
  - !<ColorSpace>
    name: ref

  - !<ColorSpace>
    name: gamma_a
    from_scene_reference: !<ExponentTransform> {value: [2.2, 2.2, 2.2, 1]}

  - !<ColorSpace>
    name: ctx_space
    to_scene_reference: !<ColorSpaceTransform> {src: $CTX_CS, dst: ref}
)";
        std::string ctx_path        = Filesystem::temp_directory_path()
                               + "/oiio_color_test_derive_ctx.ocio";
        OIIO_CHECK_ASSERT(Filesystem::write_text_file(ctx_path, ctx_yaml));
        ColorConfig cc(ctx_path);
        OIIO_CHECK_ASSERT(!cc.has_error());
        characterization_cache_reset();

        ColorSpaceInfoOptions ctx_ident, ctx_gamma;
        ctx_ident.context = { { "CTX_CS", "ref" } };
        ctx_gamma.context = { { "CTX_CS", "gamma_a" } };

        // Under the identity context the space measures linear; under the
        // gamma context it does not. Each derivation runs under its own
        // per-call context.
        ColorSpaceInfo ident = cc.derive_color_space_info("ctx_space",
                                                          ctx_ident);
        OIIO_CHECK_ASSERT(ident.valid());
        OIIO_CHECK_ASSERT(ident.available(F::TransferFunction));
        OIIO_CHECK_EQUAL(int(ident.transfer_function_kind()),
                         int(ColorTransferFunctionKind::Linear));

        ColorSpaceInfo gamma = cc.derive_color_space_info("ctx_space",
                                                          ctx_gamma);
        OIIO_CHECK_ASSERT(gamma.valid());
        OIIO_CHECK_ASSERT(gamma.available(F::TransferFunction));
        OIIO_CHECK_ASSERT(int(gamma.transfer_function_kind())
                          != int(ColorTransferFunctionKind::Linear));

        // Isolation: the cached facts are context-bucketed. A cheap get
        // under each context sees exactly its own derivation.
        ColorSpaceInfo cheap_ident = cc.get_color_space_info("ctx_space",
                                                             ctx_ident);
        OIIO_CHECK_ASSERT(cheap_ident.computed(F::TransferFunction));
        OIIO_CHECK_EQUAL(int(cheap_ident.transfer_function_kind()),
                         int(ColorTransferFunctionKind::Linear));
        ColorSpaceInfo cheap_gamma = cc.get_color_space_info("ctx_space",
                                                             ctx_gamma);
        OIIO_CHECK_ASSERT(cheap_gamma.computed(F::TransferFunction));
        OIIO_CHECK_ASSERT(int(cheap_gamma.transfer_function_kind())
                          != int(ColorTransferFunctionKind::Linear));

        characterization_cache_reset();
        Filesystem::remove(ctx_path);
    }

    characterization_cache_reset();
}



// Search/engine convergence: find_color_spaces' per-candidate
// characterization consumes the same shared field-selective cache the
// public derive verbs publish into. A repeat search is cache-only and
// result-identical; a prior complete derive of every space leaves search
// cache-only; results never change.
static void
test_search_engine_coupling()
{
    using OIIO::pvt::characterization_cache_reset;
    using OIIO::pvt::characterization_cache_size;

    if (!ColorConfig::supportsOpenColorIO())
        return;
    if (ColorConfig::OpenColorIO_version_hex() < 0x02020000)
        return;

    // Interoperable, deliberately unnamed config (no generated-local id
    // tier): a table-identified 2.2-gamma space, an unidentified 2.2-gamma
    // twin, and the linear anchor.
    static const char* config_yaml = R"(ocio_profile_version: 2.1
search_path: ""
roles:
  default: ref
  scene_linear: ref
  aces_interchange: ref
displays:
  disp:
    - !<View> {name: main, colorspace: ref}
colorspaces:
  - !<ColorSpace>
    name: ref

  - !<ColorSpace>
    name: srgb_rec709_scene
    encoding: sdr-video
    from_scene_reference: !<ExponentTransform> {value: [2.2, 2.2, 2.2, 1]}

  - !<ColorSpace>
    name: gamma_a
    from_scene_reference: !<ExponentTransform> {value: [2.2, 2.2, 2.2, 1]}
)";
    std::string config_path        = Filesystem::temp_directory_path()
                              + "/oiio_color_test_searchconv.ocio";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(config_path, config_yaml));
    ColorConfig cc(config_path);
    OIIO_CHECK_ASSERT(!cc.has_error());

    characterization_cache_reset();

    OIIO::pvt::FindColorSpacesOptions o;
    o.transfer_functions = { "srgb_rec709_scene" };
    o.chromaticities     = { "srgb_rec709_scene" };

    const auto r1 = OIIO::pvt::find_color_spaces(cc, o);
    OIIO_CHECK_ASSERT(std::find(r1.begin(), r1.end(), "srgb_rec709_scene")
                      != r1.end());
    const size_t published = characterization_cache_size();
    // The walk published its per-candidate characterizations (partial
    // records: only the requested axes).
    OIIO_CHECK_ASSERT(published > 0);

    // Repeat search: cache-only (settled fields are never re-derived, so
    // nothing new publishes) and result-identical.
    const auto r2 = OIIO::pvt::find_color_spaces(cc, o);
    OIIO_CHECK_ASSERT(r2 == r1);
    OIIO_CHECK_EQUAL(characterization_cache_size(), published);

    // A prior COMPLETE derive of every space makes a fresh search
    // cache-only too -- and the results are identical to the from-scratch
    // walk.
    characterization_cache_reset();
    const auto all = cc.getColorSpaceNames();
    OIIO_CHECK_ASSERT(!cc.derive_color_space_infos(all).empty());
    const size_t post_derive = characterization_cache_size();
    OIIO_CHECK_ASSERT(post_derive > 0);
    const auto r3 = OIIO::pvt::find_color_spaces(cc, o);
    OIIO_CHECK_ASSERT(r3 == r1);
    OIIO_CHECK_EQUAL(characterization_cache_size(), post_derive);

    characterization_cache_reset();
    Filesystem::remove(config_path);
}



// ---------------------------------------------------------------------------
// 3.2 config-utility bundle (serialize / from_text / archive / evolve /
// debug info / caches / scoped context).
// ---------------------------------------------------------------------------

// A small config whose reference space is positively identifiable as a scene
// interchange (named "ACES2065-1"), so the in-memory interoperability repair
// binds the aces_interchange role -- observable through serialize().
static const char* utility_config_yaml = R"(ocio_profile_version: 2.1
search_path: ""
roles:
  default: ACES2065-1
  scene_linear: ACES2065-1
displays:
  disp:
    - !<View> {name: main, colorspace: ACES2065-1}
colorspaces:
  - !<ColorSpace>
    name: ACES2065-1

  - !<ColorSpace>
    name: gamma22
    from_scene_reference: !<ExponentTransform> {value: [2.2, 2.2, 2.2, 1]}
)";


static void
test_config_serialize()
{
    if (!ColorConfig::supportsOpenColorIO())
        return;

    std::string config_path = Filesystem::temp_directory_path()
                              + "/oiio_color_test_serialize.ocio";
    OIIO_CHECK_ASSERT(
        Filesystem::write_text_file(config_path, utility_config_yaml));

    ColorConfig cc(config_path);
    OIIO_CHECK_ASSERT(!cc.has_error());

    std::string text = cc.serialize();
    OIIO_CHECK_ASSERT(!cc.has_error());
    OIIO_CHECK_ASSERT(Strutil::starts_with(text, "ocio_profile_version"));
    OIIO_CHECK_ASSERT(text.find("ACES2065-1") != std::string::npos);
    // The authored config has no aces_interchange role...
    OIIO_CHECK_ASSERT(text.find("aces_interchange") == std::string::npos);
    // ...but the interopified in-memory copy's serialization shows the
    // repair: evidence of what is in memory, not what was on disk.
    ColorConfig::SerializeOptions iopts;
    iopts.interopified   = true;
    std::string repaired = cc.serialize(iopts);
    OIIO_CHECK_ASSERT(!cc.has_error());
    OIIO_CHECK_ASSERT(repaired.find("aces_interchange") != std::string::npos);
    // Serialization is deterministic.
    OIIO_CHECK_EQUAL(text, cc.serialize());

    Filesystem::remove(config_path);
}



static void
test_config_from_text()
{
    if (!ColorConfig::supportsOpenColorIO())
        return;

    // A config constructed from memory matches the same config loaded from
    // a file.
    ColorConfig cc = ColorConfig::from_text(utility_config_yaml);
    OIIO_CHECK_ASSERT(!cc.has_error());
    OIIO_CHECK_EQUAL(cc.getNumColorSpaces(), 2);
    OIIO_CHECK_EQUAL(cc.getColorSpaceIndex("gamma22"), 1);
    OIIO_CHECK_ASSERT(Strutil::starts_with(cc.configname(), "text:"));

    // serialize() -> from_text() round-trips the config.
    ColorConfig rt = ColorConfig::from_text(cc.serialize());
    OIIO_CHECK_ASSERT(!rt.has_error());
    OIIO_CHECK_EQUAL(rt.getColorSpaceNames(), cc.getColorSpaceNames());
    OIIO_CHECK_EQUAL(rt.serialize(), cc.serialize());

    // The result is movable (the factory relies on it).
    ColorConfig moved = std::move(rt);
    OIIO_CHECK_EQUAL(moved.getNumColorSpaces(), 2);

    // Unparsable text is an error, not a throw. Like the file constructor
    // handed a bad path, the failed config still carries the built-in
    // minimal inventory fallback.
    ColorConfig bad = ColorConfig::from_text("this is not an OCIO config");
    OIIO_CHECK_ASSERT(bad.has_error());
    std::string errmsg = bad.geterror();
    OIIO_CHECK_ASSERT(
        Strutil::contains(errmsg, "Error reading OCIO config from text"));
    OIIO_CHECK_EQUAL(bad.getColorSpaceIndex("gamma22"), -1);
}



static void
test_config_archive()
{
    if (!ColorConfig::supportsOpenColorIO())
        return;

    std::string wd = Filesystem::temp_directory_path()
                     + "/oiio_color_test_archive_wd";
    // A crashed prior run can leave wd behind; start from a clean slate so
    // the create below is meaningful either way.
    Filesystem::remove_all(wd);
    OIIO_CHECK_ASSERT(Filesystem::create_directory(wd));
    std::string arc = Filesystem::temp_directory_path()
                      + "/oiio_color_test_archive.ocioz";

    // A config with no working directory is not archivable (OCIO must know
    // where to gather candidate LUT files from)...
    ColorConfig cc = ColorConfig::from_text(utility_config_yaml);
    OIIO_CHECK_FALSE(cc.archive(arc));
    OIIO_CHECK_ASSERT(cc.has_error());
    OIIO_CHECK_ASSERT(Strutil::contains(cc.geterror(), "not archivable"));

    // ...supplying one per-call makes the archive succeed.
    ColorConfig::ArchiveOptions aopts;
    aopts.working_dir = wd;
    OIIO_CHECK_ASSERT(cc.archive(arc, aopts));
    OIIO_CHECK_FALSE(cc.has_error());
    OIIO_CHECK_ASSERT(Filesystem::exists(arc));
    // .ocioz archives are zip containers ("PK" magic).
    char magic[2] = { 0, 0 };
    OIIO_CHECK_EQUAL(Filesystem::read_bytes(arc, magic, 2), size_t(2));
    OIIO_CHECK_ASSERT(magic[0] == 'P' && magic[1] == 'K');

    // The archive is itself a loadable config, equivalent to the original.
    ColorConfig back(arc);
    OIIO_CHECK_ASSERT(!back.has_error());
    OIIO_CHECK_EQUAL(back.getColorSpaceNames(), cc.getColorSpaceNames());

    // A from_text config constructed WITH a working directory is archivable
    // with default options.
    ColorConfig cc2 = ColorConfig::from_text(utility_config_yaml, wd);
    OIIO_CHECK_ASSERT(cc2.archive(arc));
    OIIO_CHECK_FALSE(cc2.has_error());

    Filesystem::remove(arc);
    Filesystem::remove(wd);
}



static void
test_config_evolve()
{
    if (!ColorConfig::supportsOpenColorIO())
        return;

    ColorConfig cc = ColorConfig::from_text(utility_config_yaml);
    OIIO_CHECK_ASSERT(!cc.has_error());
    const std::string original_text = cc.serialize();

    // A default evolve is a plain, independent copy (ColorConfig itself is
    // non-copyable; evolve() IS the copy mechanism), marked by provenance.
    ColorConfig copy = cc.evolve();
    OIIO_CHECK_ASSERT(!copy.has_error());
    OIIO_CHECK_EQUAL(copy.serialize(), original_text);
    OIIO_CHECK_ASSERT(Strutil::ends_with(copy.configname(), "#evolved"));

    // Context-variable overrides serialize with the evolved config (they
    // change its structural cache identity); the source is untouched.
    ColorConfig::EvolveOptions copts;
    copts.context      = { { "SHOT", "sh010" } };
    ColorConfig ctxcfg = cc.evolve(copts);
    OIIO_CHECK_ASSERT(!ctxcfg.has_error());
    OIIO_CHECK_ASSERT(ctxcfg.serialize().find("SHOT") != std::string::npos);
    OIIO_CHECK_ASSERT(original_text.find("SHOT") == std::string::npos);
    OIIO_CHECK_EQUAL(cc.serialize(), original_text);

    // An evolve chain doesn't stack provenance suffixes...
    ColorConfig chain = ctxcfg.evolve();
    OIIO_CHECK_ASSERT(Strutil::ends_with(chain.configname(), "#evolved"));
    OIIO_CHECK_ASSERT(
        !Strutil::ends_with(chain.configname(), "#evolved#evolved"));
    // ...and reset returns to the ORIGINAL root, dropping prior overrides.
    ColorConfig::EvolveOptions ropts;
    ropts.reset      = true;
    ColorConfig back = ctxcfg.evolve(ropts);
    OIIO_CHECK_ASSERT(!back.has_error());
    OIIO_CHECK_EQUAL(back.serialize(), original_text);

    // working_dir re-points runtime file resolution: it turns this
    // unarchivable (no working directory) config archivable.
    std::string wd = Filesystem::temp_directory_path()
                     + "/oiio_color_test_evolve_wd";
    OIIO_CHECK_ASSERT(Filesystem::create_directory(wd));
    std::string arc = Filesystem::temp_directory_path()
                      + "/oiio_color_test_evolve.ocioz";
    ColorConfig::EvolveOptions wopts;
    wopts.working_dir = wd;
    ColorConfig wdcfg = cc.evolve(wopts);
    OIIO_CHECK_ASSERT(!wdcfg.has_error());
    OIIO_CHECK_ASSERT(!cc.archive(arc));  // source still has no working dir
    OIIO_CHECK_ASSERT(cc.has_error() && cc.geterror().size());
    OIIO_CHECK_ASSERT(wdcfg.archive(arc));
    OIIO_CHECK_FALSE(wdcfg.has_error());
    Filesystem::remove(arc);
    Filesystem::remove(wd);
}



static void
test_config_debug_info()
{
    if (!ColorConfig::supportsOpenColorIO())
        return;

    ColorConfig cc            = ColorConfig::from_text(utility_config_yaml);
    ColorConfigDebugInfo info = cc.get_debug_info();
    OIIO_CHECK_FALSE(cc.has_error());
    // Identity: versions, config name, registry data version.
    OIIO_CHECK_ASSERT(info.oiio_version.size());
    OIIO_CHECK_ASSERT(info.ocio_version.size());
    OIIO_CHECK_EQUAL(info.config_name, cc.configname());
    OIIO_CHECK_ASSERT(Strutil::contains(info.registry_data_version,
                                        "interop-identities-config"));
    OIIO_CHECK_ASSERT(info.cache_entries.size());
    // Reporting is lazy: a fresh config's interchange discovery is pending,
    // and get_debug_info itself must not have triggered it.
    OIIO_CHECK_ASSERT(info.interchange_state == ColorInterchangeState::Pending);
    OIIO_CHECK_ASSERT(info.interchange_name.empty());
    OIIO_CHECK_ASSERT(cc.get_debug_info().interchange_state
                      == ColorInterchangeState::Pending);
    // to_string() renders the same paste-able report. (The exact formatting
    // is documented as unstable; assert only stable tokens.)
    std::string report = info.to_string();
    OIIO_CHECK_ASSERT(Strutil::contains(report, "OpenImageIO"));
    OIIO_CHECK_ASSERT(Strutil::contains(report, "OpenColorIO"));
    OIIO_CHECK_ASSERT(Strutil::contains(report, cc.configname()));
    OIIO_CHECK_ASSERT(Strutil::contains(report, "interop-identities-config"));
    OIIO_CHECK_ASSERT(Strutil::contains(report, "pending"));
    // Once a query runs the discovery, the result is reported.
    ColorConfig::SerializeOptions iopts;
    iopts.interopified = true;
    (void)cc.serialize(iopts);
    info = cc.get_debug_info();
    OIIO_CHECK_ASSERT(info.interchange_state
                      == ColorInterchangeState::Interoperable);
    OIIO_CHECK_ASSERT(Strutil::contains(info.interchange_name, "ACES2065-1"));
    OIIO_CHECK_ASSERT(Strutil::contains(info.to_string(), "ACES2065-1"));
}



static void
test_config_clear_caches()
{
    using OIIO::pvt::characterization_cache_size;
    using OIIO::pvt::color_space_fingerprint_cache_size;
    using OIIO::pvt::color_space_fingerprint_cached;

    if (!ColorConfig::supportsOpenColorIO())
        return;

    // A structurally UNIQUE config (an extra space no other test's config
    // declares), so the process-global entries this test creates -- and
    // clear_caches() then erases -- are provably its own.
    std::string yaml = std::string(utility_config_yaml)
                       + "  - !<ColorSpace>\n    name: clear_caches_space\n";
    ColorConfig cc = ColorConfig::from_text(yaml);
    OIIO_CHECK_ASSERT(!cc.has_error());

    const size_t fp_base   = color_space_fingerprint_cache_size();
    const size_t char_base = characterization_cache_size();

    // Warm this config's per-instance processor cache and its entries in
    // the process-global fingerprint and characterization caches.
    auto proc = cc.createColorProcessor("gamma22", "ACES2065-1");
    OIIO_CHECK_ASSERT(proc);
    auto fp = color_space_fingerprint_cached(cc, "gamma22");
    OIIO_CHECK_ASSERT(fp.computed());
    OIIO_CHECK_ASSERT(color_space_fingerprint_cache_size() > fp_base);
    // (The derive path -- the cheap get_color_space_info tier reads the
    // shared cache but only derivation publishes into it.)
    (void)cc.derive_color_space_info("gamma22");
    OIIO_CHECK_ASSERT(characterization_cache_size() > char_base);

    cc.clear_caches();
    OIIO_CHECK_FALSE(cc.has_error());
    // Only THIS config's process-global entries were dropped: the totals
    // return to the pre-warm baseline, other configs' entries survive.
    OIIO_CHECK_EQUAL(color_space_fingerprint_cache_size(), fp_base);
    OIIO_CHECK_EQUAL(characterization_cache_size(), char_base);
    // Clearing is semantics-free: everything repopulates on demand.
    proc = cc.createColorProcessor("gamma22", "ACES2065-1");
    OIIO_CHECK_ASSERT(proc);
    auto fp2 = color_space_fingerprint_cached(cc, "gamma22");
    OIIO_CHECK_ASSERT(fp2.computed());
    OIIO_CHECK_ASSERT(fp2.values == fp.values);
}



int
main(int argc, char* argv[])
{
#if !defined(NDEBUG) || defined(OIIO_CI) || defined(OIIO_CODE_COVERAGE)
    // For the sake of test time, reduce the default iterations for DEBUG,
    // CI, and code coverage builds. Explicit use of --iters or --trials
    // will override this, since it comes before the getargs() call.
    iterations /= 10;
    ntrials = 1;
#endif

    getargs(argc, argv);

    // Internal subprocess entry point for run_bench_phases()'s true-cold
    // construction measurement -- runs nothing else.
    if (bench_child)
        return bench_child_construct_and_exit();

    // First, while the process-global color caches are still cold, so the
    // worker threads race the one-time lazy init.
    test_thread_stress();

    test_sRGB_conversion();
    test_Rec709_conversion();
    test_interop_identities_config();
    test_interop_id_grammar();
    test_registry_invariants();
    test_registry_round_trip();
    test_builtin_interop_ids_sync();
    test_legacy_table_registry_sync();
    test_color_space_classification();
    test_color_space_fingerprint();
    test_transfer_curve_discrimination();
    test_config_interoperability();
    test_cross_config_processor();
    test_cross_config_conversion();
    test_cross_config_display_ciid_convert();
    test_cross_config_display_interchange();
    test_cross_config_display();
    test_color_space_fingerprint_cache();
    test_interop_resolve();
    test_icc_utils();
    test_identify_icc();
    test_mastering_volume();
    test_interop_derive();
    test_color_space_info();
    test_characterize_color_space();
    test_derive_color_space_info();
    test_search_engine_coupling();
    test_copy_config_default_view_transform();
    test_config_serialize();
    test_config_from_text();
    test_config_archive();
    test_config_evolve();
    test_config_debug_info();
    test_config_clear_caches();

    // --bench is opt-in and heavy; the default `ctest -R unit_color` run
    // never sets it.
    if (bench_mode)
        run_bench_phases();
    test_color_interop_ids();
    test_cicp_interop_id();

    return unit_test_failures != 0;
}
