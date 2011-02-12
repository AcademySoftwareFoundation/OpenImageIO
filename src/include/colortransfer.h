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

#ifndef OPENIMAGEIO_COLORTRANSFER_H
#define OPENIMAGEIO_COLORTRANSFER_H

#include "export.h"
#include "version.h"

OIIO_NAMESPACE_ENTER
{

/// Base class a functor that remaps values accorrding to a color
/// transfer function.
class DLLPUBLIC ColorTransfer {
public:
    ColorTransfer (std::string name_) { m_name = name_; };
    virtual ~ColorTransfer (void) { };
    
    /// Return the name of the color transfer function, e.g., "sRGB_to_linear",
    /// "linear_to_Rec709", etc.
    const std::string name (void) { return m_name; };
    
    /// Return a vector of transfer paramater names
    ///
    const std::vector<std::string> & paramaters (void) { return m_params; };
    
    /// Set a transfer function paramater If the name is not recognized,
    /// return false.
    virtual bool set (std::string name_, float param);
    
    /// Get a transfer function paramater
    /// If the name is not recognized, return false.
    virtual bool get (std::string name_, float &param);
    
    /// Evalutate the transfer function.
    virtual float operator() (float x) = 0;
    
    /// This static function allocates and returns an instance of the
    /// specific color transfer implementation for the name you provide.
    /// Example use:
    ///     ColorTransfer *mytf = ColorTransfer::create ("KodakLog_to_linear");
    /// The caller is responsible for deleting it when it's done.
    /// If the name is not recognized, return NULL.
    static ColorTransfer *create (const std::string &name);
    
protected:
    std::string m_name;
    std::vector<std::string> m_params;
    
    // Add transfer function paramater
    bool add_paramater (const std:: string &name_) {
        m_params.push_back (name_);
        return true;
    };
    
};


}
OIIO_NAMESPACE_EXIT

#endif // OPENIMAGEIO_COLORTRANSFER_H
