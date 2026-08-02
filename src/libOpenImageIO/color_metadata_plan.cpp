// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// One central write-side color-metadata derivation, replacing the per-plugin
// "figure out which color attributes to emit for this color space" code that
// every writer used to hand-roll. A writer now declares which signals its
// format can carry and consumes the computed plan; all write-side policy
// (the never-guess omission rule, the provenance suppression rule, the
// id -> CICP table lookup) lives here, and name -> interop id derivation is
// consumed from the shared characterization engine (color_characterization
// .cpp) rather than derived privately.
//
// This is the write-side twin of the read-side reconciler and is deliberately
// kept a separate module: read and write share one policy namespace but no
// engine. The only thing shared is the single-locked-snapshot primitive
// (ColorPolicySnapshot), which both policy readers route through so the lock
// discipline is defined once.

#include <mutex>
#include <string>
#include <vector>

#include <OpenImageIO/color.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>

#include "color_pvt.h"
#include "imageio_pvt.h"

OIIO_NAMESPACE_BEGIN

namespace pvt {

// The one process-global lock behind every colorpolicy snapshot (read and
// write). A snapshot holds it for its lifetime; all gets happen inside, so a
// call reads a single consistent view with no mid-call re-lock.
static std::mutex&
color_policy_mutex()
{
    static std::mutex m;
    return m;
}

void
apply_profile_selection(std::map<std::string, std::string>& keys,
                        const ColorConfig& config, string_view selection)
{
    for (string_view raw : Strutil::splitsv(selection, ",")) {
        std::string entry(Strutil::strip(raw));
        if (entry.empty())
            continue;
        bool remove = false;
        if (entry.front() == '+') {
            entry.erase(0, 1);
        } else if (entry.front() == '-') {
            remove = true;
            entry.erase(0, 1);
        }
        entry = Strutil::strip(entry);
        if (entry.empty())
            continue;

        // A target on the policy axis (`read:...`/`write:...`) is a single
        // key; anything else is a whole profile name (a config rule name).
        if (Strutil::starts_with(entry, "read:")
            || Strutil::starts_with(entry, "write:")) {
            std::string key = entry, value;
            if (auto eq = entry.find('='); eq != std::string::npos) {
                key   = entry.substr(0, eq);
                value = entry.substr(eq + 1);
            }
            const std::string full = "oiio:colorpolicy:" + key;
            if (remove)
                keys.erase(full);
            else
                keys[full] = value;  // set (value may be empty for a bare +key)
        } else {
            // Profile: merge (or erase) its declared keys. An undefined profile
            // yields no keys -- a graceful fall-through (spec 09).
            const auto profile_keys = config_declared_policy_keys(config,
                                                                  entry);
            for (const auto& kv : profile_keys) {
                if (remove)
                    keys.erase(kv.first);
                else
                    keys[kv.first] = kv.second;  // cascades over earlier entries
            }
        }
    }
}


ColorPolicySnapshot::ColorPolicySnapshot(const ImageSpec* hints,
                                         const ColorConfig* config,
                                         string_view filepath)
    : m_hints(hints)
    , m_lock(color_policy_mutex())
{
    // Config-declared policy (spec 09): the config author's own opinions,
    // read here under the same lock so the whole snapshot is one consistent
    // view. Merged weakest->strongest into m_config_keys, which get_string /
    // get_int consult as ONE layer BELOW the global attribute table (so an
    // explicit OIIO::attribute still overrides -- ladder layer 4 > 2/3).
    if (config) {
        // Layer 2: the config's `oiio:default` profile -- its baseline
        // alteration of OIIO's builtin defaults.
        m_config_keys = config_declared_policy_keys(*config, "oiio:default");

        // Layer 3 (spec 09): active profiles, a composable +/- selection. Two
        // entry points compose over the layer-2 baseline just loaded: the env
        // var OPENIMAGEIO_COLORPOLICY is the base, then the global attribute
        // `oiio:colorpolicy:profile` composes on top (more explicit /
        // programmatic wins, so an attribute `-entry` can subtract what the env
        // var added). Each mutates m_config_keys, which get_string/get_int read
        // BELOW the global individual-key table (layer 4), so an absolute
        // per-key OIIO::attribute still overrides a selected profile.
        apply_profile_selection(m_config_keys, *config,
                                Sysutil::getenv("OPENIMAGEIO_COLORPOLICY"));
        std::string attrsel;
        OIIO::getattribute("oiio:colorpolicy:profile", attrsel);
        apply_profile_selection(m_config_keys, *config, attrsel);

        // Layer 5 (spec 09): the per-file opinions of the config file-rule that
        // MATCHES this file's path. Kept separate from m_config_keys because it
        // sits ABOVE the global attribute table (layer 4), not below it -- the
        // documented CSS-specificity rung (a file-matching rule outranks a
        // user's "absolute" global key; only the per-call hint, layer 6, wins).
        if (!filepath.empty())
            m_matched_keys = config_matched_rule_policy_keys(*config, filepath);
    }
}

std::string
ColorPolicySnapshot::get_string(const char* name, ColorPlanDecider* layer) const
{
    if (m_hints) {
        if (auto a = m_hints->find_attribute(name, TypeString)) {
            if (layer)
                *layer = ColorPlanDecider::PerSpecAttribute;
            return a->get_ustring().string();
        }
    }
    // Layer 5: the matched file-rule's per-file key -- above the global table.
    {
        auto it = m_matched_keys.find(name);
        if (it != m_matched_keys.end() && !it->second.empty()) {
            if (layer)
                *layer = ColorPlanDecider::MatchedRule;
            return it->second;
        }
    }
    std::string v;
    if (OIIO::getattribute(name, v) && !v.empty()) {
        if (layer)
            *layer = ColorPlanDecider::GlobalAttribute;
        return v;
    }
    // Below the global table: the config author's declared policy (spec 09).
    auto it = m_config_keys.find(name);
    if (it != m_config_keys.end() && !it->second.empty()) {
        if (layer)
            *layer = ColorPlanDecider::ConfigDeclared;
        return it->second;
    }
    if (layer)
        *layer = ColorPlanDecider::BuiltinDefault;
    return {};
}

int
ColorPolicySnapshot::get_int(const char* name, int dflt) const
{
    if (m_hints) {
        if (auto a = m_hints->find_attribute(name, TypeInt))
            return a->get_int();
    }
    {
        auto it = m_matched_keys.find(name);  // layer 5, above the global table
        if (it != m_matched_keys.end())
            return Strutil::from_string<int>(it->second);
    }
    int v = dflt;
    if (OIIO::getattribute(name, v))
        return v;
    auto it = m_config_keys.find(name);
    if (it != m_config_keys.end())
        return Strutil::from_string<int>(it->second);
    return dflt;
}


namespace {

