// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <cstdio>
#include <vector>

#include <openjpeg.h>
#include <opj_config.h>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/tiffutils.h>

#ifndef OIIO_OPJ_VERSION
#    if defined(OPJ_VERSION_MAJOR)
// OpenJPEG >= 2.1 defines these symbols
#        define OIIO_OPJ_VERSION                                 \
            (OPJ_VERSION_MAJOR * 10000 + OPJ_VERSION_MINOR * 100 \
             + OPJ_VERSION_BUILD)
#    else
// Older, assume it's the minimum of 2.0
#        define OIIO_OPJ_VERSION 20000
#    endif
#endif


OIIO_PLUGIN_NAMESPACE_BEGIN

namespace {


// TODO(sergey): This actually a straight duplication from the png reader,
// consider de-duplicating the code somehow?
template<class T>
void
j2k_associateAlpha(T* data, int size, int channels, int alpha_channel,
                   float gamma)
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
    ~Jpeg2000Input() override { close(); }
    const char* format_name(void) const override { return "jpeg2000"; }
    int supports(string_view feature) const override
    {
        return feature == "ioproxy";
        // FIXME: we should support Exif/IPTC, but currently don't.
    }
    bool valid_file(Filesystem::IOProxy* ioproxy) const override;
    bool open(const std::string& name, ImageSpec& spec) override;
    bool open(const std::string& name, ImageSpec& newspec,
              const ImageSpec& config) override;
    bool close(void) override;
    bool read_native_scanline(int subimage, int miplevel, int y, int z,
                              void* data) override;

private:
    std::string m_filename;
    std::vector<int> m_bpp;  // per channel bpp
    opj_image_t* m_image;
    opj_codec_t* m_codec;
    opj_stream_t* m_stream;
    bool m_keep_unassociated_alpha;  // Do not convert unassociated alpha

    void init(void);

    static bool is_jp2_header(const uint8_t header[12]);
    static bool is_j2k_header(const uint8_t header[5]);

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

    static OPJ_SIZE_T StreamRead(void* p_buffer, OPJ_SIZE_T p_nb_bytes,
                                 void* p_user_data)
    {
        auto in = static_cast<Jpeg2000Input*>(p_user_data);
        auto r  = in->ioproxy()->read(p_buffer, p_nb_bytes);
        return r ? OPJ_SIZE_T(r) : OPJ_SIZE_T(-1);
    }

    static OPJ_BOOL StreamSeek(OPJ_OFF_T p_nb_bytes, void* p_user_data)
    {
        auto in = static_cast<Jpeg2000Input*>(p_user_data);
        return in->ioseek(p_nb_bytes, SEEK_SET);
    }

    static OPJ_OFF_T StreamSkip(OPJ_OFF_T p_nb_bytes, void* p_user_data)
    {
        auto in = static_cast<Jpeg2000Input*>(p_user_data);
        return in->ioseek(p_nb_bytes, SEEK_CUR) ? p_nb_bytes : OPJ_SIZE_T(-1);
    }

    static void StreamFree(void* p_user_data) {}

    static void openjpeg_error_callback(const char* msg, void* data)
    {
        if (ImageInput* input = (ImageInput*)data) {
            input->errorfmt("{}",
                            msg && msg[0] ? msg : "Unknown OpenJpeg error");
        }
    }

    static void openjpeg_dummy_callback(const char* /*msg*/, void* /*data*/) {}
};


// Obligatory material to make this a recognizable imageio plugin
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT int jpeg2000_imageio_version = OIIO_PLUGIN_VERSION;
OIIO_EXPORT const char*
jpeg2000_imageio_library_version()
{
    return ustring::fmtformat("OpenJpeg {}", opj_version()).c_str();
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
    m_image                   = NULL;
    m_codec                   = NULL;
    m_stream                  = NULL;
    m_keep_unassociated_alpha = false;
    ioproxy_clear();
}

bool
Jpeg2000Input::valid_file(Filesystem::IOProxy* ioproxy) const
{
    if (!ioproxy || ioproxy->mode() != Filesystem::IOProxy::Mode::Read)
        return false;

    uint8_t header[12];
    auto r = ioproxy->pread(header, sizeof(header), 0);
    if (r != sizeof(header)) {
        return false;
    }
    return is_jp2_header(header) || is_j2k_header(header);
}

