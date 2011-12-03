/*
  Copyright 2010 Larry Gritz and the other authors and contributors.
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

#ifndef OPENIMAGEIO_COLOR_H
#define OPENIMAGEIO_COLOR_H

#include "export.h"
#include "version.h"

OIIO_NAMESPACE_ENTER
{

/// The ColorProcessor encapsulates a baked color transformation, suitable for
/// application to raw pixels, or ImageBuf(s). These are generated using
/// ColorConfig::createColorProcessor, and referenced in ImageBufAlgo
/// (amongst other places)

class DLLPUBLIC ColorProcessor;



/// Represents the set of all color transformations that are allowed.
/// If OpenColorIO is enabled at build time, this configuration is loaded
/// at runtime, allowing the user to have complete control of all color
/// transformation math. ($OCIO)  (See opencolorio.org for details).
/// If OpenColorIO is not enabled at build time, a generic color configuration
/// is provided for minimal color support.
///
/// NOTE: ColorConfig(s) and ColorProcessor(s) are potentially heavy-weight.
/// Their construction / destruction should be kept to a minimum.

class DLLPUBLIC ColorConfig
{
public:
    /// If OpenColorIO is enabled at build time, initialize with the current
    /// color configuration. ($OCIO)
    /// If OpenColorIO is not enabled, this does nothing.
    ///
    /// Multiple calls to this are inexpensive.
    ColorConfig();
    
    /// If OpenColorIO is enabled at build time, initialize with the 
    /// specified color configuration (.ocio) file
    /// If OpenColorIO is not enabled, this will result in an error.
    /// 
    /// Multiple calls to this are potentially expensive.
    ColorConfig(const char * filename);
    
    ~ColorConfig();
    
    /// Has an error string occurred?
    /// (This will not affect the error state.)
    bool error () const;
    
    /// This routine will return the error string (and clear any error
    /// flags).  If no error has occurred since the last time geterror()
    /// was called, it will return an empty string.
    std::string geterror ();
    
    /// Get the number of ColorSpace(s) defined in this configuration
    int getNumColorSpaces() const;
    
    /// Query the name of the specified ColorSpace
    const char * getColorSpaceNameByIndex(int index) const;
    
    /// Given the specified input and output ColorSpace, construct the
    /// processor.  It is possible that this will return NULL, if the
    /// inputColorSpace doesnt exist, the outputColorSpace doesn't exist,
    /// or if the specified transformation is illegal (for example, it may
    /// require the inversion of a 3D-LUT, etc).   When the user is finished
    /// with a ColorProcess, deleteColorProcessor should be called.
    /// ColorProcessor(s) remain valid even if the ColorConfig that created
    /// them no longer exists.
    /// 
    /// Multiple calls to this are potentially expensive.
    
    ColorProcessor* createColorProcessor(const char * inputColorSpace,
                                         const char * outputColorSpace) const;
    
    /// Delete the specified ColorProcessor
    static void deleteColorProcessor(ColorProcessor * processor);
    
    /// Return if OpenImageIO was built with OCIO support
    static bool supportsOpenColorIO();
    
    
    
    
    private:
    ColorConfig(const ColorConfig &);
    ColorConfig& operator= (const ColorConfig &);
    
    class Impl;
    friend class Impl;
    Impl * m_impl;
    Impl * getImpl() { return m_impl; }
    const Impl * getImpl() const { return m_impl; }
};

}
OIIO_NAMESPACE_EXIT

#endif // OPENIMAGEIO_COLOR_H
