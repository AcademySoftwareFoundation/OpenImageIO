// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <vector>

#include <openjpeg.h>
#include <opj_config.h>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>

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

#ifdef USE_OPENJPH
#    include <openjph/ojph_arg.h>
#    include <openjph/ojph_codestream.h>
#    include <openjph/ojph_file.h>
#    include <openjph/ojph_mem.h>
#    include <openjph/ojph_params.h>
#endif

OIIO_PLUGIN_NAMESPACE_BEGIN


class Jpeg2000Output final : public ImageOutput {
public:
    Jpeg2000Output() { init(); }
    ~Jpeg2000Output() override { close(); }
    const char* format_name(void) const override { return "jpeg2000"; }
    int supports(string_view feature) const override
    {
        return feature == "alpha" || feature == "ioproxy" || feature == "tiles";
        // FIXME: we should support Exif/IPTC, but currently don't.
    }
    bool open(const std::string& name, const ImageSpec& spec,
              OpenMode mode = Create) override;
    bool close() override;
    bool write_scanline(int y, int z, TypeDesc format, const void* data,
                        stride_t xstride) override;
    bool write_tile(int x, int y, int z, TypeDesc format, const void* data,
                    stride_t xstride, stride_t ystride,
                    stride_t zstride) override;

private:
    std::string m_filename;
    opj_cparameters_t m_compression_parameters;
    opj_image_t* m_image;
    opj_codec_t* m_codec;
    opj_stream_t* m_stream;
    unsigned int m_dither;
    bool m_convert_alpha;  //< Do we deassociate alpha?
    std::vector<unsigned char> m_tilebuffer;
    std::vector<unsigned char> m_scratch;


#ifdef USE_OPENJPH
    // opj_cparameters_t m_compression_parameters;
    std::unique_ptr<ojph::j2c_outfile> m_jph_image;
    std::unique_ptr<ojph::codestream> m_jph_stream;
    int output_depth;
#endif

    void init(void)
    {
        m_image         = NULL;
        m_codec         = NULL;
        m_stream        = NULL;
        m_convert_alpha = true;

        ioproxy_clear();
    }

    opj_image_t* create_jpeg2000_image();

    void init_components(opj_image_cmptparm_t* components, int precision);

    opj_codec_t* create_compressor();

    void destroy_compressor()
    {
        if (m_codec) {
            opj_destroy_codec(m_codec);
            m_codec = NULL;
        }
    }

    void destroy_stream()
    {
        if (m_stream) {
            opj_stream_destroy(m_stream);
            m_stream = NULL;
        }
#ifdef USE_OPENJPH
        m_jph_stream.reset();
        m_jph_image.reset();
#endif
    }

    bool save_image();

    template<typename T> void write_scanline(int y, int z, const void* data);

    void setup_cinema_compression(OPJ_RSIZ_CAPABILITIES p_rsizCap);

    void setup_compression_params();

    OPJ_PROG_ORDER get_progression_order(const std::string& progression_order);

    static OPJ_SIZE_T StreamWrite(void* p_buffer, OPJ_SIZE_T p_nb_bytes,
                                  void* p_user_data)
    {
        auto in = static_cast<Jpeg2000Output*>(p_user_data);
        auto r  = in->ioproxy()->write(p_buffer, p_nb_bytes);
        return r ? OPJ_SIZE_T(r) : OPJ_SIZE_T(-1);
    }

    static OPJ_BOOL StreamSeek(OPJ_OFF_T p_nb_bytes, void* p_user_data)
    {
        auto in = static_cast<Jpeg2000Output*>(p_user_data);
        return in->ioseek(p_nb_bytes, SEEK_SET);
    }

    static OPJ_OFF_T StreamSkip(OPJ_OFF_T p_nb_bytes, void* p_user_data)
    {
        auto in = static_cast<Jpeg2000Output*>(p_user_data);
        return in->ioseek(p_nb_bytes, SEEK_CUR) ? p_nb_bytes : OPJ_SIZE_T(-1);
    }

