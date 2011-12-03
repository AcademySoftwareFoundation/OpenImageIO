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

class DLLPUBLIC ColorProcessor;

/// Represents the current ColorConfiguration. This will rely on OpenColorIO,
/// if enabled at build time.
/// WARNING: ColorConfig and ColorProcessor are potentially heavy-weight.
/// Their construction / destruction should be kept to an absolute minimum.

class DLLPUBLIC ColorConfig
{
public:
    static bool supportsOpenColorIO();
    
    /// Initialize with the current color configuration. ($OCIO)
    /// If OpenImageIO was not build with OCIO support, this will print
    /// a warning to the shell, and continue.
    /// Get the global OpenColorIO config
    /// This will auto-initialize (using $OCIO) on first use
    /// Multiple calls to this are immediate.
    ColorConfig();
    
    /// Repeated calls to this do not cache.
    ColorConfig(const char * filename);
    
    ~ColorConfig();
    
    /// This routine will return the error string (and clear any error
    /// flags).  If no error has occurred since the last time geterror()
    /// was called, it will return an empty string.
    
    std::string geterror ();
    
    /// Is the error string set? Will not clear it.
    bool error () const;
    
    
    int getNumColorSpaces() const;
    const char * getColorSpaceNameByIndex(int index) const;
    
    ColorProcessor* createColorProcessor(const char * inputColorSpace,
                                         const char * outputColorSpace) const;
    
    static void deleteColorProcessor(ColorProcessor * processor);
    
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
