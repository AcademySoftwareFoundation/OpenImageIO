// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>

#include <OpenImageIO/export.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/ustring.h>

// Preprocessor symbol to allow conditional compilation depending on
// whether the ColorProcessor class is exposed (it was not prior to OIIO 1.9).
#define OIIO_HAS_COLORPROCESSOR 1

//
// Some general color management information materials to have handy:
//   - CIF recommendations for scene referred color spaces for rendering and
//     textures:
//     https://github.com/AcademySoftwareFoundation/ColorInterop/blob/main/Recommendations/01_TextureAssetColorSpaces/TextureAssetColorSpaces.md
//   - CIF recommendations for display referred color spaces:
//     https://docs.google.com/document/d/1MmBG4a3Dr6S6EO781WjK-xZW7QdHpuo-zd7wtMvG1Rs
//


// Preprocessor symbol to allow conditional compilation depending on
// whether the ColorConfig returns ColorProcessor shared pointers or raw.
#define OIIO_COLORCONFIG_USES_SHARED_PTR 1


OIIO_NAMESPACE_3_1_BEGIN

/// The ColorProcessor encapsulates a baked color transformation, suitable for
/// application to raw pixels, or ImageBuf(s). These are generated using
/// ColorConfig::createColorProcessor, and referenced in ImageBufAlgo
/// (amongst other places)
class OIIO_API ColorProcessor {
public:
    ColorProcessor() {};
    virtual ~ColorProcessor(void) {};
    OIIO_NODISCARD virtual bool isNoOp() const { return false; }
    OIIO_NODISCARD virtual bool hasChannelCrosstalk() const { return false; }

    // Convert an array/image of color values. The strides are the distance,
    // in bytes, between subsequent color channels, pixels, and scanlines.
    virtual void apply(float* data, int width, int height, int channels,
                       stride_t chanstride, stride_t xstride,
                       stride_t ystride) const
        = 0;
    // Convert a single 3-color
    void apply(float* data)
    {
        apply((float*)data, 1, 1, 3, sizeof(float), 3 * sizeof(float),
              3 * sizeof(float));
    }
};



using ColorProcessorHandle = std::shared_ptr<ColorProcessor>;



/// Represents the set of all color transformations that are allowed.
/// If OpenColorIO is enabled at build time, this configuration is loaded
/// at runtime, allowing the user to have complete control of all color
/// transformation math. ($OCIO)  (See opencolorio.org for details).
/// If OpenColorIO is not enabled at build time, a generic color configuration
/// is provided for minimal color support.
///
/// NOTE: ColorConfig(s) and ColorProcessor(s) are potentially heavy-weight.
/// Their construction / destruction should be kept to a minimum.

#ifdef OIIO_INTERNAL
namespace pvt {
// Access shim letting the internal color-space classification test hooks
// (see color_pvt.h) reach ColorConfig's private implementation. Only
// visible when building OIIO itself (same pattern as deepdata.h); the
// installed public header exposes no new symbol.
struct ColorConfigClassificationPeek;
}  // namespace pvt
#endif


/// Options controlling ColorConfig::find_color_spaces(). The four hint axes
/// are passed separately; this bundles the search-scope toggles and the
/// per-call context. Default-constructed options give the default search
/// (active, simple, context-invariant spaces; encoding axis may use the
/// interop-identity twin).
///
/// @version 3.2
struct ColorSpaceSearchOptions {
    /// Also consider the config's inactive color spaces.
    bool include_inactive = false;
    /// Also consider spaces whose transforms depend on context variables.
    bool include_context_sensitive = false;
    /// Also consider complex (non-simple) spaces by inspecting their
    /// authored and realized transforms.
    bool include_complex = false;
    /// Limit the encoding axis to encodings explicitly authored in the
    /// config -- no inference through a space's interop-identity twin.
    bool authored_encoding_only = false;
    /// OCIO context-variable overrides, scoped to the one call.
    std::map<std::string, std::string> context;
    /// Name of a config-declared color policy profile to interpret this
    /// call under. Accepted and currently ignored; honored when the policy
    /// layer lands. (3.2.0)
    std::string profile;
    /// Inline color-policy overrides for this call, in the
    /// `oiio:colorpolicy:*` grammar. These are policies, NOT OCIO context
    /// variables -- `context` above remains the context mechanism.
    /// Accepted and currently ignored; honored when the policy layer
    /// lands. (3.2.0)
    std::string policies;
};


/// The individually queryable fields of a ColorSpaceInfo record, for the
/// per-field computed()/available()/derived() cost-visibility queries.
///
/// @version 3.2
enum class ColorSpaceInfoField : uint32_t {
    EqualityID,
    ColorInteropID,
    Encoding,
    ImageState,
    Range,
    Chromaticities,
    TransferFunction,
};


/// Semantic classification of a color space's transfer function, as
/// reported by ColorSpaceInfo::transfer_function_kind().
///
/// @version 3.2
enum class ColorTransferFunctionKind : uint8_t {
    Undetermined,  ///< not determined (or not yet attempted)
    Linear,        ///< linear/identity curve
    Named,         ///< a recognized named family (see transfer_function())
    Sampled,       ///< successfully sampled behavior, but no known family
};


/// Per-call context for color-space characterization queries
/// (ColorConfig::get_color_space_info). Default construction uses the
/// ColorConfig's current context.
///
/// @version 3.2
struct ColorSpaceInfoOptions {
    /// OCIO context-variable overrides, scoped to the one call.
    std::map<std::string, std::string> context;
    /// Name of a config-declared color policy profile to interpret this
    /// call under. Accepted and currently ignored; honored when the policy
    /// layer lands. (3.2.0)
    std::string profile;
    /// Inline color-policy overrides for this call, in the
    /// `oiio:colorpolicy:*` grammar. These are policies, NOT OCIO context
    /// variables -- `context` above remains the context mechanism.
    /// Accepted and currently ignored; honored when the policy layer
    /// lands. (3.2.0)
    std::string policies;
};


/// Options controlling ColorConfig::serialize().
///
/// @version 3.2
struct ColorConfigSerializeOptions {
    /// Serialize the interoperability-repaired in-memory copy of the
    /// config (the one cross-config conversions actually route through)
    /// instead of the original: evidence of what is in memory, not what
    /// was on disk. May trigger the lazy interoperability bootstrap.
    bool interopified = false;
};


