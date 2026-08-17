// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// Unit tests for the spec-aware color-metadata resolution surface (pvt):
// fact extraction from an ImageSpec and the spec resolve() overload, driven
// directly through color_pvt.h. Also locks the reader-path / spec-path
// same-hints-same-answer regression.

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "color_pvt.h"
#include <OpenImageIO/color.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>

#include <OpenImageIO/unittest.h>

#include "imageio_pvt.h"

using namespace OIIO;
using namespace OIIO::pvt;


// A small, valid OCIO config whose srgb_rec709_display carries a real
// (non-identity) transform, so pixel values distinguish which source space
// a conversion actually used. CICP (1,13,*,*) maps to srgb_rec709_display
// via the built-in table (CICP describes display encodings, so the display
// twin is listed first), which this config resolves locally.
static std::string
write_test_config()
{
    std::string path = Filesystem::temp_directory_path()
                       + "/oiio_csr_test.ocio";
    std::ofstream f(path);
    f << R"(ocio_profile_version: 2
environment: {}
search_path: ""
roles:
  default: raw_data
  data: raw_data
  scene_linear: lin_test_scene
displays:
  disp:
    - !<View> {name: view, colorspace: srgb_rec709_scene}
active_displays: [disp]
active_views: [view]
colorspaces:
  - !<ColorSpace>
    name: raw_data
    isdata: true
  - !<ColorSpace>
    name: lin_test_scene
  - !<ColorSpace>
    name: lin_ap1_scene
  - !<ColorSpace>
    name: srgb_rec709_scene
  - !<ColorSpace>
    name: srgb_rec709_display
    from_scene_reference: !<ExponentTransform> {value: [2.2, 2.2, 2.2, 1.0]}
file_rules:
  - !<Rule> {name: lin_rule, pattern: "*lin_test_scene*", extension: "*", colorspace: lin_test_scene}
  - !<Rule> {name: Default, colorspace: default}
)";
    f.close();
    return path;
}


// A strict-parsing config that declares the "error:unknown" catch space
// the effective-strict failure split honors (with a real transform, so
// pixels prove which source space a conversion used).
static std::string
write_strict_config()
{
    std::string path = Filesystem::temp_directory_path()
                       + "/oiio_csr_strict.ocio";
    std::ofstream f(path);
    f << R"(ocio_profile_version: 2
environment: {}
search_path: ""
strictparsing: true
roles:
  default: raw_data
  scene_linear: lin_strict
displays:
  disp:
    - !<View> {name: view, colorspace: lin_strict}
active_displays: [disp]
active_views: [view]
colorspaces:
  - !<ColorSpace>
    name: raw_data
    isdata: true
  - !<ColorSpace>
    name: lin_strict
  - !<ColorSpace>
    name: "error:unknown"
    from_scene_reference: !<ExponentTransform> {value: [2.2, 2.2, 2.2, 1.0]}
)";
    f.close();
    return path;
}


// A minimal identity 1D LUT, for exercising ociofiletransform.
static std::string
write_cube_lut(const std::string& basename)
{
    std::string path = Filesystem::temp_directory_path() + "/" + basename;
    std::ofstream f(path);
    f << "LUT_1D_SIZE 2\n0.0 0.0 0.0\n0.9 0.9 0.9\n";
    f.close();
    return path;
}


// A config that resolves NO interop identity locally, for the
// config-or-registry failover vectors.
static std::string
write_sparse_config()
{
    std::string path = Filesystem::temp_directory_path()
                       + "/oiio_csr_sparse.ocio";
    std::ofstream f(path);
    f << R"(ocio_profile_version: 2
environment: {}
search_path: ""
roles:
  default: raw_data
  scene_linear: lin_only
displays:
  disp:
    - !<View> {name: view, colorspace: lin_only}
active_displays: [disp]
active_views: [view]
colorspaces:
  - !<ColorSpace>
    name: raw_data
    isdata: true
  - !<ColorSpace>
    name: lin_only
)";
    f.close();
    return path;
}


// A >=128-byte blob carrying the 'acsp' signature at offset 36 (the
// resolver's decodability test).
static std::vector<unsigned char>
fake_icc_profile()
{
    std::vector<unsigned char> p(200, 0x00);
    p[36] = 'a';
    p[37] = 'c';
    p[38] = 's';
    p[39] = 'p';
    return p;
}


static const int kCicpSrgb[4] = { 1, 13, 0, 1 };


