// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <tsl/robin_map.h>

#include <OpenImageIO/Imath.h>

#include <OpenImageIO/color.h>
#include <OpenImageIO/color_interop_ids.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/unordered_map_concurrent.h>

#include "imageio_pvt.h"

#define MAKE_OCIO_VERSION_HEX(maj, min, patch) \
    (((maj) << 24) | ((min) << 16) | (patch))

#include <OpenColorIO/OpenColorIO.h>

#include "interop_identities_config.h"

namespace OCIO = OCIO_NAMESPACE;


OIIO_NAMESPACE_3_1_BEGIN

namespace {
// Some test colors we use to interrogate transformations
static const int n_test_colors = 5;
static const Imath::C3f test_colors[n_test_colors]
    = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { 1, 1, 1 }, { 0.5, 0.5, 0.5 } };

// Make an editable copy of `config`, working around an OCIO bug (fixed in
// 2.3.1) where createEditableCopy() drops the default view transform name.
static OCIO::ConfigRcPtr
copy_config(const OCIO::ConstConfigRcPtr& config)
{
    auto copy = config->createEditableCopy();
#if OCIO_VERSION_HEX < 0x02030100
    // OCIO before 2.3.1 loses the default view transform name when copying a
    // config; restore it.
    std::string default_vt = config->getDefaultViewTransformName();
    if (!default_vt.empty()
        && default_vt != copy->getDefaultViewTransformName())
        copy->setDefaultViewTransformName(default_vt.c_str());
#endif
    return copy;
}
}  // namespace


// Test probe backing pvt::copy_config_preserves_default_view_transform()
// (declared in the current namespace, forwarded here). Builds a config with
// TWO view transforms whose explicit default is the NON-first one, runs the
// real copy_config(), and reports whether the copy kept that explicit default.
// The two-VT shape matters: OCIO reports the first view transform as the
// implicit default when none is set, so a single-VT probe would pass even if
// createEditableCopy() silently dropped the explicit default. Returns true on
// preservation -- native on OCIO >= 2.3.1, workaround-restored below that --
// and vacuously true when OCIO support or the view-transform API is
// unavailable.
bool
copy_config_default_vt_probe()
{
    if (!ColorConfig::supportsOpenColorIO())
        return true;
    try {
        OCIO::ConfigRcPtr cfg = OCIO::Config::CreateRaw()->createEditableCopy();
        for (const char* name : { "probe_vt_a", "probe_vt_b" }) {
            auto vt = OCIO::ViewTransform::Create(OCIO::REFERENCE_SPACE_SCENE);
            vt->setName(name);
            vt->setTransform(OCIO::MatrixTransform::Create(),
                             OCIO::VIEWTRANSFORM_DIR_FROM_REFERENCE);
            cfg->addViewTransform(vt);
        }
        // Explicit default is the SECOND view transform, not OCIO's implicit
        // first-VT default -- so a dropped default is observable.
        cfg->setDefaultViewTransformName("probe_vt_b");

        OCIO::ConstConfigRcPtr src = cfg;
        OCIO::ConfigRcPtr copy     = copy_config(src);
        const char* copied_default = copy->getDefaultViewTransformName();
        return copied_default && std::string(copied_default) == "probe_vt_b";
    } catch (const OCIO::Exception&) {
        return true;  // view-transform API unavailable -> vacuous pass
    }
}


#if 1 || !defined(NDEBUG) /* allow color configuration debugging */
static bool colordebug = Strutil::stoi(Sysutil::getenv("OIIO_DEBUG_COLOR"))
                         || Strutil::stoi(Sysutil::getenv("OIIO_DEBUG_ALL"));
#    define DBG(...)    \
        if (colordebug) \
        Strutil::print(__VA_ARGS__)
#else
#    define DBG(...)
#endif


static int disable_ocio = Strutil::stoi(Sysutil::getenv("OIIO_DISABLE_OCIO"));
static int disable_builtin_configs = Strutil::stoi(
    Sysutil::getenv("OIIO_DISABLE_BUILTIN_OCIO_CONFIGS"));
static OCIO::ConstConfigRcPtr ocio_current_config;



const ColorConfig&
ColorConfig::default_colorconfig()
{
    static ColorConfig config;
    return config;
}



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



bool
ColorConfig::supportsOpenColorIO()
{
    return (disable_ocio == 0);
}



int
ColorConfig::OpenColorIO_version_hex()
{
    return OCIO_VERSION_HEX;
}


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
    int m_flags   = 0;
    bool examined = false;
    bool analyzed = false;  // classification bits computed (see Impl::analyze)
    bool active   = true;   // member of the active colorspace enumeration
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

    void setflag(int flagval) { m_flags |= flagval; }

    // Set flag to include any bits in flagval, and also if alias is not yet
    // set, set it to name.
    void setflag(int flagval, std::string& alias)
    {
        m_flags |= flagval;
        if (alias.empty())
            alias = name;
    }

    int flags() const { return m_flags; }
};


// The classification bits observed by tests through pvt::ColorSpaceAnalysis
// (imageio_pvt.h) are the raw CSInfo classification bits; keep the two in
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