/// Options describing how ColorConfig::evolve() should modify the copy it
/// returns. Default-constructed options request a plain copy.
///
/// @version 3.2
struct ColorConfigEvolveOptions {
    /// If non-empty, the evolved config's working directory (used by OCIO
    /// to resolve relative FileTransform sources and search paths, and
    /// required for archiving).
    std::string working_dir;
    /// Default-value overrides for the config's context variables
    /// (OCIO environment vars), as key -> value.
    std::map<std::string, std::string> context;
    /// Start from the ORIGINAL config this one was first constructed with,
    /// discarding the modifications of any previous evolve() steps, before
    /// applying the fields above.
    bool reset = false;
};


/// Options controlling ColorConfig::archive().
///
/// @version 3.2
struct ColorConfigArchiveOptions {
    /// If non-empty, archive as if the config's working directory were this
    /// directory (OCIO gathers the LUT files under the working directory
    /// into the archive). Required when the config has no working directory
    /// of its own (e.g. one built by ColorConfig::from_text without one).
    std::string working_dir;
    /// Archive the interoperability-repaired in-memory copy of the config
    /// instead of the original. May trigger the lazy interop bootstrap.
    bool interopified = false;
};


/// Options controlling ColorConfig::get_debug_info(). There are no options
/// yet; the struct exists so future report selectors can be added without
/// changing the method signature. It has no other caller, so it belongs
/// with get_debug_info() and the two types below -- if that method ever
/// moves, this moves with it rather than being stranded here empty.
///
/// @version 3.2
struct ColorConfigDebugInfoOptions {};


/// State of a config's scene-interchange discovery, as reported by
/// ColorConfigDebugInfo::interchange_state. `Pending` is a distinct,
/// load-bearing value: querying the debug info never triggers the lazy
/// discovery, so a discovery that has not run yet reports as pending
/// rather than as a negative result.
///
/// @version 3.2
enum class ColorInterchangeState : uint8_t {
    Pending,        ///< discovery has not run (querying does not trigger it)
    Interoperable,  ///< a scene interchange space was identified
    NotFound,       ///< discovery ran and identified none
};


/// A config's identity and cache state, as returned by
/// ColorConfig::get_debug_info(), for diagnostics and bug reports. Every
/// field is formatted from existing internal state: constructing this
/// never triggers lazy work.
///
/// @version 3.2
struct OIIO_API ColorConfigDebugInfo {
    /// OpenImageIO version string.
    std::string oiio_version;
    /// OpenColorIO version string (empty if OCIO is unavailable).
    std::string ocio_version;
    /// This config's name (see ColorConfig::configname()).
    std::string config_name;
    /// The config's structural cache identity -- context excluded.
    std::string structural_cache_id;
    /// OCIO's cache id for the config, with the context folded in.
    std::string cache_id;
    /// Data version of the built-in interop identities registry.
    std::string registry_data_version;

    /// Whether a scene interchange space has been identified for this
    /// config, or whether that discovery has simply not run yet.
    ColorInterchangeState interchange_state = ColorInterchangeState::Pending;
    /// The identified scene interchange space name; empty unless
    /// `interchange_state` is `Interoperable`.
    std::string interchange_name;

    /// Per-cache-layer entry counts, keyed by layer name (e.g.
    /// "color processors", "fingerprints", "characterizations"). A map,
    /// not one fixed field per layer, deliberately: cache layers come and
    /// go, and the CONTENTS of a map are not ABI, whereas a field per
    /// layer would make every future cache change an ABI break. Treat the
    /// key set as informational, not as a contract.
    std::map<std::string, std::size_t> cache_entries;

    /// Render all of the above as a human-readable multi-line report, so a
    /// bug report has one thing to paste. The exact text is informational
    /// and may change between versions -- display it, don't parse it.
    std::string to_string() const;
};


/// Options controlling ColorConfig::clear_caches(). There are no options
/// yet; the struct exists so future selectors can be added without
/// changing the method signature.
///
/// @version 3.2
struct ColorConfigClearCachesOptions {};


/// Immutable snapshot of the computed characterization information for one
/// resolved color space, as returned by ColorConfig::get_color_space_info().
/// Accessor views remain valid for this object's lifetime. Copies are cheap
/// (shared immutable state). A later, more complete characterization updates
/// the information seen by future queries; it never mutates a snapshot
/// already held by a caller.
///
/// Each field carries per-field cost visibility: `computed(field)` reports
/// whether determination of the field has been attempted at all,
/// `available(field)` whether the attempt produced a usable value, and
/// `derived(field)` whether that value required behavioral derivation
/// (probing transforms) rather than direct config/registry inspection. A
/// computed-but-unavailable field is a stable negative result, not an error.
///
/// @version 3.2
class OIIO_API ColorSpaceInfo {
public:
    ColorSpaceInfo();
    ~ColorSpaceInfo();
    ColorSpaceInfo(const ColorSpaceInfo&);
    ColorSpaceInfo(ColorSpaceInfo&&) noexcept;
    ColorSpaceInfo& operator=(const ColorSpaceInfo&);
    ColorSpaceInfo& operator=(ColorSpaceInfo&&) noexcept;

    /// False only for the default object or a failed/unknown query.
    OIIO_NODISCARD bool valid() const noexcept;

    /// Canonical local color-space name. Empty when !valid().
    OIIO_NODISCARD string_view name() const noexcept;

    /// Mathematical identity determined by fingerprint equivalence.
    /// It deliberately ignores authored interop_id and name coincidence.
    OIIO_NODISCARD string_view equality_id() const noexcept;

    /// Authoritative/write Color Interop ID. Declaration-first semantics.
    OIIO_NODISCARD string_view color_interop_id() const noexcept;

    /// Effective encoding: authored value first, otherwise a derived
    /// interop-counterpart value.
    OIIO_NODISCARD string_view encoding() const noexcept;

    /// "scene" or "display"; empty when undetermined.
    OIIO_NODISCARD string_view image_state() const noexcept;

    /// "full" or "narrow"; empty when not intrinsic/determinable. Range
    /// describes pixel state and is never guessed from a color-space name.
    OIIO_NODISCARD string_view range() const noexcept;

    /// Eight floats in Rx,Ry,Gx,Gy,Bx,By,Wx,Wy order, or an empty span.
    OIIO_NODISCARD cspan<float> chromaticities() const noexcept;

    OIIO_NODISCARD ColorTransferFunctionKind
    transfer_function_kind() const noexcept;

    /// Normalized family such as "srgb" or "g24"; empty for an
    /// undetermined or sampled-but-unnamed transfer function.
    OIIO_NODISCARD string_view transfer_function() const noexcept;

