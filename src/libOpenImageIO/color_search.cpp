// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// Color-space search by characterization (the config-driving walk behind
// pvt::find_color_spaces) and the per-candidate characteristic probes it
// uses. The pure term grammar and axis combination live in
// characterization_search.cpp. Split out of color_ocio.cpp; see
// color_ocio_pvt.h for the shared internal declarations.

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <OpenImageIO/strutil.h>

#include "color_ocio_pvt.h"



// The built-in interop identities config and the interoperability
// assertion/bootstrap machinery below touch ColorConfig::Impl, which lives in
// the ABI-versioned v3_1 namespace -- so they must too. The OIIO_API pvt
// shims that expose them are declared (by color_pvt.h) in the library's
// "current" namespace and are defined further down in a separate
// OIIO_NAMESPACE_BEGIN block; those reach back here with explicit v3_1::
// qualification.
OIIO_NAMESPACE_3_1_BEGIN

//////////////////////////////////////////////////////////////////////////
//
// Color-space search by characterization (pvt::find_color_spaces): resolve a
// set of partial-characterization hints, then walk the config in index order
// keeping every color space whose derivable gamut / transfer / encoding /
// image-state satisfies each hinted axis under the three-valued filter
// (pvt::three_valued_axis). The term grammar and axis combination are pure
// (characterization_search.cpp); everything here needs the live config.

// The pure search primitives and shared types are declared in the library's
// non-versioned pvt namespace (color_pvt.h); alias it so this versioned
// block reaches them without colliding with the local v3_x::pvt (which holds
// the classification peek).
namespace spvt = OIIO::pvt;

