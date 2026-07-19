// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// One central write-side color-metadata derivation, replacing the per-plugin
// "figure out which color attributes to emit for this color space" code that
// every writer used to hand-roll. A writer now declares which signals its
// format can carry and consumes the computed plan; all derivation
// (name -> interop id, name -> CICP, the never-guess omission rule, the
// provenance suppression rule) lives here.
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

ColorPolicySnapshot::ColorPolicySnapshot(const ImageSpec* hints)
    : m_hints(hints)
    , m_lock(color_policy_mutex())
{
}

std::string
ColorPolicySnapshot::get_string(const char* name) const
{
    if (m_hints) {
        if (auto a = m_hints->find_attribute(name, TypeString))
            return a->get_ustring().string();
    }
    std::string v;
    OIIO::getattribute(name, v);
    return v;
}

int
ColorPolicySnapshot::get_int(const char* name, int dflt) const
{
    if (m_hints) {
        if (auto a = m_hints->find_attribute(name, TypeInt))
            return a->get_int();
    }
    int v = dflt;
    OIIO::getattribute(name, v);
    return v;
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
ColorWritePolicy::snapshot(const ImageSpec* config_hints)
{
    ColorWritePolicy p;
    ColorPolicySnapshot snap(config_hints);

    p.cicp = parse_signal(snap.get_string("oiio:colorpolicy:write:cicp"));
    p.chromaticities
        = parse_signal(snap.get_string("oiio:colorpolicy:write:chromaticities"));
    p.gamma = parse_signal(snap.get_string("oiio:colorpolicy:write:gamma"));
    p.icc   = parse_signal(snap.get_string("oiio:colorpolicy:write:icc"));
    p.interop_id
        = parse_signal(snap.get_string("oiio:colorpolicy:write:interop_id"));
    p.mdcv = parse_signal(snap.get_string("oiio:colorpolicy:write:mdcv"));

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
    // legacy table, config-local id) feeds both derived signals below. This
    // is deliberately derive_color_interop_id(), NOT the cheap
    // get_color_interop_id() lookup: write planning is the intentional home
    // of the expensive derivation.
    const std::string derived_id(derive_color_interop_id(cfg, colorspace));

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

    // Provenance write rule: drop oiio:SourcePath, keep oiio:SourceFormat.
    plan.suppress_source_path = true;
    plan.keep_source_format   = true;
    return plan;
}

}  // namespace pvt

OIIO_NAMESPACE_END
