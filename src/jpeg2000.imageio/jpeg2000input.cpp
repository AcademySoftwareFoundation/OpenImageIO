// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

#include <cstdio>
#include <vector>

#include <openjpeg.h>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imageio.h>



OIIO_PLUGIN_NAMESPACE_BEGIN

namespace {

static void
openjpeg_error_callback(const char* msg, void* data)
{
    if (ImageInput* input = (ImageInput*)data) {
        if (!msg || !msg[0])
            msg = "Unknown OpenJpeg error";
        input->errorf("%s", msg);
    }
}


static void
openjpeg_dummy_callback(const char* /*msg*/, void* /*data*/)
{
}



// TODO(sergey): This actually a straight duplication from the png reader,
// consider de-duplicating the code somehow?
template<class T>
void
associateAlpha(T* data, int size, int channels, int alpha_channel, float gamma)
{
    T max = std::numeric_limits<T>::max();
    if (gamma == 1) {
        for (int x = 0; x < size; ++x, data += channels)
            for (int c = 0; c < channels; c++)
                if (c != alpha_channel) {
                    unsigned int f = data[c];
                    data[c]        = (f * data[alpha_channel]) / max;
                }
    } else {  //With gamma correction
        float inv_max = 1.0 / max;
        for (int x = 0; x < size; ++x, data += channels) {
            float alpha_associate = pow(data[alpha_channel] * inv_max, gamma);
            // We need to transform to linear space, associate the alpha, and
            // then transform back.  That is, if D = data[c], we want
            //
            // D' = max * ( (D/max)^(1/gamma) * (alpha/max) ) ^ gamma
            //
            // This happens to simplify to something which looks like
            // multiplying by a nonlinear alpha:
            //
            // D' = D * (alpha/max)^gamma
            for (int c = 0; c < channels; c++)
                if (c != alpha_channel)
                    data[c] = static_cast<T>(data[c] * alpha_associate);
        }
    }
}

}  // namespace


class Jpeg2000Input final : public ImageInput {
public:
    Jpeg2000Input() { init(); }
    virtual ~Jpeg2000Input() { close(); }
    virtual const char* format_name(void) const override { return "jpeg2000"; }
    virtual int supports(string_view /*feature*/) const override
    {
        return false;
        // FIXME: we should support Exif/IPTC, but currently don't.
    }
    virtual bool open(const std::string& name, ImageSpec& spec) override;
    virtual bool open(const std::string& name, ImageSpec& newspec,
                      const ImageSpec& config) override;
    virtual bool close(void) override;
    virtual bool read_native_scanline(int subimage, int miplevel, int y, int z,
                                      void* data) override;

private:
    std::string m_filename;
    std::vector<int> m_bpp;  // per channel bpp
    opj_image_t* m_image;
    FILE* m_file;
    opj_codec_t* m_codec;
    opj_stream_t* m_stream;
    bool m_keep_unassociated_alpha;  // Do not convert unassociated alpha

    void init(void);

    bool isJp2File(const int* const p_magicTable) const;

    opj_codec_t* create_decompressor();
    void destroy_decompressor();

    void destroy_stream()
    {
        if (m_stream) {
            opj_stream_destroy(m_stream);
            m_stream = NULL;
        }
    }

    template<typename T> void read_scanline(int y, int z, void* data);

    uint16_t baseTypeConvertU10ToU16(int src)
    {
        return (uint16_t)((src << 6) | (src >> 4));
    }

    uint16_t baseTypeConvertU12ToU16(int src)
    {
        return (uint16_t)((src << 4) | (src >> 8));
    }

    template<typename T> void yuv_to_rgb(T* p_scanline)
    {
        for (int x = 0, i = 0; x < m_spec.width; ++x, i += m_spec.nchannels) {
            float y = convert_type<T, float>(p_scanline[i + 0]);
            float u = convert_type<T, float>(p_scanline[i + 1]) - 0.5f;
            float v = convert_type<T, float>(p_scanline[i + 2]) - 0.5f;
            float r = y + 1.402 * v;
            float g = y - 0.344 * u - 0.714 * v;
            float b = y + 1.772 * u;
            p_scanline[i + 0] = convert_type<float, T>(r);
            p_scanline[i + 1] = convert_type<float, T>(g);
            p_scanline[i + 2] = convert_type<float, T>(b);
        }
    }

    void setup_event_mgr(opj_codec_t* codec)
    {
        opj_set_error_handler(codec, openjpeg_error_callback, this);
        opj_set_warning_handler(codec, openjpeg_dummy_callback, NULL);
        opj_set_info_handler(codec, openjpeg_dummy_callback, NULL);
    }
};


