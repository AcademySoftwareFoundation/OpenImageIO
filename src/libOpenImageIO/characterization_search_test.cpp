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

#include <algorithm>
#include <atomic>
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

// ---------------------------------------------------------------------------
// Layer 2: config-driven search over small in-memory OCIO configs.
// ---------------------------------------------------------------------------

// A tiny scratch directory that cleans itself up, for OCIO config/LUT files.
struct ScratchDir {
    std::string path;
    ScratchDir()
    {
        static std::atomic<int> counter { 0 };
        path = Filesystem::temp_directory_path() + "/oiio_charsearch_"
               + std::to_string(uintptr_t(this)) + "_"
               + std::to_string(counter.fetch_add(1));
        Filesystem::create_directory(path);
    }
    ~ScratchDir() { Filesystem::remove_all(path); }
    std::string write(string_view name, string_view contents) const
    {
        std::string p = path + "/" + std::string(name);
        Filesystem::write_text_file(p, contents);
        return p;
    }
};


ColorConfig
config_from_text(const ScratchDir& dir, string_view name, string_view text)
{
    return ColorConfig(dir.write(name, text));
}


// The search-core fixture: an active simple space, an inactive simple space, a
// display simple space, a matrix space with no declared encoding (its encoding
// is *unknown*), and a data space (never a candidate).
constexpr const char* kSearchConfig = R"OCIO(ocio_profile_version: 2.1
roles:
  default: active_simple
  scene_linear: active_simple
file_rules:
  - !<Rule> {name: Default, colorspace: active_simple}
inactive_colorspaces: [inactive_simple]
colorspaces:
  - !<ColorSpace>
    name: active_simple
    encoding: scene-linear
  - !<ColorSpace>
    name: inactive_simple
    encoding: scene-linear
  - !<ColorSpace>
    name: unknown_encoding
    from_scene_reference: !<MatrixTransform> {matrix: [0.73, 0.02, 0.01, 0, 0.01, 0.91, 0.03, 0, 0.04, 0.02, 1.17, 0, 0, 0, 0, 1]}
  - !<ColorSpace>
    name: data_space
    isdata: true
display_colorspaces:
  - !<ColorSpace>
    name: display_simple
    encoding: display-linear
)OCIO";


void
test_universe_and_visibility()
{
    ScratchDir dir;
    ColorConfig config = config_from_text(dir, "search.ocio", kSearchConfig);

    pvt::FindColorSpacesOptions opt;
    // Default universe: active/display simple spaces + the matrix space; the
    // data space and inactive space are absent.
    OIIO_CHECK_ASSERT(
        pvt::find_color_spaces(config, opt)
        == std::vector<std::string>({ "active_simple", "display_simple",
                                      "unknown_encoding" }));

    // include_inactive appends the inactive simple space.
    opt.include_inactive = true;
    OIIO_CHECK_ASSERT(
        pvt::find_color_spaces(config, opt)
        == std::vector<std::string>({ "active_simple", "display_simple",
                                      "unknown_encoding", "inactive_simple" }));

    // Active off, inactive on: only the inactive space.
    opt.include_active = false;
    OIIO_CHECK_ASSERT(pvt::find_color_spaces(config, opt)
                      == std::vector<std::string>({ "inactive_simple" }));

    // Both off: empty (short-circuit).
    opt.include_inactive = false;
    OIIO_CHECK_ASSERT(pvt::find_color_spaces(config, opt).empty());

    // Determinism: an identical query returns a byte-identical ordered list.
    pvt::FindColorSpacesOptions again;
    OIIO_CHECK_ASSERT(pvt::find_color_spaces(config, again)
                      == pvt::find_color_spaces(config, again));
}


// An untagged, unencoded display space that IS g26_p3d65_display by value
// (same matrix + gamma as the registry definition): the fingerprint tier of
// pvt::derive_color_interop_id derives the identity, and the encoding axis
// adopts the twin's sdr-cinema outright.
constexpr const char* kUntaggedTheatricalConfig
    = R"OCIO(ocio_profile_version: 2.3
roles:
  aces_interchange: reference
  cie_xyz_d65_interchange: xyz
  default: reference
  scene_linear: reference
file_rules:
  - !<Rule> {name: Default, colorspace: reference}
