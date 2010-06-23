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

#include <cstdio>
#include <cstdlib>
#include <cmath>

#include "libdpx/DPX.h"

#include "dassert.h"
#include "typedesc.h"
#include "imageio.h"
#include "fmath.h"

using namespace OpenImageIO;



class DPXOutput : public ImageOutput {
public:
    DPXOutput ();
    virtual ~DPXOutput ();
    virtual const char * format_name (void) const { return "dpx"; }
    virtual bool supports (const std::string &feature) const {
        // Support nothing nonstandard
        return false;
    }
    virtual bool open (const std::string &name, const ImageSpec &spec,
                       bool append=false);
    virtual bool close ();
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride);

private:
    std::string m_filename;           ///< Stash the filename
    FILE *m_file;                     ///< Open image handle
    std::vector<unsigned char> m_scratch;

    // Initialize private members to pre-opened state
    void init (void) {
        m_file = NULL;
    }
};




// Obligatory material to make this a recognizeable imageio plugin:
extern "C" {

DLLEXPORT ImageOutput *dpx_output_imageio_create () { return new DPXOutput; }

// DLLEXPORT int dpx_imageio_version = OPENIMAGEIO_PLUGIN_VERSION;   // it's in dpxinput.cpp

DLLEXPORT const char * dpx_output_extensions[] = {
    "dpx", NULL
};

};



DPXOutput::DPXOutput ()
{
    init ();
}



DPXOutput::~DPXOutput ()
{
    // Close, if not already done.
    close ();
}



bool
DPXOutput::open (const std::string &name, const ImageSpec &userspec, bool append)
{
    close ();  // Close any already-opened file
    m_spec = userspec;  // Stash the spec

    m_file = fopen (name.c_str(), "wb");
    if (! m_file) {
        error ("Could not open file \"%s\"", name.c_str());
        return false;
    }

    error ("DPX writing is not supported yet, please poke Leszek in the "
        "mailing list");
    return false;
}



bool
DPXOutput::close ()
{
    if (m_file) {
        // close the stream
        fclose (m_file);
        m_file = NULL;
    }

    init ();      // re-initialize
    return true;  // How can we fail?
                  // Epicly. -- IneQuation
}



bool
DPXOutput::write_scanline (int y, int z, TypeDesc format,
                            const void *data, stride_t xstride)
{
    return true;
}