// Hidden implementation of ColorConfig
class ColorConfig::Impl {
public:
    OCIO::ConfigRcPtr config_;
    OCIO::ConfigRcPtr builtinconfig_;

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
    // verdict; it is cleared with the Impl (i.e. the config) lifetime.
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
    // in an empty handle, just return it.
    ColorProcessorHandle addproc(const ColorProcCacheKey& key,
                                 ColorProcessorHandle handle)
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
        return handle;
    }

    int getNumColorSpaces() const { return (int)colorspaces.size(); }

    const char* getColorSpaceNameByIndex(int index) const
    {
        return colorspaces[index].name.c_str();
    }

    string_view resolve(string_view name) const;

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
    // run in the encode direction).
    std::string effectiveEncoding(string_view name) const;
    std::optional<OIIO::pvt::Chromaticities>
    deriveChromaticities(string_view name) const;
    std::optional<OIIO::pvt::TransferFunctionSignature>
    deriveTransferSignature(string_view name) const;

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

    // Step 2 of ColorConfig::get_color_interop_id(): the write-side analog of
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
    // queries skip it. This is a hint, not a permanent verdict.
    void markLearnedComplex(string_view name) const
    {
        std::lock_guard<std::mutex> lock(m_learned_complex_mutex);
        m_learned_complex.emplace(name);
    }
    bool isLearnedComplex(string_view name) const
    {
        std::lock_guard<std::mutex> lock(m_learned_complex_mutex);
        return m_learned_complex.count(std::string(name)) != 0;
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
        if (!cs->examined) {
            spin_rw_write_lock lock(m_mutex);
            if (!cs->examined) {
                classify_by_name(*cs);
                classify_by_conversions(*cs);
                reclassify_heuristics(*cs);
                cs->examined = true;
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



// ColorConfig utility to take inventory of the color spaces available.
// It sets up knowledge of "linear", "srgb_rec709_scene", "Rec709", etc,
// even if the underlying OCIO configuration lacks them.
void
ColorConfig::Impl::inventory()
{
    DBG("inventorying config {}\n", configname());
    if (config_ && !disable_ocio) {
        try {
            bool nonraw = false;
            for (int i = 0, e = config_->getNumColorSpaces(); i < e; ++i)
                nonraw |= !Strutil::iequals(config_->getColorSpaceNameByIndex(i),
                                            "raw");
            if (nonraw) {
                for (int i = 0, e = config_->getNumColorSpaces(); i < e; ++i)
                    add(config_->getColorSpaceNameByIndex(i), i);
                for (auto&& cs : colorspaces)
                    classify_by_name(cs);
                OCIO::ConstColorSpaceRcPtr lin = config_->getColorSpace(
                    "scene_linear");
                if (lin)
                    scene_linear_alias = lin->getName();
                return;  // If any non-"raw" spaces were defined, we're done
            }
        } catch (OCIO::Exception& e) {
            DBG("OCIO exception in inventory: {}", e.what());
        }
    }

    // If we had some kind of bogus configuration that seemed to define
    // only a "raw" color space and nothing else, that's useless, so
    // figure out our own way to move forward.
    config_.reset();

    // If there was no configuration, or we didn't compile with OCIO
    // support at all, register a few basic names we know about.
    // For the "no OCIO / no config" case, we assume an unsophisticated
    // color pipeline where "linear" and the like are all assumed to use
    // Rec709/sRGB color primaries.
    int linflags = CSInfo::is_linear_response | CSInfo::is_scene_linear
                   | CSInfo::is_lin_srgb;
    add("linear", 0, linflags);
    add("scene_linear", 0, linflags);
    add("default", 0, linflags);
    add("rgb", 0, linflags);
    add("RGB", 0, linflags);
    add("lin_rec709_scene", 0, linflags);
    add("lin_srgb", 0, linflags);
    add("lin_rec709", 0, linflags);
    add("srgb_rec709_scene", 1, CSInfo::is_srgb);
    add("sRGB", 1, CSInfo::is_srgb);
    add("Rec709", 2, CSInfo::is_Rec709);

    for (auto&& cs : colorspaces)
        classify_by_name(cs);
}



inline bool
close_colors(cspan<Imath::C3f> a, cspan<Imath::C3f> b)
{
    OIIO_DASSERT(a.size() == b.size());
    for (size_t i = 0, e = a.size(); i < e; ++i)
        if (std::abs(a[i].x - b[i].x) > 1.0e-3f
            || std::abs(a[i].y - b[i].y) > 1.0e-3f
            || std::abs(a[i].z - b[i].z) > 1.0e-3f)
            return false;
    return true;
}



OCIO::ConstCPUProcessorRcPtr
ColorConfig::Impl::get_to_builtin_cpu_proc(const char* my_from,
                                           const char* builtin_to) const
{
    try {
        auto proc = OCIO::Config::GetProcessorToBuiltinColorSpace(config_,
                                                                  my_from,
                                                                  builtin_to);
        return proc ? proc->getDefaultCPUProcessor()
                    : OCIO::ConstCPUProcessorRcPtr();
    } catch (...) {
        return {};
    }
}



// Is this config's `my_from` color space equivalent to the built-in
// `builtin_to` color space? Find out by transforming the primaries, white,
// and half white and see if the results indicate that it was the identity
// transform (or close enough).
bool
ColorConfig::Impl::check_same_as_builtin_transform(const char* my_from,
                                                   const char* builtin_to) const
{
    if (disable_builtin_configs)
        return false;
    auto proc = get_to_builtin_cpu_proc(my_from, builtin_to);
    if (proc) {
        Imath::C3f colors[n_test_colors];
        std::copy(test_colors, test_colors + n_test_colors, colors);
        proc->apply(OCIO::PackedImageDesc(colors, n_test_colors, 1, 3));
        if (close_colors(colors, test_colors))
            return true;
    }
    return false;
}



// If we transform test_colors from "from" to "to" space, do we get
// result_colors? This is a building block for deducing some color spaces.
bool
ColorConfig::Impl::test_conversion_yields(const char* from, const char* to,
                                          cspan<Imath::C3f> test_colors,
                                          cspan<Imath::C3f> result_colors) const
{
    auto proc = m_self->createColorProcessor(from, to);
    if (!proc)
        return false;
    OIIO_DASSERT(test_colors.size() == result_colors.size());
    auto n             = test_colors.size();
    Imath::C3f* colors = OIIO_ALLOCA(Imath::C3f, n);
    std::copy(test_colors.data(), test_colors.data() + n, colors);
    proc->apply((float*)colors, int(n), 1, 3, sizeof(float), 3 * sizeof(float),
                int(n) * 3 * sizeof(float));
    return close_colors({ colors, n }, result_colors);
}



static bool
transform_has_Lut3D(string_view name, OCIO::ConstTransformRcPtr transform)
{
    using namespace OCIO;
    auto ttype = transform ? transform->getTransformType() : -1;
    if (ttype == TRANSFORM_TYPE_LUT3D || ttype == TRANSFORM_TYPE_COLORSPACE
        || ttype == TRANSFORM_TYPE_FILE || ttype == TRANSFORM_TYPE_LOOK
        || ttype == TRANSFORM_TYPE_DISPLAY_VIEW) {
        return true;
    }
    if (ttype == TRANSFORM_TYPE_GROUP) {
        auto group = dynamic_cast<const GroupTransform*>(transform.get());
        for (int i = 0, n = group->getNumTransforms(); i < n; ++i) {
            if (transform_has_Lut3D("", group->getTransform(i)))
                return true;
        }
    }
    if (name.size() && ttype >= 0)
        DBG("{} has type {}\n", name, ttype);
    return false;
}



void
ColorConfig::Impl::classify_by_name(CSInfo& cs)
{
    // General heuristics based on the names -- for a few canonical names,
    // believe them! Woe be unto the poor soul who names a color space "sRGB"
    // or "ACEScg" and it's really something entirely different.
    if (Strutil::iequals(cs.name, "srgb_rec709_scene")
        || Strutil::iequals(cs.name, "srgb_tx")
        || Strutil::iequals(cs.name, "srgb_texture")
        || Strutil::iequals(cs.name, "srgb texture")
        || Strutil::iequals(cs.name, "srgb_rec709_scene")
        || Strutil::iequals(cs.name, "sRGB - Texture")
        || Strutil::iequals(cs.name, "sRGB")) {
        cs.setflag(CSInfo::is_srgb, srgb_alias);
    } else if (Strutil::iequals(cs.name, "lin_rec709_scene")
               || Strutil::iequals(cs.name, "lin_rec709")
               || Strutil::iequals(cs.name, "Linear Rec.709 (sRGB)")
               || Strutil::iequals(cs.name, "lin_srgb")
               || Strutil::iequals(cs.name, "linear")) {
        cs.setflag(CSInfo::is_lin_srgb | CSInfo::is_linear_response,
                   lin_srgb_alias);
    } else if (Strutil::iequals(cs.name, "ACEScg")
               || Strutil::iequals(cs.name, "lin_ap1_scene")
               || Strutil::iequals(cs.name, "lin_ap1")) {
        cs.setflag(CSInfo::is_ACEScg | CSInfo::is_linear_response,
                   ACEScg_alias);
    } else if (Strutil::iequals(cs.name, "Rec709")) {
        cs.setflag(CSInfo::is_Rec709, Rec709_alias);
    } else if (config_
               && Strutil::iequals(cs.name, config_->getCanonicalName("data"))) {
        cs.setflag(CSInfo::is_data);
    }
#ifdef OIIO_SITE_spi
    // Ugly SPI-specific hacks, so sorry
    else if (Strutil::starts_with(cs.name, "cgln")) {
        cs.setflag(CSInfo::is_ACEScg | CSInfo::is_linear_response,
                   ACEScg_alias);
    } else if (cs.name == "srgbf" || cs.name == "srgbh" || cs.name == "srgb16"
               || cs.name == "srgb8") {
        cs.setflag(CSInfo::is_srgb, srgb_alias);
    } else if (cs.name == "srgblnf" || cs.name == "srgblnh"
               || cs.name == "srgbln16" || cs.name == "srgbln8") {
        cs.setflag(CSInfo::is_lin_srgb, lin_srgb_alias);
    } else if (Strutil::starts_with(cs.name, "nc")) {
        cs.setflag(CSInfo::is_data);
        DBG("Classifying {} as data based on SPI name\n", cs.name);
    }
#endif

    // Set up some canonical names
    if (cs.flags() & CSInfo::is_srgb)
        cs.canonical = "srgb_rec709_scene";
    else if (cs.flags() & CSInfo::is_lin_srgb)
        cs.canonical = "lin_rec709_scene";
    else if (cs.flags() & CSInfo::is_ACEScg)
        cs.canonical = "lin_ap1_scene";
    else if (cs.flags() & CSInfo::is_Rec709)
        cs.canonical = "Rec709";
    if (cs.canonical.size()) {
        DBG("classify by name identified '{}' as canonical {}\n", cs.name,
            cs.canonical);
        cs.examined = true;
    }
}



void
ColorConfig::Impl::classify_by_conversions(CSInfo& cs)
{
    DBG("classifying by conversions {}\n", cs.name);
    if (cs.examined)
        return;  // Already classified

    if (isColorSpaceLinear(cs.name))
        cs.setflag(CSInfo::is_linear_response);
    if (cs.ocio_cs && cs.ocio_cs->isData()) {
        cs.setflag(CSInfo::is_data);
        DBG("Classifying {} as data isData() [1]\n", cs.name);
    }

    // If the name didn't already tell us what it is, and we have a new enough
    // OCIO that has built-in configs, test whether this color space is
    // equivalent to one of a few particular built-in color spaces. That lets
    // us identify some color spaces even if they are named something
    // nonstandard. Skip this part if the color space we're classifying is
    // itself part of the built-in config -- in that case, it will already be
    // tagged correctly by the name above.
    if (!(cs.flags() & CSInfo::is_known) && config_ && !disable_ocio
        && !m_config_is_built_in) {
        using namespace OCIO;
        try {
            cs.ocio_cs = config_->getColorSpace(cs.name.c_str());
            if (transform_has_Lut3D(cs.name, cs.ocio_cs->getTransform(
                                                 COLORSPACE_DIR_TO_REFERENCE))
                || transform_has_Lut3D(cs.name,
                                       cs.ocio_cs->getTransform(
                                           COLORSPACE_DIR_FROM_REFERENCE))) {
                // Skip things with LUT3d because they are expensive due to LUT
                // inversion costs, and they're not gonna be our favourite
                // canonical spaces anyway.
                // DBG("{} has LUT3\n", cs.name);
            } else if (check_same_as_builtin_transform(cs.name.c_str(),
                                                       "srgb_tx")) {
                cs.setflag(CSInfo::is_srgb, srgb_alias);
            } else if (check_same_as_builtin_transform(cs.name.c_str(),
                                                       "lin_srgb")) {
                cs.setflag(CSInfo::is_lin_srgb | CSInfo::is_linear_response,
                           lin_srgb_alias);
            } else if (check_same_as_builtin_transform(cs.name.c_str(),
                                                       "ACEScg")) {
                cs.setflag(CSInfo::is_ACEScg | CSInfo::is_linear_response,
                           ACEScg_alias);
            }
            if (cs.ocio_cs->isData()) {
                cs.setflag(CSInfo::is_data);
                DBG("Classifying {} as data isData() [2]\n", cs.name);
            }
        } catch (OCIO::Exception& e) {
            DBG("OCIO exception in classify_by_conversions: {}", e.what());
        }
    }

    // Set up some canonical names
    if (cs.flags() & CSInfo::is_srgb)
        cs.canonical = "srgb_rec709_scene";
    else if (cs.flags() & CSInfo::is_lin_srgb)
        cs.canonical = "lin_rec709_scene";
    else if (cs.flags() & CSInfo::is_ACEScg)
        cs.canonical = "lin_ap1_scene";
    else if (cs.flags() & CSInfo::is_Rec709)
        cs.canonical = "Rec709";
}



void
ColorConfig::Impl::reclassify_heuristics(CSInfo& cs)
{
#if OCIO_VERSION_HEX < MAKE_OCIO_VERSION_HEX(2, 2, 0)
    // Extra checks for OCIO < 2.2. For >= 2.2, there is no need, we
    // already figured this out using the built-in configs.
    if (!(cs.flags() & CSInfo::is_known)) {
        // If this isn't one of the known color spaces, let's try some
        // tricks!
        static float srgb05 = linear_to_sRGB(0.5f);
        static Imath::C3f lin_srgb_to_srgb_results[n_test_colors]
            = { { 1, 0, 0 },
                { 0, 1, 0 },
                { 0, 0, 1 },
                { 1, 1, 1 },
                { srgb05, srgb05, srgb05 } };
        // If there is a known srgb space, and transforming our test
        // colors from "this cs" to srgb gives us what we expect for a
        // lin_srgb->srgb, then guess what? -- this is lin_srgb!
        if (srgb_alias.size()
            && test_conversion_yields(cs.name.c_str(), srgb_alias.c_str(),
                                      test_colors, lin_srgb_to_srgb_results)) {
            setflag(cs, CSInfo::is_lin_srgb | CSInfo::is_linear_response,
                    lin_srgb_alias);
            cs.canonical = "lin_srgb";
        }
    }
#endif
}



void
ColorConfig::Impl::identify_builtin_equivalents()
{
    if (disable_builtin_configs)
        return;
    Timer timer;
    if (auto n = IdentifyBuiltinColorSpace("srgb_tx")) {
        if (CSInfo* cs = find(n)) {
            cs->setflag(CSInfo::is_srgb, srgb_alias);
            DBG("Identified {} = builtin '{}'\n", "srgb_rec709_scene",
                cs->name);
        }
    } else {
        DBG("No config space identified as srgb\n");
    }
    DBG("identify_builtin_equivalents srgb took {:0.2f}s\n", timer.lap());
    if (auto n = IdentifyBuiltinColorSpace("lin_srgb")) {
        if (CSInfo* cs = find(n)) {
            cs->setflag(CSInfo::is_lin_srgb | CSInfo::is_linear_response,
                        lin_srgb_alias);
            DBG("Identified {} = builtin '{}'\n", "lin_rec709_scene", cs->name);
        }
    } else {
        DBG("No config space identified as lin_srgb\n");
    }
    DBG("identify_builtin_equivalents lin_srgb took {:0.2f}s\n", timer.lap());
    if (auto n = IdentifyBuiltinColorSpace("ACEScg")) {
        if (CSInfo* cs = find(n)) {
            cs->setflag(CSInfo::is_ACEScg | CSInfo::is_linear_response,
                        ACEScg_alias);
            DBG("Identified {} = builtin '{}'\n", "ACEScg", cs->name);
        }
    } else {
        DBG("No config space identified as acescg\n");
    }
    DBG("identify_builtin_equivalents acescg took {:0.2f}s\n", timer.lap());
}



const char*
ColorConfig::Impl::IdentifyBuiltinColorSpace(const char* name) const
{
    if (!config_ || disable_builtin_configs)
        return nullptr;
    try {
        return OCIO::Config::IdentifyBuiltinColorSpace(config_, builtinconfig_,
                                                       name);
    } catch (OCIO::Exception& e) {
        DBG("OCIO exception in IdentifyBuiltinColorSpace: {}", e.what());
    }
    return nullptr;
}



ColorConfig::ColorConfig(string_view filename) { (void)reset(filename); }



ColorConfig::~ColorConfig() {}



// OIIO doctoring of OCIO configs for different default file rules. Currently,
// we only do this for built-in configs.
static void
fix_config_file_rules(OCIO::ConfigRcPtr& config)
{
    OIIO_CONTRACT_ASSERT(config);
    DBG("Fixing up rules:\n");
#if 1
    // Just start with a clean slate
    auto rules = OCIO::FileRules::Create();
#else
    // Alternate universe: Start with the existing rules
    auto rules = config->getFileRules()->createEditableCopy();
#endif
    for (size_t i = 0, e = rules->getNumEntries(); i != e; ++i) {
        DBG("  rule {}/{}: pat='{}' ext='{}' -> \"{}\"\n", i, rules->getName(i),
            rules->getRegex(i), rules->getExtension(i),
            rules->getColorSpace(i));
#if 0
        // If we wanted to doctor just the exr rule, here's how:
        if (Strutil::iequals(rules->getExtension(i), "exr")) {
            // Change the rule for exr extension, if it exists, to "unknown".
            // Make no assumptions. OCIO's built-in configs think it should be
            // ACES2065-1, which is almost never right.
            rules->setColorSpace(i, "unknown");
            DBG("    changed cs to \"{}\"\n", rules->getColorSpace(i));
        } else
#endif
        if (!strcmp(rules->getName(i), "Default")) {
            // Default rule or one that matches everything -- for OIIO, we
            // just want to change this to unknown. We made decisions about
            // default per-file-format color space decisions in the individual
            // readers. We don't even consider file extension to be reliable
            // evidence of the file type.
            rules->setColorSpace(i, "unknown");
            DBG("    changed cs to \"{}\"\n", rules->getColorSpace(i));
        }
    }

    // But make the path search rule (look for the right-most color space name
    // embedded in the path) have precedence over file naming rules.
    rules->insertPathSearchRule(0);
    config->setFileRules(rules);
}



bool
ColorConfig::Impl::init(string_view filename)
{
    OIIO_MAYBE_UNUSED Timer timer;
    bool ok = true;

    auto oldlog = OCIO::GetLoggingLevel();
    OCIO::SetLoggingLevel(OCIO::LOGGING_LEVEL_NONE);

    try {
        auto cfg = OCIO::Config::CreateFromFile("ocio://default");
        OIIO_CONTRACT_ASSERT(cfg);
        builtinconfig_ = copy_config(cfg);
        fix_config_file_rules(builtinconfig_);
    } catch (OCIO::Exception& e) {
        error("Error making OCIO built-in config: {}", e.what());
    }

    // If no filename was specified, use env $OCIO
    if (filename.empty())
        filename = Sysutil::getenv("OCIO");
    if (filename.empty() && !disable_builtin_configs)
        filename = "ocio://default";
    if (filename.size() && !OIIO::Filesystem::exists(filename)
        && !Strutil::istarts_with(filename, "ocio://")) {
        error("Requested non-existent OCIO config \"{}\"", filename);
    } else {
        // Either filename passed, or taken from $OCIO, and it seems to exist
        try {
            configname(filename);
            auto cfg = OCIO::Config::CreateFromFile(
                std::string(filename).c_str());
            if (cfg)
                config_ = copy_config(cfg);
            if (config_ && Strutil::istarts_with(filename, "ocio://"))
                fix_config_file_rules(config_);
        } catch (OCIO::Exception& e) {
            error("Error reading OCIO config \"{}\": {}", filename, e.what());
        } catch (...) {
            error("Error reading OCIO config \"{}\"", filename);
        }
    }
    OCIO::SetLoggingLevel(oldlog);

    ok = config_.get() != nullptr;

    DBG("OCIO config {} loaded in {:0.2f} seconds\n", filename, timer.lap());

    inventory();
    // NOTE: inventory already does classify_by_name

    DBG("\nIDENTIFY BUILTIN EQUIVALENTS\n");
    identify_builtin_equivalents();  // OCIO 2.3+ only
    DBG("OCIO 2.3+ builtin equivalents in {:0.2f} seconds\n", timer.lap());

#if 1
    for (auto&& cs : colorspaces) {
        // examine(&cs);
        DBG("Color space '{}':\n", cs.name);
        if (cs.flags() & CSInfo::is_srgb)
            DBG("'{}' is srgb\n", cs.name);
        if (cs.flags() & CSInfo::is_lin_srgb)
            DBG("'{}' is lin_srgb\n", cs.name);
        if (cs.flags() & CSInfo::is_ACEScg)
            DBG("'{}' is ACEScg\n", cs.name);
        if (cs.flags() & CSInfo::is_Rec709)
            DBG("'{}' is Rec709\n", cs.name);
        if (cs.flags() & CSInfo::is_linear_response)
            DBG("'{}' has linear response\n", cs.name);
        if (cs.flags() & CSInfo::is_scene_linear)
            DBG("'{}' is scene_linear\n", cs.name);
        if (cs.flags())
            DBG("\n");
    }
#endif
    debug_print_aliases();
    DBG("OCIO config {} classified in {:0.2f} seconds\n", filename,
        timer.lap());

    return ok;
}



bool
ColorConfig::reset(string_view filename)
{
    OIIO::pvt::LoggedTimer logtime("ColorConfig::reset");
    if (m_impl
        && (filename == getImpl()->configname()
            || (filename == ""
                && getImpl()->configname() == "ocio://default"))) {
        // Request to reset to the config we're already using. Just return,
        // don't do anything expensive.
        return true;
    }

    m_impl.reset(new ColorConfig::Impl(this));
    return m_impl->init(filename);
}



bool
ColorConfig::has_error() const
{
    return (getImpl()->haserror());
}



std::string
ColorConfig::geterror(bool clear) const
{
    return getImpl()->geterror(clear);
}



int
ColorConfig::getNumColorSpaces() const
{
    return (int)getImpl()->getNumColorSpaces();
}



const char*
ColorConfig::getColorSpaceNameByIndex(int index) const
{
    return getImpl()->getColorSpaceNameByIndex(index);
}



int
ColorConfig::getColorSpaceIndex(string_view name) const
{
    // Check for exact matches
    for (int i = 0, e = getNumColorSpaces(); i < e; ++i)
        if (Strutil::iequals(getColorSpaceNameByIndex(i), name))
            return i;
    // Check for aliases and equivalents
    for (int i = 0, e = getNumColorSpaces(); i < e; ++i)
        if (equivalent(getColorSpaceNameByIndex(i), name))
            return i;
    return -1;
}



const char*
ColorConfig::getColorSpaceFamilyByName(string_view name) const
{
    if (getImpl()->config_ && !disable_ocio) {
        try {
            OCIO::ConstColorSpaceRcPtr c = getImpl()->config_->getColorSpace(
                std::string(name).c_str());
            if (c)
                return c->getFamily();
        } catch (OCIO::Exception& e) {
            DBG("OCIO exception in getColorSpaceFamilyByName: {}", e.what());
        }
    }
    return nullptr;
}



std::vector<std::string>
ColorConfig::getColorSpaceNames() const
{
    std::vector<std::string> result;
    int n = getNumColorSpaces();
    result.reserve(n);
    for (int i = 0; i < n; ++i)
        result.emplace_back(getColorSpaceNameByIndex(i));
    return result;
}

int
ColorConfig::getNumRoles() const
{
    if (getImpl()->config_ && !disable_ocio)
        return getImpl()->config_->getNumRoles();
    return 0;
}

const char*
ColorConfig::getRoleByIndex(int index) const
{
    try {
        if (getImpl()->config_ && !disable_ocio)
            return getImpl()->config_->getRoleName(index);
    } catch (OCIO::Exception& e) {
        DBG("OCIO exception in getRoleByIndex: {}", e.what());
    }
    return nullptr;
}


std::vector<std::string>
ColorConfig::getRoles() const
{
    std::vector<std::string> result;
    for (int i = 0, e = getNumRoles(); i != e; ++i)
        result.emplace_back(getRoleByIndex(i));
    return result;
}



int
ColorConfig::getNumLooks() const
{
    try {
        if (getImpl()->config_ && !disable_ocio)
            return getImpl()->config_->getNumLooks();
    } catch (OCIO::Exception& e) {
        DBG("OCIO exception in getNumLooks: {}", e.what());
    }
    return 0;
}



const char*
ColorConfig::getLookNameByIndex(int index) const
{
    try {
        if (getImpl()->config_ && !disable_ocio)
            return getImpl()->config_->getLookNameByIndex(index);
    } catch (OCIO::Exception& e) {
        DBG("OCIO exception in getLookNameByIndex: {}", e.what());
    }
    return nullptr;
}



std::vector<std::string>
ColorConfig::getLookNames() const
{
    std::vector<std::string> result;
    for (int i = 0, e = getNumLooks(); i != e; ++i)
        result.emplace_back(getLookNameByIndex(i));
    return result;
}



bool
ColorConfig::isColorSpaceLinear(string_view name) const
{
    return getImpl()->isColorSpaceLinear(name);
}



bool
ColorConfig::Impl::isColorSpaceLinear(string_view name) const
{
    if (config_ && !disable_builtin_configs && !disable_ocio) {
        try {
            return config_->isColorSpaceLinear(c_str(name),
                                               OCIO::REFERENCE_SPACE_SCENE)
                   || config_->isColorSpaceLinear(c_str(name),
                                                  OCIO::REFERENCE_SPACE_DISPLAY);
        } catch (...) {
            return false;
        }
    }
    return Strutil::iequals(name, "linear")
           || Strutil::istarts_with(name, "linear ")
           || Strutil::istarts_with(name, "linear_")
           || Strutil::istarts_with(name, "lin_")
           || Strutil::iends_with(name, "_linear")
           || Strutil::iends_with(name, "_lin");
}



bool
ColorConfig::isData(string_view name) const
{
    return getImpl()->isData(name);
}



bool
ColorConfig::Impl::isData(string_view name) const
{
    if (const CSInfo* cs = find(name)) {
        return cs->flags() & CSInfo::is_data;
    }
    return false;
}



std::vector<std::string>
ColorConfig::getAliases(string_view color_space) const
{
    std::vector<std::string> result;
    auto config = getImpl()->config_;
    if (config) {
        auto cs = config->getColorSpace(c_str(color_space));
        if (cs) {
            for (int i = 0, e = cs->getNumAliases(); i < e; ++i)
                result.emplace_back(cs->getAlias(i));
        }
    }
    return result;
}



const char*
ColorConfig::getColorSpaceNameByRole(string_view role) const
{
    if (getImpl()->config_ && !disable_ocio) {
        try {
            OCIO::ConstColorSpaceRcPtr c = getImpl()->config_->getColorSpace(
                std::string(role).c_str());
            // DBG("looking first for named color space {} -> {}\n", role,
            //     c ? c->getName() : "not found");
            // Catch special case of obvious name synonyms
            if (!c
                && (Strutil::iequals(role, "RGB")
                    || Strutil::iequals(role, "default")))
                role = string_view("linear");
            if (!c && Strutil::iequals(role, "linear"))
                c = getImpl()->config_->getColorSpace("scene_linear");
            if (!c && Strutil::iequals(role, "scene_linear"))
                c = getImpl()->config_->getColorSpace("linear");
            if (!c && Strutil::iequals(role, "srgb")) {
                c = getImpl()->config_->getColorSpace("sRGB - Texture");
                // DBG("Unilaterally substituting {} -> '{}'\n", role,
                //                c->getName());
            }

            if (c) {
                // DBG("found color space {} for role {}\n", c->getName(),
                //                role);
                return c->getName();
            }
        } catch (OCIO::Exception& e) {
            DBG("OCIO exception in getColorSpaceNameByRole: {}", e.what());
        }
    }

    // No OCIO at build time, or no OCIO configuration at run time
    if (Strutil::iequals(role, "linear")
        || Strutil::iequals(role, "scene_linear"))
        return "linear";

    return nullptr;  // Dunno what role
}



TypeDesc
ColorConfig::getColorSpaceDataType(string_view name, int* bits) const
{
    if (getImpl()->config_ && !disable_ocio) {
        try {
            OCIO::ConstColorSpaceRcPtr c = getImpl()->config_->getColorSpace(
                std::string(name).c_str());
            if (c) {
                OCIO::BitDepth b = c->getBitDepth();
                switch (b) {
                case OCIO::BIT_DEPTH_UNKNOWN: return TypeDesc::UNKNOWN;
                case OCIO::BIT_DEPTH_UINT8: *bits = 8; return TypeDesc::UINT8;
                case OCIO::BIT_DEPTH_UINT10:
                    *bits = 10;
                    return TypeDesc::UINT16;
                case OCIO::BIT_DEPTH_UINT12:
                    *bits = 12;
                    return TypeDesc::UINT16;
                case OCIO::BIT_DEPTH_UINT14:
                    *bits = 14;
                    return TypeDesc::UINT16;
                case OCIO::BIT_DEPTH_UINT16:
                    *bits = 16;
                    return TypeDesc::UINT16;
                case OCIO::BIT_DEPTH_UINT32:
                    *bits = 32;
                    return TypeDesc::UINT32;
                case OCIO::BIT_DEPTH_F16: *bits = 16; return TypeDesc::HALF;
                case OCIO::BIT_DEPTH_F32: *bits = 32; return TypeDesc::FLOAT;
                }
            }
        } catch (OCIO::Exception& e) {
            DBG("OCIO exception in getColorSpaceDataType: {}", e.what());
        }
    }
    return TypeUnknown;
}



int
ColorConfig::getNumDisplays() const
{
    try {
        if (getImpl()->config_ && !disable_ocio)
            return getImpl()->config_->getNumDisplays();
    } catch (OCIO::Exception& e) {
        DBG("OCIO exception in getNumDisplays: {}", e.what());
    }
    return 0;
}



const char*
ColorConfig::getDisplayNameByIndex(int index) const
{
    try {
        if (getImpl()->config_ && !disable_ocio)
            return getImpl()->config_->getDisplay(index);
    } catch (OCIO::Exception& e) {
        DBG("OCIO exception in getDisplayNameByIndex: {}", e.what());
    }
    return nullptr;
}



std::vector<std::string>
ColorConfig::getDisplayNames() const
{
    std::vector<std::string> result;
    for (int i = 0, e = getNumDisplays(); i != e; ++i)
        result.emplace_back(getDisplayNameByIndex(i));
    return result;
}



int
ColorConfig::getNumViews(string_view display) const
{
    if (display.empty())
        display = getDefaultDisplayName();
    try {
        if (getImpl()->config_ && !disable_ocio)
            return getImpl()->config_->getNumViews(
                std::string(display).c_str());
    } catch (OCIO::Exception& e) {
        DBG("OCIO exception in getNumViews: {}", e.what());
    }
    return 0;
}



const char*
ColorConfig::getViewNameByIndex(string_view display, int index) const
{
    if (display.empty())
        display = getDefaultDisplayName();
    try {
        if (getImpl()->config_ && !disable_ocio)
            return getImpl()->config_->getView(std::string(display).c_str(),
                                               index);
    } catch (OCIO::Exception& e) {
        DBG("OCIO exception in getViewNameByIndex: {}", e.what());
    }
    return nullptr;
}



std::vector<std::string>
ColorConfig::getViewNames(string_view display) const
{
    std::vector<std::string> result;
    if (display.empty())
        display = getDefaultDisplayName();
    for (int i = 0, e = getNumViews(display); i != e; ++i)
        result.emplace_back(getViewNameByIndex(display, i));
    return result;
}



const char*
ColorConfig::getDefaultDisplayName() const
{
    try {
        if (getImpl()->config_ && !disable_ocio)
            return getImpl()->config_->getDefaultDisplay();
    } catch (OCIO::Exception& e) {
        DBG("OCIO exception in getDefaultDisplayName: {}", e.what());
    }
    return nullptr;
}



const char*
ColorConfig::getDefaultViewName(string_view display) const
{
    if (display.empty() || display == "default")
        display = getDefaultDisplayName();
    try {
        if (getImpl()->config_ && !disable_ocio)
            return getImpl()->config_->getDefaultView(c_str(display));
    } catch (OCIO::Exception& e) {
        DBG("OCIO exception in getDefaultViewName: {}", e.what());
    }
    return nullptr;
}


const char*
ColorConfig::getDefaultViewName(string_view display,
                                string_view inputColorSpace) const
{
    try {
        if (display.empty() || display == "default")
            display = getDefaultDisplayName();
        if (inputColorSpace.empty() || inputColorSpace == "default")
            inputColorSpace = getImpl()->config_->getColorSpaceFromFilepath(
                c_str(inputColorSpace));
        if (getImpl()->config_ && !disable_ocio)
            return getImpl()->config_->getDefaultView(c_str(display),
                                                      c_str(inputColorSpace));
    } catch (OCIO::Exception& e) {
        DBG("OCIO exception in getDefaultViewName: {}", e.what());
    }
    return nullptr;
}


const char*
ColorConfig::getDisplayViewColorSpaceName(const std::string& display,
                                          const std::string& view) const
{
    if (getImpl()->config_ && !disable_ocio) {
        try {
            string_view name = getImpl()->config_->getDisplayViewColorSpaceName(
                c_str(display), c_str(view));
            // Handle certain Shared View cases
            if (strcmp(c_str(name), "<USE_DISPLAY_NAME>") == 0)
                name = display;
            return c_str(name);
        } catch (OCIO::Exception& e) {
            DBG("OCIO exception in getDisplayViewColorSpaceName: {}", e.what());
        }
    }
    return nullptr;
}



const char*
ColorConfig::getDisplayViewLooks(const std::string& display,
                                 const std::string& view) const
{
    try {
        if (getImpl()->config_ && !disable_ocio)
            return getImpl()->config_->getDisplayViewLooks(display.c_str(),
                                                           view.c_str());
    } catch (OCIO::Exception& e) {
        DBG("OCIO exception in getDisplayViewLooks: {}", e.what());
    }
    return nullptr;
}



int
ColorConfig::getNumNamedTransforms() const
{
    try {
        if (getImpl()->config_ && !disable_ocio)
            return getImpl()->config_->getNumNamedTransforms();
    } catch (OCIO::Exception& e) {
        DBG("OCIO exception in getNumNamedTransforms: {}", e.what());
    }
    return 0;
}



const char*
ColorConfig::getNamedTransformNameByIndex(int index) const
{
    try {
        if (getImpl()->config_ && !disable_ocio)
            return getImpl()->config_->getNamedTransformNameByIndex(index);
    } catch (OCIO::Exception& e) {
        DBG("OCIO exception in getNamedTransformNameByIndex: {}", e.what());
    }
    return nullptr;
}



std::vector<std::string>
ColorConfig::getNamedTransformNames() const
{
    std::vector<std::string> result;
    for (int i = 0, e = getNumNamedTransforms(); i != e; ++i)
        result.emplace_back(getNamedTransformNameByIndex(i));
    return result;
}



std::vector<std::string>
ColorConfig::getNamedTransformAliases(string_view named_transform) const
{
    std::vector<std::string> result;
    auto config = getImpl()->config_;
    if (config) {
        auto nt = config->getNamedTransform(c_str(named_transform));
        if (nt) {
            for (int i = 0, e = nt->getNumAliases(); i < e; ++i)
                result.emplace_back(nt->getAlias(i));
        }
    }
    return result;
}



std::string
ColorConfig::configname() const
{
    if (getImpl()->config_ && !disable_ocio)
        return getImpl()->configname();
    return "built-in";
}



string_view
ColorConfig::resolve(string_view name) const
{
    return getImpl()->resolve(name);
}



namespace {

// Helpers for the interop-ID resolution tiers layered onto resolve(). They
// only read the OCIO config and the pure grammar/sanitization functions from
// imageio_pvt.h; none of them touch the interoperability bootstrap or the
// fingerprint engine. Every string_view they return is backed by an OCIO-owned
// color space name (stable for the life of the config), never a temporary.

// Tier 1a'' -- the config-local form "<config>:local:<base>". Resolves only
// when the id parses as outer:local:base and `outer` sanitizes to this config's
// own name; then it matches `base` against every color space's sanitized name
// or alias (across all reference types and visibilities). Deliberately never
// consults a color space's interop_id attribute -- that is tier 1c's job.
string_view
resolve_local_namespace(const OCIO::ConstConfigRcPtr& config, string_view name)
{
    OIIO::pvt::InteropIdParts parts
        = OIIO::pvt::parse_interop_id(std::string(name));
    if (parts.form != OIIO::pvt::InteropIdForm::OUTER_INNER_BASE
        || parts.inner != "local")
        return {};
    const char* cfgname = config->getName();
    if (!cfgname || !*cfgname)
        return {};
    if (OIIO::pvt::sanitize_id_token(cfgname) != parts.outer)
        return {};
    int n = 0;
    try {
        n = config->getNumColorSpaces(OCIO::SEARCH_REFERENCE_SPACE_ALL,
                                      OCIO::COLORSPACE_ALL);
    } catch (...) {
        return {};
    }
    for (int i = 0; i < n; ++i) {
        const char* nm
            = config->getColorSpaceNameByIndex(OCIO::SEARCH_REFERENCE_SPACE_ALL,
                                               OCIO::COLORSPACE_ALL, i);
        if (!nm || !*nm)
            continue;
        OCIO::ConstColorSpaceRcPtr cs;
        try {
            cs = config->getColorSpace(nm);
        } catch (...) {
            continue;
        }
        if (!cs)
            continue;
        if (OIIO::pvt::sanitize_id_token(cs->getName()) == parts.base)
            return cs->getName();
        for (int a = 0, ae = cs->getNumAliases(); a < ae; ++a)
            if (OIIO::pvt::sanitize_id_token(cs->getAlias(a)) == parts.base)
                return cs->getName();
    }
    return {};
}

// Tier 1c -- match `name` against a color space's explicit interop_id attribute
// (OCIO 2.5+). A match requires the attribute to equal the query exactly, or to
// match with exactly ONE side's leftmost namespace stripped (never both -- so
// "oiio:x" does not false-match a query for "ocio:x" just because both strip to
// "x"). Utility tokens (data/unknown/bypass) are excluded from this lookup
// entirely: declaring interop_id: bypass must not make a space reachable by
// querying "bypass".
string_view
resolve_explicit_interop_id(const OCIO::ConstConfigRcPtr& config,
                            string_view name)
{
#if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 5, 0)
    std::string id(name);
    std::string id_stripped = OIIO::pvt::strip_leftmost_namespace(id);
    // Linear scan over the catalog; add a cached inverted map if this tier gets hot.
    // only fires when the direct/stripped/local tiers all miss (a rare path),
    // so a per-query scan is cheaper than maintaining a cache. Build the map if
    // this ever shows up hot.
    int n = 0;
    try {
        n = config->getNumColorSpaces(OCIO::SEARCH_REFERENCE_SPACE_ALL,
                                      OCIO::COLORSPACE_ALL);
    } catch (...) {
        return {};
    }
    for (int i = 0; i < n; ++i) {
        const char* nm
            = config->getColorSpaceNameByIndex(OCIO::SEARCH_REFERENCE_SPACE_ALL,
                                               OCIO::COLORSPACE_ALL, i);
        if (!nm || !*nm)
            continue;
        OCIO::ConstColorSpaceRcPtr cs;
        try {
            cs = config->getColorSpace(nm);
        } catch (...) {
            continue;
        }
        if (!cs)
            continue;
        const char* iid = cs->getInteropID();
        if (!iid || !*iid)
            continue;
        std::string attr(iid);
        if (OIIO::pvt::is_utility_interop_id(attr))
            continue;
        if (attr == id || attr == id_stripped
            || OIIO::pvt::strip_leftmost_namespace(attr) == id)
            return cs->getName();
    }
#else
    (void)config;
    (void)name;
#endif
    return {};
}

// The color space name that `token` (a name, alias, or role) resolves to in
// `config`, or empty if it resolves to nothing.
std::string
resolve_token_colorspace_name(const OCIO::ConstConfigRcPtr& config,
                              const char* token)
{
    try {
        OCIO::ConstColorSpaceRcPtr c = config->getColorSpace(token);
        return c ? std::string(c->getName()) : std::string();
    } catch (...) {
        return std::string();
    }
}

// Whether a data color space `cs` (name `nm`) identifies as `token` -- either
// via its explicit interop_id attribute (OCIO 2.5+), or because `token`'s
// name/alias/role resolution (precomputed as `token_target`) lands on it.
bool
data_space_identifies_as(const OCIO::ConstColorSpaceRcPtr& cs, const char* nm,
                         string_view token, const std::string& token_target)
{
#if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 5, 0)
    const char* iid = cs->getInteropID();
    if (iid && *iid && token == string_view(iid))
        return true;
#endif
    return !token_target.empty() && token_target == nm;
}

// Utility-token preference for the literal queries "data" and "bypass": rank
// every data color space and return the lowest-ranked one. rank 0 = identifies
// as the requested token, rank 1 = plain data space (no identity), rank 2 =
// identifies as the other token (data<->bypass), rank 3 = identifies as
// "unknown"; lowest rank wins and a rank-0 hit short-circuits. ("unknown" is
// intentionally not routed here -- it only ever resolves as a literal
// name/alias, handled by tier 1a.)
string_view
resolve_data_utility(const OCIO::ConstConfigRcPtr& config, string_view requested)
{
    const char* req         = requested == "data" ? "data" : "bypass";
    const char* other       = requested == "data" ? "bypass" : "data";
    std::string req_target  = resolve_token_colorspace_name(config, req);
    std::string other_target = resolve_token_colorspace_name(config, other);
    std::string unk_target  = resolve_token_colorspace_name(config, "unknown");

    const char* best = nullptr;
    int best_rank    = 4;  // worse than any real rank (0..3)
    int n            = 0;
    try {
        n = config->getNumColorSpaces(OCIO::SEARCH_REFERENCE_SPACE_ALL,
                                      OCIO::COLORSPACE_ALL);
    } catch (...) {
        return {};
    }
    for (int i = 0; i < n; ++i) {
        const char* nm
            = config->getColorSpaceNameByIndex(OCIO::SEARCH_REFERENCE_SPACE_ALL,
                                               OCIO::COLORSPACE_ALL, i);
        if (!nm || !*nm)
            continue;
        OCIO::ConstColorSpaceRcPtr cs;
        try {
            cs = config->getColorSpace(nm);
        } catch (...) {
            continue;
        }
        if (!cs || !cs->isData())
            continue;
        // A one-space OCIO::Config::CreateRaw() config is nothing but a
        // synthetic data space named "raw". Skip that space so an empty/raw
        // config does not spuriously resolve "data"/"bypass" to it -- but key
        // the skip on the config's single-colorspace shape (n == 1), not the
        // name alone: a real config may legitimately hold a data space named
        // "Raw" alongside others, and that one is a valid target. A role
        // explicitly naming the requested token still wins.
        if (n == 1 && Strutil::iequals(nm, "raw") && req_target != nm)
            continue;
        int rank;
        if (data_space_identifies_as(cs, nm, req, req_target))
            rank = 0;
        else if (data_space_identifies_as(cs, nm, other, other_target))
            rank = 2;
        else if (data_space_identifies_as(cs, nm, "unknown", unk_target))
            rank = 3;
        else
            rank = 1;  // plain data space, no identity
        if (rank < best_rank) {
            best_rank = rank;
            best      = cs->getName();
            if (rank == 0)
                break;
        }
    }
    return best ? string_view(best) : string_view();
}

}  // namespace



string_view
ColorConfig::Impl::resolve_name_tier1a(string_view name) const
{
    OCIO::ConstConfigRcPtr config = config_;
    if (config && !disable_ocio) {
        try {
            OCIO::ConstColorSpaceRcPtr cs = config->getColorSpace(c_str(name));
            if (cs)
                return cs->getName();
        } catch (OCIO::Exception& e) {
            DBG("OCIO exception in resolve: {}", e.what());
        }
    }
    // OCIO did not know this name as a color space, role, or alias.

    // Maybe it's an informal alias of common names?
    spin_rw_write_lock lock(m_mutex);
    if ((Strutil::iequals(name, "sRGB")
         || Strutil::iequals(name, "srgb_rec709_scene"))
        && !srgb_alias.empty())
        return srgb_alias;
    if ((Strutil::iequals(name, "lin_srgb")
         || Strutil::iequals(name, "lin_rec709")
         || Strutil::iequals(name, "lin_rec709_scene")
         || Strutil::iequals(name, "linear"))
        && lin_srgb_alias.size())
        return lin_srgb_alias;
    if ((Strutil::iequals(name, "ACEScg")
         || Strutil::iequals(name, "lin_ap1_scene"))
        && !ACEScg_alias.empty())
        return ACEScg_alias;
    if (Strutil::iequals(name, "scene_linear") && !scene_linear_alias.empty()) {
        return scene_linear_alias;
    }
    if (Strutil::iequals(name, "Rec709") && Rec709_alias.size())
        return Rec709_alias;

    return {};
}



string_view
ColorConfig::Impl::resolve(string_view name) const
{
    // Tier 1a: direct OCIO color space / role / alias, then informal aliases.
    if (string_view r = resolve_name_tier1a(name); !r.empty())
        return r;

    // Tier 1a': stripped-namespace retry. Only meaningful when the name carries
    // a namespace to strip. strip_leftmost_namespace() consumes exactly one
    // leading "<ns>:"; note "my-studio::srgb" strips to ":srgb" (the blank inner
    // colon survives), which deliberately will NOT match "srgb".
    if (name.find(':') != string_view::npos) {
        std::string stripped
            = OIIO::pvt::strip_leftmost_namespace(std::string(name));
        if (string_view r = resolve_name_tier1a(stripped); !r.empty())
            return r;
    }

    // The remaining tiers all consult the OCIO config directly.
    if (config_ && !disable_ocio) {
        // Tier 1a'': config-local form "<config>:local:<space>".
        if (string_view r = resolve_local_namespace(config_, name); !r.empty())
            return r;

        // Tier 1c: a color space's explicit interop_id attribute (OCIO 2.5+).
        if (string_view r = resolve_explicit_interop_id(config_, name);
            !r.empty())
            return r;

        // Utility tokens: "data"/"bypass" resolve to a ranked data color space.
        // "unknown" is intentionally not ranked -- it only ever resolves via the
        // literal name/alias match already attempted in tier 1a.
        if (name == "data" || name == "bypass")
            if (string_view r = resolve_data_utility(config_, name); !r.empty())
                return r;

        // Tier 2: registry equivalence (fingerprint match). Canonicalize the id
        // through the built-in interop identities registry and return this
        // config's OWN equivalent simple color space -- the user's own space
        // wins over building any cross-config processor (that is a later,
        // separate feature; this tier returns only names). Utility tokens have
        // no registry fingerprint and miss automatically. This is the first tier
        // that fingerprints, so it lazily builds the process-global registry
        // index and this config's probe/bootstrap state on first use; every
        // earlier tier -- and ColorConfig construction -- stays fingerprint-free.
        // The const_cast reaches the memoizing (logically-const) fingerprint
        // caches, mirroring getImpl()'s non-const handle onto a const config.
        if (string_view r
            = const_cast<ColorConfig::Impl*>(this)->resolve_registry_equivalence(
                name);
            !r.empty())
            return r;
    }

    // Total miss: preserve OIIO's historical passthrough -- return the input
    // name unchanged so callers that assumed identity resolution keep working.
    return name;
}



bool
ColorConfig::equivalent(string_view color_space1,
                        string_view color_space2) const
{
    // Empty color spaces never match
    if (color_space1.empty() || color_space2.empty())
        return false;
    // Easy case: matching names are the same!
    if (Strutil::iequals(color_space1, color_space2))
        return true;

    // If "resolved" names (after converting aliases and roles to color
    // spaces) match, they are equivalent.
    color_space1 = resolve(color_space1);
    color_space2 = resolve(color_space2);
    if (color_space1.empty() || color_space2.empty())
        return false;
    if (Strutil::iequals(color_space1, color_space2))
        return true;

    // If the color spaces' flags (when masking only the bits that refer to
    // specific known color spaces) match, consider them equivalent.
    const int mask = CSInfo::is_srgb | CSInfo::is_lin_srgb | CSInfo::is_ACEScg
                     | CSInfo::is_Rec709;
    const CSInfo* csi1 = getImpl()->find(color_space1);
    const CSInfo* csi2 = getImpl()->find(color_space2);
    if (csi1 && csi2) {
        int flags1 = csi1->flags() & mask;
        int flags2 = csi2->flags() & mask;
        if ((flags1 | flags2) && csi1->flags() == csi2->flags())
            return true;
        if ((csi1->canonical.size() && csi2->canonical.size())
            && Strutil::iequals(csi1->canonical, csi2->canonical))
            return true;
    }

    return false;
}



bool
equivalent_colorspace(string_view a, string_view b)
{
    return ColorConfig::default_colorconfig().equivalent(a, b);
}



inline OCIO::BitDepth
ocio_bitdepth(TypeDesc type)
{
    if (type == TypeDesc::UINT8)
        return OCIO::BIT_DEPTH_UINT8;
    if (type == TypeDesc::UINT16)
        return OCIO::BIT_DEPTH_UINT16;
    if (type == TypeDesc::UINT32)
        return OCIO::BIT_DEPTH_UINT32;
    // N.B.: OCIOv2 also supports 10, 12, and 14 bit int, but we won't
    // ever have data in that format at this stage.
    if (type == TypeDesc::HALF)
        return OCIO::BIT_DEPTH_F16;
    if (type == TypeDesc::FLOAT)
        return OCIO::BIT_DEPTH_F32;
    return OCIO::BIT_DEPTH_UNKNOWN;
}



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



ColorProcessorHandle
ColorConfig::createColorProcessor(string_view inputColorSpace,
                                  string_view outputColorSpace,
                                  string_view context_key,
                                  string_view context_value) const
{
    return createColorProcessor(ustring(inputColorSpace),
                                ustring(outputColorSpace), ustring(context_key),
                                ustring(context_value));
}



ColorProcessorHandle
ColorConfig::createColorProcessor(ustring inputColorSpace,
                                  ustring outputColorSpace, ustring context_key,
                                  ustring context_value) const
{
    std::string pending_error;

    // First, look up the requested processor in the cache. If it already
    // exists, just return it.
    ColorProcCacheKey prockey(inputColorSpace, outputColorSpace, context_key,
                              context_value);
    ColorProcessorHandle handle = getImpl()->findproc(prockey);
    if (handle)
        return handle;

    // DBG("createColorProcessor {} -> {}\n", inputColorSpace,
    //                outputColorSpace);
    // Ask OCIO to make a Processor that can handle the requested
    // transformation.
    OCIO::ConstProcessorRcPtr p;
    if (getImpl()->config_ && !disable_ocio) {
        // Canonicalize the names
        inputColorSpace  = ustring(resolve(inputColorSpace));
        outputColorSpace = ustring(resolve(outputColorSpace));
        // DBG("after role substitution, {} -> {}\n", inputColorSpace,
        //                outputColorSpace);
        auto config = getImpl()->config_;
        try {
            auto context = config->getCurrentContext();
            auto keys    = Strutil::splits(context_key, ",");
            auto values  = Strutil::splits(context_value, ",");
            if (keys.size() && values.size() && keys.size() == values.size()) {
                OCIO::ContextRcPtr ctx = context->createEditableCopy();
                for (size_t i = 0; i < keys.size(); ++i)
                    ctx->setStringVar(keys[i].c_str(), values[i].c_str());
                context = ctx;
            }

            // Get the processor corresponding to this transform.
            p = getImpl()->config_->getProcessor(context,
                                                 inputColorSpace.c_str(),
                                                 outputColorSpace.c_str());
            getImpl()->clear_error();
            // DBG("Created OCIO processor '{}' -> '{}'\n",
            //                inputColorSpace, outputColorSpace);
        } catch (OCIO::Exception& e) {
            // Don't quit yet, remember the error and see if any of our
            // built-in knowledge of some generic spaces will save us.
            p.reset();
            pending_error = e.what();
            // DBG("FAILED to create OCIO processor '{}' -> '{}'\n",
            //                inputColorSpace, outputColorSpace);
        } catch (...) {
            p.reset();
            getImpl()->error(
                "An unknown error occurred in OpenColorIO, getProcessor");
        }

        if (p && !p->isNoOp()) {
            // If we got a valid processor that does something useful,
            // return it now. If it boils down to a no-op, give a second
            // chance below to recognize it as a special case.
            try {
                handle = ColorProcessorHandle(new ColorProcessor_OCIO(p));
            } catch (OCIO::Exception& e) {
                getImpl()->error("Exception from OCIO: {}", e.what());
            }
            // DBG("OCIO processor '{}' -> '{}' is NOT NoOp, handle = {}\n",
            //                inputColorSpace, outputColorSpace, (bool)handle);
        } else if (!p) {
            // Local resolution failed (OCIO threw for a name this config does
            // not define). If a requested name is a registry-known interop
            // identity the config lacks, reconcile the conversion across configs
            // through the interop bridge. This is deliberately on by default
            // (observable via debug log), with OCIO strict_parsing as the
            // opt-out. This may return a bridged processor, a lenient
            // pass-through fallback (non-strict parsing), or nothing --
            // leaving today's error -- when the feature does not apply or
            // strict parsing is on.
            std::string reconciled;
            ColorProcessorHandle bridged
                = getImpl()->reconcile_cross_config(inputColorSpace,
                                                    outputColorSpace, reconciled);
            if (bridged) {
                handle = bridged;
                if (reconciled.empty())
                    getImpl()->clear_error();  // clean bridge success
                pending_error = reconciled;    // empty on success; a
                                               // continue-message on fallback
            } else if (reconciled.size()) {
                pending_error = reconciled;  // strict hard error: why+how-to-fix
            }
            // else: reconciliation declined -- keep today's OCIO error.
        }
    }

    if (!handle && p) {
        // If we found a processor from OCIO, even if it was a NoOp, and we
        // still don't have a better idea, return it.
        try {
            handle = ColorProcessorHandle(new ColorProcessor_OCIO(p));
        } catch (OCIO::Exception& e) {
            getImpl()->error("Exception from OCIO: {}", e.what());
        }
    }

    if (pending_error.size())
        getImpl()->error("{}", pending_error);

    return getImpl()->addproc(prockey, handle);
}



ColorProcessorHandle
ColorConfig::createLookTransform(string_view looks, string_view inputColorSpace,
                                 string_view outputColorSpace, bool inverse,
                                 string_view context_key,
                                 string_view context_value) const
{
    return createLookTransform(ustring(looks), ustring(inputColorSpace),
                               ustring(outputColorSpace), inverse,
                               ustring(context_key), ustring(context_value));
}



ColorProcessorHandle
ColorConfig::createLookTransform(ustring looks, ustring inputColorSpace,
                                 ustring outputColorSpace, bool inverse,
                                 ustring context_key,
                                 ustring context_value) const
{
    // First, look up the requested processor in the cache. If it already
    // exists, just return it.
    ColorProcCacheKey prockey(inputColorSpace, outputColorSpace, context_key,
                              context_value, looks, ustring() /*display*/,
                              ustring() /*view*/, ustring() /*file*/,
                              ustring() /*namedtransform*/, inverse);
    ColorProcessorHandle handle = getImpl()->findproc(prockey);
    if (handle)
        return handle;

    // Ask OCIO to make a Processor that can handle the requested
    // transformation.
    if (getImpl()->config_ && !disable_ocio) {
        OCIO::ConstConfigRcPtr config = getImpl()->config_;
        try {
            OCIO::LookTransformRcPtr transform = OCIO::LookTransform::Create();
            transform->setLooks(looks.c_str());
            OCIO::TransformDirection dir;
            if (inverse) {
                // The TRANSFORM_DIR_INVERSE applies an inverse for the
                // end-to-end transform, which would otherwise do dst->inv
                // look -> src.  This is an unintuitive result for the artist
                // (who would expect in, out to remain unchanged), so we
                // account for that here by flipping src/dst
                transform->setSrc(c_str(resolve(outputColorSpace)));
                transform->setDst(c_str(resolve(inputColorSpace)));
                dir = OCIO::TRANSFORM_DIR_INVERSE;
            } else {  // forward
                transform->setSrc(c_str(resolve(inputColorSpace)));
                transform->setDst(c_str(resolve(outputColorSpace)));
                dir = OCIO::TRANSFORM_DIR_FORWARD;
            }
            auto context = config->getCurrentContext();
            auto keys    = Strutil::splits(context_key, ",");
            auto values  = Strutil::splits(context_value, ",");
            if (keys.size() && values.size() && keys.size() == values.size()) {
                OCIO::ContextRcPtr ctx = context->createEditableCopy();
                for (size_t i = 0; i < keys.size(); ++i)
                    ctx->setStringVar(keys[i].c_str(), values[i].c_str());
                context = ctx;
            }

            // Get the processor corresponding to this transform.
            OCIO::ConstProcessorRcPtr p;
            p = getImpl()->config_->getProcessor(context, transform, dir);
            getImpl()->clear_error();
            handle = ColorProcessorHandle(new ColorProcessor_OCIO(p));
        } catch (OCIO::Exception& e) {
            getImpl()->error(e.what());
        } catch (...) {
            getImpl()->error(
                "An unknown error occurred in OpenColorIO, getProcessor");
        }
    }

    return getImpl()->addproc(prockey, handle);
}



ColorProcessorHandle
ColorConfig::createDisplayTransform(string_view display, string_view view,
                                    string_view inputColorSpace,
                                    string_view looks, bool inverse,
                                    string_view context_key,
                                    string_view context_value) const
{
    return createDisplayTransform(ustring(display), ustring(view),
                                  ustring(inputColorSpace), ustring(looks),
                                  inverse, ustring(context_key),
                                  ustring(context_value));
}



ColorProcessorHandle
ColorConfig::createDisplayTransform(ustring display, ustring view,
                                    ustring inputColorSpace, ustring looks,
                                    bool inverse, ustring context_key,
                                    ustring context_value) const
{
    if (display.empty() || display == "default")
        display = getDefaultDisplayName();
    if (view.empty() || view == "default")
        view = getDefaultViewName(display, inputColorSpace);
    // First, look up the requested processor in the cache. If it already
    // exists, just return it.
    ColorProcCacheKey prockey(inputColorSpace, ustring() /*outputColorSpace*/,
                              context_key, context_value, looks, display, view,
                              ustring() /*file*/, ustring() /*namedtransform*/,
                              inverse);
    ColorProcessorHandle handle = getImpl()->findproc(prockey);
    if (handle)
        return handle;

    // Ask OCIO to make a Processor that can handle the requested
    // transformation.
    if (getImpl()->config_ && !disable_ocio) {
        OCIO::ConstConfigRcPtr config = getImpl()->config_;
        std::string pending_error;
        try {
            auto transform = OCIO::DisplayViewTransform::Create();
            auto legacy_viewing_pipeline = OCIO::LegacyViewingPipeline::Create();
            OCIO::TransformDirection dir = inverse
                                               ? OCIO::TRANSFORM_DIR_INVERSE
                                               : OCIO::TRANSFORM_DIR_FORWARD;
            transform->setSrc(inputColorSpace.c_str());
            transform->setDisplay(display.c_str());
            transform->setView(view.c_str());
            transform->setDirection(dir);
            legacy_viewing_pipeline->setDisplayViewTransform(transform);
            if (looks.size()) {
                legacy_viewing_pipeline->setLooksOverride(looks.c_str());
                legacy_viewing_pipeline->setLooksOverrideEnabled(true);
            }
            auto context = config->getCurrentContext();
            auto keys    = Strutil::splits(context_key, ",");
            auto values  = Strutil::splits(context_value, ",");
            if (keys.size() && values.size() && keys.size() == values.size()) {
                OCIO::ContextRcPtr ctx = context->createEditableCopy();
                for (size_t i = 0; i < keys.size(); ++i)
                    ctx->setStringVar(keys[i].c_str(), values[i].c_str());
                context = ctx;
            }

            // Get the processor corresponding to this transform.
            OCIO::ConstProcessorRcPtr p;
            p = legacy_viewing_pipeline->getProcessor(config, context);
            getImpl()->clear_error();
            handle = ColorProcessorHandle(new ColorProcessor_OCIO(p));
        } catch (OCIO::Exception& e) {
            // Local resolution failed (OCIO threw for an input name this config
            // does not define). If the input is a registry-known interop
            // identity the config lacks, reconcile the display transform across
            // configs through the interop bridge -- on by default, observable
            // via debug log, with OCIO strict_parsing as the opt-out -- mirroring
            // the color-space route. Crucially, on failure this takes the
            // strict-aware fallback -- it does NOT silently continue with
            // setSrc(ROLE_SCENE_LINEAR) as an earlier iteration of this display
            // path did.
            pending_error = e.what();
            std::string reconciled;
            ColorProcessorHandle bridged
                = getImpl()->reconcile_cross_config_display(inputColorSpace,
                                                            display, view,
                                                            inverse, reconciled);
            if (bridged) {
                handle = bridged;
                if (reconciled.empty())
                    getImpl()->clear_error();  // clean bridge success
                pending_error = reconciled;    // empty on success; a
                                               // continue-message on fallback
            } else if (reconciled.size()) {
                pending_error = reconciled;  // strict hard error: why+how-to-fix
            }
            // else: reconciliation declined -- keep today's OCIO error.
        } catch (...) {
            getImpl()->error(
                "An unknown error occurred in OpenColorIO, getProcessor");
        }
        if (pending_error.size())
            getImpl()->error("{}", pending_error);
    }

    return getImpl()->addproc(prockey, handle);
}



ColorProcessorHandle
ColorConfig::createFileTransform(string_view name, bool inverse) const
{
    return createFileTransform(ustring(name), inverse);
}



ColorProcessorHandle
ColorConfig::createFileTransform(ustring name, bool inverse) const
{
    // First, look up the requested processor in the cache. If it already
    // exists, just return it.
    ColorProcCacheKey prockey(ustring() /*inputColorSpace*/,
                              ustring() /*outputColorSpace*/,
                              ustring() /*context_key*/,
                              ustring() /*context_value*/, ustring() /*looks*/,
                              ustring() /*display*/, ustring() /*view*/,
                              ustring() /*file*/, name, inverse);
    ColorProcessorHandle handle = getImpl()->findproc(prockey);
    if (handle)
        return handle;

    // Ask OCIO to make a Processor that can handle the requested
    // transformation.
    OCIO::ConstConfigRcPtr config = getImpl()->config_;
    // If no config was found, config_ will be null. But that shouldn't
    // stop us for a filetransform, which doesn't need color spaces anyway.
    // Just use the default current config, it'll be freed when we exit.
    if (!config)
        config = ocio_current_config;
    if (config) {
        try {
            OCIO::FileTransformRcPtr transform = OCIO::FileTransform::Create();
            transform->setSrc(name.c_str());
            transform->setInterpolation(OCIO::INTERP_BEST);
            OCIO::TransformDirection dir(inverse ? OCIO::TRANSFORM_DIR_INVERSE
                                                 : OCIO::TRANSFORM_DIR_FORWARD);
            OCIO::ConstContextRcPtr context = config->getCurrentContext();
            // Get the processor corresponding to this transform.
            OCIO::ConstProcessorRcPtr p;
            p = config->getProcessor(context, transform, dir);
            getImpl()->clear_error();
            handle = ColorProcessorHandle(new ColorProcessor_OCIO(p));
        } catch (OCIO::Exception& e) {
            getImpl()->error(e.what());
        } catch (...) {
            getImpl()->error(
                "An unknown error occurred in OpenColorIO, getProcessor");
        }
    }

    return getImpl()->addproc(prockey, handle);
}



ColorProcessorHandle
ColorConfig::createNamedTransform(string_view name, bool inverse,
                                  string_view context_key,
                                  string_view context_value) const
{
    return createNamedTransform(ustring(name), inverse, ustring(context_key),
                                ustring(context_value));
}



ColorProcessorHandle
ColorConfig::createNamedTransform(ustring name, bool inverse,
                                  ustring context_key,
                                  ustring context_value) const
{
    // First, look up the requested processor in the cache. If it already
    // exists, just return it.
    ColorProcCacheKey prockey(ustring() /*inputColorSpace*/,
                              ustring() /*outputColorSpace*/, context_key,
                              context_value, ustring() /*looks*/,
                              ustring() /*display*/, ustring() /*view*/,
                              ustring() /*file*/, name, inverse);
    ColorProcessorHandle handle = getImpl()->findproc(prockey);
    if (handle)
        return handle;

    // Ask OCIO to make a Processor that can handle the requested
    // transformation.
    if (getImpl()->config_ && !disable_ocio) {
        OCIO::ConstConfigRcPtr config = getImpl()->config_;
        try {
            auto transform = config->getNamedTransform(name.c_str());
            OCIO::TransformDirection dir(inverse ? OCIO::TRANSFORM_DIR_INVERSE
                                                 : OCIO::TRANSFORM_DIR_FORWARD);
            auto context = config->getCurrentContext();
            auto keys    = Strutil::splits(context_key, ",");
            auto values  = Strutil::splits(context_value, ",");
            if (keys.size() && values.size() && keys.size() == values.size()) {
                OCIO::ContextRcPtr ctx = context->createEditableCopy();
                for (size_t i = 0; i < keys.size(); ++i)
                    ctx->setStringVar(keys[i].c_str(), values[i].c_str());
                context = ctx;
            }

            // Get the processor corresponding to this transform.
            OCIO::ConstProcessorRcPtr p;
            p = config->getProcessor(context, transform, dir);
            getImpl()->clear_error();
            handle = ColorProcessorHandle(new ColorProcessor_OCIO(p));
        } catch (OCIO::Exception& e) {
            getImpl()->error(e.what());
        } catch (...) {
            getImpl()->error(
                "An unknown error occurred in OpenColorIO, getProcessor");
        }
    }

    return getImpl()->addproc(prockey, handle);
}



ColorProcessorHandle
ColorConfig::createMatrixTransform(M44fParam M, bool inverse) const
{
    return ColorProcessorHandle(
        new ColorProcessor_Matrix(*(const Imath::M44f*)M.data(), inverse));
}



string_view
ColorConfig::getColorSpaceFromFilepath(string_view str) const
{
    if (getImpl() && getImpl()->config_) {
        try {
            std::string s(str);
            string_view r = getImpl()->config_->getColorSpaceFromFilepath(
                s.c_str());
            return r;
        } catch (OCIO::Exception& e) {
            DBG("OCIO exception in getColorSpaceFromFilepath: {}", e.what());
        }
    }
    // Fall back on parseColorSpaceFromString
    return parseColorSpaceFromString(str);
}

string_view
ColorConfig::getColorSpaceFromFilepath(string_view str, string_view default_cs,
                                       bool cs_name_match) const
{
    if (getImpl() && getImpl()->config_) {
        try {
            std::string s(str);
            string_view r = getImpl()->config_->getColorSpaceFromFilepath(
                s.c_str());
            if (!getImpl()->config_->filepathOnlyMatchesDefaultRule(s.c_str()))
                return r;
        } catch (OCIO::Exception& e) {
            DBG("OCIO exception in getColorSpaceFromFilepath: {}", e.what());
        }
    }
    if (cs_name_match) {
        string_view parsed = parseColorSpaceFromString(str);
        if (parsed.size())
            return parsed;
    }
    return default_cs;
}

bool
ColorConfig::filepathOnlyMatchesDefaultRule(string_view str) const
{
    try {
        return getImpl()->config_->filepathOnlyMatchesDefaultRule(c_str(str));
    } catch (OCIO::Exception& e) {
        DBG("OCIO exception in filepathOnlyMatchesDefaultRule: {}", e.what());
    }
    return false;
}

string_view
ColorConfig::parseColorSpaceFromString(string_view str) const
{
    // Reproduce the logic in OCIO v1 parseColorSpaceFromString

    if (str.empty())
        return "";

    // Get the colorspace names, sorted shortest-to-longest
    auto names = getColorSpaceNames();
    std::sort(names.begin(), names.end(),
              [](const std::string& a, const std::string& b) {
                  return a.length() < b.length();
              });

    // See if it matches a LUT name.
    // This is the position of the RIGHT end of the colorspace substring,
    // not the left
    size_t rightMostColorPos = std::string::npos;
    std::string rightMostColorspace;

    // Find the right-most occurrence within the string for each colorspace.
    for (auto&& csname : names) {
        // find right-most extension matched in filename
        size_t pos = Strutil::irfind(str, csname);
        if (pos == std::string::npos)
            continue;

        // If we have found a match, move the pointer over to the right end
        // of the substring.  This will allow us to find the longest name
        // that matches the rightmost colorspace
        pos += csname.size();

        if (rightMostColorPos == std::string::npos
            || pos >= rightMostColorPos) {
            rightMostColorPos   = pos;
            rightMostColorspace = csname;
        }
    }
    return string_view(ustring(rightMostColorspace));
}


//////////////////////////////////////////////////////////////////////////
//
// Color-space classification: the "simple" transform allowlist and the lazy
// per-space analysis pass that sets the CSInfo classification bits.

namespace {
using namespace OCIO;

// Shared classification of atomic (non-structural) transform types. Returns
// true when the transform is simple enough for interop matching. GROUP,
// FILE, and COLORSPACE are structural -- they need recursive or deferred
// handling that differs by caller, so callers handle those themselves. This
// is the single policy switch shared by the recursive authored-transform
// scan (containsBlockableTransform) and, in the future, op-by-op inspection
// of a realized GroupTransform.
bool
isSimpleAtomicTransform(const ConstTransformRcPtr& transform)
{
    if (!transform)
        return true;
    switch (transform->getTransformType()) {
    case TRANSFORM_TYPE_LUT3D:
    case TRANSFORM_TYPE_CDL:
    case TRANSFORM_TYPE_LOOK:
    case TRANSFORM_TYPE_DISPLAY_VIEW: return false;

    case TRANSFORM_TYPE_BUILTIN: {
        auto builtin = DynamicPtrCast<const BuiltinTransform>(transform);
        if (builtin) {
            string_view style(builtin->getStyle() ? builtin->getStyle() : "");
            // ACES rendering pipelines (output + LMT) carry rendering, tone
            // mapping, and look logic that defeats the primaries+TF
            // fingerprint. "DISPLAY - CIE-XYZ-D65_to_*" builtins, by
            // contrast, are pure display encodings (matrix + transfer
            // function) and fingerprint cleanly -- keep them simple.
            if (Strutil::starts_with(style, "ACES-OUTPUT")
                || Strutil::starts_with(style, "ACES-LMT"))
                return false;
        }
        return true;
    }

    case TRANSFORM_TYPE_FIXED_FUNCTION: {
#if OCIO_VERSION_HEX <= MAKE_OCIO_VERSION_HEX(2, 4, 0)
        return false;
#else
        auto ff = DynamicPtrCast<const FixedFunctionTransform>(transform);
        if (ff) {
            const auto style = ff->getStyle();
            if (style == FIXED_FUNCTION_LIN_TO_DOUBLE_LOG
                || style == FIXED_FUNCTION_LIN_TO_GAMMA_LOG)
                return true;
        }
        return false;
#endif
    }

    case TRANSFORM_TYPE_MATRIX:
    case TRANSFORM_TYPE_RANGE:
    case TRANSFORM_TYPE_EXPONENT:
    case TRANSFORM_TYPE_EXPONENT_WITH_LINEAR:
    case TRANSFORM_TYPE_LOG:
    case TRANSFORM_TYPE_LOG_AFFINE:
    case TRANSFORM_TYPE_LOG_CAMERA:
    case TRANSFORM_TYPE_ALLOCATION:
    case TRANSFORM_TYPE_LUT1D:
#if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 3, 0)
    case TRANSFORM_TYPE_GRADING_RGB_CURVE:
#endif
        return true;

    default:
        // Unknown/unclassified transform types are not simple.
        return false;
    }
}

// Does any string authored into this transform reference a context var, or
// could a FileTransform in it resolve through a context-dependent search
// path? Direct (non-recursive) scan of the authored transform only, except
// recursing through GROUP.
// For now a '$'-substring scan stands in for per-search-path-entry analysis;
// upgrade to per-entry var analysis if configs with mixed var/non-var search
// paths need finer verdicts.
bool
transformUsesContextVars(const ConstTransformRcPtr& transform,
                         bool search_path_has_vars)
{
    if (!transform)
        return false;
    auto has_var = [](const char* s) {
        return s && Strutil::contains(s, "$");
    };
    switch (transform->getTransformType()) {
    case TRANSFORM_TYPE_FILE: {
        auto ft = DynamicPtrCast<const FileTransform>(transform);
        if (!ft)
            return false;
        return has_var(ft->getSrc()) || has_var(ft->getCCCId())
               || search_path_has_vars;
    }
    case TRANSFORM_TYPE_GROUP: {
        auto gt = DynamicPtrCast<const GroupTransform>(transform);
        for (int i = 0, e = gt ? gt->getNumTransforms() : 0; i < e; ++i)
            if (transformUsesContextVars(gt->getTransform(i),
                                         search_path_has_vars))
                return true;
        return false;
    }
    case TRANSFORM_TYPE_COLORSPACE: {
        auto cst = DynamicPtrCast<const ColorSpaceTransform>(transform);
        if (!cst)
            return false;
        return has_var(cst->getSrc()) || has_var(cst->getDst());
    }
    default: return false;
    }
}

bool
fileTransformIsBlockable(const ConstFileTransformRcPtr& fileTransform)
{
    if (!fileTransform) {
        return true;
    }
    const char* src = fileTransform->getSrc();
    if (!src || !*src) {
        return true;
    }
    if (Strutil::iends_with(src, ".spi1d")
        || Strutil::iends_with(src, ".spimtx")) {
        return false;
    }
    return true;
}

bool
containsBlockableTransform(const ConstConfigRcPtr& config,
                           const ConstContextRcPtr& context,
                           const ConstTransformRcPtr& transform,
                           std::unordered_set<std::string>& keep,
                           std::unordered_set<std::string>& omit);
bool
containsBlockableTransform(const ConstConfigRcPtr& config,
                           const ConstContextRcPtr& context, const char* name,
                           std::unordered_set<std::string>& keep,
                           std::unordered_set<std::string>& omit);
bool
containsBlockableTransform(const ConstConfigRcPtr& config,
                           const ConstTransformRcPtr& transform,
                           std::unordered_set<std::string>& keep,
                           std::unordered_set<std::string>& omit);

// Walk every color space, keeping the ones with only simple transforms.
// keep/omit memoize spaces already scanned so referenced sub-transforms
// aren't rescanned.
std::unordered_set<std::string>
scan_simple_color_space_names(const ConstConfigRcPtr& config)
{
    std::unordered_set<std::string> keep;
    if (!config) {
        return keep;
    }

    std::unordered_set<std::string> omit;
    ConstContextRcPtr ctx = config->getCurrentContext();

    const int n = config->getNumColorSpaces(SEARCH_REFERENCE_SPACE_ALL,
                                            COLORSPACE_ALL);
    for (int i = 0; i < n; ++i) {
        const char* name
            = config->getColorSpaceNameByIndex(SEARCH_REFERENCE_SPACE_ALL,
                                               COLORSPACE_ALL, i);
        if (!name || !*name) {
            continue;
        }
        if (keep.count(name) || omit.count(name)) {
            continue;
        }

        if (containsBlockableTransform(config, ctx, name, keep, omit)) {
            omit.insert(name);
        } else {
            keep.insert(name);
        }
    }

    return keep;
}

bool
colorSpaceHasBlockableTransform(const ConstConfigRcPtr& config,
                                const ConstColorSpaceRcPtr& cs,
                                std::unordered_set<std::string>& keep,
                                std::unordered_set<std::string>& omit)
{
    if (!cs) {
        return true;
    }
    const char* csName = cs->getName();
    if (cs->isData()) {
        if (csName && *csName)
            omit.insert(csName);
        return true;
    }
    if (csName && keep.count(csName)) {
        return false;
    }

    ConstTransformRcPtr toRef = cs->getTransform(COLORSPACE_DIR_TO_REFERENCE);
    if (toRef && containsBlockableTransform(config, toRef, keep, omit)) {
        if (csName && *csName)
            omit.insert(csName);
        return true;
    }

    ConstTransformRcPtr fromRef = cs->getTransform(
        COLORSPACE_DIR_FROM_REFERENCE);
    if (fromRef && containsBlockableTransform(config, fromRef, keep, omit)) {
        if (csName && *csName)
            omit.insert(csName);
        return true;
    }

    if (csName && *csName)
        keep.insert(csName);
    return false;
}

bool
namedTransformHasBlockableTransform(const ConstConfigRcPtr& config,
                                    const ConstNamedTransformRcPtr& nt,
                                    std::unordered_set<std::string>& keep,
                                    std::unordered_set<std::string>& omit)
{
    if (!nt) {
        return true;
    }
    ConstTransformRcPtr fwd = nt->getTransform(TRANSFORM_DIR_FORWARD);
    if (fwd && containsBlockableTransform(config, fwd, keep, omit)) {
        return true;
    }
    ConstTransformRcPtr rev = nt->getTransform(TRANSFORM_DIR_INVERSE);
    if (rev && containsBlockableTransform(config, rev, keep, omit)) {
        return true;
    }
    return false;
}

bool
containsBlockableTransform(const ConstConfigRcPtr& config,
                           const ConstContextRcPtr& context, const char* name,
                           std::unordered_set<std::string>& keep,
                           std::unordered_set<std::string>& omit)
{
    if (!name || !*name) {
        return true;
    }
    ConstContextRcPtr ctx = context ? context : config->getCurrentContext();
    auto name_cs          = ctx->resolveStringVar(c_str(name));


    ConstColorSpaceRcPtr cs = config->getColorSpace(c_str(name_cs));
    if (cs) {
        if (omit.count(c_str(cs->getName()))) {
            return true;
        }
        if (keep.count(c_str(cs->getName()))) {
            return false;
        }
        return colorSpaceHasBlockableTransform(config, cs, keep, omit);
    }

    ConstNamedTransformRcPtr nt = config->getNamedTransform(c_str(name_cs));
    if (!nt) {
        return true;
    }
    return namedTransformHasBlockableTransform(config, nt, keep, omit);
}

bool
containsBlockableTransform(const ConstConfigRcPtr& config,
                           const ConstTransformRcPtr& transform,
                           std::unordered_set<std::string>& keep,
                           std::unordered_set<std::string>& omit)
{
    return containsBlockableTransform(config, config->getCurrentContext(),
                                      transform, keep, omit);
}

bool
containsBlockableTransform(const ConstConfigRcPtr& config,
                           const ConstContextRcPtr& context,
                           const ConstTransformRcPtr& transform,
                           std::unordered_set<std::string>& keep,
                           std::unordered_set<std::string>& omit)
{
    if (!transform) {
        return false;
    }

    ConstContextRcPtr ctx = context ? context : config->getCurrentContext();

    switch (transform->getTransformType()) {
    case TRANSFORM_TYPE_FILE: {
        ConstFileTransformRcPtr ft = DynamicPtrCast<const FileTransform>(
            transform);
        return fileTransformIsBlockable(ft);
    }
    case TRANSFORM_TYPE_GROUP: {
        ConstGroupTransformRcPtr gt = DynamicPtrCast<const GroupTransform>(
            transform);
        if (!gt)
            return false;
        for (int i = 0, e = gt->getNumTransforms(); i < e; ++i) {
            if (containsBlockableTransform(config, ctx, gt->getTransform(i),
                                           keep, omit)) {
                return true;
            }
        }
        return false;
    }
    case TRANSFORM_TYPE_COLORSPACE: {
        ConstColorSpaceTransformRcPtr cst
            = DynamicPtrCast<const ColorSpaceTransform>(transform);
        if (!cst) {
            return true;
        }

        const char* src = cst->getSrc();
        const char* dst = cst->getDst();

        auto src_cs_name            = ctx->resolveStringVar(c_str(src));
        auto dst_cs_name            = ctx->resolveStringVar(c_str(dst));
        ConstColorSpaceRcPtr src_cs = config->getColorSpace(c_str(src_cs_name));
        ConstColorSpaceRcPtr dst_cs = config->getColorSpace(c_str(dst_cs_name));

        if (!src_cs && dst_cs) {
            bool blocked = containsBlockableTransform(config, ctx,
                                                      c_str(dst_cs->getName()),
                                                      keep, omit);
            return blocked;
        }
        if (!dst_cs && src_cs) {
            bool blocked = containsBlockableTransform(config, ctx,
                                                      c_str(src_cs->getName()),
                                                      keep, omit);
            return blocked;
        }

        if (src_cs && dst_cs) {
            if (omit.count(src_cs->getName())
                || omit.count(dst_cs->getName())) {
                return true;
            }
            if (keep.count(c_str(src_cs->getName()))
                && keep.count(c_str(dst_cs->getName())))
                return false;
            bool blocked = containsBlockableTransform(config, ctx,
                                                      c_str(src_cs->getName()),
                                                      keep, omit);
            if (blocked)
                return true;
            blocked = containsBlockableTransform(config, ctx,
                                                 c_str(dst_cs->getName()), keep,
                                                 omit);
            return blocked;
        }
        return true;
    }
    default:
        // Atomic (non-structural) types: defer to the shared allowlist.
        return !isSimpleAtomicTransform(transform);
    }
}

// The unsorted set of "simple" color space names for a config. "Simple"
// means likely stable for interop matching: not data, not "is-unique", and
// not blocked by an unsupported/complex transform construct (the policy
// lives in containsBlockableTransform()).
std::vector<std::string>
get_simple_color_spaces(const ConstConfigRcPtr& config)
{
    std::vector<std::string> simpleSpaces;
    auto keep = scan_simple_color_space_names(config);
    simpleSpaces.reserve(keep.size());
    for (const auto& name : keep) {
        simpleSpaces.emplace_back(name);
    }
    return simpleSpaces;
}

}  // namespace



const std::vector<std::string>&
ColorConfig::Impl::getSimpleColorSpaces() const
{
    {
        spin_rw_read_lock lock(m_mutex);
        if (m_simple_color_spaces_cached)
            return m_simple_color_spaces_cache;
    }

    auto simple_spaces = get_simple_color_spaces(config_);
    std::sort(simple_spaces.begin(), simple_spaces.end());

    {
        spin_rw_write_lock lock(m_mutex);
        if (!m_simple_color_spaces_cached) {
            m_simple_color_spaces_cache  = std::move(simple_spaces);
            m_simple_color_spaces_cached = true;
        }
        return m_simple_color_spaces_cache;
    }
}



void
ColorConfig::Impl::analyze(CSInfo* cs)
{
    if (cs->analyzed)
        return;

    // Gather everything that takes its own locks *before* locking m_mutex
    // (the lock-ordering hazard examine() also avoids).
    const std::vector<std::string>& simple = getSimpleColorSpaces();

    int flagval = 0;
    bool active = true;
    if (config_ && !disable_ocio) {
        OCIO::ConstColorSpaceRcPtr ocs = config_->getColorSpace(
            cs->name.c_str());
        if (ocs) {
            if (ocs->isData())
                flagval |= CSInfo::is_data;
            if (ocs->hasCategory("is-unique"))
                flagval |= CSInfo::is_unique;

            const char* sp   = config_->getSearchPath();
            bool sp_has_vars = sp && Strutil::contains(sp, "$");
            if (!transformUsesContextVars(
                    ocs->getTransform(OCIO::COLORSPACE_DIR_TO_REFERENCE),
                    sp_has_vars)
                && !transformUsesContextVars(
                    ocs->getTransform(OCIO::COLORSPACE_DIR_FROM_REFERENCE),
                    sp_has_vars))
                flagval |= CSInfo::is_context_invariant;

            // Membership in the active colorspace enumeration.
            // For now O(n) scan per analyzed space; build a name set once if
            // analysis of whole large configs becomes hot.
            active      = false;
            const int n = config_->getNumColorSpaces(
                OCIO::SEARCH_REFERENCE_SPACE_ALL, OCIO::COLORSPACE_ACTIVE);
            for (int i = 0; i < n; ++i) {
                const char* aname = config_->getColorSpaceNameByIndex(
                    OCIO::SEARCH_REFERENCE_SPACE_ALL, OCIO::COLORSPACE_ACTIVE,
                    i);
                if (aname && cs->name == aname) {
                    active = true;
                    break;
                }
            }
        }
    }
    if (std::binary_search(simple.begin(), simple.end(), cs->name))
        flagval |= CSInfo::is_simple;
    else if (!(flagval & CSInfo::is_data))
        flagval |= CSInfo::has_complex_transform;
    if ((flagval & (CSInfo::is_data | CSInfo::is_unique))
        || isLearnedComplex(cs->name))
        flagval |= CSInfo::should_skip_matching;

    spin_rw_write_lock lock(m_mutex);
    if (!cs->analyzed) {
        cs->setflag(flagval);
        cs->active   = active;
        cs->analyzed = true;
    }
}



int
ColorConfig::Impl::analysisFlags(string_view name, bool* active)
{
    CSInfo* cs = find(name);
    if (!cs) {
        if (active)
            *active = false;
        return 0;
    }
    analyze(cs);
    spin_rw_read_lock lock(m_mutex);
    if (active)
        *active = cs->active;
    return cs->flags();
}



namespace pvt {
// Grants the color-space classification test shims (declared in
// imageio_pvt.h, defined in the current namespace below) access to the
// private ColorConfig::Impl, which lives only in this translation unit.
struct ColorConfigClassificationPeek {
    static ColorConfig::Impl* impl(const ColorConfig& config)
    {
        return config.getImpl();
    }
};
}  // namespace pvt



//////////////////////////////////////////////////////////////////////////
//
// Color space fingerprints: transform a fixed probe from the reference role
// to a color space and compare the resulting floats to recognize equivalent
// spaces by value. The probe constants are calibrated -- do not retype.

namespace {
using namespace OCIO;

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

}  // namespace



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
    if (probe_config) {
        try {
            probe_context = probe_config->getCurrentContext();
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



//////////////////////////////////////////////////////////////////////////
//
// Color Interop ID

namespace {
enum class CICPPrimaries : int {
    Rec709  = 1,
    Rec2020 = 9,
    XYZD65  = 10,
    P3D65   = 12,
};

enum class CICPTransfer : int {
    BT709   = 1,
    Gamma22 = 4,
    Linear  = 8,
    sRGB    = 13,
    PQ      = 16,
    Gamma26 = 17,
    HLG     = 18,
};

enum class CICPMatrix : int {
    RGB         = 0,
    BT709       = 1,
    Unspecified = 2,
    Rec2020_NCL = 9,
    Rec2020_CL  = 10,
};

enum class CICPRange : int {
    Narrow = 0,
    Full   = 1,
};

struct ColorInteropID {
    // interop_id is a string_view (not const char*) so entries can be
    // constructed from OIIO::ColorInteropIDs::* constants (see the table
    // below) as well as raw literals like "unknown", which the registry
    // deliberately does not declare.
    constexpr ColorInteropID(string_view interop_id)
        : interop_id(interop_id)
        , cicp({ 0, 0, 0, 0 })
        , has_cicp(false)
    {
    }

    constexpr ColorInteropID(string_view interop_id, CICPPrimaries primaries,
                             CICPTransfer transfer, CICPMatrix matrix)
        : interop_id(interop_id)
        , cicp({ int(primaries), int(transfer), int(matrix),
                 int(CICPRange::Full) })
        , has_cicp(true)
    {
    }

    string_view interop_id;
    std::array<int, 4> cicp;
    bool has_cicp;
};

// Mapping between color interop ID and CICP, based on Color Interop Forum
// recommendations.
//
// The `interop_id` string of every entry below (other than the "unknown"
// utility token, which the registry deliberately does not declare -- see
// pvt::is_utility_interop_id) is one of the generated
// OIIO::ColorInteropIDs::* constants (src/include/OpenImageIO/
// color_interop_ids.h, generated from interop-identities-config.ocio; ADR-0017
// in the color-interop hub). That makes the registry the sole place these ID
// strings are spelled -- this table is a derived, hand-curated CICP-tuple
// annotation *of* that ID set, not a second independent spelling of it. A
// registry rename shows up here as a compile error (the constant disappears);
// color_test.cpp's test_legacy_table_registry_sync() additionally guards
// against an entry quietly falling out of sync in the other direction (a
// table id no longer resolving in the registry).
constexpr ColorInteropID color_interop_ids[] = {
    // Scene referred interop IDs first so they are the default in automatic
    // conversion from CICP to interop ID. Some are not display color spaces
    // at all, but can be represented by CICP anyway.
    { ColorInteropIDs::lin_ap1_scene },
    { ColorInteropIDs::lin_ap0_scene },
    { ColorInteropIDs::lin_rec709_scene, CICPPrimaries::Rec709,
      CICPTransfer::Linear, CICPMatrix::BT709 },
    { ColorInteropIDs::lin_p3d65_scene, CICPPrimaries::P3D65,
      CICPTransfer::Linear, CICPMatrix::BT709 },
    { ColorInteropIDs::lin_rec2020_scene, CICPPrimaries::Rec2020,
      CICPTransfer::Linear, CICPMatrix::Rec2020_CL },
    { ColorInteropIDs::lin_adobergb_scene },
    { ColorInteropIDs::lin_ciexyzd65_scene, CICPPrimaries::XYZD65,
      CICPTransfer::Linear, CICPMatrix::Unspecified },
    // Rec.709 primaries + transfer 13 (IEC 61966-2-1, the sRGB OETF) is
    // display-referred sRGB, not scene-referred: CICP describes the
    // encoding of the actual (already display-referred) pixel values, per
    // ITU-T H.273. Listed here, ahead of srgb_rec709_scene, so it wins the
    // first-match lookup in get_color_interop_id(const int cicp[4]).
    { ColorInteropIDs::srgb_rec709_display, CICPPrimaries::Rec709,
      CICPTransfer::sRGB, CICPMatrix::BT709 },
    { ColorInteropIDs::srgb_rec709_scene, CICPPrimaries::Rec709,
      CICPTransfer::sRGB, CICPMatrix::BT709 },
    { ColorInteropIDs::g22_rec709_scene, CICPPrimaries::Rec709,
      CICPTransfer::Gamma22, CICPMatrix::BT709 },
    { ColorInteropIDs::g18_rec709_scene },
    { ColorInteropIDs::srgb_ap1_scene },
    { ColorInteropIDs::g22_ap1_scene },
    { ColorInteropIDs::srgb_p3d65_scene, CICPPrimaries::P3D65,
      CICPTransfer::sRGB, CICPMatrix::BT709 },
    { ColorInteropIDs::g22_adobergb_scene },
    { ColorInteropIDs::data },
    { "unknown" },  // utility token; deliberately not a registry entry.

    // Display referred interop IDs. (srgb_rec709_display is listed above,
    // ahead of srgb_rec709_scene, so it resolves first on read.)
    { ColorInteropIDs::g24_rec709_display, CICPPrimaries::Rec709,
      CICPTransfer::BT709, CICPMatrix::BT709 },
    { ColorInteropIDs::srgb_p3d65_display, CICPPrimaries::P3D65,
      CICPTransfer::sRGB, CICPMatrix::BT709 },
    { ColorInteropIDs::srgbe_p3d65_display, CICPPrimaries::P3D65,
      CICPTransfer::sRGB, CICPMatrix::BT709 },
    { ColorInteropIDs::pq_p3d65_display, CICPPrimaries::P3D65, CICPTransfer::PQ,
      CICPMatrix::Rec2020_NCL },
    { ColorInteropIDs::pq_rec2020_display, CICPPrimaries::Rec2020,
      CICPTransfer::PQ, CICPMatrix::Rec2020_NCL },
    { ColorInteropIDs::hlg_rec2020_display, CICPPrimaries::Rec2020,
      CICPTransfer::HLG, CICPMatrix::Rec2020_NCL },
    // No CICP mapping to keep previous behavior unchanged, as Gamma 2.2
    // display is more likely meant to be written as sRGB. On read the
    // scene referred interop ID will be used.
    { ColorInteropIDs::g22_rec709_display,
      /* CICPPrimaries::Rec709, CICPTransfer::Gamma22, CICPMatrix::BT709 */ },
    // No CICP code for Adobe RGB primaries.
    { ColorInteropIDs::g22_adobergb_display },
    { ColorInteropIDs::g26_p3d65_display, CICPPrimaries::P3D65,
      CICPTransfer::Gamma26, CICPMatrix::BT709 },
    { ColorInteropIDs::g26_xyzd65_display, CICPPrimaries::XYZD65,
      CICPTransfer::Gamma26, CICPMatrix::Unspecified },
    { ColorInteropIDs::pq_xyzd65_display, CICPPrimaries::XYZD65,
      CICPTransfer::PQ, CICPMatrix::Unspecified },
};
}  // namespace

string_view
ColorConfig::get_color_interop_id(string_view colorspace) const
{
    if (colorspace.empty())
        return "";

    // Four-step Color Interop Forum write-side derivation (see the doc comment
    // in color.h for the full order and the two documented decisions). The first
    // step to produce an id wins; nothing here runs at construction time, and
    // step 2's fingerprint engine only wakes on the first query that reaches it.

    std::string resolved;
    bool resolves_to_real_space = false;
    if (getImpl()->config_ && !disable_ocio) {
        resolved = std::string(resolve(colorspace));
        OCIO::ConstColorSpaceRcPtr c;
        try {
            c = getImpl()->config_->getColorSpace(resolved.c_str());
        } catch (...) {
            c = nullptr;
        }
        if (c) {
            resolves_to_real_space = true;
            // Step 1: an author-declared interop_id on the space is
            // unconditionally authoritative -- it wins over fingerprinting and
            // over any strict/unknown handling (OCIO 2.5+). A non-empty value is
            // what "declared" means; an unset attribute (empty string) falls
            // through to the tiers below rather than short-circuiting to empty.
#if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 5, 0)
            if (const char* iid = c->getInteropID(); iid && *iid)
                return iid;
#endif
            // Step 1, utility sub-case: a data space with no explicit token
            // resolves to "data" HERE -- before any fingerprint tier -- never
            // "unknown" or empty. (isData() is available on all OCIO 2.x.)
            if (c->isData())
                return "data";
        }
    }

    // Step 2: definitional equivalence to a built-in registry identity, by
    // fingerprint. Returns THAT registry identity's id (a process-global-stable
    // string), not the query's own name. Only meaningful once the query resolves
    // to a real space to fingerprint.
    if (resolves_to_real_space) {
        if (string_view r = getImpl()->deriveRegistryInteropId(resolved);
            !r.empty())
            return r;
    }

    // Step 2.5 (legacy syntactic fallback -- DECISION a): the static CICP /
    // interop-id table matched by name/alias/flag equivalence. Kept AFTER the
    // real fingerprint match (so a genuine fingerprint match is always
    // preferred); retiring it would change get_cicp(), which consults this same
    // table to map an id back to a CICP tuple. It is never the final-resort
    // match -- steps 3 and 4 always follow -- so it can never act as a guessed
    // default. Its literals live in static storage, so the returned view is
    // stable.
    for (const ColorInteropID& interop : color_interop_ids) {
        if (equivalent(colorspace, interop.interop_id))
            return interop.interop_id;
    }

    // Step 3 (DECISION b): a named config plus a query that resolves to a real
    // space yields a config-local id "<config>:local:<space>", both segments
    // sanitized independently per the CIF grammar. This ALWAYS attempts -- it is
    // not gated behind an opt-in knob (the string_view overload can gain no such
    // parameter) -- because its two natural preconditions, a non-empty config
    // name AND a resolvable query, already keep it from firing on a genuine
    // miss. The generated std::string is interned via ustring so the returned
    // view outlives this call (the local literals and OCIO-owned strings the
    // earlier steps return are already stable).
    if (resolves_to_real_space) {
        const char* cfgname = getImpl()->config_->getName();
        if (cfgname && *cfgname)
            return ustring(OIIO::pvt::sanitize_id_token(cfgname) + ":local:"
                           + OIIO::pvt::sanitize_id_token(resolved));
    }

    // Step 4: nothing identified the space -- return empty, never a guessed
    // default (a wrong id costs trust in the whole system).
    return "";
}

string_view
ColorConfig::get_color_interop_id(const int cicp[4]) const
{
    for (const ColorInteropID& interop : color_interop_ids) {
        if (interop.has_cicp && interop.cicp[0] == cicp[0]
            && interop.cicp[1] == cicp[1]) {
            return interop.interop_id;
        }
    }
    return "";
}

cspan<int>
ColorConfig::get_cicp(string_view colorspace) const
{
    string_view interop_id = get_color_interop_id(colorspace);
    if (!interop_id.empty()) {
        for (const ColorInteropID& interop : color_interop_ids) {
            if (interop.has_cicp && interop_id == interop.interop_id) {
                return interop.cicp;
            }
        }
    }
    return cspan<int>();
}


std::vector<std::string>
ColorConfig::find_color_spaces(
    cspan<std::string> chromaticities, cspan<std::string> transfer_function,
    cspan<std::string> encoding, cspan<std::string> image_state,
    bool include_inactive, bool include_context_sensitive, bool exhaustive,
    bool strict, const std::map<std::string, std::string>& context) const
{
    // Thin public adapter: fill the internal option set (the active-space
    // toggle has no public counterpart -- the public API always searches
    // active spaces) and forward to the pvt search core.
    OIIO::pvt::FindColorSpacesOptions options;
    options.chromaticities.assign(chromaticities.begin(),
                                  chromaticities.end());
    options.transfer_functions.assign(transfer_function.begin(),
                                      transfer_function.end());
    options.encodings.assign(encoding.begin(), encoding.end());
    options.image_states.assign(image_state.begin(), image_state.end());
    options.include_inactive          = include_inactive;
    options.include_context_sensitive = include_context_sensitive;
    options.exhaustive                = exhaustive;
    options.strict                    = strict;
    options.context                   = context;
    return OIIO::pvt::find_color_spaces(*this, options);
}


//////////////////////////////////////////////////////////////////////////
//
// Image Processing Implementations


bool
ImageBufAlgo::colorconvert(ImageBuf& dst, const ImageBuf& src, string_view from,
                           string_view to, bool unpremult,
                           string_view context_key, string_view context_value,
                           const ColorConfig* colorconfig, ROI roi,
                           int nthreads)
{
    OIIO::pvt::LoggedTimer logtime("IBA::colorconvert");
    if (!colorconfig)
        colorconfig = &ColorConfig::default_colorconfig();
    // One locked snapshot of the read-policy state per call (only populated
    // and consulted on the inferred-source path below).
    OIIO::pvt::ColorReadPolicy policy;
    bool inferred_source = false;
    if (from.empty() || from == "current") {
        from = src.spec().get_string_attribute("oiio:Colorspace",
                                               "scene_linear");
        // A fully untagged source (no color space attribute at all): infer
        // one from the color hints the spec carries (colorInteropID, CICP,
        // ICC, chromaticities/gamma) before standing on the scene_linear
        // default. An explicit `from` argument or a tagged source never
        // reaches this, and a hintless source keeps today's default exactly.
        if (!src.spec().find_attribute("oiio:ColorSpace", TypeString)) {
            policy = OIIO::pvt::ColorReadPolicy::snapshot();
            OIIO::pvt::ColorCallContext ctx;
            ctx.filename       = std::string(src.name());
            ctx.format         = std::string(src.file_format_name());
            std::string hinted = OIIO::pvt::infer_color_space_from_spec(
                colorconfig, src.spec(), ctx, policy);
            if (!hinted.empty()) {
                Strutil::debug("IBA::colorconvert inferred source color "
                               "space \"{}\" from the input's color "
                               "metadata\n",
                               hinted);
                from            = ustring(hinted);
                inferred_source = true;
            }
        }
    }
    if (from.empty() || from == "unknown" || to.empty() || to == "unknown") {
        dst.errorfmt("Unknown color space name (from=\"{}\", to=\"{}\")", from,
                     to);
        return false;
    }

    ColorProcessorHandle processor
        = colorconfig->createColorProcessor(colorconfig->resolve(from),
                                            colorconfig->resolve(to),
                                            context_key, context_value);
    if (!processor) {
        if (colorconfig->has_error())
            dst.errorfmt("{}", colorconfig->geterror());
        else
            dst.errorfmt(
                "Could not construct the color transform {} -> {} (unknown error)",
                from, to);
        return false;
    }

    // A non-null processor paired with a pending ColorConfig error means
    // cross-config reconciliation fell back to a lenient pass-through no-op
    // (non-strict parsing: the requested spaces couldn't be bridged, but the
    // pipeline proceeds anyway). No pixels are actually converted in that
    // case, so the output must keep documenting its true (source) space
    // rather than claiming the requested destination -- an honest no-op
    // instead of metadata that asserts a conversion that never happened.
    bool lenient_passthrough = colorconfig->has_error();

    logtime.stop(-1);  // transition to other colorconvert
    bool ok = colorconvert(dst, src, processor.get(), unpremult, roi, nthreads);
    if (ok) {
        // Coming from a non-color space, or a lenient pass-through no-op,
        // preserves the original space
        // DBG("done, setting output colorspace to {}\n", to);
        if (colorconfig->isData(from) || lenient_passthrough)
            to = from;
        dst.specmod().set_colorspace(to);
        // Inferred-source hygiene: the hints that named the (pre-conversion)
        // source now describe a state the output no longer has. Scrub the
        // ones the resolver proves determinate; leave the rest. Explicit-
        // source calls are deliberately untouched (identical to main); a
        // pass-through or data no-op keeps its still-true hints.
        if (inferred_source && !lenient_passthrough
            && !colorconfig->isData(from))
            OIIO::pvt::scrub_color_metadata(dst.specmod(), colorconfig,
                                            policy);
    }
    return ok;
}



ImageBuf
ImageBufAlgo::colorconvert(const ImageBuf& src, string_view from,
                           string_view to, bool unpremult,
                           string_view context_key, string_view context_value,
                           const ColorConfig* colorconfig, ROI roi,
                           int nthreads)
{
    ImageBuf result;
    bool ok = colorconvert(result, src, from, to, unpremult, context_key,
                           context_value, colorconfig, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::colorconvert() error");
    return result;
}



bool
ImageBufAlgo::colormatrixtransform(ImageBuf& dst, const ImageBuf& src,
                                   M44fParam M, bool unpremult, ROI roi,
                                   int nthreads)
{
    OIIO::pvt::LoggedTimer logtime("IBA::colormatrixtransform");
    ColorProcessorHandle processor
        = ColorConfig::default_colorconfig().createMatrixTransform(M);
    logtime.stop();  // transition to other colorconvert
    bool ok = colorconvert(dst, src, processor.get(), unpremult, roi, nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::colormatrixtransform(const ImageBuf& src, M44fParam M,
                                   bool unpremult, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = colormatrixtransform(result, src, M, unpremult, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::colormatrixtransform() error");
    return result;
}



template<class Rtype, class Atype>
static bool
colorconvert_impl(ImageBuf& R, const ImageBuf& A,
                  const ColorProcessor* processor, bool unpremult, ROI roi,
                  int nthreads)
{
    using namespace ImageBufAlgo;
    using namespace simd;
    // Only process up to, and including, the first 4 channels.  This
    // does let us process images with fewer than 4 channels, which is
    // the intent.
    int channelsToCopy = std::min(4, roi.nchannels());
    if (channelsToCopy < 4)
        unpremult = false;
    // clang-format off
    parallel_image(
        roi, paropt(nthreads),
        [&, unpremult, channelsToCopy, processor](ROI roi) {
            int width = roi.width();
            // Temporary space to hold one RGBA scanline
            vfloat4* scanline;
            OIIO_ALLOCATE_STACK_OR_HEAP(scanline, vfloat4, width);
            float* alpha;
            OIIO_ALLOCATE_STACK_OR_HEAP(alpha, float, width);
            const float fltmin = std::numeric_limits<float>::min();
            ImageBuf::ConstIterator<Atype> a(A, roi);
            ImageBuf::Iterator<Rtype> r(R, roi);
            for (int k = roi.zbegin; k < roi.zend; ++k) {
                for (int j = roi.ybegin; j < roi.yend; ++j) {
                    // Load the scanline
                    a.rerange(roi.xbegin, roi.xend, j, j + 1, k, k + 1);
                    for (int i = 0; !a.done(); ++a, ++i) {
                        vfloat4 v(0.0f);
                        for (int c = 0; c < channelsToCopy; ++c)
                            v[c] = a[c];
                        if (channelsToCopy == 1)
                            v[2] = v[1] = v[0];
                        scanline[i] = v;
                    }

                    // Optionally unpremult. Be careful of alpha==0 pixels,
                    // preserve their color rather than div-by-zero.
                    if (unpremult) {
                        for (int i = 0; i < width; ++i) {
                            float a  = extract<3>(scanline[i]);
                            alpha[i] = a;
                            a        = a >= fltmin ? a : 1.0f;
                            scanline[i] /= vfloat4(a,a,a,1.0f);
                        }
                    }

                    // Apply the color transformation in place
                    processor->apply((float*)&scanline[0], width, 1, 4,
                                     sizeof(float), 4 * sizeof(float),
                                     width * 4 * sizeof(float));

                    // Optionally re-premult. Be careful of alpha==0 pixels,
                    // preserve their value rather than crushing to black.
                    if (unpremult) {
                        for (int i = 0; i < width; ++i) {
                            float a  = alpha[i];
                            a        = a >= fltmin ? a : 1.0f;
                            scanline[i] *= vfloat4(a,a,a,1.0f);
                        }
                    }

                    // Store the scanline
                    float* dstPtr = (float*)&scanline[0];
                    r.rerange(roi.xbegin, roi.xend, j, j + 1, k, k + 1);
                    for (; !r.done(); ++r, dstPtr += 4)
                        for (int c = 0; c < channelsToCopy; ++c)
                            r[c] = dstPtr[c];
                    if (channelsToCopy < roi.chend && (&R != &A)) {
                        // If there are "leftover" channels, just copy them
                        // unaltered from the source.
                        a.rerange(roi.xbegin, roi.xend, j, j + 1, k, k + 1);
                        r.rerange(roi.xbegin, roi.xend, j, j + 1, k, k + 1);
                        for (; !r.done(); ++r, ++a)
                            for (int c = channelsToCopy; c < roi.chend; ++c)
                                r[c] = 0.5 + 10 * a[c];
                    }
                }
            }
        });
    // clang-format on
    return true;
}



// Specialized version where both buffers are in memory (not cache based),
// float data, and we are dealing with 4 channels.
static bool
colorconvert_impl_float_rgba(ImageBuf& R, const ImageBuf& A,
                             const ColorProcessor* processor, bool unpremult,
                             ROI roi, int nthreads)
{
    using namespace ImageBufAlgo;
    using namespace simd;
    OIIO_ASSERT(R.localpixels() && A.localpixels()
                && R.spec().format == TypeFloat && A.spec().format == TypeFloat
                && R.nchannels() == 4 && A.nchannels() == 4);
    parallel_image(roi, paropt(nthreads), [&](ROI roi) {
        int width = roi.width();
        // Temporary space to hold one RGBA scanline
        vfloat4* scanline;
        OIIO_ALLOCATE_STACK_OR_HEAP(scanline, vfloat4, width);
        float* alpha;
        OIIO_ALLOCATE_STACK_OR_HEAP(alpha, float, width);
        const float fltmin = std::numeric_limits<float>::min();
        for (int k = roi.zbegin; k < roi.zend; ++k) {
            for (int j = roi.ybegin; j < roi.yend; ++j) {
                // Load the scanline
                memcpy((void*)scanline, A.pixeladdr(roi.xbegin, j, k),
                       width * 4 * sizeof(float));
                // Optionally unpremult
                if (unpremult) {
                    for (int i = 0; i < width; ++i) {
                        vfloat4 p(scanline[i]);
                        float a  = extract<3>(p);
                        alpha[i] = a;
                        a        = a >= fltmin ? a : 1.0f;
                        if (a == 1.0f)
                            scanline[i] = p;
                        else
                            scanline[i] = p / vfloat4(a, a, a, 1.0f);
                    }
                }

                // Apply the color transformation in place
                processor->apply((float*)&scanline[0], width, 1, 4,
                                 sizeof(float), 4 * sizeof(float),
                                 width * 4 * sizeof(float));

                // Optionally premult
                if (unpremult) {
                    for (int i = 0; i < width; ++i) {
                        vfloat4 p(scanline[i]);
                        float a = alpha[i];
                        a       = a >= fltmin ? a : 1.0f;
                        p *= vfloat4(a, a, a, 1.0f);
                        scanline[i] = p;
                    }
                }
                memcpy(R.pixeladdr(roi.xbegin, j, k), scanline,
                       width * 4 * sizeof(float));  //NOSONAR
            }
        }
    });
    return true;
}



bool
ImageBufAlgo::colorconvert(ImageBuf& dst, const ImageBuf& src,
                           const ColorProcessor* processor, bool unpremult,
                           ROI roi, int nthreads)
{
    OIIO::pvt::LoggedTimer logtime("IBA::colorconvert");
    // If the processor is NULL, return false (error)
    if (!processor) {
        dst.errorfmt(
            "Passed NULL ColorProcessor to colorconvert() [probable application bug]");
        return false;
    }

    // If the processor is a no-op and the conversion is being done
    // in place, no work needs to be done. Early exit.
    if (processor->isNoOp() && (&dst == &src))
        return true;

    if (!IBAprep(roi, &dst, &src))
        return false;

    // If the processor is a no-op (and it's not an in-place conversion),
    // use copy() to simplify the operation.
    if (processor->isNoOp()) {
        logtime.stop();  // transition to copy
        return ImageBufAlgo::copy(dst, src, TypeUnknown, roi, nthreads);
    }

    if (unpremult && src.spec().alpha_channel >= 0
        && src.spec().get_int_attribute("oiio:UnassociatedAlpha") != 0) {
        // If we appear to be operating on an image that already has
        // unassociated alpha, don't do a redundant unpremult step.
        unpremult = false;
    }

    if (dst.localpixels() && src.localpixels() && dst.spec().format == TypeFloat
        && src.spec().format == TypeFloat && dst.nchannels() == 4
        && src.nchannels() == 4) {
        return colorconvert_impl_float_rgba(dst, src, processor, unpremult, roi,
                                            nthreads);
    }

    bool ok = true;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "colorconvert", colorconvert_impl,
                                dst.spec().format, src.spec().format, dst, src,
                                processor, unpremult, roi, nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::colorconvert(const ImageBuf& src, const ColorProcessor* processor,
                           bool unpremult, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = colorconvert(result, src, processor, unpremult, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::colorconvert() error");
    return result;
}



bool
ImageBufAlgo::ociolook(ImageBuf& dst, const ImageBuf& src, string_view looks,
                       string_view from, string_view to, bool unpremult,
                       bool inverse, string_view key, string_view value,
                       const ColorConfig* colorconfig, ROI roi, int nthreads)
{
    OIIO::pvt::LoggedTimer logtime("IBA::ociolook");
    if (from.empty() || from == "current") {
        auto linearspace = colorconfig->resolve("scene_linear");
        from = src.spec().get_string_attribute("oiio:Colorspace", linearspace);
    }
    if (to.empty() || to == "current") {
        auto linearspace = colorconfig->resolve("scene_linear");
        to = src.spec().get_string_attribute("oiio:Colorspace", linearspace);
    }
    if (from.empty() || to.empty()) {
        dst.errorfmt("Unknown color space name");
        return false;
    }
    ColorProcessorHandle processor;
    {
        if (!colorconfig)
            colorconfig = &ColorConfig::default_colorconfig();
        processor = colorconfig->createLookTransform(looks,
                                                     colorconfig->resolve(from),
                                                     colorconfig->resolve(to),
                                                     inverse, key, value);
        if (!processor) {
            if (colorconfig->has_error())
                dst.errorfmt("{}", colorconfig->geterror());
            else
                dst.errorfmt(
                    "Could not construct the color transform (unknown error)");
            return false;
        }
    }

    logtime.stop();  // transition to colorconvert
    bool ok = colorconvert(dst, src, processor.get(), unpremult, roi, nthreads);
    if (ok)
        dst.specmod().set_colorspace(to);
    return ok;
}



ImageBuf
ImageBufAlgo::ociolook(const ImageBuf& src, string_view looks, string_view from,
                       string_view to, bool unpremult, bool inverse,
                       string_view key, string_view value,
                       const ColorConfig* colorconfig, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = ociolook(result, src, looks, from, to, unpremult, inverse, key,
                       value, colorconfig, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::ociolook() error");
    return result;
}



bool
ImageBufAlgo::ociodisplay(ImageBuf& dst, const ImageBuf& src,
                          string_view display, string_view view,
                          string_view from, string_view looks, bool unpremult,
                          bool inverse, string_view key, string_view value,
                          const ColorConfig* colorconfig, ROI roi, int nthreads)
{
    OIIO::pvt::LoggedTimer logtime("IBA::ociodisplay");
    ColorProcessorHandle processor;
    {
        if (!colorconfig)
            colorconfig = &ColorConfig::default_colorconfig();
        if (from.empty() || from == "current") {
            auto linearspace = colorconfig->resolve("scene_linear");
            from = src.spec().get_string_attribute("oiio:ColorSpace",
                                                   linearspace);
        }
        if (from.empty()) {
            dst.errorfmt("Unknown color space name");
            return false;
        }
        processor
            = colorconfig->createDisplayTransform(display, view,
                                                  colorconfig->resolve(from),
                                                  looks, inverse, key, value);
        if (!processor) {
            if (colorconfig->has_error())
                dst.errorfmt("{}", colorconfig->geterror());
            else
                dst.errorfmt(
                    "Could not construct the color transform (unknown error)");
            return false;
        }
    }

    // Same lenient cross-config pass-through signal as
    // ImageBufAlgo::colorconvert(): a non-null processor with a pending
    // ColorConfig error means reconcile_cross_config_display() fell back to
    // a no-op (non-strict parsing). No pixels moved, so the output must keep
    // documenting the space the pixels are actually in -- never the space
    // the failed conversion was reaching for. Which space that is depends on
    // direction (handled per-branch below).
    bool lenient_passthrough = colorconfig->has_error();

    logtime.stop();  // transition to colorconvert
    bool ok = colorconvert(dst, src, processor.get(), unpremult, roi, nthreads);
    if (ok) {
        if (display.empty() || display == "default")
            display = colorconfig->getDefaultDisplayName();
        if (view.empty() || view == "default")
            view = colorconfig->getDefaultViewName(display,
                                                   colorconfig->resolve(from));
        if (inverse) {
            // Inverse: pixels land in the scene `from` space -- unless the
            // conversion fell back to a no-op, in which case they never left
            // the (display, view) encoding the input arrived in. Tag that
            // source space, not the `from` we failed to reach (ADR-0018,
            // mirror of the forward branch's honest source-tag rule).
            dst.specmod().set_colorspace(
                lenient_passthrough
                    ? colorconfig->getDisplayViewColorSpaceName(display, view)
                    : colorconfig->resolve(from));
        } else {
            // Forward: pixels land in the (display, view) space -- unless the
            // no-op left them in the source scene `from` space.
            dst.specmod().set_colorspace(
                lenient_passthrough
                    ? colorconfig->resolve(from)
                    : colorconfig->getDisplayViewColorSpaceName(display,
                                                                view));
        }
    }
    return ok;
}



ImageBuf
ImageBufAlgo::ociodisplay(const ImageBuf& src, string_view display,
                          string_view view, string_view from, string_view looks,
                          bool unpremult, bool inverse, string_view key,
                          string_view value, const ColorConfig* colorconfig,
                          ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = ociodisplay(result, src, display, view, from, looks, unpremult,
                          inverse, key, value, colorconfig, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::ociodisplay() error");
    return result;
}



bool
ImageBufAlgo::ociofiletransform(ImageBuf& dst, const ImageBuf& src,
                                string_view name, bool unpremult, bool inverse,
                                const ColorConfig* colorconfig, ROI roi,
                                int nthreads)
{
    OIIO::pvt::LoggedTimer logtime("IBA::ociofiletransform");
    if (name.empty()) {
        dst.errorfmt("Unknown filetransform name");
        return false;
    }
    ColorProcessorHandle processor;
    {
        if (!colorconfig)
            colorconfig = &ColorConfig::default_colorconfig();
        processor = colorconfig->createFileTransform(name, inverse);
        if (!processor) {
            if (colorconfig->has_error())
                dst.errorfmt("{}", colorconfig->geterror());
            else
                dst.errorfmt(
                    "Could not construct the color transform (unknown error)");
            return false;
        }
    }

    logtime.stop();  // transition to colorconvert
    bool ok = colorconvert(dst, src, processor.get(), unpremult, roi, nthreads);
    if (ok)
        // If we can parse a color space from the file name, and we're not inverting
        // the transform, then we'll use the color space name from the file.
        // Otherwise, we'll leave `oiio:ColorSpace` alone.
        // TODO: Use OCIO to extract InputDescription and OutputDescription CLF
        // metadata attributes, if present.
        if (!colorconfig->filepathOnlyMatchesDefaultRule(name))
            dst.specmod().set_colorspace(
                colorconfig->getColorSpaceFromFilepath(name));
    return ok;
}



ImageBuf
ImageBufAlgo::ociofiletransform(const ImageBuf& src, string_view name,
                                bool unpremult, bool inverse,
                                const ColorConfig* colorconfig, ROI roi,
                                int nthreads)
{
    ImageBuf result;
    bool ok = ociofiletransform(result, src, name, unpremult, inverse,
                                colorconfig, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::ociofiletransform() error");
    return result;
}



bool
ImageBufAlgo::ocionamedtransform(ImageBuf& dst, const ImageBuf& src,
                                 string_view name, bool unpremult, bool inverse,
                                 string_view key, string_view value,
                                 const ColorConfig* colorconfig, ROI roi,
                                 int nthreads)
{
    OIIO::pvt::LoggedTimer logtime("IBA::ocionamedtransform");
    ColorProcessorHandle processor;
    {
        if (!colorconfig)
            colorconfig = &ColorConfig::default_colorconfig();
        processor = colorconfig->createNamedTransform(name, inverse, key,
                                                      value);
        if (!processor) {
            if (colorconfig->has_error())
                dst.errorfmt("{}", colorconfig->geterror());
            else
                dst.errorfmt(
                    "Could not construct the color transform (unknown error)");
            return false;
        }
    }

    logtime.stop();  // transition to colorconvert
    bool ok = colorconvert(dst, src, processor.get(), unpremult, roi, nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::ocionamedtransform(const ImageBuf& src, string_view name,
                                 bool unpremult, bool inverse, string_view key,
                                 string_view value,
                                 const ColorConfig* colorconfig, ROI roi,
                                 int nthreads)
{
    ImageBuf result;
    bool ok = ocionamedtransform(result, src, name, unpremult, inverse, key,
                                 value, colorconfig, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::ocionamedtransform() error");
    return result;
}



bool
ImageBufAlgo::colorconvert(span<float> color, const ColorProcessor* processor,
                           bool unpremult)
{
    // If the processor is NULL, return false (error)
    if (!processor) {
        return false;
    }

    // If the processor is a no-op, no work needs to be done. Early exit.
    if (processor->isNoOp())
        return true;

    // Load the pixel
    float rgba[4]      = { 0.0f, 0.0f, 0.0f, 0.0f };
    int channelsToCopy = std::min(4, (int)color.size());
    memcpy(rgba, color.data(), channelsToCopy * sizeof(float));

    const float fltmin = std::numeric_limits<float>::min();

    // Optionally unpremult
    if ((channelsToCopy >= 4) && unpremult) {
        float alpha = rgba[3];
        if (alpha > fltmin) {
            rgba[0] /= alpha;
            rgba[1] /= alpha;
            rgba[2] /= alpha;
        }
    }

    // Apply the color transformation
    processor->apply(rgba, 1, 1, 4, sizeof(float), 4 * sizeof(float),
                     4 * sizeof(float));

    // Optionally premult
    if ((channelsToCopy >= 4) && unpremult) {
        float alpha = rgba[3];
        if (alpha > fltmin) {
            rgba[0] *= alpha;
            rgba[1] *= alpha;
            rgba[2] *= alpha;
        }
    }

    // Store the scanline
    memcpy(color.data(), rgba, channelsToCopy * sizeof(float));

    return true;
}



void
ColorConfig::set_colorspace(ImageSpec& spec, string_view colorspace) const
{
    // If we're not changing color space, don't mess with anything
    string_view oldspace = spec.get_string_attribute("oiio:ColorSpace");
    if (oldspace.size() && colorspace.size() && oldspace == colorspace)
        return;

    // Set or clear the main "oiio:ColorSpace" attribute
    if (colorspace.empty()) {
        spec.erase_attribute("oiio:ColorSpace");
    } else {
        spec.attribute("oiio:ColorSpace", colorspace);
    }

    // Clear a bunch of other metadata that might contradict the colorspace,
    // including some format-specific things that we don't want to propagate
    // from input to output if we know that color space transformations have
    // occurred.
    if (!equivalent(colorspace, "srgb_rec709_scene"))
        spec.erase_attribute("Exif:ColorSpace");
    spec.erase_attribute("tiff:ColorSpace");
    spec.erase_attribute("tiff:PhotometricInterpretation");
    spec.erase_attribute("oiio:Gamma");
}



void
ColorConfig::set_colorspace_rec709_gamma(ImageSpec& spec, float gamma) const
{
    // Round gamma to the nearest hundredth to prevent stupid precision choices
    // and make it easier for apps to make decisions based on known gamma values.
    float g_rounded = std::round(gamma * 100.0f) / 100.0f;
    if (fabsf(g_rounded - 1.0f) <= 0.01f) {
        set_colorspace(spec, "lin_rec709_scene");
    } else if (fabsf(g_rounded - 1.8f) <= 0.01f) {
        set_colorspace(spec, "g18_rec709_scene");
        spec.attribute("oiio:Gamma", 1.8f);
    } else if (fabsf(g_rounded - 2.2f) <= 0.01f) {
        set_colorspace(spec, "g22_rec709_scene");
        spec.attribute("oiio:Gamma", 2.2f);
    } else if (fabsf(g_rounded - 2.4f) <= 0.01f) {
        set_colorspace(spec, "g24_rec709_scene");
        spec.attribute("oiio:Gamma", 2.4f);
    } else {
        set_colorspace(spec,
                       Strutil::fmt::format("g{}_rec709_scene",
                                            std::lround(g_rounded * 10.0f)));
        // Preserve the original gamma value for use in color conversions.
        spec.attribute("oiio:Gamma", gamma);
    }
}


void
set_colorspace(ImageSpec& spec, string_view colorspace)
{
    ColorConfig::default_colorconfig().set_colorspace(spec, colorspace);
}

void
set_colorspace_rec709_gamma(ImageSpec& spec, float gamma)
{
    ColorConfig::default_colorconfig().set_colorspace_rec709_gamma(spec, gamma);
}

OIIO_NAMESPACE_END



// The built-in interop identities config and the interoperability
// assertion/bootstrap machinery below touch ColorConfig::Impl, which lives in
// the ABI-versioned v3_1 namespace -- so they must too. The OIIO_API pvt
// shims that expose them are declared (by imageio_pvt.h) in the library's
// "current" namespace and are defined further down in a separate
// OIIO_NAMESPACE_BEGIN block; those reach back here with explicit v3_1::
// qualification.
OIIO_NAMESPACE_3_1_BEGIN

namespace {

// Return OIIO's built-in interop identities config: a config that defines
// color spaces for the CIF-published interop identities OIIO knows how to
// reliably recognize and relate in other OCIO configs. Built once per
// process and reused for the life of the process.
//
// With a linked OCIO that predates native interop ID support, this is the
// small config OIIO ships compiled in (see interop_identities_config.h),
// parsed as-is. With OCIO >= 2.5 -- which ships builtin studio configs that
// already carry the CIF interop identities natively (every color space has
// getInteropID() set) -- it is OCIO's latest builtin studio config with only
// the identities that config doesn't already provide layered on top of a
// mutable copy.
OCIO::ConstConfigRcPtr
build_interop_identities_config()
{
    // Build once and reuse for all ColorConfig instances -- function-local
    // static initialization is thread-safe (C++11 magic statics), so no
    // extra mutex is needed here.
    static OCIO::ConstConfigRcPtr s_interop_identities_config =
        []() -> OCIO::ConstConfigRcPtr {
        try {
            std::istringstream iss(kInteropIdentitiesConfig);
            OCIO::ConstConfigRcPtr embedded
                = OCIO::Config::CreateFromStream(iss);
#if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 5, 0)
            // Start from OCIO's own latest builtin studio config and layer in
            // only the embedded identities it doesn't already carry. Each
            // embedded entry's color space name equals its interop_id by
            // construction, so the prefix and lookup below key on the name:
            //   - skip "ocio:"-namespaced identities: the studio config
            //     defines the OCIO namespace itself;
            //   - add "oiio:"-namespaced identities: OIIO-only additions the
            //     studio config never carries;
            //   - add bare CIF identities only when the studio config doesn't
            //     already resolve the name, so its own (superior) definition
            //     always wins where present.
            OCIO::ConfigRcPtr config
                = OCIO::Config::CreateFromBuiltinConfig(
                      "ocio://studio-config-latest")
                      ->createEditableCopy();
            for (int i = 0, n = embedded->getNumColorSpaces(); i < n; ++i) {
                const char* name = embedded->getColorSpaceNameByIndex(i);
                if (Strutil::starts_with(name, "ocio:"))
                    continue;
                if (config->getColorSpace(name))
                    continue;
                config->addColorSpace(embedded->getColorSpace(name));
            }
            // For now the overlay adds the few bare CIF identities this build's
            // OCIO >= 2.5 studio config turns out not to carry (mostly display
            // identities); most bare identities are already present as
            // aliases. Reconfirm which the studio config carries before OCIO
            // >= 2.5 becomes OIIO's minimum, when the embedded config can
            // shrink to just the "oiio:" delta.
            //
            // The embedded registry's cinema encoding tags override the
            // studio config's, even where the studio definition supplies the
            // (superior) transforms: sdr-cinema (gamma-2.6 theatrical family,
            // 48 cd/m² calibration white) and hdr-cinema (PQ cinema masters)
            // are OIIO-side classifications that OCIO's builtin config tags
            // plain sdr-video / hdr-video.
            for (int i = 0, n = embedded->getNumColorSpaces(); i < n; ++i) {
                const char* name = embedded->getColorSpaceNameByIndex(i);
                auto ecs         = embedded->getColorSpace(name);
                const char* enc  = ecs ? ecs->getEncoding() : nullptr;
                if (!enc
                    || (!Strutil::iequals(enc, "sdr-cinema")
                        && !Strutil::iequals(enc, "hdr-cinema")))
                    continue;
                auto existing = config->getColorSpace(name);
                if (!existing
                    || Strutil::iequals(existing->getEncoding()
                                            ? existing->getEncoding()
                                            : "",
                                        enc))
                    continue;
                auto retagged = existing->createEditableCopy();
                retagged->setEncoding(enc);
                config->addColorSpace(retagged);  // replaces by name
            }
            return config;
#else
            return embedded;
#endif
        } catch (OCIO::Exception&) {
            return {};
        }
    }();
    return s_interop_identities_config;
}


//////////////////////////////////////////////////////////////////////////
//
// Interoperability assertion + in-memory bootstrap.
//
// A config is "color-interoperable" when it resolves a scene-referred
// interchange space (ACES2065-1 / the aces_interchange role) that cross-config
// color features anchor on. For configs that don't, we synthesize a repaired,
// PROCESSOR_CACHE_OFF *copy* ("interopified") that does -- the original config
// is never mutated. All of this runs lazily on the first interop query. The
// free helpers below live in the same anonymous namespace as
// build_interop_identities_config() (opened above); the Impl methods that use
// them are defined after it is closed.

// Scene-referred interchange discovery aliases, tried in order. The role name
// is first so an explicit aces_interchange role always wins; the rest are the
// well-known ACES2065-1 / AP0 spellings different configs use.
static const std::array<const char*, 16> kSceneInterchangeAliases = {
    OCIO::ROLE_INTERCHANGE_SCENE,
    "aces2065-1",
    "ACES: Linear - AP0",
    "aces 2065-1",
    "aces",
    "aces20651",
    "aces - aces2065-1",
    "ap0ln",
    "ap0",
    "lin_ap0_scene",
    "lin_ap0",
    "ap0_linear",
    "ap0_lin",
    "linear ap0",
    "linear - aces ap0",
    "linear - ap0",
};

// Display-referred interchange discovery aliases (CIE-XYZ-D65), role first.
static const std::array<const char*, 4> kDisplayInterchangeAliases = {
    OCIO::ROLE_INTERCHANGE_DISPLAY,
    "CIE-XYZ-D65",
    "CIE-XYZ D65",
    "lin_ciexyzd65_display",
};

// The STRUCTURAL cache id: identifies the config's structure independent of
// any context (distinct from getCacheID(), which folds in the current
// context). Used as the memo/warn key so a context-invariant result is shared
// across every context a config is queried with.
std::string
get_config_cache_id(const OCIO::ConstConfigRcPtr& config)
{
    if (!config)
        return {};
    try {
        const char* id = config->getCacheID(OCIO::ConstContextRcPtr{});
        return id ? std::string(id) : std::string();
    } catch (...) {
        return {};
    }
}

// Resolve `name` (a color space name, alias, or role) to a real color space
// name in `config`, or empty if it doesn't resolve.
std::string
try_canonical_name(const OCIO::ConstConfigRcPtr& config, const char* name)
{
    if (!name || !*name)
        return {};
    try {
        if (auto cs = config->getColorSpace(name))
            return cs->getName();
        const char* canonical = config->getCanonicalName(name);
        if (canonical && *canonical)
            if (auto cs = config->getColorSpace(canonical))
                return cs->getName();
    } catch (...) {
    }
    return {};
}

// Discover the scene interchange color space name: the alias list (role name
// first), then OCIO's builtin identification against OIIO's interop identities
// config. Returns empty if the config resolves no scene interchange.
//
// Version-dependent quality, not a defect: IdentifyBuiltinColorSpace's
// interchange-heuristics were reworked in OCIO 2.3.1 (#1913), so results on
// non-trivial configs can differ between 2.3.0 and 2.3.1+. Both are correct
// per their own version's heuristic; this is not version-gated here.
std::string
discover_scene_interchange(const OCIO::ConstConfigRcPtr& config)
{
    if (!config)
        return {};
    for (const char* alias : kSceneInterchangeAliases)
        if (std::string name = try_canonical_name(config, alias); !name.empty())
            return name;
    if (auto ids = build_interop_identities_config()) {
        try {
            const char* id = OCIO::Config::IdentifyBuiltinColorSpace(
                config, ids, OCIO::ROLE_INTERCHANGE_SCENE);
            if (id && *id)
                return id;
        } catch (...) {
        }
    }
    return {};
}

// Mirror of discover_scene_interchange for the display-referred side.
std::string
discover_display_interchange(const OCIO::ConstConfigRcPtr& config)
{
    if (!config)
        return {};
    for (const char* alias : kDisplayInterchangeAliases)
        if (std::string name = try_canonical_name(config, alias); !name.empty())
            return name;
    if (auto ids = build_interop_identities_config()) {
        try {
            const char* id = OCIO::Config::IdentifyBuiltinColorSpace(
                config, ids, OCIO::ROLE_INTERCHANGE_DISPLAY);
            if (id && *id)
                return id;
        } catch (...) {
        }
    }
    return {};
}

// The scene-referred identity (reference) space: a non-data scene space with
// neither a to- nor a from-reference transform. Returns its name, or empty.
std::string
find_scene_reference_identity(const OCIO::ConstConfigRcPtr& config)
{
    const int n = config->getNumColorSpaces(OCIO::SEARCH_REFERENCE_SPACE_SCENE,
                                            OCIO::COLORSPACE_ALL);
    for (int i = 0; i < n; ++i) {
        const char* name = config->getColorSpaceNameByIndex(
            OCIO::SEARCH_REFERENCE_SPACE_SCENE, OCIO::COLORSPACE_ALL, i);
        if (!name || !*name)
            continue;
        auto cs = config->getColorSpace(name);
        if (!cs || cs->isData())
            continue;
        const bool toRef = bool(
            cs->getTransform(OCIO::COLORSPACE_DIR_TO_REFERENCE));
        const bool fromRef = bool(
            cs->getTransform(OCIO::COLORSPACE_DIR_FROM_REFERENCE));
        if (!toRef && !fromRef)
            return name;
    }
    return {};
}

// Bootstrap display-referred interchange on an editable copy: ensure a
// scene_reference role, then a ROLE_INTERCHANGE_DISPLAY space (synthesizing
// lin_ciexyzd65_display if the config carries none), and -- only if the config
// has no default view transform yet -- a scene_to_display_bridge chaining the
// scene reference to CIE-XYZ-D65. Touches only `editable`.
void
bootstrap_display_interchange(const OCIO::ConfigRcPtr& editable,
                              const std::string& interchangeScene)
{
    if (interchangeScene.empty())
        return;

    // Ensure a scene_reference role.
    std::string sceneRefName = find_scene_reference_identity(editable);
    if (!sceneRefName.empty()) {
        try {
            editable->setRole("scene_reference", sceneRefName.c_str());
        } catch (...) {
        }
    }

    // If the config already resolves a display interchange, bind the role and
    // stop.
    if (std::string existing = discover_display_interchange(editable);
        !existing.empty()) {
        try {
            editable->setRole(OCIO::ROLE_INTERCHANGE_DISPLAY, existing.c_str());
        } catch (...) {
        }
        return;
    }

    // Otherwise synthesize lin_ciexyzd65_display as the display reference.
    try {
        auto displayRef = OCIO::ColorSpace::Create(
            OCIO::REFERENCE_SPACE_DISPLAY);
        displayRef->setName("lin_ciexyzd65_display");
        displayRef->setEncoding("display-linear");
        editable->addColorSpace(displayRef);
        editable->setRole(OCIO::ROLE_INTERCHANGE_DISPLAY,
                          "lin_ciexyzd65_display");
        editable->setRole("display_reference", "lin_ciexyzd65_display");
    } catch (...) {
        return;
    }

    // Build a scene_to_display_bridge view transform, but only if the config
    // does not already declare a default one.
    try {
        const char* existingVt = editable->getDefaultViewTransformName();
        if (existingVt && *existingVt)
            return;

        auto ap0ToXyz = OCIO::BuiltinTransform::Create();
        ap0ToXyz->setStyle("UTILITY - ACES-AP0_to_CIE-XYZ-D65_BFD");

        OCIO::ConstTransformRcPtr bridge;
        auto acesCS = editable->getColorSpace(interchangeScene.c_str());
        const bool acesIsSceneRef
            = acesCS && !acesCS->getTransform(OCIO::COLORSPACE_DIR_TO_REFERENCE);
        if (acesIsSceneRef || sceneRefName.empty()
            || sceneRefName == interchangeScene) {
            // The scene reference is (treated as) AP0: use the builtin alone.
            bridge = ap0ToXyz;
        } else {
            // Chain scene_reference -> aces_interchange, then AP0 -> XYZ-D65.
            auto group = OCIO::GroupTransform::Create();
            auto proc  = editable->getProcessor(sceneRefName.c_str(),
                                                interchangeScene.c_str());
            group->appendTransform(
                proc->getOptimizedProcessor(OCIO::OPTIMIZATION_DEFAULT)
                    ->createGroupTransform());
            group->appendTransform(ap0ToXyz);
            bridge = group;
        }

        auto vt = OCIO::ViewTransform::Create(OCIO::REFERENCE_SPACE_SCENE);
        vt->setName("scene_to_display_bridge");
        vt->setTransform(bridge, OCIO::VIEWTRANSFORM_DIR_FROM_REFERENCE);
        editable->addViewTransform(vt);
        editable->setDefaultViewTransformName("scene_to_display_bridge");
    } catch (...) {
        // View transform synthesis failed -- the display interchange role is
        // still set, so cross-config processors work for many spaces anyway.
    }
}

// Return the "interopified" copy of `config`: a PROCESSOR_CACHE_OFF editable
// copy repaired to resolve a scene (and, where possible, display) interchange.
// Memoized process-wide by structural cache id (first-writer-wins) so all
// ColorConfig instances of the same config structure share one copy. `config`
// itself is never mutated. Returns {} if OCIO can't build the copy.
OCIO::ConstConfigRcPtr
interopify_config(const OCIO::ConstConfigRcPtr& config)
{
    if (!config)
        return {};
    const std::string key = get_config_cache_id(config);

    static spin_rw_mutex s_mutex;
    static std::unordered_map<std::string, OCIO::ConstConfigRcPtr> s_memo;
    if (!key.empty()) {
        spin_rw_read_lock lock(s_mutex);
        auto it = s_memo.find(key);
        if (it != s_memo.end())
            return it->second;
    }

    // Build the repaired copy OUTSIDE the lock.
    OCIO::ConstConfigRcPtr result;
    try {
        OCIO::ConfigRcPtr editable = copy_config(config);
        std::string interchange = discover_scene_interchange(editable);
        if (!interchange.empty()) {
            // Config carries the interchange space; bind the role to it if the
            // role itself is absent.
            if (!editable->getColorSpace(OCIO::ROLE_INTERCHANGE_SCENE))
                editable->setRole(OCIO::ROLE_INTERCHANGE_SCENE,
                                  interchange.c_str());
        } else if (std::string identity
                   = find_scene_reference_identity(editable);
                   !identity.empty()) {
            // Repair: anchor lin_ap0_scene on the scene-referred identity space
            // and make it the scene interchange. For now this treats the
            // config's scene reference as AP0; synthesize a real scene-linear
            // -> AP0 transform via a builtin color space when the reference is
            // known not to be AP0.
            if (!editable->getColorSpace("lin_ap0_scene")) {
                auto cs = OCIO::ColorSpace::Create(OCIO::REFERENCE_SPACE_SCENE);
                cs->setName("lin_ap0_scene");
                cs->setEncoding("scene-linear");
                editable->addColorSpace(cs);
            }
            editable->setRole(OCIO::ROLE_INTERCHANGE_SCENE, "lin_ap0_scene");
            interchange = "lin_ap0_scene";
        }

        // Ensure lin_ap0_scene resolves (as an alias of the interchange space)
        // so the probe path's fallback lookups find it by that name.
        if (!interchange.empty()
            && !editable->getColorSpace("lin_ap0_scene")) {
            if (auto cs = editable->getColorSpace(interchange.c_str())) {
                auto editableCS = cs->createEditableCopy();
                editableCS->addAlias("lin_ap0_scene");
                editable->addColorSpace(editableCS);
            }
        }

        bootstrap_display_interchange(editable, interchange);

        // Probe processors are one-shot (results are memoized upstream), so
        // OCIO's per-config processor cache yields no hits here while adding
        // mutex contention, a full-cache scan on every miss, and keeping every
        // probe processor alive; disabling it is the documented fast path.
        editable->setProcessorCacheFlags(OCIO::PROCESSOR_CACHE_OFF);
        result = editable;
    } catch (...) {
        return {};
    }

    if (key.empty() || !result)
        return result;
    spin_rw_write_lock lock(s_mutex);
    return s_memo.emplace(key, result).first->second;  // existing on race
}

// Warn at most once per STRUCTURAL config id across the process. Returns true
// only for the caller that first records `id` (which should emit the warning).
// spin_rw_mutex-guarded set, mirroring the learned-complex blacklist shape.
bool
note_interop_warning(const std::string& id)
{
    static spin_rw_mutex s_mutex;
    static std::unordered_set<std::string> s_warned;
    {
        spin_rw_read_lock lock(s_mutex);
        if (s_warned.count(id))
            return false;
    }
    spin_rw_write_lock lock(s_mutex);
    return s_warned.insert(id).second;
}



// The cross-config chokepoint: the single wrapper over OCIO's two-config
// GetProcessorFromConfigs. Every cross-config color route funnels through
// here (color-space now; a display-view sibling with the explicit-interchange
// overload lands in a later slice), so the failure policy lives in exactly one
// place. Both configs must expose the interchange role GetProcessorFromConfigs
// needs (aces_interchange for scene-referred names, cie_xyz_d65_interchange
// for display-referred) -- that is the caller's obligation, satisfied by the
// interoperability bootstrap/repair above. With both contexts null the 4-arg
// overload is used (OCIO takes each config's current context); otherwise the
// context-aware overload runs, defaulting a missing side to that config's
// current context. On any OCIO failure the processor is null and `errmsg` is
// set from the exception -- never thrown across the boundary, never silent.
//
// Fast path: when both configs already carry the aces_interchange role (true
// by construction for the interopified analysis copy + the built-in identities
// config -- interopifiedResolvesSceneInterchange() is the caller's gate), pass
// "aces_interchange" explicitly as both interchange names. This skips OCIO's
// own interchange-role lookup/validation on every call and is materially
// faster; the role check below (Config::hasRole, not a name heuristic) is what
// makes it safe to take. Any config missing the role on either side falls
// through unchanged to the discovery form.
OCIO::ConstProcessorRcPtr
processor_from_configs(const OCIO::ConstConfigRcPtr& src_config,
                       string_view src_name,
                       const OCIO::ConstConfigRcPtr& dst_config,
                       string_view dst_name, std::string& errmsg,
                       const OCIO::ConstContextRcPtr& src_context = nullptr,
                       const OCIO::ConstContextRcPtr& dst_context = nullptr)
{
    errmsg.clear();
    if (!src_config || !dst_config) {
        errmsg = "Cross-config processor requires two valid configs";
        return {};
    }
    const std::string src(src_name);
    const std::string dst(dst_name);
    const bool explicit_interchange
        = src_config->hasRole(OCIO::ROLE_INTERCHANGE_SCENE)
          && dst_config->hasRole(OCIO::ROLE_INTERCHANGE_SCENE);
    try {
        if (!src_context && !dst_context) {
            if (explicit_interchange)
                return OCIO::Config::GetProcessorFromConfigs(
                    src_config, src.c_str(), OCIO::ROLE_INTERCHANGE_SCENE,
                    dst_config, dst.c_str(), OCIO::ROLE_INTERCHANGE_SCENE);
            return OCIO::Config::GetProcessorFromConfigs(src_config, src.c_str(),
                                                         dst_config, dst.c_str());
        }
        OCIO::ConstContextRcPtr sctx = src_context ? src_context
                                                   : src_config->getCurrentContext();
        OCIO::ConstContextRcPtr dctx = dst_context ? dst_context
                                                   : dst_config->getCurrentContext();
        if (explicit_interchange)
            return OCIO::Config::GetProcessorFromConfigs(
                sctx, src_config, src.c_str(), OCIO::ROLE_INTERCHANGE_SCENE,
                dctx, dst_config, dst.c_str(), OCIO::ROLE_INTERCHANGE_SCENE);
        return OCIO::Config::GetProcessorFromConfigs(sctx, src_config, src.c_str(),
                                                     dctx, dst_config, dst.c_str());
    } catch (OCIO::Exception& e) {
        errmsg = e.what();
    } catch (...) {
        errmsg = "Unknown error in OpenColorIO GetProcessorFromConfigs";
    }
    return {};
}

// Display-view sibling of the cross-config chokepoint: the single wrapper over
// OCIO's two-config display-view GetProcessorFromConfigs overload. Every
// cross-config display route funnels through here, so the failure policy lives
// in one place (as with processor_from_configs). This is the cross-config
// display bridge composition -- a scene-referred source in one config routed to
// a display/view in another -- which relies on both configs exposing the
// aces_interchange role OCIO auto-detects (the caller's obligation, satisfied
// by the interoperability bootstrap/repair). With both contexts null the 6-arg
// overload runs; otherwise the context-aware overload, defaulting a missing
// side to that config's current context. On any OCIO failure the processor is
// null and `errmsg` is set from the exception -- never thrown across the
// boundary, never silent.
//
// Version-dependent quality, not a defect: display-view data-space no-op
// semantics changed in OCIO 2.3.1 (#1896) -- a display/view that is itself a
// data space is a no-op transform on 2.3.1+, whereas 2.3.0 could produce a
// non-identity result in that case. No workaround here; document only.
//
// Fast path: same construction as processor_from_configs above, and the same
// role -- aces_interchange, not cie_xyz_d65_interchange. Every caller of this
// helper routes a scene-referred source (the identities config's ACES2065-1
// identity space) into a display/view, and OCIO picks the interchange role
// from the SOURCE color space's reference type
// (Config::GetProcessorFromConfigs, display/view overload), so
// aces_interchange is what this route actually resolves through today; there
// is no display/XYZ-referred source in this chokepoint to guarantee
// cie_xyz_d65_interchange for. If a display-referred source ever routes
// through here, this fast path must gate on cie_xyz_d65_interchange instead
// (or fall back) for that case.
OCIO::ConstProcessorRcPtr
display_processor_from_configs(const OCIO::ConstConfigRcPtr& src_config,
                              string_view src_name,
                              const OCIO::ConstConfigRcPtr& dst_config,
                              string_view display, string_view view,
                              OCIO::TransformDirection direction,
                              std::string& errmsg,
                              const OCIO::ConstContextRcPtr& src_context = nullptr,
                              const OCIO::ConstContextRcPtr& dst_context = nullptr)
{
    errmsg.clear();
    if (!src_config || !dst_config) {
        errmsg = "Cross-config display processor requires two valid configs";
        return {};
    }
    const std::string src(src_name);
    const std::string disp(display);
    const std::string vw(view);
    const bool explicit_interchange
        = src_config->hasRole(OCIO::ROLE_INTERCHANGE_SCENE)
          && dst_config->hasRole(OCIO::ROLE_INTERCHANGE_SCENE);
    try {
        if (!src_context && !dst_context) {
            if (explicit_interchange)
                return OCIO::Config::GetProcessorFromConfigs(
                    src_config, src.c_str(), OCIO::ROLE_INTERCHANGE_SCENE,
                    dst_config, disp.c_str(), vw.c_str(),
                    OCIO::ROLE_INTERCHANGE_SCENE, direction);
            return OCIO::Config::GetProcessorFromConfigs(src_config, src.c_str(),
                                                         dst_config, disp.c_str(),
                                                         vw.c_str(), direction);
        }
        OCIO::ConstContextRcPtr sctx = src_context ? src_context
                                                   : src_config->getCurrentContext();
        OCIO::ConstContextRcPtr dctx = dst_context ? dst_context
                                                   : dst_config->getCurrentContext();
        if (explicit_interchange)
            return OCIO::Config::GetProcessorFromConfigs(
                sctx, src_config, src.c_str(), OCIO::ROLE_INTERCHANGE_SCENE,
                dctx, dst_config, disp.c_str(), vw.c_str(),
                OCIO::ROLE_INTERCHANGE_SCENE, direction);
        return OCIO::Config::GetProcessorFromConfigs(sctx, src_config, src.c_str(),
                                                     dctx, dst_config, disp.c_str(),
                                                     vw.c_str(), direction);
    } catch (OCIO::Exception& e) {
        errmsg = e.what();
    } catch (...) {
        errmsg = "Unknown error in OpenColorIO GetProcessorFromConfigs (display)";
    }
    return {};
}



//////////////////////////////////////////////////////////////////////////
//
// Registry fingerprint index: the one genuinely new primitive the read-side
// registry-equivalence tier (and the write-side derivation that shares this
// file) needs. It fingerprints the built-in interop identities config's own
// simple color spaces so a query config's spaces can be matched against them by
// value. Everything else it uses -- the registry config, the interopified probe
// copy, the probe protocol, the fingerprint compute and match -- is reused from
// the foundation above.

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
// index. Nothing here runs until the first resolve() query reaches the
// registry-equivalence tier; ColorConfig construction never touches it. The
// index is immutable for the life of the process -- the registry config is a
// process-global constant, so entries are content-addressed and never
// invalidated or evicted. The C++11 magic-static guard makes the one-time build
// thread-safe (same idiom as build_interop_identities_config()). The write-side
// derivation fingerprints the same registry the same way and reuses this exact
// builder.
const RegistryFingerprintIndex&
registry_fingerprint_index()
{
    static const RegistryFingerprintIndex s_index =
        []() -> RegistryFingerprintIndex {
        RegistryFingerprintIndex idx;
        OCIO::ConstConfigRcPtr registry = build_interop_identities_config();
        if (!registry)
            return idx;
        idx.config = interopify_config(registry);
        if (!idx.config)
            return idx;
        try {
            OCIO::ConstContextRcPtr context = idx.config->getCurrentContext();
            ProbeValues probes = initialize_probe_values(idx.config, context);
            for (const auto& name : get_simple_color_spaces(idx.config)) {
                auto cs = idx.config->getColorSpace(name.c_str());
                auto fp = compute_fingerprint(idx.config, cs, context, probes);
                if (fp)
                    idx.entries.emplace_back(name, std::move(*fp));
            }
        } catch (...) {
            idx.entries.clear();
        }
        std::sort(idx.entries.begin(), idx.entries.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        return idx;
    }();
    return s_index;
}

// Map a query interop id to its registry entry's fingerprint. Resolves the id
// against the registry by name or alias (utility tokens name no registry space
// and so miss here) to the canonical registry space, then finds that space's
// fingerprint in the sorted index. On a hit, `canonical_id_out` receives the
// registry space's own interop id (its interop_id attribute on OCIO >= 2.5,
// else its name) for the cheap direct-id compare on the query side. Returns
// null when the id resolves to no registry space, or to one with no fingerprint
// (e.g. a non-simple registry space).
const OIIO::pvt::ColorSpaceFingerprint*
registry_fingerprint_for_id(const RegistryFingerprintIndex& index,
                            string_view id, std::string& canonical_id_out)
{
    canonical_id_out.clear();
    if (!index.config || id.empty())
        return nullptr;
    OCIO::ConstColorSpaceRcPtr cs;
    try {
        cs = index.config->getColorSpace(std::string(id).c_str());
    } catch (...) {
        return nullptr;
    }
    if (!cs)
        return nullptr;
    const std::string name = cs->getName();
#if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 5, 0)
    if (const char* iid = cs->getInteropID(); iid && *iid)
        canonical_id_out = iid;
#endif
    if (canonical_id_out.empty())
        canonical_id_out = name;
    auto it = std::lower_bound(index.entries.begin(), index.entries.end(), name,
                               [](const auto& e, const std::string& key) {
                                   return e.first < key;
                               });
    if (it != index.entries.end() && it->first == name)
        return &it->second;
    return nullptr;
}




// Reverse of registry_fingerprint_for_id (the write-side direction): given a
// query space's fingerprint, return the built-in registry identity whose
// fingerprint matches it within tolerance, walking the index's sorted
// deterministic order so first-match is stable. The returned view is the
// registry identity's own interop id -- its interop_id attribute (OCIO >= 2.5,
// where the registry is the studio config whose space names, e.g. "ACEScg",
// differ from the CIF ids, e.g. "lin_ap1_scene"), else its name (the embedded
// config, where name == interop_id). This mirrors registry_fingerprint_for_id's
// canonicalization. Both strings are owned by the process-global registry
// config/index, so the view is stable for the life of the process. Empty when
// nothing matches.
string_view
registry_id_for_fingerprint(const RegistryFingerprintIndex& index,
                            const OIIO::pvt::ColorSpaceFingerprint& query_fp)
{
    for (const auto& entry : index.entries) {
        if (!fingerprints_match(query_fp, entry.second))
            continue;
#if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 5, 0)
        if (index.config) {
            try {
                if (auto cs = index.config->getColorSpace(entry.first.c_str()))
                    if (const char* iid = cs->getInteropID(); iid && *iid)
                        return iid;
            } catch (...) {
            }
        }
#endif
        return entry.first;
    }
    return {};
}



// ---------------------------------------------------------------------------
// ICC profile identification -- decode an embedded ICC profile through
// OCIO's matrix/TRC ICC FileTransform reader inside a throwaway in-memory
// probe config, then fingerprint the decoded transform against the interop
// identities registry above. Mirror-inside: the reader's acceptance rules
// (colorant matrix + per-channel tone curves, hardcoded Bradford D50->D65
// adaptation) are OCIO's, never re-derived here.
// ---------------------------------------------------------------------------

// Serves the embedded ICC blob to OCIO's ICC FileTransform reader without
// touching disk. The probe config is built in code (never parsed), so
// getConfigData() is intentionally empty -- OCIO only consults the proxy
// for LUT/file reads once setConfigIOProxy is attached to an
// already-constructed config. The config carries exactly one file (the
// virtual profile name), which OCIO absolutizes against the config's
// search path before asking the proxy -- so serve the single blob for any
// non-null request rather than matching the (absolutized) filename.
class IccBlobProxy final : public OCIO::ConfigIOProxy {
public:
    IccBlobProxy(cspan<uint8_t> blob, std::string hash)
        : m_blob(blob.begin(), blob.end())
        , m_hash(std::move(hash))
    {
    }

    std::vector<uint8_t> getLutData(const char* filepath) const override
    {
        if (filepath)
            return m_blob;
        throw OCIO::Exception("IccBlobProxy: unexpected LUT request");
    }
    std::string getConfigData() const override { return {}; }
    std::string getFastLutFileHash(const char* filepath) const override
    {
        return filepath ? m_hash : std::string();
    }

private:
    std::vector<uint8_t> m_blob;
    std::string m_hash;
};



// Build the throwaway probe config for one ICC profile: a display-referred
// space "icc_probe" whose to_reference is a FileTransform on the profile
// set INVERSE (OCIO's ICC FileTransform forward maps reference(PCS) ->
// device; inverting makes to_reference DECODE device code values into the
// CIE-XYZ-D65 display reference), plus an identity "cie_xyz_d65" space
// carrying the display interchange role so the fingerprint probe values
// are already in the reference and normalization is a no-op.
//
// The per-profile virtual filename embeds the content identifier -- this
// is load-bearing, NOT cosmetic: OCIO's process-global GetFastFileHash
// cache (PathUtils.cpp) keys purely on the resolved filename and never
// re-consults the ConfigIOProxy, and a config's processor cache ID hashes
// its serialization (which embeds the src). A shared filename would make
// every probe config collide on both keys, handing back the FIRST
// profile's processor for every later profile in the process (e.g. an
// undecodable cLUT "decoding" as a previously-seen sRGB). A content-unique
// name keeps distinct profiles distinct.
OCIO::ConstConfigRcPtr
make_icc_probe_config(cspan<uint8_t> iccdata)
{
    const std::string hash = OIIO::pvt::icc_profile_identifier(iccdata);
    // Start from CreateRaw's minimal working config (current version, "raw"
    // space + default role already valid). validate() rejects an empty
    // search_path when a FileTransform is present, so set one -- the actual
    // bytes come from the ConfigIOProxy, never this path.
    auto cfg = OCIO::Config::CreateRaw()->createEditableCopy();
    cfg->setName("oiio-icc-probe");
    cfg->setSearchPath(".");
    // Every processor built against the probe is a one-shot; a per-config
    // processor cache would only add mutex serialization and retained
    // processors.
    cfg->setProcessorCacheFlags(OCIO::PROCESSOR_CACHE_OFF);

    auto xyz = OCIO::ColorSpace::Create(OCIO::REFERENCE_SPACE_DISPLAY);
    xyz->setName("cie_xyz_d65");
    cfg->addColorSpace(xyz);
    cfg->setRole(OCIO::ROLE_INTERCHANGE_DISPLAY, "cie_xyz_d65");

    // OCIO validation requires a view transform whenever display-referred
    // spaces exist; bridge scene AP0 <-> CIE-XYZ-D65 with the standard
    // utility builtin (also wires the scene interchange, matching the
    // registry topology).
    auto ap0 = OCIO::ColorSpace::Create(OCIO::REFERENCE_SPACE_SCENE);
    ap0->setName("lin_ap0");
    cfg->addColorSpace(ap0);
    cfg->setRole(OCIO::ROLE_INTERCHANGE_SCENE, "lin_ap0");
    auto bridge = OCIO::ViewTransform::Create(OCIO::REFERENCE_SPACE_SCENE);
    bridge->setName("scene_to_display_bridge");
    auto toxyz = OCIO::BuiltinTransform::Create();
    toxyz->setStyle("UTILITY - ACES-AP0_to_CIE-XYZ-D65_BFD");
    bridge->setTransform(toxyz, OCIO::VIEWTRANSFORM_DIR_FROM_REFERENCE);
    cfg->addViewTransform(bridge);
    cfg->setDefaultViewTransformName("scene_to_display_bridge");

    auto probe = OCIO::ColorSpace::Create(OCIO::REFERENCE_SPACE_DISPLAY);
    probe->setName("icc_probe");
    auto ft = OCIO::FileTransform::Create();
    ft->setSrc(("embedded_" + hash + ".icc").c_str());
    ft->setDirection(OCIO::TRANSFORM_DIR_INVERSE);
    probe->setTransform(ft, OCIO::COLORSPACE_DIR_TO_REFERENCE);
    cfg->addColorSpace(probe);

    cfg->setConfigIOProxy(std::make_shared<IccBlobProxy>(iccdata, hash));
    cfg->validate();
    return cfg;
}



// Core of pvt::identify_icc_profile() (the pvt shim at the end of this
// file forwards here). Identify-first: a profile that decodes and matches
// a registry identity yields that identity (resolved against the caller's
// config when possible); a decodable-but-unmatched profile yields the bare
// "icc:<identifier>" token. There is deliberately NO session-synthetic
// registration on this branch -- nothing consumes a registered synthetic
// yet, so the token itself is the complete answer for the unmatched case.
OIIO::pvt::IccIdentifyResult
identify_icc_profile_impl(const ColorConfig& config, cspan<uint8_t> iccdata)
{
    OIIO::pvt::IccIdentifyResult result;
    if (!OIIO::pvt::is_icc_profile(iccdata))
        return result;  // not ICC: empty id, decodable false
    const std::string token = "icc:"
                              + OIIO::pvt::icc_profile_identifier(iccdata);

    // Decode-validate eagerly: undecodable profiles (cLUT/AToB -- OCIO's
    // reader is matrix/TRC-only) throw when the processor is built.
    OCIO::ConstConfigRcPtr probecfg;
    try {
        probecfg = make_icc_probe_config(iccdata);
        probecfg->getProcessor("icc_probe", "cie_xyz_d65");
    } catch (...) {
        result.id = token;
        return result;  // decodable stays false
    }
    result.decodable = true;

    // Fingerprint the decoded profile against the registry identities. The
    // probe values are authored in CIE-XYZ-D65, which IS this config's
    // display reference, so initialize_probe_values leaves them untouched.
    std::string ciid;
    try {
        auto context       = probecfg->getCurrentContext();
        ProbeValues probes = initialize_probe_values(probecfg, context);
        if (auto fp = compute_fingerprint(probecfg,
                                          probecfg->getColorSpace("icc_probe"),
                                          context, probes))
            ciid = registry_id_for_fingerprint(registry_fingerprint_index(),
                                               *fp);
    } catch (...) {
    }
    if (ciid.empty()) {
        result.id = token;  // decodable, unmatched
        return result;
    }
    // Prefer a caller-local resolution of the matched identity; fall back
    // to the canonical interop id itself.
    string_view local = config.resolve(ciid);
    result.id         = local.empty() ? ciid : std::string(local);
    return result;
}



// ---------------------------------------------------------------------------
// Mastering display volume (SMPTE ST 2086) derivation -- the five-tier
// first-hit-wins ladder, ported from the proven POC:
//   1. ACES-OUTPUT builtin style table (nominal peak + limiting gamut)
//   2. display-interchange CST probe   (view_transform-based views)
//   3. inverse DISPLAY-builtin probe   (v1-style with an encoding tail)
//   4. registry-identity decode probe  (v1-style pure-LUT with interop id)
//   5. no record (honestly yields nothing, never guesses)
// Tiers 2-4 share one numeric probe; they differ only in how the code->XYZ
// decode is built.
// ---------------------------------------------------------------------------

// Reference chromaticities for the style table (R, G, B, W xy).
static const float kRec709xy[4][2]  = { { 0.64f, 0.33f },
                                        { 0.30f, 0.60f },
                                        { 0.15f, 0.06f },
                                        { 0.3127f, 0.329f } };
static const float kP3D65xy[4][2]   = { { 0.68f, 0.32f },
                                        { 0.265f, 0.69f },
                                        { 0.15f, 0.06f },
                                        { 0.3127f, 0.329f } };
static const float kRec2020xy[4][2] = { { 0.708f, 0.292f },
                                        { 0.170f, 0.797f },
                                        { 0.131f, 0.046f },
                                        { 0.3127f, 0.329f } };

// Recursive scan for a BuiltinTransform whose style begins with `prefix` in
// a transform tree (GroupTransform children included). ColorSpaceTransform
// indirection is intentionally not followed -- a style reachable only
// through a referenced space is invisible by design; callers fall through
// to their own next tier. `stop_at_first` returns on the first hit (the
// ACES-OUTPUT lookup); otherwise every child is scanned so `style_out`
// holds the LAST match (the DISPLAY-builtin encoding tail, where the tail
// -- not the first hit -- is what's wanted).
bool
find_builtin_style(const OCIO::ConstTransformRcPtr& transform,
                   const char* prefix, bool stop_at_first,
                   std::string& style_out)
{
    if (!transform)
        return false;
    if (transform->getTransformType() == OCIO::TRANSFORM_TYPE_BUILTIN) {
        auto builtin = OCIO::DynamicPtrCast<const OCIO::BuiltinTransform>(
            transform);
        if (builtin && Strutil::starts_with(builtin->getStyle(), prefix)) {
            style_out = builtin->getStyle();
            return true;
        }
        return false;
    }
    if (transform->getTransformType() == OCIO::TRANSFORM_TYPE_GROUP) {
        auto group = OCIO::DynamicPtrCast<const OCIO::GroupTransform>(
            transform);
        if (!group)
            return false;
        bool found = false;
        for (int i = 0; i < group->getNumTransforms(); ++i) {
            if (find_builtin_style(group->getTransform(i), prefix,
                                   stop_at_first, style_out)) {
                found = true;
                if (stop_at_first)
                    return true;
            }
        }
        return found;
    }
    return false;
}

// DISPLAY-builtin encoding tail prefix of a display encoding style.
static const char* kDisplayBuiltinPrefix = "DISPLAY - CIE-XYZ-D65_to_";

// Parse an ACES-OUTPUT builtin style into a mastering volume. Peak nits is
// the first "<number>nit" token (ACES 1.1 styles carry a second mid-grey
// token -- "1000nit-15nit" -- which is not a mastering value); tokenless
// SDR styles fall back to their ACES-defined nominals (video 100, cinema
// 48). The limiting gamut comes from the P3lim / P3-D65 / REC2020[lim] /
// REC709[lim] token; whitepoint is D65, carried by the tables themselves.
// SDR-VIDEO without a gamut token is Rec.709 by definition. Styles with no
// resolvable gamut (e.g. DCI-white cinema sims) return false and fall
// through to the numeric probe.
bool
mastering_volume_from_style(const std::string& style,
                            OIIO::pvt::MasteringDisplayVolume& volume)
{
    if (!Strutil::starts_with(style, "ACES-OUTPUT"))
        return false;
    double peak       = 0.0;
    const auto nitpos = style.find("nit");
    if (nitpos != std::string::npos) {
        size_t begin = nitpos;
        while (begin > 0
               && (isdigit(static_cast<unsigned char>(style[begin - 1]))
                   || style[begin - 1] == '.'))
            --begin;
        if (begin == nitpos)
            return false;
        peak = Strutil::stod(style.substr(begin, nitpos - begin));
    } else if (style.find("SDR-VIDEO") != std::string::npos) {
        peak = 100.0;
    } else if (style.find("SDR-CINEMA") != std::string::npos) {
        peak = 48.0;
    } else {
        return false;
    }
    const float(*gamut)[2] = nullptr;
    if (style.find("P3lim") != std::string::npos
        || style.find("P3-D65") != std::string::npos)
        gamut = kP3D65xy;
    else if (style.find("REC2020") != std::string::npos)
        gamut = kRec2020xy;
    else if (style.find("REC709") != std::string::npos
             || style.find("SDR-VIDEO") != std::string::npos)
        gamut = kRec709xy;
    if (!gamut)
        return false;
    memcpy(volume.primaries, gamut, sizeof(volume.primaries));
    volume.max_luminance = peak;
    // min stays 0.0 (what a code-0 probe reports for PQ and Rec.1886
    // alike); wire encoders wanting the conventional 0.0001 cd/m^2 floor
    // clamp at encode time.
    volume.min_luminance = 0.0;
    volume.style         = style;
    return true;
}

// Cinema predicate for a "DISPLAY - CIE-XYZ-D65_to_*" builtin style --
// classify by the encoding FAMILY that leads the style suffix, never by
// DCI/DCDM device tokens anywhere in the string. Gamma-2.6 theatrical
// encodings (G2.6-P3-DCI-BFD, G2.6-P3-D60-BFD, G2.6-P3-D65, and DCDM's
// gamma 2.6 with its baked 48/52.37 scale) decode interchange Y relative
// to the 48 cd/m^2 projector calibration white. ST2084/PQ decodes to
// absolute nits/100, and the video encodings (sRGB, G2.2/REC.1886,
// DisplayP3, HLG) are relative to the 100 cd/m^2 video white. Device
// tokens are unreliable: the "DCDM" in ST2084-DCDM-D65 names the XYZ
// container, not a 48-nit convention, and G2.6-P3-D60-BFD carries no
// DCI/DCDM token at all.
bool
is_cinema_display_builtin(const std::string& style)
{
    if (!Strutil::starts_with(style, kDisplayBuiltinPrefix))
        return false;
    string_view suffix = string_view(style).substr(
        strlen(kDisplayBuiltinPrefix));
    return Strutil::starts_with(suffix, "G2.6-P3-")
           || Strutil::starts_with(suffix, "DCDM-");
}

// Cinema classification from an interop identity: the registry twin's
// encoding is the authority (identity-first; the style-suffix predicate
// above is the structural fallback). "sdr-cinema" marks the gamma-2.6
// theatrical encodings (48 cd/m^2 calibration white); "hdr-cinema" marks
// PQ cinema masters, which decode to absolute nits/100 like all PQ and so
// anchor at 100. Linear theatrical spaces stay "display-linear" in the
// registry, so the p3dci gamut token still decides those. Returns -1 when
// the identity has no registry twin -- the caller falls back to whatever
// structural evidence it holds; else 0/1.
int
cinema_from_identity(string_view interop_id)
{
    if (interop_id.empty())
        return -1;
    const RegistryFingerprintIndex& index = registry_fingerprint_index();
    if (!index.config)
        return -1;
    OCIO::ConstColorSpaceRcPtr twin;
    try {
        twin = index.config->getColorSpace(std::string(interop_id).c_str());
    } catch (...) {
        return -1;
    }
    if (!twin)
        return -1;
    const char* enc = twin->getEncoding();
    string_view encoding(enc ? enc : "");
    if (Strutil::iequals(encoding, "sdr-cinema"))
        return 1;
    if (Strutil::iequals(encoding, "display-linear")
        && Strutil::icontains(interop_id, "p3dci"))
        return 1;  // e.g. lin_p3dci_display: linear feed to a 48-nit projector
    return 0;      // known twin with a video or PQ (absolute) encoding
}

// Snap a probed peak to the nearest nominal mastering target. mDCV wants
// the nominal (an ACES 1.1 1000-nit tonescale saturates at ~991.48 through
// the probe), so anything within the 2% window snaps; first match wins.
double
snap_nominal_nits(double nits)
{
    static const double kNominal[] = { 48,  100,  108,  203,  300,  500,
                                       600, 1000, 2000, 4000, 10000 };
    for (double nominal : kNominal)
        if (std::abs(nits - nominal) / nominal < 0.02)
            return nominal;
    return nits;
}

// Core of pvt::derive_mastering_volume() (the pvt shim at the end of this
// file forwards here).
bool
derive_mastering_volume_impl(const ColorConfig& config, string_view display,
                             string_view view,
                             OIIO::pvt::MasteringDisplayVolume& volume)
{
    auto* impl = pvt::ColorConfigClassificationPeek::impl(config);
    if (!impl || !impl->config_)
        return false;
    const OCIO::ConstConfigRcPtr cfg = impl->config_;

    std::string disp(display);
    std::string vw(view);
    try {
        if (disp.empty())
            disp = cfg->getDefaultDisplay();
        if (disp.empty())
            return false;
        if (vw.empty())
            vw = cfg->getDefaultView(disp.c_str());
        if (vw.empty())
            return false;

        std::string style;                  // ACES-OUTPUT style, if any
        std::string display_encoding_tail;  // v1-style DISPLAY builtin tail
        std::string output_space;           // v1-style output space name
        const char* vtname  = cfg->getDisplayViewTransformName(disp.c_str(),
                                                               vw.c_str());
        const bool vt_based = vtname && vtname[0];
        if (vt_based) {
            if (auto vt = cfg->getViewTransform(vtname)) {
                if (!find_builtin_style(
                        vt->getTransform(OCIO::VIEWTRANSFORM_DIR_FROM_REFERENCE),
                        "ACES-OUTPUT", true, style))
                    find_builtin_style(
                        vt->getTransform(OCIO::VIEWTRANSFORM_DIR_TO_REFERENCE),
                        "ACES-OUTPUT", true, style);
            }
        } else {
            const char* csname = cfg->getDisplayViewColorSpaceName(disp.c_str(),
                                                                   vw.c_str());
            if (csname && csname[0]) {
                output_space = csname;
                if (auto cs = cfg->getColorSpace(csname)) {
                    auto fromref = cs->getTransform(
                        OCIO::COLORSPACE_DIR_FROM_REFERENCE);
                    auto toref = cs->getTransform(
                        OCIO::COLORSPACE_DIR_TO_REFERENCE);
                    if (!find_builtin_style(fromref, "ACES-OUTPUT", true, style))
                        find_builtin_style(toref, "ACES-OUTPUT", true, style);
                    find_builtin_style(fromref, kDisplayBuiltinPrefix, false,
                                       display_encoding_tail);
                    if (display_encoding_tail.empty())
                        find_builtin_style(toref, kDisplayBuiltinPrefix, false,
                                           display_encoding_tail);
                }
            }
        }

        // Tier 1: the style table carries the NOMINAL peak (1000 where the
        // probe sees the tonescale asymptote 991.48) and the LIMITING gamut
        // (the probe can only see the encoding gamut).
        if (mastering_volume_from_style(style, volume))
            return true;

        // Tiers 2-4: build the code->XYZ decode, then run the shared
        // numeric probe. Cinema anchoring (48 vs 100 cd/m^2) is decided per
        // construction, from the evidence each has -- identity-first via
        // the registry twin's encoding, structural style-family fallback.
        OCIO::ConstCPUProcessorRcPtr decode;
        bool cinema  = false;
        auto context = cfg->getCurrentContext();
        if (vt_based) {
            // Tier 2: CST from the display colorspace to the display
            // interchange role.
            const char* dcsname
                = cfg->getDisplayViewColorSpaceName(disp.c_str(), vw.c_str());
            if (!dcsname || !dcsname[0])
                return false;
            std::string tail;
            if (auto dcs = cfg->getColorSpace(dcsname)) {
                find_builtin_style(dcs->getTransform(
                                       OCIO::COLORSPACE_DIR_FROM_REFERENCE),
                                   kDisplayBuiltinPrefix, false, tail);
                if (tail.empty())
                    find_builtin_style(dcs->getTransform(
                                           OCIO::COLORSPACE_DIR_TO_REFERENCE),
                                       kDisplayBuiltinPrefix, false, tail);
            }
            const int idcinema = cinema_from_identity(
                config.get_color_interop_id(dcsname));
            cinema   = idcinema >= 0 ? (idcinema > 0)
                                     : is_cinema_display_builtin(tail);
            auto cst = OCIO::ColorSpaceTransform::Create();
            cst->setSrc(dcsname);
            cst->setDst(OCIO::ROLE_INTERCHANGE_DISPLAY);
            decode = cfg->getProcessor(context, cst,
                                       OCIO::TRANSFORM_DIR_FORWARD)
                         ->getDefaultCPUProcessor();
            // Provenance: the unparseable ACES style tier 1 found, if any.
        } else if (!display_encoding_tail.empty()) {
            // Tier 3: the LAST DISPLAY builtin in the output space's chain,
            // instantiated INVERSE. A CST is NOT used here -- the v1 output
            // space is scene-referred and a CST to the interchange would
            // re-apply a view transform.
            auto builtin = OCIO::BuiltinTransform::Create();
            builtin->setStyle(display_encoding_tail.c_str());
            builtin->setDirection(OCIO::TRANSFORM_DIR_INVERSE);
            decode = cfg->getProcessor(context, builtin,
                                       OCIO::TRANSFORM_DIR_FORWARD)
                         ->getDefaultCPUProcessor();
            style  = display_encoding_tail;  // provenance: the encoding tail
            cinema = is_cinema_display_builtin(display_encoding_tail);
        } else {
            // Tier 4: registry-identity decode. The identity must be
            // display-referred -- a scene-referred identity cannot anchor
            // display luminance.
            if (output_space.empty())
                return false;
            string_view interop_id = config.get_color_interop_id(output_space);
            const RegistryFingerprintIndex& index = registry_fingerprint_index();
            if (interop_id.empty() || !index.config)
                return false;
            auto registrycs = index.config->getColorSpace(
                std::string(interop_id).c_str());
            if (!registrycs
                || registrycs->getReferenceSpaceType()
                       != OCIO::REFERENCE_SPACE_DISPLAY)
                return false;
            decode = index.config
                         ->getProcessor(registrycs->getName(),
                                        OCIO::ROLE_INTERCHANGE_DISPLAY)
                         ->getDefaultCPUProcessor();
            style  = interop_id;  // provenance: the identity
            cinema = cinema_from_identity(interop_id) > 0;
        }

        // The shared probe. Luminance: achromatic 1e5 drive through the
        // view (saturates any tonescale), decoded at CIE-XYZ-D65 where
        // Y = nits/anchor; the anchor is 100 (video, PQ absolute) or 48
        // (gamma-2.6 theatrical projector calibration white). Peak is
        // capped at the 10000 PQ container ceiling and snapped to the
        // nominal targets; black is probe-honest (no 0.0001 floor).
        auto dvt = OCIO::DisplayViewTransform::Create();
        dvt->setSrc(OCIO::ROLE_SCENE_LINEAR);
        dvt->setDisplay(disp.c_str());
        dvt->setView(vw.c_str());
        auto dvtcpu = cfg->getProcessor(context, dvt,
                                        OCIO::TRANSFORM_DIR_FORWARD)
                          ->getDefaultCPUProcessor();

        float peakrgb[3] = { 1e5f, 1e5f, 1e5f };
        dvtcpu->applyRGB(peakrgb);
        decode->applyRGB(peakrgb);
        float blackrgb[3] = { 0.0f, 0.0f, 0.0f };
        dvtcpu->applyRGB(blackrgb);
        decode->applyRGB(blackrgb);
        const double anchor  = cinema ? 48.0 : 100.0;
        volume.max_luminance = snap_nominal_nits(
            std::min(double(peakrgb[1]) * anchor, 10000.0));
        volume.min_luminance = std::max(double(blackrgb[1]) * anchor, 0.0);

        // Primaries: decode the four basis vectors R,G,B,W to XYZ and
        // convert to xy. Invariant to the per-channel TRC (a basis vector's
        // xy is independent of the curve), so exact for matrix+TRC
        // encodings; reports the ENCODING gamut (hull-fitting a custom
        // view's true limiting gamut is a known follow-up).
        static const float kBasis[4][3] = { { 1.0f, 0.0f, 0.0f },
                                            { 0.0f, 1.0f, 0.0f },
                                            { 0.0f, 0.0f, 1.0f },
                                            { 1.0f, 1.0f, 1.0f } };
        for (int i = 0; i < 4; ++i) {
            float xyz[3] = { kBasis[i][0], kBasis[i][1], kBasis[i][2] };
            decode->applyRGB(xyz);
            const double sum = double(xyz[0]) + double(xyz[1]) + double(xyz[2]);
            volume.primaries[i][0] = sum != 0.0 ? float(xyz[0] / sum) : 0.0f;
            volume.primaries[i][1] = sum != 0.0 ? float(xyz[1] / sum) : 0.0f;
        }
        volume.style = style;  // provenance: builtin style or interop id
        return true;
    } catch (...) {
        // No display interchange role, unresolvable scene source, etc. --
        // the volume is not derivable from this config.
        return false;
    }
}

}  // namespace



void
ColorConfig::Impl::interop_bootstrap(InteropState& state) const
{
    state.is_interoperable = false;
    state.interchange_colorspace.clear();
    if (!config_ || disable_builtin_configs)
        return;

    std::string name = discover_scene_interchange(config_);
    // Builtin config naming convention: ocio://default resolves the role name
    // itself even when the alias list above discovers nothing.
    if (name.empty() && Strutil::iequals(configname(), "ocio://default"))
        name = OCIO::ROLE_INTERCHANGE_SCENE;
    if (!name.empty()) {
        state.interchange_colorspace = name;
        state.is_interoperable       = true;
    }
}



void
ColorConfig::Impl::ensure_interop() const
{
    {
        spin_rw_read_lock lock(m_mutex);
        if (m_interop_ready)
            return;
    }

    // Do all OCIO work OUTSIDE the lock (OCIO takes its own locks; a racing
    // builder's work is simply discarded -- duplicate work is fine, blocking
    // is not), then publish under the write lock. Same discipline as examine()
    // and ensureProbeConfig(); ColorConfig construction never reaches here.
    InteropState state;
    interop_bootstrap(state);
    if (config_ && !disable_ocio)
        state.interopified = interopify_config(config_);

    // Non-interoperable configs warn. This is a WARNING, not an error: it is
    // deliberately never written to the ColorConfig error string here. That
    // string is a single overwrite-on-set slot shared per config (see
    // error()/geterror() above), so setting it at bootstrap -- before any
    // cross-config route is even attempted -- would make has_error() true for
    // callers who never touch cross-config features, polluting otherwise-
    // healthy same-config use and risking a spurious trip of the uncaught-
    // error exit dump. Instead: an OIIO::debug line (attr/env-gated, never an
    // unconditional stderr print -- R4), printed at most once per structural
    // config id across the process, plus the composed message recorded
    // in-memory on THIS Impl -- every Impl that finds itself non-
    // interoperable composes and records its own `warned`/`warning_message`,
    // regardless of which Impl (if any) won the process-global debug-line
    // dedup claim below; the dedup only throttles the printed line, it must
    // not decide whether this Impl's own observable reflects reality (a
    // second ColorConfig wrapping the same structural config independently
    // discovers it is non-interoperable and must report that). reconcile_
    // cross_config{,_display} compose their own why+how-to-fix error text
    // only if/when a cross-config route is actually attempted and fails
    // (unchanged by this slice). Skip when builtin configs are disabled --
    // we didn't actually assess interoperability in that case.
    if (!state.is_interoperable && config_ && !disable_builtin_configs) {
        state.warning_message = Strutil::fmt::format(
            "OpenImageIO ColorConfig \"{}\" is not color-interoperable: "
            "no scene interchange role (aces_interchange) could be found "
            "or repaired. Cross-config color conversions and display "
            "transforms are unavailable for this config -- OCIO strict "
            "parsing will error on them, non-strict parsing will pass "
            "them through unchanged. Add the aces_interchange role (and "
            "a matching color space) to this config to enable "
            "cross-config features.",
            configname());
        state.warned            = true;
        const std::string key = get_config_cache_id(config_);
        if (!key.empty() && note_interop_warning(key))
            Strutil::debug("{}\n", state.warning_message);
    }

    spin_rw_write_lock lock(m_mutex);
    if (!m_interop_ready) {
        m_interop       = std::move(state);
        m_interop_ready = true;
    }
}



OCIO::ConstConfigRcPtr
ColorConfig::Impl::interopifiedConfig() const
{
    ensure_interop();
    spin_rw_read_lock lock(m_mutex);
    return m_interop.interopified;
}



// Reconcile a color conversion whose local resolution failed. Fires only when
// a requested name is a registry-known interop identity this config lacks; a
// locally-absent, registry-unknown name is a genuine unknown and is left to
// the caller's today's-error path -- only the foreign endpoint may come from
// the identities config, a name unknown to both configs is declined (this
// asymmetry is deliberate: it never masks a typo). The foreign endpoint is
// drawn from the built-in interop identities config; the local endpoint from
// this config's in-memory, repaired ("interopified") copy, which carries the
// interchange role GetProcessorFromConfigs bridges through.
//
// Returned-handle / errmsg contract (errmsg is always assigned):
//   * bridged success   -> non-null handle, errmsg empty.
//   * lenient fallback   -> non-null pass-through no-op handle, errmsg set to a
//     continue-message (non-strict parsing: reconciliation could not build the
//     transform, but the pipeline proceeds -- the oiiotool --colorconvert:
//     strict=0 idiom, one layer down).
//   * strict hard error  -> null handle, errmsg set to a why + how-to-fix
//     message (OCIO strict parsing restores today's hard-error behavior).
//   * declined (feature N/A / builtin configs disabled) -> null handle, errmsg
//     empty (caller keeps today's OCIO error).
//
// The gate consults the interoperability state (the repaired copy actually
// resolves a scene interchange), never bare name-presence. Every reconciliation
// it triggers emits a single, complete OIIO::debug narration; nothing is
// silent, and no OCIO exception crosses this boundary.
ColorProcessorHandle
ColorConfig::Impl::reconcile_cross_config(string_view src, string_view dst,
                                          std::string& errmsg) const
{
    errmsg.clear();
    if (!config_ || disable_ocio || disable_builtin_configs)
        return {};

    OCIO::ConstConfigRcPtr ids = build_interop_identities_config();
    if (!ids)
        return {};

    // Classify each endpoint: does this config resolve it locally, and if not,
    // is it a registry-known interop identity? A locally-absent, registry-known
    // name is the "foreign" endpoint the bridge serves.
    const std::string s(src), d(dst);
    const std::string s_local = try_canonical_name(config_, s.c_str());
    const std::string d_local = try_canonical_name(config_, d.c_str());
    const std::string s_reg = s_local.empty() ? try_canonical_name(ids, s.c_str())
                                              : std::string();
    const std::string d_reg = d_local.empty() ? try_canonical_name(ids, d.c_str())
                                              : std::string();
    const bool src_foreign = s_local.empty() && !s_reg.empty();
    const bool dst_foreign = d_local.empty() && !d_reg.empty();

    // Feature applies only when at least one endpoint is a registry-known
    // identity this config lacks AND the other endpoint resolves locally (or is
    // itself foreign). Any locally-absent, registry-unknown endpoint is a
    // genuine unknown: decline, leaving today's error untouched.
    if (!src_foreign && !dst_foreign)
        return {};
    if (!src_foreign && s_local.empty())
        return {};
    if (!dst_foreign && d_local.empty())
        return {};

    // OCIO strict parsing opts out of the lenient bridge entirely: preserve
    // today's hard-error behavior exactly (the strict-facility user story).
    bool strict = true;
    try {
        strict = config_->isStrictParsingEnabled();
    } catch (...) {
    }

    const std::string foreign_name = src_foreign ? s : d;

    if (strict) {
        // Strict mode never touches the interopify/repair machinery -- the
        // membership check above (against the already-built, memoized
        // identities config) is all that is needed to compose the hard
        // error, so skip interopifiedConfig()'s repair/bootstrap entirely.
        errmsg = Strutil::fmt::format(
            "Could not reconcile color conversion \"{}\" -> \"{}\": OCIO "
            "strict parsing is enabled. \"{}\" is a registry-known interop "
            "identity this config does not define; add the aces_interchange "
            "role (and a matching color space) to \"{}\", or use a color "
            "space name this config defines.",
            src, dst, foreign_name, configname());
        Strutil::debug("OpenImageIO ColorConfig(\"{}\"): {}\n", configname(),
                       errmsg);
        return {};
    }

    // Gate on the interoperability state, not name-presence: only bridge when
    // in-memory detection/repair produced a copy that resolves the scene
    // interchange role GetProcessorFromConfigs needs.
    OCIO::ConstConfigRcPtr bridge = interopifiedConfig();
    const bool gate_open = bridge && interopifiedResolvesSceneInterchange();

    OCIO::ConstProcessorRcPtr proc;
    std::string ocio_err;
    if (gate_open) {
        // The repaired copy is a superset of config_, so the local endpoint
        // still resolves there; the foreign endpoint comes from the identities
        // config.
        OCIO::ConstConfigRcPtr src_cfg = src_foreign ? ids : bridge;
        OCIO::ConstConfigRcPtr dst_cfg = dst_foreign ? ids : bridge;
        std::string src_name = src_foreign ? s_reg
                                           : try_canonical_name(bridge, s.c_str());
        std::string dst_name = dst_foreign ? d_reg
                                           : try_canonical_name(bridge, d.c_str());
        if (src_name.empty())
            src_name = s;
        if (dst_name.empty())
            dst_name = d;

        // R2(b)/R4(b): narrate every cross-config route as one complete message.
        Strutil::debug(
            "OpenImageIO ColorConfig(\"{}\"): reconciling color conversion "
            "across configs -- source \"{}\" ({}) -> destination \"{}\" ({})\n",
            configname(), src_name,
            src_foreign ? "interop identities config" : "this config", dst_name,
            dst_foreign ? "interop identities config" : "this config");

        proc = processor_from_configs(src_cfg, src_name, dst_cfg, dst_name,
                                      ocio_err);
        if (proc) {
            try {
                return ColorProcessorHandle(new ColorProcessor_OCIO(proc));
            } catch (OCIO::Exception& e) {
                ocio_err = e.what();
            } catch (...) {
                ocio_err = "Unknown error constructing cross-config processor";
            }
        }
    }

    // Reconciliation did not complete: the gate is closed, or the bridge could
    // not build the transform. (Strict parsing already returned above.)
    // Compose one complete why + how-to-fix message (the ColorConfig error
    // string is overwrite-on-set).
    std::string why;
    if (!gate_open)
        why = Strutil::fmt::format(
            "config \"{}\" is not color-interoperable (no scene interchange "
            "role could be found or repaired)",
            configname());
    else
        why = ocio_err.empty() ? "the interop bridge could not build the "
                                 "transform"
                               : ocio_err;
    errmsg = Strutil::fmt::format(
        "Could not reconcile color conversion \"{}\" -> \"{}\": {}. \"{}\" is a "
        "registry-known interop identity this config does not define; add the "
        "aces_interchange role (and a matching color space) to \"{}\", or use a "
        "color space name this config defines.",
        src, dst, why, foreign_name, configname());

    // Lenient parsing: warn (debug) and continue with a pass-through no-op so
    // the pipeline proceeds. The message is still recorded on the ColorConfig
    // for callers that surface it.
    Strutil::debug("OpenImageIO ColorConfig(\"{}\"): {} Continuing with a "
                   "pass-through (non-strict parsing).\n",
                   configname(), errmsg);
    return ColorProcessorHandle(new ColorProcessor_Matrix(Imath::M44f(),
                                                          false));
}



// Display-view sibling of reconcile_cross_config, mirroring its strict/lenient/
// narration contract exactly (one policy, two routes). The display and view are
// inherently local to this config; only the INPUT (source) color space can be a
// locally-absent, registry-known interop identity -- the "foreign" endpoint the
// bridge serves. The foreign source comes from the built-in interop identities
// config; the local display/view from this config's in-memory repaired copy,
// which carries the interchange role OCIO's display-view GetProcessorFromConfigs
// bridges through.
//
// Returned-handle / errmsg contract matches reconcile_cross_config:
//   * bridged success  -> non-null handle, errmsg empty.
//   * lenient fallback  -> non-null pass-through no-op handle, errmsg set to the
//     continue-message (non-strict parsing). Critically, the prototype's silent
//     setSrc(ROLE_SCENE_LINEAR) "continue anyway" is NOT taken here: an
//     unbridgeable source stays untouched (pass-through), never reinterpreted as
//     scene_linear.
//   * strict hard error -> null handle, errmsg set to why + how-to-fix.
//   * declined (input resolves locally, or is a genuine unknown, or feature
//     N/A) -> null handle, errmsg empty (caller keeps today's OCIO error).
//
// TODO: the cross-config display route does not carry a looks override --
// looks are config-local and the cross-config display bridge has none. A
// foreign source that also needs a looks override is not handled yet; add
// looks support to this route if/when a caller needs looks applied across
// configs.
ColorProcessorHandle
ColorConfig::Impl::reconcile_cross_config_display(string_view input,
                                                  string_view display,
                                                  string_view view, bool inverse,
                                                  std::string& errmsg) const
{
    errmsg.clear();
    if (!config_ || disable_ocio || disable_builtin_configs)
        return {};

    OCIO::ConstConfigRcPtr ids = build_interop_identities_config();
    if (!ids)
        return {};

    // Only the input can be foreign. If it resolves locally, today's local
    // display path already handles it -- decline. If it is locally absent but
    // registry-unknown, it is a genuine unknown: decline, leaving today's error.
    const std::string in(input);
    const std::string in_local = try_canonical_name(config_, in.c_str());
    if (!in_local.empty())
        return {};
    const std::string in_reg = try_canonical_name(ids, in.c_str());
    if (in_reg.empty())
        return {};

    // OCIO strict parsing opts out of the lenient bridge entirely (today's
    // hard-error behavior), exactly as the color-space route does.
    bool strict = true;
    try {
        strict = config_->isStrictParsingEnabled();
    } catch (...) {
    }

    if (strict) {
        // Strict mode never touches the interopify/repair machinery -- the
        // membership check above (against the already-built, memoized
        // identities config) is all that is needed to compose the hard
        // error, so skip interopifiedConfig()'s repair/bootstrap entirely.
        errmsg = Strutil::fmt::format(
            "Could not reconcile display transform \"{}\" -> display \"{}\" "
            "view \"{}\": OCIO strict parsing is enabled. \"{}\" is a "
            "registry-known interop identity this config does not define; "
            "add the aces_interchange role (and a matching color space) to "
            "\"{}\", or use a color space name this config defines.",
            input, display, view, input, configname());
        Strutil::debug("OpenImageIO ColorConfig(\"{}\"): {}\n", configname(),
                       errmsg);
        return {};
    }

    // Gate on the interoperability state, not name-presence: only bridge when
    // in-memory detection/repair produced a copy resolving the scene interchange
    // role the display-view chokepoint needs.
    OCIO::ConstConfigRcPtr bridge = interopifiedConfig();
    const bool gate_open = bridge && interopifiedResolvesSceneInterchange();

    OCIO::ConstProcessorRcPtr proc;
    std::string ocio_err;
    if (gate_open) {
        const OCIO::TransformDirection dir
            = inverse ? OCIO::TRANSFORM_DIR_INVERSE : OCIO::TRANSFORM_DIR_FORWARD;

        // R3/R4(b): narrate the cross-config display route as one complete
        // message (say "display transform" so the route is identifiable).
        Strutil::debug(
            "OpenImageIO ColorConfig(\"{}\"): reconciling display transform "
            "across configs -- source \"{}\" (interop identities config) -> "
            "display \"{}\" view \"{}\" (this config)\n",
            configname(), in_reg, display, view);

        proc = display_processor_from_configs(ids, in_reg, bridge, display, view,
                                              dir, ocio_err);
        if (proc) {
            try {
                return ColorProcessorHandle(new ColorProcessor_OCIO(proc));
            } catch (OCIO::Exception& e) {
                ocio_err = e.what();
            } catch (...) {
                ocio_err = "Unknown error constructing cross-config display "
                           "processor";
            }
        }
    }

    // Reconciliation did not complete: the gate is closed, or the bridge could
    // not build the transform. (Strict parsing already returned above.)
    // Compose one complete why + how-to-fix message (the ColorConfig error
    // string is overwrite-on-set).
    std::string why;
    if (!gate_open)
        why = Strutil::fmt::format(
            "config \"{}\" is not color-interoperable (no scene interchange "
            "role could be found or repaired)",
            configname());
    else
        why = ocio_err.empty() ? "the interop bridge could not build the "
                                 "transform"
                               : ocio_err;
    errmsg = Strutil::fmt::format(
        "Could not reconcile display transform \"{}\" -> display \"{}\" view "
        "\"{}\": {}. \"{}\" is a registry-known interop identity this config "
        "does not define; add the aces_interchange role (and a matching color "
        "space) to \"{}\", or use a color space name this config defines.",
        input, display, view, why, input, configname());

    // Lenient parsing: warn (debug) and continue with a pass-through no-op.
    // On bridge failure, do NOT fall back to treating the input as
    // scene_linear -- that silently reinterprets pixels; take the
    // strict-aware error path instead. Here (non-strict), that means the
    // pixels pass through unchanged, and the message is recorded on the
    // ColorConfig for callers that surface it.
    Strutil::debug("OpenImageIO ColorConfig(\"{}\"): {} Continuing with a "
                   "pass-through (non-strict parsing).\n",
                   configname(), errmsg);
    return ColorProcessorHandle(new ColorProcessor_Matrix(Imath::M44f(),
                                                          false));
}



bool
ColorConfig::Impl::interopIsInteroperable() const
{
    ensure_interop();
    spin_rw_read_lock lock(m_mutex);
    return m_interop.is_interoperable;
}



std::string
ColorConfig::Impl::interopInterchangeName() const
{
    ensure_interop();
    spin_rw_read_lock lock(m_mutex);
    return m_interop.interchange_colorspace;
}



bool
ColorConfig::Impl::interopComputed() const
{
    spin_rw_read_lock lock(m_mutex);
    return m_interop_ready;
}



bool
ColorConfig::Impl::interopWarned() const
{
    ensure_interop();
    spin_rw_read_lock lock(m_mutex);
    return m_interop.warned;
}



bool
ColorConfig::Impl::interopifiedResolvesSceneInterchange() const
{
    ensure_interop();
    OCIO::ConstConfigRcPtr cfg;
    {
        spin_rw_read_lock lock(m_mutex);
        cfg = m_interop.interopified;
    }
    if (!cfg)
        return false;
    try {
        if (cfg->getColorSpace(OCIO::ROLE_INTERCHANGE_SCENE))
            return true;
        const char* canon = cfg->getCanonicalName(OCIO::ROLE_INTERCHANGE_SCENE);
        return canon && *canon;
    } catch (...) {
        return false;
    }
}



bool
ColorConfig::Impl::interopifiedCacheOff() const
{
    ensure_interop();
    OCIO::ConstConfigRcPtr cfg;
    {
        spin_rw_read_lock lock(m_mutex);
        cfg = m_interop.interopified;
    }
    return cfg && cfg->getProcessorCacheFlags() == OCIO::PROCESSOR_CACHE_OFF;
}



namespace {

// Process-global flyweight color space fingerprint cache. Keyed on
// (structural config cache id, context cache id, color space name) with a
// context-invariant bucket collapse (see fingerprint_cache_key). Reuses OIIO's
// existing sharded concurrent map -- find_or_insert is exactly the
// first-writer-wins publish this needs, retrieve() is the cheap read-locked
// hit. Content-addressed: a changed config or context simply produces new keys,
// so stale entries orphan harmlessly -- there is no invalidation or eviction
// path. clear() exists only for test/debug reset, never called from steady
// state (see fingerprint_cache_reset).
using FingerprintCache
    = unordered_map_concurrent<std::string, OIIO::pvt::ColorSpaceFingerprint>;

FingerprintCache&
fingerprint_cache()
{
    static FingerprintCache cache;
    return cache;
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
    auto result = cache.find_or_insert(key, *computed);
    return result.first->second;  // the published value (possibly another
                                  // thread's, on race)
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
    auto& cache       = fingerprint_cache();
    std::size_t count = 0;
    for (auto& entry : fingerprints) {
        const bool invariant
            = (analysisFlags(entry.first) & CSInfo::is_context_invariant) != 0;
        const std::string key = fingerprint_cache_key(cfgId, ctxId, invariant,
                                                      entry.first);
        cache.find_or_insert(key, entry.second);
        ++count;
    }
    return count;
}



//////////////////////////////////////////////////////////////////////////
//
// Color-space search by characterization (pvt::find_color_spaces): resolve a
// set of partial-characterization hints, then walk the config in index order
// keeping every color space whose derivable gamut / transfer / encoding /
// image-state satisfies each hinted axis under the three-valued filter
// (pvt::three_valued_axis). The term grammar and axis combination are pure
// (characterization_search.cpp); everything here needs the live config.

// The pure search primitives and shared types are declared in the library's
// non-versioned pvt namespace (imageio_pvt.h); alias it so this versioned
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
    const size_t sep = family.find('_');
    if (sep != std::string::npos)
        family.resize(sep);
    return family;
}

// A context carrying this query's per-call variable overrides, layered on the
// config's current context. Overrides are scoped to the one search.
ConstContextRcPtr
make_context_with_overrides(const ConstConfigRcPtr& config,
                            const std::map<std::string, std::string>& vars)
{
    if (!config)
        return nullptr;
    ConstContextRcPtr context = config->getCurrentContext();
    if (!vars.empty()) {
        ContextRcPtr ctx = context->createEditableCopy();
        for (const auto& kv : vars)
            ctx->setStringVar(kv.first.c_str(), kv.second.c_str());
        context = ctx;
    }
    return context;
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
            outputs.push_back(
                (double(rgb[0]) + double(rgb[1]) + double(rgb[2])) / 3.0);
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
        const std::string alias        = nt->getAlias(i) ? nt->getAlias(i) : "";
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
            const auto child = inspect_authored_transform(
                config, context, group->getTransform(i), visited);
            result.allowed &= child.allowed;
            result.has_file_transform |= child.has_file_transform;
        }
        return result;
    }
    case TRANSFORM_TYPE_FILE: {
        auto file = DynamicPtrCast<const FileTransform>(transform);
        if (!file || !file->getSrc())
            return { false, true };
        const std::string source = context
                                       ? context->resolveStringVar(
                                             file->getSrc())
                                       : std::string(file->getSrc());
        const bool allowed = Strutil::iends_with(source, ".spi1d")
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



int
ColorConfig::Impl::compute_analysis_flags(const std::string& name,
                                          bool& active) const
{
    // Mirrors analyze() but for any name (including spaces outside the active
    // CSInfo inventory). Gather lock-taking work before any caller lock.
    const std::vector<std::string>& simple = getSimpleColorSpaces();

    int flagval = 0;
    active      = true;
    if (config_ && !disable_ocio) {
        OCIO::ConstColorSpaceRcPtr ocs = config_->getColorSpace(name.c_str());
        if (ocs) {
            if (ocs->isData())
                flagval |= CSInfo::is_data;
            if (ocs->hasCategory("is-unique"))
                flagval |= CSInfo::is_unique;

            const char* sp   = config_->getSearchPath();
            bool sp_has_vars = sp && Strutil::contains(sp, "$");
            if (!transformUsesContextVars(
                    ocs->getTransform(OCIO::COLORSPACE_DIR_TO_REFERENCE),
                    sp_has_vars)
                && !transformUsesContextVars(
                    ocs->getTransform(OCIO::COLORSPACE_DIR_FROM_REFERENCE),
                    sp_has_vars))
                flagval |= CSInfo::is_context_invariant;

            active      = false;
            const int n = config_->getNumColorSpaces(
                OCIO::SEARCH_REFERENCE_SPACE_ALL, OCIO::COLORSPACE_ACTIVE);
            for (int i = 0; i < n; ++i) {
                const char* aname = config_->getColorSpaceNameByIndex(
                    OCIO::SEARCH_REFERENCE_SPACE_ALL, OCIO::COLORSPACE_ACTIVE,
                    i);
                if (aname && name == aname) {
                    active = true;
                    break;
                }
            }
        }
    }
    if (std::binary_search(simple.begin(), simple.end(), name))
        flagval |= CSInfo::is_simple;
    else if (!(flagval & CSInfo::is_data))
        flagval |= CSInfo::has_complex_transform;
    if ((flagval & (CSInfo::is_data | CSInfo::is_unique))
        || isLearnedComplex(name))
        flagval |= CSInfo::should_skip_matching;
    return flagval;
}



std::string
ColorConfig::Impl::effectiveEncoding(string_view name) const
{
    if (!config_ || disable_ocio)
        return {};
    std::string resolved(resolve(name));
    auto cs = config_->getColorSpace(resolved.c_str());
    if (cs && cs->getEncoding() && cs->getEncoding()[0])
        return cs->getEncoding();
    // Fall back to the encoding declared by the interop-identity equivalent.
    std::string id(m_self->get_color_interop_id(resolved));
    if (!id.empty()) {
        if (auto registry = build_interop_identities_config())
            if (auto ics = registry->getColorSpace(id.c_str()))
                if (ics->getEncoding() && ics->getEncoding()[0])
                    return ics->getEncoding();
    }
    return {};
}



std::optional<spvt::Chromaticities>
ColorConfig::Impl::deriveChromaticities(string_view name) const
{
    if (!config_ || disable_ocio || name.empty())
        return {};
    std::string resolved(resolve(name));
    // Reserved/registry table first: a space that resolves to a known interop
    // id uses that id's reserved primaries (single hypothesis, exact ==).
    if (auto reserved = spvt::reserved_chromaticities_for_id(
            std::string(m_self->get_color_interop_id(resolved))))
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
        auto proc = config_->getProcessor(resolved.c_str(),
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
ColorConfig::Impl::deriveTransferSignature(string_view name) const
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
        auto proc = config_->getProcessor(source.c_str(), resolved.c_str());
        sig       = probe_signature_over(proc ? proc->getDefaultCPUProcessor()
                                              : OCIO::ConstCPUProcessorRcPtr());
    } catch (...) {
        return {};
    }
    if (!sig)
        return {};
    sig->encoding = effectiveEncoding(resolved);
    sig->family   = tf_curve_family(m_self->get_color_interop_id(resolved));
    return sig;
}



std::vector<std::string>
ColorConfig::Impl::find_color_spaces(
    const spvt::FindColorSpacesOptions& options)
{
    if (!options.include_active && !options.include_inactive)
        return {};
    if (!config_ || disable_ocio)
        return {};

    // Context overrides are scoped to this one query -- resolve and probe
    // everything below against a context copy carrying the overrides.
    const OCIO::ConstContextRcPtr ctx = make_context_with_overrides(
        config_, options.context);
    const OCIO::ConstConfigRcPtr registry = build_interop_identities_config();

    // ---------------- Hint resolution (fail-fast, pre-walk) ----------------
    // Per axis: exact local name -> known interop id -> axis-specific
    // fallback. Every unresolvable hint throws std::invalid_argument here,
    // before a single candidate is examined.

    auto local_space = [&](const std::string& raw) {
        return config_->getColorSpace(raw.c_str());
    };
    auto registry_space
        = [&](const std::string& raw) -> OCIO::ConstColorSpaceRcPtr {
        return registry ? registry->getColorSpace(raw.c_str())
                        : OCIO::ConstColorSpaceRcPtr();
    };
    auto reference_state = [](const OCIO::ConstColorSpaceRcPtr& cs) {
        return std::string(cs->getReferenceSpaceType()
                                   == OCIO::REFERENCE_SPACE_DISPLAY
                               ? "display"
                               : "scene");
    };

    // The interop-identity twin's encoding for a candidate (empty when the
    // space has no identity or the identity has no registry entry).
    auto twin_encoding = [&](const std::string& name) -> std::string {
        std::string id(m_self->get_color_interop_id(name));
        if (id.empty() || !registry)
            return {};
        auto ics = registry->getColorSpace(id.c_str());
        if (ics && ics->getEncoding() && ics->getEncoding()[0])
            return Strutil::lower(ics->getEncoding());
        return {};
    };

    auto resolve_encoding
        = [&](const std::string& raw) -> std::vector<std::string> {
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
            const int nn = cfg->getNumColorSpaces(
                OCIO::SEARCH_REFERENCE_SPACE_ALL, OCIO::COLORSPACE_ALL);
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

    auto resolve_state
        = [&](const std::string& raw) -> std::vector<std::string> {
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

    auto resolve_chromaticities
        = [&](const std::string& raw) -> std::vector<spvt::Chromaticities> {
        if (auto local = local_space(raw)) {
            if (auto value = deriveChromaticities(local->getName()))
                return { *value };
            throw std::invalid_argument(
                "chromaticities hint has no derivable value: " + raw);
        }
        // Registry chromaticities come from the reserved-primaries table
        // (table-only; gamuts absent from the table, e.g. ciexyzd65, are
        // documented as not-yet-resolvable, sharing the probe-port follow-on).
        if (auto rcs = registry_space(raw)) {
            if (auto value = spvt::reserved_chromaticities_for_id(rcs->getName()))
                return { *value };
            throw std::invalid_argument(
                "chromaticities hint has no derivable value: " + raw);
        }
        // Gamut-component fragment: the complete component of some registry id.
        const std::string component = Strutil::lower(raw);
        std::vector<spvt::Chromaticities> values;
        if (registry) {
            const int nn = registry->getNumColorSpaces(
                OCIO::SEARCH_REFERENCE_SPACE_ALL, OCIO::COLORSPACE_ALL);
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
            term.mode = mode;
            spvt::TransferHint& hint = term.hint;
            if (auto local = local_space(value)) {
                const std::string name = local->getName();
                try {
                    hint.identity = isColorSpaceLinear(name);
                    hint.family   = tf_curve_family(
                        m_self->get_color_interop_id(name));
                } catch (...) {
                }
                if (!hint.identity)
                    if (auto sig = deriveTransferSignature(name))
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
                    const std::string transform_name
                        = Strutil::lower(nt->getName() ? nt->getName() : "");
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
                            if (auto registered = find_named_transform(
                                    registry, transform_name)) {
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
            || isLearnedComplex(name))
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
            const auto authored = inspect_authored_transform(config_, ctx,
                                                             transform, visited);
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
                markLearnedComplex(name);
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

        // Per-candidate probes are wrapped so any derivation failure yields an
        // "unknown" property (three-valued), never an abort.
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
                std::string twin = twin_encoding(name);
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
            chromaticities = deriveChromaticities(name);
        if (!chromaticity_axis_accepts(chromaticity_terms, chromaticities))
            continue;

        spvt::TransferProperty transfer;
        if (!transfer_terms.empty()) {
            try {
                transfer.identity = isColorSpaceLinear(name);
                transfer.family   = tf_curve_family(
                    m_self->get_color_interop_id(name));
            } catch (...) {
            }
            if (!transfer.identity)
                if (auto sig = deriveTransferSignature(name))
                    transfer.signature = std::move(*sig);
        }
        if (!transfer_axis_accepts(transfer_terms, transfer))
            continue;

        result.push_back({ (flags & CSInfo::is_context_invariant) ? 0 : 1,
                           active ? 0 : 1,
                           (flags & CSInfo::is_simple) ? 0 : 1, name });
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
// namespace by imageio_pvt.h, so they must be defined there too, not inside
// the ABI-versioned v3_1 namespace the helpers above live in.
OIIO_NAMESPACE_BEGIN

namespace pvt {

int
interop_identities_config_size()
{
    auto config = v3_1::build_interop_identities_config();
    return config ? config->getNumColorSpaces() : 0;
}

bool
copy_config_preserves_default_view_transform()
{
    return v3_1::copy_config_default_vt_probe();
}

bool
interop_identities_config_resolves(string_view interop_id)
{
    auto config = v3_1::build_interop_identities_config();
    if (!config || interop_id.empty())
        return false;
    try {
        // getColorSpace resolves by color space name or alias, which is how
        // every CIF identity in this config is reachable (see the config's
        // per-entry name/alias scheme).
        return bool(config->getColorSpace(std::string(interop_id).c_str()));
    } catch (OCIO::Exception&) {
        return false;
    }
}

std::vector<std::string>
interop_identities_config_names()
{
    std::vector<std::string> names;
    auto config = v3_1::build_interop_identities_config();
    if (!config)
        return names;
    int n = config->getNumColorSpaces();
    names.reserve(n);
    for (int i = 0; i < n; ++i)
        names.emplace_back(config->getColorSpaceNameByIndex(i));
    return names;
}


std::vector<std::string>
embedded_interop_identities_ids()
{
    // Line-scan the embedded registry YAML for `interop_id: <token>` -- the
    // same token scan gen_color_interop_ids.py performs on the source file,
    // so this is byte-for-byte the set the generated ColorInteropIDs
    // constants were emitted from, independent of the linked OCIO version
    // (the parsed composite config's declared names diverge from the
    // canonical id set with OCIO >= 2.5's studio-config overlay).
    std::set<std::string> ids;
    string_view yaml(kInteropIdentitiesConfig);  // array is NUL-terminated
    for (string_view line : Strutil::splitsv(yaml, "\n")) {
        line = Strutil::strip(line);
        if (Strutil::parse_prefix(line, "interop_id:"))
            ids.emplace(Strutil::strip(line));
    }
    return { ids.begin(), ids.end() };
}


std::vector<std::string>
legacy_interop_id_table_names()
{
    std::vector<std::string> names;
    for (const auto& interop : v3_1::color_interop_ids)
        names.emplace_back(std::string(interop.interop_id));
    return names;
}


int
color_space_analysis_flags(const ColorConfig& config, string_view name,
                           bool* active)
{
    auto* impl = v3_1::pvt::ColorConfigClassificationPeek::impl(config);
    if (!impl) {
        if (active)
            *active = false;
        return 0;
    }
    return impl->analysisFlags(name, active);
}

bool
color_space_analyzed(const ColorConfig& config, string_view name)
{
    auto* impl = v3_1::pvt::ColorConfigClassificationPeek::impl(config);
    return impl ? impl->analysisComputed(name) : false;
}


std::vector<std::string>
find_color_spaces(const ColorConfig& config,
                  const FindColorSpacesOptions& options)
{
    auto* impl = v3_1::pvt::ColorConfigClassificationPeek::impl(config);
    return impl ? impl->find_color_spaces(options) : std::vector<std::string>{};
}


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


bool
color_config_is_interoperable(const ColorConfig& config)
{
    auto* impl = v3_1::pvt::ColorConfigClassificationPeek::impl(config);
    return impl ? impl->interopIsInteroperable() : false;
}

std::string
color_config_interchange_name(const ColorConfig& config)
{
    auto* impl = v3_1::pvt::ColorConfigClassificationPeek::impl(config);
    return impl ? impl->interopInterchangeName() : std::string();
}

bool
color_config_interop_computed(const ColorConfig& config)
{
    auto* impl = v3_1::pvt::ColorConfigClassificationPeek::impl(config);
    return impl ? impl->interopComputed() : false;
}

bool
color_config_interop_warned(const ColorConfig& config)
{
    auto* impl = v3_1::pvt::ColorConfigClassificationPeek::impl(config);
    return impl ? impl->interopWarned() : false;
}

bool
color_config_interopified_resolves_scene_interchange(const ColorConfig& config)
{
    auto* impl = v3_1::pvt::ColorConfigClassificationPeek::impl(config);
    return impl ? impl->interopifiedResolvesSceneInterchange() : false;
}

bool
color_config_interopified_cache_off(const ColorConfig& config)
{
    auto* impl = v3_1::pvt::ColorConfigClassificationPeek::impl(config);
    return impl ? impl->interopifiedCacheOff() : false;
}


std::vector<float>
cross_config_probe(const ColorConfig& src_config, string_view src_name,
                   const ColorConfig& dst_config, string_view dst_name,
                   cspan<float> probe, string_view context_key,
                   string_view context_value)
{
    std::vector<float> out;
    auto* src_impl = v3_1::pvt::ColorConfigClassificationPeek::impl(src_config);
    auto* dst_impl = v3_1::pvt::ColorConfigClassificationPeek::impl(dst_config);
    if (!src_impl || !dst_impl)
        return out;
    OCIO::ConstConfigRcPtr sc = src_impl->config_;
    OCIO::ConstConfigRcPtr dc = dst_impl->config_;
    if (!sc || !dc) {
        dst_impl->error("Cross-config probe requires two OCIO-backed configs");
        return out;
    }
    if (probe.size() != 3) {
        dst_impl->error("Cross-config probe expects a 3-channel pixel");
        return out;
    }

    // A non-empty key/value pair drives the context-aware overload: set the var
    // on both configs' current contexts, exercising the chokepoint's 6-arg path.
    OCIO::ConstContextRcPtr sctx, dctx;
    if (context_key.size() && context_value.size()) {
        const std::string k(context_key), v(context_value);
        auto se = sc->getCurrentContext()->createEditableCopy();
        se->setStringVar(k.c_str(), v.c_str());
        sctx = se;
        auto de = dc->getCurrentContext()->createEditableCopy();
        de->setStringVar(k.c_str(), v.c_str());
        dctx = de;
    }

    std::string err;
    auto proc = v3_1::processor_from_configs(sc, src_name, dc, dst_name, err,
                                             sctx, dctx);
    if (!proc) {
        dst_impl->error("{}", err);
        return out;
    }
    // Probe-pixel comparison discipline (abs 1e-6/channel): apply the default
    // CPU processor to the caller's pixel and hand back the transformed floats.
    out.assign(probe.begin(), probe.end());
    try {
        proc->getDefaultCPUProcessor()->applyRGB(out.data());
    } catch (OCIO::Exception& e) {
        dst_impl->error("{}", e.what());
        out.clear();
    }
    return out;
}


std::vector<float>
identities_route_probe(const ColorConfig& config, string_view local_name,
                       string_view registry_name, cspan<float> probe)
{
    std::vector<float> out;
    auto* impl = v3_1::pvt::ColorConfigClassificationPeek::impl(config);
    if (!impl || probe.size() != 3)
        return out;
    // Route the config's repaired copy's local endpoint to the identities
    // config's registry endpoint through the same chokepoint the public
    // createColorProcessor bridge path uses -- the reference it must reproduce.
    OCIO::ConstConfigRcPtr bridge = impl->interopifiedConfig();
    OCIO::ConstConfigRcPtr ids    = v3_1::build_interop_identities_config();
    if (!bridge || !ids)
        return out;
    std::string err;
    auto proc = v3_1::processor_from_configs(bridge, local_name, ids,
                                             registry_name, err);
    if (!proc)
        return out;
    out.assign(probe.begin(), probe.end());
    try {
        proc->getDefaultCPUProcessor()->applyRGB(out.data());
    } catch (OCIO::Exception&) {
        out.clear();
    }
    return out;
}


std::vector<float>
identities_display_route_probe(const ColorConfig& config,
                               string_view registry_name, string_view display,
                               string_view view, cspan<float> probe)
{
    std::vector<float> out;
    auto* impl = v3_1::pvt::ColorConfigClassificationPeek::impl(config);
    if (!impl || probe.size() != 3)
        return out;
    // Route the identities config's registry source into this config's repaired
    // copy display/view through the same display-view chokepoint the public
    // createDisplayTransform bridge path uses -- the reference it must reproduce.
    OCIO::ConstConfigRcPtr bridge = impl->interopifiedConfig();
    OCIO::ConstConfigRcPtr ids    = v3_1::build_interop_identities_config();
    if (!bridge || !ids)
        return out;
    std::string err;
    auto proc = v3_1::display_processor_from_configs(ids, registry_name, bridge,
                                                     display, view,
                                                     OCIO::TRANSFORM_DIR_FORWARD,
                                                     err);
    if (!proc)
        return out;
    out.assign(probe.begin(), probe.end());
    try {
        proc->getDefaultCPUProcessor()->applyRGB(out.data());
    } catch (OCIO::Exception&) {
        out.clear();
    }
    return out;
}


IccIdentifyResult
identify_icc_profile(const ColorConfig& config, cspan<uint8_t> iccdata)
{
    return v3_1::identify_icc_profile_impl(config, iccdata);
}

bool
derive_mastering_volume(const ColorConfig& config, string_view display,
                        string_view view, MasteringDisplayVolume& volume)
{
    return v3_1::derive_mastering_volume_impl(config, display, view, volume);
}

}  // namespace pvt

OIIO_NAMESPACE_END
