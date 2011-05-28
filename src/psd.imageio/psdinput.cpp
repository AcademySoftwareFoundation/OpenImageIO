/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
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

#include "psd_pvt.h"

#include "imageio.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

class PSDInput : public ImageInput {
public:
    PSDInput () { init(); }
    virtual ~PSDInput () { close(); }
    virtual const char * format_name (void) const { return "psd"; }
    virtual bool open (const std::string &name, ImageSpec &newspec);
    virtual bool close ();
    virtual bool read_native_scanline (int y, int z, void *data);

private:
    std::string m_filename;           ///< Stash the filename
    std::ifstream m_file;             ///< Open image handle
    psd_pvt::PSDFileHeader m_header;        ///< File header
    psd_pvt::PSDColorModeData m_color_mode; ///< Color mode data
    psd_pvt::PSDImageResourceSection m_image_resources; ///< Image resources section

    /// Reset everything to initial state
    ///
    void init () {
    }
};

// Obligatory material to make this a recognizeable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

DLLEXPORT ImageInput *psd_input_imageio_create () { return new PSDInput; }

DLLEXPORT int psd_imageio_version = OIIO_PLUGIN_VERSION;

DLLEXPORT const char * psd_input_extensions[] = {
    "psd", "pdd", "8bps", "psb", "8bpd", NULL
};

OIIO_PLUGIN_EXPORTS_END



bool
PSDInput::open (const std::string &name, ImageSpec &newspec)
{
    const char *err = NULL;
    m_filename = name;
    m_file.open (name.c_str(), std::ios::binary);
    if (!m_file.is_open ()) {
        error ("\"%s\": failed to open file", name.c_str());
        return false;
    }
    // file header
    err = m_header.read (m_file);
    if (err) {
        error ("\"%s\": [file header] %s", name.c_str(), err);
        return false;
    }
    // color mode data
    err = m_color_mode.read (m_file, m_header);
    if (err) {
        error ("\"%s\": [color mode data] %s", name.c_str(), err);
        return false;
    }
    // image resources
    err = m_image_resources.read (m_file);
    if (err) {
        error ("\"%s\": [image resources] %s", name.c_str(), err);
        return false;
    }
    return true;
}



bool
PSDInput::close ()
{
    init();  // Reset to initial state
    return false;
}



bool
PSDInput::read_native_scanline (int y, int z, void *data)
{
    return false;
}

OIIO_PLUGIN_NAMESPACE_END

