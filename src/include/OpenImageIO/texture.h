// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// clang-format off

/// \file
/// An API for accessing filtered texture lookups via a system that
/// automatically manages a cache of resident texture.

#pragma once

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/simd.h>
#include <OpenImageIO/ustring.h>
#include <OpenImageIO/vecparam.h>


// Define symbols that let client applications determine if newly added
// features are supported.
#define OIIO_TEXTURESYSTEM_SUPPORTS_CLOSE 1
#define OIIO_TEXTURESYSTEM_SUPPORTS_COLORSPACE 1

// Is the getattributetype() method present? (Added in 2.5)
#define OIIO_TEXTURESYSTEM_SUPPORTS_GETATTRIBUTETYPE 1

#define OIIO_TEXTURESYSTEM_SUPPORTS_STOCHASTIC 1
#define OIIO_TEXTURESYSTEM_SUPPORTS_DECODE_BY_USTRINGHASH 1

// Does TextureSystem::create() return a shared pointer?
#define OIIO_TEXTURESYSTEM_CREATE_SHARED 1

// Revision of the TextureOpt class
#define OIIO_TEXTUREOPT_VERSION 2
#define OIIO_TEXTUREOPTBATCH_VERSION 1

// Preprocessor utility: Concatenation
#define OIIO_CONCAT_HELPER(a,b) a ## b
#define OIIO_CONCAT_VERSION(a,b) OIIO_CONCAT_HELPER(a,b)

#define TextureOpt_current \
    OIIO_CONCAT_VERSION(TextureOpt_v, OIIO_TEXTUREOPT_VERSION)
#define TextureOptBatch_current \
    OIIO_CONCAT_VERSION(TextureOptBatch_v, OIIO_TEXTUREOPTBATCH_VERSION)


#ifndef INCLUDED_IMATHVEC_H
// Placeholder declaration for Imath::V3f if no Imath headers have been
// included.
namespace Imath {
template <class T> class Vec3;
using V3f = Vec3<float>;
}
#endif


OIIO_NAMESPACE_BEGIN

// Forward declarations

class ImageCache;
class TextureSystemImpl;


namespace pvt {


// Used internally by TextureSystem.  Unfortunately, this is the only
// clean place to store it.  Sorry, users, this isn't really for you.
enum TexFormat {
    TexFormatUnknown,
    TexFormatTexture,
    TexFormatTexture3d,
    TexFormatShadow,
    TexFormatCubeFaceShadow,
    TexFormatVolumeShadow,
    TexFormatLatLongEnv,
    TexFormatCubeFaceEnv,
    TexFormatLast
};

enum EnvLayout {
    LayoutTexture = 0 /* ordinary texture - no special env wrap */,
    LayoutLatLong,
    LayoutCubeThreeByTwo,
    LayoutCubeOneBySix,
    EnvLayoutLast
};

}  // namespace pvt



namespace Tex {

/// Wrap mode describes what happens when texture coordinates describe
/// a value outside the usual [0,1] range where a texture is defined.
enum class Wrap : uint8_t {
    Default,               ///< Use the default found in the file
    Black,                 ///< Black outside [0..1]
    Clamp,                 ///< Clamp to [0..1]
    Periodic,              ///< Periodic mod 1
    Mirror,                ///< Mirror the image
    PeriodicPow2,          ///< Periodic, but only for powers of 2!!!
    PeriodicSharedBorder,  ///< Periodic with shared border (env)
    Last                   ///< Mark the end -- don't use this!
};

/// Utility: Return the Wrap enum corresponding to a wrap name:
/// "default", "black", "clamp", "periodic", "mirror".
OIIO_API Wrap decode_wrapmode (const char *name);
OIIO_API Wrap decode_wrapmode (ustring name);
OIIO_API Wrap decode_wrapmode (ustringhash name);

/// Utility: Parse a single wrap mode (e.g., "periodic") or a
/// comma-separated wrap modes string (e.g., "black,clamp") into
/// separate Wrap enums for s and t.
OIIO_API void parse_wrapmodes (const char *wrapmodes,
                               Wrap &swrapcode, Wrap &twrapcode);


/// Mip mode determines if/how we use mipmaps
///
enum class MipMode : uint8_t {
    Default,    ///< Default high-quality lookup
    NoMIP,      ///< Just use highest-res image, no MIP mapping
    OneLevel,   ///< Use just one mipmap level
    Trilinear,  ///< Use two MIPmap levels (trilinear)
    Aniso,      ///< Use two MIPmap levels w/ anisotropic
};

/// Interp mode determines how we sample within a mipmap level
///
enum class InterpMode : uint8_t {
    Closest,      ///< Force closest texel
    Bilinear,     ///< Force bilinear lookup within a mip level
    Bicubic,      ///< Force cubic lookup within a mip level
    SmartBicubic  ///< Bicubic when magnifying, else bilinear
};


/// Fixed width for SIMD batching texture lookups.
/// May be changed for experimentation or future expansion.
#ifndef OIIO_TEXTURE_SIMD_BATCH_WIDTH
#    define OIIO_TEXTURE_SIMD_BATCH_WIDTH 16
#endif

/// The SIMD width for batched texturing operations. This is fixed within
/// any release of OpenImageIO, but may change from release to release and
/// also may be overridden at build time. A typical batch size is 16.
inline constexpr int BatchWidth = OIIO_TEXTURE_SIMD_BATCH_WIDTH;
inline constexpr int BatchAlign = BatchWidth * sizeof(float);

/// A type alias for a SIMD vector of floats with the batch width.
typedef simd::VecType<float, OIIO_TEXTURE_SIMD_BATCH_WIDTH>::type FloatWide;

/// A type alias for a SIMD vector of ints with the batch width.
typedef simd::VecType<int, OIIO_TEXTURE_SIMD_BATCH_WIDTH>::type IntWide;

/// `RunMask` is defined to be an integer large enough to hold at least
/// `BatchWidth` bits. The least significant bit corresponds to the first
/// (i.e., `[0]`) position of all batch arrays. For each position `i` in the
/// batch, the bit identified by `(1 << i)` controls whether that position
/// will be computed.
typedef uint64_t RunMask;


// The defined constant `RunMaskOn` contains the value with all bits
// `0..BatchWidth-1` set to 1.
#if OIIO_TEXTURE_SIMD_BATCH_WIDTH == 4
inline constexpr RunMask RunMaskOn = 0xf;
#elif OIIO_TEXTURE_SIMD_BATCH_WIDTH == 8
inline constexpr RunMask RunMaskOn = 0xff;
#elif OIIO_TEXTURE_SIMD_BATCH_WIDTH == 16
inline constexpr RunMask RunMaskOn = 0xffff;
#elif OIIO_TEXTURE_SIMD_BATCH_WIDTH == 32
inline constexpr RunMask RunMaskOn = 0xffffffff;
#elif OIIO_TEXTURE_SIMD_BATCH_WIDTH == 64
inline constexpr RunMask RunMaskOn = 0xffffffffffffffffULL;
#else
#    error "Not a valid OIIO_TEXTURE_SIMD_BATCH_WIDTH choice"
#endif

}  // namespace Tex


/// Data type for flags that indicate on a point-by-point basis whether
/// we want computations to be performed.
typedef unsigned char Runflag;

/// Pre-defined values for Runflag's.
///
enum RunFlagVal { RunFlagOff = 0, RunFlagOn = 255 };

class TextureOptions;  // forward declaration



/// TextureOpt is a structure that holds many options controlling
/// single-point texture lookups.  Because each texture lookup API call
/// takes a reference to a TextureOpt, the call signatures remain
/// uncluttered rather than having an ever-growing list of parameters, most
/// of which will never vary from their defaults.
///
/// Users should use `TextureOpt`, which will always be an alias to the latest
/// version of this class. But the "real name" is versioned to allow future
/// compatibility changes.
class OIIO_API TextureOpt_v2 {
public:
    // Definitions for preserving back compatibility.
    // These aliases will eventually be deprecated.
    using Wrap = Tex::Wrap;
    using MipMode = Tex::MipMode;
    using InterpMode = Tex::InterpMode;
    static constexpr Tex::Wrap WrapDefault = Tex::Wrap::Default;
    static constexpr Tex::Wrap WrapBlack = Tex::Wrap::Black;
    static constexpr Tex::Wrap WrapClamp = Tex::Wrap::Clamp;
    static constexpr Tex::Wrap WrapPeriodic = Tex::Wrap::Periodic;
    static constexpr Tex::Wrap WrapMirror = Tex::Wrap::Mirror;
    static constexpr Tex::Wrap WrapPeriodicPow2 = Tex::Wrap::PeriodicPow2;
    static constexpr Tex::Wrap WrapPeriodicSharedBorder = Tex::Wrap::PeriodicSharedBorder;
    static constexpr Tex::Wrap WrapLast = Tex::Wrap::Last;
    static constexpr Tex::MipMode MipModeDefault = MipMode::Default;
    static constexpr Tex::MipMode MipModeNoMIP = MipMode::NoMIP;
    static constexpr Tex::MipMode MipModeOneLevel = MipMode::OneLevel;
    static constexpr Tex::MipMode MipModeTrilinear = MipMode::Trilinear;
    static constexpr Tex::MipMode MipModeAniso = MipMode::Aniso;
    static constexpr Tex::InterpMode InterpClosest = Tex::InterpMode::Closest;
    static constexpr Tex::InterpMode InterpBilinear = Tex::InterpMode::Bilinear;
    static constexpr Tex::InterpMode InterpBicubic = Tex::InterpMode::Bicubic;
    static constexpr Tex::InterpMode InterpSmartBicubic = Tex::InterpMode::SmartBicubic;


    /// Create a TextureOpt with all fields initialized to reasonable
    /// defaults.
    OIIO_HOSTDEVICE TextureOpt_v2() { }

