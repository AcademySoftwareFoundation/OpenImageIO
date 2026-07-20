// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


/// \file
/// Internal (library-private) declarations for the color-interop domain:
/// the read-side color-metadata resolver, the write-side plan, the color
/// policy snapshots, the interop-id grammar, the pure characterization
/// primitives (curve family, chromaticities, transfer signatures), ICC
/// byte-level inspection, mastering-volume derivation, and the internal /
/// test-only hooks into ColorConfig's classification, fingerprint,
/// interoperability, and search machinery. Split out of imageio_pvt.h.
/// Deliberately OCIO-free so format plugins and unit tests (built without
/// OpenColorIO include paths) can include it; the OCIO-typed internals
/// shared by the color_*.cpp translation units live in
/// src/libOpenImageIO/color_ocio_pvt.h. Not installed.


#ifndef OPENIMAGEIO_COLOR_PVT_H
#define OPENIMAGEIO_COLOR_PVT_H

#include <array>
#include <map>
#include <mutex>
#include <optional>
#include <utility>

#include <OpenImageIO/color.h>
#include <OpenImageIO/imageio.h>


OIIO_NAMESPACE_BEGIN

namespace pvt {

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

/// True if the internal copy_config() helper preserves a config's explicit
/// default view transform name across an editable copy. OCIO < 2.3.1's
/// createEditableCopy() drops it; copy_config() restores it so the cross-config
/// display bridge does not shadow a config's own default view transform. The
/// probe builds a two-view-transform config whose explicit default is the
/// non-first one (OCIO's implicit default is the first), copies it, and checks
/// the name survived. Vacuously true when OCIO support is unavailable. For
/// internal/test use only.
OIIO_API bool
copy_config_preserves_default_view_transform();

/// The `name:` of every color space declared in OIIO's built-in interop
/// identities config (see interop_identities_config_size), in the config's
/// own enumeration order. By construction, each entry's `name:` equals its
/// `interop_id:` in the source config -- this does not include entries only
/// reachable as an alias. Empty if OCIO support is unavailable or the
/// embedded config failed to parse. For internal/test use only.
OIIO_API std::vector<std::string>
interop_identities_config_names();

/// Every distinct `interop_id:` value declared in the EMBEDDED interop
/// identities config source (the compiled-in OCIO-2.3 YAML the registry is
/// built from), sorted -- regardless of which composite config
/// interop_identities_config_names() reports at the linked OCIO version
/// (with OCIO >= 2.5 that composite is the studio config plus OIIO's
/// additions, whose declared names are not the canonical id set). This is
/// the canonical CIID set, gathered by the same `interop_id:` token scan the
/// gen_color_interop_ids.py generator performs on the source file, and
/// exists so a unit test can assert the generated OIIO::ColorInteropIDs
/// constants are in exact sync with it. For internal/test use only.
OIIO_API std::vector<std::string>
embedded_interop_identities_ids();

/// The `interop_id` string of every entry in the internal legacy static
/// CICP/interop-id table (color_ocio.cpp's `color_interop_ids[]` -- the
/// syntactic-fallback tier `ColorConfig::get_color_interop_id` consults, and
/// the table `ColorConfig::get_cicp` shares), in table order. Every entry
/// other than the "unknown" utility token is one of the generated
/// OIIO::ColorInteropIDs::* constants, so this is expected to be a subset of
/// embedded_interop_identities_ids() plus that one utility token. For
/// internal/test use only -- lets a test assert the two haven't drifted
/// apart.
OIIO_API std::vector<std::string>
legacy_interop_id_table_names();

/// The full write-side Color Interop ID derivation cascade for `colorspace`:
///   1. an author-declared `interop_id` attribute on the resolved space
///      (verbatim, unconditionally authoritative; a data space with no
///      declared token yields "data");
///   2. definitional equivalence (by fingerprint) to a built-in registry
///      identity, yielding THAT identity's id;
///   3. the legacy static id/CICP table by name/alias equivalence;
///   4. a config-local id "<config>:local:<space>" when this config is named
///      and the query resolves to a (sanitization-unique) real space;
///   5. otherwise empty -- never a guessed default.
/// This is the EXPENSIVE path (step 2 can build the registry index and OCIO
/// processors; step 4 manufactures an id) and is consumed at write-planning
/// time (plan_color_metadata) and by the characterization machinery. The
/// public ColorConfig::get_color_interop_id() performs only the cheap subset
/// (steps 1 and 3). The returned view is stable for the process lifetime.
/// For internal use only; a public wrapper can follow with its in-tree
/// consumer.
OIIO_API string_view
derive_color_interop_id(const ColorConfig& config, string_view colorspace);


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
    bool identity = false;  ///< linear/identity curve
    std::string family;     ///< "" when unidentified
    std::optional<TransferFunctionSignature> signature;