// Every ColorMetadataFacts field the spec carries is extracted; absent
// attributes leave their fields absent.
static void
test_facts_from_spec()
{
    ImageSpec spec(4, 4, 3, TypeFloat);
    spec.attribute("acesImageContainerFlag", 1);
    spec.attribute("colorInteropID", "lin_ap1_scene");
    spec.attribute("CICP", TypeDesc(TypeDesc::INT, 4), kCicpSrgb);
    const float chroma[8] = { 0.64f, 0.33f, 0.30f,   0.60f,
                              0.15f, 0.06f, 0.3127f, 0.3290f };
    spec.attribute("chromaticities", TypeDesc(TypeDesc::FLOAT, 8), chroma);
    spec.attribute("oiio:Gamma", 2.4f);
    auto icc = fake_icc_profile();
    spec.attribute("ICCProfile", TypeDesc(TypeDesc::UINT8, int(icc.size())),
                   icc.data());

    ColorMetadataFacts f = color_facts_from_spec(spec);
    OIIO_CHECK_ASSERT(f.aces_image_container);
    OIIO_CHECK_EQUAL(f.color_interop_id, "lin_ap1_scene");
    OIIO_CHECK_EQUAL(f.icc_profile.size(), icc.size());
    OIIO_CHECK_ASSERT(f.has_cicp);
    OIIO_CHECK_EQUAL(f.cicp[1], 13);
    OIIO_CHECK_ASSERT(f.has_chromaticities);
    OIIO_CHECK_EQUAL(f.chromaticities[6], 0.3127f);
    OIIO_CHECK_ASSERT(f.has_gamma);
    OIIO_CHECK_EQUAL(f.gamma, 2.4f);

    ColorMetadataFacts empty = color_facts_from_spec(
        ImageSpec(4, 4, 3, TypeFloat));
    OIIO_CHECK_ASSERT(!empty.aces_image_container);
    OIIO_CHECK_ASSERT(empty.color_interop_id.empty());
    OIIO_CHECK_ASSERT(empty.icc_profile.empty());
    OIIO_CHECK_ASSERT(!empty.has_cicp);
    OIIO_CHECK_ASSERT(!empty.has_chromaticities);
    OIIO_CHECK_ASSERT(!empty.has_gamma);
}


// Regression lock: the same hints resolve identically whether entered as a
// facts struct (the reader path) or through the spec overload (the IBA
// path) -- equal resolved values AND step-by-step equal traces.
static void
test_same_hints_same_answer(const ColorConfig& config)
{
    ImageSpec spec(4, 4, 3, TypeFloat);
    spec.attribute("colorInteropID", "unknown");  // unusable, falls through
    spec.attribute("CICP", TypeDesc(TypeDesc::INT, 4), kCicpSrgb);

    ColorMetadataFacts facts;
    facts.color_interop_id = "unknown";
    facts.has_cicp         = true;
    for (int i = 0; i < 4; ++i)
        facts.cicp[i] = kCicpSrgb[i];

    auto a = resolve_color_metadata(&config, "", facts, {}, {});
    auto b = resolve_color_metadata(&config, spec, {}, {});
    OIIO_CHECK_EQUAL(a.resolved, b.resolved);
    OIIO_CHECK_EQUAL(a.resolved, "srgb_rec709_display");
    OIIO_CHECK_EQUAL(a.steps.size(), b.steps.size());
    for (size_t i = 0; i < a.steps.size() && i < b.steps.size(); ++i) {
        OIIO_CHECK_EQUAL(int(a.steps[i].rule), int(b.steps[i].rule));
        OIIO_CHECK_EQUAL(int(a.steps[i].outcome), int(b.steps[i].outcome));
        OIIO_CHECK_EQUAL(a.steps[i].candidate, b.steps[i].candidate);
        OIIO_CHECK_EQUAL(a.steps[i].resolved, b.steps[i].resolved);
        OIIO_CHECK_EQUAL(a.steps[i].reason, b.steps[i].reason);
    }
}


// Config-or-registry failover: a config that cannot name the CICP identity
// bridges to the registry id under the (default) lenient scope; config-only
// scope refuses instead of bridging.
static void
test_config_or_registry_failover(const ColorConfig& sparse)
{
    ImageSpec spec(4, 4, 3, TypeFloat);
    spec.attribute("CICP", TypeDesc(TypeDesc::INT, 4), kCicpSrgb);

    auto lenient = resolve_color_metadata(&sparse, spec, {}, {});
    OIIO_CHECK_EQUAL(lenient.resolved, "srgb_rec709_display");
    OIIO_CHECK_ASSERT(lenient.has_genuine_metadata_match());

    ColorReadPolicy config_only;
    config_only.scope = ColorResolutionScope::ConfigOnly;
    auto strict       = resolve_color_metadata(&sparse, spec, {}, config_only);
    OIIO_CHECK_ASSERT(!strict.has_genuine_metadata_match());
}


// The inference helper: a usable hint answers; a synthetic-only answer
// (here: chromaticities with no config match) is not usable as an IBA
// source; when CICP and a decodable ICC profile are both present, CICP
// outranks ICC (PNG chunk precedence: cICP > iCCP) and answers directly.
static void
test_infer_helper(const ColorConfig& config)
{
    {
        ImageSpec spec(4, 4, 3, TypeFloat);
        spec.attribute("CICP", TypeDesc(TypeDesc::INT, 4), kCicpSrgb);
        OIIO_CHECK_EQUAL(infer_color_space_from_spec(&config, spec, {}, {}),
                         "srgb_rec709_display");
    }
    {
        // Wide-gamut chromaticities resolve only to a custom: synthetic --
        // no constructible space, so no inference.
        ImageSpec spec(4, 4, 3, TypeFloat);
        const float chroma[8] = { 0.708f, 0.292f, 0.170f,  0.797f,
                                  0.131f, 0.046f, 0.3127f, 0.3290f };
        spec.attribute("chromaticities", TypeDesc(TypeDesc::FLOAT, 8), chroma);
        OIIO_CHECK_EQUAL(infer_color_space_from_spec(&config, spec, {}, {}),
                         "");
    }
    {
        // CICP outranks the decodable ICC profile (cICP > iCCP) and
        // answers on the first pass; no config-only retry is needed.
        ImageSpec spec(4, 4, 3, TypeFloat);
        auto icc = fake_icc_profile();
        spec.attribute("ICCProfile", TypeDesc(TypeDesc::UINT8, int(icc.size())),
                       icc.data());
        spec.attribute("CICP", TypeDesc(TypeDesc::INT, 4), kCicpSrgb);
        OIIO_CHECK_EQUAL(infer_color_space_from_spec(&config, spec, {}, {}),
                         "srgb_rec709_display");
    }
    {
        ImageSpec spec(4, 4, 3, TypeFloat);  // no hints at all
        OIIO_CHECK_EQUAL(infer_color_space_from_spec(&config, spec, {}, {}),
                         "");
    }
}