display_colorspaces:
  - !<ColorSpace>
    name: xyz
    encoding: display-linear
  - !<ColorSpace>
    name: mystery_theatrical
    from_display_reference: !<GroupTransform>
      children:
        - !<MatrixTransform> {matrix: [2.49349691194143, -0.931383617919124, -0.402710784450717, 0, -0.829488969561575, 1.76266406031835, 0.0236246858419436, 0, 0.0358458302437845, -0.0761723892680418, 0.956884524007688, 0, 0, 0, 0, 1]}
        - !<ExponentTransform> {value: 2.6, style: mirror, direction: inverse}
colorspaces:
  - !<ColorSpace>
    name: reference
    encoding: scene-linear
)OCIO";


void
test_encoding_adopted_via_fingerprint()
{
    ScratchDir dir;
    ColorConfig config = config_from_text(dir, "untagged.ocio",
                                          kUntaggedTheatricalConfig);

    // No interop_id attribute, no authored encoding: the identity comes from
    // the fingerprint tier and the encoding is adopted from the twin.
    pvt::FindColorSpacesOptions opt;
    opt.encodings = { "sdr-cinema" };
    OIIO_CHECK_ASSERT(pvt::find_color_spaces(config, opt)
                      == std::vector<std::string>({ "mystery_theatrical" }));

    // strict: no authored encoding means the property stays unknown.
    opt.strict = true;
    OIIO_CHECK_ASSERT(pvt::find_color_spaces(config, opt).empty());
}


// A space whose authored encoding (sdr-video) disagrees with its
// interop-identity twin's (g26_p3d65_display -> sdr-cinema): the encoding
// axis accepts both values unless strict.
constexpr const char* kTwinEncodingConfig = R"OCIO(ocio_profile_version: 2.3
roles:
  default: reference
  scene_linear: reference
file_rules:
  - !<Rule> {name: Default, colorspace: reference}
colorspaces:
  - !<ColorSpace>
    name: reference
    encoding: scene-linear
  - !<ColorSpace>
    name: theatrical_output
    encoding: sdr-video
    interop_id: g26_p3d65_display
    from_scene_reference: !<ExponentTransform> {value: 2.6, style: mirror, direction: inverse}
  - !<ColorSpace>
    name: plain_video
    encoding: sdr-video
    from_scene_reference: !<ExponentTransform> {value: 2.2, style: mirror, direction: inverse}
)OCIO";


void
test_encoding_twin_inference_and_strict()
{
    ScratchDir dir;
    ColorConfig config = config_from_text(dir, "twin.ocio",
                                          kTwinEncodingConfig);

    // Inferred: authored sdr-video, but the g26_p3d65_display twin carries
    // sdr-cinema -- the candidate matches both values.
    pvt::FindColorSpacesOptions opt;
    opt.encodings = { "sdr-cinema" };
    OIIO_CHECK_ASSERT(pvt::find_color_spaces(config, opt)
                      == std::vector<std::string>({ "theatrical_output" }));

    opt.encodings = { "sdr-video" };
    OIIO_CHECK_ASSERT(pvt::find_color_spaces(config, opt)
                      == std::vector<std::string>(
                          { "plain_video", "theatrical_output" }));

    // Hint-by-example reads the named space's own effective encoding
    // (sdr-video), not its twin's.
    opt.encodings = { "theatrical_output" };
    OIIO_CHECK_ASSERT(pvt::find_color_spaces(config, opt)
                      == std::vector<std::string>(
                          { "plain_video", "theatrical_output" }));

    // Exclusion removes a proven (inferred) match; inverse requires a proven
    // difference on every characterized value.
    opt.encodings = { "-sdr-cinema" };
    OIIO_CHECK_ASSERT(pvt::find_color_spaces(config, opt)
                      == std::vector<std::string>(
                          { "plain_video", "reference" }));
    opt.encodings = { "~sdr-cinema" };
    OIIO_CHECK_ASSERT(pvt::find_color_spaces(config, opt)
                      == std::vector<std::string>(
                          { "plain_video", "reference" }));

    // strict: authored attributes only -- the twin's encoding never enters.
    opt.strict    = true;
    opt.encodings = { "sdr-cinema" };
    OIIO_CHECK_ASSERT(pvt::find_color_spaces(config, opt).empty());
    opt.encodings = { "sdr-video" };
    OIIO_CHECK_ASSERT(pvt::find_color_spaces(config, opt)
                      == std::vector<std::string>(
                          { "plain_video", "theatrical_output" }));
}


