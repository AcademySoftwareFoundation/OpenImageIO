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
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>

#include "imageio_pvt.h"
#include "color_pvt.h"

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

ColorPolicySnapshot::ColorPolicySnapshot(const ImageSpec* hints,
                                         const ColorConfig* config)
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

        // Layer 3 (optional, spec 09): active profiles selected via the
        // `oiio:colorpolicy:profile` key -- itself resolved through the
        // layers already established (hint > global > the oiio:default key
        // just read). Comma-separated; later profiles override earlier, and
        // every profile overrides the oiio:default baseline.
        std::string sel;
        if (m_hints)
            if (auto a = m_hints->find_attribute("oiio:colorpolicy:profile",
                                                 TypeString))
                sel = a->get_ustring().string();
        if (sel.empty())
            OIIO::getattribute("oiio:colorpolicy:profile", sel);
        if (sel.empty()) {
            auto it = m_config_keys.find("oiio:colorpolicy:profile");
            if (it != m_config_keys.end())
                sel = it->second;
        }
        for (string_view name : Strutil::splitsv(sel, ",")) {
            const std::string rule = "oiio:" + std::string(Strutil::strip(name));
            for (auto& kv : config_declared_policy_keys(*config, rule))
                m_config_keys[kv.first] = kv.second;  // profile wins over default
        }
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
    int v = dflt;
    if (OIIO::getattribute(name, v))
        return v;
    auto it = m_config_keys.find(name);
    if (it != m_config_keys.end())
        return Strutil::from_string<int>(it->second);
    return dflt;
}


namespace {

ColorSignalPolicy
parse_signal(const std::string& v)
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
ColorPlanField
plan_string_signal(ColorSignalPolicy pol, bool capable,
                   const std::string& explicit_present,
                   const std::string& derived)
{
    ColorPlanField f;
    if (!capable || pol == ColorSignalPolicy::Never) {
        f.action = capable ? ColorPlanAction::Suppress : ColorPlanAction::Omit;
        return f;
    }
    if (!explicit_present.empty()) {
        f.action = ColorPlanAction::Write;
        f.str    = explicit_present;
    } else if (!derived.empty()) {
        f.action = ColorPlanAction::Derive;
        f.str    = derived;
    }
    return f;  // else stays Omit
}

}  // namespace