bool
Jpeg2000Input::open(const std::string& name, ImageSpec& p_spec)
{
    m_filename = name;

    if (!ioproxy_use_or_open(name))
        return false;
    ioseek(0);

    m_codec = create_decompressor();
    if (!m_codec) {
        errorfmt("Could not create Jpeg2000 stream decompressor");
        close();
        return false;
    }

    setup_event_mgr(m_codec);

    opj_dparameters_t parameters;
    opj_set_default_decoder_parameters(&parameters);
    opj_setup_decoder(m_codec, &parameters);

#if OIIO_OPJ_VERSION >= 20200
    // Set up multithread in OpenJPEG library -- added in OpenJPEG 2.2,
    // but it doesn't seem reliably safe until 2.4.
    int nthreads = threads();
    if (!nthreads)
        nthreads = OIIO::get_int_attribute("threads");
    opj_codec_set_threads(m_codec, nthreads);
#endif

    m_stream = opj_stream_default_create(true /* is_input */);
    if (!m_stream) {
        errorfmt("Could not create Jpeg2000 stream");
        close();
        return false;
    }

    opj_stream_set_user_data(m_stream, this, StreamFree);
    opj_stream_set_read_function(m_stream, StreamRead);
    opj_stream_set_seek_function(m_stream, StreamSeek);
    opj_stream_set_skip_function(m_stream, StreamSkip);
    opj_stream_set_user_data_length(m_stream, ioproxy()->size());
    // opj_stream_set_write_function(m_stream, StreamWrite);

    OIIO_ASSERT(m_image == nullptr);
    if (!opj_read_header(m_stream, m_codec, &m_image) || !m_image
        || has_error()) {
        if (!has_error())
            errorfmt("Could not read Jpeg2000 header");
    }
    if (!has_error()) {
        if (!opj_decode(m_codec, m_stream, m_image)) {
            if (!has_error())
                errorfmt("Could not decode Jpeg2000 data");
        }
    }

    destroy_decompressor();
    destroy_stream();

    if (has_error()) {
        close();
        return false;
    }
    OIIO_ASSERT(m_image != nullptr);

    // we support only one, three or four components in image
    const int channelCount = m_image->numcomps;
    if (channelCount != 1 && channelCount != 3 && channelCount != 4) {
        errorfmt(
            "Only images with one, three or four components are supported");
        close();
        return false;
    }

    for (int c = 0; c < channelCount; ++c) {
        const opj_image_comp_t& comp(m_image->comps[c]);
        if (!comp.data) {
            errorfmt("Could not read Jpeg2000 component, no channel data {}",
                     c);
            close();
            return false;
        }
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
    TypeDesc format = (maxPrecision <= 8) ? TypeDesc::UINT8 : TypeDesc::UINT16;
    m_spec   = ImageSpec(datawindow.width(), datawindow.height(), channelCount,
                         format);
    m_spec.x = datawindow.xbegin;
    m_spec.y = datawindow.ybegin;
    m_spec.full_x      = m_image->x0;
    m_spec.full_y      = m_image->y0;
    m_spec.full_width  = m_image->x1;
    m_spec.full_height = m_image->y1;

    m_spec.attribute("oiio:BitsPerSample", maxPrecision);
    m_spec.set_colorspace("sRGB");

    if (m_image->icc_profile_len && m_image->icc_profile_buf) {
        m_spec.attribute("ICCProfile",
                         TypeDesc(TypeDesc::UINT8, m_image->icc_profile_len),
                         m_image->icc_profile_buf);
        std::string errormsg;
        bool ok = decode_icc_profile(
            cspan<uint8_t>((const uint8_t*)m_image->icc_profile_buf,
                           m_image->icc_profile_len),
            m_spec, errormsg);
        if (!ok && OIIO::get_int_attribute("imageinput:strict")) {
            errorfmt("Possible corrupt file, could not decode ICC profile: {}\n",
                     errormsg);
            return false;
        }
    }

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
    ioproxy_retrieve_from_config(config);
    return open(name, newspec);
}


bool
Jpeg2000Input::read_native_scanline(int subimage, int miplevel, int y, int z,
                                    void* data)
{
    lock_guard lock(*this);
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
            j2k_associateAlpha((unsigned short*)data, m_spec.width,
                               m_spec.nchannels, m_spec.alpha_channel, gamma);
        else
            j2k_associateAlpha((unsigned char*)data, m_spec.width,
                               m_spec.nchannels, m_spec.alpha_channel, gamma);
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
    init();
    return true;
}

bool
Jpeg2000Input::is_jp2_header(const uint8_t header[12])
{
    const uint8_t jp2_header[] = { 0x0,  0x0,  0x0,  0x0C, 0x6A, 0x50,
                                   0x20, 0x20, 0x0D, 0x0A, 0x87, 0x0A };
    return memcmp(header, jp2_header, sizeof(jp2_header)) == 0;
}

bool
Jpeg2000Input::is_j2k_header(const uint8_t header[5])
{
    const uint8_t j2k_header[] = { 0xFF, 0x4F, 0xFF, 0x51, 0x00 };
    return memcmp(header, j2k_header, sizeof(j2k_header)) == 0;
}

opj_codec_t*
Jpeg2000Input::create_decompressor()
{
    uint8_t header[12];
    auto r = ioproxy()->pread(header, sizeof(header), 0);
    if (r != sizeof(header)) {
        errorfmt("Empty file \"{}\"", m_filename);
        return nullptr;
    }
    return opj_create_decompress(is_jp2_header(header) ? OPJ_CODEC_JP2
                                                       : OPJ_CODEC_J2K);
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