    /// Whether determination of this field has been attempted.
    OIIO_NODISCARD bool computed(ColorSpaceInfoField field) const noexcept;

    /// Whether the attempted field has a usable value.
    OIIO_NODISCARD bool available(ColorSpaceInfoField field) const noexcept;

    /// Whether the published value required behavioral derivation rather
    /// than direct config/registry inspection.
    OIIO_NODISCARD bool derived(ColorSpaceInfoField field) const noexcept;

private:
    class Impl;
    std::shared_ptr<const Impl> m_impl;

    explicit ColorSpaceInfo(std::shared_ptr<const Impl>);
    friend class ColorConfig;
};


class OIIO_API ColorConfig {
public:
    /// Construct a ColorConfig using the named OCIO configuration file,
    /// or if filename is empty, to the current color configuration
    /// specified by env variable $OCIO.
    ///
    /// Multiple calls to this are potentially expensive. A ColorConfig
    /// should usually be shared by an app for its entire runtime.
    ColorConfig(string_view filename = "");

    ~ColorConfig();

    /// Move construction/assignment: transfers ownership of the underlying
    /// configuration state. The moved-from object may only be destroyed or
    /// assigned to. (ColorConfig remains non-copyable.)
    ///
    /// @version 3.2
    ColorConfig(ColorConfig&& other) noexcept;
    ColorConfig& operator=(ColorConfig&& other) noexcept;

    /// Reset the config to the named OCIO configuration file, or if
    /// filename is empty, to the current color configuration specified
    /// by env variable $OCIO. Return true for success, false if there
    /// was an error.
    ///
    /// Multiple calls to this are potentially expensive. A ColorConfig
    /// should usually be shared by an app for its entire runtime.
    OIIO_NODISCARD_ERROR bool reset(string_view filename = "");

    /// Has an error string occurred?
    /// (This will not affect the error state.)
    OIIO_NODISCARD bool has_error() const;

    /// DEPRECATED(2.4), old name for has_error().
    OIIO_DEPRECATED("Use has_error()")
    bool error() const { return has_error(); }

    /// This routine will return the error string (and by default, clear any
    /// error flags).  If no error has occurred since the last time
    /// geterror() was called, it will return an empty string.
    OIIO_NODISCARD std::string geterror(bool clear = true) const;

    /// Get the number of ColorSpace(s) defined in this configuration
    OIIO_NODISCARD int getNumColorSpaces() const;

    /// Query the name of the specified ColorSpace.
    OIIO_NODISCARD const char* getColorSpaceNameByIndex(int index) const;

    /// Given a color space name, return the index of an equivalent color
    /// space, or -1 if not found. It will first look for an exact match of
    /// the name, but if not found, will match a color space that is
    /// "equivalent" to the named color space.
    OIIO_NODISCARD int getColorSpaceIndex(string_view name) const;

    /// Get the name of the color space representing the named role,
    /// or nullptr if none could be identified.
    OIIO_NODISCARD const char* getColorSpaceNameByRole(string_view role) const;

    /// Get the data type that OCIO thinks this color space is. The name
    /// may be either a color space name or a role. For an unknown space or
    /// any error, return TypeUnknown.
    OIIO_NODISCARD OIIO::TypeDesc getColorSpaceDataType(string_view name,
                                                        int* bits) const;

    /// Retrieve the full list of known color space names, as a vector
    /// of strings.
    OIIO_NODISCARD std::vector<std::string> getColorSpaceNames() const;

    /// Get the name of the color space family of the named color space,
    /// or nullptr if none could be identified.
    OIIO_NODISCARD const char* getColorSpaceFamilyByName(string_view name) const;

    // Get the number of Roles defined in this configuration
    OIIO_NODISCARD int getNumRoles() const;

    /// Query the name of the specified Role, or return nullptr if there is no
    /// role with that index.
    OIIO_NODISCARD const char* getRoleByIndex(int index) const;

    /// Retrieve the full list of known Roles, as a vector of strings.
    OIIO_NODISCARD std::vector<std::string> getRoles() const;

    /// Get the number of Looks defined in this configuration
    OIIO_NODISCARD int getNumLooks() const;

    /// Query the name of the specified Look, or return nullptr if there is no
    /// look with that index.
    OIIO_NODISCARD const char* getLookNameByIndex(int index) const;

    /// Retrieve the full list of known look names, as a vector of strings.
    OIIO_NODISCARD std::vector<std::string> getLookNames() const;

    /// Get the number of NamedTransforms defined in this configuration
    OIIO_NODISCARD int getNumNamedTransforms() const;

    /// Query the name of the specified NamedTransform, or nullptr if there is
    /// no NamedTransform with that index.
    OIIO_NODISCARD const char* getNamedTransformNameByIndex(int index) const;

    /// Retrieve the full list of known NamedTransforms, as a vector of strings
    OIIO_NODISCARD std::vector<std::string> getNamedTransformNames() const;

    /// Retrieve the full list of aliases for the named NamedTransform.
    OIIO_NODISCARD std::vector<std::string>
    getNamedTransformAliases(string_view named_transform) const;

    /// Is the color space known to be linear? This is very conservative, and
    /// will return false if it's not sure.
    OIIO_NODISCARD bool isColorSpaceLinear(string_view name) const;

    /// Is the color space non-color-managed "data"?
    OIIO_NODISCARD bool isData(string_view name) const;

    /// Retrieve the full list of aliases for the named color space.
    OIIO_NODISCARD std::vector<std::string>
    getAliases(string_view color_space) const;

    /// Given the specified input and output ColorSpace, request a handle to
    /// a ColorProcessor.  It is possible that this will return an empty
    /// handle, if the inputColorSpace doesn't exist, the outputColorSpace
    /// doesn't exist, or if the specified transformation is illegal (for
    /// example, it may require the inversion of a 3D-LUT, etc).
    ///
    /// The handle is actually a shared_ptr, so when you're done with a
    /// ColorProcess, just discard it. ColorProcessor(s) remain valid even
    /// if the ColorConfig that created them no longer exists.
    ///
    /// Created ColorProcessors are cached, so asking for the same color
    /// space transformation multiple times shouldn't be very expensive.
    OIIO_NODISCARD ColorProcessorHandle createColorProcessor(
        string_view inputColorSpace, string_view outputColorSpace,
        string_view context_key = "", string_view context_value = "") const;
    OIIO_NODISCARD ColorProcessorHandle
    createColorProcessor(ustring inputColorSpace, ustring outputColorSpace,
                         ustring context_key   = ustring(),
                         ustring context_value = ustring()) const;

