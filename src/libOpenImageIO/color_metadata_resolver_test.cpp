// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// Unit tests for the read-side color-metadata reconciler (pvt), driven
// directly through color_pvt.h. The vectors port a proven prototype's
// precedence, utility-token, strict-parsing, CICP-over-ICC, and
// filename-invariance cases.

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "color_pvt.h"
#include <OpenImageIO/color.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>

#include <OpenImageIO/unittest.h>

#include "imageio_pvt.h"

using namespace OIIO;
using namespace OIIO::pvt;


// A small, valid OCIO config carrying the identities the config-dependent
// vectors resolve against. Written to a temp file and loaded as a
// ColorConfig (OCIO is enabled in this build).
static std::string
write_test_config()
{
    std::string path = Filesystem::temp_directory_path()
                       + "/oiio_cmr_test.ocio";
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


static ColorMetadataFacts
ciid_facts(const std::string& id)
{
    ColorMetadataFacts f;
    f.color_interop_id = id;
    return f;
}

static ColorMetadataFacts
cicp_facts(int p, int t, int m, int r)
{
    ColorMetadataFacts f;
    f.has_cicp = true;
    f.cicp[0]  = p;
    f.cicp[1]  = t;
    f.cicp[2]  = m;
    f.cicp[3]  = r;
    return f;
}


// ACES container flag wins outright (rule 2), no config needed.
static void
test_aces_container()
{
    ColorMetadataFacts f;
    f.aces_image_container = true;
    auto e                 = resolve_color_metadata(nullptr, "", f, { }, { });
    OIIO_CHECK_EQUAL(e.resolved, "lin_ap0_scene");
    OIIO_CHECK_ASSERT(e.has_genuine_metadata_match());
}


// A known registry colorInteropID with no config bridges to itself verbatim.
static void
test_ciid_registry_bridge()
{
    auto e = resolve_color_metadata(nullptr, "",
                                    ciid_facts("lin_adobergb_scene"), { }, { });
    OIIO_CHECK_EQUAL(e.resolved, "lin_adobergb_scene");
    OIIO_CHECK_ASSERT(e.has_genuine_metadata_match());
}


// Vector 2: ciid="unknown" is an unusable payload -- it falls through, and
// the CICP identity resolves instead. (OIIO's CICP table maps (1,13) to the
// display-referred identity -- CICP describes display encodings, so
// srgb_rec709_display is listed ahead of the scene twin and wins on read;
// OIIO's central mapping is authoritative here.)
static void
test_unknown_ciid_falls_through_to_cicp(const ColorConfig& config)
{
    ColorMetadataFacts f = cicp_facts(1, 13, 0, 1);
    f.color_interop_id   = "unknown";
    auto e               = resolve_color_metadata(&config, "", f, { }, { });
    OIIO_CHECK_EQUAL(e.resolved, "srgb_rec709_display");
    // The interop-id rule was visited and missed; CICP matched.
    bool saw_ciid_missed = false, saw_cicp_matched = false;
    for (auto& s : e.steps) {
        if (s.rule == ColorRule::ColorInteropID
            && s.outcome == ColorRuleOutcome::Missed)
            saw_ciid_missed = true;
        if (s.rule == ColorRule::Cicp && s.outcome == ColorRuleOutcome::Matched)
            saw_cicp_matched = true;
    }
    OIIO_CHECK_ASSERT(saw_ciid_missed);
    OIIO_CHECK_ASSERT(saw_cicp_matched);
}


// Vector 12: data / bypass. With a local isData space it resolves to that
// space; with no config at all the literal token is the answer.
static void
test_utility_tokens(const ColorConfig& config)
{
    auto e = resolve_color_metadata(&config, "", ciid_facts("data"), { }, { });
    OIIO_CHECK_EQUAL(e.resolved, "raw_data");

    auto e2 = resolve_color_metadata(nullptr, "", ciid_facts("data"), { }, { });
    OIIO_CHECK_EQUAL(e2.resolved, "data");
    // Exactly one step, the interop-id rule, matched.
    OIIO_CHECK_EQUAL(e2.steps.size(), size_t(1));
    OIIO_CHECK_EQUAL(int(e2.steps[0].rule), int(ColorRule::ColorInteropID));
}


// Vector 9: an explicit assignment that misses, under a non-lenient scope,
// yields the literal "unknown" in exactly two steps and never consults the
// metadata behind it.
static void
test_strict_terminal(const ColorConfig& config)
{
    ColorReadPolicy p;
    p.scope              = ColorResolutionScope::ConfigOnly;
    ColorMetadataFacts f = ciid_facts("lin_ap1_scene");  // must be ignored
    auto e = resolve_color_metadata(&config, "not-a-space", f, { }, p);
    OIIO_CHECK_EQUAL(e.resolved, "unknown");
    OIIO_CHECK_EQUAL(e.steps.size(), size_t(2));
    OIIO_CHECK_EQUAL(int(e.steps[0].rule), int(ColorRule::ExplicitAssignment));
    OIIO_CHECK_EQUAL(int(e.steps[0].outcome), int(ColorRuleOutcome::Missed));
    OIIO_CHECK_EQUAL(int(e.steps[1].rule), int(ColorRule::StrictParsing));
    OIIO_CHECK_ASSERT(!e.has_genuine_metadata_match());
}


// Vector 16: garbage ICC bytes are invalid and fall through. (CICP now sits
// above ICC, so the fall-through is observed on a lone garbage profile: the
// ICC step records Invalid and no genuine metadata match results.)
static void
test_garbage_icc_falls_through(const ColorConfig& config)
{
    ColorMetadataFacts f;
    f.icc_profile = std::vector<unsigned char>(200, 0x42);
    auto e        = resolve_color_metadata(&config, "", f, { }, { });
    OIIO_CHECK_ASSERT(!e.has_genuine_metadata_match());
    bool icc_invalid = false;
    for (auto& s : e.steps)
        if (s.rule == ColorRule::IccProfile
            && s.outcome == ColorRuleOutcome::Invalid)
            icc_invalid = true;
    OIIO_CHECK_ASSERT(icc_invalid);
}


// Build a minimally-decodable ICC profile: a >=128-byte header carrying the
// 'acsp' signature at offset 36.
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


// Vector 18: a decodable-but-unmatched ICC profile resolves to its own
// registered synthetic id.
static void
test_icc_synthetic(const ColorConfig& config)
{
    ColorMetadataFacts f;
    f.icc_profile = fake_icc_profile();
    auto e        = resolve_color_metadata(&config, "", f, { }, { });
    OIIO_CHECK_ASSERT(Strutil::starts_with(e.resolved, "icc:"));
    OIIO_CHECK_EQUAL(e.resolved, e.registered_synthetic);
}


// CICP sits ABOVE ICC, per the PNG spec's own chunk precedence
// (cICP > iCCP). A CICP tuple plus a decodable ICC profile resolves via
// CICP, not ICC; the ICC rule is never even visited.
static void
test_cicp_over_icc(const ColorConfig& config)
{
    ColorMetadataFacts f = cicp_facts(1, 13, 0, 1);
    f.icc_profile        = fake_icc_profile();
    auto e               = resolve_color_metadata(&config, "", f, { }, { });
    OIIO_CHECK_EQUAL(e.resolved, "srgb_rec709_display");
    OIIO_CHECK_ASSERT(!Strutil::starts_with(e.resolved, "icc:"));
    for (auto& s : e.steps)
        OIIO_CHECK_ASSERT(s.rule != ColorRule::IccProfile);
}


// Spec-impossible CICP values are log-only (debugfmt): resolution behavior
// is exactly as before. A reserved primaries/transfer of 0 misses the
// identity mapping and falls through; a non-zero matrix byte never affects
// identification (primaries + transfer only). The debug line itself is not
// asserted here -- debugfmt is env-gated and capturing stderr isn't worth
// the plumbing for a log-only path.
static void
test_spec_impossible_cicp(const ColorConfig& config)
{
    // Reserved (0,0): no identity mapping, no genuine match, CICP missed.
    auto e = resolve_color_metadata(&config, "", cicp_facts(0, 0, 0, 0), { },
                                    { });
    OIIO_CHECK_ASSERT(!e.has_genuine_metadata_match());
    bool cicp_missed = false;
    for (auto& s : e.steps)
        if (s.rule == ColorRule::Cicp && s.outcome == ColorRuleOutcome::Missed)
            cicp_missed = true;
    OIIO_CHECK_ASSERT(cicp_missed);

    // Non-zero matrix byte: identical resolution to the (1,13,0,*) vectors.
    auto e2 = resolve_color_metadata(&config, "", cicp_facts(1, 13, 5, 1), { },
                                     { });
    OIIO_CHECK_EQUAL(e2.resolved, "srgb_rec709_display");
    OIIO_CHECK_ASSERT(e2.has_genuine_metadata_match());
}


// Vector 7: under metadata-only file-rules placement, the same metadata with
// and without a FileRules-matching filename produces equal resolved values
// AND byte-identical step traces, with both FileRules rungs inapplicable.
static void
test_filename_invariance(const ColorConfig& config)
{
    ColorMetadataFacts f = cicp_facts(1, 13, 0, 1);
    ColorCallContext no_name;
    ColorCallContext with_name;
    with_name.filename = "/tmp/frame.g24_rec709_display.exr";
    auto a             = resolve_color_metadata(&config, "", f, no_name, { });
    auto b             = resolve_color_metadata(&config, "", f, with_name, { });
    OIIO_CHECK_EQUAL(a.resolved, b.resolved);
    OIIO_CHECK_EQUAL(a.steps.size(), b.steps.size());
    for (size_t i = 0; i < a.steps.size() && i < b.steps.size(); ++i) {
        OIIO_CHECK_EQUAL(int(a.steps[i].rule), int(b.steps[i].rule));
        OIIO_CHECK_EQUAL(int(a.steps[i].outcome), int(b.steps[i].outcome));
        OIIO_CHECK_EQUAL(a.steps[i].candidate, b.steps[i].candidate);
        OIIO_CHECK_EQUAL(a.steps[i].resolved, b.steps[i].resolved);
        OIIO_CHECK_EQUAL(a.steps[i].reason, b.steps[i].reason);
    }
    for (auto& s : a.steps) {
        if (s.rule == ColorRule::FileRulesFirst
            || s.rule == ColorRule::FileRulesFallback)
            OIIO_CHECK_EQUAL(int(s.outcome),
                             int(ColorRuleOutcome::Inapplicable));
    }
}


// The central entry point stamps the color space exactly as the reader's
// former inline code did: ACES flag -> lin_ap0_scene; colorInteropID ->
// verbatim; neither -> nothing.
static void
test_reconcile_entry_point()
{
    {
        ImageSpec spec(4, 4, 3, TypeFloat);
        spec.attribute("acesImageContainerFlag", 1);
        reconcile_color_metadata(spec, ColorReadPolicy());
        OIIO_CHECK_EQUAL(spec.get_string_attribute("oiio:ColorSpace"),
                         "lin_ap0_scene");
    }
    {
        ImageSpec spec(4, 4, 3, TypeFloat);
        spec.attribute("colorInteropID", "lin_adobergb_scene");
        reconcile_color_metadata(spec, ColorReadPolicy());
        OIIO_CHECK_EQUAL(spec.get_string_attribute("oiio:ColorSpace"),
                         "lin_adobergb_scene");
    }
    {
        ImageSpec spec(4, 4, 3, TypeFloat);
        reconcile_color_metadata(spec, ColorReadPolicy());
        OIIO_CHECK_EQUAL(spec.get_string_attribute("oiio:ColorSpace"), "");
    }
    {
        // A CICP tuple overrides the color space with the interop id it maps
        // to (Rec.709 primaries + sRGB transfer -> srgb_rec709_display; CICP
        // describes display encodings, so the display twin wins on read), and
        // the CICP source attribute is kept in place.
        ImageSpec spec(4, 4, 3, TypeFloat);
        const int cicp[4] = { 1, 13, 0, 1 };
        spec.attribute("CICP", TypeDesc(TypeDesc::INT, 4), cicp);
        reconcile_color_metadata(spec, ColorReadPolicy());
        OIIO_CHECK_EQUAL(spec.get_string_attribute("oiio:ColorSpace"),
                         "srgb_rec709_display");
        OIIO_CHECK_EQUAL(spec.find_attribute("CICP") != nullptr, true);
    }
}


// ColorReadCaps: the per-format consulted-signals table (the read-direction
// mirror of ColorWriteCaps) and the caps-narrowed spec->facts extraction.
// Zero-diff proof: every wired format's row is today's format-invariant
// consulted trio, so reconciliation under any format name -- or none -- is
// identical; the caps-narrowed extraction drops exactly the disabled
// signals.
static void
test_read_caps()
{
    // The table: wired readers and the unknown-format default share the
    // consulted trio; signals resolution does not consult stay off.
    for (string_view fmt : { "openexr", "png", "" }) {
        const ColorReadCaps caps = color_read_caps_for_format(fmt);
        OIIO_CHECK_ASSERT(caps.aces_container && caps.interop_id && caps.cicp);
        OIIO_CHECK_ASSERT(!caps.icc && !caps.chromaticities && !caps.gamma);
    }

    // Caps-narrowed extraction: enabled signals extract exactly as the
    // unrestricted overload does; disabled ones stay absent.
    ImageSpec spec(4, 4, 3, TypeFloat);
    spec.attribute("colorInteropID", "srgb_rec709_scene");
    const int cicp[4] = { 1, 13, 0, 1 };
    spec.attribute("CICP", TypeDesc(TypeDesc::INT, 4), cicp);
    const float chroma[8] = { 0.64f, 0.33f, 0.30f,   0.60f,
                              0.15f, 0.06f, 0.3127f, 0.329f };
    spec.attribute("chromaticities", TypeDesc(TypeDesc::FLOAT, 8), chroma);
    spec.attribute("oiio:Gamma", 2.2f);

    const ColorMetadataFacts full = color_facts_from_spec(spec);
    OIIO_CHECK_ASSERT(full.has_cicp && full.has_chromaticities
                      && full.has_gamma
                      && full.color_interop_id == "srgb_rec709_scene");
    const ColorMetadataFacts narrowed
        = color_facts_from_spec(spec, color_read_caps_for_format("png"));
    OIIO_CHECK_ASSERT(narrowed.has_cicp
                      && narrowed.color_interop_id == "srgb_rec709_scene");
    OIIO_CHECK_EQUAL(narrowed.has_chromaticities, false);
    OIIO_CHECK_EQUAL(narrowed.has_gamma, false);

    // Zero-diff: reconciliation stamps the same result under every wired
    // format name and with no format at all (the rows coincide by design).
    for (string_view fmt : { "openexr", "png", "" }) {
        ImageSpec a(4, 4, 3, TypeFloat);
        a.attribute("colorInteropID", "lin_adobergb_scene");
        reconcile_color_metadata(a, ColorReadPolicy(), fmt);
        OIIO_CHECK_EQUAL(a.get_string_attribute("oiio:ColorSpace"),
                         "lin_adobergb_scene");
        ImageSpec b(4, 4, 3, TypeFloat);
        b.attribute("CICP", TypeDesc(TypeDesc::INT, 4), cicp);
        reconcile_color_metadata(b, ColorReadPolicy(), fmt);
        OIIO_CHECK_EQUAL(b.get_string_attribute("oiio:ColorSpace"),
                         "srgb_rec709_display");
        OIIO_CHECK_EQUAL(b.find_attribute("CICP") != nullptr, true);
    }
}


// render_color_read_plan: the read-side dry-run preview. A spec carrying a
// CICP tuple resolves through the cascade; the rendered plan names the CICP
// rule, its matched outcome, the tuple, and the final assignment.
static void
test_render_read_plan(const ColorConfig& config)
{
    ImageSpec spec(16, 16, 3, TypeDesc::FLOAT);
    const int cicp[4] = { 1, 13, 0, 1 };
    spec.attribute("CICP", TypeDesc(TypeDesc::INT, 4), cicp);
    const std::string plan = render_color_read_plan(spec, &config);
    OIIO_CHECK_ASSERT(Strutil::contains(plan, "rule 5  CICP"));
    OIIO_CHECK_ASSERT(Strutil::contains(plan, "matched"));
    OIIO_CHECK_ASSERT(Strutil::contains(plan, "(tuple 1,13,0,1)"));
    OIIO_CHECK_ASSERT(Strutil::contains(
        plan, "Resolved color space: 'srgb_rec709_display' (via CICP)"));
}


// Deferred + consume-once CICP resolution (spec 09, "Deferred resolution and
// consume-once policy"). A CICP tuple is state-ambiguous, so a defer_cicp read
// deposits it as pending without committing a color space; the caller then
// sets cicp_state and resolves. Resolution consumes the pending tuple and the
// one-shot global cicp_state key.
static void
test_deferred_cicp(const ColorConfig& config)
{
    // Rec.709 primaries + sRGB transfer: maps to srgb_rec709_display, whose
    // scene twin (srgb_rec709_scene) also exists in the config.
    const int cicp[4] = { 1, 13, 0, 1 };

    // Deferred, then policy set AFTER read decides the twin: SCENE.
    {
        ImageSpec spec(4, 4, 3, TypeFloat);
        spec.attribute("CICP", TypeDesc(TypeDesc::INT, 4), cicp);
        ColorReadPolicy defer;
        defer.defer_cicp = true;
        reconcile_color_metadata(spec, defer, "png");
        // Pending: no color space committed, marker present, CICP kept.
        OIIO_CHECK_EQUAL(spec.get_string_attribute("oiio:ColorSpace"), "");
        OIIO_CHECK_EQUAL(spec.get_int_attribute("oiio:cicp:pending"), 1);
        OIIO_CHECK_EQUAL(spec.find_attribute("CICP") != nullptr, true);

        ColorReadPolicy scene;
        scene.cicp_state = ColorStatePreference::Scene;
        const bool did = resolve_pending_cicp(spec, scene, &config);
        OIIO_CHECK_ASSERT(did);
        OIIO_CHECK_EQUAL(spec.get_string_attribute("oiio:ColorSpace"),
                         "srgb_rec709_scene");
        // Consumed: the pending marker is gone.
        OIIO_CHECK_EQUAL(spec.find_attribute("oiio:cicp:pending"), nullptr);
        // Second resolve is a no-op (nothing pending).
        OIIO_CHECK_EQUAL(resolve_pending_cicp(spec, scene, &config), false);
    }

    // Same tuple, DISPLAY policy set after read -> display twin. Proves the
    // post-read policy, not the tuple, decides the result.
    {
        ImageSpec spec(4, 4, 3, TypeFloat);
        spec.attribute("CICP", TypeDesc(TypeDesc::INT, 4), cicp);
        ColorReadPolicy defer;
        defer.defer_cicp = true;
        reconcile_color_metadata(spec, defer, "png");
        ColorReadPolicy display;
        display.cicp_state = ColorStatePreference::Display;
        resolve_pending_cicp(spec, display, &config);
        OIIO_CHECK_EQUAL(spec.get_string_attribute("oiio:ColorSpace"),
                         "srgb_rec709_display");
    }

    // No-op: setting cicp_state when nothing is pending changes nothing.
    {
        ImageSpec spec(4, 4, 3, TypeFloat);
        spec.attribute("oiio:ColorSpace", "lin_ap1_scene");
        ColorReadPolicy scene;
        scene.cicp_state = ColorStatePreference::Scene;
        OIIO_CHECK_EQUAL(resolve_pending_cicp(spec, scene, &config), false);
        OIIO_CHECK_EQUAL(spec.get_string_attribute("oiio:ColorSpace"),
                         "lin_ap1_scene");
    }

    // Consume-once via the GLOBAL cicp_state key: after a pending resolve it is
    // cleared, so it does NOT silently re-apply to the next file.
    {
        OIIO::attribute("oiio:colorpolicy:read:defer_cicp", 1);
        OIIO::attribute("oiio:colorpolicy:read:cicp_state", "scene");

        // File 1: deferred read + resolve under the global scene policy.
        ImageSpec s1(4, 4, 3, TypeFloat);
        s1.attribute("CICP", TypeDesc(TypeDesc::INT, 4), cicp);
        reconcile_color_metadata(s1, ColorReadPolicy::snapshot(), "png");
        OIIO_CHECK_EQUAL(s1.get_int_attribute("oiio:cicp:pending"), 1);
        resolve_pending_cicp(s1, ColorReadPolicy::snapshot(), &config);
        OIIO_CHECK_EQUAL(s1.get_string_attribute("oiio:ColorSpace"),
                         "srgb_rec709_scene");

        // The global cicp_state was consumed by that resolve.
        std::string leftover;
        OIIO::getattribute("oiio:colorpolicy:read:cicp_state", leftover);
        OIIO_CHECK_EQUAL(leftover, "");

        // File 2: same deferred read, but the prior scene policy is gone, so
        // the default (display) twin wins -- consume-once proven.
        ImageSpec s2(4, 4, 3, TypeFloat);
        s2.attribute("CICP", TypeDesc(TypeDesc::INT, 4), cicp);
        reconcile_color_metadata(s2, ColorReadPolicy::snapshot(), "png");
        resolve_pending_cicp(s2, ColorReadPolicy::snapshot(), &config);
        OIIO_CHECK_EQUAL(s2.get_string_attribute("oiio:ColorSpace"),
                         "srgb_rec709_display");

        OIIO::attribute("oiio:colorpolicy:read:defer_cicp", 0);
    }

    // Eager path unchanged: without defer_cicp the default policy commits the
    // display twin at read time and leaves no pending marker.
    {
        ImageSpec spec(4, 4, 3, TypeFloat);
        spec.attribute("CICP", TypeDesc(TypeDesc::INT, 4), cicp);
        reconcile_color_metadata(spec, ColorReadPolicy(), "png");
        OIIO_CHECK_EQUAL(spec.get_string_attribute("oiio:ColorSpace"),
                         "srgb_rec709_display");
        OIIO_CHECK_EQUAL(spec.find_attribute("oiio:cicp:pending"), nullptr);
    }
}


int
main(int /*argc*/, char* /*argv*/[])
{
    test_aces_container();
    test_ciid_registry_bridge();
    test_reconcile_entry_point();
    test_read_caps();

    const std::string cfgpath = write_test_config();
    ColorConfig config(cfgpath);
    if (config.has_error()) {
        Strutil::print("Could not load test config: {}\n", config.geterror());
        return 1;
    }

    test_unknown_ciid_falls_through_to_cicp(config);
    test_utility_tokens(config);
    test_strict_terminal(config);
    test_garbage_icc_falls_through(config);
    test_icc_synthetic(config);
    test_cicp_over_icc(config);
    test_spec_impossible_cicp(config);
    test_filename_invariance(config);
    test_render_read_plan(config);
    test_deferred_cicp(config);

    Filesystem::remove(cfgpath);
    return unit_test_failures != 0;
}