// IBA wiring: an untagged, CICP-carrying source converts exactly as if the
// caller had passed the mapped space explicitly; an explicit source always
// wins over contradictory hints; a hintless untagged source keeps today's
// scene_linear default.
static void
test_iba_inference(const ColorConfig& config)
{
    ImageSpec spec(8, 8, 3, TypeFloat);
    spec.attribute("CICP", TypeDesc(TypeDesc::INT, 4), kCicpSrgb);
    ImageBuf src(spec);
    ImageBufAlgo::fill(src, { 0.5f, 0.25f, 0.75f });

    // Inferred source == explicit source, pixel for pixel.
    ImageBuf inferred = ImageBufAlgo::colorconvert(src, "", "lin_test_scene",
                                                   true, "", "", &config);
    OIIO_CHECK_ASSERT(!inferred.has_error());
    ImageBuf explicit_src
        = ImageBufAlgo::colorconvert(src, "srgb_rec709_display",
                                     "lin_test_scene", true, "", "", &config);
    OIIO_CHECK_ASSERT(!explicit_src.has_error());
    auto cmp = ImageBufAlgo::compare(inferred, explicit_src, 0.0f, 0.0f);
    OIIO_CHECK_EQUAL(cmp.nfail, 0);
    OIIO_CHECK_EQUAL(inferred.spec().get_string_attribute("oiio:ColorSpace"),
                     "lin_test_scene");

    // The conversion was real: it differs from a no-op source.
    ImageBuf noop = ImageBufAlgo::colorconvert(src, "lin_test_scene",
                                               "lin_test_scene", true, "", "",
                                               &config);
    OIIO_CHECK_ASSERT(!noop.has_error());
    auto cmp2 = ImageBufAlgo::compare(inferred, noop, 0.0f, 0.0f);
    OIIO_CHECK_ASSERT(cmp2.nfail > 0);

    // Explicit source wins over the contradictory CICP hint.
    ImageBuf explicit_wins = ImageBufAlgo::colorconvert(src, "lin_ap1_scene",
                                                        "lin_test_scene", true,
                                                        "", "", &config);
    OIIO_CHECK_ASSERT(!explicit_wins.has_error());
    auto cmp3 = ImageBufAlgo::compare(explicit_wins, explicit_src, 0.0f, 0.0f);
    OIIO_CHECK_ASSERT(cmp3.nfail > 0);

    // No hints: today's scene_linear default stands (scene_linear ->
    // lin_test_scene is a no-op in this config).
    ImageBuf plain_src(ImageSpec(8, 8, 3, TypeFloat));
    ImageBufAlgo::fill(plain_src, { 0.5f, 0.25f, 0.75f });
    ImageBuf dflt = ImageBufAlgo::colorconvert(plain_src, "", "lin_test_scene",
                                               true, "", "", &config);
    OIIO_CHECK_ASSERT(!dflt.has_error());
    ImageBuf dflt_explicit
        = ImageBufAlgo::colorconvert(plain_src, "scene_linear",
                                     "lin_test_scene", true, "", "", &config);
    auto cmp4 = ImageBufAlgo::compare(dflt, dflt_explicit, 0.0f, 0.0f);
    OIIO_CHECK_EQUAL(cmp4.nfail, 0);
}


