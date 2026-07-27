// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// The color space fingerprint engine: the calibrated probe protocol, the
// per-config probe state, the process-global flyweight fingerprint cache,
// and the registry-equivalence resolution built on them. Split out of
// color_ocio.cpp; see color_ocio_pvt.h for the shared internal declarations.

#include <algorithm>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/unordered_map_concurrent.h>

#include "color_ocio_pvt.h"


OIIO_NAMESPACE_3_1_BEGIN

using namespace OCIO;

//////////////////////////////////////////////////////////////////////////
//
// Color space fingerprints: transform a fixed probe from the reference role
// to a color space and compare the resulting floats to recognize equivalent
// spaces by value. The probe constants are calibrated -- do not retype.

namespace {

// The two directions a color space's transform can be authored in. If only the
// opposite direction is authored, use it inverted; if neither is (the literal
// reference space), an identity matrix stands in.
struct DirectionalTransform {
    ConstTransformRcPtr transform;
    TransformDirection direction = TRANSFORM_DIR_FORWARD;
};

DirectionalTransform
transform_for_direction(const ConstColorSpaceRcPtr& cs,
                        ColorSpaceDirection direction)
{
    if (auto t = cs->getTransform(direction))
        return { t, TRANSFORM_DIR_FORWARD };
    if (auto opp = cs->getTransform(ColorSpaceDirection(1 - int(direction))))
        return { opp, TRANSFORM_DIR_INVERSE };
    return { MatrixTransform::Create(), TRANSFORM_DIR_FORWARD };
}

}  // namespace

