// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

/// \file
/// Shared internal declarations for the color_*.cpp translation units that
/// together implement ColorConfig and the color-interop machinery
/// (color_ocio.cpp, color_registry.cpp, color_fingerprint.cpp,
/// color_crossconfig.cpp, color_icc_probe.cpp, color_search.cpp).
/// Everything here requires OpenColorIO types and is private to
/// libOpenImageIO -- NOT installed, and not includable by format plugins or
/// unit tests (which are built without OCIO include paths).

#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <tsl/robin_map.h>

#include <OpenImageIO/Imath.h>

#include <OpenImageIO/color.h>
#include <OpenImageIO/simd.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/unordered_map_concurrent.h>

#include "color_pvt.h"
#include "imageio_pvt.h"

#define MAKE_OCIO_VERSION_HEX(maj, min, patch) \
    (((maj) << 24) | ((min) << 16) | (patch))

#include <OpenColorIO/OpenColorIO.h>

namespace OCIO = OCIO_NAMESPACE;


OIIO_NAMESPACE_3_1_BEGIN

#if 1 || !defined(NDEBUG) /* allow color configuration debugging */
extern bool colordebug;  // defined in color_ocio.cpp
#    define DBG(...)    \
        if (colordebug) \
        Strutil::print(__VA_ARGS__)
#else
#    define DBG(...)
#endif

// Runtime kill switches (defined in color_ocio.cpp).
extern int disable_ocio;
extern int disable_builtin_configs;


// Class used as the key to index color processors in the cache.
class ColorProcCacheKey {
public:
    ColorProcCacheKey(ustring in, ustring out, ustring key = ustring(),
                      ustring val = ustring(), ustring looks = ustring(),
                      ustring display = ustring(), ustring view = ustring(),
                      ustring file           = ustring(),
                      ustring namedtransform = ustring(), bool inverse = false)
        : inputColorSpace(in)
        , outputColorSpace(out)
        , context_key(key)
        , context_value(val)
        , looks(looks)
        , file(file)
        , namedtransform(namedtransform)
        , inverse(inverse)
    {
        hash = inputColorSpace.hash() + 14033ul * outputColorSpace.hash()
               + 823ul * context_key.hash() + 28411ul * context_value.hash()
               + 1741ul
                     * (looks.hash() + display.hash() + view.hash()
                        + file.hash() + namedtransform.hash())
               + (inverse ? 6421 : 0);
        // N.B. no separate multipliers for looks, display, view, file,
        // namedtransform, because they're never used for the same lookup.
    }

    friend bool operator<(const ColorProcCacheKey& a,
                          const ColorProcCacheKey& b)
    {
        return std::tie(a.hash, a.inputColorSpace, a.outputColorSpace,
                        a.context_key, a.context_value, a.looks, a.display,
                        a.view, a.file, a.namedtransform, a.inverse)
               < std::tie(b.hash, b.inputColorSpace, b.outputColorSpace,
                          b.context_key, b.context_value, b.looks, b.display,
                          b.view, b.file, b.namedtransform, b.inverse);
    }

    friend bool operator==(const ColorProcCacheKey& a,
                           const ColorProcCacheKey& b)
    {
        return std::tie(a.hash, a.inputColorSpace, a.outputColorSpace,
                        a.context_key, a.context_value, a.looks, a.display,
                        a.view, a.file, a.namedtransform, a.inverse)
               == std::tie(b.hash, b.inputColorSpace, b.outputColorSpace,
                           b.context_key, b.context_value, b.looks, b.display,
                           b.view, b.file, b.namedtransform, b.inverse);
    }
    ustring inputColorSpace;
    ustring outputColorSpace;
    ustring context_key;
    ustring context_value;
    ustring looks;
    ustring display;
    ustring view;
    ustring file;
    ustring namedtransform;
    bool inverse;
    size_t hash;
};


struct ColorProcCacheKeyHasher {
    size_t operator()(const ColorProcCacheKey& c) const { return c.hash; }
};


typedef tsl::robin_map<ColorProcCacheKey, ColorProcessorHandle,
                       ColorProcCacheKeyHasher>
    ColorProcessorMap;


struct CSInfo {
    std::string name;  // Name of this color space
    int index;         // More than one can have the same index -- aliases
    enum Flags {
        none               = 0,
        is_linear_response = 1,   // any cs with linear transfer function
        is_scene_linear    = 2,   // equivalent to scene_linear
        is_srgb            = 4,   // sRGB (primaries, and transfer function)
        is_lin_srgb        = 8,   // sRGB/Rec709 primaries, linear response
        is_ACEScg          = 16,  // ACEScg
        is_Rec709          = 32,  // Rec709 primaries and transfer function
        is_data            = 64,  // Non-color-managed data
        is_known           = is_srgb | is_lin_srgb | is_ACEScg | is_Rec709,
        // Color-space classification bits, computed lazily by Impl::analyze()
        // (a second pass, separate from the classify_* heuristics that set
        // the bits above). Used to decide which spaces are stable candidates
        // for interop matching.
        is_unique             = 128,   // has OCIO category "is-unique"
        should_skip_matching  = 256,   // never a matching candidate
        has_complex_transform = 512,   // rejected by the simple allowlist
        is_simple             = 1024,  // member of the simple set
        is_context_invariant  = 2048,  // no context vars affect this space
    };
    // The lazily-computed classification state is written under the Impl's
    // m_mutex but deliberately READ without it on hot paths (the examine()/
    // analyze() double-checked fast path, equivalent()'s flag compare), so
    // the flag words are atomics: `examined`/`analyzed` publish with release
    // stores paired with acquire loads on the unlocked fast-path checks, and
    // m_flags is or-accumulated. Plain ints/bools here were a C++ data race
    // (UB) under concurrent first-use classification. `active` stays a plain
    // bool: it is only accessed under m_mutex.
    std::atomic<int> m_flags { 0 };
    std::atomic<bool> examined { false };
    std::atomic<bool> analyzed { false };  // analyze() ran (see Impl::analyze)
    bool active = true;     // member of the active colorspace enumeration
    std::string canonical;  // Canonical name for this color space
    OCIO::ConstColorSpaceRcPtr ocio_cs;