ColorWritePolicy
ColorWritePolicy::snapshot(const ImageSpec* config_hints,
                          const ColorConfig* config)
{
    ColorWritePolicy p;
    ColorPolicySnapshot snap(config_hints, config);

    p.cicp = parse_signal(
        snap.get_string("oiio:colorpolicy:write:cicp", &p.cicp_layer));
    p.chromaticities
        = parse_signal(snap.get_string("oiio:colorpolicy:write:chromaticities",
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

    p.custom_namespace_for_generated_ids
        = snap.get_string("oiio:colorpolicy:write:custom_namespace_for_generated_ids");
    p.aces_container_allow_lossless_compression
        = snap.get_int("oiio:colorpolicy:write:aces_container_allow_lossless_compression",
                       0)
          != 0;
    p.cicp_custom_gama
        = snap.get_int("oiio:colorpolicy:write:cicp_custom_gama", 0) != 0;
    p.write_narrow_range
        = snap.get_int("oiio:colorpolicy:write:write_narrow_range", 0) != 0;
    p.write_yuv = snap.get_int("oiio:colorpolicy:write:write_yuv", 0) != 0;
    return p;
}


ColorMetadataPlan
plan_color_metadata(const ColorConfig* config, const ImageSpec& spec,
                    const ColorWriteCaps& caps, const ColorWritePolicy& policy)
{
    // A null config means "use the process default" -- the config the writers
    // historically derived against.
    const ColorConfig& cfg = config ? *config
                                     : ColorConfig::default_colorconfig();
    const std::string colorspace = spec.get_string_attribute("oiio:ColorSpace");

    ColorMetadataPlan plan;

    // One full derivation cascade (declared id, registry fingerprint match,
    // legacy table, config-local id) feeds both derived signals below,
    // consumed through the shared characterization engine's DERIVE tier --
    // the same cached records the public derive_color_space_info verbs and
    // the search walk publish, so a space is characterized once, not per
    // consumer. Write planning is the intentional home of the expensive
    // derivation, so this requests the interop-id field's full cascade
    // (never the cheap subset, which a mislabeled config's syntactic table
    // match could fool). No other field is requested: every other plan
    // signal consumes only authored metadata today.
    const std::string derived_id
        = characterize_color_space(cfg, colorspace,
                                   CharacterizationField::ColorInteropID)
              .color_interop_id;

    // interop id: author's colorInteropID verbatim, else name -> interop id.
    plan.interop_id
        = plan_string_signal(policy.interop_id, caps.interop_id,
                             spec.get_string_attribute("colorInteropID"),
                             derived_id);

    // CICP: author's CICP int[4] verbatim, else name -> interop id -> CICP
    // tuple (get_cicp on the derived id is a cheap table lookup).
    if (caps.cicp && policy.cicp != ColorSignalPolicy::Never) {
        int explicit_cicp[4];
        if (spec.getattribute("CICP", TypeDesc(TypeDesc::INT, 4), explicit_cicp)) {
            plan.cicp.action = ColorPlanAction::Write;
            plan.cicp.ints.assign(explicit_cicp, explicit_cicp + 4);
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

    // Chromaticities: author's chromaticities float[8] verbatim. There is no
    // reliable in-tree name -> chromaticities derivation, so an unspecified one
    // omits rather than guessing. A future chromaticity engine can supply
    // derived cHRM for formats that need it without an authored attribute.
    if (caps.chromaticities && policy.chromaticities != ColorSignalPolicy::Never) {
        float chrm[8];
        if (spec.getattribute("chromaticities", TypeDesc(TypeDesc::FLOAT, 8),
                              chrm)) {
            plan.chromaticities.action = ColorPlanAction::Write;
            plan.chromaticities.floats.assign(chrm, chrm + 8);
        }
    } else if (caps.chromaticities) {
        plan.chromaticities.action = ColorPlanAction::Suppress;
    }

    // B5 (spec 07): once a colorInteropID is going to be emitted, the
    // chromaticities attribute is redundant derivable metadata that drifts and
    // contradicts -- suppress it regardless of whether the author supplied one.
    // The one exception, an ST 2065-4 / ACES container that REQUIRES its AP0
    // chromaticities (B4), is enforced by the EXR writer that owns that
    // container machinery, not here.
    if (plan.interop_id.emit())
        plan.chromaticities.action = ColorPlanAction::Suppress;

    // Gamma: author's gamma verbatim; no name -> gamma derivation (omit).
    if (caps.gamma && policy.gamma != ColorSignalPolicy::Never) {
        if (auto a = spec.find_attribute("oiio:Gamma", TypeFloat)) {
            plan.gamma.action = ColorPlanAction::Write;
            plan.gamma.gamma  = a->get_float();
        }
    } else if (caps.gamma) {
        plan.gamma.action = ColorPlanAction::Suppress;
    }

    // ICC: author's ICCProfile blob verbatim; no derivation (omit).
    if (caps.icc && policy.icc != ColorSignalPolicy::Never) {
        if (auto a = spec.find_attribute("ICCProfile")) {
            plan.icc.action = ColorPlanAction::Write;
            const unsigned char* p
                = reinterpret_cast<const unsigned char*>(a->data());
            plan.icc.ints.assign(p, p + a->type().size());  // raw bytes
        }
    } else if (caps.icc) {
        plan.icc.action = ColorPlanAction::Suppress;
    }

    // mDCV (mastering-display volume): opt-in, format-supplied only; no
    // in-tree derivation, so it stays Omit unless a policy suppresses it.
    if (caps.mdcv && policy.mdcv == ColorSignalPolicy::Never)
        plan.mdcv.action = ColorPlanAction::Suppress;

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
    plan.chromaticities.decider = decider(caps.chromaticities,
                                          plan.chromaticities.action,
                                          policy.chromaticities_layer);
    plan.gamma.decider = decider(caps.gamma, plan.gamma.action,
                                 policy.gamma_layer);
    plan.icc.decider = decider(caps.icc, plan.icc.action, policy.icc_layer);
    plan.interop_id.decider = decider(caps.interop_id, plan.interop_id.action,
                                      policy.interop_id_layer);
    plan.mdcv.decider = decider(caps.mdcv, plan.mdcv.action, policy.mdcv_layer);

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
    return caps;
}


namespace {

const char*
action_name(ColorPlanAction a)
{
    switch (a) {
    case ColorPlanAction::Write: return "write";
    case ColorPlanAction::Derive: return "derive";
    case ColorPlanAction::Suppress: return "suppress";
    default: return "omit";
    }
}

const char*
decider_name(ColorPlanDecider d)
{
    switch (d) {
    case ColorPlanDecider::ConfigDeclared: return "config declared";
    case ColorPlanDecider::GlobalAttribute: return "global attribute";
    case ColorPlanDecider::PerSpecAttribute: return "per-spec attribute";
    case ColorPlanDecider::ExplicitMetadata: return "explicit metadata";
    case ColorPlanDecider::FormatIncapable: return "format incapable";
    default: return "builtin default";
    }
}

// Render the one populated value carrier of a field ("-" when the plan says
// to emit nothing). ICC bytes are summarized, never dumped.
std::string
field_value(const ColorPlanField& f, bool is_icc)
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
    const ColorMetadataPlan plan
        = plan_color_metadata(nullptr, spec, caps,
                              ColorWritePolicy::snapshot(&spec));
    std::string out = Strutil::fmt::format(
        "Color write plan for format \"{}\":\n", format_name);
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

}  // namespace pvt

OIIO_NAMESPACE_END
