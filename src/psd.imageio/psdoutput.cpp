/*
  Copyright 2011 Larry Gritz and the other authors and contributors.
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

#include <fstream>
#include <iostream>

#include "imageio.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

class PSDOutput : public ImageOutput {
public:
    PSDOutput ();
    virtual ~PSDOutput ();
    virtual const char * format_name (void) const { return "psd"; }
    virtual bool supports (const std::string &feature) const {
        // Support nothing nonstandard
        return false;
    }
    virtual bool open (const std::string &name, const ImageSpec &spec,
                       OpenMode mode=Create);
    virtual bool close ();
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride);

private:
    std::string m_filename;           ///< Stash the filename
    std::ofstream m_file;             ///< Open image handle

    // Initialize private members to pre-opened state
    void init (void) {
    }

};

// Obligatory material to make this a recognizeable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput *psd_output_imageio_create () { return new PSDOutput; }

OIIO_EXPORT const char * psd_output_extensions[] = {
    "psd", NULL
};

OIIO_PLUGIN_EXPORTS_END



PSDOutput::PSDOutput ()
{
    init ();
}



PSDOutput::~PSDOutput ()
{
    // Close, if not already done.
    close ();
}



bool
PSDOutput::open (const std::string &name, const ImageSpec &userspec,
                 OpenMode mode)
{
    return false;
}



bool
PSDOutput::close ()
{
    init ();
    return false;
}



bool
PSDOutput::write_scanline (int y, int z, TypeDesc format,
                            const void *data, stride_t xstride)
{
    return false;
}

OIIO_PLUGIN_NAMESPACE_END

