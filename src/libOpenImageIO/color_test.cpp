// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <unordered_set>
#include <vector>

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/benchmark.h>
#include <OpenImageIO/color.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/simd.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/timer.h>
#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/unittest.h>

#include "imageio_pvt.h"


using namespace OIIO;
using namespace simd;


// Aid for things that are too short to benchmark accurately
#define REP10(x) x, x, x, x, x, x, x, x, x, x

static int iterations = 1000000;
static int ntrials    = 5;
static bool verbose   = false;



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
        OIIO_CHECK_EQUAL((int)parts.form,
                          (int)InteropIdForm::OUTER_INNER_BASE);
        OIIO_CHECK_EQUAL(parts.outer, "show1-config");
        OIIO_CHECK_EQUAL(parts.inner, "local");
        OIIO_CHECK_EQUAL(parts.base, "srgb");
    }

    {
        auto parts = parse_interop_id("my-studio::srgb");
        OIIO_CHECK_ASSERT(is_valid_interop_id("my-studio::srgb"));
        OIIO_CHECK_EQUAL((int)parts.form,
                          (int)InteropIdForm::OUTER_BLANK_BASE);
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
    OIIO_CHECK_FALSE(is_valid_interop_id("caf\xc3\xa9"));  // "café"
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
    OIIO_CHECK_EQUAL(sanitize_id_token("a\xe4\xb8\xad" "b"), "a^b");

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
    std::unordered_set<std::string> declared_names(names.begin(),
                                                    names.end());
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

    // Cross-check against the CIF wiki's published Color Interop IDs:
    // https://github.com/AcademySoftwareFoundation/ColorInterop/wiki/Registered-Color-Interop-IDs
    // A representative subset of the published IDs; extend to the full
    // list if it is ever vendored.
    static const char* published_ids[] = {
        "lin_ap0_scene",       "lin_rec709_scene",     "lin_p3d65_scene",
        "lin_rec2020_scene",   "lin_adobergb_scene",   "srgb_rec709_display",
        "g24_rec709_display",  "g22_rec709_display",   "lin_rec709_display",
        "lin_p3d65_display",   "lin_p3d60_display",    // oiio: alias, bare
        "lin_ciexyzd65_display",                       // ocio: alias, bare
        "data",
    };
    for (const char* id : published_ids)
        OIIO_CHECK_ASSERT(interop_identities_config_resolves(id));
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

    test_sRGB_conversion();
    test_Rec709_conversion();
    test_interop_identities_config();
    test_interop_id_grammar();
    test_registry_invariants();
    test_color_space_classification();
    test_color_space_fingerprint();

    return unit_test_failures != 0;
}
