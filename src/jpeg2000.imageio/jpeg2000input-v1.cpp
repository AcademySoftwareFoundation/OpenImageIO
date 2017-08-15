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
#include <cstdio>
#include <vector>
#include <openjpeg.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>



OIIO_PLUGIN_NAMESPACE_BEGIN

namespace {

void openjpeg_dummy_callback(const char*, void*) {}

// TODO(sergey): This actually a stright duplication from the png reader,
// consider de-duplicating the code somehow?
template <class T>
void
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

}  // namespace


class Jpeg2000Input final : public ImageInput {
 public:
    Jpeg2000Input () { init (); }
    virtual ~Jpeg2000Input () { close (); }
    virtual const char *format_name (void) const { return "jpeg2000"; }
    virtual int supports (string_view feature) const {
        return false;
        // FIXME: we should support Exif/IPTC, but currently don't.
    }
    virtual bool open (const std::string &name, ImageSpec &spec);
    virtual bool open (const std::string &name, ImageSpec &newspec,
                       const ImageSpec &config);
    virtual bool close (void);
    virtual bool read_native_scanline (int y, int z, void *data);

 private:
    std::string m_filename;
    std::vector<int> m_bpp;   // per channel bpp
    opj_image_t *m_image;
    FILE *m_file;
    bool m_keep_unassociated_alpha;   // Do not convert unassociated alpha

    void init (void);

    bool isJp2File(const int* const p_magicTable) const;

    opj_dinfo_t* create_decompressor();

    template<typename T>
    void read_scanline(int y, int z, void *data);

    uint16_t baseTypeConvertU10ToU16(int src)
    {
        return (uint16_t)((src << 6) | (src >> 4));
    }

    uint16_t baseTypeConvertU12ToU16(int src)
    {
        return (uint16_t)((src << 4) | (src >> 8));
    }

    size_t get_file_length(FILE *p_file)
    {
        fseek(p_file, 0, SEEK_END);
        const size_t fileLength = ftell(p_file);
        rewind(m_file);
        return fileLength;
    }

    template<typename T>
    void yuv_to_rgb(T *p_scanline)
    {
        for (int x = 0, i = 0; x < m_spec.width; ++x, i += m_spec.nchannels) {
            float y = convert_type<T,float>(p_scanline[i+0]);
            float u = convert_type<T,float>(p_scanline[i+1])-0.5f;
            float v = convert_type<T,float>(p_scanline[i+2])-0.5f;
            float r = y + 1.402*v;
            float g = y - 0.344*u - 0.714*v;
            float b = y + 1.772*u;
            p_scanline[i+0] = convert_type<float,T>(r);
            p_scanline[i+1] = convert_type<float,T>(g);
            p_scanline[i+2] = convert_type<float,T>(b);
        } 
    }

    void setup_event_mgr(opj_event_mgr_t& event_mgr, opj_dinfo_t* p_decompressor)
    {
        event_mgr.error_handler = openjpeg_dummy_callback;
        event_mgr.warning_handler = openjpeg_dummy_callback;
        event_mgr.info_handler = openjpeg_dummy_callback;
        opj_set_event_mgr((opj_common_ptr) p_decompressor, &event_mgr, NULL);
    }

    bool fread (void *p_buf, size_t p_itemSize, size_t p_nitems)
    {
        size_t n = ::fread (p_buf, p_itemSize, p_nitems, m_file);
        if (n != p_nitems)
            error ("Read error");
        return n == p_nitems;
    }
};


// Obligatory material to make this a recognizeable imageio plugin
OIIO_PLUGIN_EXPORTS_BEGIN

    OIIO_EXPORT int jpeg2000_imageio_version = OIIO_PLUGIN_VERSION;
    OIIO_EXPORT const char* jpeg2000_imageio_library_version () {
        return ustring::format("OpenJpeg %s", opj_version()).c_str();
    }
    OIIO_EXPORT ImageInput *jpeg2000_input_imageio_create () {
        return new Jpeg2000Input;
    }
    OIIO_EXPORT const char *jpeg2000_input_extensions[] = {
        "jp2", "j2k", "j2c", NULL
    };

OIIO_PLUGIN_EXPORTS_END


