/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/


/// \file
/// An API for accessing filtered texture lookups via a system that
/// automatically manages a cache of resident texture.


#ifndef OPENIMAGEIO_TEXTURE_H
#define OPENIMAGEIO_TEXTURE_H

#include <OpenImageIO/varyingref.h>
#include <OpenImageIO/ustring.h>
#include <OpenImageIO/imageio.h>

#include <OpenEXR/ImathVec.h>       /* because we need V3f */

OIIO_NAMESPACE_BEGIN

// Forward declaration
namespace pvt {

class TextureSystemImpl;

// Used internally by TextureSystem.  Unfortunately, this is the only
// clean place to store it.  Sorry, users, this isn't really for you.
enum TexFormat {
    TexFormatUnknown, TexFormatTexture, TexFormatTexture3d,
    TexFormatShadow, TexFormatCubeFaceShadow, TexFormatVolumeShadow,
    TexFormatLatLongEnv, TexFormatCubeFaceEnv,
    TexFormatLast
};

enum EnvLayout {
    LayoutTexture=0 /* ordinary texture - no special env wrap */,
    LayoutLatLong, LayoutCubeThreeByTwo, LayoutCubeOneBySix, EnvLayoutLast
};

} // pvt namespace




namespace Tex {

/// Wrap mode describes what happens when texture coordinates describe
/// a value outside the usual [0,1] range where a texture is defined.
enum class Wrap {
    Default,        ///< Use the default found in the file
    Black,          ///< Black outside [0..1]
    Clamp,          ///< Clamp to [0..1]
    Periodic,       ///< Periodic mod 1
    Mirror,         ///< Mirror the image
    PeriodicPow2,   ///< Periodic, but only for powers of 2!!!
    PeriodicSharedBorder,  ///< Periodic with shared border (env)
    Last            ///< Mark the end -- don't use this!
};

/// Utility: Return the Wrap enum corresponding to a wrap name:
/// "default", "black", "clamp", "periodic", "mirror".
OIIO_API Wrap decode_wrapmode (const char *name);
OIIO_API Wrap decode_wrapmode (ustring name);

/// Utility: Parse a single wrap mode (e.g., "periodic") or a
/// comma-separated wrap modes string (e.g., "black,clamp") into
/// separate Wrap enums for s and t.
OIIO_API void parse_wrapmodes (const char *wrapmodes,
                               Wrap &swrapcode, Wrap &twrapcode);


/// Mip mode determines if/how we use mipmaps
///
enum class MipMode {
    Default,      ///< Default high-quality lookup
    NoMIP,        ///< Just use highest-res image, no MIP mapping
    OneLevel,     ///< Use just one mipmap level
    Trilinear,    ///< Use two MIPmap levels (trilinear)
    Aniso         ///< Use two MIPmap levels w/ anisotropic
};

/// Interp mode determines how we sample within a mipmap level
///
enum class InterpMode {
    Closest,      ///< Force closest texel
    Bilinear,     ///< Force bilinear lookup within a mip level
    Bicubic,      ///< Force cubic lookup within a mip level
    SmartBicubic  ///< Bicubic when maxifying, else bilinear
};


/// Fixed width for SIMD batching texture lookups.
/// May be changed for experimentation or future expansion.
#ifndef OIIO_TEXTURE_SIMD_BATCH_WIDTH
#define OIIO_TEXTURE_SIMD_BATCH_WIDTH 16
#endif

static constexpr int BatchWidth = OIIO_TEXTURE_SIMD_BATCH_WIDTH;
static constexpr int BatchAlign = BatchWidth * sizeof(float);

typedef simd::VecType<float,OIIO_TEXTURE_SIMD_BATCH_WIDTH>::type FloatWide;
typedef simd::VecType<int,OIIO_TEXTURE_SIMD_BATCH_WIDTH>::type IntWide;
typedef uint64_t RunMask;

#if OIIO_TEXTURE_SIMD_BATCH_WIDTH == 4
static constexpr RunMask RunMaskOn = 0xf;
#elif OIIO_TEXTURE_SIMD_BATCH_WIDTH == 8
static constexpr RunMask RunMaskOn = 0xff;
#elif OIIO_TEXTURE_SIMD_BATCH_WIDTH == 16
static constexpr RunMask RunMaskOn = 0xffff;
#elif OIIO_TEXTURE_SIMD_BATCH_WIDTH == 32
static constexpr RunMask RunMaskOn = 0xffffffff;
#elif OIIO_TEXTURE_SIMD_BATCH_WIDTH == 64
static constexpr RunMask RunMaskOn = 0xffffffffffffffffULL;
#else
# error "Not a valid OIIO_TEXTURE_SIMD_BATCH_WIDTH choice"
#endif

}  // namespace Tex


/// Data type for flags that indicate on a point-by-point basis whether
/// we want computations to be performed.
typedef unsigned char Runflag;

/// Pre-defined values for Runflag's.
///
enum RunFlagVal { RunFlagOff = 0, RunFlagOn = 255 };