// The scrubber applies the two-bucket rule categorically: after an
// identity-known color change (which the caller asserts), every
// file-provenance fact is stale and erased -- no per-signal re-resolution
// -- while the deliberate unknown-marker family is honored.
static void
test_scrubber(const ColorConfig& config)
{
    (void)config;  // categorical scrubbing needs no config
    {
        ImageSpec spec(4, 4, 3, TypeFloat);
        spec.attribute("oiio:ColorSpace", "lin_test_scene");
        spec.attribute("CICP", TypeDesc(TypeDesc::INT, 4), kCicpSrgb);
        spec.attribute("colorInteropID", "lin_ap1_scene");
        const float chroma[8] = { 0.64f, 0.33f, 0.30f,   0.60f,
                                  0.15f, 0.06f, 0.3127f, 0.3290f };
        spec.attribute("chromaticities", TypeDesc(TypeDesc::FLOAT, 8), chroma);
        spec.attribute("oiio:Gamma", 2.4f);
        auto icc = fake_icc_profile();
        spec.attribute("ICCProfile", TypeDesc(TypeDesc::UINT8, int(icc.size())),
                       icc.data());
        scrub_color_metadata(spec);
        // Every provenance fact is gone.
        OIIO_CHECK_ASSERT(!spec.find_attribute("CICP"));
        OIIO_CHECK_ASSERT(!spec.find_attribute("colorInteropID"));
        OIIO_CHECK_ASSERT(!spec.find_attribute("chromaticities"));
        OIIO_CHECK_ASSERT(!spec.find_attribute("oiio:Gamma"));
        OIIO_CHECK_ASSERT(!spec.find_attribute("ICCProfile"));
        // The color space itself is never scrubbed.
        OIIO_CHECK_EQUAL(spec.get_string_attribute("oiio:ColorSpace"),
                         "lin_test_scene");
    }
    {
        // Categorical: even claims no resolver could decide (a vendor id,
        // a garbage ICC blob) are provenance and go -- never persist stale
        // information.
        ImageSpec spec(4, 4, 3, TypeFloat);
        spec.attribute("oiio:ColorSpace", "lin_test_scene");
        spec.attribute("colorInteropID", "vendorx_mystery");
        std::vector<unsigned char> garbage(200, 0x42);
        spec.attribute("ICCProfile",
                       TypeDesc(TypeDesc::UINT8, int(garbage.size())),
                       garbage.data());
        scrub_color_metadata(spec);
        OIIO_CHECK_ASSERT(!spec.find_attribute("colorInteropID"));
        OIIO_CHECK_ASSERT(!spec.find_attribute("ICCProfile"));
    }
    {
        // The deliberate unknown-marker family (ocio:unknown /
        // oiio:unknown / error:unknown) is honored, never scrubbed
        // (treatment/error state, not provenance); a bare "unknown" claim
        // named the pre-operation state and goes.
        for (const char* marker :
             { "ocio:unknown", "oiio:unknown", "error:unknown" }) {
            ImageSpec spec(4, 4, 3, TypeFloat);
            spec.attribute("oiio:ColorSpace", "lin_test_scene");
            spec.attribute("colorInteropID", marker);
            scrub_color_metadata(spec);
            OIIO_CHECK_EQUAL(spec.get_string_attribute("colorInteropID"),
                             marker);
        }

        ImageSpec spec(4, 4, 3, TypeFloat);
        spec.attribute("oiio:ColorSpace", "lin_test_scene");
        spec.attribute("colorInteropID", "unknown");
        scrub_color_metadata(spec);
        OIIO_CHECK_ASSERT(!spec.find_attribute("colorInteropID"));
    }
    {
        // Current-state descriptors are the other bucket: never touched by
        // the provenance scrub.
        ImageSpec spec(4, 4, 3, TypeFloat);
        spec.attribute("oiio:ColorSpace", "lin_test_scene");
        spec.attribute("oiio:ColorSpace:state", "scene");
        spec.attribute("oiio:ColorSpace:range", "narrow");
        scrub_color_metadata(spec);
        OIIO_CHECK_EQUAL(spec.get_string_attribute("oiio:ColorSpace:state"),
                         "scene");
        OIIO_CHECK_EQUAL(spec.get_string_attribute("oiio:ColorSpace:range"),
                         "narrow");
    }
}


// Check a current-state descriptor against the update-or-erase rule: a
// usable value updates the attribute, an unavailable one erases it.
static void
check_descriptor(const ImageSpec& spec, const char* name, string_view value)
{
    if (value.size())
        OIIO_CHECK_EQUAL(spec.get_string_attribute(name), std::string(value));
    else
        OIIO_CHECK_ASSERT(!spec.find_attribute(name));
}


