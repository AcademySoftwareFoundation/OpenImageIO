/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2008 Larry Gritz.
// 
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
// 
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 
// (This is the MIT open source license.)
/////////////////////////////////////////////////////////////////////////////


#ifndef TEXTURE_H
#define TEXTURE_H

#include "varyingref.h"
#include "ustring.h"

namespace Imath {
#ifndef INCLUDED_IMATHVEC_H
    class V3f;
#endif
#ifndef INCLUDED_IMATHMATRIX_H
    class M44f;
#endif
};


namespace OpenImageIO {

// Forward declaration
namespace pvt {
    class TextureSystemImpl;
};


typedef unsigned char Runflag;
enum RunFlagVal { RunFlagOff = 0, RunFlagOn = 255 };


/// Encapsulate all the 
class TextureOptions {
public:
    enum Wrap {
        WrapDefault,        ///< Use the default found in the file
        WrapBlack,          ///< Black outside [0..1]
        WrapClamp,          ///< Clamp to [0..1]
        WrapPeriodic,       ///< Periodic mod 1
        WrapMirror,         ///< Mirror the image
        WrapLast            ///< Mark the end -- don't use this!
    };

    /// Create a TextureOptions with all fields initialized to reasonable
    /// defaults.
    TextureOptions ();

    // Options that must be the same for all points we're texturing at once
    int firstchannel;       ///< First channel of the lookup
    int nchannels;          ///< Number of channels to look up: 1 or 3
    Wrap swrap;             ///< Wrap mode in the s direction
    Wrap twrap;             ///< Wrap mode in the t direction

    // Options that may be different for each point we're texturing
    VaryingRef<float> sblur, tblur;   ///< Blur amount
    VaryingRef<float> swidth, twidth; ///< Multiplier for derivatives
    VaryingRef<float> bias;           ///< Bias
    VaryingRef<float> fill;           ///< Fill value for missing channels
    VaryingRef<int>   samples;        ///< Number of samples

    // For 3D volume texture lookups only:
    Wrap zwrap;                ///< Wrap mode in the z direction
    VaryingRef<float> zblur;   ///< Blur amount in the z direction
    VaryingRef<float> zwidth;  ///< Multiplier for derivatives in z direction

    // Storage for results
    VaryingRef<float> alpha;   ///< If non-null put the alpha channel here

    /// Utility: Return the Wrap enum corresponding to a wrap name:
    /// "default", "black", "clamp", "periodic", "mirror".
    static Wrap decode_wrapmode (const char *name);

    /// Utility: Parse a single wrap mode (e.g., "periodic") or a
    /// comma-separated wrap modes string (e.g., "black,clamp") into
    /// separate Wrap enums for s and t.
    static void parse_wrapmodes (const char *wrapmodes,
                                 TextureOptions::Wrap &swrapcode,
                                 TextureOptions::Wrap &twrapcode);
    

    /// Special private ctr that makes a canonical default TextureOptions.
    /// For use internal to libtexture.  Users, don't call this!
    /// Though, there is no harm.  It's just not as efficient as the
    /// default ctr that memcpy's a canonical pre-constructed default.
    TextureOptions (bool);

private:
    // Options set INTERNALLY by libtexture after the options are passed
    // by the user.  Users should not attempt to alter these!
    bool stateful;         // False for a new-ish TextureOptions
    int actualchannels;    // True number of channels read
    typedef bool (*wrap_impl) (int &coord, int width);
    wrap_impl swrap_func, twrap_func;
    friend class OpenImageIO::pvt::TextureSystemImpl;
};



class TextureSystem {
public:
    static TextureSystem *create ();
    static void destroy (TextureSystem * &x);

    TextureSystem (void) { }
    virtual ~TextureSystem () { }

    /// Close everything, free resources, start from scratch.
    ///
    virtual void clear () = 0;

    // Set options
    virtual void max_open_files (int nfiles) = 0;
    virtual void max_memory_MB (float size) = 0;
    virtual void searchpath (const std::string &path) = 0;
    virtual void worldtocommon (const float *mx) = 0;
    void worldtocommon (const Imath::M44f &w2c) {
        worldtocommon ((const float *)&w2c);
    }