class TextureOptions;  // forward declaration



/// Encapsulate all the options needed for texture lookups.  Making
/// these options all separate parameters to the texture API routines is
/// very ugly and also a big pain whenever we think of new options to
/// add.  So instead we collect all those little options into one
/// structure that can just be passed by reference to the texture API
/// routines.
class OIIO_API TextureOpt {
public:
    /// Wrap mode describes what happens when texture coordinates describe
    /// a value outside the usual [0,1] range where a texture is defined.
    enum Wrap {
        WrapDefault,        ///< Use the default found in the file
        WrapBlack,          ///< Black outside [0..1]
        WrapClamp,          ///< Clamp to [0..1]
        WrapPeriodic,       ///< Periodic mod 1
        WrapMirror,         ///< Mirror the image
        WrapPeriodicPow2,   ///< Periodic, but only for powers of 2!!!
        WrapPeriodicSharedBorder,  ///< Periodic with shared border (env)
        WrapLast            ///< Mark the end -- don't use this!
    };

    /// Mip mode determines if/how we use mipmaps
    ///
    enum MipMode {
        MipModeDefault,      ///< Default high-quality lookup
        MipModeNoMIP,        ///< Just use highest-res image, no MIP mapping
        MipModeOneLevel,     ///< Use just one mipmap level
        MipModeTrilinear,    ///< Use two MIPmap levels (trilinear)
        MipModeAniso         ///< Use two MIPmap levels w/ anisotropic
    };

    /// Interp mode determines how we sample within a mipmap level
    ///
    enum InterpMode {
        InterpClosest,      ///< Force closest texel
        InterpBilinear,     ///< Force bilinear lookup within a mip level
        InterpBicubic,      ///< Force cubic lookup within a mip level
        InterpSmartBicubic  ///< Bicubic when maxifying, else bilinear
    };


    /// Create a TextureOpt with all fields initialized to reasonable
    /// defaults.
    TextureOpt () 
        : firstchannel(0), subimage(0),
        swrap(WrapDefault), twrap(WrapDefault),
        mipmode(MipModeDefault), interpmode(InterpSmartBicubic),
        anisotropic(32), conservative_filter(true),
        sblur(0.0f), tblur(0.0f), swidth(1.0f), twidth(1.0f),
        fill(0.0f), missingcolor(NULL),
        // dresultds(NULL), dresultdt(NULL),
        time(0.0f), // bias(0.0f), samples(1),
        rwrap(WrapDefault), rblur(0.0f), rwidth(1.0f), // dresultdr(NULL),
        // actualchannels(0),
        envlayout(0)
    { }

    /// Convert a TextureOptions for one index into a TextureOpt.
    ///
    TextureOpt (const TextureOptions &opt, int index);

    int firstchannel;         ///< First channel of the lookup
    int subimage;             ///< Subimage or face ID
    ustring subimagename;     ///< Subimage name
    Wrap swrap;               ///< Wrap mode in the s direction
    Wrap twrap;               ///< Wrap mode in the t direction
    MipMode mipmode;          ///< Mip mode
    InterpMode interpmode;    ///< Interpolation mode
    int anisotropic;          ///< Maximum anisotropic ratio
    bool conservative_filter; ///< True == over-blur rather than alias
    float sblur, tblur;       ///< Blur amount
    float swidth, twidth;     ///< Multiplier for derivatives
    float fill;               ///< Fill value for missing channels
    const float *missingcolor;///< Color for missing texture
    float time;               ///< Time (for time-dependent texture lookups)
    float bias;               ///< Bias for shadows
    int   samples;            ///< Number of samples for shadows

    // For 3D volume texture lookups only:
    Wrap rwrap;               ///< Wrap mode in the r direction
    float rblur;              ///< Blur amount in the r direction
    float rwidth;             ///< Multiplier for derivatives in r direction

    /// Utility: Return the Wrap enum corresponding to a wrap name:
    /// "default", "black", "clamp", "periodic", "mirror".
    static Wrap decode_wrapmode (const char *name) {
        return (Wrap) Tex::decode_wrapmode(name);
    }
    static Wrap decode_wrapmode (ustring name) {
        return (Wrap) Tex::decode_wrapmode(name);
    }

    /// Utility: Parse a single wrap mode (e.g., "periodic") or a
    /// comma-separated wrap modes string (e.g., "black,clamp") into
    /// separate Wrap enums for s and t.
    static void parse_wrapmodes (const char *wrapmodes,
                                 TextureOpt::Wrap &swrapcode,
                                 TextureOpt::Wrap &twrapcode) {
        Tex::parse_wrapmodes (wrapmodes,
                              *(Tex::Wrap *)&swrapcode,
                              *(Tex::Wrap *)&twrapcode);
    }

private:
    // Options set INTERNALLY by libtexture after the options are passed
    // by the user.  Users should not attempt to alter these!
    int envlayout;    // Layout for environment wrap
    friend class pvt::TextureSystemImpl;
};