// ColorConfig::set_colorspace routes through the shared identity-known
// hygiene: changing an existing claim scrubs the provenance-facts bucket
// and maintains (update-or-erase) any current-state descriptors present;
// first tagging preserves the facts (they are the evidence read paths
// derive the claim from); the empty name erases everything; re-asserting
// the current claim is a no-op. All of it cheap: no fingerprint is ever
// derived.
static void
test_set_colorspace_hygiene(const ColorConfig& config)
{
    const float chroma[8] = { 0.64f, 0.33f, 0.30f,   0.60f,
                              0.15f, 0.06f, 0.3127f, 0.3290f };
    auto icc              = fake_icc_profile();
    auto add_facts        = [&](ImageSpec& spec) {
        spec.attribute("CICP", TypeDesc(TypeDesc::INT, 4), kCicpSrgb);
        spec.attribute("colorInteropID", "lin_ap1_scene");
        spec.attribute("chromaticities", TypeDesc(TypeDesc::FLOAT, 8), chroma);
        spec.attribute("oiio:Gamma", 2.4f);
        spec.attribute("ICCProfile", TypeDesc(TypeDesc::UINT8, int(icc.size())),
                              icc.data());
        spec.attribute("Exif:ColorSpace", 1);
        spec.attribute("tiff:ColorSpace", 1);
    };

    // Changing an existing claim: verdict updated, the full provenance
    // bucket (and the format-specific hints) scrubbed -- and no
    // descriptors are introduced on a spec that carried none.
    {
        ImageSpec spec(4, 4, 3, TypeFloat);
        spec.attribute("oiio:ColorSpace", "srgb_rec709_scene");
        add_facts(spec);
        config.set_colorspace(spec, "lin_test_scene");
        OIIO_CHECK_EQUAL(spec.get_string_attribute("oiio:ColorSpace"),
                         "lin_test_scene");
        for (const char* attr :
             { "CICP", "colorInteropID", "chromaticities", "oiio:Gamma",
               "ICCProfile", "Exif:ColorSpace", "tiff:ColorSpace" })
            OIIO_CHECK_ASSERT(!spec.find_attribute(attr));
        for (const char* attr :
             { "oiio:ColorSpace:state", "oiio:ColorSpace:encoding",
               "oiio:ColorSpace:range", "oiio:ColorSpace:equality_id" })
            OIIO_CHECK_ASSERT(!spec.find_attribute(attr));
    }

    // Descriptor maintenance on a change is update-or-erase against the
    // cheap characterization of the new space -- and never computes a
    // fingerprint.
    {
        const size_t fp_before = color_space_fingerprint_cache_size();
        ImageSpec spec(4, 4, 3, TypeFloat);
        spec.attribute("oiio:ColorSpace", "srgb_rec709_display");
        spec.attribute("oiio:ColorSpace:state", "display");
        spec.attribute("oiio:ColorSpace:range", "narrow");
        spec.attribute("oiio:ColorSpace:equality_id", "stale_equality_id");
        config.set_colorspace(spec, "lin_test_scene");
        ColorSpaceInfo info = config.get_color_space_info("lin_test_scene");
        OIIO_CHECK_ASSERT(info.valid());
        OIIO_CHECK_EQUAL(info.image_state(), "scene");
        check_descriptor(spec, "oiio:ColorSpace:state", info.image_state());
        check_descriptor(spec, "oiio:ColorSpace:encoding", info.encoding());
        // Stale values that the cheap get cannot vouch for are erased,
        // never retained and never derived.
        check_descriptor(spec, "oiio:ColorSpace:range", info.range());
        check_descriptor(spec, "oiio:ColorSpace:equality_id",
                         info.equality_id());
        OIIO_CHECK_EQUAL(color_space_fingerprint_cache_size(), fp_before);
    }

    // First tagging (no previous claim) preserves the provenance facts:
    // read paths derive the claim from them. Only the longstanding
    // hand-invalidated hints (gamma, Exif/tiff) are removed.
    {
        ImageSpec spec(4, 4, 3, TypeFloat);
        add_facts(spec);
        config.set_colorspace(spec, "lin_test_scene");
        OIIO_CHECK_EQUAL(spec.get_string_attribute("oiio:ColorSpace"),
                         "lin_test_scene");
        for (const char* attr :
             { "CICP", "colorInteropID", "chromaticities", "ICCProfile" })
            OIIO_CHECK_ASSERT(spec.find_attribute(attr));
        OIIO_CHECK_ASSERT(!spec.find_attribute("oiio:Gamma"));
        OIIO_CHECK_ASSERT(!spec.find_attribute("Exif:ColorSpace"));
        OIIO_CHECK_ASSERT(!spec.find_attribute("tiff:ColorSpace"));
        OIIO_CHECK_ASSERT(!spec.find_attribute("oiio:ColorSpace:state"));
    }

    // Empty name: absence semantics -- verdict, facts, and descriptors
    // are all erased.
    {
        ImageSpec spec(4, 4, 3, TypeFloat);
        spec.attribute("oiio:ColorSpace", "srgb_rec709_scene");
        add_facts(spec);
        spec.attribute("oiio:ColorSpace:state", "scene");
        spec.attribute("oiio:ColorSpace:range", "narrow");
        config.set_colorspace(spec, "");
        OIIO_CHECK_ASSERT(!spec.find_attribute("oiio:ColorSpace"));
        for (const char* attr :
             { "CICP", "colorInteropID", "chromaticities", "oiio:Gamma",
               "ICCProfile", "oiio:ColorSpace:state", "oiio:ColorSpace:range" })
            OIIO_CHECK_ASSERT(!spec.find_attribute(attr));
    }

    // Re-asserting the current claim is a no-op: facts and descriptors
    // (even stale ones) are untouched. Refreshes ride an actual change of
    // claim or the pixel-operation hygiene, keeping redundant re-tagging
    // read paths (e.g. Exif decode) byte-identical.
    {
        ImageSpec spec(4, 4, 3, TypeFloat);
        spec.attribute("oiio:ColorSpace", "lin_test_scene");
        add_facts(spec);
        spec.attribute("oiio:ColorSpace:state", "display");
        config.set_colorspace(spec, "lin_test_scene");
        OIIO_CHECK_ASSERT(spec.find_attribute("CICP"));
        OIIO_CHECK_ASSERT(spec.find_attribute("colorInteropID"));
        OIIO_CHECK_ASSERT(spec.find_attribute("oiio:Gamma"));
        OIIO_CHECK_EQUAL(spec.get_string_attribute("oiio:ColorSpace:state"),
                         "display");
    }

    // set_colorspace_rec709_gamma inherits the routing and still records
    // the gamma afterward.
    {
        ImageSpec spec(4, 4, 3, TypeFloat);
        spec.attribute("oiio:ColorSpace", "lin_test_scene");
        config.set_colorspace_rec709_gamma(spec, 2.2f);
        OIIO_CHECK_EQUAL(spec.get_string_attribute("oiio:ColorSpace"),
                         "g22_rec709_scene");
        OIIO_CHECK_EQUAL(spec.get_float_attribute("oiio:Gamma"), 2.2f);
    }

    // ImageSpec::set_colorspace routes through ColorConfig::set_colorspace
    // (default config) and additionally always invalidates CICP -- but the
    // first-tagging path still preserves the other evidence facts.
    {
        ImageSpec spec(4, 4, 3, TypeFloat);
        spec.attribute("CICP", TypeDesc(TypeDesc::INT, 4), kCicpSrgb);
        spec.attribute("colorInteropID", "lin_ap1_scene");
        spec.set_colorspace("lin_ap1_scene");
        OIIO_CHECK_EQUAL(spec.get_string_attribute("oiio:ColorSpace"),
                         "lin_ap1_scene");
        OIIO_CHECK_ASSERT(!spec.find_attribute("CICP"));
        OIIO_CHECK_ASSERT(spec.find_attribute("colorInteropID"));
    }
}