    CSInfo(string_view name_, int index_, int flags_ = none,
           string_view canonical_ = "")
        : name(name_)
        , index(index_)
        , m_flags(flags_)
        , canonical(canonical_)
    {
    }

    // Atomics are not copyable; the copy constructor exists only for
    // std::vector growth during single-threaded inventory().
    CSInfo(const CSInfo& other)
        : name(other.name)
        , index(other.index)
        , m_flags(other.m_flags.load(std::memory_order_relaxed))
        , examined(other.examined.load(std::memory_order_relaxed))
        , analyzed(other.analyzed.load(std::memory_order_relaxed))
        , active(other.active)
        , canonical(other.canonical)
        , ocio_cs(other.ocio_cs)
    {
    }

    void setflag(int flagval)
    {
        m_flags.fetch_or(flagval, std::memory_order_relaxed);
    }

    // Set flag to include any bits in flagval, and also if alias is not yet
    // set, set it to name.
    void setflag(int flagval, std::string& alias)
    {
        m_flags.fetch_or(flagval, std::memory_order_relaxed);
        if (alias.empty())
            alias = name;
    }

    int flags() const { return m_flags.load(std::memory_order_relaxed); }
};


// The classification bits observed by tests through pvt::ColorSpaceAnalysis
// (color_pvt.h) are the raw CSInfo classification bits; keep the two in
// sync so a shim result is interpreted correctly.
static_assert(int(OIIO::pvt::ColorSpaceIsData) == CSInfo::is_data
                  && int(OIIO::pvt::ColorSpaceIsUnique) == CSInfo::is_unique
                  && int(OIIO::pvt::ColorSpaceShouldSkipMatching)
                         == CSInfo::should_skip_matching
                  && int(OIIO::pvt::ColorSpaceHasComplexTransform)
                         == CSInfo::has_complex_transform
                  && int(OIIO::pvt::ColorSpaceIsSimple) == CSInfo::is_simple
                  && int(OIIO::pvt::ColorSpaceIsContextInvariant)
                         == CSInfo::is_context_invariant,
              "pvt::ColorSpaceAnalysis must mirror CSInfo classification bits");


// Color space fingerprint probe layout. The identity probe is the first
// kFingerprintBasePixels RGBA pixels (primaries, black, dark neutral, white);
// a trailing linearity quartet -- four (dark, bright) pairs -- rides in the
// same vector (total kFingerprintProbePixels pixels) but is excluded from
// equality matching (see fingerprints_match).
static constexpr int kFingerprintBasePixels  = 6;
static constexpr int kFingerprintProbePixels = 14;  // 6 identity + 8 linearity
// Absolute (not relative) tolerance: scene probes are bounded [0,1] ACES and
// display probes bounded [0,~1.1] XYZ, so one epsilon works everywhere. Chosen
// empirically to separate distinct spaces while tolerating cross-optimization
// float noise. Ceiling: less discriminating for HDR values well above 1.0.
static constexpr float kFingerprintAbsTolerance = 5e-3f;

// The reference-space probe sets after normalization into a config's reference
// space; the raw calibrated values live in initialize_probe_values().
struct ProbeValues {
    std::vector<float> scene;
    std::vector<float> display;
};


// The cache id of an OCIO context ("" when unavailable). Used to scope
// context-dependent failure state (the learned-complex set) by the exact
// context it was observed under, so a failure under one context can never
// poison queries under another.
inline std::string
context_cache_id(const OCIO::ConstContextRcPtr& ctx)
{
    try {
        if (ctx)
            if (const char* id = ctx->getCacheID())
                return id;
    } catch (...) {
    }
    return {};
}


// A context carrying a query's per-call variable overrides, layered on the
// config's current context. Overrides are scoped to the one query. Shared by
// the characterization search and the characterization engine.
inline OCIO::ConstContextRcPtr
make_context_with_overrides(const OCIO::ConstConfigRcPtr& config,
                            const std::map<std::string, std::string>& vars)
{
    if (!config)
        return nullptr;
    OCIO::ConstContextRcPtr context = config->getCurrentContext();
    if (!vars.empty()) {
        OCIO::ContextRcPtr ctx = context->createEditableCopy();
        for (const auto& kv : vars)
            ctx->setStringVar(kv.first.c_str(), kv.second.c_str());
        context = ctx;
    }
    return context;
}