// Fingerprint probe protocol.
//
// Six RGBA identity pixels per reference-space kind (24 floats), transformed
// FROM the reference space TO each color space; the results are the identity
// fingerprint:
//   Pixel 0-2: chromatic primaries covering the reference gamut triangle.
//   Pixel 3:   black (0,0,0), 50% alpha.
//   Pixel 4:   dark neutral (~18% grey).
//   Pixel 5:   diffuse white -- (1,1,1) for scene, D65 illuminant for display.
// A linearity quartet (pixels 6-13) is appended so the same probe can later
// derive per-space linearity, but it is excluded from equality matching.
//
// The calibrated probe is authored in the interchange reference primaries
// (ACES AP0 for scene, CIE-XYZ-D65 for display). initialize_probe_values first
// normalizes it into THIS config's reference space (in case the config's
// reference primaries differ) by applying the interchange space's
// to-reference transform, so fingerprints are comparable across configs. This
// slice assumes the config resolves the interchange role.
//
// Optimization is disabled (OPTIMIZATION_NONE) throughout so results are
// byte-for-byte reproducible across builds and platforms.
ProbeValues
initialize_probe_values(const ConstConfigRcPtr& config,
                        const ConstContextRcPtr& context)
{
    // clang-format off
    std::vector<float> acesVals = {
        0.408933127871f,
        0.106169822808f,
        0.027842572707f,
        0.0f,
        0.374615373650f,
        0.739417755017f,
        0.118862613721f,
        0.0f,
        0.171696591718f,
        0.104272268468f,
        0.786227391453f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        0.5f,
        0.037018876439f,
        0.030827687576f,
        0.021641700645f,
        0.0f,
        1.0f,
        1.0f,
        1.0f,
        1.0f,
        // Linearity quartet: four (dark, bright) pairs at 0.0625 / 4.0 --
        // neutral, R, G, B -- mirroring OCIO's isColorSpaceLinear probe.
        0.0625f,
        0.0625f,
        0.0625f,
        0.0f,
        4.0f,
        4.0f,
        4.0f,
        0.0f,
        0.0625f,
        0.0f,
        0.0f,
        0.0f,
        4.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0625f,
        0.0f,
        0.0f,
        0.0f,
        4.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0625f,
        0.0f,
        0.0f,
        0.0f,
        4.0f,
        0.0f,
    };
    std::vector<float> xyzVals = {
        0.383684057405f,
        0.213552088801f,
        0.030478901760f,
        0.0f,
        0.350178969169f,
        0.657853997550f,
        0.127445793983f,
        0.0f,
        0.173708304342f,
        0.081402847459f,
        0.858056140808f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        0.5f,
        0.034956685913f,
        0.033530856964f,
        0.023553027375f,
        0.0f,
        0.950455927052f,
        1.0f,
        1.089057750760f,
        1.0f,
        // Linearity quartet -- display-linear XYZ units.
        0.0625f,
        0.0625f,
        0.0625f,
        0.0f,
        4.0f,
        4.0f,
        4.0f,
        0.0f,
        0.0625f,
        0.0f,
        0.0f,
        0.0f,
        4.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0625f,
        0.0f,
        0.0f,
        0.0f,
        4.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0625f,
        0.0f,
        0.0f,
        0.0f,
        4.0f,
        0.0f,
    };
    // clang-format on
    OIIO_DASSERT(acesVals.size() == size_t(kFingerprintProbePixels) * 4
                 && xyzVals.size() == size_t(kFingerprintProbePixels) * 4);

    // Normalize the scene probe into this config's scene reference space.
    try {
        auto cs = config->getColorSpace(ROLE_INTERCHANGE_SCENE);
        if (!cs)
            cs = config->getColorSpace("ACES2065-1");
        if (!cs)
            cs = config->getColorSpace("lin_ap0_scene");
        if (cs) {
            auto toRef = transform_for_direction(cs,
                                                 COLORSPACE_DIR_TO_REFERENCE);
            auto proc = config->getProcessor(context, toRef.transform,
                                             toRef.direction);
            if (!proc->isNoOp()) {
                auto cpu = proc->getOptimizedCPUProcessor(OPTIMIZATION_NONE);
                PackedImageDesc img(acesVals.data(), long(acesVals.size() / 4),
                                    1, 4);
                cpu->apply(img);
            }
        }
    } catch (...) {
    }

    // Same for the display probe (CIE-XYZ-D65 reference), if the config has
    // any display-referred spaces at all.
    try {
        if (config->getNumColorSpaces(SEARCH_REFERENCE_SPACE_DISPLAY,
                                      COLORSPACE_ALL)
            > 0) {
            auto cs = config->getColorSpace(ROLE_INTERCHANGE_DISPLAY);
            if (!cs)
                cs = config->getColorSpace("CIE-XYZ-D65");
            if (!cs)
                cs = config->getColorSpace("lin_ciexyzd65_display");
            if (cs) {
                auto toRef
                    = transform_for_direction(cs, COLORSPACE_DIR_TO_REFERENCE);
                auto proc = config->getProcessor(context, toRef.transform,
                                                 toRef.direction);
                if (!proc->isNoOp()) {
                    auto cpu = proc->getOptimizedCPUProcessor(
                        OPTIMIZATION_NONE);
                    PackedImageDesc img(xyzVals.data(),
                                        long(xyzVals.size() / 4), 1, 4);
                    cpu->apply(img);
                }
            }
        }
    } catch (...) {
    }

    return { std::move(acesVals), std::move(xyzVals) };
}

// Transform the (reference-space) probe by the color space's from-reference
// transform; the resulting floats are its fingerprint. The space's reference
// kind (scene vs display) selects which probe set is used.
std::optional<OIIO::pvt::ColorSpaceFingerprint>
compute_fingerprint(const ConstConfigRcPtr& config,
                    const ConstColorSpaceRcPtr& cs,
                    const ConstContextRcPtr& context, const ProbeValues& probes)
{
    if (!cs)
        return std::nullopt;
    try {
        auto fromRef = transform_for_direction(cs,
                                               COLORSPACE_DIR_FROM_REFERENCE);
        auto proc = config->getProcessor(context, fromRef.transform,
                                         fromRef.direction);
        auto cpu = proc->getOptimizedCPUProcessor(OPTIMIZATION_NONE);
        const int kind = int(cs->getReferenceSpaceType());
        std::vector<float> values = kind == int(REFERENCE_SPACE_DISPLAY)
                                        ? probes.display
                                        : probes.scene;
        PackedImageDesc img(values.data(), long(values.size() / 4), 1, 4);
        cpu->apply(img);
        return OIIO::pvt::ColorSpaceFingerprint{ kind, std::move(values) };
    } catch (...) {
        return std::nullopt;
    }
}