/// Texture options for a batch of Tex::BatchWidth points and run mask.
class OIIO_API TextureOptBatch {
public:
    /// Create a TextureOptBatch with all fields initialized to reasonable
    /// defaults.
    TextureOptBatch () {}   // use inline initializers

    // Options that may be different for each point we're texturing
    alignas(Tex::BatchAlign) float sblur[Tex::BatchWidth];    ///< Blur amount
    alignas(Tex::BatchAlign) float tblur[Tex::BatchWidth];
    alignas(Tex::BatchAlign) float rblur[Tex::BatchWidth];
    alignas(Tex::BatchAlign) float swidth[Tex::BatchWidth];   ///< Multiplier for derivatives
    alignas(Tex::BatchAlign) float twidth[Tex::BatchWidth];
    alignas(Tex::BatchAlign) float rwidth[Tex::BatchWidth];
    // Note: rblur,rwidth only used for volumetric lookups

    // Options that must be the same for all points we're texturing at once
    int firstchannel = 0;                 ///< First channel of the lookup
    int subimage = 0;                     ///< Subimage or face ID
    ustring subimagename;                 ///< Subimage name
    Tex::Wrap swrap = Tex::Wrap::Default; ///< Wrap mode in the s direction
    Tex::Wrap twrap = Tex::Wrap::Default; ///< Wrap mode in the t direction
    Tex::Wrap rwrap = Tex::Wrap::Default; ///< Wrap mode in the r direction (volumetric)
    Tex::MipMode mipmode = Tex::MipMode::Default;  ///< Mip mode
    Tex::InterpMode interpmode = Tex::InterpMode::SmartBicubic;  ///< Interpolation mode
    int anisotropic = 32;                 ///< Maximum anisotropic ratio
    int conservative_filter = 1;          ///< True: over-blur rather than alias
    float fill = 0.0f;                    ///< Fill value for missing channels
    const float *missingcolor = nullptr;  ///< Color for missing texture

private:
    // Options set INTERNALLY by libtexture after the options are passed
    // by the user.  Users should not attempt to alter these!
    int envlayout = 0;               // Layout for environment wrap

    friend class pvt::TextureSystemImpl;
};



/// DEPRECATED(1.8)
/// Encapsulate all the options needed for texture lookups.  Making
/// these options all separate parameters to the texture API routines is
/// very ugly and also a big pain whenever we think of new options to
/// add.  So instead we collect all those little options into one
/// structure that can just be passed by reference to the texture API
/// routines.
class OIIO_API TextureOptions {
public:
    /// Wrap mode describes what happens when texture coordinates describe
    /// a value outside the usual [0,1] range where a texture is defined.
    enum Wrap {
        WrapDefault,        ///< Use the default found in the file
        WrapBlack,          ///< Black outside [0..1]
        WrapClamp,          ///< Clamp to [0..1]
        WrapPeriodic,       ///< Periodic mod 1
        WrapMirror,         ///< Mirror the image
        WrapPeriodicPow2,   ///< Periodic, but only for powers of 2!!!
        WrapPeriodicSharedBorder,  ///< Periodic with shared border (env)
        WrapLast            ///< Mark the end -- don't use this!
    };

    /// Mip mode determines if/how we use mipmaps
    ///
    enum MipMode {
        MipModeDefault,      ///< Default high-quality lookup
        MipModeNoMIP,        ///< Just use highest-res image, no MIP mapping
        MipModeOneLevel,     ///< Use just one mipmap level
        MipModeTrilinear,    ///< Use two MIPmap levels (trilinear)
        MipModeAniso         ///< Use two MIPmap levels w/ anisotropic
    };

    /// Interp mode determines how we sample within a mipmap level
    ///
    enum InterpMode {
        InterpClosest,      ///< Force closest texel
        InterpBilinear,     ///< Force bilinear lookup within a mip level
        InterpBicubic,      ///< Force cubic lookup within a mip level
        InterpSmartBicubic  ///< Bicubic when maxifying, else bilinear
    };

    /// Create a TextureOptions with all fields initialized to reasonable
    /// defaults.
    TextureOptions ();

    /// Convert a TextureOpt for one point into a TextureOptions with
    /// uninform values.
    TextureOptions (const TextureOpt &opt);

    // Options that must be the same for all points we're texturing at once
    int firstchannel;         ///< First channel of the lookup
    int subimage;             ///< Subimage or face ID
    ustring subimagename;     ///< Subimage name
    Wrap swrap;               ///< Wrap mode in the s direction
    Wrap twrap;               ///< Wrap mode in the t direction
    MipMode mipmode;          ///< Mip mode
    InterpMode interpmode;    ///< Interpolation mode
    int anisotropic;          ///< Maximum anisotropic ratio
    bool conservative_filter; ///< True == over-blur rather than alias