namespace {
using namespace OCIO;

// Drop a leading "namespace:" from an interop id ("ocio:lin_ap1_scene" ->
// "lin_ap1_scene"). Unchanged when there is no colon.
std::string
search_strip_namespace(string_view id)
{
    const size_t colon = id.find(':');
    if (colon != string_view::npos)
        id.remove_prefix(colon + 1);
    return std::string(id);
}

// The complete gamut component of an interop id: strip the namespace and a
// trailing _scene/_display, then take everything after the last '_'
// ("lin_awg3_scene" -> "awg3"). Empty when no '_' remains. Only a *complete*
// component matches a chromaticity hint; an arbitrary fragment ("p3") does not.
std::string
gamut_component(string_view interop_id)
{
    std::string base = search_strip_namespace(interop_id);
    for (string_view suffix : { "_scene", "_display" }) {
        if (base.size() > suffix.size() && Strutil::ends_with(base, suffix)) {
            base.resize(base.size() - suffix.size());
            break;
        }
    }
    const size_t sep = base.rfind('_');
    return sep == std::string::npos ? std::string() : base.substr(sep + 1);
}

// The curve-only transfer family of an interop id or crv_ named-transform name
// ("srgb_rec709_scene" -> "srgb", "crv_srgb_tx" -> "srgb"): the family token
// (spvt::family_token strips a crv_ prefix and one state suffix) reduced to the
// leading component. This is what lets a crv_ hint and an interop-identified
// candidate agree on the same transfer vocabulary. Assumes the transfer token
// never contains '_', which holds for all current registry ids.
std::string
tf_curve_family(string_view id)
{
    std::string family = spvt::family_token(search_strip_namespace(id));
    const size_t sep   = family.find('_');
    if (sep != std::string::npos)
        family.resize(sep);
    return family;
}

// Run the fixed neutral-axis probe set through a realized CPU processor (encode
// direction) and reduce it to a behavioral transfer signature. This is the one
// config-driving step the pure tf_signature_from_probes() left to its caller.
std::optional<spvt::TransferFunctionSignature>
probe_signature_over(const ConstCPUProcessorRcPtr& cpu)
{
    if (!cpu)
        return {};
    const cspan<double> axis = spvt::tf_probe_axis();
    std::vector<double> outputs;
    outputs.reserve(axis.size());
    try {
        for (double v : axis) {
            float rgb[3] = { float(v), float(v), float(v) };
            cpu->applyRGB(rgb);
            outputs.push_back((double(rgb[0]) + double(rgb[1]) + double(rgb[2]))
                              / 3.0);
        }
    } catch (...) {
        return {};
    }
    return spvt::tf_signature_from_probes(outputs);
}

// Lowercased keys under which a named transform can be found as a transfer
// hint: its name, the name minus a " - curve" suffix, and its crv_-shaped
// name/aliases plus their curve-family forms.
std::vector<std::string>
named_transform_keys(const ConstNamedTransformRcPtr& nt)
{
    std::vector<std::string> keys;
    if (!nt)
        return keys;
    const auto add = [&](std::string key) {
        key = Strutil::lower(key);
        if (!key.empty()
            && std::find(keys.begin(), keys.end(), key) == keys.end())
            keys.push_back(std::move(key));
    };

    const std::string name    = nt->getName() ? nt->getName() : "";
    const std::string lowered = Strutil::lower(name);
    add(name);
    static constexpr string_view curve_suffix = " - curve";
    if (Strutil::ends_with(lowered, curve_suffix))
        add(lowered.substr(0, lowered.size() - curve_suffix.size()));
    if (Strutil::starts_with(lowered, "crv_"))
        add(spvt::family_token(lowered));

    for (size_t i = 0, e = nt->getNumAliases(); i < e; ++i) {
        const std::string alias = nt->getAlias(i) ? nt->getAlias(i) : "";
        const std::string lowered_alias = Strutil::lower(alias);
        if (Strutil::starts_with(lowered_alias, "crv_")) {
            add(alias);
            add(spvt::family_token(lowered_alias));
        } else if (lowered_alias.size() > 4
                   && Strutil::ends_with(lowered_alias, "_crv")) {
            add(alias);
            add(lowered_alias.substr(0, lowered_alias.size() - 4));
        }
    }
    return keys;
}

ConstNamedTransformRcPtr
find_named_transform(const ConstConfigRcPtr& config, const std::string& query)
{
    if (!config)
        return {};
    const std::string wanted = Strutil::lower(query);
    for (int i = 0, e = config->getNumNamedTransforms(); i < e; ++i) {
        const char* name = config->getNamedTransformNameByIndex(i);
        auto nt          = name ? config->getNamedTransform(name)
                                : ConstNamedTransformRcPtr();
        const auto keys  = named_transform_keys(nt);
        if (std::find(keys.begin(), keys.end(), wanted) != keys.end())
            return nt;
    }
    return {};
}

// Authored-transform inspection for the exhaustive path: every atomic
// transform must pass the shared simple-atomic allowlist, every FileTransform
// must reference one of the exhaustive-eligible container formats, and
// ColorSpaceTransform references recurse (cycle-guarded by `visited`). Also
// reports whether any FileTransform was seen at all -- the exhaustive walk
// only revisits spaces that actually reference files.
struct AuthoredInspection {
    bool allowed            = true;
    bool has_file_transform = false;
};

AuthoredInspection
inspect_authored_transform(const ConstConfigRcPtr& config,
                           const ConstContextRcPtr& context,
                           const ConstTransformRcPtr& transform,
                           std::unordered_set<std::string>& visited)
{
    if (!transform)
        return {};
    switch (transform->getTransformType()) {
    case TRANSFORM_TYPE_GROUP: {
        auto group = DynamicPtrCast<const GroupTransform>(transform);
        if (!group)
            return { false, false };
        AuthoredInspection result;
        for (int i = 0, e = group->getNumTransforms(); i < e; ++i) {
            const auto child
                = inspect_authored_transform(config, context,
                                             group->getTransform(i), visited);
            result.allowed &= child.allowed;
            result.has_file_transform |= child.has_file_transform;
        }
        return result;
    }
    case TRANSFORM_TYPE_FILE: {
        auto file = DynamicPtrCast<const FileTransform>(transform);
        if (!file || !file->getSrc())
            return { false, true };
        const std::string source = context ? context->resolveStringVar(
                                       file->getSrc())
                                           : std::string(file->getSrc());
        const bool allowed       = Strutil::iends_with(source, ".spi1d")
                             || Strutil::iends_with(source, ".spimtx")
                             || Strutil::iends_with(source, ".ctf")
                             || Strutil::iends_with(source, ".clf");
        return { allowed, true };
    }
    case TRANSFORM_TYPE_COLORSPACE: {
        auto cst = DynamicPtrCast<const ColorSpaceTransform>(transform);
        if (!cst)
            return { false, false };
        AuthoredInspection result;
        for (const char* raw : { cst->getSrc(), cst->getDst() }) {
            std::string name = raw ? raw : "";
            if (context)
                name = context->resolveStringVar(name.c_str());
            auto referenced = config->getColorSpace(name.c_str());
            if (!referenced || !visited.insert(name).second)
                continue;
            ConstTransformRcPtr selected = referenced->getTransform(
                COLORSPACE_DIR_FROM_REFERENCE);
            if (!selected)
                selected = referenced->getTransform(
                    COLORSPACE_DIR_TO_REFERENCE);
            const auto child = inspect_authored_transform(config, context,
                                                          selected, visited);
            result.allowed &= child.allowed;
            result.has_file_transform |= child.has_file_transform;
        }
        return result;
    }
    default: return { isSimpleAtomicTransform(transform), false };
    }
}

// Realized-op inspection: after OCIO realizes the transform into a processor,
// every op in the group must pass the shared simple-atomic allowlist (file
// transforms have been realized into LUT ops by then).
bool
inspect_realized_transform(const ConstTransformRcPtr& transform)
{
    if (!transform)
        return true;
    if (transform->getTransformType() == TRANSFORM_TYPE_GROUP) {
        auto group = DynamicPtrCast<const GroupTransform>(transform);
        if (!group)
            return false;
        for (int i = 0, e = group->getNumTransforms(); i < e; ++i)
            if (!inspect_realized_transform(group->getTransform(i)))
                return false;
        return true;
    }
    return isSimpleAtomicTransform(transform);
}

// A resolved hint term on a value axis (encoding / image-state / chromaticity)
// and on the transfer axis. Each pairs a term mode with the resolved value(s)
// the hint denotes.
struct ResolvedStringTerm {
    spvt::SearchTermMode mode = spvt::SearchTermMode::include;
    std::vector<std::string> values;
};
struct ResolvedChromaticityTerm {
    spvt::SearchTermMode mode = spvt::SearchTermMode::include;
    std::vector<spvt::Chromaticities> values;
};
struct ResolvedTransferTerm {
    spvt::SearchTermMode mode = spvt::SearchTermMode::include;
    spvt::TransferHint hint;
};

// Route every per-axis verdict through the one pure three-valued combinator:
// build the parallel (modes, matches) spans, then combine.
template<class Term, class Property, class Matches, class Known>
bool
evaluate_axis(const std::vector<Term>& terms, const Property& property,
              Matches matches, Known known)
{
    std::vector<spvt::SearchTermMode> modes;
    std::vector<unsigned char> hits;
    modes.reserve(terms.size());
    hits.reserve(terms.size());
    for (const Term& term : terms) {
        modes.push_back(term.mode);
        hits.push_back(matches(term, property) ? 1 : 0);
    }
    return spvt::three_valued_axis(modes, hits, known(property));
}

bool
string_axis_accepts(const std::vector<ResolvedStringTerm>& terms,
                    const std::optional<std::string>& property)
{
    return evaluate_axis(
        terms, property,
        [](const ResolvedStringTerm& term,
           const std::optional<std::string>& value) {
            return value
                   && std::find(term.values.begin(), term.values.end(), *value)
                          != term.values.end();
        },
        [](const std::optional<std::string>& value) {
            return value.has_value();
        });
}

// Set-valued variant for axes where a candidate legitimately carries more
// than one value (the encoding axis: authored attribute + interop-identity
// twin). A term matches when any value matches; the property is known when
// the set is non-empty.
bool
string_set_axis_accepts(const std::vector<ResolvedStringTerm>& terms,
                        const std::vector<std::string>& values)
{
    return evaluate_axis(
        terms, values,
        [](const ResolvedStringTerm& term,
           const std::vector<std::string>& vals) {
            for (const std::string& v : vals)
                if (std::find(term.values.begin(), term.values.end(), v)
                    != term.values.end())
                    return true;
            return false;
        },
        [](const std::vector<std::string>& vals) { return !vals.empty(); });
}

bool
chromaticity_axis_accepts(const std::vector<ResolvedChromaticityTerm>& terms,
                          const std::optional<spvt::Chromaticities>& property)
{
    return evaluate_axis(
        terms, property,
        [](const ResolvedChromaticityTerm& term,
           const std::optional<spvt::Chromaticities>& value) {
            // Exact ==; all fuzz was absorbed at derivation by
            // round_chromaticity_coord.
            return value
                   && std::find(term.values.begin(), term.values.end(), *value)
                          != term.values.end();
        },
        [](const std::optional<spvt::Chromaticities>& value) {
            return value.has_value();
        });
}

bool
transfer_axis_accepts(const std::vector<ResolvedTransferTerm>& terms,
                      const spvt::TransferProperty& property)
{
    return evaluate_axis(
        terms, property,
        [](const ResolvedTransferTerm& term,
           const spvt::TransferProperty& value) {
            return spvt::transfer_hint_matches(term.hint, value);
        },
        [](const spvt::TransferProperty& value) { return value.known(); });
}

}  // namespace