// Exact, tolerance-gated identity match. Reference kinds must match (a scene
// and a display space never compare equal), vector lengths must match, and
// every identity-probe float must agree within kFingerprintAbsTolerance. The
// first structural mismatch or first out-of-tolerance float returns false. The
// trailing linearity quartet (pixels 6-13) is excluded: its 4.0 inputs clamp
// differently through LUT-backed curves than through analytic ones (e.g. an
// ICC TRC table vs ExponentWithLinear), which would turn equivalent spaces
// into false mismatches. There is deliberately no best/closest scoring.
bool
fingerprints_match(const OIIO::pvt::ColorSpaceFingerprint& left,
                   const OIIO::pvt::ColorSpaceFingerprint& right)
{
    if (left.reference_kind != right.reference_kind)
        return false;
    if (left.values.size() != right.values.size())
        return false;
    const size_t bound = std::min(right.values.size(),
                                  size_t(kFingerprintBasePixels) * 4);
    for (size_t i = 0; i < bound; ++i)
        if (std::abs(left.values[i] - right.values[i])
            > kFingerprintAbsTolerance)
            return false;
    return true;
}


void
ColorConfig::Impl::ensureProbeConfig() const
{
    {
        spin_rw_read_lock lock(m_mutex);
        if (m_probe_ready)
            return;
    }

    // Probe through the interopified copy: it is already a PROCESSOR_CACHE_OFF
    // editable copy of config_ repaired to resolve the scene (and, where
    // possible, display) interchange, so fingerprinting works even for configs
    // that don't natively carry the interchange role -- and there is no second
    // editable copy to build. ensure_interop() builds it lazily and leaves
    // config_ untouched. Normalize the probes OUTSIDE the lock (OCIO takes its
    // own locks); publish under the write lock, letting a racing builder's work
    // be discarded (flyweight: duplicate work allowed, blocking never).
    ensure_interop();
    OCIO::ConstConfigRcPtr probe_config;
    OCIO::ConstContextRcPtr probe_context;
    ProbeValues probe_values;
    {
        spin_rw_read_lock lock(m_mutex);
        probe_config = m_interop.interopified;
    }
    // Fail-don't-guess: the probe constants are AP0-calibrated, so they are
    // only meaningful when the (repaired) copy POSITIVELY resolves the scene
    // interchange to normalize against (see interopify_config). Without it,
    // leave the probe config unset so fingerprints report "not computable"
    // instead of silently assuming the config's reference is AP0.
    if (probe_config && !interopifiedResolvesSceneInterchange())
        probe_config.reset();
    if (probe_config) {
        try {
            // Probe under THIS instance's current context, never the memoized
            // interopified copy's: that copy is shared process-wide across
            // all instances of the same structural config (first-writer-wins),
            // so its captured context belongs to whichever instance built it
            // first. The fingerprint cache keys entries by this instance's
            // context id (fingerprint_cache_scope); the probe must run under
            // exactly that context.
            probe_context = config_ ? config_->getCurrentContext()
                                    : probe_config->getCurrentContext();
            probe_values = initialize_probe_values(probe_config, probe_context);
        } catch (...) {
            probe_config.reset();
        }
    }

    spin_rw_write_lock lock(m_mutex);
    if (!m_probe_ready) {
        m_probe_config  = std::move(probe_config);
        m_probe_context = std::move(probe_context);
        m_probe_values  = std::move(probe_values);
        m_probe_ready   = true;
    }
}


std::optional<OIIO::pvt::ColorSpaceFingerprint>
ColorConfig::Impl::computeFingerprint(string_view name) const
{
    ensureProbeConfig();

    OCIO::ConstConfigRcPtr config;
    OCIO::ConstContextRcPtr context;
    ProbeValues probes;
    {
        spin_rw_read_lock lock(m_mutex);
        config  = m_probe_config;
        context = m_probe_context;
        probes  = m_probe_values;
    }
    if (!config)
        return std::nullopt;
    auto cs = config->getColorSpace(std::string(name).c_str());
    return compute_fingerprint(config, cs, context, probes);
}