// IBA wiring of the scrubber: identity-known operations scrub the outgoing
// spec's provenance facts uniformly -- inferred AND explicit sources alike
// (the facts describe the pre-operation source either way).
static void
test_iba_scrub_wiring(const ColorConfig& config)
{
    ImageSpec spec(8, 8, 3, TypeFloat);
    spec.attribute("CICP", TypeDesc(TypeDesc::INT, 4), kCicpSrgb);
    auto icc = fake_icc_profile();
    spec.attribute("ICCProfile", TypeDesc(TypeDesc::UINT8, int(icc.size())),
                   icc.data());
    ImageBuf src(spec);
    ImageBufAlgo::fill(src, { 0.5f, 0.25f, 0.75f });

    ImageBuf inferred = ImageBufAlgo::colorconvert(src, "", "lin_test_scene",
                                                   true, "", "", &config);
    OIIO_CHECK_ASSERT(!inferred.has_error());
    OIIO_CHECK_EQUAL(inferred.spec().get_string_attribute("oiio:ColorSpace"),
                     "lin_test_scene");
    OIIO_CHECK_ASSERT(!inferred.spec().find_attribute("ICCProfile"));
    OIIO_CHECK_ASSERT(!inferred.spec().find_attribute("CICP"));

    ImageBuf explicit_src = ImageBufAlgo::colorconvert(src, "srgb_rec709_scene",
                                                       "lin_test_scene", true,
                                                       "", "", &config);
    OIIO_CHECK_ASSERT(!explicit_src.has_error());
    // Uniform two-bucket scrub: the explicit-source path scrubs too.
    OIIO_CHECK_ASSERT(!explicit_src.spec().find_attribute("ICCProfile"));
    OIIO_CHECK_ASSERT(!explicit_src.spec().find_attribute("CICP"));
}


// A hint-laden source buffer: tagged (or not), every provenance fact
// present, plus pre-existing (stale) current-state descriptors.
static ImageBuf
make_hinted_src(const char* space)
{
    ImageSpec spec(4, 4, 3, TypeFloat);
    if (space && space[0])
        spec.attribute("oiio:ColorSpace", space);
    spec.attribute("CICP", TypeDesc(TypeDesc::INT, 4), kCicpSrgb);
    spec.attribute("colorInteropID", "lin_ap1_scene");
    auto icc = fake_icc_profile();
    spec.attribute("ICCProfile", TypeDesc(TypeDesc::UINT8, int(icc.size())),
                   icc.data());
    spec.attribute("oiio:ColorSpace:range", "narrow");
    spec.attribute("oiio:ColorSpace:equality_id", "stale_equality_id");
    ImageBuf buf(spec);
    ImageBufAlgo::fill(buf, { 0.5f, 0.25f, 0.75f });
    return buf;
}