// Obligatory material to make this a recognizeable imageio plugin
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT int jpeg2000_imageio_version = OIIO_PLUGIN_VERSION;
OIIO_EXPORT const char*
jpeg2000_imageio_library_version()
{
    return ustring::sprintf("OpenJpeg %s", opj_version()).c_str();
}
OIIO_EXPORT ImageInput*
jpeg2000_input_imageio_create()
{
    return new Jpeg2000Input;
}
OIIO_EXPORT const char* jpeg2000_input_extensions[] = { "jp2", "j2k", "j2c",
                                                        nullptr };

OIIO_PLUGIN_EXPORTS_END


void
Jpeg2000Input::init(void)
{
    m_file                    = NULL;
    m_image                   = NULL;
    m_codec                   = NULL;
    m_stream                  = NULL;
    m_keep_unassociated_alpha = false;
}


bool
Jpeg2000Input::open(const std::string& p_name, ImageSpec& p_spec)
{
    m_filename = p_name;
    if (!Filesystem::exists(m_filename)) {
        errorf("Could not open file \"%s\"", m_filename);
        return false;
    }

    m_codec = create_decompressor();
    if (!m_codec) {
        errorf("Could not create Jpeg2000 stream decompressor");
        close();
        return false;
    }

    setup_event_mgr(m_codec);

    opj_dparameters_t parameters;
    opj_set_default_decoder_parameters(&parameters);
    opj_setup_decoder(m_codec, &parameters);

#if defined(OPJ_VERSION_MAJOR)
    // OpenJpeg >= 2.1
    m_stream = opj_stream_create_default_file_stream(m_filename.c_str(), true);
#else
    // OpenJpeg 2.0: need to open a stream ourselves
    m_file   = Filesystem::fopen(m_filename, "rb");
    m_stream = opj_stream_create_default_file_stream(m_file, true);
#endif
    if (!m_stream) {
        errorf("Could not open Jpeg2000 stream");
        close();
        return false;
    }

    OIIO_ASSERT(m_image == nullptr);
    if (!opj_read_header(m_stream, m_codec, &m_image)) {
        errorf("Could not read Jpeg2000 header");
        close();
        return false;
    }
    opj_decode(m_codec, m_stream, m_image);

    destroy_decompressor();
    destroy_stream();

    // we support only one, three or four components in image
    const int channelCount = m_image->numcomps;
    if (channelCount != 1 && channelCount != 3 && channelCount != 4) {
        errorf("Only images with one, three or four components are supported");
        close();
        return false;
    }

    unsigned int maxPrecision = 0;
    ROI datawindow;
    m_bpp.clear();
    m_bpp.reserve(channelCount);
    std::vector<TypeDesc> chantypes(channelCount, TypeDesc::UINT8);
    for (int i = 0; i < channelCount; i++) {
        const opj_image_comp_t& comp(m_image->comps[i]);
        m_bpp.push_back(comp.prec);
        maxPrecision = std::max(comp.prec, maxPrecision);
        ROI roichan(comp.x0, comp.x0 + comp.w * comp.dx, comp.y0,
                    comp.y0 + comp.h * comp.dy);
        datawindow = roi_union(datawindow, roichan);
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

    m_spec   = ImageSpec(datawindow.width(), datawindow.height(), channelCount,
                       format);
    m_spec.x = datawindow.xbegin;
    m_spec.y = datawindow.ybegin;
    m_spec.full_x      = m_image->x0;
    m_spec.full_y      = m_image->y0;
    m_spec.full_width  = m_image->x1;
    m_spec.full_height = m_image->y1;

    m_spec.attribute("oiio:BitsPerSample", maxPrecision);
    m_spec.attribute("oiio:Orientation", 1);
    m_spec.attribute("oiio:ColorSpace", "sRGB");
#ifndef OPENJPEG_VERSION
    // Sigh... openjpeg.h doesn't seem to have a clear version #define.
    // OPENJPEG_VERSION only seems to exist in 1.3, which doesn't have
    // the ICC fields. So assume its absence in the newer one (at least,
    // 1.5) means the field is valid.
    if (m_image->icc_profile_len && m_image->icc_profile_buf)
        m_spec.attribute("ICCProfile",
                         TypeDesc(TypeDesc::UINT8, m_image->icc_profile_len),
                         m_image->icc_profile_buf);
#endif

    p_spec = m_spec;
    return true;
}



bool
Jpeg2000Input::open(const std::string& name, ImageSpec& newspec,
                    const ImageSpec& config)
{
    // Check 'config' for any special requests
    if (config.get_int_attribute("oiio:UnassociatedAlpha", 0) == 1)
        m_keep_unassociated_alpha = true;
    return open(name, newspec);
}


bool
Jpeg2000Input::read_native_scanline(int subimage, int miplevel, int y, int z,
                                    void* data)
{
    lock_guard lock(m_mutex);
    if (!seek_subimage(subimage, miplevel))
        return false;

    if (m_spec.format == TypeDesc::UINT8)
        read_scanline<uint8_t>(y, z, data);
    else
        read_scanline<uint16_t>(y, z, data);

    // JPEG2000 specifically dictates unassociated (un-"premultiplied") alpha.
    // Convert to associated unless we were requested not to do so.
    if (m_spec.alpha_channel != -1 && !m_keep_unassociated_alpha) {
        float gamma = m_spec.get_float_attribute("oiio:Gamma", 2.2f);
        if (m_spec.format == TypeDesc::UINT16)
            associateAlpha((unsigned short*)data, m_spec.width,
                           m_spec.nchannels, m_spec.alpha_channel, gamma);
        else
            associateAlpha((unsigned char*)data, m_spec.width, m_spec.nchannels,
                           m_spec.alpha_channel, gamma);
    }

    return true;
}



inline bool
Jpeg2000Input::close(void)
{
    if (m_image) {
        opj_image_destroy(m_image);
        m_image = NULL;
    }
    destroy_decompressor();
    destroy_stream();
    if (m_file) {
        fclose(m_file);
        m_file = NULL;
    }
    return true;
}


bool
Jpeg2000Input::isJp2File(const int* const p_magicTable) const
{
    const int32_t JP2_MAGIC = 0x0000000C, JP2_MAGIC2 = 0x0C000000;
    if (p_magicTable[0] == JP2_MAGIC || p_magicTable[0] == JP2_MAGIC2) {
        const int32_t JP2_SIG1_MAGIC = 0x6A502020, JP2_SIG1_MAGIC2 = 0x2020506A;
        const int32_t JP2_SIG2_MAGIC = 0x0D0A870A, JP2_SIG2_MAGIC2 = 0x0A870A0D;
        if ((p_magicTable[1] == JP2_SIG1_MAGIC
             || p_magicTable[1] == JP2_SIG1_MAGIC2)
            && (p_magicTable[2] == JP2_SIG2_MAGIC
                || p_magicTable[2] == JP2_SIG2_MAGIC2)) {
            return true;
        }
    }
    return false;
}


opj_codec_t*
Jpeg2000Input::create_decompressor()
{
    int magic[3];
    size_t r = Filesystem::read_bytes(m_filename, magic, sizeof(magic));
    if (r != 3 * sizeof(int)) {
        errorf("Empty file \"%s\"", m_filename);
        return NULL;
    }

    opj_codec_t* codec = NULL;
    if (isJp2File(magic))
        codec = opj_create_decompress(OPJ_CODEC_JP2);
    else
        codec = opj_create_decompress(OPJ_CODEC_J2K);
    return codec;
}



void
Jpeg2000Input::destroy_decompressor()
{
    if (m_codec) {
        opj_destroy_codec(m_codec);
        m_codec = NULL;
    }
}



template<typename T>
void
Jpeg2000Input::read_scanline(int y, int /*z*/, void* data)
{
    T* scanline = static_cast<T*>(data);
    int nc      = m_spec.nchannels;
    // It's easier to loop over channels
    int bits = sizeof(T) * 8;
    for (int c = 0; c < nc; ++c) {
        const opj_image_comp_t& comp(m_image->comps[c]);
        int chan_ybegin = comp.y0, chan_yend = comp.y0 + comp.h * comp.dy;
        int chan_xend = comp.w * comp.dx;
        int yoff      = (y - comp.y0) / comp.dy;
        for (int x = 0; x < m_spec.width; ++x) {
            if (yoff < chan_ybegin || yoff >= chan_yend || x > chan_xend) {
                // Outside the window of this channel
                scanline[x * nc + c] = T(0);
            } else {
                unsigned int val = comp.data[yoff * comp.w + x / comp.dx];
                if (comp.sgnd)
                    val += (1 << (bits / 2 - 1));
                scanline[x * nc + c] = (T)bit_range_convert(val, comp.prec,
                                                            bits);
            }
        }
    }
    if (m_image->color_space == OPJ_CLRSPC_SYCC)
        yuv_to_rgb(scanline);
}


OIIO_PLUGIN_NAMESPACE_END