void
Jpeg2000Input::init (void)
{
    m_file = NULL;
    m_image = NULL;
    m_keep_unassociated_alpha = false;
}


bool
Jpeg2000Input::open (const std::string &p_name, ImageSpec &p_spec)
{
    m_filename = p_name;
    m_file = Filesystem::fopen(m_filename, "rb");
    if (!m_file) {
        error ("Could not open file \"%s\"", m_filename.c_str());
        return false;
    }

    opj_dinfo_t* decompressor = create_decompressor();
    if (!decompressor) {
        error ("Could not create Jpeg2000 stream decompressor");
        close();
        return false;
    }

    opj_event_mgr_t event_mgr;
    setup_event_mgr(event_mgr, decompressor);

    opj_dparameters_t parameters;
    opj_set_default_decoder_parameters(&parameters);
    opj_setup_decoder(decompressor, &parameters);

    const size_t fileLength = get_file_length(m_file);
    std::vector<uint8_t> fileContent(fileLength+1, 0);
    fread(&fileContent[0], sizeof(uint8_t), fileLength);

    opj_cio_t *cio = opj_cio_open((opj_common_ptr)decompressor, &fileContent[0], (int) fileLength);
    if (!cio) { 
        error ("Could not open Jpeg2000 stream");
        opj_destroy_decompress(decompressor);
        close();
        return false;
    }

    m_image = opj_decode(decompressor, cio);
    opj_cio_close(cio);
    opj_destroy_decompress(decompressor);
    if (!m_image) {
        error ("Could not decode Jpeg2000 stream");
        close();
        return false;
    }

    // we support only one, three or four components in image
    const int channelCount = m_image->numcomps;
    if (channelCount != 1 && channelCount != 3 && channelCount != 4) {
        error ("Only images with one, three or four components are supported");
        close();
        return false;
    }

    int maxPrecision = 0;
    ROI datawindow;
    m_bpp.clear ();
    m_bpp.reserve (channelCount);
    std::vector<TypeDesc> chantypes (channelCount, TypeDesc::UINT8);
    for (int i = 0; i < channelCount; i++) {
        const opj_image_comp_t &comp (m_image->comps[i]);
        m_bpp.push_back (comp.prec);
        maxPrecision = std::max(comp.prec, maxPrecision);
        ROI roichan (comp.x0, comp.x0+comp.w*comp.dx,
                     comp.y0, comp.y0+comp.h*comp.dy);
        datawindow = roi_union (datawindow, roichan);
        // std::cout << "  chan " << i << "\n";
        // std::cout << "     dx=" << comp.dx << " dy=" << comp.dy
        //           << " x0=" << comp.x0 << " y0=" << comp.y0
        //           << " w=" << comp.w << " h=" << comp.h
        //           << " prec=" << comp.prec << " bpp=" << comp.bpp << "\n";
        // std::cout << "     sgnd=" << comp.sgnd << " resno_decoded=" << comp.resno_decoded << " factor=" << comp.factor << "\n";
        // std::cout << "     roichan=" << roichan << "\n";
    }
    // std::cout << "overall x0=" << m_image->x0 << " y0=" << m_image->y0
    //           << " x1=" << m_image->x1 << " y1=" << m_image->y1 << "\n";
    // std::cout << "color_space=" << m_image->color_space << "\n";
    const TypeDesc format = (maxPrecision <= 8) ? TypeDesc::UINT8
                                                : TypeDesc::UINT16;

    m_spec = ImageSpec (datawindow.width(), datawindow.height(),
                        channelCount, format);
    m_spec.x = datawindow.xbegin;
    m_spec.y = datawindow.ybegin;
    m_spec.full_x = m_image->x0;
    m_spec.full_y = m_image->y0;
    m_spec.full_width  = m_image->x1;
    m_spec.full_height = m_image->y1;

    m_spec.attribute ("oiio:BitsPerSample", maxPrecision);
    m_spec.attribute ("oiio:Orientation", 1);
    m_spec.attribute ("oiio:ColorSpace", "sRGB");
#ifndef OPENJPEG_VERSION
    // Sigh... openjpeg.h doesn't seem to have a clear version #define.
    // OPENJPEG_VERSION only seems to exist in 1.3, which doesn't have
    // the ICC fields. So assume its absence in the newer one (at least,
    // 1.5) means the field is valid.
    if (m_image->icc_profile_len && m_image->icc_profile_buf)
        m_spec.attribute ("ICCProfile", TypeDesc(TypeDesc::UINT8,m_image->icc_profile_len),
                          m_image->icc_profile_buf);
#endif

    p_spec = m_spec;
    return true;
}