std::vector<std::pair<std::string, OIIO::pvt::ColorSpaceFingerprint>>
ColorConfig::Impl::fingerprintSimpleColorSpaces() const
{
    std::vector<std::pair<std::string, OIIO::pvt::ColorSpaceFingerprint>> out;

    // Iterate the classification's sorted simple-space cache (already sorted),
    // so the fingerprinted order is deterministic. Gather it before touching
    // the probe state's lock (getSimpleColorSpaces() takes m_mutex itself).
    const std::vector<std::string>& simple = getSimpleColorSpaces();
    ensureProbeConfig();

    OCIO::ConstConfigRcPtr config;
    OCIO::ConstContextRcPtr context;
    ProbeValues probes;
    {
        spin_rw_read_lock lock(m_mutex);
        config  = m_probe_config;
        context = m_probe_context;
        probes  = m_probe_values;
    }
    if (!config)
        return out;

    out.reserve(simple.size());
    for (const auto& name : simple) {
        auto cs = config->getColorSpace(name.c_str());
        auto fp = compute_fingerprint(config, cs, context, probes);
        if (fp)
            out.emplace_back(name, std::move(*fp));
    }
    return out;
}


namespace {

// Process-global flyweight color space fingerprint cache. Keyed on
// (structural config cache id, context cache id, color space name) with a
// context-invariant bucket collapse (see fingerprint_cache_key). Reuses OIIO's
// existing sharded concurrent map -- find_or_insert is exactly the
// first-writer-wins publish this needs, retrieve() is the cheap read-locked
// hit. Content-addressed: a changed config or context simply produces new keys,
// so stale entries orphan harmlessly. Retention is bounded by a hard cap
// (see fingerprint_cache_publish); there is no per-entry invalidation.
// clear() also exists for test/debug reset (see fingerprint_cache_reset).
using FingerprintCache
    = unordered_map_concurrent<std::string, OIIO::pvt::ColorSpaceFingerprint>;

FingerprintCache&
fingerprint_cache()
{
    static FingerprintCache cache;
    return cache;
}

// Publish `fp` under `key` (first-writer-wins) with a hard size bound: the
// cache is content-addressed with no invalidation path, so a long-lived
// process that churns configs or contexts would otherwise accrete orphaned
// entries forever. On hitting the cap the whole cache is dropped and
// repopulated by subsequent queries -- fingerprints are cheap to recompute,
// so a full clear beats LRU bookkeeping here.
// ponytail: clear-on-limit; upgrade to LRU only if churny workloads show
// recompute cost in profiles.
constexpr size_t fingerprint_cache_max_entries = 8192;

OIIO::pvt::ColorSpaceFingerprint
fingerprint_cache_publish(const std::string& key,
                          const OIIO::pvt::ColorSpaceFingerprint& fp)
{
    auto& cache = fingerprint_cache();
    if (cache.size() >= fingerprint_cache_max_entries)
        cache.clear();
    auto result = cache.find_or_insert(key, fp);
    return result.first->second;  // the published value (possibly another
                                  // thread's, on race)
}

// Build the cache key for `name`. A context-invariant space collapses to a
// single per-structural-config bucket ("<cfgId>|invariant|<name>"); every other
// space -- including one the classifier hasn't proven invariant, or doesn't
// know at all -- stays context-scoped ("<ctxId>|<cfgId>|<name>"), so nothing is
// ever shared that wasn't proven stable. The config component is the STRUCTURAL
// cache id (get_config_cache_id), never the context-folded getCacheID(), which
// is what makes the invariant collapse sound: the structural id doesn't change
// when only context vars do.
std::string
fingerprint_cache_key(const std::string& cfgId, const std::string& ctxId,
                      bool invariant, string_view name)
{
    return invariant
               ? Strutil::fmt::format("{}|invariant|{}", cfgId, name)
               : Strutil::fmt::format("{}|{}|{}", ctxId, cfgId, name);
}

// The structural config id + current-context id components of a cache key, read
// from `config`. Empty structural id means the config can't be keyed.
void
fingerprint_cache_scope(const OCIO::ConstConfigRcPtr& config, std::string& cfgId,
                        std::string& ctxId)
{
    cfgId = get_config_cache_id(config);
    ctxId.clear();
    if (!config)
        return;
    try {
        if (auto ctx = config->getCurrentContext())
            if (const char* id = ctx->getCacheID())
                ctxId = id;
    } catch (...) {
    }
}

}  // namespace


