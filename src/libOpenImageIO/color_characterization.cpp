// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// The field-selective color-space characterization engine
// (pvt::characterize_color_space), its process-global cache, and the public
// surface adapted to it: the opaque immutable ColorSpaceInfo record and
// ColorConfig::get_color_space_info (scalar and batch). See color_pvt.h for
// the engine contract. Split alongside color_search.cpp; see
// color_ocio_pvt.h for the shared internal declarations.

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <OpenImageIO/strutil.h>

#include "color_ocio_pvt.h"


// The engine touches ColorConfig::Impl, which lives in the ABI-versioned
// v3_1 namespace -- so the core below must too (same pattern as
// color_search.cpp). The OIIO_API pvt shims declared by color_pvt.h in the
// library's "current" namespace are defined at the end of this file and
// reach back here with explicit v3_1:: qualification.
OIIO_NAMESPACE_3_1_BEGIN

namespace spvt = OIIO::pvt;

namespace {

using Field = spvt::CharacterizationField;

// ---------------------------------------------------------------------------
// Process-global characterization cache. Keyed on (structural config cache
// id, effective context cache id, canonical space name) with the same
// context-invariant bucket collapse the fingerprint cache uses (see
// color_fingerprint.cpp). Values are whole CharacterizationRecord snapshots;
// publication is a field-wise first-writer-wins merge under the lock, and
// computation never happens while holding it. Content-addressed: a changed
// config or context yields new keys and orphans old entries, so
// ColorConfig::reset() needs no invalidation.
// ---------------------------------------------------------------------------

std::mutex&
char_cache_mutex()
{
    static std::mutex m;
    return m;
}

std::unordered_map<std::string, spvt::CharacterizationRecord>&
char_cache()
{
    static std::unordered_map<std::string, spvt::CharacterizationRecord> cache;
    return cache;
}

// ponytail: clear-on-limit like the fingerprint cache; upgrade to LRU only
// if churny workloads show recompute cost in profiles.
constexpr size_t char_cache_max_entries = 8192;

std::string
char_cache_key(const std::string& cfgId, const std::string& ctxId,
               bool invariant, string_view name)
{
    return invariant ? Strutil::fmt::format("{}|invariant|{}", cfgId, name)
                     : Strutil::fmt::format("{}|{}|{}", ctxId, cfgId, name);
}

// Copy field `f`'s value slot from `src` into `dst` and mirror its tri-state
// bits. The one field-copy primitive both merge directions use.
void
copy_field(spvt::CharacterizationRecord& dst,
           const spvt::CharacterizationRecord& src, Field f)
{
    switch (f) {
    case Field::EqualityID: dst.equality_id = src.equality_id; break;
    case Field::ColorInteropID:
        dst.color_interop_id = src.color_interop_id;
        break;
    case Field::Encoding: dst.encoding = src.encoding; break;
    case Field::ImageState: dst.image_state = src.image_state; break;
    case Field::Range: dst.range = src.range; break;
    case Field::Chromaticities:
        dst.chromaticities    = src.chromaticities;
        dst.chromaticities_xy = src.chromaticities_xy;
        break;
    case Field::TransferFunction:
        dst.transfer_kind      = src.transfer_kind;
        dst.transfer_function  = src.transfer_function;
        dst.transfer_signature = src.transfer_signature;
        dst.transfer_identity  = src.transfer_identity;
        break;
    default: break;
    }
    const uint32_t bit = uint32_t(f);
    dst.computed_mask |= (src.computed_mask & bit);
    dst.available_mask = (dst.available_mask & ~bit)
                         | (src.available_mask & bit);
    dst.derived_mask = (dst.derived_mask & ~bit) | (src.derived_mask & bit);
}

const Field all_fields[] = { Field::EqualityID,     Field::ColorInteropID,
                             Field::Encoding,       Field::ImageState,
                             Field::Range,          Field::Chromaticities,
                             Field::TransferFunction };

// Merge `src` into `dst`, field-wise: a field is copied when `dst` has not
// computed it, when `dst`'s attempt found no usable value and `src`'s did,
// or when `src` holds a behaviorally DERIVED value and `dst`'s is merely
// direct -- the derive tier may correct a cheap verdict (a mislabeled
// config's syntactic table match, outranked by the fingerprint cascade) and
// the correction must survive merging with a later fresh cheap pass.
// Established values of equal provenance are never overwritten
// (first-writer-wins per field).
void
merge_record(spvt::CharacterizationRecord& dst,
             const spvt::CharacterizationRecord& src)
{
    for (Field f : all_fields) {
        if (!src.computed(f))
            continue;
        if (!dst.computed(f) || (!dst.available(f) && src.available(f))
            || (src.derived(f) && !dst.derived(f)))
            copy_field(dst, src, f);
    }
    // Full-attempt bookkeeping accumulates regardless of which side's value
    // won: once a field's full derivation has been attempted anywhere, it is
    // settled and never retried.
    dst.full_attempt_mask |= src.full_attempt_mask;
}

// Set field `f`'s tri-state on `rec`: attempted always; usable and
// derivation provenance as reported.
void
mark(spvt::CharacterizationRecord& rec, Field f, bool is_available,
     bool is_derived = false)
{
    const uint32_t bit = uint32_t(f);
    rec.computed_mask |= bit;
    if (is_available)
        rec.available_mask |= bit;
    if (is_derived)
        rec.derived_mask |= bit;
}

}  // namespace



