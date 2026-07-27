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
//
// Carve-out: the CICP matrix and range bytes do not participate in
// identification (primaries + transfer only); they are writer-side/format
// concerns.

#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <OpenImageIO/color.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/ustring.h>

#include "color_pvt.h"
#include "imageio_pvt.h"

OIIO_NAMESPACE_BEGIN

namespace pvt {

namespace {

    // The scene/display twin of an interop id: swap a trailing _scene<->_display.
    // Empty if the id carries neither suffix.
    std::string state_twin(const std::string& id)
    {
        if (Strutil::ends_with(id, "_scene"))
            return id.substr(0, id.size() - 6) + "_display";
        if (Strutil::ends_with(id, "_display"))
            return id.substr(0, id.size() - 8) + "_scene";
        return { };
    }

    bool ends_with_scene(const std::string& id)
    { return Strutil::ends_with(id, "_scene"); }

    // The canonical local name a config resolves `cand` to, or "" if the config
    // has no color space reachable by that name/alias/role. A null config never
    // resolves anything locally.
    std::string local_name(const ColorConfig* config, const std::string& cand)
    {
        if (!config)
            return { };
        int idx = config->getColorSpaceIndex(cand);
        if (idx < 0)
            return { };
        const char* name = config->getColorSpaceNameByIndex(idx);
        return name ? std::string(name) : std::string { };
    }

    // Is `id` a known color interop identity in OIIO's built-in registry?
    bool known_registry_id(const std::string& id)
    { return !id.empty() && interop_identities_config_resolves(id); }