    /// Convert a TextureOptions for one index into a TextureOpt.
    ///
    TextureOpt_v2(const TextureOptions& opt, int index);

    int firstchannel = 0;           ///< First channel of the lookup
    int subimage = 0;               ///< Subimage or face ID
    ustring subimagename;           ///< Subimage name
    Wrap swrap = Wrap::Default;     ///< Wrap mode in the s direction
    Wrap twrap = Wrap::Default;     ///< Wrap mode in the t direction
    Wrap rwrap = Wrap::Default;     ///< Wrap mode in the r direction (volume)
    MipMode mipmode = MipMode::Default;  ///< Mip mode
    InterpMode interpmode = InterpMode::SmartBicubic;  ///< Interpolation mode
    bool conservative_filter = true;  ///< True == over-blur rather than alias
    uint16_t anisotropic = 32;        ///< Maximum anisotropic ratio
    float sblur = 0, tblur = 0, rblur = 0;  ///< Blur amount
    float swidth = 1, twidth = 1;   ///< Multiplier for derivatives
    float rwidth = 1;               ///< Multiplier for derivs in r direction
    float fill = 0;                 ///< Fill value for missing channels
    const float* missingcolor = nullptr;  ///< Color for missing texture
    float rnd = -1;                 ///< Stratified sample value
    int colortransformid = 0;       ///< Color space id of the texture

    /// Utility: Return the Wrap enum corresponding to a wrap name:
    /// "default", "black", "clamp", "periodic", "mirror".
    static Wrap decode_wrapmode(const char* name)
    {
        return (Wrap)Tex::decode_wrapmode(name);
    }
    static Wrap decode_wrapmode(ustring name)
    {
        return (Wrap)Tex::decode_wrapmode(name);
    }
    static Wrap decode_wrapmode(ustringhash name)
    {
        return (Wrap)Tex::decode_wrapmode(name);
    }

    /// Utility: Parse a single wrap mode (e.g., "periodic") or a
    /// comma-separated wrap modes string (e.g., "black,clamp") into
    /// separate Wrap enums for s and t.
    static void parse_wrapmodes(const char* wrapmodes, Wrap& swrapcode,
                                Wrap& twrapcode)
    {
        Tex::parse_wrapmodes(wrapmodes, swrapcode, twrapcode);
    }

private:
    // Options set INTERNALLY by libtexture after the options are passed
    // by the user.  Users should not attempt to alter these!
    int envlayout = 0;  // Layout for environment wrap
    friend class TextureSystemImpl;
};


using TextureOpt = TextureOpt_current;



/// Texture options for a batch of Tex::BatchWidth points and run mask.
class OIIO_API TextureOptBatch_current {
public:
    using simd_t = simd::VecType<float, Tex::BatchWidth>::type;

    /// Create a TextureOptBatch with all fields initialized to reasonable
    /// defaults.
    TextureOptBatch_current() {
        *((simd_t*)&sblur) = simd_t::Zero();
        *((simd_t*)&tblur) = simd_t::Zero();
        *((simd_t*)&rblur) = simd_t::Zero();
        *((simd_t*)&swidth) = simd_t::One();
        *((simd_t*)&twidth) = simd_t::One();
        *((simd_t*)&rwidth) = simd_t::One();
        *((simd_t*)&rnd) = simd_t(-1.0f);
    }

    // Options that may be different for each point we're texturing
    alignas(Tex::BatchAlign) float sblur[Tex::BatchWidth];    ///< Blur amount
    alignas(Tex::BatchAlign) float tblur[Tex::BatchWidth];
    alignas(Tex::BatchAlign) float rblur[Tex::BatchWidth];
    alignas(Tex::BatchAlign) float swidth[Tex::BatchWidth];   ///< Multiplier for derivatives
    alignas(Tex::BatchAlign) float twidth[Tex::BatchWidth];
    alignas(Tex::BatchAlign) float rwidth[Tex::BatchWidth];
    // Note: rblur,rwidth only used for volumetric lookups
    alignas(Tex::BatchAlign) float rnd[Tex::BatchWidth];

    // Options that must be the same for all points we're texturing at once
    int firstchannel = 0;                 ///< First channel of the lookup
    int subimage = 0;                     ///< Subimage or face ID
    ustring subimagename;                 ///< Subimage name
#if OIIO_TEXTUREOPTBATCH_VERSION == 1
    // Required at the moment by OSL
    // N.B. We'd like the following types to be Tex::Wrap, MipMode, and
    // InterpMode, and to adjust the size of anisotropic and interpmode, like
    // we did for TextureOpt. But it requires extensive changes on the OSL
    // side. We'll come back to that later, maybe that is for
    // TextureOptBatch_v2.
    int swrap = int(Tex::Wrap::Default); ///< Wrap mode in the s direction
    int twrap = int(Tex::Wrap::Default); ///< Wrap mode in the t direction
    int rwrap = int(Tex::Wrap::Default); ///< Wrap mode in the r direction (volumetric)
    int mipmode = int(Tex::MipMode::Default);  ///< Mip mode
    int interpmode = int(Tex::InterpMode::SmartBicubic);  ///< Interpolation mode
    int anisotropic = 32;                 ///< Maximum anisotropic ratio
    int conservative_filter = 1;          ///< True: over-blur rather than alias
#else
    // Ideal would be for v2:
    Tex::Wrap swrap = Tex::Wrap::Default; ///< Wrap mode in the s direction
    Tex::Wrap twrap = Tex::Wrap::Default; ///< Wrap mode in the t direction
    Tex::Wrap rwrap = Tex::Wrap::Default; ///< Wrap mode in the r direction (volumetric)
    Tex::MipMode mipmode = Tex::MipMode::Default;  ///< Mip mode
    Tex::InterpMode interpmode = Tex::InterpMode::SmartBicubic;  ///< Interpolation mode
    // FIXME: fix the following order and type for v3 to match TextureOpt
    int anisotropic = 32;                 ///< Maximum anisotropic ratio
    int conservative_filter = 1;          ///< True: over-blur rather than alias
#endif
    float fill = 0.0f;                    ///< Fill value for missing channels
    const float *missingcolor = nullptr;  ///< Color for missing texture
    int colortransformid = 0;             ///< Color space id of the texture

private:
    // Options set INTERNALLY by libtexture after the options are passed
    // by the user.  Users should not attempt to alter these!
    int envlayout = 0;               // Layout for environment wrap

    friend class TextureSystemImpl;
};


using TextureOptBatch = TextureOptBatch_current;

// clang-format on



/// Define an API to an abstract class that that manages texture files,
/// caches of open file handles as well as tiles of texels so that truly
/// huge amounts of texture may be accessed by an application with low
/// memory footprint, and ways to perform antialiased texture, shadow
/// map, and environment map lookups.
class OIIO_API TextureSystem {
public:
    /// @{
    /// @name Creating and destroying a texture system
    ///
    /// TextureSystem is an abstract API described as a pure virtual class.
    /// The actual internal implementation is not exposed through the
    /// external API of OpenImageIO.  Because of this, you cannot construct
    /// or destroy the concrete implementation, so two static methods of
    /// TextureSystem are provided:

    /// Create a TextureSystem and return a shared pointer to it.
    ///
    /// @param  shared
    ///     If `shared` is `true`, the pointer returned will be a shared
    ///     TextureSystem, (so that multiple parts of an application that
    ///     request a TextureSystem will all end up with the same one, and
    ///     the same underlying ImageCache). If `shared` is `false`, a
    ///     completely unique TextureCache will be created and returned.
    ///
    /// @param  imagecache
    ///     If `shared` is `false` and `imagecache` is not empty, the
    ///     TextureSystem will use this as its underlying ImageCache. In
    ///     that case, it is the caller who is responsible for eventually
    ///     freeing the ImageCache after the TextureSystem is destroyed.  If
    ///     `shared` is `false` and `imagecache` is empty, then a custom
    ///     ImageCache will be created, owned by the TextureSystem, and
    ///     automatically freed when the TS destroys. If `shared` is true,
    ///     this parameter will not be used, since the global shared
    ///     TextureSystem uses the global shared ImageCache.
    ///
    /// @returns
    ///     A shared pointer to a TextureSystem which will be destroyed only
    ///     when the last shared_ptr to it is destroyed.
    ///
    /// @see    TextureSystem::destroy
    static std::shared_ptr<TextureSystem>
    create(bool shared = true, std::shared_ptr<ImageCache> imagecache = {});

    /// Release the shared_ptr to a TextureSystem, including freeing all
    /// system resources that it holds if no one else is still using it. This
    /// is not strictly necessary to call, simply destroying the shared_ptr
    /// will do the same thing, but this call is for backward compatibility
    /// and is helpful if you want to use the teardown_imagecache option.
    ///
    /// @param  ts
    ///     Shared pointer to the TextureSystem to destroy.
    ///
    /// @param  teardown_imagecache
    ///     For a shared TextureSystem, if the `teardown_imagecache`
    ///     parameter is `true`, it will try to truly destroy the shared
    ///     cache if nobody else is still holding a reference (otherwise, it
    ///     will leave it intact). This parameter has no effect if `ts` was
    ///     not the single globally shared TextureSystem.
    static void destroy(std::shared_ptr<TextureSystem>& ts,
                        bool teardown_imagecache = false);

    /// @}