std::string
ColorConfig::Impl::effectiveEncoding(string_view name) const
{
    if (!config_ || disable_ocio)
        return {};
    std::string resolved(resolve(name));
    auto cs = config_->getColorSpace(resolved.c_str());
    if (cs && cs->getEncoding() && cs->getEncoding()[0])
        return cs->getEncoding();
    // Fall back to the encoding declared by the interop-identity equivalent
    // (full derivation: the twin may only be discoverable by fingerprint).
    std::string id(derive_color_interop_id_impl(*m_self, resolved));
    if (!id.empty()) {
        if (auto registry = build_interop_identities_config())
            if (auto ics = registry->getColorSpace(id.c_str()))
                if (ics->getEncoding() && ics->getEncoding()[0])
                    return ics->getEncoding();
    }
    return {};
}



std::optional<spvt::Chromaticities>
ColorConfig::Impl::deriveChromaticities(
    string_view name, const OCIO::ConstContextRcPtr& context) const
{
    if (!config_ || disable_ocio || name.empty())
        return {};
    std::string resolved(resolve(name));
    // Reserved/registry table first: a space that resolves to a known interop
    // id uses that id's reserved primaries (single hypothesis, exact ==).
    if (auto reserved = spvt::reserved_chromaticities_for_id(
            std::string(derive_color_interop_id_impl(*m_self, resolved))))
        return reserved;

    // Probe fallback: push pure R/G/B/W through colorspace -> the scene
    // interchange (the config's AP0 anchor) and solve for xy. This is the
    // config-driving step the pure chromaticities_from_ap0_probes() left to
    // its caller. Single hypothesis (D65 whitepoint + Bradford CAT) via the
    // CPU processor; a multi-hypothesis whitepoint/CAT sweep is a documented
    // follow-on for non-D65 / log-curve spaces.
    if (!interopIsInteroperable())
        return {};
    const std::string interchange = interopInterchangeName();
    if (interchange.empty())
        return {};
    auto cs = config_->getColorSpace(resolved.c_str());
    if (!cs || cs->isData())
        return {};
    float rgb[12] = { 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1 };
    try {
        // Probe under the explicit per-call context when one was supplied
        // (context overrides are scoped to the whole query, probes included).
        auto ctx  = context ? context : config_->getCurrentContext();
        auto proc = config_->getProcessor(ctx, resolved.c_str(),
                                          interchange.c_str());
        if (!proc)
            return {};
        auto cpu = proc->getDefaultCPUProcessor();
        if (!cpu)
            return {};
        OCIO::PackedImageDesc desc(rgb, 4, 1, 3);
        cpu->apply(desc);
    } catch (...) {
        return {};
    }
    return spvt::chromaticities_from_ap0_probes(cspan<float>(rgb, 12));
}