    ColorSignalPolicy parse_signal(const std::string& v)
    {
        if (v == "always")
            return ColorSignalPolicy::Always;
        if (v == "never")
            return ColorSignalPolicy::Never;
        return ColorSignalPolicy::Auto;  // "" or "auto" -- today's behavior
    }

    // Resolve one signal to an action + value carrier. `explicit_present` is a
    // value the author already put on the spec (emitted verbatim, Write);
    // `derived` is what OIIO could derive from the color space ("" / empty ==
    // couldn't determine -> Omit, the never-guess rule). A "never" policy
    // suppresses the signal outright; the format-capability gate is applied by
    // the caller.
    ColorPlanField plan_string_signal(ColorSignalPolicy pol, bool capable,
                                      const std::string& explicit_present,
                                      const std::string& derived)
    {
        ColorPlanField f;
        if (!capable || pol == ColorSignalPolicy::Never) {
            f.action = capable ? ColorPlanAction::Suppress
                               : ColorPlanAction::Omit;
            return f;
        }
        if (!explicit_present.empty()) {
            // Verbatim in all modes: the author's bytes are theirs. The marker
            // collapse below is deliberately NOT applied here -- it governs what
            // OIIO SYNTHESIZES, not what a user wrote.
            f.action = ColorPlanAction::Write;
            f.str    = explicit_present;
        } else if (!derived.empty()) {
            // Writer boundary for the unknown-marker family (ADR-0020 Amendment
            // 2). The markers are OIIO's INTERNAL taxonomy: they carry the *why*
            // behind an unknown, and the `ocio` namespace in particular is
            // reserved to the OpenColorIO project. A file gets the Color Interop
            // Forum's registered vocabulary and nothing else, so a derived marker
            // is translated on the way out:
            //   ocio:unknown, error:unknown -> bare "unknown". Nothing is silently
            //     dropped: "unknown" is the Forum's registered utility id for
            //     exactly this case, so the FACT survives and only OIIO's private
            //     reason for it is discarded.
            //   oiio:unknown -> omitted. It is a TREATMENT marker (synthetic
            //     isData/NoOp) that may legally coexist with a definite
            //     oiio:ColorSpace under the disparity rule, so it makes no
            //     identity claim at all -- and colorInteropID is an identity
            //     field. Emitting it there would be a category error.
            // ponytail: nothing today synthesizes oiio:/error:unknown into an id
            // (only ocio:unknown is minted, color_ocio.cpp), so those two arms are
            // currently unreachable. They are stated anyway so the boundary is
            // correct the day a derive path does produce them.
            switch (classify_interop_marker(derived)) {
            case InteropMarker::OiioUnknown: return f;  // stays Omit
            case InteropMarker::OcioUnknown:
            case InteropMarker::ErrorUnknown: f.str = "unknown"; break;
            default: f.str = derived; break;
            }
            f.action = ColorPlanAction::Derive;
        }
        return f;  // else stays Omit
    }