    /// @{
    /// @name Setting options and limits for the texture system
    ///
    /// These are the list of attributes that can bet set or queried by
    /// attribute/getattribute:
    ///
    /// All attributes ordinarily recognized by ImageCache are accepted and
    /// passed through to the underlying ImageCache. These include:
    /// - `int max_open_files` :
    ///             Maximum number of file handles held open.
    /// - `float max_memory_MB` :
    ///             Maximum tile cache size, in MB.
    /// - `string searchpath` :
    ///             Colon-separated search path for texture files.
    /// - `string plugin_searchpath` :
    ///             Colon-separated search path for plugins.
    /// - `int autotile` :
    ///             If >0, tile size to emulate for non-tiled images.
    /// - `int autoscanline` :
    ///             If nonzero, autotile using full width tiles.
    /// - `int automip` :
    ///             If nonzero, emulate mipmap on the fly.
    /// - `int accept_untiled` :
    ///             If nonzero, accept untiled images.
    /// - `int accept_unmipped` :
    ///             If nonzero, accept unmipped images.
    /// - `int failure_retries` :
    ///             How many times to retry a read failure.
    /// - `int deduplicate` :
    ///             If nonzero, detect duplicate textures (default=1).
    /// - `int max_open_files_strict` :
    ///             If nonzero, work harder to make sure that we have
    ///             smaller possible overages to the max open files limit.
    /// - `string substitute_image` :
    ///             If supplied, an image to substatute for all texture
    ///             references.
    /// - `int max_errors_per_file` :
    ///             Limits how many errors to issue for each file. (default:
    ///             100)
    /// - `string colorspace` :
    ///             The working colorspace of the texture system.
    /// - `string colorconfig` :
    ///             Name of the OCIO config to use (default: "").
    ///
    /// Texture-specific settings:
    /// - `matrix44 worldtocommon` / `matrix44 commontoworld` :
    ///             The 4x4 matrices that provide the spatial transformation
    ///             from "world" to a "common" coordinate system and back.
    ///             This is mainly used for shadow map lookups, in which the
    ///             shadow map itself encodes the world coordinate system,
    ///             but positions passed to `shadow()` are expressed in
    ///             "common" coordinates. You do not need to set
    ///             `commontoworld` and `worldtocommon` separately; just
    ///             setting either one will implicitly set the other, since
    ///             each is the inverse of the other.
    /// - `int gray_to_rgb` :
    ///             If set to nonzero, texture lookups of single-channel
    ///             (grayscale) images will replicate the sole channel's
    ///             values into the next two channels, making it behave like
    ///             an RGB image that happens to have all three channels
    ///             with identical pixel values.  (Channels beyond the third
    ///             will get the "fill" value.) The default value of zero
    ///             means that all missing channels will get the "fill"
    ///             color.
    /// - `int max_tile_channels` :
    ///             The maximum number of color channels in a texture file
    ///             for which all channels will be loaded as a single cached
    ///             tile. Files with more than this number of color channels
    ///             will have only the requested subset loaded, in order to
    ///             save cache space (but at the possible wasted expense of
    ///             separate tiles that overlap their channel ranges). The
    ///             default is 5.
    /// - `int max_mip_res` :
    ///             **NEW 2.1** Sets the maximum MIP-map resolution for
    ///             filtered texture lookups. The MIP levels used will be
    ///             clamped to those having fewer than this number of pixels
    ///             in each dimension. This can be helpful as a way to limit
    ///             disk I/O when doing fast preview renders (with the
    ///             tradeoff that you may see some texture more blurry than
    ///             they would ideally be). The default is `1 << 30`, a
    ///             value so large that no such clamping will be performed.
    /// - `string latlong_up` :
    ///             The default "up" direction for latlong environment maps
    ///             (only applies if the map itself doesn't specify a format
    ///             or is in a format that explicitly requires a particular
    ///             orientation).  The default is `"y"`.  (Currently any
    ///             other value will result in *z* being "up.")
    /// - `int flip_t` :
    ///             If nonzero, `t` coordinates will be flipped `1-t` for
    ///             all texture lookups. The default is 0.
    /// - `int stochastic` :
    ///             Bit field determining how to use stochastic sampling for
    ///             MipModeStochasticAniso and/or MipModeStochasticTrilinear.
    ///             Bit 1 = sample MIP level, bit 2 = sample anisotropy
    ///             (default=0).
    ///
    /// - `string options`
    ///             This catch-all is simply a comma-separated list of
    ///             `name=value` settings of named options, which will be
    ///             parsed and individually set.
    ///
    ///                 ic->attribute ("options", "max_memory_MB=512.0,autotile=1");
    ///
    ///             Note that if an option takes a string value that must
    ///             itself contain a comma, it is permissible to enclose the
    ///             value in either single `\'` or double `"` quotes.
    ///
    /// **Read-only attributes**
    ///
    /// Additionally, there are some read-only attributes that can be
    /// queried with `getattribute()` even though they cannot be set via
    /// `attribute()`:
    ///
    ///
    /// The following member functions of TextureSystem allow you to set
    /// (and in some cases retrieve) options that control the overall
    /// behavior of the texture system:

    /// Set a named attribute (i.e., a property or option) of the
    /// TextureSystem.
    ///
    /// Example:
    ///
    ///     TextureSystem *ts;
    ///     ...
    ///     int maxfiles = 50;
    ///     ts->attribute ("max_open_files", TypeDesc::INT, make_cspan(maxfiles));
    ///
    ///     const char *path = "/my/path";
    ///     ts->attribute ("searchpath", TypeDesc::STRING, make_cspan(&path, 1));
    ///
    ///     // There are specialized versions for retrieving a single int,
    ///     // float, or string without needing types or pointers:
    ///     ts->attribute ("max_open_files", 50);
    ///     ts->attribute ("max_memory_MB", 4000.0f);
    ///     ts->attribute ("searchpath", "/my/path");
    ///
    /// Note: When passing a string, you need to pass a pointer to the
    /// `char*`, not a pointer to the first character.  (Rationale: for an
    /// `int` attribute, you pass the address of the `int`.  So for a
    /// string, which is a `char*`, you need to pass the address of the
    /// string, i.e., a `char**`).
    ///
    /// @param  name    Name of the attribute to set.
    /// @param  type    TypeDesc describing the type of the attribute.
    /// @param  value   Pointer to the value data.
    /// @returns        `true` if the name and type were recognized and the
    ///                 attribute was set, or `false` upon failure
    ///                 (including it being an unrecognized attribute or not
    ///                 of the correct type).
    ///
    /// @version 3.1
    template<typename T>
    bool attribute(string_view name, TypeDesc type, span<T> value)
    {
        OIIO_DASSERT(BaseTypeFromC<T>::value == type.basetype
                     && type.size() == value.size_bytes());
        return attribute(name, type, OIIO::as_bytes(value));
    }

    /// A version of `attribute()` that takes its value from a span of untyped
    /// bytes. The total size of `value` must match the `type` (if not, an
    /// assertion will be thrown for debug builds of OIIO, an error will be
    /// printed for release builds).
    ///
    /// @version 3.1
    bool attribute(string_view name, TypeDesc type, cspan<std::byte> value);

    /// A version of `attribute()` where the `value` is only a pointer
    /// specifying the beginning of the memory where the value should be
    /// copied from. This is "unsafe" in the sense that there is no assurance
    /// that it points to a sufficient amount of memory, so the span-based
    /// versions of `attribute()` preferred.
    bool attribute(string_view name, TypeDesc type, const void* value);

    /// Specialized `attribute()` for setting a single `int` value.
    bool attribute(string_view name, int value)
    {
        return attribute(name, TypeInt, &value);
    }
    /// Specialized `attribute()` for setting a single `float` value.
    bool attribute(string_view name, float value)
    {
        return attribute(name, TypeFloat, &value);
    }
    bool attribute(string_view name, double value)
    {
        float f = (float)value;
        return attribute(name, TypeFloat, &f);
    }
    /// Specialized `attribute()` for setting a single string value.
    bool attribute(string_view name, string_view value)
    {
        std::string valstr(value);
        const char* s = valstr.c_str();
        return attribute(name, TypeDesc::STRING, &s);
    }

    /// Get the named attribute of the texture system, store it in `*val`.
    /// All of the attributes that may be set with the `attribute() call`
    /// may also be queried with `getattribute()`.
    ///
    /// Examples:
    ///
    ///     TextureSystem *ic;
    ///     ...
    ///     int maxfiles;
    ///     ts->getattribute ("max_open_files", TypeDesc::INT, &maxfiles);
    ///
    ///     const char *path;
    ///     ts->getattribute ("searchpath", TypeDesc::STRING, &path);
    ///
    ///     // There are specialized versions for retrieving a single int,
    ///     // float, or string without needing types or pointers:
    ///     int maxfiles;
    ///     ts->getattribute ("max_open_files", maxfiles);
    ///     const char *path;
    ///     ts->getattribute ("searchpath", &path);
    ///
    /// Note: When retrieving a string, you need to pass a pointer to the
    /// `char*`, not a pointer to the first character. Also, the `char*`
    /// will end up pointing to characters owned by the ImageCache; the
    /// caller does not need to ever free the memory that contains the
    /// characters.
    ///
    /// @param  name    Name of the attribute to retrieve.
    /// @param  type    TypeDesc describing the type of the attribute.
    /// @param  value   Pointer where the attribute value should be stored.
    /// @returns        `true` if the name and type were recognized and the
    ///                 attribute was retrieved, or `false` upon failure
    ///                 (including it being an unrecognized attribute or not
    ///                 of the correct type).
    template<typename T>
    bool getattribute(string_view name, TypeDesc type, span<T> value) const
    {
        OIIO_DASSERT(BaseTypeFromC<T>::value == type.basetype
                     && type.size() == value.size_bytes());
        return getattribute(name, type, OIIO::as_writable_bytes(value));
    }

    /// A version of `getattribute()` that stores the value in a span of
    /// untyped bytes. The total size of `value` must match the `type` (if
    /// not, an assertion will be thrown for debug OIIO builds, an error will
    /// be printed for release builds).
    ///
    /// @version 3.1
    bool getattribute(string_view name, TypeDesc type,
                      span<std::byte> value) const;

