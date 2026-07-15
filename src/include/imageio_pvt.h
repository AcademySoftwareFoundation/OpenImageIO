// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


/// \file
/// Declarations for things that are used privately by OpenImageIO.


#ifndef OPENIMAGEIO_IMAGEIO_PVT_H
#define OPENIMAGEIO_IMAGEIO_PVT_H

#include <mutex>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/thread.h>
#include <OpenImageIO/timer.h>



OIIO_NAMESPACE_BEGIN

// Note: Everything in pvt namespace is expected to be local to the library
// and does not appear in exported headers that client software will see.
// Therefore, it should all stay in the current namespace except where
// specifically noted.

namespace pvt {

// Mutex allowing thread safety of the ImageIO internals below
extern std::recursive_mutex imageio_mutex;

extern atomic_int oiio_threads;
extern atomic_int oiio_read_chunk;
extern atomic_int oiio_try_all_readers;
extern ustring font_searchpath;
extern ustring plugin_searchpath;
extern std::string format_list;
extern std::string input_format_list;
extern std::string output_format_list;
extern std::string extension_list;
extern std::string library_list;
extern OIIO_UTIL_API int oiio_print_debug;
extern OIIO_UTIL_API int oiio_print_uncaught_errors;
extern int oiio_log_times;
extern int openexr_core;
extern int jpeg_com_attributes;
extern int png_linear_premult;
extern int enable_hwy;
extern int limit_channels;
extern int limit_imagesize_MB;
extern int imagebuf_print_uncaught_errors;
extern int imagebuf_use_imagecache;
extern int imageinput_strict;
extern atomic_ll IB_local_mem_current;
extern atomic_ll IB_local_mem_peak;
extern std::atomic<float> IB_total_open_time;
extern std::atomic<float> IB_total_image_read_time;

// These live in libOpenImageIO_Util
extern OIIO_UTIL_API int oiio_use_tbb;
extern OIIO_UTIL_API int oiio_ustring_cleanup;

OIIO_API const std::vector<std::string>&
font_dirs();
OIIO_API const std::vector<std::string>&
font_file_list();
OIIO_API const std::vector<std::string>&
font_list();
OIIO_API const std::vector<std::string>&
font_family_list();
OIIO_API const std::vector<std::string>
font_style_list(string_view family);
OIIO_API const std::string
font_filename(string_view family, string_view style = "");


// Make sure all plugins are inventoried. For internal use only.
void
catalog_all_plugins(std::string searchpath);

// Inexpensive check if a file extension or format name corresponds to a
// procedural input plugin.
bool
is_procedural_plugin(const std::string& name);

/// Given the format, set the default quantization range.
void
get_default_quantize(TypeDesc format, long long& quant_min,
                     long long& quant_max) noexcept;

/// Turn potentially non-contiguous-stride data (e.g. "RGBxRGBx") into
/// contiguous-stride ("RGBRGB"), for any format or stride values (measured in
/// bytes).  Caller must pass in a dst pointing to enough memory to hold the
/// contiguous rectangle.  Return span where the contiguous data ended up,
/// which is either dst or src (if the strides indicated that data were
/// already contiguous).
OIIO_API span<const std::byte>
contiguize(const image_span<const std::byte>& src, span<std::byte> dst);

/// Turn potentially non-contiguous-stride data (e.g. "RGBxRGBx") into
/// contiguous-stride ("RGBRGB"), for any format or stride values
/// (measured in bytes).  Caller must pass in a dst pointing to enough
/// memory to hold the contiguous rectangle.  Return a ptr to where the
/// contiguous data ended up, which is either dst or src (if the strides
/// indicated that data were already contiguous).
OIIO_API const void*
contiguize(const void* src, int nchannels, stride_t xstride, stride_t ystride,
           stride_t zstride, void* dst, int width, int height, int depth,
           TypeDesc format);

/// Turn contiguous data from any format into float data.  Return a
/// pointer to the converted data (which may still point to src if no
/// conversion was necessary).
const float*
convert_to_float(const void* src, float* dst, int nvals, TypeDesc format);

/// Turn contiguous float data into any format.  Return a pointer to the
/// converted data (which may still point to src if no conversion was
/// necessary).
const void*
convert_from_float(const float* src, void* dst, size_t nvals, TypeDesc format);

/// A version of convert_from_float that will break up big jobs with
/// multiple threads.
const void*
parallel_convert_from_float(const float* src, void* dst, size_t nvals,
                            TypeDesc format);

/// Internal utility: Error checking on the spec -- if it contains texture-
/// specific metadata but there are clues it's not actually a texture file
/// written by maketx or `oiiotool -otex`, then assume these metadata are
/// wrong and delete them. Return true if we think it's one of these
/// incorrect files and it was fixed.
OIIO_API bool
check_texture_metadata_sanity(ImageSpec& spec);

/// Get the timing report from log_time entries.
OIIO_API std::string
timing_report();

/// An object that, if oiio_log_times is nonzero, logs time until its
/// destruction. If oiio_log_times is 0, it does nothing.
class LoggedTimer {
public:
    LoggedTimer(string_view name)
        : m_timer(oiio_log_times)
    {
        if (oiio_log_times)
            m_name = name;
    }
    ~LoggedTimer()
    {
        if (oiio_log_times)
            log_time(m_name, m_timer, m_count);
    }
    // Stop the timer. An optional count_offset will be added to the
    // "invocations count" of the underlying timer, if a single invocation
    // does not correctly describe the thing being timed.
    void stop(int count_offset = 0)
    {
        m_timer.stop();
        m_count += count_offset;
    }
    void start() { m_timer.start(); }
    void rename(string_view name) { m_name = name; }

private:
    Timer m_timer;
    std::string m_name;
    int m_count = 1;
};


// Access to an internal periodic blue noise table.
inline constexpr int bntable_res = 256;
extern float bluenoise_table[bntable_res][bntable_res][4];

// 1-channel value lookup of periodic blue noise of a 2D coordinate.
inline float
bluenoise_1chan(int x, int y)
{
    x &= bntable_res - 1;
    y &= bntable_res - 1;
    return bluenoise_table[y][x][0];
}

// 4-channel pointer lookup of periodic blue noise of a 2D coordinate.
inline const float*
bluenoise_4chan_ptr(int x, int y)
{
    x &= bntable_res - 1;
    y &= bntable_res - 1;
    return bluenoise_table[y][x];
}

// 4-channel pointer lookup of periodic blue noise of 3D coordinate + seed +
// channel channel number. The pointer is to the 4 floats of the mod 4 group
// of channels, i.e. if ch=5, the pointer will be to the 4 floats representing
// channels 4..7.
inline const float*
bluenoise_4chan_ptr(int x, int y, int z, int ch = 0, int seed = 0)
{
    if (z | (ch & ~3) | seed) {
        x += bjhash::bjfinal(z, ch, seed);
        y += bjhash::bjfinal(z, ch, seed + 83533);
    }
    x &= bntable_res - 1;
    y &= bntable_res - 1;
    return bluenoise_table[y][x];
}



struct print_info_options {
    bool verbose            = false;
    bool filenameprefix     = false;
    bool sum                = false;
    bool subimages          = false;
    bool compute_sha1       = false;
    bool compute_stats      = false;
    bool dumpdata           = false;
    bool dumpdata_showempty = true;
    bool dumpdata_C         = false;
    bool native             = false;
    std::string dumpdata_C_name;
    std::string metamatch;
    std::string nometamatch;
    std::string infoformat;
    size_t namefieldlength = 20;
    ROI roi;
};

OIIO_API std::string
compute_sha1(ImageInput* input, int subimage, int miplevel, std::string& err);
OIIO_API bool
print_stats(std::ostream& out, string_view indent, const ImageBuf& input,
            const ImageSpec& spec, ROI roi, std::string& err);



enum class ComputeDevice : int {
    CPU  = 0,
    CUDA = 1,
    // Might expand later...
};

// Which compute device is currently active, and should be used by any
// OIIO facilities that know how to use it.
OIIO_API ComputeDevice
compute_device();

#if 0
/// Return true if CUDA is available to OpenImageIO at this time -- support
/// enabled at build time, and has already been turned on with enable_cuda()
/// or with OIIO::attribute("cuda", 1), and hardware is present and was
/// successfully initialized.
OIIO_API bool
openimageio_cuda();
#endif

// Set an attribute related to OIIO's use of GPUs/compute devices. This is a
// strictly internal function. User code should just call OIIO::attribute()
// and GPU-related attributes will be directed here automatically.
OIIO_API bool
gpu_attribute(string_view name, TypeDesc type, const void* val);

// Retrieve an attribute related to OIIO's use of GPUs/compute devices. This
// is a strictly internal function. User code should just call
// OIIO::getattribute() and GPU-related attributes will be directed here
// automatically.
OIIO_API bool
gpu_getattribute(string_view name, TypeDesc type, void* val);


/// Allocate compute device memory
OIIO_API void*
device_malloc(size_t size);

/// Allocate unified compute device memory -- visible on both CPU & GPU
OIIO_API void*
device_unified_malloc(size_t size);

/// Free compute device memory
OIIO_API void
device_free(void* mem);


/// Number of color spaces in OIIO's built-in interop identities config --
/// a small OCIO config OIIO ships with (compiled in) that defines color
/// spaces for the CIF-published interop identities OIIO knows how to
/// reliably recognize and relate in other OCIO configs. The config is
/// parsed on first call and the result reused for the life of the
/// process. Returns 0 if OCIO support is unavailable or the embedded
/// config failed to parse. For internal/test use only.
OIIO_API int
interop_identities_config_size();

/// True if OIIO's built-in interop identities config (see
/// interop_identities_config_size) resolves `interop_id` -- i.e. the config
/// has a color space reachable by that name or alias. Returns false if OCIO
/// support is unavailable, the config failed to build, or the id is empty or
/// unknown. For internal/test use only.
OIIO_API bool
interop_identities_config_resolves(string_view interop_id);

/// The `name:` of every color space declared in OIIO's built-in interop
/// identities config (see interop_identities_config_size), in the config's
/// own enumeration order. By construction, each entry's `name:` equals its
/// `interop_id:` in the source config -- this does not include entries only
/// reachable as an alias. Empty if OCIO support is unavailable or the
/// embedded config failed to parse. For internal/test use only.
OIIO_API std::vector<std::string>
interop_identities_config_names();


// ---------------------------------------------------------------------------
// Color-space classification -- how ColorConfig internally classifies a
// color space for interop matching (the "simple" transform allowlist and
// related properties). For internal/test use only.
// ---------------------------------------------------------------------------

/// Classification bit flags for a color space. These mirror the internal
/// CSInfo classification bits exactly (a static_assert in color_ocio.cpp
/// keeps them in sync), so a color_space_analysis_flags() result can be
/// tested against them.
enum ColorSpaceAnalysis {
    ColorSpaceIsData              = 64,    // "isdata: true" in the config
    ColorSpaceIsUnique            = 128,   // has OCIO category "is-unique"
    ColorSpaceShouldSkipMatching  = 256,   // never a matching candidate
    ColorSpaceHasComplexTransform = 512,   // rejected by the simple allowlist
    ColorSpaceIsSimple            = 1024,  // member of the simple set
    ColorSpaceIsContextInvariant  = 2048,  // no context vars affect it
};

/// Compute (lazily, on first request for this config) and return the interop
/// classification flags (see ColorSpaceAnalysis) for the color space `name`
/// in `config`. `active`, if non-null, receives whether the space is part of
/// the config's active colorspace enumeration. Returns 0 for unknown names.
/// For internal/test use only.
OIIO_API int
color_space_analysis_flags(const ColorConfig& config, string_view name,
                           bool* active = nullptr);

/// Whether the lazy classification pass has already run for `name` in
/// `config`. Does NOT trigger classification -- used to verify that
/// constructing a ColorConfig does no classification work. Returns false for
/// unknown names. For internal/test use only.
OIIO_API bool
color_space_analyzed(const ColorConfig& config, string_view name);


// ---------------------------------------------------------------------------
// Color space fingerprints -- the probe-based numeric signature ColorConfig
// computes for a color space so equivalent spaces can be recognized by value
// rather than by name. For internal/test use only.
// ---------------------------------------------------------------------------

/// A color space fingerprint: the floats produced by transforming a fixed
/// probe from the reference role to the color space, tagged with which
/// reference kind (scene vs display) selected the probe. `values` is empty
/// when the space is unknown or cannot be probed.
struct ColorSpaceFingerprint {
    int reference_kind = 0;     ///< OCIO ReferenceSpaceType (0 scene, 1 display)
    std::vector<float> values;  ///< probe floats; empty if not computable
    bool computed() const { return !values.empty(); }
};

/// Compute the color space fingerprint for `name` in `config`. Probing runs on
/// a lazily built, processor-cache-disabled copy of the config and is
/// byte-reproducible across builds. Returns an empty fingerprint
/// (values.empty()) when the space is unknown or cannot be probed. For
/// internal/test use only.
OIIO_API ColorSpaceFingerprint
color_space_fingerprint(const ColorConfig& config, string_view name);

/// Whether two color space fingerprints denote the same color space under the
/// exact, tolerance-gated identity comparison: reference kinds must match,
/// vector lengths must match, and every identity-probe float must agree within
/// the fingerprint tolerance (the trailing linearity probes are excluded). For
/// internal/test use only.
OIIO_API bool
color_space_fingerprints_match(const ColorSpaceFingerprint& a,
                               const ColorSpaceFingerprint& b);

/// The names of `config`'s "simple" color spaces that were successfully
/// fingerprinted, in the deterministic sorted order the engine iterates them
/// (reusing the classification simple-space cache). For internal/test use only.
OIIO_API std::vector<std::string>
color_space_fingerprint_order(const ColorConfig& config);


// ---------------------------------------------------------------------------
// Color space fingerprint cache -- a process-global flyweight cache of the
// fingerprints above, keyed on (structural config cache id, context cache id,
// color space name) with a context-invariant bucket collapse so a
// context-invariant space is fingerprinted once and shared across every context
// of the same structural config. Content-addressed (no invalidation): a changed
// config or context yields new keys and orphans the old entries. For
// internal/test use only.
// ---------------------------------------------------------------------------

/// Return the fingerprint for `name` in `config`, from the process-global
/// fingerprint cache. A cache hit is a cheap read; a miss computes the
/// fingerprint (outside the cache lock) and publishes it first-writer-wins.
/// Returns an empty fingerprint when the space is unknown or cannot be probed.
/// For internal/test use only.
OIIO_API ColorSpaceFingerprint
color_space_fingerprint_cached(const ColorConfig& config, string_view name);

/// Fingerprint every "simple" color space in `config` and publish each into the
/// process-global fingerprint cache (the bulk "warm" pass). Returns how many
/// were fingerprinted. For internal/test use only.
OIIO_API size_t
color_space_fingerprint_warm(const ColorConfig& config);

/// The number of entries currently in the process-global fingerprint cache.
/// For internal/test use only.
OIIO_API size_t
color_space_fingerprint_cache_size();

/// Empty the process-global fingerprint cache. For test/debug reset only --
/// never needed in steady state, since keys are content-addressed. For
/// internal/test use only.
OIIO_API void
color_space_fingerprint_cache_reset();


// ---------------------------------------------------------------------------
// Config interoperability check -- whether a config resolves a scene-referred
// interchange space (ACES2065-1 / the aces_interchange role) that cross-config
// color features anchor on, and the in-memory "interopified" repair copy built
// for configs that don't. Computed lazily on first query; constructing a
// ColorConfig runs none of it. For internal/test use only.
// ---------------------------------------------------------------------------

/// Whether `config` is color-interoperable: it resolves a scene interchange
/// space by role, a well-known ACES2065-1 alias, or builtin identification.
/// Triggers the lazy interop bootstrap. For internal/test use only.
OIIO_API bool
color_config_is_interoperable(const ColorConfig& config);

/// The name of the scene interchange color space `config` resolves, or empty
/// if it is not interoperable. Triggers the lazy interop bootstrap. For
/// internal/test use only.
OIIO_API std::string
color_config_interchange_name(const ColorConfig& config);

/// Whether the lazy interop bootstrap has already run for `config`. Does NOT
/// trigger it -- used to verify that constructing a ColorConfig does no interop
/// work. For internal/test use only.
OIIO_API bool
color_config_interop_computed(const ColorConfig& config);

/// Whether `config` emitted the once-per-config "not color-interoperable"
/// warning (true only for the config instance that first warned for a given
/// config structure). Triggers the lazy interop bootstrap. For internal/test
/// use only.
OIIO_API bool
color_config_interop_warned(const ColorConfig& config);

/// Whether the interopified (repaired, in-memory) copy of `config` resolves a
/// scene interchange -- true even for non-interoperable configs once repaired.
/// Triggers the lazy interop bootstrap. For internal/test use only.
OIIO_API bool
color_config_interopified_resolves_scene_interchange(const ColorConfig& config);

/// Whether the interopified copy of `config` has its OCIO processor cache
/// disabled (as the one-shot probe path requires). Triggers the lazy interop
/// bootstrap. For internal/test use only.
OIIO_API bool
color_config_interopified_cache_off(const ColorConfig& config);

/// Exercise the central cross-config processor chokepoint: build a processor
/// from `src_name` in `src_config` to `dst_name` in `dst_config` through the
/// configs' shared OCIO interchange roles, apply it to the 3-channel `probe`
/// pixel, and return the transformed floats. A non-empty context_key/value
/// pair drives the chokepoint's context-aware overload. On failure returns an
/// empty vector and sets the error on `dst_config` (retrievable via
/// dst_config.geterror()); the chokepoint never throws. Comparisons should use
/// probe-pixel agreement (abs 1e-6/channel), not byte-identical processors. For
/// internal/test use only.
OIIO_API std::vector<float>
cross_config_probe(const ColorConfig& src_config, string_view src_name,
                   const ColorConfig& dst_config, string_view dst_name,
                   cspan<float> probe, string_view context_key = {},
                   string_view context_value = {});

/// Route `local_name` in `config`'s in-memory interop-repaired copy to
/// `registry_name` in the built-in interop identities config through the pvt
/// cross-config chokepoint, and apply the result to the 3-channel `probe`.
/// This is the reference the public ColorConfig::createColorProcessor bridge
/// path should reproduce (probe-pixel agreement, abs 1e-6/channel). Returns an
/// empty vector if the route can't be built. For internal/test use only.
OIIO_API std::vector<float>
identities_route_probe(const ColorConfig& config, string_view local_name,
                       string_view registry_name, cspan<float> probe);

/// Route `registry_name` in the built-in interop identities config into
/// `config`'s in-memory interop-repaired copy display/view (`display`, `view`)
/// through the pvt cross-config display-view chokepoint, and apply the result
/// to the 3-channel `probe`. This is the reference the public
/// ColorConfig::createDisplayTransform cross-config bridge path should reproduce
/// (probe-pixel agreement, abs 1e-6/channel). Returns an empty vector if the
/// route can't be built. For internal/test use only.
OIIO_API std::vector<float>
identities_display_route_probe(const ColorConfig& config,
                               string_view registry_name, string_view display,
                               string_view view, cspan<float> probe);


// ---------------------------------------------------------------------------
// Color interop ID grammar and sanitization -- the ID grammar and
// sanitization rules from the CIF recommendation "An ID for Color Interop"
// (Annexes B and C):
// https://github.com/AcademySoftwareFoundation/ColorInterop/wiki
// Pure, stateless functions; no OCIO dependency.
// ---------------------------------------------------------------------------

enum class InteropIdForm {
    INVALID,
    BASE,              // base
    INNER_BASE,        // inner:base
    OUTER_INNER_BASE,  // outer:inner:base (an "inner" of "local" is the
                        // reserved local-namespace form one layer up; the
                        // grammar itself does not special-case it)
    OUTER_BLANK_BASE,  // outer::base (blank inner)
};

// Parsed segments of a color interop ID. `form` is INVALID unless `id`
// matched one of the 4 legal forms; only the segments implied by `form`
// are populated.
struct InteropIdParts {
    InteropIdForm form = InteropIdForm::INVALID;
    std::string outer;
    std::string inner;
    std::string base;
};

// Parse and validate a color interop ID against the CIF Annex B grammar
// (0, 1, or 2 colons; 3+ is always invalid; every present segment must be
// a non-empty id-token, except the blank inner of the `outer::base` form).
OIIO_API InteropIdParts
parse_interop_id(const std::string& id);

// True if `id` matches one of the 4 legal interop ID forms. Never folds
// case or sanitizes -- an id containing uppercase or non-ASCII characters
// is simply invalid; only sanitize_id_token() repairs those.
OIIO_API bool
is_valid_interop_id(const std::string& id);

// Sanitize an arbitrary string into a valid interop id-token per CIF
// Annex C: fold A-Z to lowercase, remap a fixed set of punctuation,
// collapse each non-ASCII code point (however many UTF-8 bytes) to a
// single '^', and replace anything else unmapped with '*'.
OIIO_API std::string
sanitize_id_token(const std::string& token);

// Substring of `id` after the first colon (the separator is consumed).
// Unchanged if `id` has no colon. Not itself validated as an id -- used
// as an intermediate search key, e.g. strip_leftmost_namespace(
// "my-studio::srgb") == ":srgb" (the leading colon of the blank inner is
// retained, so the result is not itself "srgb").
OIIO_API std::string
strip_leftmost_namespace(const std::string& id);

// True for the 3 reserved, case-sensitive utility tokens that name a
// color state without a registry lookup: "data", "unknown", "bypass".
OIIO_API bool
is_utility_interop_id(const std::string& id);


// ---------------------------------------------------------------------------
// Read-side color-metadata reconciliation -- the one audited precedence
// cascade that turns the raw color attributes a reader deposited (CICP,
// ICC blob, chromaticities, gamma, colorInteropID, an ACES-container flag)
// into a single resolved color space designation, so plugins stop
// hand-rolling their own precedence. Called once, centrally, in the
// ImageInput open path after the plugin deposits raw attributes. The
// resolution engine below is pure (a ColorConfig + these value types); the
// same engine drives resolve() and its diagnostic explain() trace. For
// internal/test use only.
// ---------------------------------------------------------------------------

/// Which local assignments a resolved id is allowed to escape as.
enum class ColorResolutionScope {
    Lenient,     ///< a known interop id absent locally may still be returned
    ConfigOnly,  ///< the result must be a usable local assignment, else unknown
    ExactState,  ///< additionally bind scene/display referencing state
};

/// How scene/display candidates are ordered when substitution is allowed.
enum class ColorStatePreference { Auto, Scene, Display };

/// Whether (and where) OCIO FileRules sit in the cascade. Filenames may
/// influence resolution only through the two rungs this axis gates.
enum class ColorFileRules { Off, First, FallbackOnly };

/// Immutable facts a reader read out of the asset. An absent field makes
/// its rule inapplicable; a present-but-unusable field misses and falls
/// through. Format-derived facts (png_srgb, aces_image_container) are
/// deposited by the plugin, never re-derived mid-cascade.
struct ColorMetadataFacts {
    bool aces_image_container = false;
    std::string color_interop_id;
    std::vector<unsigned char> icc_profile;
    bool has_cicp    = false;
    int cicp[4]      = { 0, 0, 0, 0 };
    bool has_chromaticities = false;
    float chromaticities[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    bool has_gamma = false;
    float gamma    = 0.0f;
    bool png_srgb  = false;
};

/// Per-call context: filenames touch resolution only here, via the two
/// FileRules-gated rungs; `format`/`filename` also drive scene-vs-display
/// source inference; `failover` is tried after everything metadata misses.
struct ColorCallContext {
    std::string filename;
    std::string format;
    std::string failover;
};

/// A single locked snapshot of the whole read policy state, taken once per
/// call. The three typed axes plus the per-signal switches; the grammar,
/// names and defaults are owned by the color policy attribute spec
/// (`oiio:colorpolicy:read:*`). Every default reproduces main's behavior.
struct ColorReadPolicy {
    ColorResolutionScope scope         = ColorResolutionScope::Lenient;
    ColorStatePreference state_pref     = ColorStatePreference::Auto;
    ColorFileRules file_rules           = ColorFileRules::Off;
    bool ignore_cicp_for_png            = false;
    bool ignore_sidecar                 = false;
    /// Whether an all-miss falls back to the config's Default Assignment.
    /// Off reproduces main (a reader that determined nothing leaves the
    /// spec's color space untouched); a later named policy turns it on.
    bool apply_config_default           = false;