// Hidden implementation of ColorConfig
class ColorConfig::Impl {
public:
    // Frozen after construction: all construction-time fixups (e.g.
    // fix_config_file_rules) happen on a mutable local inside init(), then
    // the result is stored const. Any later modification must go through an
    // explicit createEditableCopy() producing a NEW config (and thus a new
    // cache identity) -- the const type makes the compiler enforce the
    // copy-on-modify contract that the phase-1 cache depends on.
    OCIO::ConstConfigRcPtr config_;
    OCIO::ConstConfigRcPtr builtinconfig_;
    // The config as FIRST constructed (before any evolve() modifications),
    // carried through evolve chains so `EvolveOptions::reset` can always
    // return to the root. Equals config_ for a non-evolved config.
    OCIO::ConstConfigRcPtr original_config_;

private:
    std::vector<CSInfo> colorspaces;
    std::string scene_linear_alias;  // Alias for a scene-linear color space
    std::string lin_srgb_alias;
    std::string srgb_alias;
    std::string ACEScg_alias;
    std::string Rec709_alias;
    mutable spin_rw_mutex m_mutex;
    mutable std::string m_error;
    ColorProcessorMap colorprocmap;  // cache of ColorProcessors
    // Lenient cross-config fallbacks: the pass-through no-op processors
    // reconcile_cross_config{,_display} hand back under non-strict parsing,
    // keyed by processor identity, mapped to the composed continue-message.
    // Guarded by m_mutex; entries live as long as colorprocmap holds the
    // processor (the life of this Impl).
    std::unordered_map<const ColorProcessor*, std::string> m_lenient_fallbacks;
    atomic_int colorprocs_requested;
    atomic_int colorprocs_created;
    std::string m_configname;
    ColorConfig* m_self       = nullptr;
    bool m_config_is_built_in = false;

    // Cache of the "simple" color space names (those that survive the
    // transform allowlist), sorted, computed once on first request under the
    // same double-checked pattern as examine().
    mutable std::vector<std::string> m_simple_color_spaces_cache;
    mutable bool m_simple_color_spaces_cached = false;

    // Color spaces learned to be complex only at query time (e.g. a transform
    // that failed to realize). This is a per-query hint, not a permanent
    // verdict; it is cleared with the Impl (i.e. the config) lifetime, and
    // entries are keyed "<context-cache-id>|<name>" so failure observed
    // under one context never blacklists the space for another.
    mutable std::mutex m_learned_complex_mutex;
    mutable std::unordered_set<std::string> m_learned_complex;

    // The probe config used for fingerprinting: a processor-cache-disabled
    // editable copy of config_, built lazily on the first fingerprint query
    // (constructing a ColorConfig touches none of it). Building probe
    // processors is one-shot -- each probe runs once -- so OCIO's processor
    // cache would only add lock contention and pin every probe processor for
    // the life of the config; disabling it is the documented fast path. The
    // normalized scene/display probe values are derived from this copy.
    mutable OCIO::ConstConfigRcPtr m_probe_config;
    mutable OCIO::ConstContextRcPtr m_probe_context;
    mutable ProbeValues m_probe_values;
    mutable bool m_probe_ready = false;

    // Interoperability assertion + in-memory bootstrap state, computed lazily
    // on the first interop query (see ensure_interop()); constructing a
    // ColorConfig runs none of it. `interopified` is a PROCESSOR_CACHE_OFF
    // editable copy of config_, repaired to resolve a scene (and, where
    // possible, display) interchange -- config_ itself is never mutated.
    struct InteropState {
        bool is_interoperable = false;        // config_ carries a scene interchange
        std::string interchange_colorspace;   // discovered interchange space name
        OCIO::ConstConfigRcPtr interopified;  // repaired probe copy of config_
        bool warned = false;                  // this config emitted the warning
        std::string warning_message;  // the composed once-per-config warning
                                       // (recorded here, NOT on the ColorConfig
                                       // error string -- see ensure_interop()).
    };
    mutable InteropState m_interop;
    mutable bool m_interop_ready = false;

public:
    Impl(ColorConfig* self)
        : m_self(self)
    {
    }

    ~Impl()
    {
#if 0
        // Debugging the cache -- make sure we're creating a small number
        // compared to repeated requests.
        if (colorprocs_requested)
            DBG("ColorConfig::Impl : color procs requested: {}, created: {}\n",
                           colorprocs_requested, colorprocs_created);
#endif
    }

    bool init(string_view filename);

    // Initialize from OCIO config YAML text held in memory (see
    // ColorConfig::from_text). `working_dir`, if non-empty, becomes the
    // config's working directory.
    bool init_from_text(string_view config_text, string_view working_dir);

    // Initialize from an already-built (frozen) OCIO config -- the shared
    // adoption path behind the from-memory factories. `name` becomes the
    // configname() identifier. `original`, if non-null, records the root
    // config an evolve chain resets to (defaults to `config` itself).
    bool init_from_config(OCIO::ConstConfigRcPtr config, string_view name,
                          OCIO::ConstConfigRcPtr original = nullptr);

    // Re-point the back-reference after a ColorConfig move.
    void set_self(ColorConfig* self) { m_self = self; }

    void add(const std::string& name, int index, int flags = 0)
    {
        spin_rw_write_lock lock(m_mutex);
        colorspaces.emplace_back(name, index, flags);
        // classify(colorspaces.back());
    }