    /// A version of `getattribute()` where the `value` is only a pointer
    /// specifying the beginning of the memory where the value should be
    /// copied. This is "unsafe" in the sense that there is no assurance that
    /// it points to a sufficient amount of memory, so the span-based versions
    /// of `getattribute()` preferred.
    bool getattribute(string_view name, TypeDesc type, void* value) const;

    /// Specialized `attribute()` for retrieving a single `int` value.
    bool getattribute(string_view name, int& value) const
    {
        return getattribute(name, TypeInt, &value);
    }
    /// Specialized `attribute()` for retrieving a single `float` value.
    bool getattribute(string_view name, float& value) const
    {
        return getattribute(name, TypeFloat, &value);
    }
    bool getattribute(string_view name, double& value) const
    {
        float f;
        bool ok = getattribute(name, TypeFloat, &f);
        if (ok)
            value = f;
        return ok;
    }
    /// Specialized `attribute()` for retrieving a single `string` value
    /// as a `char*`.
    bool getattribute(string_view name, char** value) const
    {
        return getattribute(name, TypeString, value);
    }
    /// Specialized `attribute()` for retrieving a single `string` value
    /// as a `std::string`.
    bool getattribute(string_view name, std::string& value) const
    {
        const char* s;
        bool ok = getattribute(name, TypeString, &s);
        if (ok)
            value = s;
        return ok;
    }

    /// If the named attribute is known, return its data type. If no such
    /// attribute exists, return `TypeUnknown`.
    ///
    /// This was added in version 2.5.
    TypeDesc getattributetype(string_view name) const;

    /// @}

    /// @{
    /// @name Opaque data for performance lookups
    ///
    /// The TextureSystem implementation needs to maintain certain
    /// per-thread state, and some methods take an opaque `Perthread`
    /// pointer to this record. There are three options for how to deal with
    /// it:
    ///
    /// 1. Don't worry about it at all: don't use the methods that want
    ///    `Perthread` pointers, or always pass `nullptr` for any
    ///    `Perthread*1 arguments, and ImageCache will do
    ///    thread-specific-pointer retrieval as necessary (though at some
    ///    small cost).
    ///
    /// 2. If your app already stores per-thread information of its own, you
    ///    may call `get_perthread_info(nullptr)` to retrieve it for that
    ///    thread, and then pass it into the functions that allow it (thus
    ///    sparing them the need and expense of retrieving the
    ///    thread-specific pointer). However, it is crucial that this
    ///    pointer not be shared between multiple threads. In this case, the
    ///    ImageCache manages the storage, which will automatically be
    ///    released when the thread terminates.
    ///
    /// 3. If your app also wants to manage the storage of the `Perthread`,
    ///    it can explicitly create one with `create_perthread_info()`, pass
    ///    it around, and eventually be responsible for destroying it with
    ///    `destroy_perthread_info()`. When managing the storage, the app
    ///    may reuse the `Perthread` for another thread after the first is
    ///    terminated, but still may not use the same `Perthread` for two
    ///    threads running concurrently.

    /// Define an opaque data type that allows us to have a pointer to
    /// certain per-thread information that the TextureSystem maintains. Any
    /// given one of these should NEVER be shared between running threads.
    class Perthread;

    /// Retrieve a Perthread, unique to the calling thread. This is a
    /// thread-specific pointer that will always return the Perthread for a
    /// thread, which will also be automatically destroyed when the thread
    /// terminates.
    ///
    /// Applications that want to manage their own Perthread pointers (with
    /// `create_thread_info` and `destroy_thread_info`) should still call
    /// this, but passing in their managed pointer. If the passed-in
    /// thread_info is not nullptr, it won't create a new one or retrieve a
    /// TSP, but it will do other necessary housekeeping on the Perthread
    /// information.
    Perthread* get_perthread_info(Perthread* thread_info = nullptr);

    /// Create a new Perthread. It is the caller's responsibility to
    /// eventually destroy it using `destroy_thread_info()`.
    Perthread* create_thread_info();

    /// Destroy a Perthread that was allocated by `create_thread_info()`.
    void destroy_thread_info(Perthread* threadinfo);

    /// Define an opaque data type that allows us to have a handle to a
    /// texture (already having its name resolved) but without exposing
    /// any internals.
    class TextureHandle;

    /// Retrieve an opaque handle for fast texture lookups.  The filename is
    /// presumed to be UTF-8 encoded. The `options`, if not null, may be used
    /// to create a separate handle for certain texture option choices
    /// (currently: the colorspace). The opaque pointer `thread_info` is
    /// thread-specific information returned by `get_perthread_info()`. Return
    /// nullptr if something has gone horribly wrong.
    TextureHandle* get_texture_handle(ustring filename,
                                      Perthread* thread_info    = nullptr,
                                      const TextureOpt* options = nullptr);
    /// Get a TextureHandle using a UTF-16 encoded wstring filename.
    TextureHandle* get_texture_handle(const std::wstring& filename,
                                      Perthread* thread_info    = nullptr,
                                      const TextureOpt* options = nullptr)
    {
        return get_texture_handle(ustring(Strutil::utf16_to_utf8(filename)),
                                  thread_info, options);
    }

    /// Return true if the texture handle (previously returned by
    /// `get_image_handle()`) is a valid texture that can be subsequently
    /// read.
    bool good(TextureHandle* texture_handle);

    /// Given a handle, return the UTF-8 encoded filename for that texture.
    ///
    /// This method was added in OpenImageIO 2.3.
    ustring filename_from_handle(TextureHandle* handle);

    /// Retrieve an id for a color transformation by name. This ID can be used
    /// as the value for TextureOpt::colortransformid. The returned value will
    /// be -1 if either color space is unknown, and 0 for a null
    /// transformation.
    int get_colortransform_id(ustring fromspace, ustring tospace) const;
    int get_colortransform_id(ustringhash fromspace, ustringhash tospace) const;
    /// @}

    /// @{
    /// @name   Texture lookups
    ///

    /// Perform a filtered 2D texture lookup on a position centered at 2D
    /// coordinates (`s`, `t`) from the texture identified by `filename`,
    /// and using relevant texture `options`.  The `nchannels` parameter
    /// determines the number of channels to retrieve (e.g., 1 for a single
    /// value, 3 for an RGB triple, etc.). The filtered results will be
    /// stored in `result[0..nchannels-1]`.
    ///
    /// We assume that this lookup will be part of an image that has pixel
    /// coordinates `x` and `y`.  By knowing how `s` and `t` change from
    /// pixel to pixel in the final image, we can properly *filter* or
    /// antialias the texture lookups.  This information is given via
    /// derivatives `dsdx` and `dtdx` that define the change in `s` and `t`
    /// per unit of `x`, and `dsdy` and `dtdy` that define the change in `s`
    /// and `t` per unit of `y`.  If it is impossible to know the
    /// derivatives, you may pass 0 for them, but in that case you will not
    /// receive an antialiased texture lookup.
    ///
    /// @param  filename
    ///             The name of the texture, as a UTF-8 encoded ustring.
    /// @param  options
    ///     Fields within `options` that are honored for 2D texture lookups
    ///     include the following:
    ///     - `int firstchannel` :
    ///             The index of the first channel to look up from the texture.
    ///     - `int subimage / ustring subimagename` :
    ///             The subimage or face within the file, specified by
    ///             either by name (if non-empty) or index. This will be
    ///             ignored if the file does not have multiple subimages or
    ///             separate per-face textures.
    ///     - `Wrap swrap, twrap` :
    ///             Specify the *wrap mode* for each direction, one of:
    ///             `WrapBlack`, `WrapClamp`, `WrapPeriodic`, `WrapMirror`,
    ///             or `WrapDefault`.
    ///     - `float swidth, twidth` :
    ///             For each direction, gives a multiplier for the derivatives.
    ///     - `float sblur, tblur` :
    ///             For each direction, specifies an additional amount of
    ///             pre-blur to apply to the texture (*after* derivatives
    ///             are taken into account), expressed as a portion of the
    ///             width of the texture.
    ///     - `float fill` :
    ///             Specifies the value that will be used for any color
    ///             channels that are requested but not found in the file.
    ///             For example, if you perform a 4-channel lookup on a
    ///             3-channel texture, the last channel will get the fill
    ///             value.  (Note: this behavior is affected by the
    ///             `"gray_to_rgb"` TextureSystem attribute.
    ///     - `const float *missingcolor` :
    ///             If not `nullptr`, specifies the color that will be
    ///             returned for missing or broken textures (rather than
    ///             being an error).
    /// @param  s/t
    ///             The 2D texture coordinates.
    /// @param  dsdx,dtdx,dsdy,dtdy
    ///             The differentials of s and t relative to canonical
    ///             directions x and y.  The choice of x and y are not
    ///             important to the implementation; it can be any imposed
    ///             2D coordinates, such as pixels in screen space, adjacent
    ///             samples in parameter space on a surface, etc. The st
    ///             derivatives determine the size and shape of the
    ///             ellipsoid over which the texture lookup is filtered.
    /// @param  nchannels
    ///             The number of channels of data to retrieve into `result`
    ///             (e.g., 1 for a single value, 3 for an RGB triple, etc.).
    /// @param  result[]
    ///             The result of the filtered texture lookup will be placed
    ///             into `result[0..nchannels-1]`.
    /// @param  dresultds/dresultdt
    ///             If non-null, these designate storage locations for the
    ///             derivatives of result, i.e., the rate of change per unit
    ///             s and t, respectively, of the filtered texture. If
    ///             supplied, they must allow for `nchannels` of storage.
    /// @returns
    ///             `true` upon success, or `false` if the file was not
    ///             found or could not be opened by any available ImageIO
    ///             plugin.
    ///
    bool texture(ustring filename, TextureOpt& options, float s, float t,
                 float dsdx, float dtdx, float dsdy, float dtdy, int nchannels,
                 float* result, float* dresultds = nullptr,
                 float* dresultdt = nullptr);