    // Options that may be different for each point we're texturing
    VaryingRef<float> sblur, tblur;   ///< Blur amount
    VaryingRef<float> swidth, twidth; ///< Multiplier for derivatives
    VaryingRef<float> time;           ///< Time
    VaryingRef<float> bias;           ///< Bias
    VaryingRef<float> fill;           ///< Fill value for missing channels
    VaryingRef<float> missingcolor;   ///< Color for missing texture
    VaryingRef<int>   samples;        ///< Number of samples

    // For 3D volume texture lookups only:
    Wrap rwrap;                ///< Wrap mode in the r direction
    VaryingRef<float> rblur;   ///< Blur amount in the r direction
    VaryingRef<float> rwidth;  ///< Multiplier for derivatives in r direction

    /// Utility: Return the Wrap enum corresponding to a wrap name:
    /// "default", "black", "clamp", "periodic", "mirror".
    static Wrap decode_wrapmode (const char *name) {
        return (Wrap)Tex::decode_wrapmode (name);
    }
    static Wrap decode_wrapmode (ustring name) {
        return (Wrap)Tex::decode_wrapmode (name);
    }

    /// Utility: Parse a single wrap mode (e.g., "periodic") or a
    /// comma-separated wrap modes string (e.g., "black,clamp") into
    /// separate Wrap enums for s and t.
    static void parse_wrapmodes (const char *wrapmodes,
                                 TextureOptions::Wrap &swrapcode,
                                 TextureOptions::Wrap &twrapcode) {
        Tex::parse_wrapmodes (wrapmodes,
                              *(Tex::Wrap *)&swrapcode,
                              *(Tex::Wrap *)&twrapcode);
    }

private:
    // Options set INTERNALLY by libtexture after the options are passed
    // by the user.  Users should not attempt to alter these!
    friend class pvt::TextureSystemImpl;
    friend class TextureOpt;
};




/// Define an API to an abstract class that that manages texture files,
/// caches of open file handles as well as tiles of texels so that truly
/// huge amounts of texture may be accessed by an application with low
/// memory footprint, and ways to perform antialiased texture, shadow
/// map, and environment map lookups.
class OIIO_API TextureSystem {
public:
    /// Create a TextureSystem and return a pointer.  This should only be
    /// freed by passing it to TextureSystem::destroy()!
    ///
    /// If shared==true, it's intended to be shared with other like-minded
    /// owners in the same process who also ask for a shared texture system.
    /// If false, a private texture system and cache will be created.
    static TextureSystem *create (bool shared=true);

    /// Destroy a TextureSystem that was created using
    /// TextureSystem::create().  For the variety that takes a
    /// teardown_imagecache parameter, if set to true it will cause the
    /// underlying ImageCache to be fully destroyed, even if it's the
    /// "shared" ImageCache.
    static void destroy (TextureSystem *x);
    static void destroy (TextureSystem *x, bool teardown_imagecache);

    TextureSystem (void) { }
    virtual ~TextureSystem () { }

    /// Set an attribute controlling the texture system.  Return true
    /// if the name and type were recognized and the attrib was set.
    /// Documented attributes:
    ///     int max_open_files : maximum number of file handles held open
    ///     float max_memory_MB : maximum tile cache size, in MB
    ///     string searchpath : colon-separated search path for texture files
    ///     string plugin_searchpath : colon-separated search path for plugins
    ///     matrix44 worldtocommon : the world-to-common transformation
    ///     matrix44 commontoworld : the common-to-world transformation
    ///     int autotile : if >0, tile size to emulate for non-tiled images
    ///     int autoscanline : autotile using full width tiles
    ///     int automip : if nonzero, emulate mipmap on the fly
    ///     int accept_untiled : if nonzero, accept untiled images
    ///     int accept_unmipped : if nonzero, accept unmipped images
    ///     int failure_retries : how many times to retry a read failure
    ///     int deduplicate : if nonzero, detect duplicate textures (default=1)
    ///     int gray_to_rgb : make 1-channel images fill RGB lookups
    ///     int max_tile_channels : max channels to store all chans in a tile
    ///     string latlong_up : default "up" direction for latlong ("y")
    ///     int flip_t : flip v coord for texture lookups?
    ///     int max_errors_per_file : Limits how many errors to issue for
    ///                               issue for each (default: 100)
    ///
    virtual bool attribute (string_view name, TypeDesc type, const void *val) = 0;
    // Shortcuts for common types
    virtual bool attribute (string_view name, int val) = 0;
    virtual bool attribute (string_view name, float val) = 0;
    virtual bool attribute (string_view name, double val) = 0;
    virtual bool attribute (string_view name, string_view val) = 0;

    /// Get the named attribute, store it in value.
    virtual bool getattribute (string_view name,
                               TypeDesc type, void *val) const = 0;
    // Shortcuts for common types
    virtual bool getattribute (string_view name, int &val) const = 0;
    virtual bool getattribute (string_view name, float &val) const = 0;
    virtual bool getattribute (string_view name, double &val) const = 0;
    virtual bool getattribute (string_view name, char **val) const = 0;
    virtual bool getattribute (string_view name, std::string &val) const = 0;