    /// Given the named look(s), input and output color spaces, request a
    /// color processor that applies an OCIO look transformation.  If
    /// inverse==true, request the inverse transformation.  The
    /// context_key and context_value can optionally be used to establish
    /// extra key/value pairs in the OCIO context if they are comma-
    /// separated lists of context keys and values, respectively.
    ///
    /// The handle is actually a shared_ptr, so when you're done with a
    /// ColorProcess, just discard it. ColorProcessor(s) remain valid even
    /// if the ColorConfig that created them no longer exists.
    ///
    /// Created ColorProcessors are cached, so asking for the same color
    /// space transformation multiple times shouldn't be very expensive.
    OIIO_NODISCARD ColorProcessorHandle createLookTransform(
        string_view looks, string_view inputColorSpace,
        string_view outputColorSpace, bool inverse = false,
        string_view context_key = "", string_view context_value = "") const;
    OIIO_NODISCARD ColorProcessorHandle createLookTransform(
        ustring looks, ustring inputColorSpace, ustring outputColorSpace,
        bool inverse = false, ustring context_key = ustring(),
        ustring context_value = ustring()) const;

    /// Get the number of displays defined in this configuration
    OIIO_NODISCARD int getNumDisplays() const;

    /// Query the name of the specified display, or nullptr if there is no
    /// display with that index.
    OIIO_NODISCARD const char* getDisplayNameByIndex(int index) const;

    /// Retrieve the full list of known display names, as a vector of
    /// strings.
    OIIO_NODISCARD std::vector<std::string> getDisplayNames() const;

    /// Get the name of the default display.
    OIIO_NODISCARD const char* getDefaultDisplayName() const;

    /// Get the number of views for a given display defined in this
    /// configuration. If the display is empty or not specified, the default
    /// display will be used.
    OIIO_NODISCARD int getNumViews(string_view display = "") const;

    /// Query the name of the specified view for the specified display, or
    /// nullptr if there is no view with that index or if the display is not
    /// found.
    OIIO_NODISCARD const char* getViewNameByIndex(string_view display,
                                                  int index) const;

    /// Retrieve the full list of known view names for the display, as a
    /// vector of strings. If the display is empty or not specified, the
    /// default display will be used.
    OIIO_NODISCARD std::vector<std::string>
    getViewNames(string_view display = "") const;

    /// Query the name of the default view for the specified display. If the
    /// display is empty or not specified, the default display will be used.
    /// This version does not consider the input color space.
    /// Returns nullptr for failure.
    OIIO_NODISCARD const char*
    getDefaultViewName(string_view display = "") const;

    /// Query the name of the default view for the specified display, given
    /// the input color space. If `display` is "default" or an empty string,
    /// the default display will be used. The input color space is used to
    /// determine the most appropriate default view for the given display.
    /// Returns nullptr for failure.
    OIIO_NODISCARD const char*
    getDefaultViewName(string_view display, string_view inputColorSpace) const;

    /// Returns the colorspace attribute of the (display, view) pair. (Note
    /// that this may be either a color space or a display color space.)
    /// Returns nullptr for failure.
    OIIO_NODISCARD const char*
    getDisplayViewColorSpaceName(const std::string& display,
                                 const std::string& view) const;

    /// Returns the looks attribute of a (display, view) pair. Returns nullptr
    /// for failure.
    OIIO_NODISCARD const char*
    getDisplayViewLooks(const std::string& display,
                        const std::string& view) const;

    /// Construct a processor to transform from the given color space
    /// to the color space of the given display and view. You may optionally
    /// override the looks that are, by default, used with the display/view
    /// combination. Looks is a potentially comma (or colon) delimited list
    /// of lookNames, where +/- prefixes are optionally allowed to denote
    /// forward/inverse transformation (and forward is assumed in the
    /// absence of either). It is possible to remove all looks from the
    /// display by passing an empty string. The context_key and context_value
    /// can optionally be used to establish extra key/value pair in the OCIO
    /// context if they are comma-separated lists of context keys and
    /// values, respectively.
    ///
    /// It is possible that this will return an empty handle if one of the
    /// color spaces or the display or view doesn't exist or is not allowed.
    ///
    /// The handle is actually a shared_ptr, so when you're done with a
    /// ColorProcess, just discard it. ColorProcessor(s) remain valid even
    /// if the ColorConfig that created them no longer exists.
    ///
    /// Created ColorProcessors are cached, so asking for the same color
    /// space transformation multiple times shouldn't be very expensive.
    OIIO_NODISCARD ColorProcessorHandle createDisplayTransform(
        string_view display, string_view view, string_view inputColorSpace,
        string_view looks = "", bool inverse = false,
        string_view context_key = "", string_view context_value = "") const;
    OIIO_NODISCARD ColorProcessorHandle createDisplayTransform(
        ustring display, ustring view, ustring inputColorSpace,
        ustring looks = ustring(), bool inverse = false,
        ustring context_key   = ustring(),
        ustring context_value = ustring()) const;

    OIIO_DEPRECATED("prefer the kind that takes an `inverse` parameter (2.5)")
    ColorProcessorHandle
    createDisplayTransform(string_view display, string_view view,
                           string_view inputColorSpace, string_view looks,
                           string_view context_key,
                           string_view context_value = "") const
    {
        return createDisplayTransform(ustring(display), ustring(view),
                                      ustring(inputColorSpace), ustring(looks),
                                      false, ustring(context_key),
                                      ustring(context_value));
    }
    OIIO_DEPRECATED("prefer the kind that takes an `inverse` parameter (2.5)")
    ColorProcessorHandle createDisplayTransform(
        ustring display, ustring view, ustring inputColorSpace, ustring looks,
        ustring context_key, ustring context_value = ustring()) const
    {
        return createDisplayTransform(display, view, inputColorSpace, looks,
                                      false, context_key, context_value);
    }

    /// Construct a processor to perform color transforms determined by an
    /// OpenColorIO FileTransform. It is possible that this will return an
    /// empty handle if the FileTransform doesn't exist or is not allowed.
    ///
    /// The handle is actually a shared_ptr, so when you're done with a
    /// ColorProcess, just discard it. ColorProcessor(s) remain valid even
    /// if the ColorConfig that created them no longer exists.
    ///
    /// Created ColorProcessors are cached, so asking for the same color
    /// space transformation multiple times shouldn't be very expensive.
    OIIO_NODISCARD ColorProcessorHandle
    createFileTransform(string_view name, bool inverse = false) const;
    OIIO_NODISCARD ColorProcessorHandle
    createFileTransform(ustring name, bool inverse = false) const;