    /// Slightly faster version of texture() lookup if the app already has a
    /// texture handle and per-thread info.
    bool texture(TextureHandle* texture_handle, Perthread* thread_info,
                 TextureOpt& options, float s, float t, float dsdx, float dtdx,
                 float dsdy, float dtdy, int nchannels, float* result,
                 float* dresultds = nullptr, float* dresultdt = nullptr);


    /// Perform a filtered 3D volumetric texture lookup on a position
    /// centered at 3D position `P` (with given differentials) from the
    /// texture identified by `filename`, and using relevant texture
    /// `options`.  The filtered results will be stored in
    /// `result[0..nchannels-1]`.
    ///
    /// The `P` coordinate and `dPdx`, `dPdy`, and `dPdz` derivatives are
    /// assumed to be in some kind of common global coordinate system
    /// (usually "world" space) and will be automatically transformed into
    /// volume local coordinates, if such a transformation is specified in
    /// the volume file itself.
    ///
    /// @param  filename
    ///             The name of the texture, as a UTF-8 encoded ustring.
    /// @param  options
    ///     Fields within `options` that are honored for 3D texture lookups
    ///     include the following:
    ///     - `int firstchannel` :
    ///             The index of the first channel to look up from the texture.
    ///     - `int subimage / ustring subimagename` :
    ///             The subimage or field within the volume, specified by
    ///             either by name (if non-empty) or index. This will be
    ///             ignored if the file does not have multiple subimages or
    ///             separate per-face textures.
    ///     - `Wrap swrap, twrap, rwrap` :
    ///             Specify the *wrap mode* for each direction, one of:
    ///             `WrapBlack`, `WrapClamp`, `WrapPeriodic`, `WrapMirror`,
    ///             or `WrapDefault`.
    ///     - `float swidth, twidth, rwidth` :
    ///             For each direction, gives a multiplier for the derivatives.
    ///     - `float sblur, tblur, rblur` :
    ///             For each direction, specifies an additional amount of
    ///             pre-blur to apply to the texture (*after* derivatives
    ///             are taken into account), expressed as a portion of the
    ///             width of the texture.
    ///     - `float fill` :
    ///             Specifies the value that will be used for any color
    ///             channels that are requested but not found in the file.
    ///             For example, if you perform a 4-channel lookup on a
    ///             3-channel texture, the last channel will get the fill
    ///             value.  (Note: this behavior is affected by the
    ///             `"gray_to_rgb"` TextureSystem attribute.
    ///     - `const float *missingcolor` :
    ///             If not `nullptr`, specifies the color that will be
    ///             returned for missing or broken textures (rather than
    ///             being an error).
    ///     - `float time` :
    ///             A time value to use if the volume texture specifies a
    ///             time-varying local transformation (default: 0).
    /// @param  P
    ///             The 2D texture coordinates.
    /// @param  dPdx/dPdy/dPdz
    ///             The differentials of `P`. We assume that this lookup
    ///             will be part of an image that has pixel coordinates `x`
    ///             and `y` and depth `z`. By knowing how `P` changes from
    ///             pixel to pixel in the final image, and as we step in *z*
    ///             depth, we can properly *filter* or antialias the texture
    ///             lookups.  This information is given via derivatives
    ///             `dPdx`, `dPdy`, and `dPdz` that define the changes in
    ///             `P` per unit of `x`, `y`, and `z`, respectively.  If it
    ///             is impossible to know the derivatives, you may pass 0
    ///             for them, but in that case you will not receive an
    ///             antialiased texture lookup.
    /// @param  nchannels
    ///             The number of channels of data to retrieve into `result`
    ///             (e.g., 1 for a single value, 3 for an RGB triple, etc.).
    /// @param  result[]
    ///             The result of the filtered texture lookup will be placed
    ///             into `result[0..nchannels-1]`.
    /// @param  dresultds/dresultdt/dresultdr
    ///             If non-null, these designate storage locations for the
    ///             derivatives of result, i.e., the rate of change per unit
    ///             s, t, and r, respectively, of the filtered texture. If
    ///             supplied, they must allow for `nchannels` of storage.
    /// @returns
    ///             `true` upon success, or `false` if the file was not
    ///             found or could not be opened by any available ImageIO
    ///             plugin.
    ///
    bool texture3d(ustring filename, TextureOpt& options, V3fParam P,
                   V3fParam dPdx, V3fParam dPdy, V3fParam dPdz, int nchannels,
                   float* result, float* dresultds = nullptr,
                   float* dresultdt = nullptr, float* dresultdr = nullptr);

    /// Slightly faster version of texture3d() lookup if the app already has
    /// a texture handle and per-thread info.
    bool texture3d(TextureHandle* texture_handle, Perthread* thread_info,
                   TextureOpt& options, V3fParam P, V3fParam dPdx,
                   V3fParam dPdy, V3fParam dPdz, int nchannels, float* result,
                   float* dresultds = nullptr, float* dresultdt = nullptr,
                   float* dresultdr = nullptr);


    /// Perform a filtered directional environment map lookup in the
    /// direction of vector `R`, from the texture identified by `filename`,
    /// and using relevant texture `options`.  The filtered results will be
    /// stored in `result[]`.
    ///
    /// @param  filename
    ///             The name of the texture, as a UTF-8 encode ustring.
    /// @param  options
    ///     Fields within `options` that are honored for environment lookups
    ///     include the following:
    ///     - `int firstchannel` :
    ///             The index of the first channel to look up from the texture.
    ///     - `int subimage / ustring subimagename` :
    ///             The subimage or face within the file, specified by
    ///             either by name (if non-empty) or index. This will be
    ///             ignored if the file does not have multiple subimages or
    ///             separate per-face textures.
    ///     - `float swidth, twidth` :
    ///             For each direction, gives a multiplier for the
    ///             derivatives.
    ///     - `float sblur, tblur` :
    ///             For each direction, specifies an additional amount of
    ///             pre-blur to apply to the texture (*after* derivatives
    ///             are taken into account), expressed as a portion of the
    ///             width of the texture.
    ///     - `float fill` :
    ///             Specifies the value that will be used for any color
    ///             channels that are requested but not found in the file.
    ///             For example, if you perform a 4-channel lookup on a
    ///             3-channel texture, the last channel will get the fill
    ///             value.  (Note: this behavior is affected by the
    ///             `"gray_to_rgb"` TextureSystem attribute.
    ///     - `const float *missingcolor` :
    ///             If not `nullptr`, specifies the color that will be
    ///             returned for missing or broken textures (rather than
    ///             being an error).
    /// @param  R
    ///             The direction vector to look up.
    /// @param  dRdx/dRdy
    ///             The differentials of `R` with respect to image
    ///             coordinates x and y.
    /// @param  nchannels
    ///             The number of channels of data to retrieve into `result`
    ///             (e.g., 1 for a single value, 3 for an RGB triple, etc.).
    /// @param  result[]
    ///             The result of the filtered texture lookup will be placed
    ///             into `result[0..nchannels-1]`.
    /// @param  dresultds/dresultdt
    ///             If non-null, these designate storage locations for the
    ///             derivatives of result, i.e., the rate of change per unit
    ///             s and t, respectively, of the filtered texture. If
    ///             supplied, they must allow for `nchannels` of storage.
    /// @returns
    ///             `true` upon success, or `false` if the file was not
    ///             found or could not be opened by any available ImageIO
    ///             plugin.
    bool environment(ustring filename, TextureOpt& options, V3fParam R,
                     V3fParam dRdx, V3fParam dRdy, int nchannels, float* result,
                     float* dresultds = nullptr, float* dresultdt = nullptr);

    /// Slightly faster version of environment() if the app already has a
    /// texture handle and per-thread info.
    bool environment(TextureHandle* texture_handle, Perthread* thread_info,
                     TextureOpt& options, V3fParam R, V3fParam dRdx,
                     V3fParam dRdy, int nchannels, float* result,
                     float* dresultds = nullptr, float* dresultdt = nullptr);

    /// @}

    /// @{
    /// @name   Batched texture lookups
    ///

    /// Perform filtered 2D texture lookups on a batch of positions from the
    /// same texture, all at once.  The parameters `s`, `t`, `dsdx`, `dtdx`,
    /// and `dsdy`, `dtdy` are each a pointer to `[BatchWidth]` values.  The
    /// `mask` determines which of those array elements to actually compute.
    ///
    /// The float* results act like `float[nchannels][BatchWidth]`, so that
    /// effectively `result[0..BatchWidth-1]` are the "red" result for each
    /// lane, `result[BatchWidth..2*BatchWidth-1]` are the "green" results,
    /// etc. The `dresultds` and `dresultdt` should either both be provided,
    /// or else both be nullptr (meaning no derivative results are
    /// required).
    ///
    /// @param  filename
    ///             The name of the texture, as a UTF-8 encode ustring.
    /// @param  options
    ///             A TextureOptBatch containing texture lookup options.
    ///             This is conceptually the same as a TextureOpt, but the
    ///             following fields are arrays of `[BatchWidth]` elements:
    ///             sblur, tblur, swidth, twidth. The other fields are, as
    ///             with TextureOpt, ordinary scalar values.
    /// @param  mask
    ///             A bit-field designating which "lanes" should be
    ///             computed: if `mask & (1<<i)` is nonzero, then results
    ///             should be computed and stored for `result[...][i]`.
    /// @param  s/t
    ///             Pointers to the 2D texture coordinates, each as a
    ///             `float[BatchWidth]`.
    /// @param  dsdx/dtdx/dsdy/dtdy
    ///             The differentials of s and t relative to canonical
    ///             directions x and y, each as a `float[BatchWidth]`.
    /// @param  nchannels
    ///             The number of channels of data to retrieve into `result`
    ///             (e.g., 1 for a single value, 3 for an RGB triple, etc.).
    /// @param  result[]
    ///             The result of the filtered texture lookup will be placed
    ///             here, as `float [nchannels][BatchWidth]`. (Note the
    ///             "SOA" data layout.)
    /// @param  dresultds/dresultdt
    ///             If non-null, these designate storage locations for the
    ///             derivatives of result, and like `result` are in SOA
    ///             layout: `float [nchannels][BatchWidth]`
    /// @returns
    ///             `true` upon success, or `false` if the file was not
    ///             found or could not be opened by any available ImageIO
    ///             plugin.
    ///
    bool texture(ustring filename, TextureOptBatch& options, Tex::RunMask mask,
                 const float* s, const float* t, const float* dsdx,
                 const float* dtdx, const float* dsdy, const float* dtdy,
                 int nchannels, float* result, float* dresultds = nullptr,
                 float* dresultdt = nullptr);
    /// Slightly faster version of texture() lookup if the app already has a
    /// texture handle and per-thread info.
    bool texture(TextureHandle* texture_handle, Perthread* thread_info,
                 TextureOptBatch& options, Tex::RunMask mask, const float* s,
                 const float* t, const float* dsdx, const float* dtdx,
                 const float* dsdy, const float* dtdy, int nchannels,
                 float* result, float* dresultds = nullptr,
                 float* dresultdt = nullptr);

