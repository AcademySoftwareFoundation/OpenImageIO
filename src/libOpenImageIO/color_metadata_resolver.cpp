// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// One audited read-side color-metadata precedence cascade, replacing the
// per-plugin "figure out the color space from whatever attributes we found"
// code that every reader used to hand-roll (and that disagreed between
// formats). A reader now only deposits raw attributes; this file decides,
// in one tested order, which of them names the color space.
//
// The precedence order, the utility-token semantics, the miss/default
// ladder and the diagnostic trace are a straight port of a proven
// prototype. The engine is pure -- a ColorConfig plus plain value types --
// so the same code path serves resolve() and its explain() trace (explain
// is just resolve with the reason strings kept), and a unit test can drive
// it directly through color_pvt.h.

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <OpenImageIO/color.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/ustring.h>

#include "imageio_pvt.h"
#include "color_pvt.h"

OIIO_NAMESPACE_BEGIN

namespace pvt {

namespace {

std::string
lower_copy(string_view s)
{
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    return out;
}

// The scene/display twin of an interop id: swap a trailing _scene<->_display.
// Empty if the id carries neither suffix.
std::string
state_twin(const std::string& id)
{
    if (Strutil::ends_with(id, "_scene"))
        return id.substr(0, id.size() - 6) + "_display";
    if (Strutil::ends_with(id, "_display"))
        return id.substr(0, id.size() - 8) + "_scene";
    return {};
}

bool
ends_with_scene(const std::string& id)
{
    return Strutil::ends_with(id, "_scene");
}

// The canonical local name a config resolves `cand` to, or "" if the config
// has no color space reachable by that name/alias/role. A null config never
// resolves anything locally.
std::string
local_name(const ColorConfig* config, const std::string& cand)
{
    if (!config)
        return {};
    int idx = config->getColorSpaceIndex(cand);
    if (idx < 0)
        return {};
    const char* name = config->getColorSpaceNameByIndex(idx);
    return name ? std::string(name) : std::string {};
}

// Is `id` a known color interop identity in OIIO's built-in registry?
bool
known_registry_id(const std::string& id)
{
    return !id.empty() && interop_identities_config_resolves(id);
}

// Order a single candidate for the current state preference: when a
// preference is set (and not exact-state), the preferred-state twin is
// tried before the candidate itself.
std::vector<std::string>
state_preference_order(const std::string& candidate, const ColorReadPolicy& p)
{
    if (p.state_pref == ColorStatePreference::Auto
        || p.scope == ColorResolutionScope::ExactState)
        return { candidate };
    const bool want_scene = p.state_pref == ColorStatePreference::Scene;
    const std::string twin = state_twin(candidate);
    std::vector<std::string> ordered;
    if (!twin.empty() && want_scene != ends_with_scene(candidate))
        ordered.push_back(twin);
    ordered.push_back(candidate);
    return ordered;
}

// The shared color-interop-id assignment channel (one call, three entry
// points: the asset-facts id, the explicit assignment, and the failover).
// Utility tokens are excluded here; the caller handles them. Returns the
// resolved local (or bridged registry) name, or "".
std::string
resolve_ciid(const ColorConfig* config, const std::string& value,
             const ColorReadPolicy& p, std::string* selected = nullptr)
{
    if (is_utility_interop_id(value) || value.empty())
        return {};
    const auto candidates = state_preference_order(value, p);
    for (const auto& cand : candidates) {
        if (is_utility_interop_id(cand))
            continue;
        const std::string local = local_name(config, cand);
        if (!local.empty()) {
            if (selected)
                *selected = cand;
            return local;
        }
    }
    if (p.scope != ColorResolutionScope::ConfigOnly) {
        for (const auto& cand : candidates) {
            if (!is_utility_interop_id(cand) && known_registry_id(cand)) {
                if (selected)
                    *selected = cand;
                return cand;
            }
        }
    }
    return {};
}

// Local-name-then-CIID resolution of an explicit / failover assignment.
std::string
resolve_explicit(const ColorConfig* config, const std::string& value,
                 const ColorReadPolicy& p)
{
    const std::string local = local_name(config, value);
    if (!local.empty())
        return local;
    return resolve_ciid(config, value, p);
}

// data / bypass: the best local isData space by rank, else the literal
// token. Rank 0 = exact token match, 1 = any isData, 2 = the other token,
// 3 = an "unknown"-labeled data space.
std::string
resolve_data_space(const ColorConfig* config, const std::string& token)
{
    if (!config)
        return token;
    const std::string other = token == "bypass" ? "data" : "bypass";
    auto role_target = [&](const char* role) -> std::string {
        const char* n = config->getColorSpaceNameByRole(role);
        return n && n[0] ? std::string(n) : std::string {};
    };
    const std::string token_role   = role_target(token.c_str());
    const std::string other_role   = role_target(other.c_str());
    const std::string unknown_role = role_target("unknown");
    auto identifies_as = [&](const std::string& name, const std::string& label,
                             const std::string& role_name) {
        if (lower_copy(name) == label)
            return true;
        if (lower_copy(std::string(config->get_color_interop_id(name))) == label)
            return true;
        for (const std::string& alias : config->getAliases(name))
            if (lower_copy(alias) == label)
                return true;
        return !role_name.empty() && role_name == name;
    };

    std::string best;
    int best_rank                    = 99;
    const std::vector<std::string> names = config->getColorSpaceNames();
    for (const std::string& name : names) {
        if (!config->isData(name))
            continue;
        // OCIO's CreateRaw injects a framework-owned "raw" space; it is not
        // an authored utility target in an otherwise empty config.
        if (names.size() == 1 && lower_copy(name) == "raw" && token_role.empty())
            continue;
        int rank = 1;
        if (identifies_as(name, token, token_role))
            rank = 0;
        else if (identifies_as(name, "unknown", unknown_role))
            rank = 3;
        else if (identifies_as(name, other, other_role))
            rank = 2;
        if (rank < best_rank) {
            best_rank = rank;
            best      = name;
        }
        if (best_rank == 0)
            break;
    }
    return best;
}

// A synthetic session id for usable-but-unmatched colorimetry, per the id
// grammar (no live endpoint is constructed in this layer). scene-referred
// for EXR sources, display-referred otherwise -- carried in the state
// choice the grammar encodes rather than a separate field.
std::string
synthesize_custom_space(const float* chroma, bool has_gamma, float gamma)
{
    std::string id = "custom:";
    if (has_gamma)
        id += Strutil::fmt::format("g{:.5f}_", gamma);
    id += Strutil::fmt::format("{:.5f}_{:.5f}_{:.5f}_{:.5f}_{:.5f}_{:.5f}_{:.5f}_{:.5f}",
                               chroma[0], chroma[1], chroma[2], chroma[3],
                               chroma[4], chroma[5], chroma[6], chroma[7]);
    return id;
}

// A deterministic, idempotent synthetic id for a decodable-but-unmatched
// ICC profile. (The prototype keys this on the profile's MD5; OIIO has no
// MD5 in-tree, so a stable content hash stands in -- the id grammar and its
// idempotence are what downstream depends on, not the digest algorithm.)
std::string
synthesize_icc_space(const std::vector<unsigned char>& profile)
{
    size_t h = Strutil::strhash(string_view(
        reinterpret_cast<const char*>(profile.data()), profile.size()));
    return Strutil::fmt::format("icc:{:016x}", uint64_t(h));
}

// ---- individual rules -------------------------------------------------

struct RuleResult {
    ColorRuleOutcome outcome = ColorRuleOutcome::Inapplicable;
    std::string candidate;
    std::string resolved;
    std::string reason;
    std::string registered_synthetic;
};

RuleResult
rule_aces(const ColorMetadataFacts& f)
{
    if (!f.aces_image_container)
        return {};
    return { ColorRuleOutcome::Matched, "lin_ap0_scene", "lin_ap0_scene", {}, {} };
}

RuleResult
rule_file_rules(const ColorConfig* config, const ColorCallContext& ctx,
                ColorFileRules position, const ColorReadPolicy& p, bool diag)
{
    if (p.file_rules != position || ctx.filename.empty() || !config)
        return {};
    string_view name = config->getColorSpaceFromFilepath(ctx.filename, "");
    if (!name.empty())
        return { ColorRuleOutcome::Matched, diag ? ctx.filename : std::string {},
                 std::string(name), {}, {} };
    return { ColorRuleOutcome::Missed, diag ? ctx.filename : std::string {}, {},
             diag ? "No FileRules entry matched the filename" : std::string {}, {} };
}

RuleResult
rule_color_interop_id(const ColorConfig* config, const ColorMetadataFacts& f,
                      const ColorReadPolicy& p, bool diag)
{
    if (f.color_interop_id.empty())
        return {};
    const std::string& id = f.color_interop_id;
    if (id == "data" || id == "bypass") {
        const std::string local = resolve_data_space(config, id);
        return { ColorRuleOutcome::Matched, diag ? id : std::string {},
                 local.empty() ? id : local, {}, {} };
    }
    const std::string resolved = resolve_explicit(config, id, p);
    if (!resolved.empty())
        return { ColorRuleOutcome::Matched, diag ? id : std::string {}, resolved, {}, {} };
    return { ColorRuleOutcome::Missed, diag ? id : std::string {}, {},
             diag ? "Color interop id did not resolve" : std::string {}, {} };
}

// ICC (spec black box): a profile is decodable if it carries the 'acsp'
// signature at offset 36 of a >=128-byte header. A decodable-but-unmatched
// profile registers and returns an icc: synthetic under lenient scope;
// undecodable is invalid. (Matching a profile to a named local space is the
// heavy identification port and is not wired here yet.)
RuleResult
rule_icc(const ColorMetadataFacts& f, const ColorReadPolicy& p, bool diag)
{
    if (f.icc_profile.empty())
        return {};
    const bool decodable = f.icc_profile.size() >= 128
                           && f.icc_profile[36] == 'a' && f.icc_profile[37] == 'c'
                           && f.icc_profile[38] == 's' && f.icc_profile[39] == 'p';
    if (!decodable)
        return { ColorRuleOutcome::Invalid, diag ? "icc" : std::string {}, {},
                 diag ? "ICC profile is not a decodable profile" : std::string {}, {} };
    if (p.scope != ColorResolutionScope::Lenient)
        return { ColorRuleOutcome::Missed, diag ? "icc" : std::string {}, {},
                 diag ? "ICC profile decoded but the resolution scope rejected it"
                      : std::string {}, {} };
    const std::string id = synthesize_icc_space(f.icc_profile);
    return { ColorRuleOutcome::Matched, diag ? "icc" : std::string {}, id, {}, id };
}

RuleResult
rule_cicp(const ColorConfig* config, const ColorMetadataFacts& f,
          const ColorReadPolicy& p, bool diag)
{
    if (!f.has_cicp)
        return {};
    // CICP -> interop-id mapping routes through the same central resolve the
    // config exposes (primaries + transfer only; matrix/range are unused).
    // The mapping is a built-in table lookup; a config is only constructed
    // when the caller supplied none (rare -- readers pass their config).
    std::unique_ptr<ColorConfig> registry;
    if (!config)
        registry.reset(new ColorConfig);
    const ColorConfig* map_cfg = config ? config : registry.get();
    string_view raw = map_cfg->get_color_interop_id(f.cicp);
    if (raw.empty())
        return { ColorRuleOutcome::Missed,
                 diag ? Strutil::fmt::format("{}/{}", f.cicp[0], f.cicp[1])
                      : std::string {},
                 {}, diag ? "CICP primaries and transfer did not map to a known identity"
                          : std::string {}, {} };
    std::string candidate(raw);
    for (const auto& c : state_preference_order(candidate, p)) {
        std::string selected;
        const std::string resolved = resolve_ciid(config, c, p, &selected);
        if (!resolved.empty())
            return { ColorRuleOutcome::Matched,
                     diag ? (selected.empty() ? c : selected) : std::string {},
                     resolved, {}, {} };
    }
    // Nothing local; under non-config-only scope the candidate id itself is
    // the (bridged) answer.
    if (p.scope != ColorResolutionScope::ConfigOnly)
        return { ColorRuleOutcome::Matched, diag ? candidate : std::string {},
                 candidate, {}, {} };
    return { ColorRuleOutcome::Missed, diag ? candidate : std::string {}, {},
             diag ? "CICP identity is unavailable in the config under config-only scope"
                  : std::string {}, {} };
}

RuleResult
rule_png_srgb(const ColorConfig* config, const ColorMetadataFacts& f,
              const ColorReadPolicy& p, bool diag)
{
    if (!f.png_srgb)
        return {};
    const std::string id = "srgb_rec709_display";
    std::string selected;
    const std::string resolved = resolve_ciid(config, id, p, &selected);
    if (!resolved.empty())
        return { ColorRuleOutcome::Matched,
                 diag ? (selected.empty() ? id : selected) : std::string {},
                 resolved, {}, {} };
    if (p.scope != ColorResolutionScope::ConfigOnly)
        return { ColorRuleOutcome::Matched, diag ? id : std::string {}, id, {}, {} };
    return { ColorRuleOutcome::Missed, diag ? id : std::string {}, {},
             diag ? "PNG sRGB identity is unavailable under config-only scope"
                  : std::string {}, {} };
}

// Colorimetry (chromaticities / gamma): the config-matching tiers
// (encoded-gamut-first, round-then-exact chromaticity equality, the
// transfer-function slope catalog) are the heavy port and are not wired
// here yet. What ships is the shape: the nonlinear-gamma split, and the
// lenient custom: synthesis with the state choice the id grammar carries.
RuleResult
rule_colorimetry(const ColorMetadataFacts& f, const ColorCallContext& ctx,
                 const ColorReadPolicy& p, bool with_gamma, bool diag)
{
    if (!f.has_chromaticities)
        return {};
    const bool has_nonlinear = f.has_gamma && f.gamma > 1.001f;
    if (with_gamma != has_nonlinear)
        return {};
    if (p.scope == ColorResolutionScope::Lenient) {
        const std::string id
            = synthesize_custom_space(f.chromaticities, has_nonlinear, f.gamma);
        (void)ctx;
        return { ColorRuleOutcome::Matched, diag ? id : std::string {}, id, {}, id };
    }
    return { ColorRuleOutcome::Missed, diag ? "chromaticities" : std::string {}, {},
             diag ? "No color space matched the supplied chromaticities"
                  : std::string {}, {} };
}

RuleResult
rule_gamma(const ColorMetadataFacts& f, const ColorCallContext& ctx,
           const ColorReadPolicy& p, bool diag)
{
    if (!f.has_gamma || f.has_chromaticities)
        return {};
    const bool has_nonlinear = f.gamma > 1.001f;
    if (p.scope == ColorResolutionScope::Lenient) {
        static const float kRec709[8]
            = { 0.64f, 0.33f, 0.30f, 0.60f, 0.15f, 0.06f, 0.3127f, 0.3290f };
        const std::string id = synthesize_custom_space(kRec709, has_nonlinear, f.gamma);
        (void)ctx;
        return { ColorRuleOutcome::Matched, diag ? id : std::string {}, id, {}, id };
    }
    return { ColorRuleOutcome::Missed,
             diag ? Strutil::fmt::format("{}", f.gamma) : std::string {}, {},
             diag ? "No color space matched gamma under the active scope"
                  : std::string {}, {} };
}

// The config's Default Assignment: FileRules final entry, else the default
// role. Empty if neither resolves.
std::string
default_assignment(const ColorConfig* config, std::string* reason)
{
    if (!config)
        return {};
    // The single-arg form returns the config's default-rule color space when
    // nothing else matches -- i.e. the FileRules Default Assignment.
    string_view fr
        = config->getColorSpaceFromFilepath("oiio_color_metadata_default_probe");
    if (!fr.empty()) {
        if (reason)
            *reason = "Resolved from the FileRules Default Assignment";
        return std::string(fr);
    }
    const char* role = config->getColorSpaceNameByRole("default");
    if (role && role[0]) {
        if (reason)
            *reason = "FileRules Default Assignment was invalid; resolved from the "
                      "default role";
        return std::string(role);
    }
    return {};
}

std::string
finish_miss(const ColorConfig* config, const ColorCallContext& ctx,
            const ColorReadPolicy& p, ColorResolutionExplanation* expl)
{
    // Failover: a caller-supplied assignment tried after metadata misses.
    if (!ctx.failover.empty()) {
        const std::string resolved = resolve_explicit(config, ctx.failover, p);
        if (!resolved.empty()) {
            if (expl) {
                expl->steps.push_back({ ColorRule::Failover, ColorRuleOutcome::Matched,
                                        ctx.failover, resolved, {} });
                expl->used_failover = true;
            }
            return resolved;
        }
        if (expl)
            expl->steps.push_back(
                { ColorRule::Failover, ColorRuleOutcome::Invalid, ctx.failover, {},
                  "Failover config-local and CIID resolution attempts both missed" });
    }

    // Non-lenient scope preserves an unresolved assignment as the literal
    // "unknown" rather than substituting a default. `scope` never overrides
    // strict parsing; both surface here as the terminal step.
    if (p.scope != ColorResolutionScope::Lenient) {
        if (expl)
            expl->steps.push_back(
                { ColorRule::StrictParsing, ColorRuleOutcome::Matched,
                  "resolution-scope", "unknown",
                  "Resolution scope preserves an unresolved assignment" });
        return "unknown";
    }

    // Lenient all-miss. Main assigns nothing here (a reader that determined
    // nothing leaves the color space untouched), so the config default is
    // gated off unless a policy opts in.
    if (p.apply_config_default) {
        std::string reason;
        const std::string resolved = default_assignment(config, &reason);
        if (!resolved.empty()) {
            if (expl) {
                expl->steps.push_back({ ColorRule::ConfigDefault,
                                        ColorRuleOutcome::Matched, {}, resolved, reason });
                expl->used_default = true;
            }
            return resolved;
        }
    }
    return {};
}

}  // namespace

bool
ColorResolutionExplanation::has_genuine_metadata_match() const
{
    if (resolved.empty())
        return false;
    for (auto it = steps.rbegin(); it != steps.rend(); ++it) {
        if (it->outcome != ColorRuleOutcome::Matched)
            continue;
        return it->rule != ColorRule::Failover && it->rule != ColorRule::ConfigDefault
               && it->rule != ColorRule::StrictParsing;
    }
    return false;
}

ColorResolutionExplanation
resolve_color_metadata(const ColorConfig* config,
                       const std::string& explicit_assignment,
                       const ColorMetadataFacts& facts, const ColorCallContext& ctx,
                       const ColorReadPolicy& policy)
{
    ColorResolutionExplanation expl;
    const bool diag = true;  // the engine always keeps its trace

    // Rule 1: an explicit assignment suppresses every metadata rule.
    if (!explicit_assignment.empty()) {
        const std::string resolved = resolve_explicit(config, explicit_assignment, policy);
        expl.steps.push_back({ ColorRule::ExplicitAssignment,
                               resolved.empty() ? ColorRuleOutcome::Missed
                                                : ColorRuleOutcome::Matched,
                               explicit_assignment, resolved, {} });
        if (!resolved.empty()) {
            expl.resolved = resolved;
            return expl;
        }
        expl.resolved = finish_miss(config, ctx, policy, &expl);
        return expl;
    }

    auto record = [&](ColorRule rule, const RuleResult& r) {
        expl.steps.push_back({ rule, r.outcome, r.candidate, r.resolved, r.reason });
        if (!r.registered_synthetic.empty())
            expl.registered_synthetic = r.registered_synthetic;
        return r.outcome == ColorRuleOutcome::Matched;
    };
    auto apply = [&](ColorRule rule, RuleResult r) {
        if (record(rule, r)) {
            expl.resolved = std::move(r.resolved);
            return true;
        }
        return false;
    };

    // Utility-token short-circuit: data/bypass outrank everything except an
    // ACES flag and an applicable FileRules-First rung.
    const bool file_rules_first_applicable
        = policy.file_rules == ColorFileRules::First && !ctx.filename.empty();
    if (!facts.aces_image_container && !file_rules_first_applicable
        && (facts.color_interop_id == "data" || facts.color_interop_id == "bypass")) {
        if (apply(ColorRule::ColorInteropID,
                  rule_color_interop_id(config, facts, policy, diag)))
            return expl;
    }

    if (apply(ColorRule::AcesContainer, rule_aces(facts)))
        return expl;
    if (apply(ColorRule::FileRulesFirst,
              rule_file_rules(config, ctx, ColorFileRules::First, policy, diag)))
        return expl;
    if (apply(ColorRule::ColorInteropID,
              rule_color_interop_id(config, facts, policy, diag)))
        return expl;
    // CICP before ICC, matching the PNG spec's own chunk precedence
    // (cICP > iCCP).
    if (apply(ColorRule::Cicp, rule_cicp(config, facts, policy, diag)))
        return expl;
    if (apply(ColorRule::IccProfile, rule_icc(facts, policy, diag)))
        return expl;
    if (apply(ColorRule::PngSrgb, rule_png_srgb(config, facts, policy, diag)))
        return expl;
    if (apply(ColorRule::ChromaticitiesAndGamma,
              rule_colorimetry(facts, ctx, policy, /*with_gamma=*/true, diag)))
        return expl;
    if (apply(ColorRule::Chromaticities,
              rule_colorimetry(facts, ctx, policy, /*with_gamma=*/false, diag)))
        return expl;
    if (apply(ColorRule::Gamma, rule_gamma(facts, ctx, policy, diag)))
        return expl;
    if (apply(ColorRule::FileRulesFallback,
              rule_file_rules(config, ctx, ColorFileRules::FallbackOnly, policy, diag)))
        return expl;

    expl.resolved = finish_miss(config, ctx, policy, &expl);
    return expl;
}

ColorMetadataFacts
color_facts_from_spec(const ImageSpec& spec)
{
    ColorMetadataFacts f;
    f.aces_image_container = spec.get_int_attribute("acesImageContainerFlag")
                             == 1;
    if (auto c = spec.find_attribute("colorInteropID", TypeString))
        f.color_interop_id = c->get_ustring().string();
    if (auto icc = spec.find_attribute("ICCProfile")) {
        const auto* d = static_cast<const unsigned char*>(icc->data());
        f.icc_profile.assign(d, d + icc->datasize());
    }
    if (spec.getattribute("CICP", TypeDesc(TypeDesc::INT, 4), f.cicp))
        f.has_cicp = true;
    if (spec.getattribute("chromaticities", TypeDesc(TypeDesc::FLOAT, 8),
                          f.chromaticities))
        f.has_chromaticities = true;
    float g = spec.get_float_attribute("oiio:Gamma", 0.0f);
    if (g > 0.0f) {
        f.has_gamma = true;
        f.gamma     = g;
    }
    // png_srgb has no ImageSpec carrier: the PNG reader folds its sRGB chunk
    // straight into oiio:ColorSpace. It becomes extractable when a reader
    // deposits it as an asset-fact attribute (a per-format change, later PR).
    return f;
}

ColorResolutionExplanation
resolve_color_metadata(const ColorConfig* config, const ImageSpec& spec,
                       const ColorCallContext& ctx,
                       const ColorReadPolicy& policy)
{
    return resolve_color_metadata(config, "", color_facts_from_spec(spec),
                                  ctx, policy);
}

std::string
infer_color_space_from_spec(const ColorConfig* config, const ImageSpec& spec,
                            const ColorCallContext& ctx,
                            const ColorReadPolicy& policy)
{
    const ColorMetadataFacts facts = color_facts_from_spec(spec);
    // A usable answer is a config-local name or a registry-known id the
    // cross-config machinery can construct. A session-synthetic (custom:/
    // icc:) answer names no constructible space in this round -- live
    // synthetic endpoints are a later round's story -- so when one wins,
    // retry under config-only scope, where an unusable signal misses and
    // falls through to the next rung instead of synthesizing.
    auto usable = [](const ColorResolutionExplanation& e) {
        return e.has_genuine_metadata_match()
               && !Strutil::starts_with(e.resolved, "custom:")
               && !Strutil::starts_with(e.resolved, "icc:");
    };
    ColorResolutionExplanation e = resolve_color_metadata(config, "", facts,
                                                          ctx, policy);
    if (usable(e))
        return e.resolved;
    if (e.has_genuine_metadata_match()
        && policy.scope != ColorResolutionScope::ConfigOnly) {
        ColorReadPolicy retry = policy;
        retry.scope           = ColorResolutionScope::ConfigOnly;
        e = resolve_color_metadata(config, "", facts, ctx, retry);
        if (usable(e))
            return e.resolved;
    }
    return {};
}

void
scrub_color_metadata(ImageSpec& spec, const ColorConfig* config,
                     const ColorReadPolicy& policy)
{
    // Judge hints only against an established, definite color space.
    const std::string current = spec.get_string_attribute("oiio:ColorSpace");
    if (current.empty() || current == "unknown")
        return;

    const ColorMetadataFacts facts = color_facts_from_spec(spec);
    const ColorCallContext ctx;  // filenames never influence scrubbing

    // A signal is scrubbed only when its lone-fact resolution genuinely
    // matches -- the trace's proof that the claim is determinate. A
    // determinate claim either names the current space (redundant) or a
    // different one (contradicted by the spec's post-operation state);
    // stale either way. Anything the resolver can't decide stays.
    auto proven = [&](const ColorMetadataFacts& f) {
        return resolve_color_metadata(config, "", f, ctx, policy)
            .has_genuine_metadata_match();
    };

    if (!facts.color_interop_id.empty()) {
        if (Strutil::iequals(facts.color_interop_id, "ocio:unknown")
            || Strutil::iequals(facts.color_interop_id, "oiio:unknown")
            || Strutil::iequals(facts.color_interop_id, "error:unknown")) {
            // The deliberate unknown-marker family (config-declared,
            // OIIO-synthetic-treatment, strict-resolution-failure) is
            // honored: never scrubbed and never inferred over.
        } else if (facts.color_interop_id == "unknown") {
            // A bare "unknown" claim is contradicted by any definite
            // color space.
            spec.erase_attribute("colorInteropID");
        } else {
            ColorMetadataFacts f;
            f.color_interop_id = facts.color_interop_id;
            if (proven(f))
                spec.erase_attribute("colorInteropID");
        }
    }
    if (facts.aces_image_container) {
        ColorMetadataFacts f;
        f.aces_image_container = true;
        if (proven(f))
            spec.erase_attribute("acesImageContainerFlag");
    }
    if (!facts.icc_profile.empty()) {
        ColorMetadataFacts f;
        f.icc_profile = facts.icc_profile;
        if (proven(f))
            spec.erase_attribute("ICCProfile");
    }
    if (facts.has_cicp) {
        ColorMetadataFacts f;
        f.has_cicp = true;
        for (int i = 0; i < 4; ++i)
            f.cicp[i] = facts.cicp[i];
        if (proven(f))
            spec.erase_attribute("CICP");
    }
    if (facts.has_chromaticities) {
        ColorMetadataFacts f;
        f.has_chromaticities = true;
        for (int i = 0; i < 8; ++i)
            f.chromaticities[i] = facts.chromaticities[i];
        f.has_gamma = facts.has_gamma;
        f.gamma     = facts.gamma;
        if (proven(f)) {
            spec.erase_attribute("chromaticities");
            if (facts.has_gamma)
                spec.erase_attribute("oiio:Gamma");
        }
    } else if (facts.has_gamma) {
        ColorMetadataFacts f;
        f.has_gamma = true;
        f.gamma     = facts.gamma;
        if (proven(f))
            spec.erase_attribute("oiio:Gamma");
    }
}

void
reconcile_color_metadata(ImageSpec& spec, const ColorReadPolicy& policy)
{
    // Each reader deposits the raw color attributes it read; this central
    // entry point reproduces the precedence that reader used to hand-roll.
    // Only the signals a reader historically consulted are wired in -- pulling
    // a reader's remaining signals into resolution is a per-format behavior
    // change and lands in its own later PR.

    // The ACES-container flag and colorInteropID: the signals the EXR reader
    // consulted. When either is present, reproduce its former inline
    // special-casing (set_colorspace, which also clears now-contradictory
    // CICP).
    ColorMetadataFacts facts;
    facts.aces_image_container
        = spec.get_int_attribute("acesImageContainerFlag") == 1;
    if (auto c = spec.find_attribute("colorInteropID", TypeString))
        facts.color_interop_id = c->get_ustring().string();

    if (facts.aces_image_container || !facts.color_interop_id.empty()) {
        ColorCallContext ctx;
        const auto expl = resolve_color_metadata(nullptr, "", facts, ctx, policy);
        if (expl.has_genuine_metadata_match())
            spec.set_colorspace(expl.resolved);
        else if (!facts.color_interop_id.empty())
            // A colorInteropID that resolves to nothing is still honored
            // verbatim (the historical passthrough); a later policy may
            // tighten this.
            spec.set_colorspace(facts.color_interop_id);
        return;
    }

    // A CICP tuple: the signal the PNG reader consulted, using the interop id
    // it maps to (via the built-in registry) to override the color space it
    // had already set from the sRGB/gamma chunks. Route that one signal
    // through the same cascade. The override is applied with a plain attribute
    // set -- keeping the CICP source attribute in place -- exactly as the PNG
    // reader used to do inline.
    int cicp[4];
    if (spec.getattribute("CICP", TypeDesc(TypeDesc::INT, 4), cicp)) {
        ColorMetadataFacts cf;
        cf.has_cicp = true;
        for (int i = 0; i < 4; ++i)
            cf.cicp[i] = cicp[i];
        ColorCallContext ctx;
        const auto expl = resolve_color_metadata(nullptr, "", cf, ctx, policy);
        if (expl.has_genuine_metadata_match())
            spec.attribute("oiio:ColorSpace", expl.resolved);
    }
}

ColorReadPolicy
ColorReadPolicy::snapshot(const ImageSpec* config_hints)
{
    // One locked read of the whole policy state, via the shared snapshot
    // primitive (the same mechanism the write-side policy uses). Per-open
    // config hints (if any) win over the global attribute table; both win over
    // the built-in defaults, which are calibrated to reproduce main.
    ColorReadPolicy p;
    ColorPolicySnapshot snap(config_hints);
    auto get_string = [&](const char* name) { return snap.get_string(name); };
    auto get_int    = [&](const char* name, int dflt) {
        return snap.get_int(name, dflt);
    };

    const std::string scope = get_string("oiio:colorpolicy:read:scope");
    if (scope == "config_only")
        p.scope = ColorResolutionScope::ConfigOnly;
    else if (scope == "exact_state")
        p.scope = ColorResolutionScope::ExactState;

    const std::string state = get_string("oiio:colorpolicy:read:state_preference");
    if (state == "scene")
        p.state_pref = ColorStatePreference::Scene;
    else if (state == "display")
        p.state_pref = ColorStatePreference::Display;

    const std::string fr = get_string("oiio:colorpolicy:read:file_rules");
    if (fr == "first")
        p.file_rules = ColorFileRules::First;
    else if (fr == "fallback_only")
        p.file_rules = ColorFileRules::FallbackOnly;

    p.ignore_cicp_for_png = get_int("oiio:colorpolicy:read:ignore_cicp_for_png", 0) != 0;
    p.ignore_sidecar      = get_int("oiio:colorpolicy:read:ignore_sidecar", 0) != 0;
    return p;
}

}  // namespace pvt

OIIO_NAMESPACE_END