    // Feature 2 (spec 09): derive a display gamma from an interop id whose transfer
    // is a *pure* power law, probed as a `g<digits>_` prefix token on the lowered
    // id (g18/g22/g24/g26 -> 1.8/2.2/2.4/2.6). Returns 0 when the id names no pure
    // power-law transfer (e.g. sRGB piecewise, log, PQ) -- verbose never guesses a
    // gamma for a curve that is not a single exponent, so the emitted gAMA stays
    // consistent with the space. Mirrors the pure-gamma cases the PNG writer
    // already special-cases inline.
    float gamma_from_id(string_view interop_id)
    {
        const std::string lo = Strutil::lower(interop_id);
        static const std::pair<const char*, float> table[] = {
            { "g18_", 1.8f },
            { "g22_", 2.2f },
            { "g24_", 2.4f },
            { "g26_", 2.6f },
        };
        for (const auto& [tok, g] : table)
            if (Strutil::starts_with(lo, tok))
                return g;
        return 0.0f;
    }

    // True when an interop id names P3-D65 gamut content (a `p3d65` gamut token).
    bool is_p3d65_content(string_view interop_id)
    {
        return Strutil::lower(interop_id).find("p3d65") != std::string::npos;
    }

    // Feature B (spec 09): oiio:default's declared write-canonical space mappings.
    // The one locked mapping: g26_p3d65_display (the P3-primaries DCDM form) is
    // canonicalized to g26_xyzd65_display -- a P3->XYZ primaries conversion WITHIN
    // the DCI-white-scaled DCDM family (gamma 2.6 + DCI white headroom, alias
    // dcdm_xyzd65), NOT a headroom change and NOT the P3-primaries form. Any other
    // id passes through unchanged. See spec 09 "Write-canonical mapping in
    // oiio:default".
    std::string canonical_write_id(string_view interop_id)
    {
        if (interop_id == "g26_p3d65_display")
            return "g26_xyzd65_display";
        return std::string(interop_id);
    }

}  // namespace


ColorWritePolicy
ColorWritePolicy::snapshot(const ImageSpec* config_hints,
                           const ColorConfig* config, string_view filepath)
{
    ColorWritePolicy p;
    ColorPolicySnapshot snap(config_hints, config, filepath);

    p.cicp = parse_signal(
        snap.get_string("oiio:colorpolicy:write:cicp", &p.cicp_layer));
    p.chromaticities = parse_signal(
        snap.get_string("oiio:colorpolicy:write:chromaticities",
                        &p.chromaticities_layer));
    p.gamma = parse_signal(
        snap.get_string("oiio:colorpolicy:write:gamma", &p.gamma_layer));
    p.icc = parse_signal(
        snap.get_string("oiio:colorpolicy:write:icc", &p.icc_layer));
    p.interop_id = parse_signal(
        snap.get_string("oiio:colorpolicy:write:interop_id",
                        &p.interop_id_layer));
    p.mdcv = parse_signal(
        snap.get_string("oiio:colorpolicy:write:mdcv", &p.mdcv_layer));

    p.force_interop_id
        = snap.get_int("oiio:colorpolicy:write:force_interop_id", 0) != 0;
    p.verbose      = snap.get_int("oiio:colorpolicy:write:verbose", 0) != 0;
    p.canonicalize = snap.get_int("oiio:colorpolicy:write:canonicalize", 0)
                     != 0;
    p.broadcast = snap.get_int("oiio:colorpolicy:write:broadcast", 0) != 0;
    return p;
}


ColorMetadataPlan
plan_color_metadata(const ColorConfig* config, const ImageSpec& spec,
                    const ColorWriteCaps& caps, const ColorWritePolicy& policy)
{
    // A null config means "use the process default" -- the config the writers
    // historically derived against.
    const ColorConfig& cfg       = config ? *config
                                          : ColorConfig::default_colorconfig();
    const std::string colorspace = spec.get_string_attribute("oiio:ColorSpace");

    ColorMetadataPlan plan;

    // One full derivation cascade (declared id, registry fingerprint match,
    // legacy table, config-local id) feeds both derived signals below,
    // consumed through the shared characterization engine's DERIVE tier --
    // the same cached records the private search walk publishes, so a space is characterized once, not per
    // consumer. Write planning is the intentional home of the expensive
    // derivation, so this requests the interop-id field's full cascade
    // (never the cheap subset, which a mislabeled config's syntactic table
    // match could fool). No other field is requested: every other plan
    // signal consumes only authored metadata today.
    std::string derived_id
        = characterize_color_space(cfg, colorspace,
                                   CharacterizationField::ColorInteropID)
              .color_interop_id;

    // Feature B (spec 09): the config's oiio:default write-canonical mapping is
    // NOT applied here as a tag-only relabel. Relabeling P3/straight-2.6 pixels
    // with the id of the XYZ/DCI-headroom form (a colorimetric change) would be
    // a factual mislabel. The mapping is now a REAL pixel conversion applied at
    // the buffer-holding write stage (see apply_write_canonical_conversion,
    // which converts the pixels through the embedded interop registry and
    // retags), so by the time planning runs the space is already the canonical
    // one and derived_id needs no remap. broadcast (layer 3 > 2) still routes P3
    // content into its own container below.

    // interop id: author's colorInteropID verbatim, else name -> interop id.
    // Feature 1 (spec 09): force_interop_id makes a slotless format capable of
    // carrying the id (emitted as an aux attribute), so a set policy flips the
    // capability gate on for the interop-id signal specifically.
    const bool interop_capable = caps.interop_id || policy.force_interop_id;
    plan.interop_id
        = plan_string_signal(policy.interop_id, interop_capable,
                             spec.get_string_attribute("colorInteropID"),
                             derived_id);

    // CICP: author's CICP int[4] verbatim, else name -> interop id -> CICP
    // tuple (get_cicp on the derived id is a cheap table lookup).
    if (caps.cicp && policy.cicp != ColorSignalPolicy::Never) {
        int explicit_cicp[4];
        if (spec.getattribute("CICP", TypeDesc(TypeDesc::INT, 4),
                              explicit_cicp)) {
            plan.cicp.action = ColorPlanAction::Write;
            plan.cicp.ints.assign(explicit_cicp, explicit_cicp + 4);
        } else if (policy.broadcast && is_p3d65_content(derived_id)) {
            // Feature A (spec 09): P3 -> broadcast container. Rec.2020 encoding
            // primaries are SIGNALED (CICP primaries code 9) and the range is
            // narrow (limited, video_full_range_flag = 0); the transfer is
            // carried from the source's own CICP, and the matrix is RGB (code 0,
            // matching PNG's RGB constraint). The P3 gamut is NOT re-gamut'd to
            // Rec.2020 -- its true volume is carried in the MDCV
            // mastering-display metadata below.
            // No-CICP transfer fallback is BT.1886 (code 1), the SDR broadcast
            // EOTF -- NOT DCDM gamma-2.6 (code 17), whose 48/52.37 DCI headroom
            // a broadcast decoder must not apply to non-cinema content.
            cspan<int> src     = cfg.get_cicp(derived_id);
            const int transfer = src.size() == 4 ? src[1] : 1;
            plan.cicp.action   = ColorPlanAction::Derive;
            plan.cicp.ints     = { 9, transfer, 0, 0 };
        } else {
            cspan<int> derived = derived_id.empty() ? cspan<int>()
                                                    : cfg.get_cicp(derived_id);
            if (derived.size() == 4) {
                plan.cicp.action = ColorPlanAction::Derive;
                plan.cicp.ints.assign(derived.begin(), derived.end());
            }
        }
    } else if (caps.cicp) {
        plan.cicp.action = ColorPlanAction::Suppress;
    }

    // Chromaticities: author's chromaticities float[8] verbatim. Minimally
    // there is no name -> chromaticities derivation, so an unspecified one omits
    // rather than guessing. Feature 2 (spec 09): under verbose, DERIVE cHRM from
    // the space's reserved gamut (a consistent, table-driven value) so the full
    // redundant set is emitted; verbose also makes the signal emittable for the
    // plan-consuming formats (PNG cHRM, EXR chromaticities), whose static caps
    // understate what they can carry. ponytail: verbose OR-ed into the gate
    // because both plan consumers can carry cHRM; the writer still gates the
    // actual emission on emit().
    const bool chrom_capable = caps.chromaticities || policy.verbose;
    if (chrom_capable && policy.chromaticities != ColorSignalPolicy::Never) {
        float chrm[8];
        if (spec.getattribute("chromaticities", TypeDesc(TypeDesc::FLOAT, 8),
                              chrm)) {
            plan.chromaticities.action = ColorPlanAction::Write;
            plan.chromaticities.floats.assign(chrm, chrm + 8);
        } else if (policy.verbose && !derived_id.empty()) {
            if (auto c = reserved_chromaticities_for_id(derived_id)) {
                plan.chromaticities.action = ColorPlanAction::Derive;
                for (const auto& xy : *c) {  // R,G,B,W (x,y) -> flat float[8]
                    plan.chromaticities.floats.push_back(float(xy[0]));
                    plan.chromaticities.floats.push_back(float(xy[1]));
                }
            }
        }
    } else if (chrom_capable) {
        plan.chromaticities.action = ColorPlanAction::Suppress;
    }

    // B5 (spec 07): once a colorInteropID is going to be emitted, the
    // chromaticities attribute is redundant derivable metadata that drifts and
    // contradicts -- suppress it regardless of whether the author supplied one.
    // The one exception, an ST 2065-4 / ACES container that REQUIRES its AP0
    // chromaticities (B4), is enforced by the EXR writer that owns that
    // container machinery, not here. Feature 2: verbose deliberately KEEPS the
    // redundant chromaticities alongside the id (the whole point of verbose),
    // so the B5 minimization is skipped when verbose is on.
    if (plan.interop_id.emit() && !policy.verbose)
        plan.chromaticities.action = ColorPlanAction::Suppress;

    // Gamma: author's gamma verbatim. Feature 2 (spec 09): under verbose,
    // DERIVE gamma from a pure-power-law transfer token (gamma_from_id); a
    // non-power-law space (sRGB piecewise, log, PQ) yields 0 and stays omitted
    // so the emitted gAMA is never inconsistent with the curve.
    const bool gamma_capable = caps.gamma || policy.verbose;
    if (gamma_capable && policy.gamma != ColorSignalPolicy::Never) {
        if (auto a = spec.find_attribute("oiio:Gamma", TypeFloat)) {
            plan.gamma.action = ColorPlanAction::Write;
            plan.gamma.gamma  = a->get_float();
        } else if (policy.verbose) {
            if (float g = gamma_from_id(derived_id); g > 0.0f) {
                plan.gamma.action = ColorPlanAction::Derive;
                plan.gamma.gamma  = g;
            }
        }
    } else if (gamma_capable) {
        plan.gamma.action = ColorPlanAction::Suppress;
    }

    // ICC: author's ICCProfile blob verbatim; no derivation (omit).
    if (caps.icc && policy.icc != ColorSignalPolicy::Never) {
        if (auto a = spec.find_attribute("ICCProfile")) {
            plan.icc.action        = ColorPlanAction::Write;
            const unsigned char* p = reinterpret_cast<const unsigned char*>(
                a->data());
            plan.icc.ints.assign(p, p + a->type().size());  // raw bytes
        }
    } else if (caps.icc) {
        plan.icc.action = ColorPlanAction::Suppress;
    }

    // mDCV (mastering-display volume). No in-tree derivation in general, so it
    // stays Omit. Feature A (spec 09): under broadcast, DERIVE the true P3(D65)
    // gamut volume (R,G,B,W xy) the Rec.2020-signaled container is actually
    // carrying, and make it emittable -- the plan consumers' static caps do not
    // enable mDCV, so broadcast is OR-ed into the gate, mirroring the verbose
    // cHRM/gAMA gates above.
    // Note the asymmetry with the five signals above: author-supplied mDCV is
    // NOT resolved here as an ExplicitMetadata Write. Authored `mdcv_*`
    // ImageSpec attributes are honored directly in the format writer (see
    // png_pvt.h), so this plan only carries the broadcast-derived volume.
    // Consequence: --colorwriteplan reports mdcv as omit even when the file
    // will carry author mDCV. ponytail: unify by reading `mdcv_*` into
    // plan.mdcv when the reconciler write-shape is settled.
    const bool mdcv_capable = caps.mdcv || policy.broadcast;
    if (mdcv_capable && policy.mdcv != ColorSignalPolicy::Never) {
        if (policy.broadcast && is_p3d65_content(derived_id)) {
            if (auto c = reserved_chromaticities_for_id(derived_id)) {
                plan.mdcv.action = ColorPlanAction::Derive;
                for (const auto& xy : *c) {  // R,G,B,W (x,y) -> flat float[8]
                    plan.mdcv.floats.push_back(float(xy[0]));
                    plan.mdcv.floats.push_back(float(xy[1]));
                }
            }
        }
    } else if (mdcv_capable) {
        plan.mdcv.action = ColorPlanAction::Suppress;
    }

    // Attribute each verdict: format incapability and the author's explicit
    // metadata trump the policy tier; everything else was decided by
    // whichever tier supplied the signal's policy.
    auto decider = [](bool capable, ColorPlanAction action,
                      ColorPlanDecider policy_layer) {
        if (!capable)
            return ColorPlanDecider::FormatIncapable;
        if (action == ColorPlanAction::Write)
            return ColorPlanDecider::ExplicitMetadata;
        return policy_layer;
    };
    plan.cicp.decider = decider(caps.cicp, plan.cicp.action, policy.cicp_layer);
    plan.chromaticities.decider = decider(chrom_capable,
                                          plan.chromaticities.action,
                                          policy.chromaticities_layer);
    plan.gamma.decider          = decider(gamma_capable, plan.gamma.action,
                                          policy.gamma_layer);
    plan.icc.decider = decider(caps.icc, plan.icc.action, policy.icc_layer);
    plan.interop_id.decider = decider(interop_capable, plan.interop_id.action,
                                      policy.interop_id_layer);
    plan.mdcv.decider       = decider(mdcv_capable, plan.mdcv.action,
                                      policy.mdcv_layer);

    // Provenance write rule: drop oiio:SourcePath, keep oiio:SourceFormat.
    plan.suppress_source_path = true;
    plan.keep_source_format   = true;
    return plan;
}


ColorWriteCaps
color_write_caps_for_format(string_view format_name)
{
    ColorWriteCaps caps;
    if (Strutil::iequals(format_name, "png")) {
        caps.cicp = true;
    } else if (Strutil::iequals(format_name, "openexr")
               || Strutil::iequals(format_name, "exr")) {
        caps.interop_id = true;
    }
    // mDCV (SMPTE ST 2086) format applicability, per oicio spec 34 "Format
    // gate" -- png/heif/avif/jxl (+ mp4/mov via the master_display string) are
    // mastering-capable; exr and tiff and jpeg have no native mDCV slot. Only
    // PNG is wired to a file so far (png_pvt.h, via libpng png_set/get_mDCV);
    // under the broadcast policy the plan OR-s mDCV in regardless of caps.mdcv
    // (see plan_color_metadata), so caps.mdcv stays false here until a second
    // format is wired. HEIF/AVIF (libheif) and JXL (libjxl) are follow-ons --
    // their mastering-display APIs need those libraries present at build time.
    return caps;
}


namespace {