    /// Perform filtered 3D volumetric texture lookups on a batch of
    /// positions from the same texture, all at once. The "point-like"
    /// parameters `P`, `dPdx`, `dPdy`, and `dPdz` are each a pointers to
    /// arrays of `float value[3][BatchWidth]` (or alternately like
    /// `Imath::Vec3<FloatWide>`). That is, each one points to all the *x*
    /// values for the batch, immediately followed by all the *y* values,
    /// followed by the *z* values. The `mask` determines which of those
    /// array elements to actually compute.
    ///
    /// The various results arrays are also arranged as arrays that behave
    /// as if they were declared `float result[channels][BatchWidth]`, where
    /// all the batch values for channel 0 are adjacent, followed by all the
    /// batch values for channel 1, etc.
    ///
    /// @param  filename
    ///             The name of the texture, as a UTF-8 encode ustring.
    /// @param  options
    ///             A TextureOptBatch containing texture lookup options.
    ///             This is conceptually the same as a TextureOpt, but the
    ///             following fields are arrays of `[BatchWidth]` elements:
    ///             sblur, tblur, swidth, twidth. The other fields are, as
    ///             with TextureOpt, ordinary scalar values.
    /// @param  mask
    ///             A bit-field designating which "lanes" should be
    ///             computed: if `mask & (1<<i)` is nonzero, then results
    ///             should be computed and stored for `result[...][i]`.
    /// @param  P
    ///             Pointers to the 3D texture coordinates, each as a
    ///             `float[3][BatchWidth]`.
    /// @param  dPdx/dPdy/dPdz
    ///             The differentials of P relative to canonical directions
    ///             x, y, and z, each as a `float[3][BatchWidth]`.
    /// @param  nchannels
    ///             The number of channels of data to retrieve into `result`
    ///             (e.g., 1 for a single value, 3 for an RGB triple, etc.).
    /// @param  result[]
    ///             The result of the filtered texture lookup will be placed
    ///             here, as `float [nchannels][BatchWidth]`. (Note the
    ///             "SOA" data layout.)
    /// @param  dresultds/dresultdt/dresultdr
    ///             If non-null, these designate storage locations for the
    ///             derivatives of result, and like `result` are in SOA
    ///             layout: `float [nchannels][BatchWidth]`
    /// @returns
    ///             `true` upon success, or `false` if the file was not
    ///             found or could not be opened by any available ImageIO
    ///             plugin.
    ///
    bool texture3d(ustring filename, TextureOptBatch& options,
                   Tex::RunMask mask, const float* P, const float* dPdx,
                   const float* dPdy, const float* dPdz, int nchannels,
                   float* result, float* dresultds = nullptr,
                   float* dresultdt = nullptr, float* dresultdr = nullptr);
    /// Slightly faster version of texture3d() lookup if the app already
    /// has a texture handle and per-thread info.
    bool texture3d(TextureHandle* texture_handle, Perthread* thread_info,
                   TextureOptBatch& options, Tex::RunMask mask, const float* P,
                   const float* dPdx, const float* dPdy, const float* dPdz,
                   int nchannels, float* result, float* dresultds = nullptr,
                   float* dresultdt = nullptr, float* dresultdr = nullptr);

    /// Perform filtered directional environment map lookups on a batch of
    /// directions from the same texture, all at once. The "point-like"
    /// parameters `R`, `dRdx`, and `dRdy` are each a pointers to arrays of
    /// `float value[3][BatchWidth]` (or alternately like
    /// `Imath::Vec3<FloatWide>`). That is, each one points to all the *x*
    /// values for the batch, immediately followed by all the *y* values,
    /// followed by the *z* values. The `mask` determines which of those
    /// array elements to actually compute.
    ///
    /// The various results arrays are also arranged as arrays that behave
    /// as if they were declared `float result[channels][BatchWidth]`, where
    /// all the batch values for channel 0 are adjacent, followed by all the
    /// batch values for channel 1, etc.
    ///
    /// @param  filename
    ///             The name of the texture, as a UTF-8 encode ustring.
    /// @param  options
    ///             A TextureOptBatch containing texture lookup options.
    ///             This is conceptually the same as a TextureOpt, but the
    ///             following fields are arrays of `[BatchWidth]` elements:
    ///             sblur, tblur, swidth, twidth. The other fields are, as
    ///             with TextureOpt, ordinary scalar values.
    /// @param  mask
    ///             A bit-field designating which "lanes" should be
    ///             computed: if `mask & (1<<i)` is nonzero, then results
    ///             should be computed and stored for `result[...][i]`.
    /// @param  R
    ///             Pointers to the 3D texture coordinates, each as a
    ///             `float[3][BatchWidth]`.
    /// @param  dRdx/dRdy
    ///             The differentials of R relative to canonical directions
    ///             x and y, each as a `float[3][BatchWidth]`.
    /// @param  nchannels
    ///             The number of channels of data to retrieve into `result`
    ///             (e.g., 1 for a single value, 3 for an RGB triple, etc.).
    /// @param  result[]
    ///             The result of the filtered texture lookup will be placed
    ///             here, as `float [nchannels][BatchWidth]`. (Note the
    ///             "SOA" data layout.)
    /// @param  dresultds/dresultdt
    ///             If non-null, these designate storage locations for the
    ///             derivatives of result, and like `result` are in SOA
    ///             layout: `float [nchannels][BatchWidth]`
    /// @returns
    ///             `true` upon success, or `false` if the file was not
    ///             found or could not be opened by any available ImageIO
    ///             plugin.
    ///
    bool environment(ustring filename, TextureOptBatch& options,
                     Tex::RunMask mask, const float* R, const float* dRdx,
                     const float* dRdy, int nchannels, float* result,
                     float* dresultds = nullptr, float* dresultdt = nullptr);
    /// Slightly faster version of environment() if the app already has a
    /// texture handle and per-thread info.
    bool environment(TextureHandle* texture_handle, Perthread* thread_info,
                     TextureOptBatch& options, Tex::RunMask mask,
                     const float* R, const float* dRdx, const float* dRdy,
                     int nchannels, float* result, float* dresultds = nullptr,
                     float* dresultdt = nullptr);

    /// @}


    /// @{
    /// @name   Texture metadata and raw texels
    ///

    /// Given possibly-relative 'filename' (UTF-8 encoded), resolve it using
    /// the search path rules and return the full resolved filename.
    std::string resolve_filename(const std::string& filename) const;