    bool known() const
    { return identity || !family.empty() || signature.has_value(); }
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
transfer_hint_matches(const TransferHint& hint,
                      const TransferProperty& property);


// ---------------------------------------------------------------------------
// ICC profile identification primitives -- cheap, OCIO-free byte-level
// inspection of an embedded ICC profile blob (e.g. the "ICCProfile" spec
// attribute), used by the color-interop ICC identification path.
// ---------------------------------------------------------------------------

/// Whether `iccdata` is structurally an ICC profile: at least 132 bytes
/// (fixed header + tag count) with the mandatory 'acsp' signature at byte
/// offset 36. This is the sole gate separating "an ICC profile" from
/// arbitrary bytes; deeper malformations are handled downstream.
OIIO_API bool
is_icc_profile(cspan<uint8_t> iccdata);

/// Process-local content identifier for an ICC profile, as lowercase hex:
/// XXH64 over the raw, unmodified profile bytes (16 hex chars). Returns
/// the empty string when `iccdata` is not an ICC profile (is_icc_profile).
/// Deliberately byte-exact: a v4 profile's embedded Profile ID field
/// (bytes 84-99, ICC.1:2022 section 7.2.18) is NOT consulted -- it is
/// creator-written and can be stale or forged, so two different blobs
/// could share one embedded ID and collide as cache identity. The
/// identifier is used for internal cache keys and "icc:<id>" synthetic
/// tokens only -- it never leaves the process and must not be written as
/// portable metadata. (If that need ever arises, switch to recomputing the
/// ICC-mandated normalized MD5 rather than trusting the embedded field.)
OIIO_API std::string
icc_profile_identifier(cspan<uint8_t> iccdata);

/// Read an ICC.1:2022 `cicpTag` from a v4 profile into `cicp` as
/// { color_primaries, transfer_characteristics, matrix_coefficients,
/// video_full_range_flag } (ITU-T H.273 code points). Returns false --
/// without touching `cicp` -- when the profile is not ICC, not v4, has no
/// cicp tag, or the tag is malformed (wrong size, non-zero reserved bytes,
/// out-of-bounds offsets, or a range flag greater than 1). Never throws.
OIIO_API bool
icc_embedded_cicp(cspan<uint8_t> iccdata, int cicp[4]);

/// Result of identify_icc_profile(). The three shapes:
///   - id empty, decodable false: the bytes are not an ICC profile at all
///     (failed is_icc_profile) -- invalid input, not a color answer.
///   - id == "icc:<identifier>", decodable false: structurally an ICC
///     profile, but OCIO's matrix/TRC reader cannot decode it (cLUT/AToB
///     transform). The token names the profile without asserting any
///     colorimetry; callers should let weaker color hints win.
///   - id non-empty, decodable true: the decoded profile either matched a
///     built-in registry identity (id is the caller-local space name when
///     the caller's config resolves the identity, else the canonical
///     interop id) or matched nothing (id is the bare "icc:<identifier>"
///     token).
struct IccIdentifyResult {
    std::string id;
    bool decodable = false;
};

/// Identify the color space of an embedded ICC profile blob as a color
/// interop ID, by decoding it through OCIO's matrix/TRC ICC reader inside
/// a throwaway in-memory probe config and fingerprinting the decoded
/// transform against the built-in interop identities registry. `config` is
/// only consulted to prefer a caller-local resolution of a matched
/// identity (ColorConfig::resolve); identification itself runs entirely
/// against the process-global registry. Never throws.
OIIO_API IccIdentifyResult
identify_icc_profile(const ColorConfig& config, cspan<uint8_t> iccdata);


// ---------------------------------------------------------------------------
// Mastering display volume (SMPTE ST 2086) derivation.
// ---------------------------------------------------------------------------

/// A mastering display colour volume: the reference monitor a picture
/// graded through a (display, view) pair was mastered on. Describes the
/// MASTERING MONITOR, not the container encoding (that is CICP territory).
struct MasteringDisplayVolume {
    /// Limiting-gamut primaries + whitepoint as CIE xy, in R, G, B, W
    /// order.
    float primaries[4][2] = { };
    /// Peak luminance, cd/m^2 (snapped to the nominal mastering targets).
    double max_luminance = 0.0;
    /// Minimum luminance, cd/m^2. Probe-honest (may be exactly 0.0); wire
    /// encoders wanting the conventional 0.0001 floor clamp at encode time.
    double min_luminance = 0.0;
    /// Provenance: the matched ACES-OUTPUT style, the DISPLAY builtin
    /// style, or the interop id that supplied the decode; empty for a
    /// plain interchange probe.
    std::string style;
};

/// Derive the ST 2086 mastering display volume for a (display, view) pair
/// of `config`, via a five-tier first-hit-wins ladder: ACES-OUTPUT style
/// table, then a shared numeric probe over three decode constructions
/// (display-interchange CST / inverse DISPLAY builtin / registry-identity
/// decode), then no record. Empty display/view resolve to the config
/// defaults. Returns false -- leaving `volume` untouched by contract of
/// interest -- when no tier fires (unresolvable display/view, an untagged
/// unmatched LUT-only view, or a config with no display interchange to
/// probe): the ladder honestly yields nothing rather than guess. Content
/// light levels (MaxCLL/MaxFALL) are out of scope by design -- they need a
/// pixel scan, not a transform inspection. Never throws.
OIIO_API bool
derive_mastering_volume(const ColorConfig& config, string_view display,
                        string_view view, MasteringDisplayVolume& volume);


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
    int reference_kind = 0;  ///< OCIO ReferenceSpaceType (0 scene, 1 display)
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
                   cspan<float> probe, string_view context_key = { },
                   string_view context_value = { });

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
// Color-space search by characterization -- find a config's color spaces whose
// derivable characteristics (gamut / transfer curve / encoding / image state)
// satisfy a set of partial-characterization hints. The two pieces below are
// the pure, config-free crown of the search: the per-term grammar parse and
// the three-valued (match / known-different / unknown) axis combination. Both
// unit-test without a live OCIO config; the config-driving walk that resolves
// hints and probes candidates lives in color_ocio.cpp (it needs the config).
// For internal/test use only.
// ---------------------------------------------------------------------------

