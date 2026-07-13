// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <unordered_set>
#include <vector>

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/benchmark.h>
#include <OpenImageIO/color.h>
#include <OpenImageIO/filesystem.h>
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



// Exercise the config interoperability check: a config carrying the
// aces_interchange role is interoperable and does not warn; a stripped config
// is not interoperable, gets an in-memory interopified repair copy that DOES
// resolve a scene interchange (with the OCIO processor cache off, leaving the
// original config unmutated), and warns exactly once per config structure. The
// whole thing is lazy -- constructing a ColorConfig runs none of it.
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
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(stripped_path, stripped_yaml));

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

        // But the in-memory interopified copy was repaired to resolve one,
        // with the processor cache off -- and the original config is
        // unmutated (is_interoperable stays false above).
        OIIO_CHECK_ASSERT(
            color_config_interopified_resolves_scene_interchange(cc));
        OIIO_CHECK_ASSERT(color_config_interopified_cache_off(cc));

        // It warned exactly once: this instance emitted the warning, and a
        // second query is silent (the lazy gate ran the bootstrap only once).
        OIIO_CHECK_ASSERT(color_config_interop_warned(cc));
        OIIO_CHECK_FALSE(color_config_is_interoperable(cc));
        OIIO_CHECK_ASSERT(color_config_interop_warned(cc));

        // A second ColorConfig over the same (structurally identical) config
        // does not warn again: the once-per-config-structure guard is
        // process-global.
        ColorConfig cc2(stripped_path);
        OIIO_CHECK_FALSE(color_config_is_interoperable(cc2));
        OIIO_CHECK_FALSE(color_config_interop_warned(cc2));
        // ...yet it still gets its own repaired, cache-off copy.
        OIIO_CHECK_ASSERT(
            color_config_interopified_resolves_scene_interchange(cc2));
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



// Exercise the cross-config conversion route in ColorConfig::createColorProcessor
// (decision 3): when a requested color space is absent from the current config
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

    // --- Zero behavior change: a name this config defines still resolves -------
    {
        ColorConfig cc(interop_path);
        auto handle = cc.createColorProcessor("ap0", "ap0");
        OIIO_CHECK_ASSERT(handle.get() != nullptr);  // local no-op, unchanged
        OIIO_CHECK_FALSE(cc.has_error());
    }

    // --- Gate respects interop state: a non-interoperable config does not
    //     bridge a registry-known name (no real cross-config transform) --------
    {
        ColorConfig cc(non_path);
        OIIO_CHECK_ASSERT(!cc.has_error());
        // Confirm the fixture is non-interoperable and its repair is unusable.
        OIIO_CHECK_FALSE(OIIO::pvt::color_config_is_interoperable(cc));
        OIIO_CHECK_FALSE(
            OIIO::pvt::color_config_interopified_resolves_scene_interchange(cc));

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

    std::string dir     = Filesystem::temp_directory_path();
    std::string path_a  = dir + "/oiio_color_test_fpcache_a.ocio";
    std::string path_b  = dir + "/oiio_color_test_fpcache_b.ocio";
    std::string path_o  = dir + "/oiio_color_test_fpcache_other.ocio";
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
    ColorSpaceFingerprint m1 = color_space_fingerprint_cached(cc1, "matrix_inv");
    OIIO_CHECK_ASSERT(m1.computed());
    OIIO_CHECK_EQUAL(color_space_fingerprint_cache_size(), size_t(1));
    ColorSpaceFingerprint m1b = color_space_fingerprint_cached(cc1, "matrix_inv");
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
    ColorSpaceFingerprint m2 = color_space_fingerprint_cached(cc2, "matrix_inv");
    OIIO_CHECK_EQUAL(color_space_fingerprint_cache_size(), size_t(2));  // collapsed
    OIIO_CHECK_ASSERT(m1.values == m2.values);

    // (2b) The context-sensitive space does NOT collapse: cc2's different
    // context keys a separate bucket. This is airtight given (2a): matrix_inv
    // collapsing proved cc1 and cc2 share one structural config id, so the only
    // thing that can grow the cache here is ctx_space's differing context id.
    // (The fingerprint VALUE is not asserted to differ: the probe copy that
    // computes it is memoized per structural config, so both contexts probe
    // through the copy the first query built -- the cache keys the contexts
    // apart regardless, which is what this slice guarantees.)
    ColorSpaceFingerprint s2 = color_space_fingerprint_cached(cc2, "ctx_space");
    OIIO_CHECK_EQUAL(color_space_fingerprint_cache_size(), size_t(3));  // new bucket
    OIIO_CHECK_ASSERT(s2.computed());

    // (3) A structurally different config keys separately; the earlier entries
    // just orphan (content-addressed, no eviction, no crash).
    {
        ColorConfig cco(path_o);
        OIIO_CHECK_ASSERT(!cco.has_error());
        size_t before = color_space_fingerprint_cache_size();
        ColorSpaceFingerprint d = color_space_fingerprint_cached(cco, "doubler");
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



// True-cold construction helper: re-exec'd as a fresh subprocess by
// run_bench_phases() below (see comment there for why). Prints
// "construct_ms <value>" and exits -- no other tests run.
static int
bench_child_construct_and_exit()
{
    Timer timer;
    ColorConfig cc("ocio://default");
    std::cout << Strutil::fmt::format("construct_ms {:.6f}\n", timer() * 1000.0);
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
    double fingerprint_vector_ms = t_vector() * 1000.0;
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

    test_sRGB_conversion();
    test_Rec709_conversion();
    test_interop_identities_config();
    test_interop_id_grammar();
    test_registry_invariants();
    test_color_space_classification();
    test_color_space_fingerprint();
    test_config_interoperability();
    test_cross_config_processor();
    test_cross_config_conversion();
    test_color_space_fingerprint_cache();

    // --bench is opt-in and heavy; the default `ctest -R unit_color` run
    // never sets it.
    if (bench_mode)
        run_bench_phases();

    return unit_test_failures != 0;
}