    static void StreamFree(void* p_user_data) {}

    static void openjpeg_error_callback(const char* msg, void* data)
    {
        if (ImageOutput* output = (ImageOutput*)data) {
            output->errorfmt("{}",
                             msg && msg[0] ? msg : "Unknown OpenJpeg error");
        }
    }

    static void openjpeg_dummy_callback(const char* /*msg*/, void* /*data*/) {}

#ifdef USE_OPENJPH
    void create_jph_image();
    template<typename T>
    void write_jph_scanline(int y, int /*z*/, const void* data);
#endif
};


// Obligatory material to make this a recognizable imageio plugin
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput*
jpeg2000_output_imageio_create()
{
    return new Jpeg2000Output;
}

OIIO_EXPORT const char* jpeg2000_output_extensions[] = { "jp2", "j2k",
#ifdef USE_OPENJPH
                                                         "j2c", "jph",
#endif
                                                         nullptr };


OIIO_PLUGIN_EXPORTS_END



bool
Jpeg2000Output::open(const std::string& name, const ImageSpec& spec,
                     OpenMode mode)
{
    if (!check_open(mode, spec, { 0, 1 << 20, 0, 1 << 20, 0, 1, 0, 4 },
                    uint64_t(OpenChecks::Disallow2Channel)))
        return false;

    m_filename = name;

    // If not uint8 or uint16, default to uint16
    if (m_spec.format != TypeDesc::UINT8 && m_spec.format != TypeDesc::UINT16)
        m_spec.set_format(TypeDesc::UINT16);

    m_dither        = (m_spec.format == TypeDesc::UINT8)
                          ? m_spec.get_int_attribute("oiio:dither", 0)
                          : 0;
    m_convert_alpha = m_spec.alpha_channel != -1
                      && !m_spec.get_int_attribute("oiio:UnassociatedAlpha", 0);

    ioproxy_retrieve_from_config(m_spec);
    if (!ioproxy_use_or_open(name))
        return false;

    // If user asked for tiles -- which this format doesn't support, emulate
    // it by buffering the whole image.
    if (m_spec.tile_width && m_spec.tile_height)
        m_tilebuffer.resize(m_spec.image_bytes());

    const ParamValue* compressionparams = m_spec.find_attribute("compression",
                                                                TypeString);
#ifdef USE_OPENJPH

    bool use_openjph = false;

    // If a j2c file is specified, we default to j2c
    // otherwise we check the compression parameter.

    std::string compressionparms_str;
    if (compressionparams) {
        compressionparms_str = compressionparams->get_string();
        if (compressionparms_str.compare(0, 5, "htj2k") == 0)
            use_openjph = true;
    }
    std::string ext = Filesystem::extension(name);
    if (ext == ".j2c")
        // TODO - Need to check if j2c files can be created with openjpeg
        use_openjph = true;

    if (use_openjph) {
        create_jph_image();
        return true;
    }
#else
    // If we are not using OpenJPH, we need to create a JPEG2000 image.
    // This is the default behavior.
    std::string compressionparms_str;
    if (compressionparams) {
        compressionparms_str = compressionparams->get_string();
        if (compressionparms_str.compare(0, 5, "htj2k") == 0) {
            errorfmt("OpenJPH not enabled, cannot create HTJ2K file");
            return false;
        }
    }
#endif
    m_image = create_jpeg2000_image();
    return true;
}



template<class T>
static void
deassociateAlpha(T* data, int size, int channels, int alpha_channel,
                 float gamma)
{
    unsigned int max = std::numeric_limits<T>::max();
    if (gamma == 1) {
        for (int x = 0; x < size; ++x, data += channels)
            if (data[alpha_channel])
                for (int c = 0; c < channels; c++)
                    if (c != alpha_channel) {
                        unsigned int f = data[c];
                        f              = (f * max) / data[alpha_channel];
                        data[c]        = (T)std::min(max, f);
                    }
    } else {
        for (int x = 0; x < size; ++x, data += channels)
            if (data[alpha_channel]) {
                // See associateAlpha() for an explanation.
                float alpha_deassociate = pow((float)max / data[alpha_channel],
                                              gamma);
                for (int c = 0; c < channels; c++)
                    if (c != alpha_channel)
                        data[c] = static_cast<T>(std::min(
                            max, (unsigned int)(data[c] * alpha_deassociate)));
            }
    }
}