    const char* action_name(ColorPlanAction a)
    {
        switch (a) {
        case ColorPlanAction::Write: return "write";
        case ColorPlanAction::Derive: return "derive";
        case ColorPlanAction::Suppress: return "suppress";
        default: return "omit";
        }
    }

    const char* decider_name(ColorPlanDecider d)
    {
        switch (d) {
        case ColorPlanDecider::ConfigDeclared: return "config declared";
        case ColorPlanDecider::GlobalAttribute: return "global attribute";
        case ColorPlanDecider::MatchedRule: return "matched rule";
        case ColorPlanDecider::PerSpecAttribute: return "per-spec attribute";
        case ColorPlanDecider::ExplicitMetadata: return "explicit metadata";
        case ColorPlanDecider::FormatIncapable: return "format incapable";
        default: return "builtin default";
        }
    }

    // Render the one populated value carrier of a field ("-" when the plan says
    // to emit nothing). ICC bytes are summarized, never dumped.
    std::string field_value(const ColorPlanField& f, bool is_icc)
    {
        if (!f.emit())
            return "-";
        if (is_icc)
            return Strutil::fmt::format("<{} bytes>", f.ints.size());
        if (!f.str.empty())
            return f.str;
        if (f.ints.size())
            return Strutil::join(f.ints, "/");
        if (f.floats.size())
            return Strutil::join(f.floats, ",");
        return Strutil::fmt::format("{:g}", f.gamma);
    }

}  // namespace


std::string
render_color_write_plan(const ImageSpec& spec, string_view format_name)
{
    const ColorWriteCaps caps = color_write_caps_for_format(format_name);
    // Preview the write plan under the ambient config's declared write policy
    // (spec 09), the same as a real write would. --colorwriteplan takes a
    // format, not an output path, so layer 5 (matched output-rule) does not
    // apply here; layers 2/3 (config default/profiles) and 4/6 do.
    const ColorMetadataPlan plan = plan_color_metadata(
        nullptr, spec, caps,
        ColorWritePolicy::snapshot(&spec, ambient_color_config()));
    std::string out
        = Strutil::fmt::format("Color write plan for format \"{}\":\n",
                               format_name);
    auto row = [&](const char* signal, const ColorPlanField& f,
                   bool is_icc = false) {
        out += Strutil::fmt::format("  {:<15} {:<9} {:<19} {}\n", signal,
                                    action_name(f.action),
                                    decider_name(f.decider),
                                    field_value(f, is_icc));
    };
    row("cicp", plan.cicp);
    row("chromaticities", plan.chromaticities);
    row("gamma", plan.gamma);
    row("icc", plan.icc, true);
    row("interop_id", plan.interop_id);
    row("mdcv", plan.mdcv);
    return out;
}


bool
apply_write_canonical_conversion(ImageBuf& buf, const ColorConfig* config,
                                 string_view filepath)
{
    // Feature B (spec 09), the reconciler write-shape: when the config's write
    // policy maps this buffer's color space to a canonical target that is a
    // real colorimetric change, CONVERT the pixels rather than merely retagging
    // them. The one locked mapping is the DCDM P3->XYZ headroom conversion
    // (g26_p3d65_display -> g26_xyzd65_display). Runs against the embedded
    // interop registry (interop_registry_processor), then stamps oiio:ColorSpace
    // to the target so the downstream metadata plan tags the pixels truthfully.
    // Returns true iff pixels were converted. No-op (false) when no mapping
    // applies, the space can't be characterized, or the registry lacks the
    // transform -- in every no-op case the buffer keeps its own (truthful) tag.
    const ColorConfig& cfg = config ? *config
                                    : ColorConfig::default_colorconfig();
    const ColorWritePolicy policy
        = ColorWritePolicy::snapshot(&buf.spec(), config, filepath);
    if (!policy.canonicalize || policy.broadcast)
        return false;
    const std::string colorspace = buf.spec().get_string_attribute(
        "oiio:ColorSpace");
    if (colorspace.empty())
        return false;
    const std::string from
        = characterize_color_space(cfg, colorspace,
                                   CharacterizationField::ColorInteropID)
              .color_interop_id;
    const std::string to = canonical_write_id(from);
    if (from.empty() || to == from)
        return false;
    ColorProcessorHandle proc = interop_registry_processor(from, to);
    if (!proc)
        return false;
    // In-place; unpremult=false because these are display-encoded pixels, not
    // premultiplied linear -- the mapping is a pure per-pixel curve/matrix.
    if (!ImageBufAlgo::colorconvert(buf, buf, proc.get(), /*unpremult=*/false))
        return false;
    buf.specmod().attribute("oiio:ColorSpace", to);
    return true;
}


void
apply_forced_interop_id(ImageSpec& spec, string_view format_name,
                        string_view filepath)
{
    const ColorWriteCaps caps = color_write_caps_for_format(format_name);
    if (caps.interop_id)
        return;  // native slot -- the format's own plan path owns the id

    const ColorConfig* config     = ambient_color_config();
    const ColorWritePolicy policy = ColorWritePolicy::snapshot(&spec, config,
                                                               filepath);
    if (!policy.force_interop_id) {
        // Default contract: a slotless format carries no transport identity.
        // Strip any authored/passthrough id so it stays untagged (the id would
        // otherwise leak out through the writer's generic XMP emission).
        spec.erase_attribute("colorInteropID");
        return;
    }
    // Forced: keep an already-authored id; otherwise derive one from the color
    // space and stamp it so the writer's generic emission (XMP) carries it.
    if (!spec.get_string_attribute("colorInteropID").empty())
        return;
    const ColorMetadataPlan plan = plan_color_metadata(config, spec, caps,
                                                       policy);
    if (plan.interop_id.emit() && is_valid_interop_id(plan.interop_id.str))
        spec.attribute("colorInteropID", plan.interop_id.str);
}

}  // namespace pvt

OIIO_NAMESPACE_END