spvt::CharacterizationRecord
characterize_color_space_impl(const ColorConfig& config,
                              string_view color_space,
                              uint32_t requested_fields,
                              const std::map<std::string, std::string>& context)
{
    spvt::CharacterizationRecord rec;
    auto* impl = pvt::ColorConfigClassificationPeek::impl(config);
    if (!impl || !impl->config_ || disable_ocio || color_space.empty())
        return rec;  // invalid

    // Resolution uses the syntactic (fingerprint-free) subset, which returns
    // empty on a genuine miss -- exactly the validity test the cheap
    // contract needs, and never wakes the fingerprint engine.
    std::string resolved(impl->resolve_syntactic(color_space));
    if (resolved.empty())
        return rec;  // invalid
    OCIO::ConstColorSpaceRcPtr cs;
    try {
        cs = impl->config_->getColorSpace(resolved.c_str());
    } catch (...) {
        cs = nullptr;
    }
    if (!cs)
        return rec;  // invalid
    rec.name = cs->getName() ? cs->getName() : resolved;

    // ---- Cheap direct facts (always computed; no processors, no probes).
    const bool is_data = cs->isData();

    // Image state: the OCIO reference-space kind. A data space's reference
    // kind is arbitrary, so its state is honestly undetermined.
    if (!is_data)
        rec.image_state = cs->getReferenceSpaceType()
                                  == OCIO::REFERENCE_SPACE_DISPLAY
                              ? "display"
                              : "scene";
    mark(rec, Field::ImageState, !rec.image_state.empty());

    // Color Interop ID: the existing cheap declared/table subset.
    rec.color_interop_id = std::string(config.get_color_interop_id(resolved));
    mark(rec, Field::ColorInteropID, !rec.color_interop_id.empty());

    // Encoding: the authored attribute only (the interop-counterpart
    // fallback is a derivation, below).
    if (cs->getEncoding() && cs->getEncoding()[0])
        rec.encoding = cs->getEncoding();
    mark(rec, Field::Encoding, !rec.encoding.empty());

    // Range: supplied only when intrinsic to a registered identity or
    // otherwise explicitly known -- nothing registers one today, so the
    // attempt is a stable negative. Never guessed from a name (the static
    // CICP table's range flag is a fixed encode-time convention, not
    // per-space knowledge).
    mark(rec, Field::Range, false);

    // ---- Cache scope. An unkeyable config skips the shared cache.
    const std::string cfgId = get_config_cache_id(impl->config_);
    OCIO::ConstContextRcPtr ctx;  // null = the config's ambient context
    if (!context.empty())
        ctx = make_context_with_overrides(impl->config_, context);
    const std::string ctxId = ctx ? context_cache_id(ctx)
                                  : impl->currentContextID();

    // ---- Merge previously cached derived facts (a cheap read; checking
    // both buckets avoids waking the classifier on the cheap path -- a
    // space is only ever published under one of them).
    if (!cfgId.empty()) {
        std::lock_guard<std::mutex> lock(char_cache_mutex());
        auto& cache = char_cache();
        for (bool invariant : { true, false }) {
            auto it = cache.find(
                char_cache_key(cfgId, ctxId, invariant, rec.name));
            if (it != cache.end()) {
                merge_record(rec, it->second);
                break;
            }
        }
    }

    // ---- Requested derivations, for fields not already settled by the
    // cache. All OCIO work happens here, outside the cache lock. Each
    // attempt -- successful or not -- is recorded as computed, so the
    // publication below caches negative results and an unprobeable space is
    // not retried on every query.
    bool attempted_derivation = false;
    auto begin_full_attempt   = [&](Field f) {
        rec.full_attempt_mask |= uint32_t(f);
        attempted_derivation = true;
    };

    if ((requested_fields & uint32_t(Field::EqualityID))
        && !rec.full_attempted(Field::EqualityID)) {
        begin_full_attempt(Field::EqualityID);
        std::string id;
        if (!is_data) {
            try {
                id = std::string(impl->deriveRegistryInteropId(rec.name));
            } catch (...) {
            }
        }
        rec.equality_id = id;
        mark(rec, Field::EqualityID, !id.empty(), !id.empty());
    }

    if ((requested_fields & uint32_t(Field::ColorInteropID))
        && !rec.full_attempted(Field::ColorInteropID)) {
        // The full declaration -> equality -> table -> generated-local
        // sequence, run even when the cheap declared/table subset already
        // answered: the equality (fingerprint) tier outranks a syntactic
        // table match, so on a mislabeled config the cascade corrects the
        // cheap verdict. This keeps the derive tier bit-exact with
        // pvt::derive_color_interop_id, whose intentional consumer is the
        // write planner. (On a well-formed config the two agree, and the
        // cascade's declared tier still short-circuits before any
        // fingerprint work.)
        begin_full_attempt(Field::ColorInteropID);
        std::string id;
        try {
            id = std::string(derive_color_interop_id_impl(config, resolved));
        } catch (...) {
        }
        if (!id.empty() && id != rec.color_interop_id) {
            rec.color_interop_id = id;
            mark(rec, Field::ColorInteropID, true, true);
        }
    }

    if ((requested_fields & uint32_t(Field::Encoding))
        && !rec.available(Field::Encoding)
        && !rec.full_attempted(Field::Encoding)) {
        // No authored (or cached) encoding: derive the interop-counterpart
        // fallback.
        begin_full_attempt(Field::Encoding);
        std::string enc;
        try {
            enc = impl->effectiveEncoding(rec.name);
        } catch (...) {
        }
        if (!enc.empty()) {
            rec.encoding = enc;
            mark(rec, Field::Encoding, true, true);
        }
    }

    if ((requested_fields & uint32_t(Field::Chromaticities))
        && !rec.full_attempted(Field::Chromaticities)) {
        begin_full_attempt(Field::Chromaticities);
        std::optional<spvt::Chromaticities> chr;
        try {
            chr = impl->deriveChromaticities(rec.name, ctx);
        } catch (...) {
        }
        if (chr) {
            rec.chromaticities_xy = chr;  // exact doubles, for search's ==
            rec.chromaticities.reserve(8);
            for (const auto& xy : *chr) {
                rec.chromaticities.push_back(float(xy[0]));
                rec.chromaticities.push_back(float(xy[1]));
            }
        }
        mark(rec, Field::Chromaticities, chr.has_value(), chr.has_value());
    }

    if ((requested_fields & uint32_t(Field::TransferFunction))
        && !rec.full_attempted(Field::TransferFunction)) {
        begin_full_attempt(Field::TransferFunction);
        bool identity = false;
        if (!is_data && !ctx) {
            // Conservative identity shortcut, ambient context only: OCIO's
            // isColorSpaceLinear() takes no context, so under a per-call
            // context override the context-threaded signature probe below
            // is the only honest linearity evidence.
            try {
                identity = impl->isColorSpaceLinear(rec.name);
            } catch (...) {
            }
        }
        rec.transfer_identity = identity;
        if (identity) {
            rec.transfer_kind = ColorTransferFunctionKind::Linear;
        } else if (!is_data) {
            std::optional<spvt::TransferFunctionSignature> sig;
            try {
                sig = impl->deriveTransferSignature(rec.name, ctx);
            } catch (...) {
            }
            if (sig) {
                rec.transfer_signature = sig;  // raw evidence, for search
                if (sig->is_linear) {
                    rec.transfer_kind = ColorTransferFunctionKind::Linear;
                } else if (!sig->family.empty()) {
                    rec.transfer_kind     = ColorTransferFunctionKind::Named;
                    rec.transfer_function = sig->family;
                } else {
                    rec.transfer_kind = ColorTransferFunctionKind::Sampled;
                }
            }
        }
        const bool determined = rec.transfer_kind
                                != ColorTransferFunctionKind::Undetermined;
        mark(rec, Field::TransferFunction, determined, determined);
    }

    if ((requested_fields & uint32_t(Field::Range))
        && !rec.full_attempted(Field::Range)) {
        // Range describes pixel state and may be supplied only by a genuine
        // registry/CICP *registration* intrinsic to an identity. No such
        // source exists yet: the static CICP table's range flag is hardwired
        // Full for every row -- a fixed encode-time convention, not
        // per-space knowledge -- so deriving from it would be exactly the
        // guessed-"full" default the contract forbids. The full attempt is
        // therefore a settled negative (cached, never retried) until a real
        // registration source appears.
        begin_full_attempt(Field::Range);
    }

    // ---- Publish derivation attempts (immutable snapshot semantics:
    // field-wise first-writer-wins merge under the lock; callers holding
    // earlier records are unaffected).
    if (attempted_derivation && !cfgId.empty()) {
        // The bucket choice may classify the space (context-invariance);
        // that is derive-path work, never reached by the cheap getter.
        bool invariant = false;
        try {
            invariant = (impl->analysisFlags(rec.name)
                         & CSInfo::is_context_invariant)
                        != 0;
        } catch (...) {
        }
        const std::string key = char_cache_key(cfgId, ctxId, invariant,
                                               rec.name);
        std::lock_guard<std::mutex> lock(char_cache_mutex());
        auto& cache = char_cache();
        if (cache.size() >= char_cache_max_entries)
            cache.clear();
        auto it = cache.find(key);
        if (it == cache.end()) {
            cache.emplace(key, rec);
        } else {
            spvt::CharacterizationRecord merged = it->second;
            merge_record(merged, rec);
            it->second = std::move(merged);
        }
    }

    return rec;
}