bool
Jpeg2000Output::write_scanline(int y, int z, TypeDesc format, const void* data,
                               stride_t xstride)
{
    y -= m_spec.y;
    if (y > m_spec.height) {
        errorfmt("Attempt to write too many scanlines to {}", m_filename);
        return false;
    }

    m_spec.auto_stride(xstride, format, spec().nchannels);
    const void* origdata = data;
    data = to_native_scanline(format, data, xstride, m_scratch, m_dither, y, z);
    if (data == origdata) {
        m_scratch.assign((unsigned char*)data,
                         (unsigned char*)data + m_spec.scanline_bytes());
        data = &m_scratch[0];
    }

    // JPEG-2000 specifically dictates unassociated (un-"premultiplied") alpha
    if (m_convert_alpha) {
        if (m_spec.format == TypeDesc::UINT16)
            deassociateAlpha((unsigned short*)data, m_spec.width,
                             m_spec.nchannels, m_spec.alpha_channel, 2.2f);
        else
            deassociateAlpha((unsigned char*)data, m_spec.width,
                             m_spec.nchannels, m_spec.alpha_channel, 2.2f);
    }

#ifdef USE_OPENJPH
    if (m_jph_image) {
        if (m_spec.format == TypeDesc::UINT8)
            write_jph_scanline<uint8_t>(y, z, data);
        else
            write_jph_scanline<uint16_t>(y, z, data);
    } else
#endif  // USE_OPENJPH
        if (m_spec.format == TypeDesc::UINT8)
            write_scanline<uint8_t>(y, z, data);
        else
            write_scanline<uint16_t>(y, z, data);

    if (y == m_spec.height - 1)
        save_image();

    return true;
}



bool
Jpeg2000Output::write_tile(int x, int y, int z, TypeDesc format,
                           const void* data, stride_t xstride, stride_t ystride,
                           stride_t zstride)
{
    // Emulate tiles by buffering the whole image
    return copy_tile_to_image_buffer(x, y, z, format, data, xstride, ystride,
                                     zstride, &m_tilebuffer[0]);
}



bool
Jpeg2000Output::close()
{
    if (!m_stream) {  // Already closed
        return true;
    }

    bool ok = true;
    if (m_spec.tile_width) {
        // We've been emulating tiles; now dump as scanlines.
        OIIO_ASSERT(m_tilebuffer.size());
        ok &= write_scanlines(m_spec.y, m_spec.y + m_spec.height, 0,
                              m_spec.format, &m_tilebuffer[0]);
        std::vector<unsigned char>().swap(m_tilebuffer);
    }

#ifdef USE_OPENJPH
    if (m_jph_image) {
        m_jph_stream->flush();
        m_jph_stream->close();
        destroy_stream();
        return true;
    }
#endif

    if (m_image) {
        opj_image_destroy(m_image);
        m_image = NULL;
    }
    destroy_compressor();
    destroy_stream();
    init();
    return ok;
}



