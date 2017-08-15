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

#include <OpenEXR/ImathColor.h>

#include "png_pvt.h"

OIIO_PLUGIN_NAMESPACE_BEGIN


class PNGInput final : public ImageInput {
public:
    PNGInput () { init(); }
    virtual ~PNGInput () { close(); }
    virtual const char * format_name (void) const { return "png"; }
    virtual bool valid_file (const std::string &filename) const;
    virtual bool open (const std::string &name, ImageSpec &newspec);
    virtual bool open (const std::string &name, ImageSpec &newspec,
                       const ImageSpec &config);
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
    int m_interlace_type;             ///< PNG interlace type
    std::vector<unsigned char> m_buf; ///< Buffer the image pixels
    int m_subimage;                   ///< What subimage are we looking at?
    Imath::Color3f m_bg;              ///< Background color
    int m_next_scanline;
    bool m_keep_unassociated_alpha;   ///< Do not convert unassociated alpha

    /// Reset everything to initial state
    ///
    void init () {
        m_subimage = -1;
        m_file = NULL;
        m_png = NULL;
        m_info = NULL;
        m_buf.clear ();
        m_next_scanline = 0;
        m_keep_unassociated_alpha = false;
    }

    /// Helper function: read the image.
    ///
    bool readimg ();

    /// Extract the background color.
    ///
    bool get_background (float *red, float *green, float *blue);
};



// Obligatory material to make this a recognizeable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageInput *png_input_imageio_create () { return new PNGInput; }

OIIO_EXPORT int png_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_EXPORT const char* png_imageio_library_version () {
    return "libpng " PNG_LIBPNG_VER_STRING;
}

OIIO_EXPORT const char * png_input_extensions[] = {
    "png", NULL
};

OIIO_PLUGIN_EXPORTS_END



bool
PNGInput::valid_file (const std::string &filename) const
{
    FILE *fd = Filesystem::fopen (filename, "rb");
    if (! fd)
        return false;
    unsigned char sig[8];
    bool ok = (fread (sig, 1, sizeof(sig), fd) == sizeof(sig) &&
               png_sig_cmp (sig, 0, 7) == 0);
    fclose (fd);
    return ok;
}



bool
PNGInput::open (const std::string &name, ImageSpec &newspec)
{
    m_filename = name;
    m_subimage = 0;

    m_file = Filesystem::fopen (name, "rb");
    if (! m_file) {
        error ("Could not open file \"%s\"", name.c_str());
        return false;
    }

    unsigned char sig[8];
    if (fread (sig, 1, sizeof(sig), m_file) != sizeof(sig)) {
        error ("Not a PNG file");
        return false;   // Read failed
    }

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

    PNG_pvt::read_info (m_png, m_info, m_bit_depth, m_color_type,
                        m_interlace_type, m_bg, m_spec,
                        m_keep_unassociated_alpha);

    newspec = spec ();
    m_next_scanline = 0;

    return true;
}



bool
PNGInput::open (const std::string &name, ImageSpec &newspec,
                const ImageSpec &config)
{
    // Check 'config' for any special requests
    if (config.get_int_attribute("oiio:UnassociatedAlpha", 0) == 1)
        m_keep_unassociated_alpha = true;
    return open (name, newspec);
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
        float inv_max = 1.0 / max;
        for (int x = 0;  x < size;  ++x, data += channels) {
            float alpha_associate = pow(data[alpha_channel]*inv_max, gamma);
            // We need to transform to linear space, associate the alpha, and
            // then transform back.  That is, if D = data[c], we want
            //
            // D' = max * ( (D/max)^(1/gamma) * (alpha/max) ) ^ gamma
            //
            // This happens to simplify to something which looks like
            // multiplying by a nonlinear alpha:
            //
            // D' = D * (alpha/max)^gamma
            for (int c = 0;  c < channels;  c++)
                if (c != alpha_channel)
                    data[c] = static_cast<T>(data[c] * alpha_associate);
        }
    }
}



bool
PNGInput::read_native_scanline (int y, int z, void *data)
{
    y -= m_spec.y;
    if (y < 0 || y >= m_spec.height)   // out of range scanline
        return false;

    if (m_interlace_type != 0) {
        // Interlaced.  Punt and read the whole image
        if (m_buf.empty ())
            readimg ();
        size_t size = spec().scanline_bytes();
        memcpy (data, &m_buf[0] + y * size, size);
    } else {
        // Not an interlaced image -- read just one row
        if (m_next_scanline > y) {
            // User is trying to read an earlier scanline than the one we're
            // up to.  Easy fix: close the file and re-open.
            ImageSpec dummyspec;
            int subimage = current_subimage();
            if (! close ()  ||
                ! open (m_filename, dummyspec)  ||
                ! seek_subimage (subimage, dummyspec))
                return false;    // Somehow, the re-open failed
            assert (m_next_scanline == 0 && current_subimage() == subimage);
        }
        while (m_next_scanline <= y) {
            // Keep reading until we're read the scanline we really need
            // std::cerr << "reading scanline " << m_next_scanline << "\n";
            std::string s = PNG_pvt::read_next_scanline (m_png, data);
            if (s.length ()) {
                close ();
                error ("%s", s.c_str ());
                return false;
            }
            ++m_next_scanline;
        }
    }

    // PNG specifically dictates unassociated (un-"premultiplied") alpha.
    // Convert to associated unless we were requested not to do so.
    if (m_spec.alpha_channel != -1 && !m_keep_unassociated_alpha) {
        float gamma = m_spec.get_float_attribute ("oiio:Gamma", 1.0f);
        if (m_spec.format == TypeDesc::UINT16)
            associateAlpha ((unsigned short *)data, m_spec.width,
                            m_spec.nchannels, m_spec.alpha_channel, 
                            gamma);
        else
            associateAlpha ((unsigned char *)data, m_spec.width,
                            m_spec.nchannels, m_spec.alpha_channel, 
                            gamma);
    }

    return true;
}

OIIO_PLUGIN_NAMESPACE_END

