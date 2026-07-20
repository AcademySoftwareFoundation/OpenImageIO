// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// Unit tests for the write-side color-metadata plan (pvt), driven directly
// through color_pvt.h. They exercise the write/suppress/derive/omit marking
// of each signal, the never-guess omission rule, the provenance suppression
// rule, and the OpenEXR writer's consumption of the plan.

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include <OpenImageIO/color.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>
#include "color_pvt.h"

#include <OpenImageIO/unittest.h>

#include "imageio_pvt.h"

using namespace OIIO;
using namespace OIIO::pvt;


// The same small config the read-side test uses: its identity spaces carry
// the interop-id / CICP metadata the derive-path vectors resolve against.
static std::string
write_test_config()
{
    std::string path = Filesystem::temp_directory_path() + "/oiio_cmp_test.ocio";
    std::ofstream f(path);
    f << R"(ocio_profile_version: 2
environment: {}
search_path: ""
roles:
  default: raw_data
  scene_linear: lin_ap1_scene
displays:
  disp:
    - !<View> {name: view, colorspace: srgb_rec709_display}
active_displays: [disp]
active_views: [view]
colorspaces:
  - !<ColorSpace>
    name: raw_data
    isdata: true
    aliases: [data]
  - !<ColorSpace>
    name: lin_ap0_scene
  - !<ColorSpace>
    name: lin_ap1_scene
  - !<ColorSpace>
    name: srgb_rec709_scene
  - !<ColorSpace>
    name: srgb_rec709_display
  - !<ColorSpace>
    name: g24_rec709_display
)";
    f.close();
    return path;
}


static ColorWriteCaps
all_caps()
{
    ColorWriteCaps c;
    c.cicp = c.chromaticities = c.gamma = c.icc = c.interop_id = c.mdcv = true;
    return c;
}


// An author-supplied colorInteropID is emitted verbatim (Write); an
// unspecified, underivable color space omits (never-guess).
static void
test_interop_id_write_and_omit()
{
    ColorWritePolicy pol;

    ImageSpec explicit_spec(4, 4, 3, TypeHalf);
    explicit_spec.attribute("colorInteropID", "lin_adobergb_scene");
    auto p1 = plan_color_metadata(nullptr, explicit_spec, all_caps(), pol);
    OIIO_CHECK_EQUAL(int(p1.interop_id.action), int(ColorPlanAction::Write));
    OIIO_CHECK_EQUAL(p1.interop_id.str, "lin_adobergb_scene");

    ImageSpec blank(4, 4, 3, TypeHalf);
    blank.attribute("oiio:ColorSpace", "not-a-real-color-space-xyzzy");
    auto p2 = plan_color_metadata(nullptr, blank, all_caps(), pol);
    OIIO_CHECK_EQUAL(int(p2.interop_id.action), int(ColorPlanAction::Omit));
    OIIO_CHECK_ASSERT(!p2.interop_id.emit());
}


// A capable signal under a "never" policy is Suppressed, not Written, even
// with an author value present.
static void
test_never_suppresses()
{
    ColorWritePolicy pol;
    pol.interop_id = ColorSignalPolicy::Never;

    ImageSpec spec(4, 4, 3, TypeHalf);
    spec.attribute("colorInteropID", "lin_adobergb_scene");
    auto p = plan_color_metadata(nullptr, spec, all_caps(), pol);
    OIIO_CHECK_EQUAL(int(p.interop_id.action), int(ColorPlanAction::Suppress));
    OIIO_CHECK_ASSERT(!p.interop_id.emit());
}


// A signal the format cannot carry stays Omit regardless of the metadata.
static void
test_incapable_omits()
{
    ColorWritePolicy pol;
    ColorWriteCaps caps;  // nothing supported
    ImageSpec spec(4, 4, 3, TypeHalf);
    spec.attribute("colorInteropID", "lin_adobergb_scene");
    auto p = plan_color_metadata(nullptr, spec, caps, pol);
    OIIO_CHECK_EQUAL(int(p.interop_id.action), int(ColorPlanAction::Omit));
}


// Author-supplied chromaticities / gamma are emitted verbatim; without an
// author value and with no in-tree deriver they omit (never-guess).
static void
test_explicit_chroma_and_gamma()
{
    ColorWritePolicy pol;
    const float chrm[8] = { 0.64f, 0.33f, 0.30f,   0.60f,
                            0.15f, 0.06f, 0.3127f, 0.3290f };

    ImageSpec spec(4, 4, 3, TypeHalf);
    spec.attribute("chromaticities", TypeDesc(TypeDesc::FLOAT, 8), chrm);
    spec.attribute("oiio:Gamma", 2.2f);
    auto p = plan_color_metadata(nullptr, spec, all_caps(), pol);
    OIIO_CHECK_EQUAL(int(p.chromaticities.action), int(ColorPlanAction::Write));
    OIIO_CHECK_EQUAL(p.chromaticities.floats.size(), size_t(8));
    OIIO_CHECK_EQUAL(int(p.gamma.action), int(ColorPlanAction::Write));
    OIIO_CHECK_EQUAL(p.gamma.gamma, 2.2f);

    ImageSpec bare(4, 4, 3, TypeHalf);
    auto p2 = plan_color_metadata(nullptr, bare, all_caps(), pol);
    OIIO_CHECK_EQUAL(int(p2.chromaticities.action), int(ColorPlanAction::Omit));
    OIIO_CHECK_EQUAL(int(p2.gamma.action), int(ColorPlanAction::Omit));
}