std::optional<spvt::TransferFunctionSignature>
ColorConfig::Impl::deriveTransferSignature(
    string_view name, const OCIO::ConstContextRcPtr& context) const
{
    if (!config_ || disable_ocio || name.empty())
        return {};
    std::string resolved(resolve(name));
    auto cs = config_->getColorSpace(resolved.c_str());
    if (!cs || cs->isData())
        return {};

    // Linear probe source: the scene interchange anchor for scene-referred
    // spaces, the display interchange (CIE-XYZ-D65 by contract) for
    // display-referred ones. The signature is the encode direction
    // (linear source -> colorspace).
    std::string source;
    if (cs->getReferenceSpaceType() == OCIO::REFERENCE_SPACE_SCENE) {
        if (interopIsInteroperable())
            source = interopInterchangeName();
    } else if (const char* disp = config_->getCanonicalName(
                   OCIO::ROLE_INTERCHANGE_DISPLAY)) {
        source = disp;
    }
    if (source.empty())
        return {};

    std::optional<spvt::TransferFunctionSignature> sig;
    try {
        // Probe under the explicit per-call context when one was supplied
        // (context overrides are scoped to the whole query, probes included).
        auto ctx  = context ? context : config_->getCurrentContext();
        auto proc = config_->getProcessor(ctx, source.c_str(),
                                          resolved.c_str());
        sig       = probe_signature_over(proc ? proc->getDefaultCPUProcessor()
                                              : OCIO::ConstCPUProcessorRcPtr());
    } catch (...) {
        return {};
    }
    if (!sig)
        return {};
    sig->encoding = effectiveEncoding(resolved);
    sig->family   = tf_curve_family(
        derive_color_interop_id_impl(*m_self, resolved));
    return sig;
}