void
test_encoding_three_valued_split()
{
    ScratchDir dir;
    ColorConfig config = config_from_text(dir, "search.ocio", kSearchConfig);

    // include: only the proven scene-linear space.
    pvt::FindColorSpacesOptions inc;
    inc.encodings = { "scene-linear" };
    OIIO_CHECK_ASSERT(pvt::find_color_spaces(config, inc)
                      == std::vector<std::string>({ "active_simple" }));

    // ~ inverse: only the space *proven different* (display_simple). The
    // unknown-encoding matrix space is REJECTED (its encoding is unknown).
    pvt::FindColorSpacesOptions inv;
    inv.encodings = { "~scene-linear" };
    OIIO_CHECK_ASSERT(pvt::find_color_spaces(config, inv)
                      == std::vector<std::string>({ "display_simple" }));

    // - exclude: everything not *proven* scene-linear -- the unknown-encoding
    //   space is PRESERVED here (the crux of the `~` vs `-` split).
    pvt::FindColorSpacesOptions exc;
    exc.encodings = { "-scene-linear" };
    OIIO_CHECK_ASSERT(
        pvt::find_color_spaces(config, exc)
        == std::vector<std::string>({ "display_simple", "unknown_encoding" }));

    // image-state axis: only the display space.
    pvt::FindColorSpacesOptions disp;
    disp.image_states = { "display" };
    OIIO_CHECK_ASSERT(pvt::find_color_spaces(config, disp)
                      == std::vector<std::string>({ "display_simple" }));

    // A bogus image-state hint fails fast.
    pvt::FindColorSpacesOptions bogus;
    bogus.image_states = { "bogus" };
    OIIO_CHECK_ASSERT(throws_invalid_argument(
        [&] { pvt::find_color_spaces(config, bogus); }));

    // A bogus encoding hint fails fast.
    pvt::FindColorSpacesOptions bad_enc;
    bad_enc.encodings = { "no_such_encoding" };
    OIIO_CHECK_ASSERT(throws_invalid_argument(
        [&] { pvt::find_color_spaces(config, bad_enc); }));

    // A chromaticity hint that is neither a local space, a known id, nor a
    // complete gamut component fails fast (the incomplete "p3" fragment case).
    pvt::FindColorSpacesOptions bad_chroma;
    bad_chroma.chromaticities = { "p3" };
    OIIO_CHECK_ASSERT(throws_invalid_argument(
        [&] { pvt::find_color_spaces(config, bad_chroma); }));
}


// Escapes + multi-term sequences: names that literally start with an operator.
constexpr const char* kEscapedNamesConfig = R"OCIO(ocio_profile_version: 2.1
roles:
  default: foo
  scene_linear: foo
file_rules:
  - !<Rule> {name: Default, colorspace: foo}
colorspaces:
  - !<ColorSpace>
    name: foo
    encoding: scene-linear
  - !<ColorSpace>
    name: "-foo"
    encoding: display-linear
  - !<ColorSpace>
    name: "~foo"
    encoding: log
)OCIO";


void
test_escapes_and_sequences()
{
    ScratchDir dir;
    ColorConfig config = config_from_text(dir, "escaped.ocio",
                                          kEscapedNamesConfig);

    // An escaped operator is part of the encoding name literal. Here every
    // space is simple, so the encoding hint selects by its declared encoding;
    // "\-foo" resolves the literal encoding of the space named "-foo"
    // (display-linear), selecting that space. The fixture spaces are identity
    // transforms whose authored encodings contradict what fingerprinting
    // infers (every one twins lin_ap0_scene); strict isolates the term
    // grammar under test from twin-encoding inference.
    pvt::FindColorSpacesOptions esc;
    esc.strict    = true;
    esc.encodings = { "\\-foo" };
    OIIO_CHECK_ASSERT(pvt::find_color_spaces(config, esc)
                      == std::vector<std::string>({ "-foo" }));

    // A sequence: include two encodings, then exclude one. foo (scene-linear)
    // survives; -foo (display-linear) is excluded.
    pvt::FindColorSpacesOptions seq;
    seq.strict    = true;
    seq.encodings = { "scene-linear", "display-linear", "-display-linear" };
    OIIO_CHECK_ASSERT(pvt::find_color_spaces(config, seq)
                      == std::vector<std::string>({ "foo" }));

    // An invalid escape fails fast.
    pvt::FindColorSpacesOptions bad;
    bad.encodings = { "\\foo" };
    OIIO_CHECK_ASSERT(
        throws_invalid_argument([&] { pvt::find_color_spaces(config, bad); }));
}


