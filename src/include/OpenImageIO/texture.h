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

#include "varyingref.h"
#include "ustring.h"
#include "imageio.h"

#include <OpenEXR/ImathVec.h>       /* because we need V3f */

OIIO_NAMESPACE_ENTER
{

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
        : nchannels(1), firstchannel(0), subimage(0),
        swrap(WrapDefault), twrap(WrapDefault),
        mipmode(MipModeDefault), interpmode(InterpSmartBicubic),
        anisotropic(32), conservative_filter(true),
        sblur(0.0f), tblur(0.0f), swidth(1.0f), twidth(1.0f),
        fill(0.0f), missingcolor(NULL),
        dresultds(NULL), dresultdt(NULL),
        time(0.0f), bias(0.0f), samples(1),
        rwrap(WrapDefault), rblur(0.0f), rwidth(1.0f), dresultdr(NULL),
        actualchannels(0),
        swrap_func(NULL), twrap_func(NULL), rwrap_func(NULL),
        envlayout(0)
    { }

    /// Convert a TextureOptions for one index into a TextureOpt.
    ///
    TextureOpt (const TextureOptions &opt, int index);

    int nchannels;            ///< Number of channels to look up
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
    float *dresultds;         ///< Deriv of the result along s (if not NULL)
    float *dresultdt;         ///< Deriv of the result along t (if not NULL)
    float time;               ///< Time (for time-dependent texture lookups)
    float bias;               ///< Bias for shadows
    int   samples;            ///< Number of samples for shadows

    // For 3D volume texture lookups only:
    Wrap rwrap;               ///< Wrap mode in the r direction
    float rblur;              ///< Blur amount in the r direction
    float rwidth;             ///< Multiplier for derivatives in r direction
    float *dresultdr;         ///< Deriv of the result along r (if not NULL)

    /// Utility: Return the Wrap enum corresponding to a wrap name:
    /// "default", "black", "clamp", "periodic", "mirror".
    static Wrap decode_wrapmode (const char *name);
    static Wrap decode_wrapmode (ustring name);

    /// Utility: Parse a single wrap mode (e.g., "periodic") or a
    /// comma-separated wrap modes string (e.g., "black,clamp") into
    /// separate Wrap enums for s and t.
    static void parse_wrapmodes (const char *wrapmodes,
                                 TextureOpt::Wrap &swrapcode,
                                 TextureOpt::Wrap &twrapcode);

private:
    // Options set INTERNALLY by libtexture after the options are passed
    // by the user.  Users should not attempt to alter these!
    int actualchannels;    // True number of channels read
    typedef bool (*wrap_impl) (int &coord, int origin, int width);
    wrap_impl swrap_func, twrap_func, rwrap_func;
    int envlayout;    // Layout for environment wrap
    friend class pvt::TextureSystemImpl;
};



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
    int nchannels;            ///< Number of channels to look up
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
    float *dresultds;                 ///< Deriv of the result along s (if not NULL)
    float *dresultdt;                 ///< Deriv of the result along t (if not NULL)

    // For 3D volume texture lookups only:
    Wrap rwrap;                ///< Wrap mode in the r direction
    VaryingRef<float> rblur;   ///< Blur amount in the r direction
    VaryingRef<float> rwidth;  ///< Multiplier for derivatives in r direction
    float *dresultdr;          ///< Deriv of the result along r (if not NULL)

    /// Utility: Return the Wrap enum corresponding to a wrap name:
    /// "default", "black", "clamp", "periodic", "mirror".
    static Wrap decode_wrapmode (const char *name) {
        return (Wrap)TextureOpt::decode_wrapmode (name);
    }
    static Wrap decode_wrapmode (ustring name) {
        return (Wrap)TextureOpt::decode_wrapmode (name);
    }

    /// Utility: Parse a single wrap mode (e.g., "periodic") or a
    /// comma-separated wrap modes string (e.g., "black,clamp") into
    /// separate Wrap enums for s and t.
    static void parse_wrapmodes (const char *wrapmodes,
                                 TextureOptions::Wrap &swrapcode,
                                 TextureOptions::Wrap &twrapcode) {
        TextureOpt::parse_wrapmodes (wrapmodes,
                                     *(TextureOpt::Wrap *)&swrapcode,
                                     *(TextureOpt::Wrap *)&twrapcode);
    }