// Name -> interop id and name -> CICP derivation, exercised against the config
// (branching on what the config can actually resolve, so the vector is
// deterministic regardless of the config's coverage).
static void
test_derivation(const ColorConfig& config)
{
    ColorWritePolicy pol;
    ImageSpec spec(4, 4, 3, TypeHalf);
    spec.attribute("oiio:ColorSpace", "srgb_rec709_display");

    auto p = plan_color_metadata(&config, spec, all_caps(), pol);

    // The plan's Derive path runs the full derivation cascade, so the
    // expectation comes from pvt::derive_color_interop_id, not the cheap
    // public lookup.
    const std::string want_id(
        derive_color_interop_id(config, "srgb_rec709_display"));
    if (!want_id.empty()) {
        OIIO_CHECK_EQUAL(int(p.interop_id.action), int(ColorPlanAction::Derive));
        OIIO_CHECK_EQUAL(p.interop_id.str, want_id);
    } else {
        OIIO_CHECK_EQUAL(int(p.interop_id.action), int(ColorPlanAction::Omit));
    }

    cspan<int> want_cicp = want_id.empty() ? cspan<int>()
                                           : config.get_cicp(want_id);
    if (want_cicp.size() == 4) {
        OIIO_CHECK_EQUAL(int(p.cicp.action), int(ColorPlanAction::Derive));
        OIIO_CHECK_EQUAL(p.cicp.ints.size(), size_t(4));
    } else {
        OIIO_CHECK_EQUAL(int(p.cicp.action), int(ColorPlanAction::Omit));
    }
}


// A MISLABELED config: a space whose name table-matches one interop id
// ("srgb_rec709_scene") but whose math (an identity transform against the
// AP0 interchange anchor) fingerprints to a different registry identity
// ("lin_ap0_scene"). The cheap declared/table subset is fooled by the name;
// the full cascade's equality (fingerprint) tier outranks it. The planner's
// Derive verdict must be the full cascade's answer, reached through the
// shared characterization engine -- and the direct cascade, the engine's
// derive tier, the public derive verb, and the plan must all agree
// bit-exact (the round-2 cheap-first divergence, resolved).
static void
test_mislabeled_config_derivation()
{
    if (!ColorConfig::supportsOpenColorIO())
        return;

    static const char* mislabeled_yaml = R"(ocio_profile_version: 2.1
name: mislabeled_cfg
search_path: ""
roles:
  default: ref
  scene_linear: ref
  aces_interchange: ref
colorspaces:
  - !<ColorSpace>
    name: ref

  - !<ColorSpace>
    name: srgb_rec709_scene
)";
    const std::string path             = Filesystem::temp_directory_path()
                                         + "/oiio_cmp_mislabeled.ocio";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(path, mislabeled_yaml));
    ColorConfig cc(path);
    OIIO_CHECK_ASSERT(!cc.has_error());
    characterization_cache_reset();

    // The cheap subset answers the syntactic table match...
    OIIO_CHECK_EQUAL(cc.get_color_interop_id("srgb_rec709_scene"),
                     "srgb_rec709_scene");
    // ...but the full cascade's fingerprint tier outranks it.
    const std::string cascade(derive_color_interop_id(cc, "srgb_rec709_scene"));
    OIIO_CHECK_EQUAL(cascade, "lin_ap0_scene");

    // The engine's derive tier (via the public derive verb) agrees with the
    // cascade bit-exact, and reports the correction as a derived value.
    ColorSpaceInfo info = cc.derive_color_space_info("srgb_rec709_scene");
    OIIO_CHECK_ASSERT(info.valid());
    OIIO_CHECK_EQUAL(info.color_interop_id(), cascade);
    OIIO_CHECK_ASSERT(info.derived(ColorSpaceInfoField::ColorInteropID));

    // The corrected verdict survives the cache merge with a later fresh
    // cheap pass (the derived value outranks the table match).
    ColorSpaceInfo cheap = cc.get_color_space_info("srgb_rec709_scene");
    OIIO_CHECK_EQUAL(cheap.color_interop_id(), cascade);

    // And the planner's Derive verdict is that same answer.
    ColorWritePolicy pol;
    ImageSpec spec(4, 4, 3, TypeHalf);
    spec.attribute("oiio:ColorSpace", "srgb_rec709_scene");
    auto p = plan_color_metadata(&cc, spec, all_caps(), pol);
    OIIO_CHECK_EQUAL(int(p.interop_id.action), int(ColorPlanAction::Derive));
    OIIO_CHECK_EQUAL(p.interop_id.str, cascade);

    characterization_cache_reset();
    Filesystem::remove(path);
}


