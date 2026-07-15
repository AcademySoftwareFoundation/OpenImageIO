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
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/unittest.h>

#include "imageio_pvt.h"

using namespace OIIO;
using namespace OIIO::pvt;


// A small, valid OCIO config whose srgb_rec709_scene carries a real
// (non-identity) transform, so pixel values distinguish which source space
// a conversion actually used. CICP (1,13,*,*) maps to srgb_rec709_scene via
// the built-in table, which this config resolves locally.
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
    OIIO_CHECK_EQUAL(a.resolved, "srgb_rec709_scene");
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
    OIIO_CHECK_EQUAL(lenient.resolved, "srgb_rec709_scene");
    OIIO_CHECK_ASSERT(lenient.has_genuine_metadata_match());

    ColorReadPolicy config_only;
    config_only.scope = ColorResolutionScope::ConfigOnly;
    auto strict = resolve_color_metadata(&sparse, spec, {}, config_only);
    OIIO_CHECK_ASSERT(!strict.has_genuine_metadata_match());
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

    Filesystem::remove(cfgpath);
    Filesystem::remove(sparsepath);
    return unit_test_failures != 0;
}