    /// Construct a processor to perform color transforms determined by an
    /// OpenColorIO NamedTransform. It is possible that this will return an
    /// empty handle if the NamedTransform doesn't exist or is not allowed.
    ///
    /// The handle is actually a shared_ptr, so when you're done with a
    /// ColorProcess, just discard it. ColorProcessor(s) remain valid even
    /// if the ColorConfig that created them no longer exists.
    ///
    /// Created ColorProcessors are cached, so asking for the same color
    /// space transformation multiple times shouldn't be very expensive.
    OIIO_NODISCARD ColorProcessorHandle createNamedTransform(
        string_view name, bool inverse = false, string_view context_key = "",
        string_view context_value = "") const;
    OIIO_NODISCARD ColorProcessorHandle createNamedTransform(
        ustring name, bool inverse = false, ustring context_key = ustring(),
        ustring context_value = ustring()) const;

    /// Construct a processor to perform color transforms specified by a
    /// 4x4 matrix.
    ///
    /// The handle is actually a shared_ptr, so when you're done with a
    /// ColorProcess, just discard it.
    ///
    /// Created ColorProcessors are cached, so asking for the same color
    /// space transformation multiple times shouldn't be very expensive.
    OIIO_NODISCARD ColorProcessorHandle
    createMatrixTransform(M44fParam M, bool inverse = false) const;

    /// Given a filepath, ask OCIO what color space it thinks the file
    /// should be, based on how the name matches file naming rules in the
    /// OCIO config.  (This is mostly a wrapper around OCIO's
    /// ColorConfig::getColorSpaceFromFilepath.)
    OIIO_NODISCARD string_view getColorSpaceFromFilepath(string_view str) const;

    /// Given a filepath, ask OCIO what color space it thinks the file
    /// should be, based on how the name matches file naming rules in the
    /// OCIO config. If no match is found, return `default_cs` instead of
    /// the OCIO config's default color space. If `cs_name_match` is
    /// true, additionally attempt to match the color space name in the
    /// filename, if the OCIO config heuristics fail to find a match.
    OIIO_NODISCARD string_view getColorSpaceFromFilepath(string_view str,
                                                         string_view default_cs,
                                                         bool cs_name_match
                                                         = false) const;

    /// Given a filepath, returns whether the result of
    /// getColorSpaceFromFilepath() is the failover condition, due
    /// to the OCIO config's file rules not otherwise finding a match
    /// for the filepath.
    OIIO_NODISCARD bool filepathOnlyMatchesDefaultRule(string_view str) const;

    /// Given a string (like a filename), look for the longest, right-most
    /// colorspace substring that appears. Returns "" if no such color space
    /// is found.
    OIIO_NODISCARD string_view parseColorSpaceFromString(string_view str) const;

    /// Turn the name, which could be a color space, an alias, a role, or
    /// an OIIO-understood universal name (like "sRGB") into a canonical
    /// color space name.
    ///
    /// When a direct color space / role / alias lookup does not recognize the
    /// name, resolve() additionally understands several syntactic forms of a
    /// Color Interop ID (see the Color Interop Forum recommendation "An ID for
    /// Color Interop", https://github.com/AcademySoftwareFoundation/ColorInterop/wiki),
    /// tried in this order:
    ///   - a namespaced id (e.g. "studio:acescg") is retried with one leading
    ///     namespace stripped;
    ///   - a "<config>:local:<space>" id resolves against this config's own
    ///     color space names/aliases when "<config>" matches this config's name;
    ///   - an id equal to a color space's explicit `interop_id` attribute, or to
    ///     it with exactly one side's namespace stripped (OCIO 2.5+);
    ///   - the utility token "data" (and, as an OpenImageIO extension not
    ///     defined by the CIF recommendation, "bypass") resolves to a ranked
    ///     data color space (while "unknown" only matches a literal color
    ///     space name/alias).
    /// If none of these recognize the name, the name is returned unchanged.
    OIIO_NODISCARD string_view resolve(string_view name) const;

    /// Like resolve(name), but a name that no tier recognizes returns
    /// `failover` instead of the name unchanged. Passing an empty failover
    /// therefore distinguishes "resolved" from "not recognized", which the
    /// 1-arg overload's historical passthrough cannot. The returned view is
    /// either a view of long-lived config/registry storage (a hit) or the
    /// caller's own `failover` (a miss).
    ///
    /// @version 3.2
    OIIO_NODISCARD string_view resolve(string_view name,
                                       string_view failover) const;

    /// Are the two color space names/aliases/roles equivalent? Each name is
    /// resolve()d first, so color interop IDs and aliases participate on either
    /// side.
    OIIO_NODISCARD bool equivalent(string_view color_space,
                                   string_view other_color_space) const;

    /// Find CICP code corresponding to the colorspace.
    /// Return a cspan of 4 ints, or an empty span if not found.
    ///
    /// @version 3.1
    OIIO_NODISCARD cspan<int> get_cicp(string_view colorspace) const;

    /// Find the Color Interop ID for the given colorspace (see the Color
    /// Interop Forum recommendation "An ID for Color Interop",
    /// https://github.com/AcademySoftwareFoundation/ColorInterop/wiki). This
    /// is a CHEAP lookup: it returns an author-declared `interop_id`
    /// attribute on the resolved space (unconditionally authoritative, OCIO
    /// 2.5+; a data space with no declared token yields "data"), else a
    /// name/alias match against the built-in id/CICP table, else the empty
    /// string. It never probes transforms, builds processors, or
    /// manufactures an id -- an unidentified space is simply "" here, never
    /// a guessed default. The full (expensive) derivation cascade --
    /// fingerprint equivalence against the built-in registry identities and
    /// config-local id generation -- runs at write-planning time when a file
    /// is written, not inside this getter.
    /// Returns empty string if not found.
    ///
    /// @version 3.1
    OIIO_NODISCARD string_view
    get_color_interop_id(string_view colorspace) const;

    /// Find color interop ID corresponding to the CICP code.
    /// Returns empty string if not found.
    ///
    /// @version 3.1
    OIIO_NODISCARD string_view get_color_interop_id(const int cicp[4]) const;