// The global oiio:colorpolicy:* tier: a value set through OIIO::attribute()
// must round-trip through OIIO::getattribute(), be visible to the policy
// snapshot, and actually change writer behavior end to end (write a file,
// reopen it, observe the signal gone) -- not merely alter a plan object.
static void
test_global_policy_tier()
{
    // Storage round-trip, string and int.
    OIIO_CHECK_ASSERT(
        OIIO::attribute("oiio:colorpolicy:write:interop_id", "never"));
    std::string v;
    OIIO_CHECK_ASSERT(
        OIIO::getattribute("oiio:colorpolicy:write:interop_id", v));
    OIIO_CHECK_EQUAL(v, "never");
    OIIO_CHECK_ASSERT(
        OIIO::attribute("oiio:colorpolicy:read:ignore_cicp_for_png", 1));
    int iv = 0;
    OIIO_CHECK_ASSERT(
        OIIO::getattribute("oiio:colorpolicy:read:ignore_cicp_for_png", iv));
    OIIO_CHECK_EQUAL(iv, 1);
    OIIO_CHECK_ASSERT(
        OIIO::attribute("oiio:colorpolicy:read:ignore_cicp_for_png", 0));

    // The write-policy snapshot sees the global (no per-spec hints in play).
    auto pol = ColorWritePolicy::snapshot();
    OIIO_CHECK_EQUAL(int(pol.interop_id), int(ColorSignalPolicy::Never));

    // End to end: an EXR write that would otherwise DERIVE an interop id from
    // the color space emits none while the global says never. Only meaningful
    // when the default config can derive one -- guard like test_exr_consumption.
    const std::string derivable(
        derive_color_interop_id(ColorConfig::default_colorconfig(),
                                "lin_ap0_scene"));
    if (!derivable.empty() && ImageOutput::create("exr")) {
        const std::string file = Filesystem::temp_directory_path()
                                 + "/oiio_cmp_globalpolicy.exr";
        std::vector<float> pix(4 * 4 * 3, 0.5f);
        ImageSpec spec(4, 4, 3, TypeHalf);
        spec.attribute("oiio:ColorSpace", "lin_ap0_scene");
        auto o = ImageOutput::create(file);
        OIIO_CHECK_ASSERT(o && o->open(file, spec));
        if (o) {
            OIIO_CHECK_ASSERT(o->write_image(TypeFloat, pix.data()));
            OIIO_CHECK_ASSERT(o->close());
        }
        auto in = ImageInput::open(file);
        OIIO_CHECK_ASSERT(in.get());
        if (in) {
            OIIO_CHECK_EQUAL(
                in->spec().get_string_attribute("colorInteropID"), "");
            in->close();
        }
        Filesystem::remove(file);
    }

    // Restore the default so later tests see auto behavior.
    OIIO_CHECK_ASSERT(
        OIIO::attribute("oiio:colorpolicy:write:interop_id", "auto"));
    pol = ColorWritePolicy::snapshot();
    OIIO_CHECK_EQUAL(int(pol.interop_id), int(ColorSignalPolicy::Auto));
}