// Per-class hygiene outcomes. Identity-known: verdict updated, provenance
// facts scrubbed (explicit AND inferred source), cheap descriptors
// maintained update-or-erase. Identity-unknowable: verdict, facts, and
// descriptors all erased (absence, never a guess). Space-preserving:
// everything passes through.
static void
test_hygiene_per_class(const ColorConfig& config)
{
    // Known, explicit source.
    {
        ImageBuf src = make_hinted_src("srgb_rec709_scene");
        ImageBuf out = ImageBufAlgo::colorconvert(src, "srgb_rec709_scene",
                                                  "lin_test_scene", true, "",
                                                  "", &config);
        OIIO_CHECK_ASSERT(!out.has_error());
        const ImageSpec& s = out.spec();
        OIIO_CHECK_EQUAL(s.get_string_attribute("oiio:ColorSpace"),
                         "lin_test_scene");
        OIIO_CHECK_ASSERT(!s.find_attribute("CICP"));
        OIIO_CHECK_ASSERT(!s.find_attribute("colorInteropID"));
        OIIO_CHECK_ASSERT(!s.find_attribute("ICCProfile"));
        ColorSpaceInfo info = config.get_color_space_info("lin_test_scene");
        OIIO_CHECK_ASSERT(info.valid());
        check_descriptor(s, "oiio:ColorSpace:state", info.image_state());
        check_descriptor(s, "oiio:ColorSpace:encoding", info.encoding());
        // Range operation-awareness: an ordinary conversion does not
        // invent a range, and the stale pre-operation value is gone.
        check_descriptor(s, "oiio:ColorSpace:range", info.range());
        OIIO_CHECK_ASSERT(info.range().empty());
    }
    // Known, inferred source: identical hygiene (uniform rule).
    {
        ImageBuf src = make_hinted_src("");
        ImageBuf out = ImageBufAlgo::colorconvert(src, "", "lin_test_scene",
                                                  true, "", "", &config);
        OIIO_CHECK_ASSERT(!out.has_error());
        const ImageSpec& s = out.spec();
        OIIO_CHECK_EQUAL(s.get_string_attribute("oiio:ColorSpace"),
                         "lin_test_scene");
        OIIO_CHECK_ASSERT(!s.find_attribute("CICP"));
        OIIO_CHECK_ASSERT(!s.find_attribute("colorInteropID"));
        OIIO_CHECK_ASSERT(!s.find_attribute("ICCProfile"));
    }
    // Known via ociodisplay, source inferred from a colorInteropID hint:
    // pixels match the explicit-source call, verdict is the view's space,
    // facts are scrubbed.
    {
        ImageSpec spec(4, 4, 3, TypeFloat);
        spec.attribute("colorInteropID", "lin_ap1_scene");
        ImageBuf src(spec);
        ImageBufAlgo::fill(src, { 0.5f, 0.25f, 0.75f });
        ImageBuf inferred = ImageBufAlgo::ociodisplay(src, "disp", "view", "",
                                                      "", true, false, "", "",
                                                      &config);
        OIIO_CHECK_ASSERT(!inferred.has_error());
        ImageBuf explicit_src
            = ImageBufAlgo::ociodisplay(src, "disp", "view", "lin_ap1_scene",
                                        "", true, false, "", "", &config);
        OIIO_CHECK_ASSERT(!explicit_src.has_error());
        auto cmp = ImageBufAlgo::compare(inferred, explicit_src, 0.0f, 0.0f);
        OIIO_CHECK_EQUAL(cmp.nfail, 0);
        OIIO_CHECK_EQUAL(inferred.spec().get_string_attribute("oiio:ColorSpace"),
                         "srgb_rec709_scene");
        OIIO_CHECK_ASSERT(!inferred.spec().find_attribute("colorInteropID"));
    }
    // Unknowable: an arbitrary LUT whose path names no color space. The
    // verdict, every provenance fact, and every descriptor are erased --
    // absence, never "oiio:unknown".
    {
        std::string lut = write_cube_lut("oiio_csr_plain.cube");
        ImageBuf src    = make_hinted_src("srgb_rec709_scene");
        ImageBuf out = ImageBufAlgo::ociofiletransform(src, lut, false, false,
                                                       &config);
        OIIO_CHECK_ASSERT(!out.has_error());
        const ImageSpec& s = out.spec();
        OIIO_CHECK_ASSERT(!s.find_attribute("oiio:ColorSpace"));
        OIIO_CHECK_ASSERT(!s.find_attribute("CICP"));
        OIIO_CHECK_ASSERT(!s.find_attribute("colorInteropID"));
        OIIO_CHECK_ASSERT(!s.find_attribute("ICCProfile"));
        OIIO_CHECK_ASSERT(!s.find_attribute("oiio:ColorSpace:range"));
        OIIO_CHECK_ASSERT(!s.find_attribute("oiio:ColorSpace:equality_id"));
        Filesystem::remove(lut);
    }
    // ... but a LUT path that names a color space via the config's file
    // rules declares the identity: full Known hygiene, longstanding
    // color-space-from-filepath behavior preserved.
    {
        std::string lut = write_cube_lut("oiio_csr_to_lin_test_scene.cube");
        ImageBuf src    = make_hinted_src("srgb_rec709_scene");
        ImageBuf out = ImageBufAlgo::ociofiletransform(src, lut, false, false,
                                                       &config);
        OIIO_CHECK_ASSERT(!out.has_error());
        OIIO_CHECK_EQUAL(out.spec().get_string_attribute("oiio:ColorSpace"),
                         "lin_test_scene");
        OIIO_CHECK_ASSERT(!out.spec().find_attribute("colorInteropID"));
        Filesystem::remove(lut);
    }
    // Preserved: a data-space no-op passes verdict, facts, and
    // descriptors through untouched (its hints are still true).
    {
        ImageBuf src = make_hinted_src("raw_data");
        ImageBuf out = ImageBufAlgo::colorconvert(src, "raw_data",
                                                  "lin_test_scene", true, "",
                                                  "", &config);
        OIIO_CHECK_ASSERT(!out.has_error());
        const ImageSpec& s = out.spec();
        OIIO_CHECK_EQUAL(s.get_string_attribute("oiio:ColorSpace"), "raw_data");
        OIIO_CHECK_ASSERT(s.find_attribute("CICP"));
        OIIO_CHECK_ASSERT(s.find_attribute("colorInteropID"));
        OIIO_CHECK_ASSERT(s.find_attribute("ICCProfile"));
        // Range operation-awareness: a space-preserving operation retains
        // the buffer's range.
        OIIO_CHECK_EQUAL(s.get_string_attribute("oiio:ColorSpace:range"),
                         "narrow");
    }
}


// The disparity rule (treatment and identity are separate axes): the
// synthetic "oiio:unknown" treatment marker may legally coexist with a
// definite verdict, and hygiene must not "fix" the disparity -- the
// marker survives identity-known hygiene untouched.
static void
test_hygiene_disparity_pin(const ColorConfig& config)
{
    ImageSpec spec(4, 4, 3, TypeFloat);
    spec.attribute("oiio:ColorSpace", "srgb_rec709_scene");
    spec.attribute("colorInteropID", "oiio:unknown");
    ImageBuf src(spec);
    ImageBufAlgo::fill(src, { 0.5f, 0.25f, 0.75f });
    ImageBuf out = ImageBufAlgo::colorconvert(src, "srgb_rec709_scene",
                                              "lin_test_scene", true, "", "",
                                              &config);
    OIIO_CHECK_ASSERT(!out.has_error());
    OIIO_CHECK_EQUAL(out.spec().get_string_attribute("oiio:ColorSpace"),
                     "lin_test_scene");
    OIIO_CHECK_EQUAL(out.spec().get_string_attribute("colorInteropID"),
                     "oiio:unknown");
}


