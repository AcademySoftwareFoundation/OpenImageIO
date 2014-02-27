/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
  All Rights Reserved.
  Based on BSD-licensed software Copyright 2004 NVIDIA Corp.

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


#ifndef OPENIMAGEIO_FILTER_H
#define OPENIMAGEIO_FILTER_H

#include "oiioversion.h"
#include "export.h"

OIIO_NAMESPACE_ENTER
{

/// Quick structure that describes a filter.
///
class OIIO_API FilterDesc {
public:
    const char *name; ///< name of the filter
    int dim;          ///< dimensionality: 1 or 2 
    float width;      ///< Recommended width or window
    bool fixedwidth;  ///< Is the width the only one that makes sense?
    bool scalable;    ///< Is it scalable (otherwise, the width is a window)?
    bool separable;   ///< Is it separable?  (only matters if dim==2)
};



/// Filter1D is the abstract data type for a 1D filter.
/// The filters are NOT expected to have their results normalized.
class OIIO_API Filter1D {
public:
    Filter1D (float width) : m_w(width) { }
    virtual ~Filter1D (void) { };

    /// Get the width of the filter
    float width (void) const { return m_w; }

    /// Evalutate the filter at an x position (relative to filter center)
    virtual float operator() (float x) const = 0;

    /// Return the name of the filter, e.g., "box", "gaussian"
    virtual const std::string name (void) const = 0;

    /// This static function allocates and returns an instance of the
    /// specific filter implementation for the name you provide.
    /// Example use:
    ///        Filter1D *myfilt = Filter1::create ("box", 1);
    /// The caller is responsible for deleting it when it's done.
    /// If the name is not recognized, return NULL.
    static Filter1D *create (const std::string &filtername, float width);

    /// Destroy a filter that was created with create().
    static void destroy (Filter1D *filt);

    /// Get the number of filters supported.
    static int num_filters ();
    /// Get the info for a particular index (0..num_filters()-1).
    static void get_filterdesc (int filternum, FilterDesc *filterdesc);

protected:
    float m_w;
};



/// Filter2D is the abstract data type for a 2D filter.
/// The filters are NOT expected to have their results normalized.
class OIIO_API Filter2D {
public:
    Filter2D (float width, float height) : m_w(width), m_h(height) { }
    virtual ~Filter2D (void) { };

    /// Get the width of the filter
    float width (void) const { return m_w; }
    /// Get the height of the filter
    float height (void) const { return m_h; }

    /// Is the filter separable?
    ///
    virtual bool separable () const { return false; }

    /// Evalutate the filter at an x and y position (relative to filter
    /// center).
    virtual float operator() (float x, float y) const = 0;

    /// Evaluate just the horizontal filter (if separable; for non-separable
    /// it just evaluates at (x,0).
    virtual float xfilt (float x) const { return (*this)(x,0.0f); }

    /// Evaluate just the vertical filter (if separable; for non-separable
    /// it just evaluates at (0,y).
    virtual float yfilt (float y) const { return (*this)(0.0f,y); }

    /// Return the name of the filter, e.g., "box", "gaussian"
    virtual const std::string name (void) const = 0;

    /// This static function allocates and returns an instance of the
    /// specific filter implementation for the name you provide.
    /// Example use:
    ///        Filter2D *myfilt = Filter2::create ("box", 1, 1);
    /// The caller is responsible for deleting it when it's done.
    /// If the name is not recognized, return NULL.
    static Filter2D *create (const std::string &filtername,
                             float width, float height);

    /// Destroy a filter that was created with create().
    static void destroy (Filter2D *filt);

    /// Get the number of filters supported.
    static int num_filters ();
    /// Get the info for a particular index (0..num_filters()-1).
    static void get_filterdesc (int filternum, FilterDesc *filterdesc);

protected:
    float m_w, m_h;
};


}
OIIO_NAMESPACE_EXIT

#endif // OPENIMAGEIO_FILTER_H