    /// Define an opaque data type that allows us to have a pointer
    /// to certain per-thread information that the TextureSystem maintains.
    /// Any given one of these should NEVER be shared between running
    /// threads.
    class Perthread;

    /// Retrieve a Perthread, unique to the calling thread. This is a
    /// thread-specific pointer that will always return the Perthread for a
    /// thread, which will also be automatically destroyed when the thread
    /// terminates.
    ///
    /// Applications that want to manage their own Perthread pointers (with
    /// create_thread_info and destroy_thread_info) should still call this,
    /// but passing in their managed pointer. If the passed-in threadinfo is
    /// not NULL, it won't create a new one or retrieve a TSP, but it will
    /// do other necessary housekeeping on the Perthread information.
    virtual Perthread * get_perthread_info (Perthread *thread_info=NULL) = 0;

    /// Create a new Perthread. It is the caller's responsibility to
    /// eventually destroy it using destroy_thread_info().
    virtual Perthread * create_thread_info () = 0;

    /// Destroy a Perthread that was allocated by create_thread_info().
    virtual void destroy_thread_info (Perthread *threadinfo) = 0;

    /// Define an opaque data type that allows us to have a handle to a
    /// texture (already having its name resolved) but without exposing
    /// any internals.
    class TextureHandle;

    /// Retrieve an opaque handle for fast texture lookups.  The opaque
    /// pointer thread_info is thread-specific information returned by
    /// get_perthread_info().  Return NULL if something has gone
    /// horribly wrong.
    virtual TextureHandle * get_texture_handle (ustring filename,
                                            Perthread *thread_info=NULL) = 0;

    /// Return true if the texture handle (previously returned by
    /// get_texture_handle()) is a valid texture that can be subsequently
    /// read or sampled.
    virtual bool good (TextureHandle *texture_handle) = 0;

    /// Filtered 2D texture lookup for a single point.
    ///
    /// s,t are the texture coordinates; dsdx, dtdx, dsdy, and dtdy are
    /// the differentials of s and t change in some canonical directions
    /// x and y.  The choice of x and y are not important to the
    /// implementation; it can be any imposed 2D coordinates, such as
    /// pixels in screen space, adjacent samples in parameter space on a
    /// surface, etc.
    ///
    /// The result is placed in result[0..nchannels-1].  If dresuls and
    /// dresultdt are non-NULL, then they [0..nchannels-1] will get the
    /// texture gradients, i.e., the rate of change per unit s and t,
    /// respectively, of the texture.
    ///
    /// Return true if the file is found and could be opened by an
    /// available ImageIO plugin, otherwise return false.
    virtual bool texture (ustring filename, TextureOpt &options,
                          float s, float t, float dsdx, float dtdx,
                          float dsdy, float dtdy,
                          int nchannels, float *result,
                          float *dresultds=NULL, float *dresultdt=NULL) = 0;

    /// Version that takes nchannels and derivatives explicitly, if the app
    /// already has a texture handle and per-thread info.
    virtual bool texture (TextureHandle *texture_handle,
                          Perthread *thread_info, TextureOpt &options,
                          float s, float t, float dsdx, float dtdx,
                          float dsdy, float dtdy,
                          int nchannels, float *result,
                          float *dresultds=NULL, float *dresultdt=NULL) = 0;

    /// Retrieve filtered (possibly anisotropic) texture lookups for
    /// an entire batch of points. The mask is a bitfield giving the
    /// lanes to compute.
    ///
    /// Return true if the file is found and could be opened by an
    /// available ImageIO plugin, otherwise return false.
    ///
    /// The float* inputs are each pointing to an array of float[BatchWidth]
    /// giving one float value for each batch lane.
    ///
    /// The float* results act like float[nchannels][BatchWidth], so that
    /// effectively result[0..BatchWidth-1] are the "red" result for each
    /// lane, result[BatchWidth..2*BatchWidth-1] are the "green" results, etc.
    /// The dresultds and dresultdt should either both be provided, or else
    /// both be nullptr (meaning no derivative results are required).
    virtual bool texture (ustring filename, TextureOptBatch &options,
                          Tex::RunMask mask, const float *s, const float *t,
                          const float *dsdx, const float *dtdx,
                          const float *dsdy, const float *dtdy,
                          int nchannels, float *result,
                          float *dresultds=nullptr,
                          float *dresultdt=nullptr) = 0;
    virtual bool texture (TextureHandle *texture_handle,
                          Perthread *thread_info, TextureOptBatch &options,
                          Tex::RunMask mask, const float *s, const float *t,
                          const float *dsdx, const float *dtdx,
                          const float *dsdy, const float *dtdy,
                          int nchannels, float *result,
                          float *dresultds=nullptr,
                          float *dresultdt=nullptr) = 0;