// equality_id maintenance is update-or-erase, never derive: an uncached
// equality id is erased (the stale pre-operation value must not survive)
// and the IBA path never computes a fingerprint; once an explicit derive
// has cached the record, a fresh operation sees the cached value.
static void
test_hygiene_equality_id(const ColorConfig& config)
{
    const size_t fp_before = color_space_fingerprint_cache_size();
    ImageBuf src           = make_hinted_src("srgb_rec709_scene");
    ImageBuf out = ImageBufAlgo::colorconvert(src, "srgb_rec709_scene",
                                              "lin_test_scene", true, "", "",
                                              &config);
    OIIO_CHECK_ASSERT(!out.has_error());
    // Uncached: erased, never derived -- and no fingerprint work happened.
    OIIO_CHECK_ASSERT(
        !out.spec().find_attribute("oiio:ColorSpace:equality_id"));
    OIIO_CHECK_EQUAL(color_space_fingerprint_cache_size(), fp_before);

    // An explicit derive caches the full record; a fresh operation now
    // sees the cached value (or a settled negative, which stays erased).
    ColorSpaceInfo derived = config.derive_color_space_info("lin_test_scene");
    OIIO_CHECK_ASSERT(derived.valid());
    ImageBuf src2 = make_hinted_src("srgb_rec709_scene");
    ImageBuf out2 = ImageBufAlgo::colorconvert(src2, "srgb_rec709_scene",
                                               "lin_test_scene", true, "", "",
                                               &config);
    OIIO_CHECK_ASSERT(!out2.has_error());
    check_descriptor(out2.spec(), "oiio:ColorSpace:equality_id",
                     derived.equality_id());
}


// The failure split, by consequence: pixel math with an unresolvable
// source errors via has_error (never a config-default guess into a
// processor); the config-declared "error:unknown" catch space is honored
// under effective-strict (strict scope AND config strictparsing); the
// (default) lenient scope keeps today's behavior exactly.
static void
test_hygiene_failure_split(const ColorConfig& config,
                           const ColorConfig& strict_config)
{
    ImageBuf untagged(ImageSpec(4, 4, 3, TypeFloat));
    ImageBufAlgo::fill(untagged, { 0.5f, 0.25f, 0.75f });

    // A source tagged literally "unknown" errors under any scope.
    {
        ImageSpec spec(4, 4, 3, TypeFloat);
        spec.attribute("oiio:ColorSpace", "unknown");
        ImageBuf src(spec);
        ImageBufAlgo::fill(src, { 0.5f, 0.25f, 0.75f });
        ImageBuf out = ImageBufAlgo::colorconvert(src, "", "lin_test_scene",
                                                  true, "", "", &config);
        OIIO_CHECK_ASSERT(out.has_error());
        (void)out.geterror();
    }

    OIIO::attribute("oiio:colorpolicy:read:scope", "config_only");
    // Strict scope, no strictparsing on the config: hard error, the
    // catch space is not consulted (effective-strict requires both).
    {
        ImageBuf out = ImageBufAlgo::colorconvert(untagged, "",
                                                  "lin_test_scene", true, "",
                                                  "", &config);
        OIIO_CHECK_ASSERT(out.has_error());
        (void)out.geterror();
    }
    // Effective-strict with the catch space declared: resolution failure
    // lands in "error:unknown" and the operation proceeds -- pixel-equal
    // to naming the catch space explicitly.
    {
        ImageBuf out = ImageBufAlgo::colorconvert(untagged, "", "lin_strict",
                                                  true, "", "", &strict_config);
        OIIO_CHECK_ASSERT(!out.has_error());
        ImageBuf explicit_catch
            = ImageBufAlgo::colorconvert(untagged, "error:unknown",
                                         "lin_strict", true, "", "",
                                         &strict_config);
        OIIO_CHECK_ASSERT(!explicit_catch.has_error());
        auto cmp = ImageBufAlgo::compare(out, explicit_catch, 0.0f, 0.0f);
        OIIO_CHECK_EQUAL(cmp.nfail, 0);
        // The conversion was real (the catch space carries a transform).
        ImageBuf noop = ImageBufAlgo::colorconvert(untagged, "lin_strict",
                                                   "lin_strict", true, "", "",
                                                   &strict_config);
        auto cmp2     = ImageBufAlgo::compare(out, noop, 0.0f, 0.0f);
        OIIO_CHECK_ASSERT(cmp2.nfail > 0);
    }
    OIIO::attribute("oiio:colorpolicy:read:scope", "lenient");

    // Lenient scope: a hintless untagged source keeps today's
    // scene_linear default (a tracking gap is not an error).
    {
        ImageBuf out = ImageBufAlgo::colorconvert(untagged, "",
                                                  "srgb_rec709_display", true,
                                                  "", "", &config);
        OIIO_CHECK_ASSERT(!out.has_error());
    }
}


int
main(int /*argc*/, char* /*argv*/[])
{
    test_facts_from_spec();

    const std::string cfgpath = write_test_config();
    ColorConfig config(cfgpath);
    if (config.has_error()) {
        Strutil::print("Could not load test config: {}\n", config.geterror());
        return 1;
    }
    const std::string sparsepath = write_sparse_config();
    ColorConfig sparse(sparsepath);
    if (sparse.has_error()) {
        Strutil::print("Could not load sparse config: {}\n", sparse.geterror());
        return 1;
    }

    const std::string strictpath = write_strict_config();
    ColorConfig strict_config(strictpath);
    if (strict_config.has_error()) {
        Strutil::print("Could not load strict test config: {}\n",
                       strict_config.geterror());
        return 1;
    }

    test_same_hints_same_answer(config);
    test_config_or_registry_failover(sparse);
    test_infer_helper(config);
    test_iba_inference(config);
    test_scrubber(config);
    test_set_colorspace_hygiene(config);
    test_iba_scrub_wiring(config);
    test_hygiene_per_class(config);
    test_hygiene_disparity_pin(config);
    test_hygiene_equality_id(config);
    test_hygiene_failure_split(config, strict_config);

    Filesystem::remove(cfgpath);
    Filesystem::remove(sparsepath);
    Filesystem::remove(strictpath);
    return unit_test_failures != 0;
}
