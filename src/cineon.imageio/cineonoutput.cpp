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

#include "libcineon/Cineon.h"

#include "typedesc.h"
#include "imageio.h"
#include "fmath.h"

using namespace cineon;

OIIO_PLUGIN_NAMESPACE_BEGIN


class CineonOutput : public ImageOutput {
public:
    CineonOutput ();
    virtual ~CineonOutput ();
    virtual const char * format_name (void) const { return "cineon"; }
    virtual bool supports (const std::string &feature) const {
        // Support nothing nonstandard
        return false;
    }
    virtual bool open (const std::string &name, const ImageSpec &spec,
                       ImageOutput::OpenMode mode);
    virtual bool close ();
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride);

private:

    // Initialize private members to pre-opened state
    void init (void) {
    }
};




// Obligatory material to make this a recognizeable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput *cineon_output_imageio_create () { return new CineonOutput; }

// OIIO_EXPORT int cineon_imageio_version = OIIO_PLUGIN_VERSION;   // it's in cineoninput.cpp

OIIO_EXPORT const char * cineon_output_extensions[] = {
    "cin", NULL
};

OIIO_PLUGIN_EXPORTS_END



CineonOutput::CineonOutput ()
{
    init ();
}



CineonOutput::~CineonOutput ()
{
    // Close, if not already done.
    close ();
}



bool
CineonOutput::open (const std::string &name, const ImageSpec &userspec,
                    ImageOutput::OpenMode mode)
{
    error ("Cineon writer is not implemented yet, please poke Leszek in the "
           "mailing list");
    return false;
}



bool
CineonOutput::close ()
{
    init();  // Reset to initial state
    return true;  // How can we fail?
                  // Epicly. -- IneQuation
}



bool
CineonOutput::write_scanline (int y, int z, TypeDesc format,
                              const void *data, stride_t xstride)
{
    error ("Cineon writer is not implemented yet, please poke Leszek in the "
           "mailing list");
    return false;
}

OIIO_PLUGIN_NAMESPACE_END

