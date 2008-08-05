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


typedef unsigned char Runflag;
enum RunFlagVal { RunFlagOff = 0, RunFlagOn = 255 };



class TextureOptions {
public:
    enum Wrap {
        WrapDefault,        ///< Use the default found in the file
        WrapBlack,          ///< Black outside [0..1]
        WrapClamp,          ///< Clamp to [0..1]
        WrapPeriodic,       ///< Periodic mod 1
        WrapMirror,         ///< Mirror the image
        WrapLast            ///< Mark the end
    };

    TextureOptions ();

    // Options that must be the same for all points we're texturing at once
    int firstchannel;
    int nchannels;
    Wrap swrap;
    Wrap twrap;

    // Options that may be different for each point we're texturing
    VaryingRef<ustring> filename;
    VaryingRef<float>   sblur, tblur;
    VaryingRef<float>   swidth, twidth;
    VaryingRef<float>   bias;
    VaryingRef<float>   fill;

    // Storage for results
    VaryingRef<float>   alpha;

    // Options set INTERNALLY by libtexture after the options are passed
    // by the user.  Users should not attempt to alter these!
    bool stateful;         // False for a new-ish TextureOptions
    int actualchannels;    // True number of channels read

    /// Special private ctr that makes a canonical default TextureOptions.
    /// For use internal to libtexture.  Users, don't call this!
    /// Though, there is no harm.  It's just not as efficient as the
    /// default ctr that memcpy's a canonical pre-constructed default.
    TextureOptions (bool);
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
    virtual void searchpath (const ustring &path) = 0;
    virtual void worldtocommon (const float *mx) = 0;
    void worldtocommon (const Imath::M44f &w2c) {
        worldtocommon ((const float *)&w2c);
    }

    // Retrieve options
    virtual int max_open_files () const = 0;
    virtual float max_memory_MB () const = 0;
    virtual ustring searchpath () const = 0;

    /// Retrieve filtered (possibly anisotropic) texture lookups for
    /// several points.  s,t are the texture coordinates; dsdx, dtdx,
    /// dsdy, and dtdy are the differentials of s and t change in some
    /// canonical directions x and y.  The choice of x and y are not
    /// important to the implementation; it can be any imposed 2D
    /// coordinates, such as pixels in screen space, adjacent samples in
    /// parameter space on a surface, etc.
    virtual void texture (ustring filename, TextureOptions &options,
                          Runflag *runflags, int firstactive, int lastactive,
                          VaryingRef<float> s, VaryingRef<float> t,
                          VaryingRef<float> dsdx, VaryingRef<float> dtdx,
                          VaryingRef<float> dsdy, VaryingRef<float> dtdy,
                          float *result) = 0;

    /// Retrieve a 3D texture lookup.
    ///
    virtual void texture (ustring filename, TextureOptions &options,
                          Runflag *runflags, int firstactive, int lastactive,
                          VaryingRef<Imath::V3f> P,
                          VaryingRef<Imath::V3f> dPdx,
                          VaryingRef<Imath::V3f> dPdy,
                          float *result) = 0;

    /// Retrieve a shadow lookup for position P.
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
                              short *runflags, int firstactive, int lastactive,
                              VaryingRef<Imath::V3f> R,
                              VaryingRef<Imath::V3f> dRdx,
                              VaryingRef<Imath::V3f> dRdy,
                              float *result) = 0;

    /// Get information about the given texture.  Return true if found
    /// and the data has been put in *data.  Return false if the texture
    /// doesn't exist, doesn't have the requested data, if the data
    /// doesn't match the type requested. or some other failure.
    virtual bool gettextureinfo (ustring filename, ustring dataname,
                                 ParamType datatype, void *data) = 0;
    

#if 0
    // Set an attribute in the graphics state, with name and explicit type
    virtual void attribute (const char *name, ParamType t, const void *val) {}
    virtual void attribute (const char *name, ParamType t, int val) {}
    virtual void attribute (const char *name, ParamType t, float val) {}
    virtual void attribute (const char *name, ParamType t, double val) {}
    virtual void attribute (const char *name, ParamType t, const char *val) {}
    virtual void attribute (const char *name, ParamType t, const int *val) {}
    virtual void attribute (const char *name, ParamType t, const float *val) {}
    virtual void attribute (const char *name, ParamType t, const char **val) {}

    // Set an attribute with type embedded in the name
    virtual void attribute (const char *typedname, const void *val) {}
    virtual void attribute (const char *typedname, int val) {}
    virtual void attribute (const char *typedname, float val) {}
    virtual void attribute (const char *typedname, double val) {}
    virtual void attribute (const char *typedname, const char *val) {}
    virtual void attribute (const char *typedname, const int *val) {}
    virtual void attribute (const char *typedname, const float *val) {}
    virtual void attribute (const char *typedname, const char **val) {}
#endif
};


};  // end namespace OpenImageIO


#endif // TEXTURE_H