// Per-spec / per-open policy hints must reach every reconcile/plan call
// site: a hint on the output spec (write) or the open-config spec (read)
// overrides the global tier, on both PNG and EXR, read and write.
static void
test_policy_hint_plumbing()
{
    std::vector<float> fpix(4 * 4 * 3, 0.5f);
    std::vector<unsigned char> upix(4 * 4 * 3, 128);

    // --- EXR write: per-spec hint overrides a global 'never'. ------------
    const std::string derivable(
        derive_color_interop_id(ColorConfig::default_colorconfig(),
                                "lin_ap0_scene"));
    if (!derivable.empty() && ImageOutput::create("exr")) {
        const std::string file = Filesystem::temp_directory_path()
                                 + "/oiio_cmp_hints.exr";
        OIIO_CHECK_ASSERT(
            OIIO::attribute("oiio:colorpolicy:write:interop_id", "never"));
        ImageSpec spec(4, 4, 3, TypeHalf);
        spec.attribute("oiio:ColorSpace", "lin_ap0_scene");
        spec.attribute("oiio:colorpolicy:write:interop_id", "auto");
        auto o = ImageOutput::create(file);
        OIIO_CHECK_ASSERT(o && o->open(file, spec));
        if (o) {
            OIIO_CHECK_ASSERT(o->write_image(TypeFloat, fpix.data()));
            OIIO_CHECK_ASSERT(o->close());
        }
        OIIO_CHECK_ASSERT(
            OIIO::attribute("oiio:colorpolicy:write:interop_id", "auto"));
        auto in = ImageInput::open(file);
        OIIO_CHECK_ASSERT(in.get());
        if (in) {
            // The per-spec 'auto' beat the global 'never': the id derived.
            OIIO_CHECK_EQUAL(
                in->spec().get_string_attribute("colorInteropID"), derivable);
            in->close();
        }
        Filesystem::remove(file);
    }

    // --- EXR read: per-open config hint overrides a global preference. ---
    if (ImageOutput::create("exr")) {
        const std::string file = Filesystem::temp_directory_path()
                                 + "/oiio_cmp_hints_read.exr";
        ImageSpec spec(4, 4, 3, TypeHalf);
        spec.attribute("colorInteropID", "srgb_rec709_display");
        auto o = ImageOutput::create(file);
        OIIO_CHECK_ASSERT(o && o->open(file, spec));
        if (o) {
            OIIO_CHECK_ASSERT(o->write_image(TypeFloat, fpix.data()));
            OIIO_CHECK_ASSERT(o->close());
        }
        // Reader-parity guard: run the probes under BOTH EXR readers
        // (openexr:core=0 -> OpenEXRInput, =1 -> OpenEXRCoreInput), so a
        // policy wired into only one reader fails here regardless of which
        // one the build defaults to.
        int core_orig = 0;
        OIIO::getattribute("openexr:core", core_orig);
        for (int core : { 0, 1 }) {
            OIIO_CHECK_ASSERT(OIIO::attribute("openexr:core", core));
            // Global tier: prefer the scene-state twin of the file's id.
            OIIO_CHECK_ASSERT(OIIO::attribute(
                "oiio:colorpolicy:read:state_preference", "scene"));
            auto in = ImageInput::open(file);
            OIIO_CHECK_ASSERT(in.get());
            if (in) {
                OIIO_CHECK_EQUAL(
                    in->spec().get_string_attribute("oiio:ColorSpace"),
                    "srgb_rec709_scene");
                in->close();
            }
            // Per-open hint: put the display preference back for THIS open
            // only.
            ImageSpec config;
            config.attribute("oiio:colorpolicy:read:state_preference",
                             "display");
            auto in2 = ImageInput::open(file, &config);
            OIIO_CHECK_ASSERT(in2.get());
            if (in2) {
                OIIO_CHECK_EQUAL(
                    in2->spec().get_string_attribute("oiio:ColorSpace"),
                    "srgb_rec709_display");
                in2->close();
            }
            OIIO_CHECK_ASSERT(OIIO::attribute(
                "oiio:colorpolicy:read:state_preference", "auto"));
        }
        OIIO::attribute("openexr:core", core_orig);
        Filesystem::remove(file);
    }

    // --- PNG: capability probe -- explicit CICP must round-trip at all
    // (libpng without cICP support skips the PNG halves). -----------------
    const int cicp_srgb[4] = { 1, 13, 0, 1 };
    bool png_cicp_ok       = false;
    const std::string pngfile = Filesystem::temp_directory_path()
                                + "/oiio_cmp_hints.png";
    if (ImageOutput::create("png")) {
        ImageSpec spec(4, 4, 3, TypeUInt8);
        spec.attribute("CICP", TypeDesc(TypeDesc::INT, 4), cicp_srgb);
        auto o = ImageOutput::create(pngfile);
        OIIO_CHECK_ASSERT(o && o->open(pngfile, spec));
        if (o) {
            OIIO_CHECK_ASSERT(o->write_image(TypeUInt8, upix.data()));
            OIIO_CHECK_ASSERT(o->close());
        }
        auto in = ImageInput::open(pngfile);
        if (in) {
            int got[4] = { -1, -1, -1, -1 };
            png_cicp_ok = in->spec().getattribute("CICP",
                                                  TypeDesc(TypeDesc::INT, 4),
                                                  got);
            in->close();
        }
    }
    if (!png_cicp_ok) {
        Strutil::print("PNG cICP unavailable; skipping PNG hint plumbing\n");
        Filesystem::remove(pngfile);
        return;
    }

    // --- PNG read: per-open config hint changes the CICP resolution. -----
    {
        // No hint: the sRGB CICP tuple resolves display-referred.
        auto in = ImageInput::open(pngfile);
        OIIO_CHECK_ASSERT(in.get());
        if (in) {
            OIIO_CHECK_EQUAL(
                in->spec().get_string_attribute("oiio:ColorSpace"),
                "srgb_rec709_display");
            in->close();
        }
        // Per-open hint: prefer the scene-state twin.
        ImageSpec config;
        config.attribute("oiio:colorpolicy:read:state_preference", "scene");
        auto in2 = ImageInput::open(pngfile, &config);
        OIIO_CHECK_ASSERT(in2.get());
        if (in2) {
            OIIO_CHECK_EQUAL(
                in2->spec().get_string_attribute("oiio:ColorSpace"),
                "srgb_rec709_scene");
            in2->close();
        }
    }

    // --- PNG write: per-spec hint overrides a global 'never'. ------------
    {
        OIIO_CHECK_ASSERT(
            OIIO::attribute("oiio:colorpolicy:write:cicp", "never"));
        ImageSpec spec(4, 4, 3, TypeUInt8);
        spec.attribute("CICP", TypeDesc(TypeDesc::INT, 4), cicp_srgb);
        spec.attribute("oiio:colorpolicy:write:cicp", "auto");
        auto o = ImageOutput::create(pngfile);
        OIIO_CHECK_ASSERT(o && o->open(pngfile, spec));
        if (o) {
            OIIO_CHECK_ASSERT(o->write_image(TypeUInt8, upix.data()));
            OIIO_CHECK_ASSERT(o->close());
        }
        OIIO_CHECK_ASSERT(
            OIIO::attribute("oiio:colorpolicy:write:cicp", "auto"));
        auto in = ImageInput::open(pngfile);
        OIIO_CHECK_ASSERT(in.get());
        if (in) {
            int got[4] = { -1, -1, -1, -1 };
            // The per-spec 'auto' beat the global 'never': the chunk exists.
            OIIO_CHECK_ASSERT(in->spec().getattribute(
                "CICP", TypeDesc(TypeDesc::INT, 4), got));
            in->close();
        }
    }
    Filesystem::remove(pngfile);
}