/// A search hint term is `include` by default; a leading `-` makes it
/// `exclude` (subtract proven matches), a leading `~` makes it `inverse`
/// (select only proven *differences*). A leading `\` escapes an operator so it
/// is part of the name.
enum class SearchTermMode { include, exclude, inverse };

/// Split a raw hint term into (mode, value). A backslash escapes exactly `-`,
/// `~`, or `\` (the operator character becomes part of the value). Throws
/// std::invalid_argument on a bare operator (`"-"`), a dangling escape
/// (`"\"`), or an invalid escape (`"\foo"`). An empty input yields an empty
/// value (the caller skips it).
OIIO_API std::pair<SearchTermMode, std::string>
parse_search_term(string_view raw);

/// Three-valued axis combination -- the heart of the search. `modes` and
/// `term_matches` are parallel (one entry per resolved term on this axis);
/// `term_matches[i]` is whether term i matches the candidate's property, and
/// `property_known` is whether the candidate's property could be derived at
/// all ("unknown" when false). An empty axis accepts everything. Include ∪
/// inverse select; an exclusion-only axis starts from the full universe;
/// exclusion always wins last. Inverse selects only *known* differences
/// (unknown is rejected), while exclusion preserves unknowns -- this is the
/// `~` vs `-` unknown-propagation split. The four per-axis evaluators in the
/// search walk all route through this one function.
OIIO_API bool
three_valued_axis(cspan<SearchTermMode> modes,
                  cspan<unsigned char> term_matches, bool property_known);

