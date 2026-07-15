// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


/// \file
/// Declarations for things that are used privately by OpenImageIO.


#ifndef OPENIMAGEIO_IMAGEIO_PVT_H
#define OPENIMAGEIO_IMAGEIO_PVT_H

#include <array>
#include <optional>

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
// Curve-family normalization -- reduce a transfer-function ("curve") named
// transform's name to a reference-space-agnostic family so two spaces sharing
// a transfer curve compare equal regardless of the state suffix the name
// carries (`_tx` pass-through / bare mirror in current configs; legacy
// `_scene` / `_display` still supported). Pure, stateless, no OCIO. For
// internal/test use only.
// ---------------------------------------------------------------------------

/// Family token for a curve name: strip a leading `crv_`, then strip at most
/// one trailing state suffix (`_scene`, `_display`, or `_tx`). A name that is
/// a bare suffix (`"_tx"`) or is empty passes through unchanged. E.g.
/// `crv_g24_tx`, `crv_g24`, and `crv_g24_display` all yield `g24`.
OIIO_API std::string
family_token(string_view name);

/// Like family_token but keeps the `crv_` prefix -- for comparing two matched
/// catalog names for family equality (`crv_srgb_tx` == `crv_srgb`).
OIIO_API std::string
family_name(string_view name);

/// True if `name` is the pass-through variant of a curve family (ends with
/// `_scene` or `_tx`, and is strictly longer than that suffix).
OIIO_API bool
curve_is_passthrough(string_view name);

/// True if `name` is the mirror variant of a curve family: it ends with the
/// legacy `_display` suffix, OR its `_tx` pass-through twin is present in
/// `catalog_names` (the current suffixless-mirror convention).
OIIO_API bool
curve_is_mirror(string_view name, cspan<std::string> catalog_names);


// ---------------------------------------------------------------------------
// Chromaticity math -- pure, config-free primitives for the chromaticity axis
// of color-space search by characterization. A Chromaticities is four (x, y)
// pairs in R, G, B, W order. Rounding is the only place numerical fuzz is
// absorbed; once coordinates are rounded, equality is coordinate-exact, so
// callers compare a Chromaticities with plain `==` / std::find (std::array
// gives that for free -- no dedicated compare helper). For internal/test use
// only.
// ---------------------------------------------------------------------------

using Chromaticities = std::array<std::array<double, 2>, 4>;

/// Round a chromaticity coordinate to 6 decimals, then snap to the nearest
/// coarser 5/4/3/2-digit grid if within 2e-7 (finest grid within tolerance
/// wins). Absorbs OCIO chromaticity floats like 0.329999998 -> 0.33 so that
/// downstream equality can be exact.
OIIO_API double
round_chromaticity_coord(double value);

/// Reserved (R,G,B,W) primaries for an interop id that names a well-known
/// gamut, probed as an `_<token>_` substring of the lowered id (a complete
/// gamut component, not an arbitrary fragment), first match wins. `adobergb`
/// matches only on the exact id or an `_adobergb_` token. Empty when no
/// reserved gamut token is present (single-hypothesis / table-only: gamuts
/// absent from the table, e.g. `ciexyzd65`, are not resolved here).
OIIO_API std::optional<Chromaticities>
reserved_chromaticities_for_id(string_view interop_id);

/// Derive chromaticities from the four AP0-anchored RGB probes (pure R, G, B,
/// W as a flat 12-element span, in that order) that the caller has pushed
/// through colorspace -> AP0 interchange. Applies the Bradford-adapted
/// AP0->XYZ(D65) matrix, solves each probe for (x, y) with rounding, and
/// snaps an equal-energy white to exact (1/3, 1/3). Empty if the span is not
/// 12 long or a probe is degenerate (non-finite / near-zero sum). Single
/// hypothesis (D65 + Bradford); the whitepoint/CAT sweep is a follow-on.
OIIO_API std::optional<Chromaticities>
chromaticities_from_ap0_probes(cspan<float> ap0_rgb);


// ---------------------------------------------------------------------------
// Transfer-signature axis -- pure, config-free primitives for the
// transfer-function axis of color-space search by characterization. A
// candidate's transfer property is the triple { identity, family, signature }:
// whether the curve is linear/identity, its reference-state-agnostic family
// key (from the curve-family normalization above), and, for non-identity
// curves, a behavioral signature probed on the neutral axis. Matching follows
// a fixed order -- identity, then family, then signature. The numerical work
// (normalized slopes, per-encoding slope tolerance, white-gain tie-break) is
// pure: a caller runs a CPU processor over tf_probe_axis() and hands the
// outputs here, so nothing in this section needs a live config. For
// internal/test use only.
// ---------------------------------------------------------------------------