size_t
characterization_cache_size_impl()
{
    std::lock_guard<std::mutex> lock(char_cache_mutex());
    return char_cache().size();
}



void
characterization_cache_reset_impl()
{
    std::lock_guard<std::mutex> lock(char_cache_mutex());
    char_cache().clear();
}



// ---------------------------------------------------------------------------
// The public opaque record: a shared immutable CharacterizationRecord.
// Everything out of line; no inline function dereferences the PIMPL.
// ---------------------------------------------------------------------------

class ColorSpaceInfo::Impl {
public:
    spvt::CharacterizationRecord rec;
    explicit Impl(spvt::CharacterizationRecord r)
        : rec(std::move(r))
    {
    }
};

namespace {

// The public field enumerators are declared in the same order as the
// internal CharacterizationField bits, so the bit for public field `f` is
// 1 << int(f).
uint32_t
field_bit(ColorSpaceInfoField f)
{
    return uint32_t(1) << uint32_t(f);
}

// The record a default-constructed (impl-less) ColorSpaceInfo reports.
const spvt::CharacterizationRecord&
empty_record()
{
    static const spvt::CharacterizationRecord empty;
    return empty;
}

}  // namespace


ColorSpaceInfo::ColorSpaceInfo() = default;
ColorSpaceInfo::~ColorSpaceInfo() = default;
ColorSpaceInfo::ColorSpaceInfo(const ColorSpaceInfo&) = default;
ColorSpaceInfo::ColorSpaceInfo(ColorSpaceInfo&&) noexcept = default;
ColorSpaceInfo&
ColorSpaceInfo::operator=(const ColorSpaceInfo&)
    = default;