    /// Search the config for color spaces matching a partial color-space
    /// characterization, and return their names ordered deterministically by
    /// (context-invariant, active, simple, name).
    ///
    /// Each of the four hint axes -- `chromaticities`, `transfer_function`,
    /// `encoding`, and `image_state` -- is a list of terms; an empty axis is
    /// unconstrained. Within an axis a term is by default an *include*, a
    /// leading `-` makes it an *exclude*, and a leading `~` makes it an
    /// *inverse* (match only spaces proven to have the opposite property); a
    /// backslash escapes a leading operator (`\-`, `\~`, `\\`). A returned
    /// space passes every non-empty axis. Each term is resolved with the
    /// precedence: exact local color space name or alias, then known interop
    /// ID, then an axis-specific fallback (a gamut component fragment such as
    /// `rec709`; a named transform or transfer triple; a literal encoding; the
    /// image state `scene`, `display`, or `all`). A candidate whose property
    /// cannot be derived is treated as *unknown*, never an error. A malformed
    /// term or an unresolvable hint is reported through the usual ColorConfig
    /// error convention (has_error() / geterror()) -- before any candidate is
    /// examined -- and the search returns an empty list. This method does not
    /// throw.
    ///
    /// By default the search considers the config's active, simple color
    /// spaces. `options.include_inactive` also considers inactive spaces;
    /// `options.include_context_sensitive` also considers spaces whose
    /// transforms depend on context variables; `options.include_complex` also
    /// considers complex (non-simple) spaces by inspecting their authored and
    /// realized transforms. `options.context` applies OCIO context-variable
    /// overrides scoped to this call only.
    ///
    /// On the encoding axis a candidate characterizes as its authored
    /// encoding attribute and, additionally, as the encoding of its
    /// interop-identity twin (so a LUT space tagged with a theatrical
    /// interop ID but authored `sdr-video` matches searches for both
    /// `sdr-video` and `sdr-cinema`); a candidate with no authored encoding
    /// adopts the twin's outright. `options.authored_encoding_only` limits
    /// the encoding axis to authored attributes only.
    ///
    /// Current limitations (each a documented behavior, not a defect):
    /// probe-derived chromaticities assume a single D65 + Bradford hypothesis,
    /// so a non-D65 or log-curve space whose gamut is not in the reserved
    /// chromaticity table may resolve as unknown; registry-side gamuts are
    /// taken from that reserved table only, so a gamut absent from it (e.g.
    /// `ciexyzd65`) is not yet resolvable. Interop IDs that contain a colon
    /// (such as `custom:*` or `icc:*`) must be quoted when passed through the
    /// `oiiotool --colorspacesearch` flag, whose modifier syntax otherwise
    /// treats `:` as its option delimiter.
    ///
    /// @version 3.2
    OIIO_NODISCARD std::vector<std::string>
    find_color_spaces(cspan<std::string> chromaticities      = {},
                      cspan<std::string> transfer_function   = {},
                      cspan<std::string> encoding            = {},
                      cspan<std::string> image_state         = {},
                      const ColorSpaceSearchOptions& options = {}) const;
    /// Retrieve the characterization information OIIO can supply CHEAPLY for
    /// the named color space (which may be a name, role, alias, or Color
    /// Interop ID): the canonical local name, the image state, the cheap
    /// Color Interop ID subset (declared attribute or built-in table match,
    /// exactly what get_color_interop_id() returns), the authored encoding,
    /// and the intrinsic range when explicitly known. Any characterization
    /// facts a previous, more expensive query already derived and cached
    /// (equality ID, chromaticities, transfer function, derived encoding)
    /// are merged into the returned snapshot; this method itself performs
    /// only direct or cached work -- it never probes transforms, builds a
    /// processor, or computes a fingerprint, and it never silently derives
    /// a missing field. Uncached derivable fields simply report
    /// `computed(field) == false`.
    ///
    /// For an unknown or unresolvable name, the returned object has
    /// `valid() == false` and an error is reported through the usual
    /// has_error()/geterror() convention. This method does not throw.
    ///
    /// @version 3.2
    OIIO_NODISCARD ColorSpaceInfo
    get_color_space_info(string_view color_space,
                         const ColorSpaceInfoOptions& options = {}) const;

    /// Batch version of get_color_space_info(): one record per requested
    /// name, in input order (duplicates included). Every requested name is
    /// validated before any record is built; if any input is invalid, one
    /// indexed error (e.g. `get_color_space_infos[3]: unknown color space
    /// "..."`) is reported through has_error()/geterror() and an empty
    /// vector is returned. An empty input span is an empty batch, not "all
    /// spaces". This method does not throw.
    ///
    /// The batch spelling is deliberately distinct (plural, matching the
    /// Python binding) rather than an overload: span's one-element
    /// converting constructor would otherwise make a `std::string` lvalue
    /// argument ambiguous between the scalar and batch forms.
    ///
    /// @version 3.2
    OIIO_NODISCARD std::vector<ColorSpaceInfo>
    get_color_space_infos(cspan<std::string> color_spaces,
                          const ColorSpaceInfoOptions& options = {}) const;

    /// Derive the complete characterization of the named color space (which
    /// may be a name, role, alias, or Color Interop ID): every field of the
    /// returned ColorSpaceInfo has been attempted, by full derivation where
    /// direct inspection does not answer -- fingerprint equivalence for the
    /// equality ID, the full interop-ID derivation cascade, the
    /// interop-counterpart encoding fallback, chromaticity probing, and
    /// transfer-function characterization. This is the EXPENSIVE
    /// counterpart of get_color_space_info(): it may build OCIO processors
    /// and probe transforms. Completed derivations (successful and
    /// negative) are cached, so later queries -- including the cheap getter
    /// -- see the derived facts without recomputing them; records already
    /// held by callers are immutable snapshots and are not affected.
    ///
    /// "Complete" does not mean every field is available: an
    /// uncharacterizable field reports `computed(field) == true` with
    /// `available(field) == false`, a stable negative result rather than an
    /// error. Range in particular is supplied only when intrinsic to a
    /// registered identity or otherwise explicitly known -- it is never
    /// guessed from a color-space name.
    ///
    /// For an unknown or unresolvable name, the returned object has
    /// `valid() == false` and an error is reported through the usual
    /// has_error()/geterror() convention. This method does not throw.
    ///
    /// @version 3.2
    OIIO_NODISCARD ColorSpaceInfo
    derive_color_space_info(string_view color_space,
                            const ColorSpaceInfoOptions& options = {}) const;