bool
Jpeg2000Output::save_image()
{
#ifdef USE_OPENJPH
    if (m_jph_stream) {
        m_jph_stream->flush();
        m_jph_stream->close();
        destroy_stream();
        return true;
    }
#endif

    m_codec = create_compressor();
    if (!m_codec)
        return false;

    opj_set_error_handler(m_codec, openjpeg_error_callback, this);
    opj_set_warning_handler(m_codec, openjpeg_dummy_callback, NULL);
    opj_set_info_handler(m_codec, openjpeg_dummy_callback, NULL);

    opj_setup_encoder(m_codec, &m_compression_parameters, m_image);

#if OIIO_OPJ_VERSION >= 20400
    // Set up multithread in OpenJPEG library -- added in OpenJPEG 2.2,
    // but it doesn't seem reliably safe until 2.4.
    int nthreads = threads();
    if (!nthreads)
        nthreads = OIIO::get_int_attribute("threads");
    opj_codec_set_threads(m_codec, nthreads);
#endif

    m_stream = opj_stream_default_create(false /* is_input */);
    if (!m_stream) {
        errorfmt("Failed write jpeg2000::save_image");
        return false;
    }

    opj_stream_set_user_data(m_stream, this, StreamFree);
    opj_stream_set_seek_function(m_stream, StreamSeek);
    opj_stream_set_skip_function(m_stream, StreamSkip);
    opj_stream_set_write_function(m_stream, StreamWrite);
    // opj_stream_set_user_data_length(m_stream, ioproxy()->size());

    if (!opj_start_compress(m_codec, m_image, m_stream)
        || !opj_encode(m_codec, m_stream)
        || !opj_end_compress(m_codec, m_stream)) {
        errorfmt("Failed write jpeg2000::save_image");
        return false;
    }

    return true;
}



opj_image_t*
Jpeg2000Output::create_jpeg2000_image()
{
    setup_compression_params();

    OPJ_COLOR_SPACE color_space = OPJ_CLRSPC_SRGB;
    if (m_spec.nchannels == 1)
        color_space = OPJ_CLRSPC_GRAY;

    int precision          = 16;
    const ParamValue* prec = m_spec.find_attribute("oiio:BitsPerSample",
                                                   TypeDesc::INT);
    if (prec)
        precision = *(int*)prec->data();
    else if (m_spec.format == TypeDesc::UINT8
             || m_spec.format == TypeDesc::INT8)
        precision = 8;

    const int MAX_J2K_COMPONENTS = 4;
    opj_image_cmptparm_t component_params[MAX_J2K_COMPONENTS];
    init_components(component_params, precision);

    m_image = opj_image_create(m_spec.nchannels, &component_params[0],
                               color_space);

    m_image->x0 = m_compression_parameters.image_offset_x0;
    m_image->y0 = m_compression_parameters.image_offset_y0;
    m_image->x1 = m_compression_parameters.image_offset_x0
                  + (m_spec.width - 1) * m_compression_parameters.subsampling_dx
                  + 1;
    m_image->y1
        = m_compression_parameters.image_offset_y0
          + (m_spec.height - 1) * m_compression_parameters.subsampling_dy + 1;

#if 0
    // FIXME: I seem to get crashes with OpenJpeg 2.x in the presence of ICC
    // profiles. I have no idea why. It seems like losing the ability to
    // write ICC profiles is the lesser evil compared to either restricting
    // ourselves to OpenJpeg 1.5 or living with crashes. I'm at the limit of
    // my knowledge of OpenJPEG, which frankly has a poor API and abysmal
    // documentation. So I'll leave the repair of this for later. If
    // someboody comes along that desperately needs JPEG2000 and ICC
    // profiles, maybe they will be motivated enough to track down the
    // problem.
    const ParamValue *icc = m_spec.find_attribute ("ICCProfile");
    if (icc && icc->type().basetype == TypeDesc::UINT8 && icc->type().arraylen > 0) {
        m_image->icc_profile_len = icc->type().arraylen;
        m_image->icc_profile_buf = (unsigned char *) icc->data();
    }
#endif

    return m_image;
}



inline void
Jpeg2000Output::init_components(opj_image_cmptparm_t* components, int precision)
{
    memset(components, 0x00, m_spec.nchannels * sizeof(opj_image_cmptparm_t));
    for (int i = 0; i < m_spec.nchannels; i++) {
        components[i].dx   = m_compression_parameters.subsampling_dx;
        components[i].dy   = m_compression_parameters.subsampling_dy;
        components[i].w    = m_spec.width;
        components[i].h    = m_spec.height;
        components[i].prec = precision;
#if OIIO_OPJ_VERSION < 20500
        // bpp field is deprecated starting with OpenJPEG 2.5
        components[i].bpp = precision;
#endif
        components[i].sgnd = 0;
    }
}