void
fingerprint_cache_erase_config(string_view cfgId)
{
    if (cfgId.empty())
        return;
    // Both key forms carry the structural config id as an exact segment:
    // "<cfgId>|invariant|<name>" or "<ctxId>|<cfgId>|<name>" (see
    // fingerprint_cache_key). Collect matching keys under the iteration's
    // per-bin locks, then erase outside the iterator (erase re-takes the
    // bin lock). Racing inserts may repopulate concurrently -- clearing is
    // semantics-free, so that is harmless.
    auto& cache = fingerprint_cache();
    std::vector<std::string> doomed;
    for (auto it = cache.begin(); it != cache.end(); ++it) {
        auto segs = Strutil::splitsv(it->first, "|");
        if (segs.size() >= 2 && (segs[0] == cfgId || segs[1] == cfgId))
            doomed.push_back(it->first);
    }
    for (const auto& key : doomed)
        cache.erase(key);
}


std::optional<OIIO::pvt::ColorSpaceFingerprint>
ColorConfig::Impl::fingerprintCached(string_view name)
{
    // No config, or a config with no structural id, can't be keyed: fall back
    // to a direct (uncached) compute rather than pollute the shared cache.
    std::string cfgId, ctxId;
    fingerprint_cache_scope(config_, cfgId, ctxId);
    if (cfgId.empty())
        return computeFingerprint(name);

    // Classify first so the key builder knows whether this space is context-
    // invariant (a shared bucket) or must stay context-scoped. analyze() is
    // memoized and far cheaper than the fingerprint probe it guards.
    const bool invariant = (analysisFlags(name) & CSInfo::is_context_invariant)
                           != 0;
    const std::string key = fingerprint_cache_key(cfgId, ctxId, invariant, name);
    auto& cache           = fingerprint_cache();

    // Cheap read-locked hit.
    OIIO::pvt::ColorSpaceFingerprint fp;
    if (cache.retrieve(key, fp))
        return fp;

    // Miss: compute OUTSIDE the cache lock (never call OCIO while holding it),
    // then publish first-writer-wins. A racing builder's entry wins and ours is
    // discarded (flyweight: duplicate work allowed, blocking never). Failures
    // are not cached, so a later query retries.
    auto computed = computeFingerprint(name);
    if (!computed)
        return std::nullopt;
    return fingerprint_cache_publish(key, *computed);
}


string_view
ColorConfig::Impl::resolve_registry_equivalence(string_view name)
{
    // Utility tokens (data/unknown/bypass) name a color STATE, not a color;
    // they have no registry fingerprint and must never reach a fingerprint
    // compare. (data/bypass were already offered to resolve_data_utility above;
    // this also covers unknown and any residual data/bypass that found no
    // space.)
    if (OIIO::pvt::is_utility_interop_id(std::string(name)))
        return {};

    // Canonicalize the id through the registry and fetch its fingerprint. A miss
    // here (an id that names no registry space, or a non-fingerprintable one)
    // ends the tier -- resolve() falls through to the input-name passthrough.
    const RegistryFingerprintIndex& index = registry_fingerprint_index();
    std::string canonical_id;
    const OIIO::pvt::ColorSpaceFingerprint* registry_fp
        = registry_fingerprint_for_id(index, name, canonical_id);
    if (!registry_fp)
        return {};

    // Walk this config's simple spaces in the classification's sorted,
    // deterministic order. For each: a cheap explicit-interop-id compare first
    // (OCIO >= 2.5), then a tolerance-gated fingerprint match through the
    // process-global cached path. First match in order wins; the returned view
    // is backed by the persistent simple-space cache. Only names are returned.
    for (const std::string& cs_name : getSimpleColorSpaces()) {
#if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 5, 0)
        if (!canonical_id.empty() && config_ && !disable_ocio) {
            try {
                if (auto qcs = config_->getColorSpace(cs_name.c_str())) {
                    const char* qid = qcs->getInteropID();
                    if (qid && *qid && canonical_id == qid)
                        return cs_name;
                }
            } catch (...) {
            }
        }
#endif
        auto fp = fingerprintCached(cs_name);
        if (fp && fingerprints_match(*fp, *registry_fp))
            return cs_name;
    }
    return {};
}