ColorSpaceInfo&
ColorSpaceInfo::operator=(ColorSpaceInfo&&) noexcept = default;

ColorSpaceInfo::ColorSpaceInfo(std::shared_ptr<const Impl> impl)
    : m_impl(std::move(impl))
{
}

bool
ColorSpaceInfo::valid() const noexcept
{
    const auto& rec = m_impl ? m_impl->rec : empty_record();
    return rec.valid();
}

string_view
ColorSpaceInfo::name() const noexcept
{
    const auto& rec = m_impl ? m_impl->rec : empty_record();
    return rec.name;
}

string_view
ColorSpaceInfo::equality_id() const noexcept
{
    const auto& rec = m_impl ? m_impl->rec : empty_record();
    return rec.equality_id;
}

string_view
ColorSpaceInfo::color_interop_id() const noexcept
{
    const auto& rec = m_impl ? m_impl->rec : empty_record();
    return rec.color_interop_id;
}

string_view
ColorSpaceInfo::encoding() const noexcept
{
    const auto& rec = m_impl ? m_impl->rec : empty_record();
    return rec.encoding;
}

string_view
ColorSpaceInfo::image_state() const noexcept
{
    const auto& rec = m_impl ? m_impl->rec : empty_record();
    return rec.image_state;
}

string_view
ColorSpaceInfo::range() const noexcept
{
    const auto& rec = m_impl ? m_impl->rec : empty_record();
    return rec.range;
}