/// The internal option set for a characterization search. Each of the four
/// hint axes is a list of grammar terms (empty = that axis is unconstrained).
/// `include_active` is on by default and has no public counterpart -- the
/// public API always searches active spaces; only this internal form can turn
/// them off. `context` is a per-call set of OCIO context variable overrides,
/// scoped to the one query.
struct FindColorSpacesOptions {
    std::vector<std::string> chromaticities;
    std::vector<std::string> transfer_functions;
    std::vector<std::string> encodings;
    std::vector<std::string> image_states;
    bool include_active            = true;
    bool include_inactive          = false;
    bool include_context_sensitive = false;
    bool exhaustive                = false;
    // strict limits encoding characterization to explicitly authored
    // encoding attributes.  The default additionally lets a candidate match
    // through the encoding of its interop-identity twin — both as the
    // fallback for an unset attribute and as a second acceptable value
    // alongside an authored one.  Hint-by-example resolution always reads
    // the named space's own effective encoding.
    bool strict = false;
    std::map<std::string, std::string> context;
};

/// Find the color spaces in `config` whose derivable characteristics satisfy
/// every non-empty hint axis of `options`, under the three-valued filter and
/// the visibility/eligibility gates. Every hint term is resolved up front
/// (an unresolvable hint throws std::invalid_argument before any candidate is
/// examined); a candidate whose property cannot be derived is tolerated as
/// "unknown". Results are returned in a deterministic order,
/// (context-invariant, active, simple, name). For internal/test use only.
OIIO_API std::vector<std::string>
find_color_spaces(const ColorConfig& config,
                  const FindColorSpacesOptions& options);


// ---------------------------------------------------------------------------
// Field-selective color-space characterization engine -- the one internal
// entry the public get/derive characterization queries and (in a later
// convergence) the search walk adapt to. characterize_color_space() always
// performs the cheap direct-fact pass (canonical name, image state, cheap
// interop-id subset, authored encoding, intrinsic range) and merges any
// previously cached derived facts; `requested_fields` selects which fields
// are additionally attempted by FULL derivation (fingerprint equality,
// chromaticity probe, transfer probe, the interop-id derivation cascade,
// the interop-counterpart encoding fallback). Derivation attempts --
// successful or not -- are published to a process-global characterization
// cache keyed by (structural config cache id, effective context cache id,
// canonical space name) with the context-invariant bucket collapse the
// fingerprint cache uses, so an unprobeable space is never retried on every
// query. For internal/test use only.
// ---------------------------------------------------------------------------

/// Bitmask of characterization fields to attempt full derivation for.
/// `None` is the cheap/direct-only pass (what the public
/// ColorConfig::get_color_space_info() requests); `All` is the complete
/// derivation the future public derive verb requests; the search walk
/// requests only the fields its non-empty axes need.
enum class CharacterizationField : uint32_t {
    None             = 0,
    EqualityID       = 1u << 0,
    ColorInteropID   = 1u << 1,
    Encoding         = 1u << 2,
    ImageState       = 1u << 3,
    Range            = 1u << 4,
    Chromaticities   = 1u << 5,
    TransferFunction = 1u << 6,
    All              = 0x7f,
};

constexpr CharacterizationField
operator|(CharacterizationField a, CharacterizationField b)
{
    return CharacterizationField(uint32_t(a) | uint32_t(b));
}
constexpr bool
operator&(CharacterizationField a, CharacterizationField b)
{
    return (uint32_t(a) & uint32_t(b)) != 0;
}

/// One characterization record: the value slots plus the per-field
/// computed / available / derived tri-state (bitmasks of
/// CharacterizationField). This is the payload behind the public opaque
/// ColorSpaceInfo. An empty `name` denotes an invalid record (unknown or
/// unresolvable query).
struct CharacterizationRecord {
    std::string name;            ///< canonical local name; empty = invalid
    uint32_t computed_mask  = 0; ///< fields whose determination was attempted
    uint32_t available_mask = 0; ///< attempted fields with a usable value
    uint32_t derived_mask   = 0; ///< values that required behavioral derivation
    /// Fields whose FULL derivation tier has been attempted (internal
    /// bookkeeping, not part of the public tri-state): distinguishes a
    /// cheap direct attempt from a full one, so an unsuccessful derivation
    /// is cached as settled and never retried.
    uint32_t full_attempt_mask = 0;
    std::string equality_id;
    std::string color_interop_id;
    std::string encoding;
    std::string image_state;     ///< "scene" / "display" / empty
    std::string range;           ///< "full" / "narrow" / empty; never guessed
    std::vector<float> chromaticities;  ///< 8 floats (RGBW xy) or empty
    ColorTransferFunctionKind transfer_kind
        = ColorTransferFunctionKind::Undetermined;
    std::string transfer_function;  ///< normalized family, or empty