string_view
ColorConfig::Impl::deriveRegistryInteropId(string_view resolved_name)
{
    if (resolved_name.empty())
        return {};
    // Gate (mirrors the read-side equivalence tier eligibility): a data
    // space is already answered by step 1's utility sub-case; a config-unique
    // space or one flagged skip-matching is never a fingerprint candidate.
    const int flags = analysisFlags(resolved_name);
    if (flags
        & (CSInfo::is_data | CSInfo::is_unique | CSInfo::should_skip_matching))
        return {};
    // Fingerprint the query space through the process-global cached path, then
    // match it against the built-in registry index. The returned id is the
    // registry identity's own (process-global-stable) string, not this query's
    // name. Lazy: this is the first place the write side touches the fingerprint
    // engine or the registry index.
    auto fp = fingerprintCached(resolved_name);
    if (!fp)
        return {};
    return registry_id_for_fingerprint(registry_fingerprint_index(), *fp);
}


std::size_t
ColorConfig::Impl::fingerprintWarm()
{
    std::string cfgId, ctxId;
    fingerprint_cache_scope(config_, cfgId, ctxId);
    if (cfgId.empty())
        return 0;

    // Compute every simple space's fingerprint OUTSIDE the cache lock (the bulk
    // pass already iterates the sorted simple-space set deterministically and
    // skips spaces that don't fingerprint), then publish each into the cache.
    auto fingerprints = fingerprintSimpleColorSpaces();
    std::size_t count = 0;
    for (auto& entry : fingerprints) {
        const bool invariant
            = (analysisFlags(entry.first) & CSInfo::is_context_invariant) != 0;
        const std::string key = fingerprint_cache_key(cfgId, ctxId, invariant,
                                                      entry.first);
        fingerprint_cache_publish(key, entry.second);
        ++count;
    }
    return count;
}

OIIO_NAMESPACE_END




// The pvt shims below are declared (OIIO_API) in the library's "current"
// namespace by color_pvt.h, so they must be defined there too, not inside
// the ABI-versioned v3_1 namespace the helpers above live in.
OIIO_NAMESPACE_BEGIN

namespace pvt {


ColorSpaceFingerprint
color_space_fingerprint(const ColorConfig& config, string_view name)
{
    auto* impl = v3_1::pvt::ColorConfigClassificationPeek::impl(config);
    if (!impl)
        return {};
    auto fp = impl->computeFingerprint(name);
    return fp ? std::move(*fp) : ColorSpaceFingerprint{};
}

bool
color_space_fingerprints_match(const ColorSpaceFingerprint& a,
                               const ColorSpaceFingerprint& b)
{
    return v3_1::fingerprints_match(a, b);
}

std::vector<std::string>
color_space_fingerprint_order(const ColorConfig& config)
{
    std::vector<std::string> names;
    auto* impl = v3_1::pvt::ColorConfigClassificationPeek::impl(config);
    if (!impl)
        return names;
    auto fingerprints = impl->fingerprintSimpleColorSpaces();
    names.reserve(fingerprints.size());
    for (auto& entry : fingerprints)
        names.push_back(std::move(entry.first));
    return names;
}


ColorSpaceFingerprint
color_space_fingerprint_cached(const ColorConfig& config, string_view name)
{
    auto* impl = v3_1::pvt::ColorConfigClassificationPeek::impl(config);
    if (!impl)
        return {};
    auto fp = impl->fingerprintCached(name);
    return fp ? std::move(*fp) : ColorSpaceFingerprint{};
}

std::size_t
color_space_fingerprint_warm(const ColorConfig& config)
{
    auto* impl = v3_1::pvt::ColorConfigClassificationPeek::impl(config);
    return impl ? impl->fingerprintWarm() : 0;
}

std::size_t
color_space_fingerprint_cache_size()
{
    return v3_1::fingerprint_cache().size();
}

void
color_space_fingerprint_cache_reset()
{
    v3_1::fingerprint_cache().clear();
}

}  // namespace pvt

OIIO_NAMESPACE_END