    // Find the CSInfo record for the named color space, or nullptr if it's
    // not a color space we know.
    const CSInfo* find(string_view name) const
    {
        for (auto&& cs : colorspaces)
            if (cs.name == name)
                return &cs;
        return nullptr;
    }
    CSInfo* find(string_view name)
    {
        for (auto&& cs : colorspaces)
            if (cs.name == name)
                return &cs;
        return nullptr;
    }

    // Search for a matching ColorProcessor, return it if found (otherwise
    // return an empty handle).
    ColorProcessorHandle findproc(const ColorProcCacheKey& key)
    {
        ++colorprocs_requested;
        spin_rw_read_lock lock(m_mutex);
        auto found = colorprocmap.find(key);
        return (found == colorprocmap.end()) ? ColorProcessorHandle()
                                             : found->second;
    }

    // Add the given color processor. Be careful -- if a matching one is
    // already in the table, just return the existing one. If they pass
    // in an empty handle, just return it. If `lenient_fallback_msg` is
    // non-null, the handle is a lenient cross-config pass-through fallback:
    // record its continue-message against the cached processor (under the
    // same lock as the insertion), so the per-call outcome travels WITH the
    // processor and a later cache hit can re-signal it.
    ColorProcessorHandle addproc(const ColorProcCacheKey& key,
                                 ColorProcessorHandle handle,
                                 const std::string* lenient_fallback_msg
                                 = nullptr)
    {
        if (!handle)
            return handle;
        spin_rw_write_lock lock(m_mutex);
        auto found = colorprocmap.find(key);
        if (found == colorprocmap.end()) {
            // No equivalent item in the map. Add this one.
            colorprocmap[key] = handle;
            ++colorprocs_created;
        } else {
            // There's already an equivalent one. Oops. Discard this one and
            // return the one already in the map.
            handle = found->second;
        }
        if (lenient_fallback_msg)
            m_lenient_fallbacks[handle.get()] = *lenient_fallback_msg;
        return handle;
    }

    // If `proc` is a registered lenient cross-config pass-through fallback
    // (see addproc), return its continue-message; otherwise return empty.
    // This -- never the shared error string, which cache hits and unrelated
    // calls may have cleared or overwritten -- is how callers must decide
    // whether a non-null processor actually converts pixels.
    std::string lenient_fallback_message(const ColorProcessor* proc) const
    {
        spin_rw_read_lock lock(m_mutex);
        auto found = m_lenient_fallbacks.find(proc);
        return found == m_lenient_fallbacks.end() ? std::string()
                                                  : found->second;
    }

    int getNumColorSpaces() const { return (int)colorspaces.size(); }

    const char* getColorSpaceNameByIndex(int index) const
    {
        return colorspaces[index].name.c_str();
    }

    string_view resolve(string_view name) const;

    // The syntactic (fingerprint-free) subset of resolve(): direct OCIO
    // name/role/alias, informal aliases, stripped-namespace retry,
    // config-local form, declared interop_id, and the data/bypass utility
    // ranking -- everything except the registry-equivalence fingerprint
    // tier. Returns empty on a miss (no passthrough), and never wakes the
    // fingerprint engine or builds the registry index.
    string_view resolve_syntactic(string_view name) const;

    // equivalent() restricted to syntactic resolution + the cheap
    // classification flags -- the equivalence the cheap public
    // get_color_interop_id() table tier uses. Never fingerprints.
    bool equivalent_syntactic(string_view color_space1,
                              string_view color_space2) const;

    // Note: Uses std::format syntax
    template<typename... Args>
    void error(const char* fmt, const Args&... args) const
    {
        spin_rw_write_lock lock(m_mutex);
        m_error = Strutil::fmt::format(fmt, args...);
    }
    std::string geterror(bool clear = true) const
    {
        std::string err;
        spin_rw_write_lock lock(m_mutex);
        if (clear) {
            std::swap(err, m_error);
        } else {
            err = m_error;
        }
        return err;
    }
    bool haserror() const
    {
        spin_rw_read_lock lock(m_mutex);
        return !m_error.empty();
    }
    void clear_error()
    {
        spin_rw_write_lock lock(m_mutex);
        m_error.clear();
    }

    const std::string& configname() const { return m_configname; }
    void configname(string_view name) { m_configname = name; }

    OCIO::ConstCPUProcessorRcPtr
    get_to_builtin_cpu_proc(const char* my_from, const char* builtin_to) const;

    bool isColorSpaceLinear(string_view name) const;

    bool isData(string_view name) const;

    // The sorted set of "simple" color space names (those that survive the
    // transform allowlist), computed once and cached.
    const std::vector<std::string>& getSimpleColorSpaces() const;

    // Return the CSInfo classification flags for the named color space,
    // computing them lazily on first request (see analyze()). Returns 0 for
    // unknown names. `active`, if non-null, receives whether the space is in
    // the config's active colorspace enumeration.
    int analysisFlags(string_view name, bool* active = nullptr);

    // Classification flags for `name` computed directly (no CSInfo cache),
    // for color spaces outside the active inventory (e.g. inactive spaces the
    // characterization search examines). `active` receives active-enumeration
    // membership. Mirrors analyze()'s body.
    int compute_analysis_flags(const std::string& name, bool& active) const;

    // The internal characterization search (see pvt::find_color_spaces). Lives
    // on Impl because it needs the config, the classification flags, the
    // interop registry, and the probe producers below.
    std::vector<std::string>
    find_color_spaces(const OIIO::pvt::FindColorSpacesOptions& options);