// A non-simple (CDL) space is excluded from the default universe, but may still
// be *named* as a hint source. A custom curve NamedTransform matches
// behaviorally by signature.
constexpr const char* kComplexConfig = R"OCIO(ocio_profile_version: 2.1
roles:
  default: reference
  aces_interchange: reference
  scene_linear: reference
file_rules:
  - !<Rule> {name: Default, colorspace: reference}
colorspaces:
  - !<ColorSpace>
    name: reference
    encoding: scene-linear
  - !<ColorSpace>
    name: complex_space
    encoding: scene-linear
    from_scene_reference: !<CDLTransform> {slope: [1, 1, 1], offset: [0, 0, 0], power: [1, 1, 1], saturation: 1}
)OCIO";


void
test_non_simple_and_hint_source()
{
    ScratchDir dir;
    ColorConfig config = config_from_text(dir, "complex.ocio", kComplexConfig);

    // The CDL space is not in the default universe.
    const auto names = pvt::find_color_spaces(config, {});
    OIIO_CHECK_ASSERT(std::find(names.begin(), names.end(), "complex_space")
                      == names.end());

    // ...but it can be a hint source: its (scene-linear) encoding selects the
    // reference space. A hint source is not itself a result.
    pvt::FindColorSpacesOptions hint;
    hint.encodings = { "complex_space" };
    OIIO_CHECK_ASSERT(pvt::find_color_spaces(config, hint)
                      == std::vector<std::string>({ "reference" }));
}


// The exhaustive realize-clean + allowlist gate: a
// non-simple, file-backed color space whose realized ops all pass the atomic
// allowlist is admitted under exhaustive=true -- decided by realizing the
// processor and inspecting its ops, NOT by the fingerprint subsystem.
void
test_exhaustive_realize_clean_gate()
{
    ScratchDir dir;
    // A trivial identity matrix CLF. Unlike .spi1d/.spimtx (which the default
    // classification already accepts as simple), a .clf file transform is not
    // simple by default -- so it exercises the exhaustive revisit path -- yet
    // it is exhaustive-eligible and realizes to an allowlisted matrix op.
    dir.write("identity.clf", R"(<?xml version="1.0" encoding="UTF-8"?>
<ProcessList id="id" compCLFversion="3.0">
  <Matrix inBitDepth="32f" outBitDepth="32f">
    <Array dim="3 3 3">
1 0 0
0 1 0
0 0 1
    </Array>
  </Matrix>
</ProcessList>
)");
    constexpr const char* cfg = R"OCIO(ocio_profile_version: 2.1
roles:
  default: reference
  aces_interchange: reference
  scene_linear: reference
file_rules:
  - !<Rule> {name: Default, colorspace: reference}
colorspaces:
  - !<ColorSpace>
    name: reference
    encoding: scene-linear
  - !<ColorSpace>
    name: clf_backed
    from_scene_reference: !<FileTransform> {src: identity.clf}
  - !<ColorSpace>
    name: cdl_backed
    from_scene_reference: !<CDLTransform> {slope: [1.1, 1, 1], offset: [0, 0, 0], power: [1, 1, 1], saturation: 1}
)OCIO";
    ColorConfig config = config_from_text(dir, "exhaustive.ocio", cfg);

    // Default: the .clf-backed space is not simple, and the CDL space is not
    // simple, so both are absent.
    const auto plain = pvt::find_color_spaces(config, {});
    OIIO_CHECK_ASSERT(std::find(plain.begin(), plain.end(), "clf_backed")
                      == plain.end());
    OIIO_CHECK_ASSERT(std::find(plain.begin(), plain.end(), "cdl_backed")
                      == plain.end());

    // Exhaustive: the .clf-backed space realizes to an allowlisted matrix op
    // and is admitted; the CDL space is rejected by the allowlist (no
    // fingerprint consulted). This is the realize-clean + allowlist gate.
    pvt::FindColorSpacesOptions ex;
    ex.exhaustive = true;
    const auto exhaustive = pvt::find_color_spaces(config, ex);
    OIIO_CHECK_ASSERT(std::find(exhaustive.begin(), exhaustive.end(),
                                "clf_backed")
                      != exhaustive.end());
    OIIO_CHECK_ASSERT(std::find(exhaustive.begin(), exhaustive.end(),
                                "cdl_backed")
                      == exhaustive.end());
}