    bool valid() const { return !name.empty(); }
    bool computed(CharacterizationField f) const
    { return (computed_mask & uint32_t(f)) != 0; }
    bool available(CharacterizationField f) const
    { return (available_mask & uint32_t(f)) != 0; }
    bool derived(CharacterizationField f) const
    { return (derived_mask & uint32_t(f)) != 0; }
    bool full_attempted(CharacterizationField f) const
    { return (full_attempt_mask & uint32_t(f)) != 0; }
};

/// Characterize `color_space` (a name, role, alias, or interop ID) in
/// `config`: cheap direct facts always, cached derived facts merged,
/// `requested_fields` additionally derived (and the attempts published to
/// the shared characterization cache). `context` is a per-call set of OCIO
/// context variable overrides scoped to this query. Returns an invalid
/// record (empty name) for an unknown/unresolvable query. Never throws.
OIIO_API CharacterizationRecord
characterize_color_space(const ColorConfig& config, string_view color_space,
                         CharacterizationField requested_fields
                         = CharacterizationField::None,
                         const std::map<std::string, std::string>& context
                         = {});

/// The number of entries currently in the process-global characterization
/// cache. For internal/test use only.
OIIO_API size_t
characterization_cache_size();

/// Empty the process-global characterization cache. For test/debug reset
/// only. For internal/test use only.
OIIO_API void
characterization_cache_reset();


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
    bool has_cicp           = false;
    int cicp[4]             = { 0, 0, 0, 0 };
    bool has_chromaticities = false;
    float chromaticities[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    bool has_gamma          = false;
    float gamma             = 0.0f;
    bool png_srgb           = false;
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
    ColorResolutionScope scope      = ColorResolutionScope::Lenient;
    ColorStatePreference state_pref = ColorStatePreference::Auto;
    ColorFileRules file_rules       = ColorFileRules::Off;
    bool ignore_cicp_for_png        = false;
    bool ignore_sidecar             = false;
    /// Whether an all-miss falls back to the config's Default Assignment.
    /// Off reproduces main (a reader that determined nothing leaves the
    /// spec's color space untouched); a later named policy turns it on.
    bool apply_config_default = false;

    /// Read every `oiio:colorpolicy:read:*` value ONCE, under one lock, from
    /// the global attribute table, optionally overridden by per-open config
    /// hints. No mid-call re-reads.
    static OIIO_API ColorReadPolicy snapshot(const ImageSpec* config_hints
                                             = nullptr);
};