    /// Get information or metadata about the named texture and store it in
    /// `*data`.
    ///
    /// Data names may include any of the following:
    ///
    ///   - `exists` (int):
    ///         Stores the value 1 if the file exists and is an image format
    ///         that OpenImageIO can read, or 0 if the file does not exist,
    ///         or could not be properly read as a texture. Note that unlike
    ///         all other queries, this query will "succeed" (return `true`)
    ///         even if the file does not exist.
    ///
    ///   - `udim` (int) :
    ///         Stores the value 1 if the file is a "virtual UDIM" or
    ///         texture atlas file (as described in Section
    ///         :ref:`sec-texturesys-udim`) or 0 otherwise.
    ///
    ///   - `subimages` (int) :
    ///         The number of subimages/faces in the file, as an integer.
    ///
    ///   - `resolution` (int[2] or int[3]):
    ///         The resolution of the texture file, if an array of 2
    ///         integers (described as `TypeDesc(INT,2)`), or the 3D
    ///         resolution of the texture file, which is an array of 3
    ///         integers (described as `TypeDesc(INT,3)`)  The third value
    ///         will be 1 unless it's a volumetric (3D) image.
    ///
    ///   - `miplevels` (int) :
    ///         The number of MIPmap levels for the specified
    ///         subimage (an integer).
    ///
    ///   - `texturetype` (string) :
    ///         A string describing the type of texture of the given file,
    ///         which describes how the texture may be used (also which
    ///         texture API call is probably the right one for it). This
    ///         currently may return one of: "unknown", "Plain Texture",
    ///         "Volume Texture", "Shadow", or "Environment".
    ///
    ///   - `textureformat` (string) :
    ///         A string describing the format of the given file, which
    ///         describes the kind of texture stored in the file. This
    ///         currently may return one of: "unknown", "Plain Texture",
    ///         "Volume Texture", "Shadow", "CubeFace Shadow", "Volume
    ///         Shadow", "LatLong Environment", or "CubeFace Environment".
    ///         Note that there are several kinds of shadows and environment
    ///         maps, all accessible through the same API calls.
    ///
    ///   - `channels` (int) :
    ///         The number of color channels in the file.
    ///
    ///   - `format` (int) :
    ///         The native data format of the pixels in the file (an
    ///         integer, giving the `TypeDesc::BASETYPE` of the data). Note
    ///         that this is not necessarily the same as the data format
    ///         stored in the image cache.
    ///
    ///   - `cachedformat` (int) :
    ///         The native data format of the pixels as stored in the image
    ///         cache (an integer, giving the `TypeDesc::BASETYPE` of the
    ///         data).  Note that this is not necessarily the same as the
    ///         native data format of the file.
    ///
    ///   - `datawindow` (int[4] or int[6]):
    ///         Returns the pixel data window of the image, which is either
    ///         an array of 4 integers (returning xmin, ymin, xmax, ymax) or
    ///         an array of 6 integers (returning xmin, ymin, zmin, xmax,
    ///         ymax, zmax). The z values may be useful for 3D/volumetric
    ///         images; for 2D images they will be 0).
    ///
    ///   - `displaywindow` (matrix) :
    ///         Returns the display (a.k.a. full) window of the image, which
    ///         is either an array of 4 integers (returning xmin, ymin,
    ///         xmax, ymax) or an array of 6 integers (returning xmin, ymin,
    ///         zmin, xmax, ymax, zmax). The z values may be useful for
    ///         3D/volumetric images; for 2D images they will be 0).
    ///
    ///   - `worldtocamera` (matrix) :
    ///         The viewing matrix, which is a 4x4 matrix (an `Imath::M44f`,
    ///         described as `TypeMatrix44`) giving the world-to-camera 3D
    ///         transformation matrix that was used when  the image was
    ///         created. Generally, only rendered images will have this.
    ///
    ///   - `worldtoscreen` (matrix) :
    ///         The projection matrix, which is a 4x4 matrix (an
    ///         `Imath::M44f`, described as `TypeMatrix44`) giving the
    ///         matrix that projected points from world space into a 2D
    ///         screen coordinate system where *x* and *y* range from -1 to
    ///         +1.  Generally, only rendered images will have this.
    ///
    ///   - `worldtoNDC` (matrix) :
    ///         The projection matrix, which is a 4x4 matrix (an
    ///         `Imath::M44f`, described as `TypeMatrix44`) giving the
    ///         matrix that projected points from world space into a 2D
    ///         screen coordinate system where *x* and *y* range from 0 to
    ///         +1.  Generally, only rendered images will have this.
    ///
    ///   - `averagecolor` (float[nchannels]) :
    ///         If available in the metadata (generally only for files that
    ///         have been processed by `maketx`), this will return the
    ///         average color of the texture (into an array of floats).
    ///
    ///   - `averagealpha` (float) :
    ///         If available in the metadata (generally only for files that
    ///         have been processed by `maketx`), this will return the
    ///         average alpha value of the texture (into a float).
    ///
    ///   - `constantcolor` (float[nchannels]) :
    ///         If the metadata (generally only for files that have been
    ///         processed by `maketx`) indicates that the texture has the
    ///         same values for all pixels in the texture, this will
    ///         retrieve the constant color of the texture (into an array of
    ///         floats). A non-constant image (or one that does not have the
    ///         special metadata tag identifying it as a constant texture)
    ///         will fail this query (return false).
    ///
    ///   - `constantalpha` (float) :
    ///         If the metadata indicates that the texture has the same
    ///         values for all pixels in the texture, this will retrieve the
    ///         constant alpha value of the texture. A non-constant image
    ///         (or one that does not have the special metadata tag
    ///         identifying it as a constant texture) will fail this query
    ///         (return false).
    ///
    ///   - `stat:tilesread` (int64) :
    ///         Number of tiles read from this file.
    ///
    ///   - `stat:bytesread` (int64) :
    ///         Number of bytes of uncompressed pixel data read
    ///
    ///   - `stat:redundant_tiles` (int64) :
    ///         Number of times a tile was read, where the same tile had
    ///         been rad before.
    ///
    ///   - `stat:redundant_bytesread` (int64) :
    ///         Number of bytes (of uncompressed pixel data) in tiles that
    ///         were read redundantly.
    ///
    ///   - `stat:redundant_bytesread` (int) :
    ///         Number of tiles read from this file.
    ///
    ///   - `stat:timesopened` (int) :
    ///         Number of times this file was opened.
    ///
    ///   - `stat:iotime` (float) :
    ///         Time (in seconds) spent on all I/O for this file.
    ///
    ///   - `stat:mipsused` (int) :
    ///         Stores 1 if any MIP levels beyond the highest resolution
    ///         were accessed, otherwise 0.
    ///
    ///   - `stat:is_duplicate` (int) :
    ///         Stores 1 if this file was a duplicate of another image,
    ///         otherwise 0.
    ///
    ///   - *Anything else* :
    ///         For all other data names, the the metadata of the image file
    ///         will be searched for an item that matches both the name and
    ///         data type.
    ///
    ///
    /// @param  filename
    ///             The name of the texture, as a UTF-8 encode ustring.
    /// @param  subimage
    ///             The subimage to query. (The metadata retrieved is for
    ///             the highest-resolution MIP level of that subimage.)
    /// @param  dataname
    ///             The name of the metadata to retrieve.
    /// @param  datatype
    ///             TypeDesc describing the data type.
    /// @param  data
    ///             Pointer to the caller-owned memory where the values
    ///             should be stored. It is the caller's responsibility to
    ///             ensure that `data` points to a large enough storage area
    ///             to accommodate the `datatype` requested.
    ///
    /// @returns
    ///             `true` if `get_textureinfo()` is able to find the
    ///             requested `dataname` for the texture and it matched the
    ///             requested `datatype`.  If the requested data was not
    ///             found or was not of the right data type, return `false`.
    ///             Except for the `"exists"` query, a file that does not
    ///             exist or could not be read properly as an image also
    ///             constitutes a query failure that will return `false`.
    bool get_texture_info(ustring filename, int subimage, ustring dataname,
                          TypeDesc datatype, void* data);

    /// A more efficient variety of `get_texture_info()` for cases where you
    /// can use a `TextureHandle*` to specify the image and optionally have
    /// a `Perthread*` for the calling thread.
    bool get_texture_info(TextureHandle* texture_handle, Perthread* thread_info,
                          int subimage, ustring dataname, TypeDesc datatype,
                          void* data);

    /// Copy the ImageSpec associated with the named texture (the first
    /// subimage by default, or as set by `subimage`).
    ///
    /// @param  filename
    ///             The name of the texture, as a UTF-8 encode ustring.
    /// @param  subimage
    ///             The subimage to query. (The spec retrieved is for the
    ///             highest-resolution MIP level of that subimage.)
    /// @param  spec
    ///             ImageSpec into which will be copied the spec for the
    ///             requested image.
    /// @returns
    ///             `true` upon success, `false` upon failure failure (such
    ///             as being unable to find, open, or read the file, or if
    ///             it does not contain the designated subimage or MIP
    ///             level).
    bool get_imagespec(ustring filename, ImageSpec& spec, int subimage = 0);
    /// A more efficient variety of `get_imagespec()` for cases where you
    /// can use a `TextureHandle*` to specify the image and optionally have
    /// a `Perthread*` for the calling thread.
    bool get_imagespec(TextureHandle* texture_handle, Perthread* thread_info,
                       ImageSpec& spec, int subimage = 0);

    /// DEPRECATED(3.0) old API. Note that the spec and subimage parameters
    /// are inverted. We recommend switching to the new API.
    bool get_imagespec(ustring filename, int subimage, ImageSpec& spec)
    {
        return get_imagespec(filename, spec, subimage);
    }
    bool get_imagespec(TextureHandle* texture_handle, Perthread* thread_info,
                       int subimage, ImageSpec& spec)
    {
        return get_imagespec(texture_handle, thread_info, spec, subimage);
    }

    /// Return a pointer to an ImageSpec associated with the named texture
    /// if the file is found and is an image format that can be read,
    /// otherwise return `nullptr`.
    ///
    /// This method is much more efficient than `get_imagespec()`, since it
    /// just returns a pointer to the spec held internally by the
    /// TextureSystem (rather than copying the spec to the user's memory).
    /// However, the caller must beware that the pointer is only valid as
    /// long as nobody (even other threads) calls `invalidate()` on the
    /// file, or `invalidate_all()`, or destroys the TextureSystem and its
    /// underlying ImageCache.
    ///
    /// @param  filename
    ///             The name of the texture, as a UTF-8 encode ustring.
    /// @param  subimage
    ///             The subimage to query.  (The spec retrieved is for the
    ///             highest-resolution MIP level of that subimage.)
    /// @returns
    ///             A pointer to the spec, if the image is found and able to
    ///             be opened and read by an available image format plugin,
    ///             and the designated subimage exists.
    const ImageSpec* imagespec(ustring filename, int subimage = 0);
    /// A more efficient variety of `imagespec()` for cases where you can
    /// use a `TextureHandle*` to specify the image and optionally have a
    /// `Perthread*` for the calling thread.
    const ImageSpec* imagespec(TextureHandle* texture_handle,
                               Perthread* thread_info = nullptr,
                               int subimage           = 0);