// Transfer axis: identity `~` (proven non-linear) and a custom-curve
// NamedTransform matched behaviorally by signature.
constexpr const char* kCustomCurveConfig = R"OCIO(ocio_profile_version: 2.1
roles:
  default: reference
  aces_interchange: reference
  scene_linear: reference
file_rules:
  - !<Rule> {name: Default, colorspace: reference}
named_transforms:
  - !<NamedTransform>
    name: Odd 2.35 - Curve
    aliases: [odd235_crv]
    encoding: sdr-video
    inverse_transform: !<ExponentTransform> {value: 2.35, style: pass_thru, direction: inverse}
colorspaces:
  - !<ColorSpace>
    name: reference
    encoding: scene-linear
  - !<ColorSpace>
    name: odd_encoded
    encoding: sdr-video
    from_scene_reference: !<ExponentTransform> {value: 2.35, style: pass_thru, direction: inverse}
)OCIO";


void
test_transfer_axis()
{
    ScratchDir dir;
    ColorConfig config = config_from_text(dir, "curve.ocio",
                                          kCustomCurveConfig);

    // A custom-curve NamedTransform with no registry identity matches the
    // behaviorally equivalent color space by probed signature.
    pvt::FindColorSpacesOptions curve;
    curve.transfer_functions = { "Odd 2.35" };
    OIIO_CHECK_ASSERT(pvt::find_color_spaces(config, curve)
                      == std::vector<std::string>({ "odd_encoded" }));

    // ~ inverse against the linear reference: only the *proven non-linear*
    // space is returned (the reference itself, being linear/identity, drops).
    pvt::FindColorSpacesOptions inv;
    inv.transfer_functions = { "~reference" };
    OIIO_CHECK_ASSERT(pvt::find_color_spaces(config, inv)
                      == std::vector<std::string>({ "odd_encoded" }));
}


// A config with one context-sensitive space: ctx_space resolves $CTX at
// probe time, behaving as gamma_a (2.35 curve) or gamma_b (1.4 curve)
// depending on the context. The explicit per-call context override must be
// honored by every characterization probe.
constexpr const char* kContextConfig = R"OCIO(ocio_profile_version: 2.1
environment:
  CTX: gamma_a
roles:
  default: ref
  aces_interchange: ref
  scene_linear: ref
file_rules:
  - !<Rule> {name: Default, colorspace: ref}
colorspaces:
  - !<ColorSpace>
    name: ref
    encoding: scene-linear
  - !<ColorSpace>
    name: gamma_a
    encoding: sdr-video
    from_scene_reference: !<ExponentTransform> {value: 2.35, style: pass_thru, direction: inverse}
  - !<ColorSpace>
    name: gamma_b
    encoding: sdr-video
    from_scene_reference: !<ExponentTransform> {value: 1.4, style: pass_thru, direction: inverse}
  - !<ColorSpace>
    name: ctx_space
    encoding: sdr-video
    to_scene_reference: !<ColorSpaceTransform> {src: $CTX, dst: ref}
)OCIO";