/// The 13 cascade rules, in exact tested precedence order (CICP above ICC,
/// per the PNG spec's own chunk precedence: cICP > iCCP).
/// STRICT_PARSING is the terminal miss diagnostic, not a 14th source rule.
enum class ColorRule {
    ExplicitAssignment,
    AcesContainer,
    FileRulesFirst,
    ColorInteropID,
    Cicp,
    IccProfile,
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

/// Infer a usable source color space from the color hints on `spec`, for a
/// color operation whose caller supplied none: the spec resolve() above,
/// with session-synthetic answers (custom:/icc:) filtered out -- they name
/// no constructible color space in this round -- via a config-only retry
/// that lets an unusable signal miss and the next rung answer. Returns ""
/// when no hint yields a usable space (the caller keeps its default).
OIIO_API std::string
infer_color_space_from_spec(const ColorConfig* config, const ImageSpec& spec,
                            const ColorCallContext& ctx,
                            const ColorReadPolicy& policy);

/// Post-IBA color-attribute scrubber: remove the color hint attributes an
/// outgoing spec carries when the resolver's trace proves what they claim
/// (each signal judged in isolation; a determinate claim is either
/// redundant with or contradicted by the spec's established color space --
/// stale either way). Couldn't-determine leaves the attribute; the
/// deliberate unknown-marker family ("ocio:unknown" / "oiio:unknown" /
/// "error:unknown") is always honored; a bare "unknown" claim is
/// contradicted by any definite color space. Never touches the color
/// space itself.
OIIO_API void
scrub_color_metadata(ImageSpec& spec, const ColorConfig* config,
                     const ColorReadPolicy& policy);

/// Dry-run preview (read-side twin of render_color_write_plan): render, as
/// plain aligned text, how `spec`'s color metadata resolves through the
/// audited read cascade -- each ColorRule visited in precedence order, its
/// outcome (matched / missed / inapplicable / invalid), the candidate/reason
/// it recorded, and the final resolved color space and the rule that decided
/// it. Re-runs the same engine reconcile_color_metadata uses, under the
/// default read policy; `config` null means the process default color config.
/// Writes no bytes and never mutates the spec. For internal/test use only.
OIIO_API std::string
render_color_read_plan(const ImageSpec& spec,
                       const ColorConfig* config = nullptr);


// ---------------------------------------------------------------------------
// Color-policy snapshot primitive -- the single-locked-snapshot mechanism
// shared by the read (reconcile) and write (plan) policy readers. One
// snapshot holds the shared policy lock for its whole lifetime, so every get
// below reads one consistent view of the oiio:colorpolicy:* attribute state
// with no mid-call re-lock (the read/write-back race the design forbids).
// Per-open / per-write config hints win over the global attribute table. For
// internal/test use only.
// ---------------------------------------------------------------------------
/// Which layer of the policy/metadata stack decided a planned signal (or,
/// for the first three values, supplied a policy setting). The snapshot
/// reports only the policy tiers (BuiltinDefault / GlobalAttribute /
/// PerSpecAttribute); the planner additionally attributes a field to the
/// author's explicit metadata or to the format's declared incapability.
enum class ColorPlanDecider {
    BuiltinDefault,    ///< no policy attribute set anywhere -- default behavior
    GlobalAttribute,   ///< a global `oiio:colorpolicy:write:*` attribute
    PerSpecAttribute,  ///< a per-write config hint on the spec
    ExplicitMetadata,  ///< author-supplied metadata present on the spec
    FormatIncapable,   ///< the format cannot carry the signal
};

class OIIO_API ColorPolicySnapshot {
public:
    explicit ColorPolicySnapshot(const ImageSpec* hints = nullptr);
    /// String colorpolicy attribute: config hint, else global, else "".
    /// `layer`, if non-null, receives which tier supplied the value
    /// (PerSpecAttribute / GlobalAttribute / BuiltinDefault for unset).
    std::string get_string(const char* name,
                           ColorPlanDecider* layer = nullptr) const;
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
    /// Who decided `action`: format incapability, the author's explicit
    /// metadata, or the policy tier (builtin / global / per-spec) that was
    /// in force when the signal was resolved.
    ColorPlanDecider decider = ColorPlanDecider::BuiltinDefault;
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
    // Which tier supplied each signal's policy above (BuiltinDefault /
    // GlobalAttribute / PerSpecAttribute only), recorded by snapshot() so
    // the plan can attribute its verdicts.
    ColorPlanDecider cicp_layer           = ColorPlanDecider::BuiltinDefault;
    ColorPlanDecider chromaticities_layer = ColorPlanDecider::BuiltinDefault;
    ColorPlanDecider gamma_layer          = ColorPlanDecider::BuiltinDefault;
    ColorPlanDecider icc_layer            = ColorPlanDecider::BuiltinDefault;
    ColorPlanDecider interop_id_layer     = ColorPlanDecider::BuiltinDefault;
    ColorPlanDecider mdcv_layer           = ColorPlanDecider::BuiltinDefault;
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

/// The one format-name -> declared-write-caps table. This is what each wired
/// writer plugin passes to plan_color_metadata (the plugins consume this
/// function, so the table cannot drift from the writers); a format with no
/// wired plan consumer returns all-false caps. Case-insensitive; "exr" is
/// accepted for "openexr". For internal/test use only.
OIIO_API ColorWriteCaps
color_write_caps_for_format(string_view format_name);

/// Dry-run preview: render, as a plain aligned text table (one row per
/// signal, fixed row order, deterministic), the color-metadata write plan
/// OIIO would execute if `spec` were written to format `format_name` -- each
/// signal's verdict, the value that would be written, and which layer
/// decided it (see ColorPlanDecider). Pure: derives the same plan the writer
/// would (process-default color config, policy snapshot honoring per-spec
/// hints on `spec`) and writes no bytes. For internal/test use only.
OIIO_API std::string
render_color_write_plan(const ImageSpec& spec, string_view format_name);
}  // namespace pvt



OIIO_NAMESPACE_END

#endif  // OPENIMAGEIO_COLOR_PVT_H