    // Probe producers for the search axes -- each drives an OCIO processor to
    // derive a candidate's characteristic, then hands the raw samples to the
    // pure pvt:: primitives. The effective (declared, else registry-inferred)
    // OCIO encoding; the chromaticities (reserved table, else AP0-probe
    // derivation); and the behavioral transfer signature (neutral-axis probe
    // run in the encode direction). `context`, when non-null, is the exact
    // context every probe processor is built under (find_color_spaces'
    // per-call override); null means the config's own current context.
    std::string effectiveEncoding(string_view name) const;
    std::optional<OIIO::pvt::Chromaticities>
    deriveChromaticities(string_view name,
                         const OCIO::ConstContextRcPtr& context = {}) const;
    std::optional<OIIO::pvt::TransferFunctionSignature>
    deriveTransferSignature(string_view name,
                            const OCIO::ConstContextRcPtr& context = {}) const;

    // Compute the color space fingerprint for `name` (transform the fixed
    // probe from the reference role to the space). Builds the probe config
    // lazily on first use. Returns nullopt if the space is unknown or can't be
    // probed. Defined below, after the probe helpers it relies on.
    std::optional<OIIO::pvt::ColorSpaceFingerprint>
    computeFingerprint(string_view name) const;

    // Fingerprint every "simple" color space, iterating the classification's
    // sorted simple-space cache so the result order is deterministic.
    std::vector<std::pair<std::string, OIIO::pvt::ColorSpaceFingerprint>>
    fingerprintSimpleColorSpaces() const;

    // Look up (or compute and publish) the fingerprint for `name` in the
    // process-global flyweight fingerprint cache. A hit is a cheap read; a miss
    // computes the fingerprint OUTSIDE any cache lock and publishes it
    // first-writer-wins. Returns nullopt if the space can't be fingerprinted.
    std::optional<OIIO::pvt::ColorSpaceFingerprint>
    fingerprintCached(string_view name);

    // Fingerprint every "simple" color space and publish each into the
    // process-global flyweight cache. Returns how many were fingerprinted (the
    // deterministic bulk "warm" pass; see fingerprintSimpleColorSpaces()).
    std::size_t fingerprintWarm();

    // Step 2 of pvt::derive_color_interop_id(): the write-side analog of
    // resolve_registry_equivalence(). Given a color space name that already
    // resolves to a real space in this config, fingerprint it and return the
    // built-in registry identity it is definitionally equal to -- i.e. the
    // REGISTRY space's own interop id, not the query's name. Empty on no match.
    // Gated to skip data / config-unique / skip-matching spaces (a data space is
    // already answered by step 1). Non-const: it populates the logically-const
    // lazy classification and fingerprint caches, and lazily builds the
    // process-global registry fingerprint index on first use. Called by
    // ColorConfig via getImpl(), so it lives in the public section.
    string_view deriveRegistryInteropId(string_view resolved_name);

    // Interoperability assertion/bootstrap queries (each triggers the lazy
    // bootstrap, except interopComputed(), which must NOT, so callers can
    // verify that constructing a ColorConfig does no interop work).
    bool interopIsInteroperable() const;
    std::string interopInterchangeName() const;
    bool interopComputed() const;  // does not trigger the bootstrap
    bool interopWarned() const;
    bool interopifiedResolvesSceneInterchange() const;
    bool interopifiedCacheOff() const;

    // The interopified (repaired, in-memory) copy of config_, or null. Triggers
    // the lazy interop bootstrap.
    OCIO::ConstConfigRcPtr interopifiedConfig() const;

    // Reconcile a color conversion whose local resolution failed, when a
    // requested name is a registry-known interop identity this config lacks.
    // Cross-config reconciliation is deliberately on by default, observable
    // via debug log, with OCIO strict_parsing as the opt-out. Routes the
    // foreign endpoint through the built-in interop identities config via the
    // cross-config chokepoint. Returned-handle / `errmsg` contract lives at
    // the definition. Never throws.
    ColorProcessorHandle reconcile_cross_config(string_view src, string_view dst,
                                                std::string& errmsg) const;

    // Display-view sibling of reconcile_cross_config: reconcile a display
    // transform whose local resolution failed because the INPUT color space is
    // a registry-known interop identity this config lacks. The display/view are
    // inherently local; only the source can be foreign. Routes the foreign
    // source through the interop identities config into this config's
    // display/view via the display-view chokepoint. Same strict/lenient/
    // narration contract as reconcile_cross_config. Never throws.
    ColorProcessorHandle reconcile_cross_config_display(string_view input,
                                                        string_view display,
                                                        string_view view,
                                                        bool inverse,
                                                        std::string& errmsg) const;

    // Whether analyze() has already run for the named space, WITHOUT
    // triggering it (used to verify lazy behavior). False for unknown names.
    bool analysisComputed(string_view name) const
    {
        const CSInfo* cs = find(name);
        if (!cs)
            return false;
        spin_rw_read_lock lock(m_mutex);
        return cs->analyzed;
    }