void
test_context_override()
{
    ScratchDir dir;
    ColorConfig config = config_from_text(dir, "ctx.ocio", kContextConfig);

    auto contains = [](const std::vector<std::string>& v, const char* name) {
        return std::find(v.begin(), v.end(), name) != v.end();
    };

    pvt::FindColorSpacesOptions opt;
    opt.include_context_sensitive = true;
    opt.transfer_functions        = { "gamma_a" };

    // Under CTX=gamma_a (also the config's ambient default), ctx_space
    // behaves as gamma_a and matches the hint.
    opt.context = { { "CTX", "gamma_a" } };
    const auto with_a = pvt::find_color_spaces(config, opt);
    OIIO_CHECK_ASSERT(contains(with_a, "gamma_a"));
    OIIO_CHECK_ASSERT(contains(with_a, "ctx_space"));

    // Under CTX=gamma_b the SAME space behaves as gamma_b: the probes must
    // run under the explicit override, so ctx_space drops out of a gamma_a
    // transfer query...
    opt.context = { { "CTX", "gamma_b" } };
    const auto with_b = pvt::find_color_spaces(config, opt);
    OIIO_CHECK_ASSERT(contains(with_b, "gamma_a"));
    OIIO_CHECK_FALSE(contains(with_b, "ctx_space"));

    // ...and reappears in a gamma_b transfer query under the same override.
    pvt::FindColorSpacesOptions opt_b;
    opt_b.include_context_sensitive = true;
    opt_b.transfer_functions        = { "gamma_b" };
    opt_b.context                   = { { "CTX", "gamma_b" } };
    const auto with_b2 = pvt::find_color_spaces(config, opt_b);
    OIIO_CHECK_ASSERT(contains(with_b2, "gamma_b"));
    OIIO_CHECK_ASSERT(contains(with_b2, "ctx_space"));

    // Two explicit contexts, same query, different result sets.
    OIIO_CHECK_ASSERT(with_a != with_b);
}


// The public ColorConfig::find_color_spaces thin adapter: it must map
// ColorSpaceSearchOptions onto the internal option set and forward to the
// pvt core, always searching active spaces (there is no include_active
// toggle on the public shape), and it must convert the core's fail-fast
// throw into the has_error()/geterror() convention (the public method never
// throws).
void
test_public_adapter()
{
    ScratchDir dir;
    ColorConfig config = config_from_text(dir, "search.ocio", kSearchConfig);

    // A hint through the public method returns the same result as the core.
    std::vector<std::string> enc = { "-scene-linear" };
    pvt::FindColorSpacesOptions core;
    core.encodings = enc;
    OIIO_CHECK_ASSERT(config.find_color_spaces({}, {}, enc, {})
                      == pvt::find_color_spaces(config, core));

    // Default (all axes empty) exposes the active/simple universe.
    OIIO_CHECK_ASSERT(
        config.find_color_spaces()
        == std::vector<std::string>({ "active_simple", "display_simple",
                                      "unknown_encoding" }));

    // include_inactive flows through the adapter.
    ColorSpaceSearchOptions inactive_opts;
    inactive_opts.include_inactive = true;
    OIIO_CHECK_ASSERT(
        config.find_color_spaces({}, {}, {}, {}, inactive_opts)
        == std::vector<std::string>({ "active_simple", "display_simple",
                                      "unknown_encoding", "inactive_simple" }));

    // The core's fail-fast throw on an unresolvable hint is converted to
    // the error convention: empty result, has_error() set, never a throw.
    std::vector<std::string> bad = { "bogus" };
    std::vector<std::string> r;
    OIIO_CHECK_ASSERT(
        !throws_invalid_argument([&] {
            r = config.find_color_spaces({}, {}, {}, bad);
        }));
    OIIO_CHECK_ASSERT(r.empty());
    OIIO_CHECK_ASSERT(config.has_error());
    OIIO_CHECK_ASSERT(!config.geterror().empty());
    OIIO_CHECK_ASSERT(!config.has_error());  // geterror() cleared it
}

}  // namespace


int
main(int /*argc*/, char* /*argv*/[])
{
    test_parse_search_term();
    test_three_valued_axis();
    test_universe_and_visibility();
    test_encoding_adopted_via_fingerprint();
    test_encoding_twin_inference_and_strict();
    test_encoding_three_valued_split();
    test_escapes_and_sequences();
    test_non_simple_and_hint_source();
    test_exhaustive_realize_clean_gate();
    test_transfer_axis();
    test_context_override();
    test_public_adapter();
    return unit_test_failures;
}
