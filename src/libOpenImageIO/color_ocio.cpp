// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <tsl/robin_map.h>

#include <OpenImageIO/Imath.h>

#include <OpenImageIO/color.h>
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
}  // namespace


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
        builtinconfig_ = cfg->createEditableCopy();
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
                config_ = cfg->createEditableCopy();
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



string_view
ColorConfig::Impl::resolve(string_view name) const
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
    constexpr ColorInteropID(const char* interop_id)
        : interop_id(interop_id)
        , cicp({ 0, 0, 0, 0 })
        , has_cicp(false)
    {
    }

    constexpr ColorInteropID(const char* interop_id, CICPPrimaries primaries,
                             CICPTransfer transfer, CICPMatrix matrix)
        : interop_id(interop_id)
        , cicp({ int(primaries), int(transfer), int(matrix),
                 int(CICPRange::Full) })
        , has_cicp(true)
    {
    }

    const char* interop_id;
    std::array<int, 4> cicp;
    bool has_cicp;
};

// Mapping between color interop ID and CICP, based on Color Interop Forum
// recommendations.
constexpr ColorInteropID color_interop_ids[] = {
    // Scene referred interop IDs first so they are the default in automatic
    // conversion from CICP to interop ID. Some are not display color spaces
    // at all, but can be represented by CICP anyway.
    { "lin_ap1_scene" },
    { "lin_ap0_scene" },
    { "lin_rec709_scene", CICPPrimaries::Rec709, CICPTransfer::Linear,
      CICPMatrix::BT709 },
    { "lin_p3d65_scene", CICPPrimaries::P3D65, CICPTransfer::Linear,
      CICPMatrix::BT709 },
    { "lin_rec2020_scene", CICPPrimaries::Rec2020, CICPTransfer::Linear,
      CICPMatrix::Rec2020_CL },
    { "lin_adobergb_scene" },
    { "lin_ciexyzd65_scene", CICPPrimaries::XYZD65, CICPTransfer::Linear,
      CICPMatrix::Unspecified },
    { "srgb_rec709_scene", CICPPrimaries::Rec709, CICPTransfer::sRGB,
      CICPMatrix::BT709 },
    { "g22_rec709_scene", CICPPrimaries::Rec709, CICPTransfer::Gamma22,
      CICPMatrix::BT709 },
    { "g18_rec709_scene" },
    { "srgb_ap1_scene" },
    { "g22_ap1_scene" },
    { "srgb_p3d65_scene", CICPPrimaries::P3D65, CICPTransfer::sRGB,
      CICPMatrix::BT709 },
    { "g22_adobergb_scene" },
    { "data" },
    { "unknown" },

    // Display referred interop IDs.
    { "srgb_rec709_display", CICPPrimaries::Rec709, CICPTransfer::sRGB,
      CICPMatrix::BT709 },
    { "g24_rec709_display", CICPPrimaries::Rec709, CICPTransfer::BT709,
      CICPMatrix::BT709 },
    { "srgb_p3d65_display", CICPPrimaries::P3D65, CICPTransfer::sRGB,
      CICPMatrix::BT709 },
    { "srgbe_p3d65_display", CICPPrimaries::P3D65, CICPTransfer::sRGB,
      CICPMatrix::BT709 },
    { "pq_p3d65_display", CICPPrimaries::P3D65, CICPTransfer::PQ,
      CICPMatrix::Rec2020_NCL },
    { "pq_rec2020_display", CICPPrimaries::Rec2020, CICPTransfer::PQ,
      CICPMatrix::Rec2020_NCL },
    { "hlg_rec2020_display", CICPPrimaries::Rec2020, CICPTransfer::HLG,
      CICPMatrix::Rec2020_NCL },
    // No CICP mapping to keep previous behavior unchanged, as Gamma 2.2
    // display is more likely meant to be written as sRGB. On read the
    // scene referred interop ID will be used.
    { "g22_rec709_display",
      /* CICPPrimaries::Rec709, CICPTransfer::Gamma22, CICPMatrix::BT709 */ },
    // No CICP code for Adobe RGB primaries.
    { "g22_adobergb_display" },
    { "g26_p3d65_display", CICPPrimaries::P3D65, CICPTransfer::Gamma26,
      CICPMatrix::BT709 },
    { "g26_xyzd65_display", CICPPrimaries::XYZD65, CICPTransfer::Gamma26,
      CICPMatrix::Unspecified },
    { "pq_xyzd65_display", CICPPrimaries::XYZD65, CICPTransfer::PQ,
      CICPMatrix::Unspecified },
};
}  // namespace