cspan<float>
ColorSpaceInfo::chromaticities() const noexcept
{
    const auto& rec = m_impl ? m_impl->rec : empty_record();
    return rec.chromaticities;
}

ColorTransferFunctionKind
ColorSpaceInfo::transfer_function_kind() const noexcept
{
    const auto& rec = m_impl ? m_impl->rec : empty_record();
    return rec.transfer_kind;
}

string_view
ColorSpaceInfo::transfer_function() const noexcept
{
    const auto& rec = m_impl ? m_impl->rec : empty_record();
    return rec.transfer_function;
}

bool
ColorSpaceInfo::computed(ColorSpaceInfoField field) const noexcept
{
    const auto& rec = m_impl ? m_impl->rec : empty_record();
    return (rec.computed_mask & field_bit(field)) != 0;
}

bool
ColorSpaceInfo::available(ColorSpaceInfoField field) const noexcept
{
    const auto& rec = m_impl ? m_impl->rec : empty_record();
    return (rec.available_mask & field_bit(field)) != 0;
}

bool
ColorSpaceInfo::derived(ColorSpaceInfoField field) const noexcept
{
    const auto& rec = m_impl ? m_impl->rec : empty_record();
    return (rec.derived_mask & field_bit(field)) != 0;
}



// ---------------------------------------------------------------------------
// Public ColorConfig entry points: the CHEAP getter, scalar and batch. Both
// request no derivation fields from the engine (direct/cached work only)
// and convert engine misses into the class's has_error()/geterror()
// convention. Neither throws.
// ---------------------------------------------------------------------------

ColorSpaceInfo
ColorConfig::get_color_space_info(string_view color_space,
                                  const ColorSpaceInfoOptions& options) const
{
    try {
        spvt::CharacterizationRecord rec = characterize_color_space_impl(
            *this, color_space, uint32_t(spvt::CharacterizationField::None),
            options.context);
        if (!rec.valid()) {
            getImpl()->error(
                "get_color_space_info: unknown color space \"{}\"",
                color_space);
            return {};
        }
        return ColorSpaceInfo(
            std::make_shared<const ColorSpaceInfo::Impl>(std::move(rec)));
    } catch (const std::exception& e) {
        getImpl()->error("get_color_space_info: {}", e.what());
        return {};
    }
}