    /// Read every `oiio:colorpolicy:read:*` value ONCE, under one lock, from
    /// the global attribute table, optionally overridden by per-open config
    /// hints. No mid-call re-reads.
    static OIIO_API ColorReadPolicy snapshot(const ImageSpec* config_hints
                                             = nullptr);
};

/// The 13 cascade rules, in exact tested precedence order (ICC above CICP).
/// STRICT_PARSING is the terminal miss diagnostic, not a 14th source rule.
enum class ColorRule {
    ExplicitAssignment,
    AcesContainer,
    FileRulesFirst,
    ColorInteropID,
    IccProfile,
    Cicp,
    PngSrgb,
    ChromaticitiesAndGamma,
    Chromaticities,
    Gamma,
    FileRulesFallback,
    Failover,
    ConfigDefault,
    StrictParsing,
};

/// The outcome of visiting one rule. Inapplicable = the fact was absent.
enum class ColorRuleOutcome { Matched, Missed, Inapplicable, Invalid };

/// One in-flight trace step, recorded for every rule the engine visits.
struct ColorResolutionStep {
    ColorRule rule           = ColorRule::ConfigDefault;
    ColorRuleOutcome outcome = ColorRuleOutcome::Inapplicable;
    std::string candidate;
    std::string resolved;
    std::string reason;
};

/// The full trace produced by the engine. `resolved` is the winning color
/// space (empty if the cascade produced no assignment at all under the
/// active policy). `registered_synthetic` names the session-synthetic id a
/// lenient colorimetry/ICC match selected (id grammar only in this layer --
/// no live endpoint is constructed).
struct ColorResolutionExplanation {
    std::string resolved;
    std::vector<ColorResolutionStep> steps;
    std::string registered_synthetic;
    bool used_failover = false;
    bool used_default  = false;