/// Behavioral transfer-function signature of a color space: channel-averaged
/// outputs of a fixed set of neutral-axis probes in the encode direction
/// (linear anchor -> color space), plus the adjacent slopes normalized by the
/// 0.18->0.50 anchor slope. `encoding` (the effective OCIO encoding) selects
/// the slope tolerance; `family` is the transfer-family key; `is_linear` is
/// the measured 64x-ratio linearity verdict.
struct TransferFunctionSignature {
    std::vector<double> slopes;  ///< adjacent slopes, 0.18->0.50 normalized
    std::vector<double> values;  ///< channel-averaged probe outputs
    std::string encoding;        ///< effective OCIO encoding of the space
    std::string family;          ///< transfer-family key (see family_token)
    bool is_linear = false;      ///< measured linearity (64x ratio check)
};

/// Per-candidate transfer property. "Unknown" -- none of the members carry a
/// verdict (known() is false) -- makes an include term miss, a `~` term
/// reject, and a `-` term preserve, in the three-valued axis evaluation.
struct TransferProperty {
    bool identity = false;                           ///< linear/identity curve
    std::string family;                              ///< "" when unidentified
    std::optional<TransferFunctionSignature> signature;

    bool known() const
    {
        return identity || !family.empty() || signature.has_value();
    }
};

/// A resolved transfer-function hint: the property a hint term denotes, as one
/// or more of identity / family / candidate signatures. (The search-term mode
/// -- include / exclude / inverse -- is layered on separately by the search
/// core; this struct carries only the resolved value.)
struct TransferHint {
    bool identity = false;  ///< hint denotes a linear/identity curve
    std::string family;     ///< curve-family key ("" when unidentified)
    std::vector<TransferFunctionSignature> signatures;
};

/// The fixed neutral-axis probe abscissae the signature is built from: the 10
/// discriminating points, followed by the (dark, bright) scaled-linearity
/// pair. A caller pushes each value as R=G=B through a CPU processor in the
/// encode direction and hands the 12 channel-averaged outputs to
/// tf_signature_from_probes().
OIIO_API cspan<double>
tf_probe_axis();

/// Per-encoding slope tolerance: 0.05 for `log`, 0.1 for `hdr-video`, else
/// 0.02 (`sdr-video` and default). Wider for log/HDR because those curves
/// vary more across their slope profiles.
OIIO_API double
tf_slope_tolerance(string_view encoding);

/// True when the curve clips superwhite: the last two probe outputs (the 1.0
/// and 1.1 points) coincide.
OIIO_API bool
tf_clips_superwhite(cspan<double> values);

/// Adjacent slopes of a 10-probe run, normalized by the 0.18->0.50 anchor
/// slope. Empty when `values` is not a full probe run (size 10) or the anchor
/// slope is degenerate (flat).
OIIO_API std::vector<double>
tf_normalized_slopes(cspan<double> values);

/// Build a signature from the 12 channel-averaged outputs of tf_probe_axis()
/// (10 discriminating probes + dark + bright). `encoding` and `family` are the
/// caller's to fill afterward (they depend on the source config). nullopt on a
/// short span or a degenerate (flat) anchor slope.
OIIO_API std::optional<TransferFunctionSignature>
tf_signature_from_probes(cspan<double> probe_outputs);

/// Tolerance-compare two probed signatures: per-encoding slope tolerance with
/// clip masking (index 0 always masked; last index masked when either side
/// clips superwhite; at least 80% of compared slopes must agree), then a
/// white-gain tie-break at the 1.0 probe that keeps a headroom-scaled curve
/// distinct from its unscaled twin.
OIIO_API bool
transfer_signatures_match(const TransferFunctionSignature& a,
                          const TransferFunctionSignature& b);

/// Does a resolved transfer hint match a candidate's transfer property, in
/// the identity -> family -> signature order: identity-vs-identity wins; else
/// if both families are known, family equality decides (behavior families beat
/// signature comparison); else a probed-signature tolerance compare against
/// any of the hint's signatures. An unknown candidate property never matches.
OIIO_API bool
transfer_hint_matches(const TransferHint& hint, const TransferProperty& property);


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

}  // namespace pvt



OIIO_NAMESPACE_END

#endif  // OPENIMAGEIO_IMAGEIO_PVT_H