private:
    // Options set INTERNALLY by libtexture after the options are passed
    // by the user.  Users should not attempt to alter these!
    int actualchannels;    // True number of channels read
    typedef bool (*wrap_impl) (int &coord, int origin, int width);
    wrap_impl swrap_func, twrap_func, rwrap_func;
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
    /// owners in the same process who also ask for a shared cache.  If
    /// false, a private image cache will be created.
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

    /// Close everything, free resources, start from scratch.
    ///
    virtual void clear () = 0;

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
    ///     string latlong_up : default "up" direction for latlong ("y")
    ///
    virtual bool attribute (string_view name, TypeDesc type, const void *val) = 0;
    // Shortcuts for common types
    virtual bool attribute (string_view name, int val) = 0;
    virtual bool attribute (string_view name, float val) = 0;
    virtual bool attribute (string_view name, double val) = 0;
    virtual bool attribute (string_view name, string_view val) = 0;

    /// Get the named attribute, store it in value.
    virtual bool getattribute (string_view name, TypeDesc type, void *val) = 0;
    // Shortcuts for common types
    virtual bool getattribute (string_view name, int &val) = 0;
    virtual bool getattribute (string_view name, float &val) = 0;
    virtual bool getattribute (string_view name, double &val) = 0;
    virtual bool getattribute (string_view name, char **val) = 0;
    virtual bool getattribute (string_view name, std::string &val) = 0;

    /// Define an opaque data type that allows us to have a pointer
    /// to certain per-thread information that the TextureSystem maintains.
    class Perthread;

    /// Retrieve an opaque handle for per-thread info, to be used for
    /// get_texture_handle and the texture routines that take handles
    /// directly.
    virtual Perthread * get_perthread_info () = 0;

    /// Define an opaque data type that allows us to have a handle to a
    /// texture (already having its name resolved) but without exposing
    /// any internals.
    class TextureHandle;

    /// Retrieve an opaque handle for fast texture lookups.  The opaque
    /// point thread_info is thread-specific information returned by
    /// get_perthread_info().  Return NULL if something has gone
    /// horribly wrong.
    virtual TextureHandle * get_texture_handle (ustring filename,
                                            Perthread *thread_info=NULL) = 0;

    /// Filtered 2D texture lookup for a single point.
    ///
    /// s,t are the texture coordinates; dsdx, dtdx, dsdy, and dtdy are
    /// the differentials of s and t change in some canonical directions
    /// x and y.  The choice of x and y are not important to the
    /// implementation; it can be any imposed 2D coordinates, such as
    /// pixels in screen space, adjacent samples in parameter space on a
    /// surface, etc.
    ///
    /// Return true if the file is found and could be opened by an
    /// available ImageIO plugin, otherwise return false.
    virtual bool texture (ustring filename, TextureOpt &options,
                          float s, float t, float dsdx, float dtdx,
                          float dsdy, float dtdy, float *result) = 0;

    /// Slightly faster version of 2D texture() lookup if the app already
    /// has a texture handle and per-thread info.
    virtual bool texture (TextureHandle *texture_handle,
                          Perthread *thread_info, TextureOpt &options,
                          float s, float t, float dsdx, float dtdx,
                          float dsdy, float dtdy, float *result) = 0;

    /// Deprecated version that uses old TextureOptions for a single-point
    /// lookup.
    virtual bool texture (ustring filename, TextureOptions &options,
                          float s, float t, float dsdx, float dtdx,
                          float dsdy, float dtdy, float *result) = 0;

    /// Retrieve filtered (possibly anisotropic) texture lookups for
    /// several points at once.
    ///
    /// All of the VaryingRef parameters (and fields in options)
    /// describe texture lookup parameters at an array of positions.
    /// But this routine only computes them from indices i where
    /// beginactive <= i < endactive, and ONLY when runflags[i] is
    /// nonzero.
    ///
    /// Return true if the file is found and could be opened by an
    /// available ImageIO plugin, otherwise return false.
    virtual bool texture (ustring filename, TextureOptions &options,
                          Runflag *runflags, int beginactive, int endactive,
                          VaryingRef<float> s, VaryingRef<float> t,
                          VaryingRef<float> dsdx, VaryingRef<float> dtdx,
                          VaryingRef<float> dsdy, VaryingRef<float> dtdy,
                          float *result) = 0;

    /// Retrieve a 3D texture lookup at a single point.
    ///
    /// Return true if the file is found and could be opened by an
    /// available ImageIO plugin, otherwise return false.
    virtual bool texture3d (ustring filename, TextureOpt &options,
                            const Imath::V3f &P, const Imath::V3f &dPdx,
                            const Imath::V3f &dPdy, const Imath::V3f &dPdz,
                            float *result) = 0;

    /// Slightly faster version of texture3d() lookup if the app already
    /// has a texture handle and per-thread info.
    virtual bool texture3d (TextureHandle *texture_handle,
                            Perthread *thread_info, TextureOpt &options,
                            const Imath::V3f &P, const Imath::V3f &dPdx,
                            const Imath::V3f &dPdy, const Imath::V3f &dPdz,
                            float *result) = 0;

    /// Deprecated
    ///
    virtual bool texture3d (ustring filename, TextureOptions &options,
                            const Imath::V3f &P, const Imath::V3f &dPdx,
                            const Imath::V3f &dPdy, const Imath::V3f &dPdz,
                            float *result) {
        TextureOpt opt (options, 0);
        return texture3d (filename, opt, P, dPdx, dPdy, dPdz, result);
    }

    /// Retrieve a 3D texture lookup at many points at once.
    ///
    /// Return true if the file is found and could be opened by an
    /// available ImageIO plugin, otherwise return false.
    virtual bool texture3d (ustring filename, TextureOptions &options,
                            Runflag *runflags, int beginactive, int endactive,
                            VaryingRef<Imath::V3f> P,
                            VaryingRef<Imath::V3f> dPdx,
                            VaryingRef<Imath::V3f> dPdy,
                            VaryingRef<Imath::V3f> dPdz,
                            float *result) = 0;

    /// Retrieve a shadow lookup for a single position P.
    ///
    /// Return true if the file is found and could be opened by an
    /// available ImageIO plugin, otherwise return false.
    virtual bool shadow (ustring filename, TextureOpt &options,
                         const Imath::V3f &P, const Imath::V3f &dPdx,
                         const Imath::V3f &dPdy, float *result) = 0;

    /// Slightly faster version of shadow() lookup if the app already
    /// has a texture handle and per-thread info.
    virtual bool shadow (TextureHandle *texture_handle, Perthread *thread_info,
                         TextureOpt &options,
                         const Imath::V3f &P, const Imath::V3f &dPdx,
                         const Imath::V3f &dPdy, float *result) = 0;

    /// Retrieve a shadow lookup for position P at many points at once.
    ///
    /// Return true if the file is found and could be opened by an
    /// available ImageIO plugin, otherwise return false.
    virtual bool shadow (ustring filename, TextureOptions &options,
                         Runflag *runflags, int beginactive, int endactive,
                         VaryingRef<Imath::V3f> P,
                         VaryingRef<Imath::V3f> dPdx,
                         VaryingRef<Imath::V3f> dPdy,
                         float *result) = 0;

    /// Retrieve an environment map lookup for direction R.
    ///
    /// Return true if the file is found and could be opened by an
    /// available ImageIO plugin, otherwise return false.
    virtual bool environment (ustring filename, TextureOpt &options,
                              const Imath::V3f &R, const Imath::V3f &dRdx,
                              const Imath::V3f &dRdy, float *result) = 0;

    /// Slightly faster version of environment() lookup if the app already
    /// has a texture handle and per-thread info.
    virtual bool environment (TextureHandle *texture_handle,
                              Perthread *thread_info, TextureOpt &options,
                              const Imath::V3f &R, const Imath::V3f &dRdx,
                              const Imath::V3f &dRdy, float *result) = 0;

    /// Retrieve an environment map lookup for direction R, for many
    /// points at once.
    ///
    /// Return true if the file is found and could be opened by an
    /// available ImageIO plugin, otherwise return false.
    virtual bool environment (ustring filename, TextureOptions &options,
                              Runflag *runflags, int beginactive, int endactive,
                              VaryingRef<Imath::V3f> R,
                              VaryingRef<Imath::V3f> dRdx,
                              VaryingRef<Imath::V3f> dRdy,
                              float *result) = 0;

    /// Given possibly-relative 'filename', resolve it using the search
    /// path rules and return the full resolved filename.
    virtual std::string resolve_filename (const std::string &filename) const=0;

    /// Get information about the given texture.  Return true if found
    /// and the data has been put in *data.  Return false if the texture
    /// doesn't exist, doesn't have the requested data, if the data
    /// doesn't match the type requested. or some other failure.
    virtual bool get_texture_info (ustring filename, int subimage,
                          ustring dataname, TypeDesc datatype, void *data) = 0;

    /// Get the ImageSpec associated with the named texture
    /// (specifically, the first MIP-map level).  If the file is found
    /// and is an image format that can be read, store a copy of its
    /// specification in spec and return true.  Return false if the file
    /// was not found or could not be opened as an image file by any
    /// available ImageIO plugin.
    virtual bool get_imagespec (ustring filename, int subimage,
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
                             TypeDesc format, void *result) = 0;

    /// Deprecated
    ///
    virtual bool get_texels (ustring filename, TextureOptions &options,
                             int miplevel, int xbegin, int xend,
                             int ybegin, int yend, int zbegin, int zend,
                             TypeDesc format, void *result) {
        TextureOpt opt (options, 0);
        return get_texels (filename, opt, miplevel, xbegin, xend,
                           ybegin, yend, zbegin, zend, format, result);
    }

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


}
OIIO_NAMESPACE_EXIT

#endif // OPENIMAGEIO_TEXTURE_H