    /// Batch version of derive_color_space_info(): one record per requested
    /// name, in input order (duplicates included). Every requested name is
    /// validated before any record is derived; if any input is invalid, one
    /// indexed error (e.g. `derive_color_space_infos[3]: unknown color
    /// space "..."`) is reported through has_error()/geterror() and an
    /// empty vector is returned. Per-field derivation failure remains an
    /// unavailable field, never a failed batch. An empty input span is an
    /// empty batch, not "all spaces". This method does not throw. (The
    /// batch name is plural for the same overload-ambiguity reason as
    /// get_color_space_infos().)
    ///
    /// @version 3.2
    OIIO_NODISCARD std::vector<ColorSpaceInfo>
    derive_color_space_infos(cspan<std::string> color_spaces,
                             const ColorSpaceInfoOptions& options = {}) const;

    /// The canonical Color Interop Forum IDs declared by OpenImageIO's
    /// built-in interop identities registry (see the CIF recommendation
    /// "An ID for Color Interop",
    /// https://github.com/AcademySoftwareFoundation/ColorInterop/wiki), in
    /// deterministic (sorted) registry order. Each is usable anywhere a
    /// `string_view` CIID is accepted -- as the argument to resolve() or
    /// equivalent(), or compared against get_color_interop_id()'s return
    /// value.
    ///
    /// Static because the builtin IDs come from the embedded registry, not
    /// from any config: there is no instance to consult. The returned
    /// storage has process lifetime, so the span stays valid and repeated
    /// calls return the same data.
    ///
    /// This is registry *data*, not an exhaustive ID grammar: raw strings
    /// remain first-class for the ids no finite set can enumerate
    /// (local/custom/icc/user-namespaced).
    ///
    /// @version 3.2
    OIIO_NODISCARD static cspan<string_view> get_builtin_interop_ids();

    /// Convenience alias so callers may spell the options type
    /// `ColorConfig::SerializeOptions`.
    using SerializeOptions = ColorConfigSerializeOptions;

    /// Return the config serialized as OCIO YAML text (a wrapper around
    /// OCIO's Config::serialize()). This is the text of the IN-MEMORY config
    /// object -- including any construction-time fix-ups OIIO applied -- not
    /// a copy of the file it was loaded from. With
    /// `options.interopified = true`, serialize the interoperability-repaired
    /// in-memory copy instead. On failure (no usable config, or an OCIO
    /// serialization error), return an empty string and report the error
    /// through the usual has_error()/geterror() convention. This method does
    /// not throw.
    ///
    /// @version 3.2
    OIIO_NODISCARD std::string
    serialize(const SerializeOptions& options = {}) const;

    /// Construct a ColorConfig from the OCIO YAML text of a config held in
    /// memory (a wrapper around OCIO's Config::CreateFromStream), rather
    /// than from a file. `working_dir`, if non-empty, sets the config's
    /// working directory, which OCIO uses to resolve relative FileTransform
    /// sources and search paths (CreateFromStream itself sets none) and
    /// which archive() requires. Like the file constructor, this does not
    /// throw: on failure the returned object reports the problem through
    /// the usual has_error()/geterror() convention.
    ///
    /// @version 3.2
    OIIO_NODISCARD static ColorConfig from_text(string_view config_text,
                                                string_view working_dir = "");

    /// Convenience alias so callers may spell the options type
    /// `ColorConfig::EvolveOptions`.
    using EvolveOptions = ColorConfigEvolveOptions;

    /// Return a NEW ColorConfig that is a copy of this one with the
    /// modifications described by `options` applied -- the public face of
    /// the copy-on-modify contract: this config is frozen and is never
    /// mutated; the evolved instance is an independent config with its own
    /// caches. `options.context` overrides context-variable defaults (which
    /// changes the config's structural cache identity, since the overrides
    /// serialize with it); `options.working_dir` re-points runtime file
    /// resolution (working directory is runtime state OCIO does not fold
    /// into the structural cache id); `options.reset` starts from the
    /// ORIGINAL config this one was first constructed with before applying
    /// the other fields, so an evolve chain can always get back to its
    /// root. On failure the returned config reports the problem through
    /// the usual has_error()/geterror() convention. This method does not
    /// throw.
    ///
    /// @version 3.2
    OIIO_NODISCARD ColorConfig evolve(const EvolveOptions& options = {}) const;

    /// Convenience alias so callers may spell the options type
    /// `ColorConfig::ArchiveOptions`.
    using ArchiveOptions = ColorConfigArchiveOptions;

    /// Archive the config and the LUT files it depends on into `filename`
    /// as an OCIO config archive (a wrapper around OCIO's
    /// Config::archive(); the conventional extension is `.ocioz`, readable
    /// wherever OCIO configs are accepted, including the ColorConfig
    /// filename constructor). It is the IN-MEMORY config object that is
    /// archived, together with every candidate LUT file under the working
    /// directory; `options.working_dir` overrides the working directory for
    /// the one archive operation, and `options.interopified` archives the
    /// interoperability-repaired in-memory copy instead. Return true on
    /// success; on failure (no usable config, a config OCIO deems
    /// unarchivable, or an I/O error) return false and report the error
    /// through the usual has_error()/geterror() convention. This method
    /// does not throw.
    ///
    /// @version 3.2
    OIIO_NODISCARD_ERROR bool archive(string_view filename,
                                      const ArchiveOptions& options = {}) const;

    /// Convenience alias so callers may spell the options type
    /// `ColorConfig::DebugInfoOptions`.
    using DebugInfoOptions = ColorConfigDebugInfoOptions;

    /// Return this config's identity and cache state, for diagnostics and
    /// bug reports: the OpenImageIO and OpenColorIO versions, the config's
    /// name and cache identities, the interoperability (interchange
    /// discovery) state, the built-in interop registry data version, and
    /// per-layer cache entry counts. This reads existing internal state
    /// only: it never triggers lazy work, so a discovery that has not yet
    /// run reports as `ColorInterchangeState::Pending`.
    /// `ColorConfigDebugInfo::to_string()` renders the same
    /// human-readable report for pasting into a bug report. `options` is
    /// reserved for future report selectors.
    ///
    /// @version 3.2
    OIIO_NODISCARD ColorConfigDebugInfo
    get_debug_info(const DebugInfoOptions& options = {}) const;

    /// Convenience alias so callers may spell the options type
    /// `ColorConfig::ClearCachesOptions`.
    using ClearCachesOptions = ColorConfigClearCachesOptions;