    /// Old multi-point API call.
    /// DEPRECATED (1.8)
    virtual bool texture (ustring filename, TextureOptions &options,
                          Runflag *runflags, int beginactive, int endactive,
                          VaryingRef<float> s, VaryingRef<float> t,
                          VaryingRef<float> dsdx, VaryingRef<float> dtdx,
                          VaryingRef<float> dsdy, VaryingRef<float> dtdy,
                          int nchannels, float *result,
                          float *dresultds=NULL, float *dresultdt=NULL) = 0;
    virtual bool texture (TextureHandle *texture_handle,
                          Perthread *thread_info, TextureOptions &options,
                          Runflag *runflags, int beginactive, int endactive,
                          VaryingRef<float> s, VaryingRef<float> t,
                          VaryingRef<float> dsdx, VaryingRef<float> dtdx,
                          VaryingRef<float> dsdy, VaryingRef<float> dtdy,
                          int nchannels, float *result,
                          float *dresultds=NULL, float *dresultdt=NULL) = 0;

    /// Retrieve a 3D texture lookup at a single point.
    ///
    /// Return true if the file is found and could be opened by an
    /// available ImageIO plugin, otherwise return false.
    virtual bool texture3d (ustring filename, TextureOpt &options,
                            const Imath::V3f &P, const Imath::V3f &dPdx,
                            const Imath::V3f &dPdy, const Imath::V3f &dPdz,
                            int nchannels, float *result,
                            float *dresultds=NULL, float *dresultdt=NULL,
                            float *dresultdr=NULL) = 0;

    /// Slightly faster version of texture3d() lookup if the app already
    /// has a texture handle and per-thread info.
    virtual bool texture3d (TextureHandle *texture_handle,
                            Perthread *thread_info, TextureOpt &options,
                            const Imath::V3f &P, const Imath::V3f &dPdx,
                            const Imath::V3f &dPdy, const Imath::V3f &dPdz,
                            int nchannels, float *result,
                            float *dresultds=NULL, float *dresultdt=NULL,
                            float *dresultdr=NULL) = 0;

    /// Batched 3D (volumetric) texture lookup.
    ///
    /// The inputs P, dPdx, dPdy, dPdz are pointers to arrays that look like:
    /// float P[3][BatchWidth], or alternately like Imath::Vec3<FloatWide>.
    /// The mask is a bitfield giving the lanes to compute.
    ///
    /// The float* results act like float[nchannels][BatchWidth], so that
    /// effectively result[0..BatchWidth-1] are the "red" result for each
    /// lane, result[BatchWidth..2*BatchWidth-1] are the "green" results, etc.
    /// The dresultds and dresultdt should either both be provided, or else
    /// both be nullptr (meaning no derivative results are required).
    ///
    /// Return true if the file is found and could be opened by an
    /// available ImageIO plugin, otherwise return false.
    virtual bool texture3d (ustring filename,
                            TextureOptBatch &options, Tex::RunMask mask,
                            const float *P, const float *dPdx,
                            const float *dPdy, const float *dPdz,
                            int nchannels, float *result,
                            float *dresultds=nullptr, float *dresultdt=nullptr,
                            float *dresultdr=nullptr) = 0;
    virtual bool texture3d (TextureHandle *texture_handle,
                            Perthread *thread_info,
                            TextureOptBatch &options, Tex::RunMask mask,
                            const float *P, const float *dPdx,
                            const float *dPdy, const float *dPdz,
                            int nchannels, float *result,
                            float *dresultds=nullptr, float *dresultdt=nullptr,
                            float *dresultdr=nullptr) = 0;

    /// Retrieve a 3D texture lookup at many points at once.
    /// DEPRECATED(1.8)
    virtual bool texture3d (ustring filename, TextureOptions &options,
                            Runflag *runflags, int beginactive, int endactive,
                            VaryingRef<Imath::V3f> P,
                            VaryingRef<Imath::V3f> dPdx,
                            VaryingRef<Imath::V3f> dPdy,
                            VaryingRef<Imath::V3f> dPdz,
                            int nchannels, float *result,
                            float *dresultds=NULL, float *dresultdt=NULL,
                            float *dresultdr=NULL) = 0;
    virtual bool texture3d (TextureHandle *texture_handle,
                            Perthread *thread_info, TextureOptions &options,
                            Runflag *runflags, int beginactive, int endactive,
                            VaryingRef<Imath::V3f> P,
                            VaryingRef<Imath::V3f> dPdx,
                            VaryingRef<Imath::V3f> dPdy,
                            VaryingRef<Imath::V3f> dPdz,
                            int nchannels, float *result,
                            float *dresultds=NULL, float *dresultdt=NULL,
                            float *dresultdr=NULL) = 0;

    /// Retrieve a shadow lookup for a single position P.
    ///
    /// Return true if the file is found and could be opened by an
    /// available ImageIO plugin, otherwise return false.
    virtual bool shadow (ustring filename, TextureOpt &options,
                         const Imath::V3f &P, const Imath::V3f &dPdx,
                         const Imath::V3f &dPdy, float *result,
                         float *dresultds=NULL, float *dresultdt=NULL) = 0;