// A Suppress verdict must be enforced at the WRITER boundary, not just in
// the plan object: an author-supplied value under a 'never' policy stays out
// of the written file even though the attribute sits on the spec (the EXR
// generic-metadata loop would otherwise emit it anyway).
static void
test_writer_level_suppress()
{
    // EXR: author colorInteropID + per-spec never -> reopened file carries
    // no colorInteropID.
    if (ImageOutput::create("exr")) {
        const std::string file = Filesystem::temp_directory_path()
                                 + "/oiio_cmp_suppress.exr";
        std::vector<float> pix(4 * 4 * 3, 0.5f);
        ImageSpec spec(4, 4, 3, TypeHalf);
        spec.attribute("colorInteropID", "lin_adobergb_scene");
        spec.attribute("oiio:colorpolicy:write:interop_id", "never");
        auto o = ImageOutput::create(file);
        OIIO_CHECK_ASSERT(o && o->open(file, spec));
        if (o) {
            OIIO_CHECK_ASSERT(o->write_image(TypeFloat, pix.data()));
            OIIO_CHECK_ASSERT(o->close());
        }
        auto in = ImageInput::open(file);
        OIIO_CHECK_ASSERT(in.get());
        if (in) {
            OIIO_CHECK_ASSERT(
                !in->spec().find_attribute("colorInteropID", TypeString));
            in->close();
        }
        Filesystem::remove(file);
    }

    // PNG: author CICP + per-spec never -> reopened file carries no cICP
    // chunk (the writer's emit gate already enforces this; guard it).
    if (ImageOutput::create("png")) {
        const std::string file = Filesystem::temp_directory_path()
                                 + "/oiio_cmp_suppress.png";
        std::vector<unsigned char> pix(4 * 4 * 3, 128);
        const int cicp[4] = { 1, 13, 0, 1 };
        ImageSpec spec(4, 4, 3, TypeUInt8);
        spec.attribute("CICP", TypeDesc(TypeDesc::INT, 4), cicp);
        spec.attribute("oiio:colorpolicy:write:cicp", "never");
        auto o = ImageOutput::create(file);
        OIIO_CHECK_ASSERT(o && o->open(file, spec));
        if (o) {
            OIIO_CHECK_ASSERT(o->write_image(TypeUInt8, pix.data()));
            OIIO_CHECK_ASSERT(o->close());
        }
        auto in = ImageInput::open(file);
        OIIO_CHECK_ASSERT(in.get());
        if (in) {
            int got[4] = { -1, -1, -1, -1 };
            OIIO_CHECK_ASSERT(!in->spec().getattribute(
                "CICP", TypeDesc(TypeDesc::INT, 4), got));
            in->close();
        }
        Filesystem::remove(file);
    }
}