    /// Drop cached derived state for this config: its per-instance color
    /// processor cache, and the entries scoped to this config's cache
    /// identity in the process-global fingerprint and characterization
    /// memo caches. Clearing is semantics-free -- every cache repopulates
    /// on demand -- so the only observable effects are memory and
    /// recompute time (get_debug_info() reports the entry counts). Shared
    /// process data not scoped to this config (e.g. the built-in interop
    /// registry) is unaffected. `options` is reserved for future
    /// selectors.
    ///
    /// @version 3.2
    void clear_caches(const ClearCachesOptions& options = {}) const;

    /// Return a filename or other identifier for the config we're using.
    OIIO_NODISCARD std::string configname() const;

    /// Set the spec's metadata to presume that color space is `name` (or to
    /// assume nothing about the color space if `name` is empty). The core
    /// operation is to set the "oiio:ColorSpace" attribute, and the
    /// surrounding metadata maintenance follows the two-bucket color
    /// metadata hygiene:
    ///
    /// - Asserting a different space than the spec already claims scrubs
    ///   the *file-provenance facts* that described the old claim
    ///   (`colorInteropID`, `CICP`, `chromaticities`, `ICCProfile`,
    ///   `oiio:Gamma`, `acesImageContainerFlag` -- the deliberate
    ///   `*:unknown` marker family excepted), and maintains any
    ///   *current-state descriptors* the spec carries
    ///   (`oiio:ColorSpace:state` / `:encoding` / `:range` /
    ///   `:equality_id`): each is updated from the cheap characterization
    ///   of the new space when available, erased when not -- never
    ///   guessed, never derived by this call.
    /// - An empty `name` erases everything: the verdict, the provenance
    ///   facts, and the descriptors (absence semantics -- assume nothing).
    /// - First tagging (no previous claim) leaves the provenance facts in
    ///   place: at read time the claim is routinely derived from those
    ///   very facts, which are evidence for it, not contradictions.
    /// - Re-asserting the space the spec already claims is a no-op.
    ///
    /// A few format-specific hints that may contradict the new claim are
    /// also removed in all cases (`Exif:ColorSpace` unless `name` is
    /// sRGB-equivalent, `tiff:ColorSpace`, `tiff:PhotometricInterpretation`).
    ///
    /// @version 3.0 (two-bucket hygiene since 3.2)
    void set_colorspace(ImageSpec& spec, string_view name) const;

    /// Set the spec's metadata to reflect Rec709 color primaries and the given
    /// gamma. The core operation is to set the "oiio:ColorSpace" attribute
    /// (via set_colorspace(), whose metadata hygiene applies), and
    /// additionally record the given gamma as "oiio:Gamma".
    ///
    /// @version 3.0
    void set_colorspace_rec709_gamma(ImageSpec& spec, float gamma) const;

    /// Return if OpenImageIO was built with OCIO support
    OIIO_NODISCARD static bool supportsOpenColorIO();

    /// Return the hex OCIO version (maj<<24 + min<<16 + patch), or 0 if no
    /// OCIO support is available.
    OIIO_NODISCARD static int OpenColorIO_version_hex();

    /// Return a default ColorConfig, which is a singleton that will be
    /// created the first time it is needed.  It will be initialized with the
    /// OCIO environment variable, if it exists, or the OCIO built-in config
    /// (for OCIO >= 2.2).  If neither of those is possible, it will be
    /// initialized with a built-in minimal config.
    OIIO_NODISCARD static const ColorConfig& default_colorconfig();

private:
    ColorConfig(const ColorConfig&)            = delete;
    ColorConfig& operator=(const ColorConfig&) = delete;

    // Tag for the internal allocate-but-don't-initialize constructor used
    // by the from-memory factories (from_text, evolve).
    struct UninitTag {};
    explicit ColorConfig(UninitTag);

    class Impl;
    std::unique_ptr<Impl> m_impl;
    Impl* getImpl() const { return m_impl.get(); }

#ifdef OIIO_INTERNAL
    friend struct pvt::ColorConfigClassificationPeek;
#endif
};

OIIO_NAMESPACE_3_1_END


// Compatibility
#ifndef OIIO_DOXYGEN
OIIO_NAMESPACE_BEGIN
using v3_1::ColorProcessorHandle;
OIIO_NAMESPACE_END
#endif


OIIO_NAMESPACE_BEGIN

/// Utility -- convert sRGB value to linear transfer function, without
/// any change in color primaries.
///    http://en.wikipedia.org/wiki/SRGB
OIIO_NODISCARD inline float
sRGB_to_linear(float x)
{
    return (x <= 0.04045f) ? (x * (1.0f / 12.92f))
                           : powf((x + 0.055f) * (1.0f / 1.055f), 2.4f);
}


#ifndef __CUDA_ARCH__
OIIO_NODISCARD inline simd::vfloat4
sRGB_to_linear(const simd::vfloat4& x)
{
    return simd::select(
        x <= 0.04045f, x * (1.0f / 12.92f),
        fast_pow_pos(madd(x, (1.0f / 1.055f), 0.055f * (1.0f / 1.055f)), 2.4f));
}
#endif

/// Utility -- convert linear value to sRGB transfer function, without
/// any change in color primaries.
OIIO_NODISCARD inline float
linear_to_sRGB(float x)
{
    return (x <= 0.0031308f) ? (12.92f * x)
                             : (1.055f * powf(x, 1.f / 2.4f) - 0.055f);
}


#ifndef __CUDA_ARCH__
/// Utility -- convert linear value to sRGB transfer function, without
/// any change in color primaries.
OIIO_NODISCARD inline simd::vfloat4
linear_to_sRGB(const simd::vfloat4& x)
{
    // x = simd::max (x, simd::vfloat4::Zero());
    return simd::select(x <= 0.0031308f, 12.92f * x,
                        madd(1.055f, fast_pow_pos(x, 1.f / 2.4f), -0.055f));
}
#endif


/// Utility -- convert Rec709 value to linear transfer function, without
/// any change in color primaries.
///    http://en.wikipedia.org/wiki/Rec._709
OIIO_NODISCARD inline float
Rec709_to_linear(float x)
{
    if (x < 0.081f)
        return x * (1.0f / 4.5f);
    else
        return powf((x + 0.099f) * (1.0f / 1.099f), (1.0f / 0.45f));
}

/// Utility -- convert linear value to Rec709 transfer function, without
/// any change in color primaries.
OIIO_NODISCARD inline float
linear_to_Rec709(float x)
{
    if (x < 0.018f)
        return x * 4.5f;
    else
        return 1.099f * powf(x, 0.45f) - 0.099f;
}


OIIO_NAMESPACE_END