    /// Slightly faster version of shadow() lookup if the app already
    /// has a texture handle and per-thread info.
    virtual bool shadow (TextureHandle *texture_handle, Perthread *thread_info,
                         TextureOpt &options,
                         const Imath::V3f &P, const Imath::V3f &dPdx,
                         const Imath::V3f &dPdy, float *result,
                         float *dresultds=NULL, float *dresultdt=NULL) = 0;

    /// Batched shadow lookups
    virtual bool shadow (ustring filename,
                         TextureOptBatch &options, Tex::RunMask mask,
                         const float *P, const float *dPdx, const float *dPdy,
                         float *result, float *dresultds=nullptr, float *dresultdt=nullptr) = 0;
    virtual bool shadow (TextureHandle *texture_handle, Perthread *thread_info,
                         TextureOptBatch &options, Tex::RunMask mask,
                         const float *P, const float *dPdx, const float *dPdy,
                         float *result, float *dresultds=nullptr, float *dresultdt=nullptr) = 0;

    /// Retrieve a shadow lookup for position P at many points at once.
    /// DEPRECATED(1.8)
    virtual bool shadow (ustring filename, TextureOptions &options,
                         Runflag *runflags, int beginactive, int endactive,
                         VaryingRef<Imath::V3f> P,
                         VaryingRef<Imath::V3f> dPdx,
                         VaryingRef<Imath::V3f> dPdy,
                         float *result,
                         float *dresultds=NULL, float *dresultdt=NULL) = 0;
    virtual bool shadow (TextureHandle *texture_handle, Perthread *thread_info,
                         TextureOptions &options,
                         Runflag *runflags, int beginactive, int endactive,
                         VaryingRef<Imath::V3f> P,
                         VaryingRef<Imath::V3f> dPdx,
                         VaryingRef<Imath::V3f> dPdy,
                         float *result,
                         float *dresultds=NULL, float *dresultdt=NULL) = 0;

    /// Retrieve an environment map lookup for direction R.
    ///
    /// Return true if the file is found and could be opened by an
    /// available ImageIO plugin, otherwise return false.
    virtual bool environment (ustring filename, TextureOpt &options,
                              const Imath::V3f &R, const Imath::V3f &dRdx,
                              const Imath::V3f &dRdy, int nchannels, float *result,
                              float *dresultds=NULL, float *dresultdt=NULL) = 0;

    /// Slightly faster version of environment() lookup if the app already
    /// has a texture handle and per-thread info.
    virtual bool environment (TextureHandle *texture_handle,
                              Perthread *thread_info, TextureOpt &options,
                              const Imath::V3f &R, const Imath::V3f &dRdx,
                              const Imath::V3f &dRdy, int nchannels, float *result,
                              float *dresultds=NULL, float *dresultdt=NULL) = 0;

    /// Batched environment looksups.
    ///
    /// The inputs R, dRdx, dRdy are pointers to arrays that look like:
    /// float R[3][BatchWidth], or alternately like Imath::Vec3<FloatWide>.
    /// The mask is a bitfield giving the lanes to compute.
    ///
    /// The float* results act like float[nchannels][BatchWidth], so that
    /// effectively result[0..BatchWidth-1] are the "red" result for each
    /// lane, result[BatchWidth..2*BatchWidth-1] are the "green" results, etc.
    /// The dresultds and dresultdt should either both be provided, or else
    /// both be nullptr (meaning no derivative results are required).
    ///
    /// Return true if the file is found and could be opened by an
    /// available ImageIO plugin, otherwise return false.
    virtual bool environment (ustring filename,
                              TextureOptBatch &options, Tex::RunMask mask,
                              const float *R, const float *dRdx, const float *dRdy,
                              int nchannels, float *result,
                              float *dresultds=nullptr, float *dresultdt=nullptr) = 0;
    virtual bool environment (TextureHandle *texture_handle, Perthread *thread_info,
                              TextureOptBatch &options, Tex::RunMask mask,
                              const float *R, const float *dRdx, const float *dRdy,
                              int nchannels, float *result,
                              float *dresultds=nullptr, float *dresultdt=nullptr) = 0;

    /// Retrieve an environment map lookup for direction R, for many
    /// points at once.
    /// DEPRECATED(1.8)
    virtual bool environment (ustring filename, TextureOptions &options,
                              Runflag *runflags, int beginactive, int endactive,
                              VaryingRef<Imath::V3f> R,
                              VaryingRef<Imath::V3f> dRdx,
                              VaryingRef<Imath::V3f> dRdy,
                              int nchannels, float *result,
                              float *dresultds=NULL, float *dresultdt=NULL) = 0;
    virtual bool environment (TextureHandle *texture_handle,
                              Perthread *thread_info, TextureOptions &options,
                              Runflag *runflags, int beginactive, int endactive,
                              VaryingRef<Imath::V3f> R,
                              VaryingRef<Imath::V3f> dRdx,
                              VaryingRef<Imath::V3f> dRdy,
                              int nchannels, float *result,
                              float *dresultds=NULL, float *dresultdt=NULL) = 0;