// Layer attribution: every planned field records who decided it -- format
// incapability, the author's explicit metadata, or the policy tier (builtin
// default / global attribute / per-spec attribute) that was in force.
static void
test_layer_attribution()
{
    // Builtin default (no policy set anywhere) and format incapability.
    {
        ImageSpec spec(4, 4, 3, TypeHalf);
        auto p = plan_color_metadata(nullptr, spec, all_caps(),
                                     ColorWritePolicy::snapshot());
        OIIO_CHECK_EQUAL(int(p.cicp.decider),
                         int(ColorPlanDecider::BuiltinDefault));
        ColorWriteCaps none;  // nothing supported
        auto p2 = plan_color_metadata(nullptr, spec, none,
                                      ColorWritePolicy::snapshot());
        OIIO_CHECK_EQUAL(int(p2.cicp.decider),
                         int(ColorPlanDecider::FormatIncapable));
        OIIO_CHECK_EQUAL(int(p2.interop_id.decider),
                         int(ColorPlanDecider::FormatIncapable));
    }

    // Author-supplied metadata wins the attribution on a Write verdict.
    {
        ImageSpec spec(4, 4, 3, TypeHalf);
        spec.attribute("colorInteropID", "lin_adobergb_scene");
        auto p = plan_color_metadata(nullptr, spec, all_caps(),
                                     ColorWritePolicy::snapshot());
        OIIO_CHECK_EQUAL(int(p.interop_id.decider),
                         int(ColorPlanDecider::ExplicitMetadata));
    }

    // A global attribute decides -- even an explicit value bows to its
    // "never" -- and a per-spec hint outranks (and re-attributes) it.
    {
        OIIO_CHECK_ASSERT(
            OIIO::attribute("oiio:colorpolicy:write:interop_id", "never"));
        ImageSpec spec(4, 4, 3, TypeHalf);
        spec.attribute("colorInteropID", "lin_adobergb_scene");
        auto p = plan_color_metadata(nullptr, spec, all_caps(),
                                     ColorWritePolicy::snapshot(&spec));
        OIIO_CHECK_EQUAL(int(p.interop_id.action),
                         int(ColorPlanAction::Suppress));
        OIIO_CHECK_EQUAL(int(p.interop_id.decider),
                         int(ColorPlanDecider::GlobalAttribute));

        spec.attribute("oiio:colorpolicy:write:interop_id", "auto");
        auto p2 = plan_color_metadata(nullptr, spec, all_caps(),
                                      ColorWritePolicy::snapshot(&spec));
        OIIO_CHECK_EQUAL(int(p2.interop_id.action), int(ColorPlanAction::Write));
        OIIO_CHECK_EQUAL(int(p2.interop_id.decider),
                         int(ColorPlanDecider::ExplicitMetadata));

        spec.attribute("oiio:colorpolicy:write:interop_id", "never");
        auto p3 = plan_color_metadata(nullptr, spec, all_caps(),
                                      ColorWritePolicy::snapshot(&spec));
        OIIO_CHECK_EQUAL(int(p3.interop_id.action),
                         int(ColorPlanAction::Suppress));
        OIIO_CHECK_EQUAL(int(p3.interop_id.decider),
                         int(ColorPlanDecider::PerSpecAttribute));
        // Restore to UNSET ("") -- not "auto", which would leave the global
        // tier attributed for everything after us.
        OIIO_CHECK_ASSERT(
            OIIO::attribute("oiio:colorpolicy:write:interop_id", ""));
    }

    // The name->caps table matches what the wired writers declare.
    {
        ColorWriteCaps png = color_write_caps_for_format("png");
        OIIO_CHECK_ASSERT(png.cicp && !png.interop_id && !png.icc);
        ColorWriteCaps exr = color_write_caps_for_format("openexr");
        OIIO_CHECK_ASSERT(exr.interop_id && !exr.cicp);
        ColorWriteCaps exr2 = color_write_caps_for_format("EXR");
        OIIO_CHECK_ASSERT(exr2.interop_id);
        ColorWriteCaps none = color_write_caps_for_format("tiff");
        OIIO_CHECK_ASSERT(!none.cicp && !none.interop_id && !none.icc
                          && !none.chromaticities && !none.gamma && !none.mdcv);
    }
}


// The provenance write rule: suppress oiio:SourcePath, keep oiio:SourceFormat.
static void
test_provenance_rule()
{
    ImageSpec spec(4, 4, 3, TypeHalf);
    auto p = plan_color_metadata(nullptr, spec, all_caps(), ColorWritePolicy());
    OIIO_CHECK_ASSERT(p.suppress_source_path);
    OIIO_CHECK_ASSERT(p.keep_source_format);
}