    /// True iff the last MATCHED step is a genuine metadata source (not
    /// failover, config-default, or the strict-parsing terminal). This is
    /// the "did metadata actually decide this?" predicate.
    OIIO_API bool has_genuine_metadata_match() const;
};

/// Run the audited cascade against `config` (may be null: rules that need a
/// config become inapplicable, and interop-id candidates fall back to the
/// built-in identity registry). Always returns the in-flight trace -- this
/// IS explain(); resolve() is just `.resolved`. `explicit_assignment`, if
/// non-empty, is a caller-forced color space that suppresses metadata rules.
OIIO_API ColorResolutionExplanation
resolve_color_metadata(const ColorConfig* config,
                       const std::string& explicit_assignment,
                       const ColorMetadataFacts& facts,
                       const ColorCallContext& ctx,
                       const ColorReadPolicy& policy);

/// The central read-side entry point. Extracts the facts a reader deposited
/// on `spec`, runs the cascade, and stamps the resolved color space. With
/// policy at its defaults this is observably identical to the per-plugin
/// precedence it replaces. Call once in the ImageInput open path.
OIIO_API void
reconcile_color_metadata(ImageSpec& spec, const ColorReadPolicy& policy);

/// Extract every ColorMetadataFacts signal `spec` carries (ACES container
/// flag, colorInteropID, ICC profile blob, CICP, chromaticities, gamma).
/// This is the one spec->facts extraction; reconcile_color_metadata above
/// still reads its historically-consulted signals itself and can adopt this
/// when per-format signals are deliberately widened (a per-format behavior
/// change, its own later PR).
OIIO_API ColorMetadataFacts
color_facts_from_spec(const ImageSpec& spec);

/// Spec-aware resolve(): run the audited cascade against the color hints
/// present on `spec` -- the same engine, entered from the ImageSpec / IBA
/// side. `config` scopes resolution to that config first, failing over to
/// the built-in identity registry exactly as the facts overload does (null
/// = registry-only answers).
OIIO_API ColorResolutionExplanation
resolve_color_metadata(const ColorConfig* config, const ImageSpec& spec,
                       const ColorCallContext& ctx,
                       const ColorReadPolicy& policy);


// ---------------------------------------------------------------------------
// Color-policy snapshot primitive -- the single-locked-snapshot mechanism
// shared by the read (reconcile) and write (plan) policy readers. One
// snapshot holds the shared policy lock for its whole lifetime, so every get
// below reads one consistent view of the oiio:colorpolicy:* attribute state
// with no mid-call re-lock (the read/write-back race the design forbids).
// Per-open / per-write config hints win over the global attribute table. For
// internal/test use only.
// ---------------------------------------------------------------------------
class OIIO_API ColorPolicySnapshot {
public:
    explicit ColorPolicySnapshot(const ImageSpec* hints = nullptr);
    /// String colorpolicy attribute: config hint, else global, else "".
    std::string get_string(const char* name) const;
    /// Int colorpolicy attribute: config hint, else global, else `dflt`.
    int get_int(const char* name, int dflt) const;

private:
    const ImageSpec* m_hints;
    std::lock_guard<std::mutex> m_lock;  // held for the snapshot's lifetime
};


// ---------------------------------------------------------------------------
// Write-side color-metadata plan -- the central derivation writer plugins
// consume in place of hand-rolled emission. plan_color_metadata() turns the
// color space designation a spec carries into a per-signal plan: for each
// signal a format can emit, whether to write an author-supplied value, derive
// one from the color space, or suppress it. Couldn't-determine OMITS (no
// breadcrumb strings). Separately owned from the read-side reconciler above --
// two modules sharing one oiio:colorpolicy:* namespace, deliberately not
// merged. For internal/test use only.
// ---------------------------------------------------------------------------

/// What the plan says to do with one signal.
enum class ColorPlanAction {
    Omit,      ///< nothing determinable / format can't carry it -- emit nothing
    Write,     ///< an author-supplied value is present -- emit it verbatim
    Derive,    ///< OIIO derived the value from the color space -- emit it
    Suppress,  ///< a policy said "never" -- emit nothing even if determinable
};

/// One planned signal. Only the carrier the signal uses is populated: `str`
/// for an interop id, `ints` for a CICP tuple, `floats` for chromaticities,
/// `gamma` for a scalar. `emit()` is true iff the writer should put bytes down.
struct ColorPlanField {
    ColorPlanAction action = ColorPlanAction::Omit;
    std::string str;
    std::vector<int> ints;
    std::vector<float> floats;
    float gamma = 0.0f;
    bool emit() const
    {
        return action == ColorPlanAction::Write
               || action == ColorPlanAction::Derive;
    }
};

/// Which signals a writer's format can carry. The plan only populates the
/// signals a writer declares supported; every other signal stays Omit.
struct ColorWriteCaps {
    bool cicp           = false;
    bool chromaticities = false;
    bool gamma          = false;
    bool icc            = false;
    bool interop_id     = false;
    bool mdcv           = false;
};

/// The whole write plan -- each signal marked write / suppress / derive / omit.
/// `suppress_source_path` / `keep_source_format` carry the provenance write
/// rule: drop `oiio:SourcePath`, keep `oiio:SourceFormat`.
struct ColorMetadataPlan {
    ColorPlanField cicp;
    ColorPlanField chromaticities;
    ColorPlanField gamma;
    ColorPlanField icc;
    ColorPlanField interop_id;
    ColorPlanField mdcv;
    bool suppress_source_path = true;
    bool keep_source_format   = true;
};

/// Per-signal write switch (spec-owned grammar `oiio:colorpolicy:write:*`).
enum class ColorSignalPolicy { Auto, Always, Never };

/// A single locked snapshot of the whole write policy state, taken once per
/// call via the shared ColorPolicySnapshot. Every default reproduces main's
/// write behavior; the grammar and names are owned by the color policy
/// attribute spec (`oiio:colorpolicy:write:*`).
struct ColorWritePolicy {
    ColorSignalPolicy cicp           = ColorSignalPolicy::Auto;
    ColorSignalPolicy chromaticities = ColorSignalPolicy::Auto;
    ColorSignalPolicy gamma          = ColorSignalPolicy::Auto;
    ColorSignalPolicy icc            = ColorSignalPolicy::Auto;
    ColorSignalPolicy interop_id     = ColorSignalPolicy::Auto;
    ColorSignalPolicy mdcv           = ColorSignalPolicy::Auto;
    std::string custom_namespace_for_generated_ids;
    bool aces_container_allow_lossless_compression = false;
    bool cicp_custom_gama                          = false;
    bool write_narrow_range                        = false;
    bool write_yuv                                 = false;

    /// Read every `oiio:colorpolicy:write:*` value ONCE, under one lock, from
    /// the global attribute table, optionally overridden by per-write config
    /// hints on the output spec. No mid-call re-reads.
    static OIIO_API ColorWritePolicy snapshot(const ImageSpec* config_hints
                                              = nullptr);
};

/// Build the write plan for `spec` under `caps` and `policy`. `config` may be
/// null, in which case the process default color config is used (matching the
/// writers' historical name->id / name->CICP derivation). Pure derivation --
/// never mutates the spec. For internal/test use only.
OIIO_API ColorMetadataPlan
plan_color_metadata(const ColorConfig* config, const ImageSpec& spec,
                    const ColorWriteCaps& caps, const ColorWritePolicy& policy);

}  // namespace pvt



OIIO_NAMESPACE_END

#endif  // OPENIMAGEIO_IMAGEIO_PVT_H