opj_codec_t*
Jpeg2000Output::create_compressor()
{
    std::string ext         = Filesystem::extension(m_filename);
    opj_codec_t* compressor = NULL;
    if (ext == ".j2k")
        compressor = opj_create_compress(OPJ_CODEC_J2K);
    else if (ext == ".jp2")
        compressor = opj_create_compress(OPJ_CODEC_JP2);

    return compressor;
}



template<typename T>
void
Jpeg2000Output::write_scanline(int y, int /*z*/, const void* data)
{
    int bits                  = sizeof(T) * 8;
    const T* scanline         = static_cast<const T*>(data);
    const size_t scanline_pos = (y - m_spec.y) * m_spec.width;
    for (int i = 0, j = 0; i < m_spec.width; i++) {
        for (int c = 0; c < m_spec.nchannels; ++c) {
            unsigned int val = scanline[j++];
            if (bits != int(m_image->comps[c].prec))
                val = bit_range_convert(val, bits, m_image->comps[c].prec);
            m_image->comps[c].data[scanline_pos + i] = val;
        }
    }
}



void
Jpeg2000Output::setup_cinema_compression(OPJ_RSIZ_CAPABILITIES p_rsizCap)
{
    m_compression_parameters.tile_size_on = false;
    m_compression_parameters.cp_tdx       = 1;
    m_compression_parameters.cp_tdy       = 1;

    m_compression_parameters.tp_flag = 'C';
    m_compression_parameters.tp_on   = 1;

    m_compression_parameters.cp_tx0          = 0;
    m_compression_parameters.cp_ty0          = 0;
    m_compression_parameters.image_offset_x0 = 0;
    m_compression_parameters.image_offset_y0 = 0;

    m_compression_parameters.cblockw_init = 32;
    m_compression_parameters.cblockh_init = 32;
    m_compression_parameters.csty |= 0x01;

    m_compression_parameters.prog_order = OPJ_CPRL;

    m_compression_parameters.roi_compno = -1;

    m_compression_parameters.subsampling_dx = 1;
    m_compression_parameters.subsampling_dy = 1;

    m_compression_parameters.irreversible = 1;

    m_compression_parameters.cp_rsiz = p_rsizCap;
    if (p_rsizCap == OPJ_CINEMA4K) {
        m_compression_parameters.cp_cinema      = OPJ_CINEMA4K_24;
        m_compression_parameters.POC[0].tile    = 1;
        m_compression_parameters.POC[0].resno0  = 0;
        m_compression_parameters.POC[0].compno0 = 0;
        m_compression_parameters.POC[0].layno1  = 1;
        m_compression_parameters.POC[0].resno1
            = m_compression_parameters.numresolution - 1;
        m_compression_parameters.POC[0].compno1 = 3;
        m_compression_parameters.POC[0].prg1    = OPJ_CPRL;
        m_compression_parameters.POC[1].tile    = 1;
        m_compression_parameters.POC[1].resno0
            = m_compression_parameters.numresolution - 1;
        m_compression_parameters.POC[1].compno0 = 0;
        m_compression_parameters.POC[1].layno1  = 1;
        m_compression_parameters.POC[1].resno1
            = m_compression_parameters.numresolution;
        m_compression_parameters.POC[1].compno1 = 3;
        m_compression_parameters.POC[1].prg1    = OPJ_CPRL;
    } else if (p_rsizCap == OPJ_CINEMA2K) {
        m_compression_parameters.cp_cinema = OPJ_CINEMA2K_24;
    }
}