    // Record a color space as complex for the life of this config, so later
    // queries skip it. This is a hint, not a permanent verdict, and it is
    // keyed by the context (cache id) the failure was observed under: a
    // realize failure under one context override set must not blacklist the
    // space for other contexts.
    void markLearnedComplex(string_view ctxscope, string_view name) const
    {
        std::lock_guard<std::mutex> lock(m_learned_complex_mutex);
        m_learned_complex.emplace(
            Strutil::fmt::format("{}|{}", ctxscope, name));
    }
    bool isLearnedComplex(string_view ctxscope, string_view name) const
    {
        std::lock_guard<std::mutex> lock(m_learned_complex_mutex);
        return m_learned_complex.count(
                   Strutil::fmt::format("{}|{}", ctxscope, name))
               != 0;
    }

    // The cache id of this config's CURRENT (ambient) context, the scope
    // used when a query supplies no explicit context override.
    std::string currentContextID() const
    {
        try {
            if (config_)
                return context_cache_id(config_->getCurrentContext());
        } catch (...) {
        }
        return {};
    }

private:
    // Return the CSInfo flags for the given color space name
    int flags(string_view name)
    {
        CSInfo* cs = find(name);
        if (!cs)
            return 0;
        examine(cs);
        spin_rw_read_lock lock(m_mutex);
        return cs->flags();
    }

    // Set cs.flag to include any bits in flagval.
    void setflag(CSInfo& cs, int flagval)
    {
        spin_rw_write_lock lock(m_mutex);
        cs.setflag(flagval);
    }

    // Set cs.flag to include any bits in flagval, and also if alias is not
    // yet set, set it to cs.name.
    void setflag(CSInfo& cs, int flagval, std::string& alias)
    {
        spin_rw_write_lock lock(m_mutex);
        cs.setflag(flagval, alias);
    }

    void inventory();

    // Build builtinconfig_ (the fixed-up ocio://default copy). Shared by
    // every init path.
    void init_builtin();

    // The shared tail of every init path: inventory + builtin-equivalent
    // identification + debug dumps. Returns whether config_ is usable.
    bool finish_init();

    // Tier 1a of resolve(): a direct OCIO color space / role / alias lookup,
    // then OIIO's informal universal-name aliases (sRGB, lin_srgb, ACEScg,
    // scene_linear, Rec709). Returns a stable view of the resolved name, or an
    // empty string_view if the name matched none of them (resolve() layers the
    // interop-ID tiers and the input-name passthrough on top of this).
    string_view resolve_name_tier1a(string_view name) const;

    // Tier 2 of resolve(): registry equivalence. Returns this config's OWN
    // simple color space that is definitionally the same color as the built-in
    // interop identity `name` names -- matched by a cheap explicit-interop-id
    // compare, then a tolerance-gated fingerprint match -- or empty on no
    // match. It only ever returns a name; it never builds a cross-config
    // processor. Fingerprints the query config and builds the process-global
    // registry fingerprint index lazily on first use (utility tokens are an
    // automatic miss and never reach a fingerprint compare). Non-const: it
    // populates the logically-const lazy classification and fingerprint caches.
    string_view resolve_registry_equivalence(string_view name);

    // Set the flags for the given color space and canonical name, if we can
    // make a guess based on the name. This is very inexpensive. This should
    // only be called from within a lock of the mutex.
    void classify_by_name(CSInfo& cs);

    // Set the flags for the given color space and canonical name, trying some
    // tricks to deduce the color space from the primaries, white point, and
    // transfer function. This is more expensive, and might only work for OCIO
    // 2.2 and above. This should only be called from within a lock of the
    // mutex.
    void classify_by_conversions(CSInfo& cs);

    // Apply more heuristics to try to deduce more color space information.
    void reclassify_heuristics(CSInfo& cs);

    // If the CSInfo hasn't yet been "examined" (fully classified by all
    // heuristics), do so. This should NOT be called from within a lock of the
    // mutex.
    void examine(CSInfo* cs)
    {
        // Unlocked fast-path check: acquire pairs with the release publish
        // below, making the classification written before it visible.
        if (!cs->examined.load(std::memory_order_acquire)) {
            spin_rw_write_lock lock(m_mutex);
            if (!cs->examined.load(std::memory_order_relaxed)) {
                classify_by_name(*cs);
                classify_by_conversions(*cs);
                reclassify_heuristics(*cs);
                cs->examined.store(true, std::memory_order_release);
            }
        }
    }

    // If the CSInfo's classification bits haven't been computed yet, do so.
    // Same double-checked lazy pattern as examine(), but a separate pass:
    // it needs the simple-space scan, not the classify_* heuristics, and is
    // a wholly new entry point not wired through add()/inventory(). Should
    // NOT be called from within a lock of the mutex. Defined below, after the
    // transform-policy helpers it relies on.
    void analyze(CSInfo* cs);

    // Build the processor-cache-disabled probe config and its normalized probe
    // values, lazily and once, under the same double-checked pattern as
    // examine(). Should NOT be called from within a lock of the mutex.
    void ensureProbeConfig() const;

    // Discover the scene interchange space of config_ (the verbatim discovery
    // order), filling `state.is_interoperable`/`interchange_colorspace`.
    // Reads config_; mutates only the passed-in state.
    void interop_bootstrap(InteropState& state) const;

    // Run the interoperability assertion + in-memory bootstrap once, lazily,
    // under the same double-checked pattern as examine(): discover whether
    // config_ is interoperable, build the interopified probe copy, and warn
    // once per structural config if the assertion fails. Should NOT be called
    // from within a lock of the mutex. ColorConfig construction never runs it.
    void ensure_interop() const;