std::vector<ColorSpaceInfo>
ColorConfig::get_color_space_infos(cspan<std::string> color_spaces,
                                   const ColorSpaceInfoOptions& options) const
{
    try {
        std::vector<ColorSpaceInfo> results;
        results.reserve(color_spaces.size());
        // Every input is validated before any record is returned: one
        // invalid name fails the whole batch with an indexed error. Batch
        // order and duplicates are preserved.
        for (size_t i = 0; i < size_t(color_spaces.size()); ++i) {
            spvt::CharacterizationRecord rec = characterize_color_space_impl(
                *this, color_spaces[i],
                uint32_t(spvt::CharacterizationField::None), options.context);
            if (!rec.valid()) {
                getImpl()->error(
                    "get_color_space_infos[{}]: unknown color space \"{}\"", i,
                    color_spaces[i]);
                return {};
            }
            results.push_back(ColorSpaceInfo(
                std::make_shared<const ColorSpaceInfo::Impl>(std::move(rec))));
        }
        return results;
    } catch (const std::exception& e) {
        getImpl()->error("get_color_space_infos: {}", e.what());
        return {};
    }
}



// ---------------------------------------------------------------------------
// Public ColorConfig entry points: the DERIVE verbs, scalar and batch. Both
// request full derivation of every field from the engine (which publishes
// completed attempts -- successful and negative -- to the shared cache) and
// convert engine misses into the class's has_error()/geterror() convention.
// Neither throws.
// ---------------------------------------------------------------------------

ColorSpaceInfo
ColorConfig::derive_color_space_info(string_view color_space,
                                     const ColorSpaceInfoOptions& options) const
{
    try {
        spvt::CharacterizationRecord rec = characterize_color_space_impl(
            *this, color_space, uint32_t(spvt::CharacterizationField::All),
            options.context);
        if (!rec.valid()) {
            getImpl()->error(
                "derive_color_space_info: unknown color space \"{}\"",
                color_space);
            return {};
        }
        return ColorSpaceInfo(
            std::make_shared<const ColorSpaceInfo::Impl>(std::move(rec)));
    } catch (const std::exception& e) {
        getImpl()->error("derive_color_space_info: {}", e.what());
        return {};
    }
}



std::vector<ColorSpaceInfo>
ColorConfig::derive_color_space_infos(cspan<std::string> color_spaces,
                                      const ColorSpaceInfoOptions& options) const
{
    try {
        // Resolve and validate EVERY requested name before deriving any
        // record, so one bad input costs no processor work. The validation
        // pass is the engine's cheap tier (no derivation requested).
        for (size_t i = 0; i < size_t(color_spaces.size()); ++i) {
            spvt::CharacterizationRecord probe = characterize_color_space_impl(
                *this, color_spaces[i],
                uint32_t(spvt::CharacterizationField::None), options.context);
            if (!probe.valid()) {
                getImpl()->error(
                    "derive_color_space_infos[{}]: unknown color space \"{}\"",
                    i, color_spaces[i]);
                return {};
            }
        }
        std::vector<ColorSpaceInfo> results;
        results.reserve(color_spaces.size());
        // Batch order and duplicates are preserved. Per-field derivation
        // failure is an unavailable field on a valid record, never a failed
        // batch.
        for (const std::string& name : color_spaces) {
            spvt::CharacterizationRecord rec = characterize_color_space_impl(
                *this, name, uint32_t(spvt::CharacterizationField::All),
                options.context);
            results.push_back(ColorSpaceInfo(
                std::make_shared<const ColorSpaceInfo::Impl>(std::move(rec))));
        }
        return results;
    } catch (const std::exception& e) {
        getImpl()->error("derive_color_space_infos: {}", e.what());
        return {};
    }
}

OIIO_NAMESPACE_END




// The pvt shims below are declared (OIIO_API) in the library's "current"
// namespace by color_pvt.h, so they must be defined there too, not inside
// the ABI-versioned v3_1 namespace the engine above lives in.
OIIO_NAMESPACE_BEGIN

namespace pvt {

CharacterizationRecord
characterize_color_space(const ColorConfig& config, string_view color_space,
                         CharacterizationField requested_fields,
                         const std::map<std::string, std::string>& context)
{
    return v3_1::characterize_color_space_impl(config, color_space,
                                               uint32_t(requested_fields),
                                               context);
}


size_t
characterization_cache_size()
{
    return v3_1::characterization_cache_size_impl();
}


void
characterization_cache_reset()
{
    v3_1::characterization_cache_reset_impl();
}

}  // namespace pvt

OIIO_NAMESPACE_END