void
Jpeg2000Output::setup_compression_params()
{
    opj_set_default_encoder_parameters(&m_compression_parameters);
    m_compression_parameters.tcp_rates[0] = 0;
    m_compression_parameters.tcp_numlayers++;
    m_compression_parameters.cp_disto_alloc = 1;

    const ParamValue* is_cinema2k = m_spec.find_attribute("jpeg2000:Cinema2K",
                                                          TypeDesc::UINT);
    if (is_cinema2k)
        setup_cinema_compression(OPJ_CINEMA2K);

    const ParamValue* is_cinema4k = m_spec.find_attribute("jpeg2000:Cinema4K",
                                                          TypeDesc::UINT);
    if (is_cinema4k)
        setup_cinema_compression(OPJ_CINEMA4K);

    const ParamValue* initial_cb_width
        = m_spec.find_attribute("jpeg2000:InitialCodeBlockWidth",
                                TypeDesc::UINT);
    if (initial_cb_width && initial_cb_width->data())
        m_compression_parameters.cblockw_init
            = *(unsigned int*)initial_cb_width->data();

    const ParamValue* initial_cb_height
        = m_spec.find_attribute("jpeg2000:InitialCodeBlockHeight",
                                TypeDesc::UINT);
    if (initial_cb_height && initial_cb_height->data())
        m_compression_parameters.cblockh_init
            = *(unsigned int*)initial_cb_height->data();

    const ParamValue* progression_order
        = m_spec.find_attribute("jpeg2000:ProgressionOrder", TypeDesc::STRING);
    if (progression_order && progression_order->data()) {
        std::string prog_order((const char*)progression_order->data());
        m_compression_parameters.prog_order = get_progression_order(prog_order);
    }

    const ParamValue* compression_mode
        = m_spec.find_attribute("jpeg2000:CompressionMode", TypeDesc::INT);
    if (compression_mode && compression_mode->data())
        m_compression_parameters.mode = *(int*)compression_mode->data();
}



OPJ_PROG_ORDER
Jpeg2000Output::get_progression_order(const std::string& progression_order)
{
    if (progression_order == "LRCP")
        return OPJ_LRCP;
    else if (progression_order == "RLCP")
        return OPJ_RLCP;
    else if (progression_order == "RPCL")
        return OPJ_RPCL;
    else if (progression_order == "PCRL")
        return OPJ_PCRL;
    else if (progression_order == "PCRL")
        return OPJ_CPRL;
    return OPJ_PROG_UNKNOWN;
}


#ifdef USE_OPENJPH


struct size_list_interpreter : public ojph::cli_interpreter::arg_inter_base {
    size_list_interpreter(const int max_num_elements, int& num_elements,
                          ojph::size* list)
        : max_num_eles(max_num_elements)
        , sizelist(list)
        , num_eles(num_elements)
    {
    }

    virtual void operate(const char* str)
    {
        const char* next_char = str;
        num_eles              = 0;
        do {
            if (num_eles) {
                if (*next_char != ',')  //separate sizes by a comma
                    throw "sizes in a sizes list must be separated by a comma";
                next_char++;
            }

            char* endptr;
            sizelist[num_eles].w = (ojph::ui32)strtoul(next_char, &endptr, 10);
            if (endptr == next_char)
                throw "size number is improperly formatted";
            next_char = endptr;
            if (*next_char != ',')
                throw "size must have a "
                      ","
                      " between the two numbers";
            next_char++;
            sizelist[num_eles].h = (ojph::ui32)strtoul(next_char, &endptr, 10);
            if (endptr == next_char)
                throw "number is improperly formatted";
            next_char = endptr;


            ++num_eles;
        } while (*next_char == ',' && num_eles < max_num_eles);
        if (num_eles < max_num_eles) {
            if (*next_char)
                throw "size elements must separated by a "
                      ","
                      "";
        } else if (*next_char)
            throw "there are too many elements in the size list";
    }

    const int max_num_eles;
    ojph::size* sizelist;
    int& num_eles;
};