    // Order a single candidate for the current state preference: when a
    // preference is set (and not exact-state), the preferred-state twin is
    // tried before the candidate itself.
    std::vector<std::string> state_preference_order(const std::string& candidate,
                                                    const ColorReadPolicy& p)
    {
        if (p.state_pref == ColorStatePreference::Auto
            || p.scope == ColorResolutionScope::ExactState)
            return { candidate };
        const bool want_scene  = p.state_pref == ColorStatePreference::Scene;
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
    std::string resolve_ciid(const ColorConfig* config,
                             const std::string& value, const ColorReadPolicy& p,
                             std::string* selected = nullptr)
    {
        if (is_utility_interop_id(value) || value.empty())
            return { };
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
        return { };
    }

    // Local-name-then-CIID resolution of an explicit / failover assignment.
    std::string resolve_explicit(const ColorConfig* config,
                                 const std::string& value,
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
    std::string resolve_data_space(const ColorConfig* config,
                                   const std::string& token)
    {
        if (!config)
            return token;
        const std::string other = token == "bypass" ? "data" : "bypass";
        auto role_target        = [&](const char* role) -> std::string {
            const char* n = config->getColorSpaceNameByRole(role);
            return n && n[0] ? std::string(n) : std::string { };
        };
        const std::string token_role   = role_target(token.c_str());
        const std::string other_role   = role_target(other.c_str());
        const std::string unknown_role = role_target("unknown");
        auto identifies_as             = [&](const std::string& name,
                                             const std::string& label,
                                             const std::string& role_name) {
            if (Strutil::lower(name) == label)
                return true;
            if (Strutil::lower(std::string(config->get_color_interop_id(name)))
                == label)
                return true;
            for (const std::string& alias : config->getAliases(name))
                if (Strutil::lower(alias) == label)
                    return true;
            return !role_name.empty() && role_name == name;
        };

        std::string best;
        int best_rank                        = 99;
        const std::vector<std::string> names = config->getColorSpaceNames();
        for (const std::string& name : names) {
            if (!config->isData(name))
                continue;
            // OCIO's CreateRaw injects a framework-owned "raw" space; it is not
            // an authored utility target in an otherwise empty config.
            if (names.size() == 1 && Strutil::lower(name) == "raw"
                && token_role.empty())
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
    std::string synthesize_custom_space(const float* chroma, bool has_gamma,
                                        float gamma)
    {
        std::string id = "custom:";
        if (has_gamma)
            id += Strutil::fmt::format("g{:.5f}_", gamma);
        id += Strutil::fmt::format(
            "{:.5f}_{:.5f}_{:.5f}_{:.5f}_{:.5f}_{:.5f}_{:.5f}_{:.5f}",
            chroma[0], chroma[1], chroma[2], chroma[3], chroma[4], chroma[5],
            chroma[6], chroma[7]);
        return id;
    }

    // A deterministic, idempotent synthetic id for a decodable-but-unmatched
    // ICC profile. (The prototype keys this on the profile's MD5; OIIO has no
    // MD5 in-tree, so a stable content hash stands in -- the id grammar and its
    // idempotence are what downstream depends on, not the digest algorithm.)
    std::string synthesize_icc_space(const std::vector<unsigned char>& profile)
    {
        // strhash64, not strhash: the id must be a full 64-bit digest on
        // every platform (strhash is size_t -- 32 bits on 32-bit builds,
        // where collision odds stop being negligible and ids diverge
        // across builds).
        uint64_t h = Strutil::strhash64(
            string_view(reinterpret_cast<const char*>(profile.data()),
                        profile.size()));
        return Strutil::fmt::format("icc:{:016x}", h);
    }

    // ---- individual rules -------------------------------------------------

    struct RuleResult {
        ColorRuleOutcome outcome = ColorRuleOutcome::Inapplicable;
        std::string candidate;
        std::string resolved;
        std::string reason;
        std::string registered_synthetic;
    };

    RuleResult rule_aces(const ColorMetadataFacts& f)
    {
        if (!f.aces_image_container)
            return { };
        return { ColorRuleOutcome::Matched,
                 "lin_ap0_scene",
                 "lin_ap0_scene",
                 { },
                 { } };
    }

    RuleResult rule_file_rules(const ColorConfig* config,
                               const ColorCallContext& ctx,
                               ColorFileRules position,
                               const ColorReadPolicy& p, bool diag)
    {
        if (p.file_rules != position || ctx.filename.empty() || !config)
            return { };
        string_view name = config->getColorSpaceFromFilepath(ctx.filename, "");
        if (!name.empty())
            return { ColorRuleOutcome::Matched,
                     diag ? ctx.filename : std::string { },
                     std::string(name),
                     { },
                     { } };
        return { ColorRuleOutcome::Missed,
                 diag ? ctx.filename : std::string { },
                 { },
                 diag ? "No FileRules entry matched the filename"
                      : std::string { },
                 { } };
    }

    RuleResult rule_color_interop_id(const ColorConfig* config,
                                     const ColorMetadataFacts& f,
                                     const ColorReadPolicy& p, bool diag)
    {
        if (f.color_interop_id.empty())
            return { };
        const std::string& id = f.color_interop_id;
        if (id == "data" || id == "bypass") {
            const std::string local = resolve_data_space(config, id);
            return { ColorRuleOutcome::Matched,
                     diag ? id : std::string { },
                     local.empty() ? id : local,
                     { },
                     { } };
        }
        const std::string resolved = resolve_explicit(config, id, p);
        if (!resolved.empty())
            return { ColorRuleOutcome::Matched,
                     diag ? id : std::string { },
                     resolved,
                     { },
                     { } };
        return { ColorRuleOutcome::Missed,
                 diag ? id : std::string { },
                 { },
                 diag ? "Color interop id did not resolve" : std::string { },
                 { } };
    }

    // ICC (spec black box): a profile is decodable if it carries the 'acsp'
    // signature at offset 36 of a >=128-byte header. A decodable-but-unmatched
    // profile registers and returns an icc: synthetic under lenient scope;
    // undecodable is invalid. (Matching a profile to a named local space is the
    // heavy identification port and is not wired here yet.)
    RuleResult rule_icc(const ColorMetadataFacts& f, const ColorReadPolicy& p,
                        bool diag)
    {
        if (f.icc_profile.empty())
            return { };
        const bool decodable = f.icc_profile.size() >= 128
                               && f.icc_profile[36] == 'a'
                               && f.icc_profile[37] == 'c'
                               && f.icc_profile[38] == 's'
                               && f.icc_profile[39] == 'p';
        if (!decodable)
            return { ColorRuleOutcome::Invalid,
                     diag ? "icc" : std::string { },
                     { },
                     diag ? "ICC profile is not a decodable profile"
                          : std::string { },
                     { } };
        if (p.scope != ColorResolutionScope::Lenient)
            return {
                ColorRuleOutcome::Missed,
                diag ? "icc" : std::string { },
                { },
                diag
                    ? "ICC profile decoded but the resolution scope rejected it"
                    : std::string { },
                { }
            };
        const std::string id = synthesize_icc_space(f.icc_profile);
        return { ColorRuleOutcome::Matched,
                 diag ? "icc" : std::string { },
                 id,
                 { },
                 id };
    }

    RuleResult rule_cicp(const ColorConfig* config, const ColorMetadataFacts& f,
                         const ColorReadPolicy& p, bool diag)
    {
        if (!f.has_cicp)
            return { };
        // Spec-impossibility watch: log-only, never an error, never a
        // correction -- identification below is unaffected and still keys on
        // primaries + transfer alone. primaries==0 / transfer==0 are ITU-T
        // H.273 reserved values. A non-zero matrix from a format whose stored
        // essence is RGB-only (e.g. PNG) is likewise impossible, but format
        // identity is not available in this layer (source-provenance
        // attributes are deposited after read), so only the reserved-value
        // cases are reported.
        if (f.cicp[0] == 0)
            OIIO::debugfmt(
                "color reconcile: CICP tuple {},{},{},{} carries reserved "
                "primaries value 0 (ITU-T H.273)\n",
                f.cicp[0], f.cicp[1], f.cicp[2], f.cicp[3]);
        if (f.cicp[1] == 0)
            OIIO::debugfmt(
                "color reconcile: CICP tuple {},{},{},{} carries reserved "
                "transfer value 0 (ITU-T H.273)\n",
                f.cicp[0], f.cicp[1], f.cicp[2], f.cicp[3]);
        // CICP -> interop-id mapping routes through the same central resolve the
        // config exposes (primaries + transfer only; matrix/range are unused).
        // The mapping is a built-in table lookup; a config is only constructed
        // when the caller supplied none (rare -- readers pass their config).
        std::unique_ptr<ColorConfig> registry;
        if (!config)
            registry.reset(new ColorConfig);
        const ColorConfig* map_cfg = config ? config : registry.get();
        string_view raw            = map_cfg->get_color_interop_id(f.cicp);
        if (raw.empty())
            return {
                ColorRuleOutcome::Missed,
                diag ? Strutil::fmt::format("{}/{}", f.cicp[0], f.cicp[1])
                     : std::string { },
                { },
                diag
                    ? "CICP primaries and transfer did not map to a known identity"
                    : std::string { },
                { }
            };
        // CICP state is governed by its own one-shot axis (cicp_state). Auto
        // falls back to the general state_pref so an unset cicp_state
        // reproduces main exactly.
        ColorReadPolicy pc = p;
        if (p.cicp_state != ColorStatePreference::Auto)
            pc.state_pref = p.cicp_state;
        std::string candidate(raw);
        for (const auto& c : state_preference_order(candidate, pc)) {
            std::string selected;
            const std::string resolved = resolve_ciid(config, c, pc, &selected);
            if (!resolved.empty())
                return { ColorRuleOutcome::Matched,
                         diag ? (selected.empty() ? c : selected)
                              : std::string { },
                         resolved,
                         { },
                         { } };
        }
        // Nothing local; under non-config-only scope the candidate id itself is
        // the (bridged) answer.
        if (p.scope != ColorResolutionScope::ConfigOnly)
            return { ColorRuleOutcome::Matched,
                     diag ? candidate : std::string { },
                     candidate,
                     { },
                     { } };
        return {
            ColorRuleOutcome::Missed,
            diag ? candidate : std::string { },
            { },
            diag
                ? "CICP identity is unavailable in the config under config-only scope"
                : std::string { },
            { }
        };
    }

    RuleResult rule_png_srgb(const ColorConfig* config,
                             const ColorMetadataFacts& f,
                             const ColorReadPolicy& p, bool diag)
    {
        if (!f.png_srgb)
            return { };
        const std::string id = "srgb_rec709_display";
        std::string selected;
        const std::string resolved = resolve_ciid(config, id, p, &selected);
        if (!resolved.empty())
            return { ColorRuleOutcome::Matched,
                     diag ? (selected.empty() ? id : selected)
                          : std::string { },
                     resolved,
                     { },
                     { } };
        if (p.scope != ColorResolutionScope::ConfigOnly)
            return { ColorRuleOutcome::Matched,
                     diag ? id : std::string { },
                     id,
                     { },
                     { } };
        return {
            ColorRuleOutcome::Missed,
            diag ? id : std::string { },
            { },
            diag ? "PNG sRGB identity is unavailable under config-only scope"
                 : std::string { },
            { }
        };
    }

    // Colorimetry (chromaticities / gamma): the config-matching tiers
    // (encoded-gamut-first, round-then-exact chromaticity equality, the
    // transfer-function slope catalog) are the heavy port and are not wired
    // here yet. What ships is the shape: the nonlinear-gamma split, and the
    // lenient custom: synthesis with the state choice the id grammar carries.
    RuleResult rule_colorimetry(const ColorMetadataFacts& f,
                                const ColorCallContext& ctx,
                                const ColorReadPolicy& p, bool with_gamma,
                                bool diag)
    {
        if (!f.has_chromaticities)
            return { };
        const bool has_nonlinear = f.has_gamma && f.gamma > 1.001f;
        if (with_gamma != has_nonlinear)
            return { };
        if (p.scope == ColorResolutionScope::Lenient) {
            const std::string id = synthesize_custom_space(f.chromaticities,
                                                           has_nonlinear,
                                                           f.gamma);
            (void)ctx;
            return { ColorRuleOutcome::Matched,
                     diag ? id : std::string { },
                     id,
                     { },
                     id };
        }
        return { ColorRuleOutcome::Missed,
                 diag ? "chromaticities" : std::string { },
                 { },
                 diag ? "No color space matched the supplied chromaticities"
                      : std::string { },
                 { } };
    }

    RuleResult rule_gamma(const ColorMetadataFacts& f,
                          const ColorCallContext& ctx, const ColorReadPolicy& p,
                          bool diag)
    {
        if (!f.has_gamma || f.has_chromaticities)
            return { };
        const bool has_nonlinear = f.gamma > 1.001f;
        if (p.scope == ColorResolutionScope::Lenient) {
            static const float kRec709[8] = { 0.64f, 0.33f, 0.30f,   0.60f,
                                              0.15f, 0.06f, 0.3127f, 0.3290f };
            const std::string id
                = synthesize_custom_space(kRec709, has_nonlinear, f.gamma);
            (void)ctx;
            return { ColorRuleOutcome::Matched,
                     diag ? id : std::string { },
                     id,
                     { },
                     id };
        }
        return { ColorRuleOutcome::Missed,
                 diag ? Strutil::fmt::format("{}", f.gamma) : std::string { },
                 { },
                 diag ? "No color space matched gamma under the active scope"
                      : std::string { },
                 { } };
    }

    // The config's Default Assignment: FileRules final entry, else the default
    // role. Empty if neither resolves.
    std::string default_assignment(const ColorConfig* config,
                                   std::string* reason)
    {
        if (!config)
            return { };
        // The single-arg form returns the config's default-rule color space when
        // nothing else matches -- i.e. the FileRules Default Assignment.
        string_view fr = config->getColorSpaceFromFilepath(
            "oiio_color_metadata_default_probe");
        if (!fr.empty()) {
            if (reason)
                *reason = "Resolved from the FileRules Default Assignment";
            return std::string(fr);
        }
        const char* role = config->getColorSpaceNameByRole("default");
        if (role && role[0]) {
            if (reason)
                *reason
                    = "FileRules Default Assignment was invalid; resolved from the "
                      "default role";
            return std::string(role);
        }
        return { };
    }

    std::string finish_miss(const ColorConfig* config,
                            const ColorCallContext& ctx,
                            const ColorReadPolicy& p,
                            ColorResolutionExplanation* expl)
    {
        // Failover: a caller-supplied assignment tried after metadata misses.
        if (!ctx.failover.empty()) {
            const std::string resolved = resolve_explicit(config, ctx.failover,
                                                          p);
            if (!resolved.empty()) {
                if (expl) {
                    expl->steps.push_back({ ColorRule::Failover,
                                            ColorRuleOutcome::Matched,
                                            ctx.failover,
                                            resolved,
                                            { } });
                    expl->used_failover = true;
                }
                return resolved;
            }
            if (expl)
                expl->steps.push_back(
                    { ColorRule::Failover,
                      ColorRuleOutcome::Invalid,
                      ctx.failover,
                      { },
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
                                            ColorRuleOutcome::Matched,
                                            { },
                                            resolved,
                                            reason });
                    expl->used_default = true;
                }
                return resolved;
            }
        }
        return { };
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
        return it->rule != ColorRule::Failover
               && it->rule != ColorRule::ConfigDefault
               && it->rule != ColorRule::StrictParsing;
    }
    return false;
}

ColorResolutionExplanation
resolve_color_metadata(const ColorConfig* config,
                       const std::string& explicit_assignment,
                       const ColorMetadataFacts& facts,
                       const ColorCallContext& ctx,
                       const ColorReadPolicy& policy)
{
    ColorResolutionExplanation expl;
    const bool diag = true;  // the engine always keeps its trace

    // Rule 1: an explicit assignment suppresses every metadata rule.
    if (!explicit_assignment.empty()) {
        const std::string resolved
            = resolve_explicit(config, explicit_assignment, policy);
        expl.steps.push_back({ ColorRule::ExplicitAssignment,
                               resolved.empty() ? ColorRuleOutcome::Missed
                                                : ColorRuleOutcome::Matched,
                               explicit_assignment,
                               resolved,
                               { } });
        if (!resolved.empty()) {
            expl.resolved = resolved;
            return expl;
        }
        expl.resolved = finish_miss(config, ctx, policy, &expl);
        return expl;
    }

    auto record = [&](ColorRule rule, const RuleResult& r) {
        expl.steps.push_back(
            { rule, r.outcome, r.candidate, r.resolved, r.reason });
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
        && (facts.color_interop_id == "data"
            || facts.color_interop_id == "bypass")) {
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
              rule_file_rules(config, ctx, ColorFileRules::FallbackOnly, policy,
                              diag)))
        return expl;

    expl.resolved = finish_miss(config, ctx, policy, &expl);
    return expl;
}

ColorReadCaps
color_read_caps_for_format(string_view format_name)
{
    // Every wired reader's consulted set is currently the same trio: the
    // signals the readers historically consulted (EXR: the ACES-container
    // flag and colorInteropID; PNG: CICP), applied format-invariantly
    // exactly as the reconciler's former inline extraction did -- including
    // for an unknown/empty format name, which preserves the historical
    // behavior for any caller that has no format to declare.
    // ponytail: identical rows today by design (behavior-preserving);
    // per-format divergence is a future data edit here, not inline code.
    (void)format_name;
    ColorReadCaps caps;
    caps.aces_container = true;
    caps.interop_id     = true;
    caps.cicp           = true;
    return caps;
}

ColorMetadataFacts
color_facts_from_spec(const ImageSpec& spec, const ColorReadCaps& caps)
{
    ColorMetadataFacts f;
    if (caps.aces_container)
        f.aces_image_container
            = spec.get_int_attribute("acesImageContainerFlag") == 1;
    if (caps.interop_id)
        if (auto c = spec.find_attribute("colorInteropID", TypeString))
            f.color_interop_id = c->get_ustring().string();
    if (caps.icc)
        if (auto icc = spec.find_attribute("ICCProfile")) {
            const auto* d = static_cast<const unsigned char*>(icc->data());
            f.icc_profile.assign(d, d + icc->datasize());
        }
    if (caps.cicp
        && spec.getattribute("CICP", TypeDesc(TypeDesc::INT, 4), f.cicp))
        f.has_cicp = true;
    if (caps.chromaticities
        && spec.getattribute("chromaticities", TypeDesc(TypeDesc::FLOAT, 8),
                             f.chromaticities))
        f.has_chromaticities = true;
    if (caps.gamma) {
        float g = spec.get_float_attribute("oiio:Gamma", 0.0f);
        if (g > 0.0f) {
            f.has_gamma = true;
            f.gamma     = g;
        }
    }
    // png_srgb has no ImageSpec carrier: the PNG reader folds its sRGB chunk
    // straight into oiio:ColorSpace. It becomes extractable when a reader
    // deposits it as an asset-fact attribute (a per-format change, later PR).
    return f;
}

ColorMetadataFacts
color_facts_from_spec(const ImageSpec& spec)
{
    return color_facts_from_spec(spec, ColorReadCaps::all());
}

ColorResolutionExplanation
resolve_color_metadata(const ColorConfig* config, const ImageSpec& spec,
                       const ColorCallContext& ctx,
                       const ColorReadPolicy& policy)
{
    return resolve_color_metadata(config, "", color_facts_from_spec(spec), ctx,
                                  policy);
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
    return { };
}

void
scrub_color_metadata(ImageSpec& spec)
{
    // Two-bucket rule: the file-provenance facts describe the SOURCE the
    // pixels came from; after an identity-known color change (which the
    // caller asserts) they are categorically stale -- never persist stale
    // information. No per-signal re-resolution: the bucket is a static
    // property of each attribute, not a per-input verdict.
    const std::string id = spec.get_string_attribute("colorInteropID");
    if (!id.empty() && !is_unknown_marker(id))
        // A definite (or bare-"unknown") claim named the pre-operation
        // space; the deliberate unknown-marker family is honored -- those
        // markers carry treatment/error state, not provenance.
        spec.erase_attribute("colorInteropID");
    spec.erase_attribute("acesImageContainerFlag");
    spec.erase_attribute("ICCProfile");
    spec.erase_attribute("CICP");
    spec.erase_attribute("oiio:cicp:pending");
    spec.erase_attribute("chromaticities");
    spec.erase_attribute("oiio:Gamma");
}

void
reconcile_color_metadata(ImageSpec& spec, const ColorReadPolicy& policy,
                         string_view format_name)
{
    // Each reader deposits the raw color attributes it read; this central
    // entry point reproduces the precedence that reader used to hand-roll.
    // Only the signals the format's read caps declare consulted enter
    // resolution -- extraction goes through the one shared spec->facts
    // reader, narrowed by the per-format table, so pulling a reader's
    // remaining signals into resolution is a caps data edit (a per-format
    // behavior change, its own later PR), not new inline code.
    const ColorMetadataFacts facts
        = color_facts_from_spec(spec, color_read_caps_for_format(format_name));

    // The ACES-container flag and colorInteropID: the signals the EXR reader
    // consulted. When either is present, reproduce its former inline
    // special-casing (set_colorspace, which also clears now-contradictory
    // CICP).
    if (facts.aces_image_container || !facts.color_interop_id.empty()) {
        ColorCallContext ctx;
        const auto expl = resolve_color_metadata(nullptr, "", facts, ctx,
                                                 policy);
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
    if (facts.has_cicp) {
        // Deferred option (spec 09): a CICP tuple is state-ambiguous, so under
        // a defer_cicp policy the reader deposits the tuple as *pending* --
        // marker only, no color-space commit -- giving the caller a window to
        // set cicp_state before resolve_pending_cicp fires. Default policy
        // stays eager (main's behavior).
        if (policy.defer_cicp) {
            spec.attribute("oiio:cicp:pending", 1);
            return;
        }
        ColorMetadataFacts cf;
        cf.has_cicp = true;
        for (int i = 0; i < 4; ++i)
            cf.cicp[i] = facts.cicp[i];
        ColorCallContext ctx;
        const auto expl = resolve_color_metadata(nullptr, "", cf, ctx, policy);
        if (expl.has_genuine_metadata_match())
            spec.attribute("oiio:ColorSpace", expl.resolved);
    }
}

bool
resolve_pending_cicp(ImageSpec& spec, const ColorReadPolicy& policy,
                     const ColorConfig* config)
{
    // No-op unless a deferred read left a pending CICP tuple.
    if (spec.get_int_attribute("oiio:cicp:pending") != 1)
        return false;

    const ColorMetadataFacts facts = color_facts_from_spec(spec);
    bool committed                 = false;
    if (facts.has_cicp) {
        ColorMetadataFacts cf;
        cf.has_cicp = true;
        for (int i = 0; i < 4; ++i)
            cf.cicp[i] = facts.cicp[i];
        ColorCallContext ctx;
        const auto expl = resolve_color_metadata(config, "", cf, ctx, policy);
        if (expl.has_genuine_metadata_match()) {
            spec.attribute("oiio:ColorSpace", expl.resolved);
            committed = true;
        }
    }

    // Consume-once: the pending tuple is now resolved, so the marker and the
    // one-shot global cicp_state key no longer apply -- clearing the global
    // stops it silently re-applying to the next file. A per-call/config-hint
    // or config-profile cicp_state is intentionally NOT consumed (it is
    // scoped or declared policy, not a lingering global override); this
    // preserves the per-call > profile > global-default precedence.
    spec.erase_attribute("oiio:cicp:pending");
    OIIO::attribute("oiio:colorpolicy:read:cicp_state", "");
    return committed;
}

const ColorConfig*
ambient_color_config()
{
    // Spec 09: the ambient config drives I/O color-metadata policy. When OCIO
    // support is unavailable there is no config to consult -- return null so
    // readers/writers behave exactly as the historical null-config snapshot.
    // ponytail: default_colorconfig() is a cached singleton; the only per-read
    // cost is one FileRules scan in config_declared_policy_keys (a handful of
    // rules). Cache the extracted keys on the config if that ever shows up hot.
    if (!ColorConfig::supportsOpenColorIO())
        return nullptr;
    return &ColorConfig::default_colorconfig();
}


ColorReadPolicy
ColorReadPolicy::snapshot(const ImageSpec* config_hints,
                         const ColorConfig* config, string_view filepath)
{
    // One locked read of the whole policy state, via the shared snapshot
    // primitive (the same mechanism the write-side policy uses). Full spec-09
    // ladder, strongest first: per-call hints (layer 6) > the config file-rule
    // matching `filepath` (layer 5) > global attribute table (layer 4) > the
    // config author's declared `oiio:default`/profile policy (layer 2/3) > the
    // built-in defaults (layer 1), calibrated to reproduce main.
    ColorReadPolicy p;
    ColorPolicySnapshot snap(config_hints, config, filepath);
    auto get_string = [&](const char* name) { return snap.get_string(name); };
    auto get_int    = [&](const char* name, int dflt) {
        return snap.get_int(name, dflt);
    };

    const std::string scope = get_string("oiio:colorpolicy:read:scope");
    if (scope == "config_only")
        p.scope = ColorResolutionScope::ConfigOnly;
    else if (scope == "exact_state")
        p.scope = ColorResolutionScope::ExactState;

    const std::string state = get_string(
        "oiio:colorpolicy:read:state_preference");
    if (state == "scene")
        p.state_pref = ColorStatePreference::Scene;
    else if (state == "display")
        p.state_pref = ColorStatePreference::Display;

    const std::string fr = get_string("oiio:colorpolicy:read:file_rules");
    if (fr == "first")
        p.file_rules = ColorFileRules::First;
    else if (fr == "fallback_only")
        p.file_rules = ColorFileRules::FallbackOnly;

    p.ignore_cicp_for_png
        = get_int("oiio:colorpolicy:read:ignore_cicp_for_png", 0) != 0;
    p.ignore_sidecar = get_int("oiio:colorpolicy:read:ignore_sidecar", 0) != 0;

    // CICP-specific one-shot state axis; empty/unset -> Auto (reproduces main).
    const std::string cicp_state = get_string(
        "oiio:colorpolicy:read:cicp_state");
    if (cicp_state == "scene")
        p.cicp_state = ColorStatePreference::Scene;
    else if (cicp_state == "display")
        p.cicp_state = ColorStatePreference::Display;
    p.defer_cicp = get_int("oiio:colorpolicy:read:defer_cicp", 0) != 0;
    return p;
}

static const char*
rule_name(ColorRule r)
{
    switch (r) {
    case ColorRule::ExplicitAssignment: return "ExplicitAssignment";
    case ColorRule::AcesContainer: return "AcesContainer";
    case ColorRule::FileRulesFirst: return "FileRulesFirst";
    case ColorRule::ColorInteropID: return "ColorInteropID";
    case ColorRule::Cicp: return "CICP";
    case ColorRule::IccProfile: return "IccProfile";
    case ColorRule::PngSrgb: return "PngSrgb";
    case ColorRule::ChromaticitiesAndGamma: return "ChromaticitiesAndGamma";
    case ColorRule::Chromaticities: return "Chromaticities";
    case ColorRule::Gamma: return "Gamma";
    case ColorRule::FileRulesFallback: return "FileRulesFallback";
    case ColorRule::Failover: return "Failover";
    case ColorRule::ConfigDefault: return "ConfigDefault";
    case ColorRule::StrictParsing: return "StrictParsing";
    }
    return "?";
}

static const char*
outcome_name(ColorRuleOutcome o)
{
    switch (o) {
    case ColorRuleOutcome::Matched: return "matched";
    case ColorRuleOutcome::Missed: return "missed";
    case ColorRuleOutcome::Invalid: return "invalid";
    default: return "inapplicable";
    }
}

std::string
render_color_read_plan(const ImageSpec& spec, const ColorConfig* config)
{
    const ColorConfig& cfg = config ? *config
                                    : ColorConfig::default_colorconfig();
    // The provenance attributes (deposited on read) give FileRules-gated rules
    // real filename/format to work with; empty when the spec carries neither.
    ColorCallContext ctx;
    ctx.filename = spec.get_string_attribute("oiio:SourcePath");
    ctx.format   = spec.get_string_attribute("oiio:SourceFormat");
    // Preview the full spec-09 ladder: consult the config's declared policy
    // (layer 2/3) and the file-rule matching this source path (layer 5), the
    // same as a real read of this file would.
    const ColorReadPolicy policy = ColorReadPolicy::snapshot(nullptr, &cfg,
                                                             ctx.filename);
    const ColorMetadataFacts facts        = color_facts_from_spec(spec);
    const ColorResolutionExplanation expl = resolve_color_metadata(&cfg, spec,
                                                                   ctx, policy);

    std::string out
        = Strutil::fmt::format("Color read plan (source: {}, format: {}):\n",
                               ctx.filename.empty() ? "-" : ctx.filename,
                               ctx.format.empty() ? "-" : ctx.format);
    for (const auto& s : expl.steps) {
        std::string detail;
        if (!s.resolved.empty())
            detail = Strutil::fmt::format("'{}'", s.resolved);
        else if (!s.candidate.empty())
            detail = Strutil::fmt::format("'{}'", s.candidate);
        if (s.rule == ColorRule::Cicp && facts.has_cicp)
            detail += Strutil::fmt::format(" (tuple {},{},{},{})",
                                           facts.cicp[0], facts.cicp[1],
                                           facts.cicp[2], facts.cicp[3]);
        if (!s.reason.empty())
            detail += Strutil::fmt::format(" ({})", s.reason);
        out += Strutil::fmt::format("  rule {:<2} {:<24} {:<13} {}\n",
                                    int(s.rule) + 1, rule_name(s.rule),
                                    outcome_name(s.outcome), detail);
    }
    if (expl.resolved.empty()) {
        out += "Resolved color space: (unresolved -- no rule matched)\n";
    } else {
        const char* via = "?";
        for (auto it = expl.steps.rbegin(); it != expl.steps.rend(); ++it)
            if (it->outcome == ColorRuleOutcome::Matched) {
                via = rule_name(it->rule);
                break;
            }
        out += Strutil::fmt::format("Resolved color space: '{}' (via {})\n",
                                    expl.resolved, via);
    }
    return out;
}

}  // namespace pvt

OIIO_NAMESPACE_END