    void debug_print_aliases()
    {
        DBG("Aliases: scene_linear={}   lin_srgb={}   srgb={}   ACEScg={}   Rec709={}\n",
            scene_linear_alias, lin_srgb_alias, srgb_alias, ACEScg_alias,
            Rec709_alias);
    }

    // For OCIO 2.3+, we can ask for the equivalent of some built-in
    // color spaces.
    void identify_builtin_equivalents();

    bool check_same_as_builtin_transform(const char* my_from,
                                         const char* builtin_to) const;
    bool test_conversion_yields(const char* from, const char* to,
                                cspan<Imath::C3f> test_colors,
                                cspan<Imath::C3f> result_colors) const;
    const char* IdentifyBuiltinColorSpace(const char* name) const;
};


namespace pvt {
// Grants the color-space classification test shims (declared in
// color_pvt.h, defined in the current namespace below) access to the
// private ColorConfig::Impl (shared by the color_*.cpp translation units).
struct ColorConfigClassificationPeek {
    static ColorConfig::Impl* impl(const ColorConfig& config)
    {
        return config.getImpl();
    }
};
}  // namespace pvt


// Custom ColorProcessor that wraps an OpenColorIO Processor.
class ColorProcessor_OCIO final : public ColorProcessor {
public:
    ColorProcessor_OCIO(OCIO::ConstProcessorRcPtr p)
        : m_p(p)
        , m_cpuproc(p->getDefaultCPUProcessor())
    {
    }
    ~ColorProcessor_OCIO() override {}

    bool isNoOp() const override { return m_p->isNoOp(); }
    bool hasChannelCrosstalk() const override
    {
        return m_p->hasChannelCrosstalk();
    }
    void apply(float* data, int width, int height, int channels,
               stride_t chanstride, stride_t xstride,
               stride_t ystride) const override
    {
        try {
            OCIO::PackedImageDesc pid(data, width, height, channels,
                                      OCIO::BIT_DEPTH_F32,  // For now, only float
                                      chanstride, xstride, ystride);
            m_cpuproc->apply(pid);
        } catch (OCIO::Exception& e) {
            OIIO::errorfmt("OCIO error in apply: {}\n", e.what());
            // FIXME -- some day, we should make ColorProcessor::apply return
            // a status, and we should indicate here that it failed.
        }
    }

private:
    OCIO::ConstProcessorRcPtr m_p;
    OCIO::ConstCPUProcessorRcPtr m_cpuproc;
};



// ColorProcessor that implements a matrix multiply color transformation.
class ColorProcessor_Matrix final : public ColorProcessor {
public:
    ColorProcessor_Matrix(const Imath::M44f& Matrix, bool inverse)
        : ColorProcessor()
        , m_M(Matrix)
    {
        if (inverse)
            m_M = m_M.inverse();
    }
    ~ColorProcessor_Matrix() override {}

    void apply(float* data, int width, int height, int channels,
               stride_t chanstride, stride_t xstride,
               stride_t ystride) const override
    {
        using namespace simd;
        if (channels == 3 && chanstride == sizeof(float)) {
            for (int y = 0; y < height; ++y) {
                char* d = (char*)data + y * ystride;
                for (int x = 0; x < width; ++x, d += xstride) {
                    vfloat4 color;
                    color.load((float*)d, 3);
                    vfloat4 xcolor = color * m_M;
                    xcolor.store((float*)d, 3);
                }
            }
        } else if (channels >= 4 && chanstride == sizeof(float)) {
            for (int y = 0; y < height; ++y) {
                char* d = (char*)data + y * ystride;
                for (int x = 0; x < width; ++x, d += xstride) {
                    vfloat4 color;
                    color.load((float*)d);
                    vfloat4 xcolor = color * m_M;
                    xcolor.store((float*)d);
                }
            }
        } else {
            channels = std::min(channels, 4);
            for (int y = 0; y < height; ++y) {
                char* d = (char*)data + y * ystride;
                for (int x = 0; x < width; ++x, d += xstride) {
                    vfloat4 color;
                    char* dc = d;
                    for (int c = 0; c < channels; ++c, dc += chanstride)
                        color[c] = *(float*)dc;
                    vfloat4 xcolor = color * m_M;
                    for (int c = 0; c < channels; ++c, dc += chanstride)
                        *(float*)dc = xcolor[c];
                }
            }
        }
    }

private:
    simd::matrix44 m_M;
};


// Test probe backing pvt::copy_config_preserves_default_view_transform().
// Defined in color_ocio.cpp.
bool
copy_config_default_vt_probe();

// Make an editable copy of `config`, working around an OCIO bug (fixed in
// 2.3.1) where createEditableCopy() drops the default view transform name.
// Defined in color_ocio.cpp.
OCIO::ConfigRcPtr
copy_config(const OCIO::ConstConfigRcPtr& config);

// The unsorted set of "simple" color space names for a config (the transform
// allowlist policy). Defined in color_ocio.cpp.
std::vector<std::string>
get_simple_color_spaces(const OCIO::ConstConfigRcPtr& config);

// Shared classification of atomic (non-structural) transform types: true when
// the transform is simple enough for interop matching. Defined in
// color_ocio.cpp.
bool
isSimpleAtomicTransform(const OCIO::ConstTransformRcPtr& transform);

