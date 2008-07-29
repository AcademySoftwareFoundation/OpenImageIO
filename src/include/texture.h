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
    class Vector3f;
};

typedef unsigned char Runflag;



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

    TextureOptions () { }

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

private:
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
                          VaryingRef<Imath::Vector3f> P,
                          VaryingRef<Imath::Vector3f> dPdx,
                          VaryingRef<Imath::Vector3f> dPdy,
                          float *result) = 0;

    /// Retrieve a shadow lookup for position P.
    ///
    virtual void shadow (ustring filename, TextureOptions &options,
                         Runflag *runflags, int firstactive, int lastactive,
                         VaryingRef<Imath::Vector3f> P,
                         VaryingRef<Imath::Vector3f> dPdx,
                         VaryingRef<Imath::Vector3f> dPdy,
                         float *result) = 0;

    /// Retrieve an environment map lookup for direction R.
    ///
    virtual void environment (ustring filename, TextureOptions &options,
                              short *runflags, int firstactive, int lastactive,
                              VaryingRef<Imath::Vector3f> R,
                              VaryingRef<Imath::Vector3f> dRdx,
                              VaryingRef<Imath::Vector3f> dRdy,
                              float *result) = 0;

    /// Get information about the given texture.  Return true if found
    /// and the data has been put in *data.  Return false if the texture
    /// doesn't exist, doesn't have the requested data, if the data
    /// doesn't match the type requested. or some other failure.
    virtual bool gettextureinfo (ustring filename, ustring dataname,
                                 ParamType datatype, void *data) = 0;
    
};


#endif // TEXTURE_H
