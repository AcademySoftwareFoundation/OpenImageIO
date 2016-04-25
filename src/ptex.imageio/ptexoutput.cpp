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

#include <Ptexture.h>

#include "OpenImageIO/typedesc.h"
#include "OpenImageIO/imageio.h"
#include "OpenImageIO/fmath.h"

OIIO_PLUGIN_NAMESPACE_BEGIN


class PtexOutput : public ImageOutput {
public:
    PtexOutput ();
    virtual ~PtexOutput ();
    virtual const char * format_name (void) const { return "ptex"; }
    virtual int supports (string_view feature) const;
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

OIIO_EXPORT ImageOutput *ptex_output_imageio_create () { return new PtexOutput; }

// OIIO_EXPORT int ptex_imageio_version = OIIO_PLUGIN_VERSION;   // it's in ptexinput.cpp

OIIO_EXPORT const char * ptex_output_extensions[] = {
    "ptex", "ptx", NULL
};

OIIO_PLUGIN_EXPORTS_END



PtexOutput::PtexOutput ()
{
    init ();
}



PtexOutput::~PtexOutput ()
{
    // Close, if not already done.
    close ();
}



int
PtexOutput::supports (string_view feature) const
{
    return (feature == "tiles"
         || feature == "multiimage"
         || feature == "mipmap"
         || feature == "alpha"
         || feature == "nchannels"
         || feature == "arbitrary_metadata"
         || feature == "exif"   // Because of arbitrary_metadata
         || feature == "iptc"); // Because of arbitrary_metadata
}



bool
PtexOutput::open (const std::string &name, const ImageSpec &userspec,
                  ImageOutput::OpenMode mode)
{
    error ("Ptex writer is not implemented yet, please poke Larry.");
    return false;
}



bool
PtexOutput::close ()
{
    init();
    return true;
}



bool
PtexOutput::write_scanline (int y, int z, TypeDesc format,
                            const void *data, stride_t xstride)
{
    error ("Ptex writer is not implemented yet, please poke Larry.");
    return false;
}

OIIO_PLUGIN_NAMESPACE_END