string_view
ColorConfig::get_color_interop_id(string_view colorspace) const
{
    if (colorspace.empty())
        return "";
#if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 5, 0)
    if (getImpl()->config_ && !disable_ocio) {
        const char* interop_id = nullptr;
        try {
            OCIO::ConstColorSpaceRcPtr c = getImpl()->config_->getColorSpace(
                std::string(resolve(colorspace)).c_str());
            if (c)
                interop_id = c->getInteropID();
        } catch (...) {
            interop_id = nullptr;
        }
        if (interop_id) {
            return interop_id;
        }
    }
#endif
    for (const ColorInteropID& interop : color_interop_ids) {
        if (equivalent(colorspace, interop.interop_id)) {
            return interop.interop_id;
        }
    }
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
    if (from.empty() || from == "current") {
        from = src.spec().get_string_attribute("oiio:Colorspace",
                                               "scene_linear");
    }
    if (from.empty() || from == "unknown" || to.empty() || to == "unknown") {
        dst.errorfmt("Unknown color space name (from=\"{}\", to=\"{}\")", from,
                     to);
        return false;
    }

    if (!colorconfig)
        colorconfig = &ColorConfig::default_colorconfig();

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
    // a no-op (non-strict parsing). No pixels moved, so (in the common,
    // forward direction) the output must keep documenting the source scene
    // space instead of claiming the requested display/view.
    bool lenient_passthrough = colorconfig->has_error();

    logtime.stop();  // transition to colorconvert
    bool ok = colorconvert(dst, src, processor.get(), unpremult, roi, nthreads);
    if (ok) {
        if (inverse)
            dst.specmod().set_colorspace(colorconfig->resolve(from));
        else {
            if (display.empty() || display == "default")
                display = colorconfig->getDefaultDisplayName();
            if (view.empty() || view == "default")
                view = colorconfig->getDefaultViewName(display,
                                                       colorconfig->resolve(
                                                           from));
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

#if OCIO_VERSION_HEX < MAKE_OCIO_VERSION_HEX(2, 3, 1)
// OCIO < 2.3.1 bug: Config::createEditableCopy() drops the source config's
// default view transform *name* (fixed in 2.3.1). Capture it from `src`
// before/around the copy and, if `copy` comes back with no default view
// transform name while `src` had one, re-set it. Shared shape so a later
// standalone fix for OIIO's own bootstrap-copy call sites can reuse it.
void
preserve_default_view_transform(const OCIO::ConstConfigRcPtr& src,
                                const OCIO::ConfigRcPtr& copy)
{
    if (!src || !copy)
        return;
    const char* srcName = src->getDefaultViewTransformName();
    if (!srcName || !*srcName)
        return;
    const char* copyName = copy->getDefaultViewTransformName();
    if (!copyName || !*copyName)
        copy->setDefaultViewTransformName(srcName);
}
#endif

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
        OCIO::ConfigRcPtr editable = config->createEditableCopy();
#if OCIO_VERSION_HEX < MAKE_OCIO_VERSION_HEX(2, 3, 1)
        preserve_default_view_transform(config, editable);
#endif
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
    try {
        if (!src_context && !dst_context)
            return OCIO::Config::GetProcessorFromConfigs(src_config, src.c_str(),
                                                         dst_config, dst.c_str());
        OCIO::ConstContextRcPtr sctx = src_context ? src_context
                                                   : src_config->getCurrentContext();
        OCIO::ConstContextRcPtr dctx = dst_context ? dst_context
                                                   : dst_config->getCurrentContext();
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
    try {
        if (!src_context && !dst_context)
            return OCIO::Config::GetProcessorFromConfigs(src_config, src.c_str(),
                                                         dst_config, disp.c_str(),
                                                         vw.c_str(), direction);
        OCIO::ConstContextRcPtr sctx = src_context ? src_context
                                                   : src_config->getCurrentContext();
        OCIO::ConstContextRcPtr dctx = dst_context ? dst_context
                                                   : dst_config->getCurrentContext();
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

    // Non-interoperable configs warn exactly once per structural config id
    // across the process (cross-config color features are otherwise silently
    // unavailable). This is a WARNING, not an error: it is deliberately never
    // written to the ColorConfig error string here. That string is a single
    // overwrite-on-set slot shared per config (see error()/geterror() above),
    // so setting it at bootstrap -- before any cross-config route is even
    // attempted -- would make has_error() true for callers who never touch
    // cross-config features, polluting otherwise-healthy same-config use and
    // risking a spurious trip of the uncaught-error exit dump. Instead: an
    // OIIO::debug line (attr/env-gated, never an unconditional stderr print --
    // R4) plus the same composed message recorded in-memory on this Impl,
    // available to callers/tests without touching the shared error string.
    // reconcile_cross_config{,_display} compose their own why+how-to-fix
    // error text only if/when a cross-config route is actually attempted and
    // fails (unchanged by this slice). Skip when builtin configs are
    // disabled -- we didn't actually assess interoperability in that case.
    if (!state.is_interoperable && config_ && !disable_builtin_configs) {
        const std::string key = get_config_cache_id(config_);
        if (!key.empty() && note_interop_warning(key)) {
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
            Strutil::debug("{}\n", state.warning_message);
            state.warned = true;
        }
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

}  // namespace pvt

OIIO_NAMESPACE_END