    /// For a texture specified by name, retrieve the rectangle of raw
    /// unfiltered texels from the subimage specified in `options` and at
    /// the designated `miplevel`, storing the pixel values beginning at the
    /// address specified by `result`.  The pixel values will be converted
    /// to the data type specified by `format`. The rectangular region to be
    /// retrieved includes `begin` but does not include `end` (much like STL
    /// begin/end usage). Requested pixels that are not part of the valid
    /// pixel data region of the image file will be filled with zero values.
    /// Channels requested but not present in the file will get the
    /// `options.fill` value.
    ///
    /// @param  filename
    ///             The name of the texture, as a UTF-8 encode ustring.
    /// @param  options
    ///             A TextureOpt describing access options, including wrap
    ///             modes, fill value, and subimage, that will be used when
    ///             retrieving pixels.
    /// @param  miplevel
    ///             The MIP level to retrieve pixels from (0 is the highest
    ///             resolution level).
    /// @param  roi
    ///             The range of pixels and channels to retrieve. The pixels
    ///             retrieved include the begin values but not the end values
    ///             (much like STL begin/end usage).
    /// @param  format
    ///             TypeDesc describing the data type of the values you want
    ///             to retrieve into `result`. The pixel values will be
    ///             converted to this type regardless of how they were
    ///             stored in the file or in the cache.
    /// @param  result
    ///             An `image_span` describing the memory layout where the
    ///             pixel values should be  stored, including bounds and
    ///             strides for each dimension.
    /// @returns
    ///             `true` for success, `false` for failure.
    ///
    /// Added in OIIO 3.1, this is the "safe" preferred alternative to
    /// the version of read_scanlines that takes raw pointers.
    ///
    bool get_texels(ustring filename, TextureOpt& options, int miplevel,
                    const ROI& roi, TypeDesc format,
                    const image_span<std::byte>& result);
    /// A more efficient variety of `get_texels()` for cases where you can
    /// use a `TextureHandle*` to specify the image and optionally have a
    /// `Perthread*` for the calling thread.
    ///
    /// Added in OIIO 3.1, this is the "safe" preferred alternative to
    /// the version of read_scanlines that takes raw pointers.
    bool get_texels(TextureHandle* texture_handle, Perthread* thread_info,
                    TextureOpt& options, int miplevel, const ROI& roi,
                    TypeDesc format, const image_span<std::byte>& result);

    /// For a texture specified by name, retrieve the rectangle of raw
    /// unfiltered texels from the subimage specified in `options` and at
    /// the designated `miplevel`, storing the pixel values beginning at the
    /// address specified by `result`.  The pixel values will be converted
    /// to the data type specified by `format`. The rectangular region to be
    /// retrieved includes `begin` but does not include `end` (much like STL
    /// begin/end usage). Requested pixels that are not part of the valid
    /// pixel data region of the image file will be filled with zero values.
    /// Channels requested but not present in the file will get the
    /// `options.fill` value.
    ///
    /// These pointer-based versions are considered "soft-deprecated" in
    /// OpenImageIO 3.1, will be marked/warned as deprecated in 3.2, and will
    /// be removed in 4.0.
    ///
    /// @param  filename
    ///             The name of the texture, as a UTF-8 encode ustring.
    /// @param  options
    ///             A TextureOpt describing access options, including wrap
    ///             modes, fill value, and subimage, that will be used when
    ///             retrieving pixels.
    /// @param  miplevel
    ///             The MIP level to retrieve pixels from (0 is the highest
    ///             resolution level).
    /// @param  xbegin/xend/ybegin/yend/zbegin/zend
    ///             The range of pixels to retrieve. The pixels retrieved
    ///             include the begin value but not the end value (much like
    ///             STL begin/end usage).
    /// @param  chbegin/chend
    ///             Channel range to retrieve. To retrieve all channels, use
    ///             `chbegin = 0`, `chend = nchannels`.
    /// @param  format
    ///             TypeDesc describing the data type of the values you want
    ///             to retrieve into `result`. The pixel values will be
    ///             converted to this type regardless of how they were
    ///             stored in the file or in the cache.
    /// @param  result
    ///             Pointer to the memory where the pixel values should be
    ///             stored.  It is up to the caller to ensure that `result`
    ///             points to an area of memory big enough to accommodate
    ///             the requested rectangle (taking into consideration its
    ///             dimensions, number of channels, and data format).
    ///
    /// @returns
    ///             `true` for success, `false` for failure.
    bool get_texels(ustring filename, TextureOpt& options, int miplevel,
                    int xbegin, int xend, int ybegin, int yend, int zbegin,
                    int zend, int chbegin, int chend, TypeDesc format,
                    void* result);
    /// A more efficient variety of `get_texels()` for cases where you can
    /// use a `TextureHandle*` to specify the image and optionally have a
    /// `Perthread*` for the calling thread.
    bool get_texels(TextureHandle* texture_handle, Perthread* thread_info,
                    TextureOpt& options, int miplevel, int xbegin, int xend,
                    int ybegin, int yend, int zbegin, int zend, int chbegin,
                    int chend, TypeDesc format, void* result);

    /// @}

    /// @{
    /// @name Methods for UDIM patterns
    ///

    /// Is the UTF-8 encoded filename a UDIM pattern?
    ///
    /// This method was added in OpenImageIO 2.3.
    bool is_udim(ustring filename);

    /// Does the handle refer to a file that's a UDIM pattern?
    ///
    /// This method was added in OpenImageIO 2.3.
    bool is_udim(TextureHandle* udimfile);

    /// For a UDIM filename pattern (UTF-8 encoded) and texture coordinates,
    /// return the TextureHandle pointer for the concrete tile file it refers
    /// to, or nullptr if there is no corresponding tile (udim sets are
    /// allowed to be sparse).
    ///
    /// This method was added in OpenImageIO 2.3.
    TextureHandle* resolve_udim(ustring udimpattern, float s, float t);

    /// A more efficient variety of `resolve_udim()` for cases where you
    /// have the `TextureHandle*` that corresponds to the "virtual" UDIM
    /// file and optionally have a `Perthread*` for the calling thread.
    ///
    /// This method was added in OpenImageIO 2.3.
    TextureHandle* resolve_udim(TextureHandle* udimfile, Perthread* thread_info,
                                float s, float t);

    /// Produce a full inventory of the set of concrete files comprising the
    /// UDIM set specified by UTF-8 encoded `udimpattern`.  The apparent
    /// number of texture atlas tiles in the u and v directions will be
    /// written to `nutiles` and `nvtiles`, respectively. The vector
    /// `filenames` will be sized to `ntiles * nvtiles` and filled with the
    /// the names of the concrete files comprising the atlas, with an empty
    /// ustring corresponding to any unpopulated tiles (the UDIM set is
    /// allowed to be sparse). The filename list is indexed as
    /// `utile + vtile * nvtiles`.
    ///
    /// This method was added in OpenImageIO 2.3.
    void inventory_udim(ustring udimpattern, std::vector<ustring>& filenames,
                        int& nutiles, int& nvtiles);

    /// A more efficient variety of `inventory_udim()` for cases where you
    /// have the `TextureHandle*` that corresponds to the "virtual" UDIM
    /// file and optionally have a `Perthread*` for the calling thread.
    ///
    /// This method was added in OpenImageIO 2.3.
    void inventory_udim(TextureHandle* udimfile, Perthread* thread_info,
                        std::vector<ustring>& filenames, int& nutiles,
                        int& nvtiles);
    /// @}

    /// @{
    /// @name Controlling the cache
    ///

    /// Invalidate any cached information about the named file (UTF-8
    /// encoded), including loaded texture tiles from that texture, and close
    /// any open file handle associated with the file. This calls
    /// `ImageCache::invalidate(filename,force)` on the underlying ImageCache.
    void invalidate(ustring filename, bool force = true);

    /// Invalidate all cached data for all textures.  This calls
    /// `ImageCache::invalidate_all(force)` on the underlying ImageCache.
    void invalidate_all(bool force = false);

    /// Close any open file handles associated with a UTF-8 encoded filename,
    /// but do not invalidate any image spec information or pixels associated
    /// with the files.  A client might do this in order to release OS file
    /// handle resources, or to make it safe for other processes to modify
    /// textures on disk.  This calls `ImageCache::close(force)` on the
    /// underlying ImageCache.
    void close(ustring filename);

    /// `close()` all files known to the cache.
    void close_all();

    /// @}

    /// @{
    /// @name Errors and statistics

    /// Is there a pending error message waiting to be retrieved?
    bool has_error() const;

    /// Return the text of all pending error messages issued against this
    /// TextureSystem, and clear the pending error message unless `clear` is
    /// false. If no error message is pending, it will return an empty
    /// string.
    std::string geterror(bool clear = true) const;

    /// Returns a big string containing useful statistics about the
    /// TextureSystem operations, suitable for saving to a file or
    /// outputting to the terminal. The `level` indicates the amount of
    /// detail in the statistics, with higher numbers (up to a maximum of 5)
    /// yielding more and more esoteric information.  If `icstats` is true,
    /// the returned string will also contain all the statistics of the
    /// underlying ImageCache, but if false will only contain
    /// texture-specific statistics.
    std::string getstats(int level = 1, bool icstats = true) const;

    /// Reset most statistics to be as they were with a fresh TextureSystem.
    /// Caveat emptor: this does not flush the cache itself, so the resulting
    /// statistics from the next set of texture requests will not match the
    /// number of tile reads, etc., that would have resulted from a new
    /// TextureSystem.
    void reset_stats();

    /// @}

    /// Return an opaque, non-owning pointer to the underlying ImageCache
    /// (if there is one).
    std::shared_ptr<ImageCache> imagecache() const;

    // For testing -- do not use
    static void unit_test_hash();

    TextureSystem(std::shared_ptr<ImageCache> imagecache);
    ~TextureSystem();

private:
    // PIMPL idiom
    using Impl = TextureSystemImpl;
    // class Impl;
    static void impl_deleter(Impl*);
    std::unique_ptr<Impl, decltype(&impl_deleter)> m_impl;

    // User code should never directly construct or destruct a TextureSystem.
    // Always use TextureSystem::create() and TextureSystem::destroy().
};


OIIO_NAMESPACE_END