// The OpenEXR writer consumes the plan: an author-supplied id survives a
// write/read round-trip untouched, and a spec that only names a color space
// gets exactly the id the plan derives (or none, matching the default config).
static void
test_exr_consumption()
{
    auto out = ImageOutput::create("exr");
    if (!out) {
        Strutil::print("EXR plugin unavailable; skipping consumption round-trip\n");
        return;
    }
    const std::string file = Filesystem::temp_directory_path()
                             + "/oiio_cmp_roundtrip.exr";
    std::vector<float> pix(4 * 4 * 3, 0.5f);

    auto write = [&](const ImageSpec& spec) {
        auto o = ImageOutput::create(file);
        OIIO_CHECK_ASSERT(o && o->open(file, spec));
        if (o) {
            OIIO_CHECK_ASSERT(o->write_image(TypeFloat, pix.data()));
            OIIO_CHECK_ASSERT(o->close());
        }
    };
    auto read_id = [&]() -> std::string {
        auto in = ImageInput::open(file);
        if (!in)
            return "<no-read>";
        std::string id = in->spec().get_string_attribute("colorInteropID");
        in->close();
        return id;
    };

    // Author value is emitted verbatim.
    {
        ImageSpec spec(4, 4, 3, TypeHalf);
        spec.attribute("colorInteropID", "lin_adobergb_scene");
        write(spec);
        OIIO_CHECK_EQUAL(read_id(), "lin_adobergb_scene");
    }

    // An explicitly-set attr of "unknown" is also verbatim -- in ALL modes.
    // The author's bytes are sacred: OIIO never rewrites them into a
    // namespaced marker (the "ocio:unknown" marker is derivation-only, for
    // configs that themselves declare unknownness).
    {
        ImageSpec spec(4, 4, 3, TypeHalf);
        spec.attribute("colorInteropID", "unknown");
        write(spec);
        OIIO_CHECK_EQUAL(read_id(), "unknown");
    }

    // A color-space-only spec gets the plan's derived id (default config).
    {
        ImageSpec spec(4, 4, 3, TypeHalf);
        spec.attribute("oiio:ColorSpace", "lin_ap0_scene");
        write(spec);
        const std::string want(
            derive_color_interop_id(ColorConfig::default_colorconfig(),
                                    "lin_ap0_scene"));
        OIIO_CHECK_EQUAL(read_id(), want);
    }

    Filesystem::remove(file);
}


// B7 (spec 07): color identity is scoped to the whole file and emitted in
// the FIRST part's header only. A multi-part EXR written with a colorInteropID
// must carry the resolved id on part 0; a later part may carry only the "data"
// utility token, never a duplicated color identity.
static void
test_exr_multipart_first_part_only()
{
    if (!ImageOutput::create("exr")) {
        Strutil::print("EXR plugin unavailable; skipping multi-part B7\n");
        return;
    }
    const std::string file = Filesystem::temp_directory_path()
                             + "/oiio_cmp_multipart.exr";
    std::vector<float> pix(4 * 4 * 3, 0.5f);

    ImageSpec s0(4, 4, 3, TypeHalf);
    s0.attribute("colorInteropID", "lin_adobergb_scene");
    s0.attribute("name", "part0");
    // A later part authored (wrongly) with the same color identity: B7 must
    // strip it, since a later part may only ever carry "data".
    ImageSpec s1(4, 4, 3, TypeHalf);
    s1.attribute("colorInteropID", "lin_adobergb_scene");
    s1.attribute("name", "part1");
    ImageSpec specs[2] = { s0, s1 };

    auto o = ImageOutput::create(file);
    OIIO_CHECK_ASSERT(o && o->supports("multiimage"));
    if (o && o->supports("multiimage")) {
        OIIO_CHECK_ASSERT(o->open(file, 2, specs));
        OIIO_CHECK_ASSERT(o->write_image(TypeFloat, pix.data()));
        OIIO_CHECK_ASSERT(
            o->open(file, specs[1], ImageOutput::AppendSubimage));
        OIIO_CHECK_ASSERT(o->write_image(TypeFloat, pix.data()));
        OIIO_CHECK_ASSERT(o->close());

        auto in = ImageInput::open(file);
        OIIO_CHECK_ASSERT(in.get());
        if (in) {
            // Part 0 keeps the color identity.
            OIIO_CHECK_ASSERT(in->seek_subimage(0, 0));
            OIIO_CHECK_EQUAL(
                in->spec().get_string_attribute("colorInteropID"),
                "lin_adobergb_scene");
            // Part 1 must NOT carry the duplicated identity (over-tagging).
            OIIO_CHECK_ASSERT(in->seek_subimage(1, 0));
            OIIO_CHECK_EQUAL(
                in->spec().get_string_attribute("colorInteropID"), "");
            in->close();
        }
    }
    Filesystem::remove(file);
}