std::vector<std::string>
ColorConfig::Impl::find_color_spaces(const spvt::FindColorSpacesOptions& options)
{
    if (!options.include_active && !options.include_inactive)
        return {};
    if (!config_ || disable_ocio)
        return {};

    // Context overrides are scoped to this one query -- resolve and probe
    // everything below against a context copy carrying the overrides.
    const OCIO::ConstContextRcPtr ctx
        = make_context_with_overrides(config_, options.context);
    // Learned-complex state is scoped to the exact context this query probes
    // under (see markLearnedComplex).
    const std::string ctx_scope           = context_cache_id(ctx);
    const OCIO::ConstConfigRcPtr registry = build_interop_identities_config();

    // ---------------- Hint resolution (fail-fast, pre-walk) ----------------
    // Per axis: exact local name -> known interop id -> axis-specific
    // fallback. Every unresolvable hint throws std::invalid_argument here,
    // before a single candidate is examined.

    auto local_space = [&](const std::string& raw) {
        return config_->getColorSpace(raw.c_str());
    };
    auto registry_space =
        [&](const std::string& raw) -> OCIO::ConstColorSpaceRcPtr {
        return registry ? registry->getColorSpace(raw.c_str())
                        : OCIO::ConstColorSpaceRcPtr();
    };
    auto reference_state = [](const OCIO::ConstColorSpaceRcPtr& cs) {
        return std::string(cs->getReferenceSpaceType()
                                   == OCIO::REFERENCE_SPACE_DISPLAY
                               ? "display"
                               : "scene");
    };

    // The interop-identity twin's encoding for a derived interop id (empty
    // when there is no id or the id has no registry entry). The id itself
    // comes from the shared characterization engine below, so repeat
    // searches and prior public derives make this cache-only.
    auto twin_encoding_for_id = [&](const std::string& id) -> std::string {
        if (id.empty() || !registry)
            return {};
        auto ics = registry->getColorSpace(id.c_str());
        if (ics && ics->getEncoding() && ics->getEncoding()[0])
            return Strutil::lower(ics->getEncoding());
        return {};
    };

    // Per-candidate characterization routes through the one shared
    // field-selective engine (the same records the public get/derive verbs
    // read and publish): each axis requests only the field bits it needs,
    // partial cached records are merged in, and a prior complete derive
    // makes the whole walk cache-only. Derivation failures surface as
    // unavailable fields, which the three-valued axes treat as "unknown".
    using CField          = spvt::CharacterizationField;
    auto characterization = [&](const std::string& name, CField fields) {
        return characterize_color_space_impl(*m_self, name, uint32_t(fields),
                                             options.context);
    };

    auto resolve_encoding =
        [&](const std::string& raw) -> std::vector<std::string> {
        if (auto local = local_space(raw)) {
            // Hint-by-example reads the space's own effective encoding
            // (authored, else twin-adopted); strict reads authored only.
            std::string encoding
                = options.strict
                      ? (local->getEncoding()
                             ? Strutil::lower(local->getEncoding())
                             : std::string())
                      : Strutil::lower(effectiveEncoding(local->getName()));
            if (encoding.empty())
                throw std::invalid_argument(
                    "encoding hint has no derivable encoding: " + raw);
            return { std::move(encoding) };
        }
        if (auto rcs = registry_space(raw)) {
            std::string encoding = rcs->getEncoding()
                                       ? Strutil::lower(rcs->getEncoding())
                                       : std::string();
            if (encoding.empty())
                throw std::invalid_argument(
                    "encoding hint has no derivable encoding: " + raw);
            return { std::move(encoding) };
        }
        // Literal fallback: must be a known encoding (authored in this config
        // or the identities registry).
        const std::string literal = Strutil::lower(raw);
        std::unordered_set<std::string> known;
        auto gather = [&](const OCIO::ConstConfigRcPtr& cfg) {
            if (!cfg)
                return;
            const int nn
                = cfg->getNumColorSpaces(OCIO::SEARCH_REFERENCE_SPACE_ALL,
                                         OCIO::COLORSPACE_ALL);
            for (int i = 0; i < nn; ++i) {
                const char* nm = cfg->getColorSpaceNameByIndex(
                    OCIO::SEARCH_REFERENCE_SPACE_ALL, OCIO::COLORSPACE_ALL, i);
                auto ccs = nm ? cfg->getColorSpace(nm)
                              : OCIO::ConstColorSpaceRcPtr();
                if (ccs && ccs->getEncoding() && ccs->getEncoding()[0])
                    known.insert(Strutil::lower(ccs->getEncoding()));
            }
        };
        gather(config_);
        gather(registry);
        if (!known.count(literal))
            throw std::invalid_argument("unresolved encoding hint: " + raw);
        return { literal };
    };

    auto resolve_state =
        [&](const std::string& raw) -> std::vector<std::string> {
        if (auto local = local_space(raw))
            return { reference_state(local) };
        if (auto rcs = registry_space(raw))
            return { reference_state(rcs) };
        const std::string literal = Strutil::lower(raw);
        if (literal == "scene" || literal == "display")
            return { literal };
        if (literal == "all")
            return { "scene", "display" };
        throw std::invalid_argument("unresolved image-state hint: " + raw);
    };

    auto resolve_chromaticities =
        [&](const std::string& raw) -> std::vector<spvt::Chromaticities> {
        if (auto local = local_space(raw)) {
            if (auto value = deriveChromaticities(local->getName(), ctx))
                return { *value };
            throw std::invalid_argument(
                "chromaticities hint has no derivable value: " + raw);
        }
        // Registry chromaticities come from the reserved-primaries table
        // (table-only; gamuts absent from the table, e.g. ciexyzd65, are
        // documented as not-yet-resolvable, sharing the probe-port follow-on).
        if (auto rcs = registry_space(raw)) {
            if (auto value = spvt::reserved_chromaticities_for_id(
                    rcs->getName()))
                return { *value };
            throw std::invalid_argument(
                "chromaticities hint has no derivable value: " + raw);
        }
        // Gamut-component fragment: the complete component of some registry id.
        const std::string component = Strutil::lower(raw);
        std::vector<spvt::Chromaticities> values;
        if (registry) {
            const int nn
                = registry->getNumColorSpaces(OCIO::SEARCH_REFERENCE_SPACE_ALL,
                                              OCIO::COLORSPACE_ALL);
            for (int i = 0; i < nn; ++i) {
                const char* nm = registry->getColorSpaceNameByIndex(
                    OCIO::SEARCH_REFERENCE_SPACE_ALL, OCIO::COLORSPACE_ALL, i);
                if (!nm || Strutil::lower(gamut_component(nm)) != component)
                    continue;
                auto value = spvt::reserved_chromaticities_for_id(nm);
                if (value
                    && std::find(values.begin(), values.end(), *value)
                           == values.end())
                    values.push_back(*value);
            }
        }
        if (values.empty())
            throw std::invalid_argument("unresolved chromaticities hint: "
                                        + raw);
        return values;
    };

    auto resolve_string_terms = [&](const std::vector<std::string>& inputs,
                                    const auto& resolver) {
        std::vector<ResolvedStringTerm> terms;
        for (const std::string& input : inputs) {
            auto [mode, value] = spvt::parse_search_term(input);
            if (value.empty())
                continue;
            terms.push_back({ mode, resolver(value) });
        }
        return terms;
    };

    auto resolve_transfer_terms = [&]() {
        std::vector<ResolvedTransferTerm> terms;
        for (const std::string& input : options.transfer_functions) {
            auto [mode, value] = spvt::parse_search_term(input);
            if (value.empty())
                continue;

            ResolvedTransferTerm term;
            term.mode                = mode;
            spvt::TransferHint& hint = term.hint;
            if (auto local = local_space(value)) {
                const std::string name = local->getName();
                try {
                    // NOTE: OCIO's isColorSpaceLinear() takes no context, so
                    // for a context-sensitive space it evaluates under the
                    // config's ambient context. The context-threaded
                    // signature probe below is the authoritative transfer
                    // evidence for such spaces.
                    hint.identity = isColorSpaceLinear(name);
                    hint.family   = tf_curve_family(
                        derive_color_interop_id_impl(*m_self, name));
                } catch (...) {
                }
                if (!hint.identity)
                    if (auto sig = deriveTransferSignature(name, ctx))
                        hint.signatures.push_back(std::move(*sig));
            } else if (auto rcs = registry_space(value)) {
                // Known interop id: the registry space is an identity, so its
                // curve behavior is exactly what the id names.
                const std::string id = rcs->getName();
                const std::string encoding
                    = rcs->getEncoding() ? Strutil::lower(rcs->getEncoding())
                                         : std::string();
                hint.identity = encoding == "scene-linear"
                                || encoding == "display-linear";
                hint.family = tf_curve_family(id);
                if (!hint.identity) {
                    try {
                        const char* role
                            = rcs->getReferenceSpaceType()
                                      == OCIO::REFERENCE_SPACE_DISPLAY
                                  ? OCIO::ROLE_INTERCHANGE_DISPLAY
                                  : OCIO::ROLE_INTERCHANGE_SCENE;
                        auto proc = registry->getProcessor(role, id.c_str());
                        if (auto sig = probe_signature_over(
                                proc ? proc->getDefaultCPUProcessor()
                                     : OCIO::ConstCPUProcessorRcPtr())) {
                            sig->encoding = encoding;
                            hint.signatures.push_back(std::move(*sig));
                        }
                    } catch (...) {
                    }
                }
            } else {
                // Named transforms: local config first, then the registry.
                // The identities registry ships no crv_ named transforms, so
                // the local-crv-must-match-registry-twin family rule finds no
                // twin and assigns no family; such hints match by signature
                // only until the registry grows crv_ entries.
                auto nt            = find_named_transform(config_, value);
                bool from_registry = false;
                OCIO::ConstConfigRcPtr source_config = config_;
                OCIO::ConstContextRcPtr source_ctx   = ctx;
                if (!nt && registry) {
                    nt            = find_named_transform(registry, value);
                    from_registry = static_cast<bool>(nt);
                    source_config = registry;
                    source_ctx    = registry->getCurrentContext();
                }
                if (nt) {
                    const std::string encoding
                        = nt->getEncoding() ? Strutil::lower(nt->getEncoding())
                                            : std::string();
                    hint.identity = encoding == "scene-linear"
                                    || encoding == "display-linear";
                    const std::string transform_name = Strutil::lower(
                        nt->getName() ? nt->getName() : "");
                    std::optional<spvt::TransferFunctionSignature> sig;
                    if (!hint.identity) {
                        try {
                            auto proc = source_config->getProcessor(
                                source_ctx, nt, OCIO::TRANSFORM_DIR_INVERSE);
                            sig = probe_signature_over(
                                proc ? proc->getDefaultCPUProcessor()
                                     : OCIO::ConstCPUProcessorRcPtr());
                            if (sig)
                                sig->encoding = encoding;
                        } catch (...) {
                        }
                        if (sig)
                            hint.signatures.push_back(*sig);
                    }
                    if (!hint.identity && sig
                        && Strutil::starts_with(transform_name, "crv_")) {
                        bool registry_behavior = from_registry;
                        if (!registry_behavior && registry) {
                            if (auto registered
                                = find_named_transform(registry,
                                                       transform_name)) {
                                try {
                                    auto rproc = registry->getProcessor(
                                        registry->getCurrentContext(),
                                        registered,
                                        OCIO::TRANSFORM_DIR_INVERSE);
                                    if (auto rsig = probe_signature_over(
                                            rproc
                                                ? rproc->getDefaultCPUProcessor()
                                                : OCIO::ConstCPUProcessorRcPtr()))
                                        registry_behavior
                                            = spvt::transfer_signatures_match(
                                                *sig, *rsig);
                                } catch (...) {
                                }
                            }
                        }
                        if (registry_behavior)
                            hint.family = tf_curve_family(transform_name);
                    }
                }
            }
            if (!hint.identity && hint.signatures.empty())
                throw std::invalid_argument(
                    "unresolved or unprobeable transfer-function hint: "
                    + value);
            terms.push_back(std::move(term));
        }
        return terms;
    };

    std::vector<ResolvedChromaticityTerm> chromaticity_terms;
    for (const std::string& input : options.chromaticities) {
        auto [mode, value] = spvt::parse_search_term(input);
        if (value.empty())
            continue;
        chromaticity_terms.push_back({ mode, resolve_chromaticities(value) });
    }
    const auto transfer_terms = resolve_transfer_terms();
    const auto encoding_terms = resolve_string_terms(options.encodings,
                                                     resolve_encoding);
    const auto state_terms    = resolve_string_terms(options.image_states,
                                                     resolve_state);

    // ---------------- Candidate eligibility ----------------
    // Default universe: simple, matchable, not data/unique/learned-complex.
    // With exhaustive=true a non-simple, file-backed space MAY be revisited if
    // its authored graph is exhaustive-eligible and realizes cleanly.
    auto candidate_eligible = [&](const std::string& name, int flags) {
        if ((flags & (CSInfo::is_data | CSInfo::is_unique))
            || isLearnedComplex(ctx_scope, name))
            return false;
        if ((flags & CSInfo::is_simple)
            && !(flags & CSInfo::should_skip_matching))
            return true;
        if (!options.exhaustive)
            return false;

        auto ocs = config_->getColorSpace(name.c_str());
        if (!ocs)
            return false;
        OCIO::ConstTransformRcPtr transform = ocs->getTransform(
            OCIO::COLORSPACE_DIR_FROM_REFERENCE);
        OCIO::TransformDirection direction = OCIO::TRANSFORM_DIR_FORWARD;
        if (!transform) {
            transform = ocs->getTransform(OCIO::COLORSPACE_DIR_TO_REFERENCE);
            direction = OCIO::TRANSFORM_DIR_INVERSE;
        }
        try {
            std::unordered_set<std::string> visited { name };
            const auto authored
                = inspect_authored_transform(config_, ctx, transform, visited);
            if (!authored.allowed || !authored.has_file_transform)
                return false;

            // The exhaustive "construction succeeds" gate is a realize-clean +
            // allowlist check -- realize the processor and require every
            // realized op to pass the same simple-atomic allowlist. It
            // deliberately does NOT consult the fingerprint subsystem (which
            // carries no tolerance gate and scans nondeterministically); the
            // allowlist realize-check is sufficient and keeps the exhaustive
            // gate deterministic and tolerance-clean.
            auto proc = config_->getProcessor(ctx, transform, direction);
            if (!proc
                || !inspect_realized_transform(proc->createGroupTransform())) {
                markLearnedComplex(ctx_scope, name);
                return false;
            }
        } catch (...) {
            return false;
        }
        return true;
    };

    // ---------------- Bounded-exhaustive walk ----------------
    struct RankedName {
        int invariant, active, simple;
        std::string name;
    };
    std::vector<RankedName> result;

    const int n = config_->getNumColorSpaces(OCIO::SEARCH_REFERENCE_SPACE_ALL,
                                             OCIO::COLORSPACE_ALL);
    for (int i = 0; i < n; ++i) {
        const char* cname = config_->getColorSpaceNameByIndex(
            OCIO::SEARCH_REFERENCE_SPACE_ALL, OCIO::COLORSPACE_ALL, i);
        if (!cname || !*cname)
            continue;
        const std::string name(cname);

        bool active = true;
        int flags   = 0;
        if (find(name))
            flags = analysisFlags(name, &active);
        else
            flags = compute_analysis_flags(name, active);  // inactive spaces

        if ((active && !options.include_active)
            || (!active && !options.include_inactive))
            continue;
        if (!(flags & CSInfo::is_context_invariant)
            && !options.include_context_sensitive)
            continue;
        if (!candidate_eligible(name, flags))
            continue;

        auto cs = config_->getColorSpace(name.c_str());
        if (!cs)
            continue;

        // Per-candidate characterization comes from the shared engine, one
        // axis at a time in the established evaluation order, so a
        // candidate rejected by a cheap axis is never probed for an
        // expensive one. Engine derivation failures yield an "unknown"
        // property (three-valued), never an abort.
        //
        // Encoding characterizes as up to two values: the authored attribute
        // plus — non-strict — the interop-identity twin's encoding (which is
        // also the adopted value when no attribute is authored). A LUT space
        // tagged g26_p3d65_display with encoding sdr-video matches both
        // "sdr-video" and "sdr-cinema".
        std::vector<std::string> encoding_values;
        if (!encoding_terms.empty()) {
            std::string literal = cs->getEncoding()
                                      ? Strutil::lower(cs->getEncoding())
                                      : std::string();
            if (!literal.empty())
                encoding_values.push_back(literal);
            if (!options.strict) {
                const auto rec = characterization(name, CField::ColorInteropID);
                std::string twin = twin_encoding_for_id(
                    rec.available(CField::ColorInteropID) ? rec.color_interop_id
                                                          : std::string());
                if (!twin.empty() && twin != literal)
                    encoding_values.push_back(std::move(twin));
            }
        }
        if (!string_set_axis_accepts(encoding_terms, encoding_values))
            continue;

        const std::optional<std::string> state { reference_state(cs) };
        if (!string_axis_accepts(state_terms, state))
            continue;

        std::optional<spvt::Chromaticities> chromaticities;
        if (!chromaticity_terms.empty())
            chromaticities = characterization(name, CField::Chromaticities)
                                 .chromaticities_xy;
        if (!chromaticity_axis_accepts(chromaticity_terms, chromaticities))
            continue;

        spvt::TransferProperty transfer;
        if (!transfer_terms.empty()) {
            // NOTE: the engine's conservative identity verdict comes from
            // OCIO's context-free isColorSpaceLinear(), which it consults
            // only under the ambient context; the context-threaded signature
            // it probes is authoritative for context-sensitive spaces. The
            // family key still derives from the interop id even when the
            // signature probe fails.
            const auto rec    = characterization(name,
                                                 CField::TransferFunction
                                                     | CField::ColorInteropID);
            transfer.identity = rec.transfer_identity;
            if (rec.available(CField::ColorInteropID))
                transfer.family = tf_curve_family(rec.color_interop_id);
            if (rec.transfer_signature)
                transfer.signature = rec.transfer_signature;
        }
        if (!transfer_axis_accepts(transfer_terms, transfer))
            continue;

        result.push_back({ (flags & CSInfo::is_context_invariant) ? 0 : 1,
                           active ? 0 : 1, (flags & CSInfo::is_simple) ? 0 : 1,
                           name });
    }

    // ---------------- Deterministic order ----------------
    // Determinism comes from this final sort, never the scan order:
    // (context-invariant, active, simple, name).
    std::sort(result.begin(), result.end(),
              [](const RankedName& l, const RankedName& r) {
                  if (l.invariant != r.invariant)
                      return l.invariant < r.invariant;
                  if (l.active != r.active)
                      return l.active < r.active;
                  if (l.simple != r.simple)
                      return l.simple < r.simple;
                  return l.name < r.name;
              });
    std::vector<std::string> names;
    names.reserve(result.size());
    for (RankedName& entry : result)
        names.push_back(std::move(entry.name));
    return names;
}

OIIO_NAMESPACE_END



// The pvt shims below are declared (OIIO_API) in the library's "current"
// namespace by color_pvt.h, so they must be defined there too, not inside
// the ABI-versioned v3_1 namespace the helpers above live in.
OIIO_NAMESPACE_BEGIN

namespace pvt {


std::vector<std::string>
find_color_spaces(const ColorConfig& config,
                  const FindColorSpacesOptions& options)
{
    auto* impl = v3_1::pvt::ColorConfigClassificationPeek::impl(config);
    return impl ? impl->find_color_spaces(options)
                : std::vector<std::string> {};
}

}  // namespace pvt

OIIO_NAMESPACE_END
