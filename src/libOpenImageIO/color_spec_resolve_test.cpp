// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// Unit tests for the spec-aware color-metadata resolution surface (pvt):
// fact extraction from an ImageSpec and the spec resolve() overload, driven
// directly through imageio_pvt.h. Also locks the reader-path / spec-path
// same-hints-same-answer regression.

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

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
)";
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
    const float chroma[8] = { 0.64f, 0.33f, 0.30f, 0.60f,
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
    auto strict = resolve_color_metadata(&sparse, spec, {}, config_only);
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
        const float chroma[8] = { 0.708f, 0.292f, 0.170f, 0.797f,
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
        spec.attribute("ICCProfile",
                       TypeDesc(TypeDesc::UINT8, int(icc.size())), icc.data());
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
    ImageBuf explicit_wins
        = ImageBufAlgo::colorconvert(src, "lin_ap1_scene", "lin_test_scene",
                                     true, "", "", &config);
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


// The scrubber erases a hint only when the resolver proves what it claims
// (redundant or contradictory either way); indeterminate hints and honored
// declarations stay.
static void
test_scrubber(const ColorConfig& config)
{
    {
        ImageSpec spec(4, 4, 3, TypeFloat);
        spec.attribute("oiio:ColorSpace", "lin_test_scene");
        spec.attribute("CICP", TypeDesc(TypeDesc::INT, 4), kCicpSrgb);
        spec.attribute("colorInteropID", "lin_ap1_scene");
        const float chroma[8] = { 0.64f, 0.33f, 0.30f, 0.60f,
                                  0.15f, 0.06f, 0.3127f, 0.3290f };
        spec.attribute("chromaticities", TypeDesc(TypeDesc::FLOAT, 8), chroma);
        spec.attribute("oiio:Gamma", 2.4f);
        auto icc = fake_icc_profile();
        spec.attribute("ICCProfile",
                       TypeDesc(TypeDesc::UINT8, int(icc.size())), icc.data());
        scrub_color_metadata(spec, &config, {});
        // All provable: CICP and CIID resolve, chroma+gamma and the ICC
        // resolve to their synthetics -- every claim is determinate.
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
        // Indeterminate claims survive: an unresolvable vendor id, a
        // garbage (undecodable) ICC blob.
        ImageSpec spec(4, 4, 3, TypeFloat);
        spec.attribute("oiio:ColorSpace", "lin_test_scene");
        spec.attribute("colorInteropID", "vendorx_mystery");
        std::vector<unsigned char> garbage(200, 0x42);
        spec.attribute("ICCProfile",
                       TypeDesc(TypeDesc::UINT8, int(garbage.size())),
                       garbage.data());
        scrub_color_metadata(spec, &config, {});
        OIIO_CHECK_ASSERT(spec.find_attribute("colorInteropID"));
        OIIO_CHECK_ASSERT(spec.find_attribute("ICCProfile"));
    }
    {
        // The deliberate unknown-marker family (ocio:unknown /
        // oiio:unknown / error:unknown) is honored, never scrubbed; a bare
        // "unknown" claim is contradicted by any definite color space.
        for (const char* marker :
             { "ocio:unknown", "oiio:unknown", "error:unknown" }) {
            ImageSpec spec(4, 4, 3, TypeFloat);
            spec.attribute("oiio:ColorSpace", "lin_test_scene");
            spec.attribute("colorInteropID", marker);
            scrub_color_metadata(spec, &config, {});
            OIIO_CHECK_EQUAL(spec.get_string_attribute("colorInteropID"),
                             marker);
        }

        ImageSpec spec(4, 4, 3, TypeFloat);
        spec.attribute("oiio:ColorSpace", "lin_test_scene");
        spec.attribute("colorInteropID", "unknown");
        scrub_color_metadata(spec, &config, {});
        OIIO_CHECK_ASSERT(!spec.find_attribute("colorInteropID"));
    }
    {
        // No established color space: nothing to judge against, no scrub.
        ImageSpec spec(4, 4, 3, TypeFloat);
        spec.attribute("CICP", TypeDesc(TypeDesc::INT, 4), kCicpSrgb);
        scrub_color_metadata(spec, &config, {});
        OIIO_CHECK_ASSERT(spec.find_attribute("CICP"));
    }
}


// IBA wiring of the scrubber: the inferred-source path scrubs the outgoing
// spec; the explicit-source path is byte-identical to main (hints kept).
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

    ImageBuf explicit_src
        = ImageBufAlgo::colorconvert(src, "srgb_rec709_scene",
                                     "lin_test_scene", true, "", "", &config);
    OIIO_CHECK_ASSERT(!explicit_src.has_error());
    // Explicit source: no scrub, stale hints intentionally untouched
    // (CICP is still cleared by set_colorspace, as on main).
    OIIO_CHECK_ASSERT(explicit_src.spec().find_attribute("ICCProfile"));
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

    test_same_hints_same_answer(config);
    test_config_or_registry_failover(sparse);
    test_infer_helper(config);
    test_iba_inference(config);
    test_scrubber(config);
    test_iba_scrub_wiring(config);

    Filesystem::remove(cfgpath);
    Filesystem::remove(sparsepath);
    return unit_test_failures != 0;
}