    // Retrieve options
    virtual int max_open_files () const = 0;
    virtual float max_memory_MB () const = 0;
    virtual std::string searchpath () const = 0;

    /// Filtered 2D texture lookup for a single point.
    ///
    /// s,t are the texture coordinates; dsdx, dtdx, dsdy, and dtdy are
    /// the differentials of s and t change in some canonical directions
    /// x and y.  The choice of x and y are not important to the
    /// implementation; it can be any imposed 2D coordinates, such as
    /// pixels in screen space, adjacent samples in parameter space on a
    /// surface, etc.
    virtual void texture (ustring filename, TextureOptions &options,
                          float s, float t, float dsdx, float dtdx,
                          float dsdy, float dtdy, float *result) = 0;

    /// Retrieve filtered (possibly anisotropic) texture lookups for
    /// several points at once.
    ///
    /// All of the VaryingRef parameters (and fields in options)
    /// describe texture lookup parameters at an array of positions.
    /// But this routine only computes them from indices i where
    /// firstactive <= i <= lastactive, and ONLY when runflags[i] is
    /// nonzero.
    virtual void texture (ustring filename, TextureOptions &options,
                          Runflag *runflags, int firstactive, int lastactive,
                          VaryingRef<float> s, VaryingRef<float> t,
                          VaryingRef<float> dsdx, VaryingRef<float> dtdx,
                          VaryingRef<float> dsdy, VaryingRef<float> dtdy,
                          float *result) = 0;

    /// Retrieve a 3D texture lookup at a single point.
    ///
    virtual void texture (ustring filename, TextureOptions &options,
                          const Imath::V3f &P,
                          const Imath::V3f &dPdx, const Imath::V3f &dPdy,
                          float *result) = 0;

    /// Retrieve a 3D texture lookup at many points at once.
    ///
    virtual void texture (ustring filename, TextureOptions &options,
                          Runflag *runflags, int firstactive, int lastactive,
                          VaryingRef<Imath::V3f> P,
                          VaryingRef<Imath::V3f> dPdx,
                          VaryingRef<Imath::V3f> dPdy,
                          float *result) = 0;

    /// Retrieve a shadow lookup for a single position P.
    ///
    virtual void shadow (ustring filename, TextureOptions &options,
                         const Imath::V3f &P, const Imath::V3f &dPdx,
                         const Imath::V3f &dPdy, float *result) = 0;

    /// Retrieve a shadow lookup for position P at many points at once.
    ///
    virtual void shadow (ustring filename, TextureOptions &options,
                         Runflag *runflags, int firstactive, int lastactive,
                         VaryingRef<Imath::V3f> P,
                         VaryingRef<Imath::V3f> dPdx,
                         VaryingRef<Imath::V3f> dPdy,
                         float *result) = 0;

    /// Retrieve an environment map lookup for direction R.
    ///
    virtual void environment (ustring filename, TextureOptions &options,
                              const Imath::V3f &R, const Imath::V3f &dRdx,
                              const Imath::V3f &dRdy, float *result) = 0;

    /// Retrieve an environment map lookup for direction R, for many
    /// points at once.
    virtual void environment (ustring filename, TextureOptions &options,
                              Runflag *runflags, int firstactive, int lastactive,
                              VaryingRef<Imath::V3f> R,
                              VaryingRef<Imath::V3f> dRdx,
                              VaryingRef<Imath::V3f> dRdy,
                              float *result) = 0;

    /// Get information about the given texture.  Return true if found
    /// and the data has been put in *data.  Return false if the texture
    /// doesn't exist, doesn't have the requested data, if the data
    /// doesn't match the type requested. or some other failure.
    virtual bool gettextureinfo (ustring filename, ustring dataname,
                                 TypeDesc datatype, void *data) = 0;
    
};


};  // end namespace OpenImageIO


#endif // TEXTURE_H