// B5 (spec 07): writing a colorInteropID means NOT also writing
// chromaticities -- a stale/redundant chromaticities attribute is dropped.
// The B4 exception is an ST 2065-4 / ACES container, which must KEEP its
// required AP0 chromaticities.
static void
test_exr_chromaticities_dropped_and_aces_kept()
{
    if (!ImageOutput::create("exr")) {
        Strutil::print("EXR plugin unavailable; skipping B5 chromaticities\n");
        return;
    }
    static const float ap0[8] = { 0.7347f, 0.2653f, 0.0f,     1.0f,
                                  0.0001f, -0.077f, 0.32168f, 0.33767f };
    // A plausible-but-stale non-AP0 chromaticities set (Rec.709 primaries).
    static const float rec709[8] = { 0.64f, 0.33f, 0.30f,   0.60f,
                                     0.15f, 0.06f, 0.3127f, 0.3290f };
    std::vector<float> pix(4 * 4 * 3, 0.5f);

    // (a) colorInteropID + stale chromaticities, not an ACES container: the
    // chromaticities must be dropped.
    {
        const std::string file = Filesystem::temp_directory_path()
                                 + "/oiio_cmp_b5_drop.exr";
        ImageSpec spec(4, 4, 3, TypeHalf);
        spec.attribute("colorInteropID", "lin_adobergb_scene");
        spec.attribute("chromaticities", TypeDesc(TypeDesc::FLOAT, 8), rec709);
        auto o = ImageOutput::create(file);
        OIIO_CHECK_ASSERT(o && o->open(file, spec));
        if (o) {
            OIIO_CHECK_ASSERT(o->write_image(TypeFloat, pix.data()));
            OIIO_CHECK_ASSERT(o->close());
        }
        auto in = ImageInput::open(file);
        OIIO_CHECK_ASSERT(in.get());
        if (in) {
            OIIO_CHECK_EQUAL(
                in->spec().get_string_attribute("colorInteropID"),
                "lin_adobergb_scene");
            OIIO_CHECK_ASSERT(
                !in->spec().find_attribute("chromaticities"));
            in->close();
        }
        Filesystem::remove(file);
    }

    // (b) ACES container (B4): the required AP0 chromaticities are kept
    // alongside colorInteropID = lin_ap0_scene.
    {
        const std::string file = Filesystem::temp_directory_path()
                                 + "/oiio_cmp_b5_aces.exr";
        ImageSpec spec(4, 4, 3, TypeHalf);
        spec.channelnames = { "R", "G", "B" };
        spec.attribute("compression", "none");
        spec.attribute("acesImageContainerFlag", 1);
        spec.attribute("colorInteropID", "lin_ap0_scene");
        spec.attribute("chromaticities", TypeDesc(TypeDesc::FLOAT, 8), ap0);
        auto o = ImageOutput::create(file);
        OIIO_CHECK_ASSERT(o && o->open(file, spec));
        if (o) {
            OIIO_CHECK_ASSERT(o->write_image(TypeFloat, pix.data()));
            OIIO_CHECK_ASSERT(o->close());
        }
        auto in = ImageInput::open(file);
        OIIO_CHECK_ASSERT(in.get());
        if (in) {
            OIIO_CHECK_EQUAL(
                in->spec().get_string_attribute("colorInteropID"),
                "lin_ap0_scene");
            OIIO_CHECK_ASSERT(
                in->spec().find_attribute("chromaticities") != nullptr);
            in->close();
        }
        Filesystem::remove(file);
    }
}


// B9.1 (spec 07): an author-supplied colorInteropID must be grammar-valid
// (spec 01) or be omitted -- a malformed id is never written.
static void
test_exr_invalid_id_omitted()
{
    if (!ImageOutput::create("exr")) {
        Strutil::print("EXR plugin unavailable; skipping B9.1 validation\n");
        return;
    }
    const std::string file = Filesystem::temp_directory_path()
                             + "/oiio_cmp_b91.exr";
    std::vector<float> pix(4 * 4 * 3, 0.5f);
    ImageSpec spec(4, 4, 3, TypeHalf);
    // Uppercase + too many colons -> grammar-invalid per is_valid_interop_id.
    spec.attribute("colorInteropID", "Totally Bogus:::Name");
    auto o = ImageOutput::create(file);
    OIIO_CHECK_ASSERT(o && o->open(file, spec));
    if (o) {
        OIIO_CHECK_ASSERT(o->write_image(TypeFloat, pix.data()));
        OIIO_CHECK_ASSERT(o->close());
    }
    auto in = ImageInput::open(file);
    OIIO_CHECK_ASSERT(in.get());
    if (in) {
        OIIO_CHECK_ASSERT(
            !in->spec().find_attribute("colorInteropID", TypeString));
        in->close();
    }
    Filesystem::remove(file);
}


int
main(int /*argc*/, char* /*argv*/[])
{
    test_interop_id_write_and_omit();
    test_never_suppresses();
    test_incapable_omits();
    test_explicit_chroma_and_gamma();
    test_mislabeled_config_derivation();
    test_layer_attribution();
    test_global_policy_tier();
    test_policy_hint_plumbing();
    test_writer_level_suppress();
    test_provenance_rule();
    test_exr_consumption();
    test_exr_multipart_first_part_only();
    test_exr_chromaticities_dropped_and_aces_kept();
    test_exr_invalid_id_omitted();

    const std::string cfgpath = write_test_config();
    ColorConfig config(cfgpath);
    if (config.has_error()) {
        Strutil::print("Could not load test config: {}\n", config.geterror());
        return 1;
    }
    test_derivation(config);
    Filesystem::remove(cfgpath);

    return unit_test_failures != 0;
}