    /// Given possibly-relative 'filename', resolve it using the search
    /// path rules and return the full resolved filename.
    virtual std::string resolve_filename (const std::string &filename) const=0;

    /// Get information about the given texture.  Return true if found
    /// and the data has been put in *data.  Return false if the texture
    /// doesn't exist, doesn't have the requested data, if the data
    /// doesn't match the type requested. or some other failure.
    virtual bool get_texture_info (ustring filename, int subimage,
                          ustring dataname, TypeDesc datatype, void *data) = 0;
    virtual bool get_texture_info (TextureHandle *texture_handle,
                          Perthread *thread_info, int subimage,
                          ustring dataname, TypeDesc datatype, void *data) = 0;

    /// Get the ImageSpec associated with the named texture
    /// (specifically, the first MIP-map level).  If the file is found
    /// and is an image format that can be read, store a copy of its
    /// specification in spec and return true.  Return false if the file
    /// was not found or could not be opened as an image file by any
    /// available ImageIO plugin.
    virtual bool get_imagespec (ustring filename, int subimage,
                                ImageSpec &spec) = 0;
    virtual bool get_imagespec (TextureHandle *texture_handle,
                                Perthread *thread_info, int subimage,
                                ImageSpec &spec) = 0;

    /// Return a pointer to an ImageSpec associated with the named
    /// texture (specifically, the first MIP-map level) if the file is
    /// found and is an image format that can be read, otherwise return
    /// NULL.
    ///
    /// This method is much more efficient than get_imagespec(), since
    /// it just returns a pointer to the spec held internally by the
    /// underlying ImageCache (rather than copying the spec to the
    /// user's memory).  However, the caller must beware that the
    /// pointer is only valid as long as nobody (even other threads)
    /// calls invalidate() on the file, or invalidate_all(), or destroys
    /// the TextureSystem.
    virtual const ImageSpec *imagespec (ustring filename, int subimage=0) = 0;
    virtual const ImageSpec *imagespec (TextureHandle *texture_handle,
                                        Perthread *thread_info = NULL,
                                        int subimage=0) = 0;

    /// Retrieve the rectangle of raw unfiltered texels spanning
    /// [xbegin..xend) X [ybegin..yend) X [zbegin..zend), with
    /// "exclusive end" a la STL, specified as integer pixel coordinates
    /// in the designated MIP-map level, storing the texel values
    /// beginning at the address specified by result.
    /// The texel values will be converted to the type specified by
    /// format.  It is up to the caller to ensure that result points to
    /// an area of memory big enough to accommodate the requested
    /// rectangle (taking into consideration its dimensions, number of
    /// channels, and data format).  Requested pixels outside
    /// the valid pixel data region will be filled in with 0 values.
    ///
    /// Return true if the file is found and could be opened by an
    /// available ImageIO plugin, otherwise return false.
    virtual bool get_texels (ustring filename, TextureOpt &options,
                             int miplevel, int xbegin, int xend,
                             int ybegin, int yend, int zbegin, int zend,
                             int chbegin, int chend,
                             TypeDesc format, void *result) = 0;
    virtual bool get_texels (TextureHandle *texture_handle,
                             Perthread *thread_info, TextureOpt &options,
                             int miplevel, int xbegin, int xend,
                             int ybegin, int yend, int zbegin, int zend,
                             int chbegin, int chend,
                             TypeDesc format, void *result) = 0;

    /// If any of the API routines returned false indicating an error,
    /// this routine will return the error string (and clear any error
    /// flags).  If no error has occurred since the last time geterror()
    /// was called, it will return an empty string.
    virtual std::string geterror () const = 0;

    /// Return the statistics output as a huge string.
    ///
    virtual std::string getstats (int level=1, bool icstats=true) const = 0;

    /// Invalidate any cached information about the named file. A client
    /// might do this if, for example, they are aware that an image
    /// being held in the cache has been updated on disk.
    virtual void invalidate (ustring filename) = 0;

    /// Invalidate all cached data for all textures.  If force is true,
    /// everything will be invalidated, no matter how wasteful it is,
    /// but if force is false, in actuality files will only be
    /// invalidated if their modification times have been changed since
    /// they were first opened.
    virtual void invalidate_all (bool force=false) = 0;

    /// Reset most statistics to be as they were with a fresh
    /// TextureSystem.  Caveat emptor: this does not flush the cache
    /// itelf, so the resulting statistics from the next set of texture
    /// requests will not match the number of tile reads, etc., that
    /// would have resulted from a new TextureSystem.
    virtual void reset_stats () = 0;

private:
    // Make delete private and unimplemented in order to prevent apps
    // from calling it.  Instead, they should call TextureSystem::destroy().
    void operator delete (void * /*todel*/) { }

};


OIIO_NAMESPACE_END

#endif // OPENIMAGEIO_TEXTURE_H
