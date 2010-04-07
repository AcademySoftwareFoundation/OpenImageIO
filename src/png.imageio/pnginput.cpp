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

#include <cstdio>
#include <cstdlib>
#include <cmath>

#include <boost/algorithm/string.hpp>
using boost::algorithm::iequals;
#include <OpenEXR/ImathColor.h>

#include "png_pvt.h"

#include "dassert.h"
#include "typedesc.h"
#include "imageio.h"
#include "thread.h"
#include "strutil.h"
#include "fmath.h"

using namespace OpenImageIO;



class PNGInput : public ImageInput {
public:
    PNGInput () { init(); }
    virtual ~PNGInput () { close(); }
    virtual const char * format_name (void) const { return "png"; }
    virtual bool open (const std::string &name, ImageSpec &newspec);
    virtual bool close ();
    virtual int current_subimage (void) const { return m_subimage; }
    virtual bool read_native_scanline (int y, int z, void *data);

private:
    std::string m_filename;           ///< Stash the filename
    FILE *m_file;                     ///< Open image handle
    png_structp m_png;                ///< PNG read structure pointer
    png_infop m_info;                 ///< PNG image info structure pointer
    int m_bit_depth;                  ///< PNG bit depth
    int m_color_type;                 ///< PNG color model type
    std::vector<unsigned char> m_buf; ///< Buffer the image pixels
    int m_subimage;                   ///< What subimage are we looking at?
    Imath::Color3f m_bg;              ///< Background color

    /// Reset everything to initial state
    ///
    void init () {
        m_subimage = -1;
        m_file = NULL;
        m_png = NULL;
        m_info = NULL;
        m_buf.clear ();
    }

    /// Helper function: read the image.
    ///
    bool readimg ();

    /// Extract the background color.
    ///
    bool get_background (float *red, float *green, float *blue);
};



// Obligatory material to make this a recognizeable imageio plugin:
extern "C" {

DLLEXPORT ImageInput *png_input_imageio_create () { return new PNGInput; }

DLLEXPORT int png_imageio_version = OPENIMAGEIO_PLUGIN_VERSION;

DLLEXPORT const char * png_input_extensions[] = {
    "png", NULL
};

};



bool
PNGInput::open (const std::string &name, ImageSpec &newspec)
{
    m_filename = name;
    m_subimage = 0;

    m_file = fopen (name.c_str(), "rb");
    if (! m_file) {
        error ("Could not open file \"%s\"", name.c_str());
        return false;
    }

    unsigned char sig[8];
    fread (sig, 1, sizeof(sig), m_file);
    if (png_sig_cmp (sig, 0, 7)) {
        error ("File failed PNG signature check");
        return false;
    }

    std::string s = PNG_pvt::create_read_struct (m_png, m_info);
    if (s.length ()) {
        close ();
        error ("%s", s.c_str ());
        return false;
    }

    png_init_io (m_png, m_file);
    png_set_sig_bytes (m_png, 8);  // already read 8 bytes

    PNG_pvt::read_info (m_png, m_info, m_bit_depth, m_color_type, m_bg,
                        m_spec);

    newspec = spec ();
    return true;
}



bool
PNGInput::readimg ()
{
    std::string s = PNG_pvt::read_into_buffer (m_png, m_info, m_spec,
                                               m_bit_depth, m_color_type,
                                               m_buf);
    if (s.length ()) {
        close ();
        error ("%s", s.c_str ());
        return false;
    }

    return true;
}



bool
PNGInput::close ()
{
    PNG_pvt::destroy_read_struct (m_png, m_info);
    if (m_file) {
        fclose (m_file);
        m_file = NULL;
    }

    init();  // Reset to initial state
    return true;
}



template <class T>
static void 
associateAlpha (T * data, int size, int channels, int alpha_channel, float gamma)
{
    T max = std::numeric_limits<T>::max();
    if (gamma == 1) {
        for (int x = 0;  x < size;  ++x, data += channels)
            for (int c = 0;  c < channels;  c++)
                if (c != alpha_channel){
                    unsigned int f = data[c];
                    data[c] = (f * data[alpha_channel]) / max;
                }
    }
    else { //With gamma correction
        float inv_gamma = 1.0 / gamma;
        for (int x = 0;  x < size;  ++x, data += channels)
            for (int c = 0;  c < channels;  c++)
                if (c != alpha_channel){
                    //FIXME: Would it be worthwhile to do some caching on pow values?
                    float f = pow(data[c], inv_gamma); //Linearize
                    f = (f * data[alpha_channel]) / max;
                    data[c] = pow(f, gamma);
                }
    }
}



bool
PNGInput::read_native_scanline (int y, int z, void *data)
{
    if (m_buf.empty ())
        readimg ();

    y -= m_spec.y;
    size_t size = spec().scanline_bytes();
    memcpy (data, &m_buf[0] + y * size, size);

    // PNG specifically dictates unassociated (un-"premultiplied") alpha
    if (m_spec.alpha_channel != -1) {   // Associate alpha
        if (m_spec.format == TypeDesc::UINT16)
            associateAlpha ((unsigned short *)data, m_spec.width,
                            m_spec.nchannels, m_spec.alpha_channel, 
                            m_spec.gamma);
        else
            associateAlpha ((unsigned char *)data, m_spec.width,
                            m_spec.nchannels, m_spec.alpha_channel, 
                            m_spec.gamma);
    }

    return true;
}