bool
Jpeg2000Input::open (const std::string &name, ImageSpec &newspec,
                     const ImageSpec &config)
{
    // Check 'config' for any special requests
    if (config.get_int_attribute("oiio:UnassociatedAlpha", 0) == 1)
        m_keep_unassociated_alpha = true;
    return open (name, newspec);
}


bool
Jpeg2000Input::read_native_scanline (int y, int z, void *data)
{
    if (m_spec.format == TypeDesc::UINT8)
        read_scanline<uint8_t>(y, z, data);
    else
        read_scanline<uint16_t>(y, z, data);

    // JPEG2000 specifically dictates unassociated (un-"premultiplied") alpha.
    // Convert to associated unless we were requested not to do so.
    if (m_spec.alpha_channel != -1 && !m_keep_unassociated_alpha) {
        float gamma = m_spec.get_float_attribute ("oiio:Gamma", 2.2f);
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



inline bool
Jpeg2000Input::close (void)
{
    if (m_file) {
        fclose(m_file);
        m_file = NULL;
    }
    if (m_image) {
        opj_image_destroy(m_image);
        m_image = NULL;
    }
    return true;
}


bool Jpeg2000Input::isJp2File(const int* const p_magicTable) const
{
    const int32_t JP2_MAGIC = 0x0000000C, JP2_MAGIC2 = 0x0C000000;
    if (p_magicTable[0] == JP2_MAGIC || p_magicTable[0] == JP2_MAGIC2) {
        const int32_t JP2_SIG1_MAGIC = 0x6A502020, JP2_SIG1_MAGIC2 = 0x2020506A;
        const int32_t JP2_SIG2_MAGIC = 0x0D0A870A, JP2_SIG2_MAGIC2 = 0x0A870A0D;
        if ((p_magicTable[1] == JP2_SIG1_MAGIC || p_magicTable[1] == JP2_SIG1_MAGIC2)
            &&  (p_magicTable[2] == JP2_SIG2_MAGIC || p_magicTable[2] == JP2_SIG2_MAGIC2))
	{
            return true;
        }
    }
    return false;
}


opj_dinfo_t*
Jpeg2000Input::create_decompressor()
{
    int magic[3];
    if (::fread (&magic, 4, 3, m_file) != 3) {
        error ("Empty file \"%s\"", m_filename.c_str());
        return NULL;
    }
    opj_dinfo_t* dinfo = NULL;
    if (isJp2File(magic))
        dinfo = opj_create_decompress(CODEC_JP2);
    else
        dinfo = opj_create_decompress(CODEC_J2K);
    rewind(m_file);
    return dinfo;
}



template<typename T>
void
Jpeg2000Input::read_scanline(int y, int z, void *data)
{
    T* scanline = static_cast<T*>(data);
    int nc = m_spec.nchannels;
    // It's easier to loop over channels
    int bits = sizeof(T)*8;
    for (int c = 0; c < nc; ++c) {
        const opj_image_comp_t &comp (m_image->comps[c]);
        int chan_ybegin = comp.y0, chan_yend = comp.y0 + comp.h*comp.dy;
        int chan_xend = comp.w * comp.dx;
        int yoff = (y - comp.y0) / comp.dy;
        for (int x = 0;  x < m_spec.width;  ++x) {
            if (yoff < chan_ybegin || yoff >= chan_yend || x > chan_xend) {
                // Outside the window of this channel
                scanline[x*nc+c] = T(0);
            } else {
                unsigned int val = comp.data[yoff*comp.w + x/comp.dx];
                if (comp.sgnd)
                    val += (1<<(bits/2-1));
                scanline[x*nc+c] = (T) bit_range_convert (val, comp.prec, bits);
            }
        }
    }
    if (m_image->color_space == CLRSPC_SYCC)
        yuv_to_rgb(scanline);
}


OIIO_PLUGIN_NAMESPACE_END