void
Jpeg2000Output::create_jph_image()
{
    m_jph_stream        = std::make_unique<ojph::codestream>();
    ojph::param_siz siz = m_jph_stream->access_siz();
    siz.set_image_extent(ojph::point(m_spec.width, m_spec.height));


    // TODO
    /*
    OPJ_COLOR_SPACE color_space = OPJ_CLRSPC_SRGB;
    if (m_spec.nchannels == 1)
        color_space = OPJ_CLRSPC_GRAY;
    */

    int precision          = 16;
    const ParamValue* prec = m_spec.find_attribute("oiio:BitsPerSample",
                                                   TypeDesc::INT);
    bool is_signed         = false;

    if (prec)
        precision = *(int*)prec->data();

    switch (m_spec.format.basetype) {
    case TypeDesc::INT8:
    case TypeDesc::UINT8:
        precision = 8;
        is_signed = false;
        break;
    case TypeDesc::FLOAT:
    case TypeDesc::HALF:
    case TypeDesc::DOUBLE:
        throw "OpenJPH::Write Double is not currently supported.";
    default: break;
    }

    output_depth = m_spec.get_int_attribute("jph:bit_depth", precision);

    siz.set_num_components(m_spec.nchannels);
    ojph::point subsample(1, 1);  // Default subsample
    for (ojph::ui32 c = 0; c < m_spec.nchannels; ++c)
        siz.set_component(c, subsample, output_depth, is_signed);

    ojph::size tile_size(0, 0);
    ojph::point tile_offset(0, 0);
    ojph::point image_offset(0, 0);
    siz.set_image_offset(image_offset);
    siz.set_tile_size(tile_size);
    siz.set_tile_offset(tile_offset);
    ojph::param_cod cod = m_jph_stream->access_cod();

    std::string block_args = m_spec.get_string_attribute("jph:block_size",
                                                         "64,64");
    std::stringstream ss(block_args);
    char comma;
    int block_size_x, block_size_y;
    ss >> block_size_x >> comma >> block_size_y;

    cod.set_block_dims(block_size_x, block_size_y);
    cod.set_color_transform(true);

    int num_precincts            = -1;
    const int max_precinct_sizes = 33;  //maximum number of decompositions is 32
    ojph::size precinct_size[max_precinct_sizes];
    std::string precinct_size_args
        = m_spec.get_string_attribute("jph:precincts", "undef");
    if (precinct_size_args != "undef") {
        size_list_interpreter sizelist(max_precinct_sizes, num_precincts,
                                       precinct_size);
        sizelist.operate(precinct_size_args.c_str());

        if (num_precincts != -1)
            cod.set_precinct_size(num_precincts, precinct_size);
    }

    std::string progression_order
        = m_spec.get_string_attribute("jph:prog_order", "RPCL");

    cod.set_progression_order(progression_order.c_str());

    cod.set_reversible(true);

    float qstep = m_spec.get_float_attribute("jph:qstep", -1);

    if (qstep > 0) {
        cod.set_reversible(false);
        m_jph_stream->access_qcd().set_irrev_quant(qstep);
    }

    cod.set_num_decomposition(m_spec.get_int_attribute("jph:num_decomps", 5));
    m_jph_stream->set_planar(false);
    m_jph_image = std::make_unique<ojph::j2c_outfile>();
    m_jph_image->open(m_filename.c_str());
    m_jph_stream->write_headers(m_jph_image.get());  //, "test comment", 1);
}



template<typename T>
void
Jpeg2000Output::write_jph_scanline(int y, int /*z*/, const void* data)
{
    int bits                 = sizeof(T) * 8;
    const T* scanline        = static_cast<const T*>(data);
    ojph::ui32 next_comp     = 0;
    ojph::line_buf* cur_line = m_jph_stream->exchange(NULL, next_comp);
    for (int c = 0; c < m_spec.nchannels; ++c) {
        assert(c == next_comp);
        for (int i = 0, j = c; i < m_spec.width; i++) {
            unsigned int val = scanline[j];
            j += m_spec.nchannels;
            if (bits != output_depth)
                val = bit_range_convert(val, bits, output_depth);
            cur_line->i32[i] = val;
        }
        cur_line = m_jph_stream->exchange(cur_line, next_comp);
    }
}

#endif

OIIO_PLUGIN_NAMESPACE_END