// The full write-side derivation cascade behind pvt::derive_color_interop_id.
// Defined in color_ocio.cpp.
string_view
derive_color_interop_id_impl(const ColorConfig& config, string_view colorspace);

// Core of pvt::characterize_color_space() -- the field-selective
// characterization engine (see color_pvt.h for the contract) -- and its
// process-global cache's test hooks. Defined in color_characterization.cpp.
OIIO::pvt::CharacterizationRecord
characterize_color_space_impl(const ColorConfig& config,
                              string_view color_space,
                              uint32_t requested_fields,
                              const std::map<std::string, std::string>& context);
size_t
characterization_cache_size_impl();
void
characterization_cache_reset_impl();

// The calibrated fingerprint probe values, normalized into `config`'s
// reference spaces. Defined in color_fingerprint.cpp.
ProbeValues
initialize_probe_values(const OCIO::ConstConfigRcPtr& config,
                        const OCIO::ConstContextRcPtr& context);

// Transform the (reference-space) probe by the color space's from-reference
// transform; the resulting floats are its fingerprint. Defined in
// color_fingerprint.cpp.
std::optional<OIIO::pvt::ColorSpaceFingerprint>
compute_fingerprint(const OCIO::ConstConfigRcPtr& config,
                    const OCIO::ConstColorSpaceRcPtr& cs,
                    const OCIO::ConstContextRcPtr& context,
                    const ProbeValues& probes);

// Exact, tolerance-gated fingerprint identity match. Defined in
// color_fingerprint.cpp.
bool
fingerprints_match(const OIIO::pvt::ColorSpaceFingerprint& left,
                   const OIIO::pvt::ColorSpaceFingerprint& right);

// OIIO's built-in interop identities config (process-global, built once).
// Defined in color_registry.cpp.
OCIO::ConstConfigRcPtr
build_interop_identities_config();


// Each entry pairs a registry color space name (which, for the identities OIIO
// recognizes, equals its interop id) with that space's fingerprint, computed
// through the SAME interopified / PROCESSOR_CACHE_OFF probe path the query side
// uses, so registry and query fingerprints are directly comparable. Sorted by
// name for binary-search lookup.
struct RegistryFingerprintIndex {
    OCIO::ConstConfigRcPtr config;  // interopified registry (the probe source)
    std::vector<std::pair<std::string, OIIO::pvt::ColorSpaceFingerprint>> entries;
};

// Build (once, lazily) and return the process-global registry fingerprint
// index. Defined in color_registry.cpp.
const RegistryFingerprintIndex&
registry_fingerprint_index();

// Map a query interop id to its registry entry's fingerprint. Defined in
// color_registry.cpp.
const OIIO::pvt::ColorSpaceFingerprint*
registry_fingerprint_for_id(const RegistryFingerprintIndex& index,
                            string_view id, std::string& canonical_id_out);

// Reverse of registry_fingerprint_for_id: registry identity for a query
// fingerprint. Defined in color_registry.cpp.
string_view
registry_id_for_fingerprint(const RegistryFingerprintIndex& index,
                            const OIIO::pvt::ColorSpaceFingerprint& query_fp);

// The STRUCTURAL cache id of a config (context-independent). Defined in
// color_crossconfig.cpp.
std::string
get_config_cache_id(const OCIO::ConstConfigRcPtr& config);

// The "interopified" (repaired, PROCESSOR_CACHE_OFF, memoized) copy of
// `config`. Defined in color_crossconfig.cpp.
OCIO::ConstConfigRcPtr
interopify_config(const OCIO::ConstConfigRcPtr& config);

// The cross-config chokepoint: the single wrapper over OCIO's two-config
// GetProcessorFromConfigs. Defined in color_crossconfig.cpp.
OCIO::ConstProcessorRcPtr
processor_from_configs(const OCIO::ConstConfigRcPtr& src_config,
                       string_view src_name,
                       const OCIO::ConstConfigRcPtr& dst_config,
                       string_view dst_name, std::string& errmsg,
                       const OCIO::ConstContextRcPtr& src_context = nullptr,
                       const OCIO::ConstContextRcPtr& dst_context = nullptr,
                       const char* interchange_role
                       = OCIO::ROLE_INTERCHANGE_SCENE);

// Display-view sibling of the cross-config chokepoint. Defined in
// color_crossconfig.cpp.
OCIO::ConstProcessorRcPtr
display_processor_from_configs(const OCIO::ConstConfigRcPtr& src_config,
                              string_view src_name,
                              const OCIO::ConstConfigRcPtr& dst_config,
                              string_view display, string_view view,
                              OCIO::TransformDirection direction,
                              std::string& errmsg,
                              const OCIO::ConstContextRcPtr& src_context = nullptr,
                              const OCIO::ConstContextRcPtr& dst_context = nullptr);

// Core of pvt::identify_icc_profile(). Defined in color_icc_probe.cpp.
OIIO::pvt::IccIdentifyResult
identify_icc_profile_impl(const ColorConfig& config, cspan<uint8_t> iccdata);

// Core of pvt::derive_mastering_volume(). Defined in color_icc_probe.cpp.
bool
derive_mastering_volume_impl(const ColorConfig& config, string_view display,
                             string_view view,
                             OIIO::pvt::MasteringDisplayVolume& volume);

OIIO_NAMESPACE_END
