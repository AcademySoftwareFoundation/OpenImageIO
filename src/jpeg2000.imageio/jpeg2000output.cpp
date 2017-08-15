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
#include <vector>
#include "openjpeg.h"
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>


OIIO_PLUGIN_NAMESPACE_BEGIN


static void
openjpeg_error_callback(const char *msg, void *data)
{
    if (ImageOutput *input = (ImageOutput *)data) {
        if (! msg || ! msg[0])
            msg = "Unknown OpenJpeg error";
        input->error ("%s", msg);
    }
}


static void
openjpeg_dummy_callback(const char *msg, void *data)
{
}



class Jpeg2000Output final : public ImageOutput {
 public:
    Jpeg2000Output () { init (); }
    virtual ~Jpeg2000Output () { close (); }
    virtual const char *format_name (void) const { return "jpeg2000"; }
    virtual int supports (string_view feature) const {
        return (feature == "alpha");
        // FIXME: we should support Exif/IPTC, but currently don't.
    }
    virtual bool open (const std::string &name, const ImageSpec &spec,
                       OpenMode mode=Create);
    virtual bool close ();
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride);
    virtual bool write_tile (int x, int y, int z, TypeDesc format,
                             const void *data, stride_t xstride,
                             stride_t ystride, stride_t zstride);
 private:
    std::string m_filename;
    FILE *m_file;
    opj_cparameters_t m_compression_parameters;
    opj_image_t *m_image;
    opj_codec_t *m_codec;
    opj_stream_t *m_stream;
    unsigned int m_dither;
    bool m_convert_alpha;             //< Do we deassociate alpha?
    std::vector<unsigned char> m_tilebuffer;
    std::vector<unsigned char> m_scratch;

    void init (void)
    {
        m_file = NULL;
        m_image = NULL;
        m_codec = NULL;
        m_stream = NULL;
        m_convert_alpha = true;
    }

    opj_image_t* create_jpeg2000_image();

    void init_components(opj_image_cmptparm_t *components, int precision);

    opj_codec_t* create_compressor();

    void destroy_compressor () {
        if (m_codec) {
            opj_destroy_codec(m_codec);
            m_codec = NULL;
        }
    }

    void destroy_stream () {
        if (m_stream) {
            opj_stream_destroy (m_stream);
            m_stream = NULL;
        }
    }

    bool save_image();

    template<typename T>
    void write_scanline(int y, int z, const void *data);

    void setup_cinema_compression(OPJ_RSIZ_CAPABILITIES p_rsizCap);

    void setup_compression_params();

    OPJ_PROG_ORDER get_progression_order(const std::string& progression_order);
};


// Obligatory material to make this a recognizeable imageio plugin
OIIO_PLUGIN_EXPORTS_BEGIN

    OIIO_EXPORT ImageOutput *jpeg2000_output_imageio_create () {
        return new Jpeg2000Output;
    }
    OIIO_EXPORT const char *jpeg2000_output_extensions[] = {
        "jp2", "j2k", NULL
    };

OIIO_PLUGIN_EXPORTS_END


bool
Jpeg2000Output::open (const std::string &name, const ImageSpec &spec,
                      OpenMode mode)
{
    if (mode != Create) {
        error ("%s does not support subimages or MIP levels", format_name());
        return false;
    }

    m_filename = name;
    m_spec = spec;

    // Check for things this format doesn't support
    if (m_spec.width < 1 || m_spec.height < 1) {
        error ("Image resolution must be at least 1x1, you asked for %d x %d",
               m_spec.width, m_spec.height);
        return false;
    }
    if (m_spec.depth < 1)
        m_spec.depth = 1;
    if (m_spec.depth > 1) {
        error ("%s does not support volume images (depth > 1)", format_name());
        return false;
    }

    if (m_spec.nchannels != 1 && m_spec.nchannels != 3 &&
            m_spec.nchannels != 4) {
        error ("%s does not support %d-channel images\n",
               format_name(), m_spec.nchannels);
        return false;
    }

    // If not uint8 or uint16, default to uint8
    if (m_spec.format != TypeDesc::UINT8 && m_spec.format != TypeDesc::UINT16)
        m_spec.set_format (TypeDesc::UINT8);

    m_dither = (m_spec.format == TypeDesc::UINT8) ?
                    m_spec.get_int_attribute ("oiio:dither", 0) : 0;
    m_convert_alpha = m_spec.alpha_channel != -1 &&
                      !m_spec.get_int_attribute("oiio:UnassociatedAlpha", 0);

    m_file = Filesystem::fopen (m_filename, "wb");
    if (m_file == NULL) {
        error ("Unable to open file \"%s\"", m_filename.c_str());
        return false;
    }

    // If user asked for tiles -- which this format doesn't support, emulate
    // it by buffering the whole image.
    if (m_spec.tile_width && m_spec.tile_height)
        m_tilebuffer.resize (m_spec.image_bytes());

    m_image = create_jpeg2000_image();
    return true;
}



template <class T>
static void
deassociateAlpha (T * data, int size, int channels, int alpha_channel, float gamma)
{
    unsigned int max = std::numeric_limits<T>::max();
    if (gamma == 1) {
        for (int x = 0;  x < size;  ++x, data += channels)
            if (data[alpha_channel])
                for (int c = 0;  c < channels;  c++)
                    if (c != alpha_channel) {
                        unsigned int f = data[c];
                        f = (f * max) / data[alpha_channel];
                        data[c] = (T) std::min (max, f);
                    }
    } else {
        for (int x = 0;  x < size;  ++x, data += channels)
            if (data[alpha_channel]) {
                // See associateAlpha() for an explanation.
                float alpha_deassociate = pow((float)max / data[alpha_channel],
                                              gamma);
                for (int c = 0;  c < channels;  c++)
                    if (c != alpha_channel)
                        data[c] = static_cast<T> (std::min (max,
                                (unsigned int)(data[c] * alpha_deassociate)));
            }
    }
}



bool
Jpeg2000Output::write_scanline (int y, int z, TypeDesc format,
                                const void *data, stride_t xstride)
{
    y -= m_spec.y;
    if (y > m_spec.height) {
        error ("Attempt to write too many scanlines to %s", m_filename);
        return false;
    }

    m_spec.auto_stride (xstride, format, spec().nchannels);
    const void *origdata = data;
    data = to_native_scanline (format, data, xstride, m_scratch,
                               m_dither, y, z);
    if (data == origdata) {
        m_scratch.assign ((unsigned char *)data,
                          (unsigned char *)data+m_spec.scanline_bytes());
        data = &m_scratch[0];
    }

    // JPEG-2000 specifically dictates unassociated (un-"premultiplied") alpha
    if (m_convert_alpha) {
        if (m_spec.format == TypeDesc::UINT16)
            deassociateAlpha ((unsigned short *)data, m_spec.width,
                              m_spec.nchannels, m_spec.alpha_channel, 2.2f);
        else
            deassociateAlpha ((unsigned char *)data, m_spec.width,
                              m_spec.nchannels, m_spec.alpha_channel, 2.2f);
    }

    if (m_spec.format == TypeDesc::UINT8)
        write_scanline<uint8_t>(y, z, data);
    else
        write_scanline<uint16_t>(y, z, data);

    if (y == m_spec.height - 1)
        save_image();

    return true;
}



bool
Jpeg2000Output::write_tile (int x, int y, int z, TypeDesc format,
                       const void *data, stride_t xstride,
                       stride_t ystride, stride_t zstride)
{
    // Emulate tiles by buffering the whole image
    return copy_tile_to_image_buffer (x, y, z, format, data, xstride,
                                      ystride, zstride, &m_tilebuffer[0]);
}



bool
Jpeg2000Output::close ()
{
    if (! m_stream) {         // Already closed
        return true;
    }

    bool ok = true;
    if (m_spec.tile_width) {
        // We've been emulating tiles; now dump as scanlines.
        ASSERT (m_tilebuffer.size());
        ok &= write_scanlines (m_spec.y, m_spec.y+m_spec.height, 0,
                               m_spec.format, &m_tilebuffer[0]);
        std::vector<unsigned char>().swap (m_tilebuffer);
    }

    if (m_image) {
        opj_image_destroy(m_image);
        m_image = NULL;
    }
    destroy_compressor ();
    destroy_stream ();
    if (m_file) {
        fclose (m_file);
        m_file = NULL;
    }
    return ok;
}



bool
Jpeg2000Output::save_image()
{
    m_codec = create_compressor();
    if (!m_codec)
        return false;

    opj_set_error_handler (m_codec, openjpeg_error_callback, this);
    opj_set_warning_handler (m_codec, openjpeg_dummy_callback, NULL);
    opj_set_info_handler (m_codec, openjpeg_dummy_callback, NULL);

    opj_setup_encoder(m_codec, &m_compression_parameters, m_image);

#if defined(OPJ_VERSION_MAJOR)
    // OpenJpeg >= 2.1
    m_stream = opj_stream_create_default_file_stream (m_filename.c_str(), false);
#else
    // OpenJpeg 2.0: need to open a stream ourselves
    m_file = Filesystem::fopen (m_filename, "wb");
    m_stream = opj_stream_create_default_file_stream (m_file, false);
#endif
    if (! m_stream) {
        error ("Failed write jpeg2000::save_image");
        return false;
    }

    if (! opj_start_compress (m_codec, m_image, m_stream) ||
        ! opj_encode (m_codec, m_stream) ||
        ! opj_end_compress (m_codec, m_stream)) {
        error ("Failed write jpeg2000::save_image");
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

    int precision = 16;
    const ParamValue *prec = m_spec.find_attribute ("oiio:BitsPerSample",
                                                          TypeDesc::INT);
    if (prec)
        precision = *(int*)prec->data();
    else if (m_spec.format == TypeDesc::UINT8 || m_spec.format == TypeDesc::INT8)
        precision = 8;

    const int MAX_COMPONENTS = 4;
    opj_image_cmptparm_t component_params[MAX_COMPONENTS];
    init_components(component_params, precision);

    m_image = opj_image_create(m_spec.nchannels, &component_params[0], color_space);

    m_image->x0 = m_compression_parameters.image_offset_x0;
    m_image->y0 = m_compression_parameters.image_offset_y0;
    m_image->x1 = m_compression_parameters.image_offset_x0 + (m_spec.width - 1) * m_compression_parameters.subsampling_dx + 1;
    m_image->y1 = m_compression_parameters.image_offset_y0 + (m_spec.height - 1) * m_compression_parameters.subsampling_dy + 1;

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
Jpeg2000Output::init_components(opj_image_cmptparm_t *components, int precision)
{
    memset(components, 0x00, m_spec.nchannels * sizeof(opj_image_cmptparm_t));
    for(int i = 0; i < m_spec.nchannels; i++)
    {
        components[i].dx = m_compression_parameters.subsampling_dx;
        components[i].dy = m_compression_parameters.subsampling_dy;
        components[i].w = m_spec.width;
        components[i].h = m_spec.height;
        components[i].prec = precision;
        components[i].bpp = precision;
        components[i].sgnd = 0;
    }
}


opj_codec_t*
Jpeg2000Output::create_compressor()
{
    std::string ext = Filesystem::extension(m_filename);
    opj_codec_t *compressor = NULL;
    if (ext == ".j2k")
        compressor = opj_create_compress (OPJ_CODEC_J2K);
    else if (ext == ".jp2")
        compressor = opj_create_compress (OPJ_CODEC_JP2);

    return compressor;
}



template<typename T>
void
Jpeg2000Output::write_scanline(int y, int z, const void *data)
{
    int bits = sizeof(T)*8;
    const T* scanline = static_cast<const T*>(data);
    const size_t scanline_pos =  (y - m_spec.y) * m_spec.width;
    for (int i = 0, j = 0; i < m_spec.width; i++) {
        for (int c = 0; c < m_spec.nchannels; ++c) {
            unsigned int val = scanline[j++];
            if (bits != int(m_image->comps[c].prec))
                val = bit_range_convert (val, bits, m_image->comps[c].prec);
            m_image->comps[c].data[scanline_pos + i] = val;
        }
    }
}



void Jpeg2000Output::setup_cinema_compression(OPJ_RSIZ_CAPABILITIES p_rsizCap)
{
    m_compression_parameters.tile_size_on = false;
    m_compression_parameters.cp_tdx=1;
    m_compression_parameters.cp_tdy=1;

    m_compression_parameters.tp_flag = 'C';
    m_compression_parameters.tp_on = 1;

    m_compression_parameters.cp_tx0 = 0;
    m_compression_parameters.cp_ty0 = 0;
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
        m_compression_parameters.cp_cinema = OPJ_CINEMA4K_24;
        m_compression_parameters.POC[0].tile  = 1; 
        m_compression_parameters.POC[0].resno0  = 0; 
        m_compression_parameters.POC[0].compno0 = 0;
        m_compression_parameters.POC[0].layno1  = 1;
        m_compression_parameters.POC[0].resno1  = m_compression_parameters.numresolution-1;
        m_compression_parameters.POC[0].compno1 = 3;
        m_compression_parameters.POC[0].prg1 = OPJ_CPRL;
        m_compression_parameters.POC[1].tile  = 1;
        m_compression_parameters.POC[1].resno0  = m_compression_parameters.numresolution-1; 
        m_compression_parameters.POC[1].compno0 = 0;
        m_compression_parameters.POC[1].layno1  = 1;
        m_compression_parameters.POC[1].resno1  = m_compression_parameters.numresolution;
        m_compression_parameters.POC[1].compno1 = 3;
        m_compression_parameters.POC[1].prg1 = OPJ_CPRL;
    }
    else if (p_rsizCap == OPJ_CINEMA2K) {
        m_compression_parameters.cp_cinema = OPJ_CINEMA2K_24;
    }
}


void Jpeg2000Output::setup_compression_params()
{
    opj_set_default_encoder_parameters(&m_compression_parameters);
    m_compression_parameters.tcp_rates[0] = 0;
    m_compression_parameters.tcp_numlayers++;
    m_compression_parameters.cp_disto_alloc = 1;

    const ParamValue *is_cinema2k = m_spec.find_attribute ("jpeg2000:Cinema2K",
                                                                 TypeDesc::UINT);
    if (is_cinema2k)
        setup_cinema_compression(OPJ_CINEMA2K);

    const ParamValue *is_cinema4k = m_spec.find_attribute ("jpeg2000:Cinema4K",
                                                                 TypeDesc::UINT);
    if (is_cinema4k)
        setup_cinema_compression(OPJ_CINEMA4K);

    const ParamValue *initial_cb_width = m_spec.find_attribute ("jpeg2000:InitialCodeBlockWidth",
                                                                      TypeDesc::UINT);
    if (initial_cb_width && initial_cb_width->data())
        m_compression_parameters.cblockw_init = *(unsigned int*)initial_cb_width->data();

    const ParamValue *initial_cb_height = m_spec.find_attribute ("jpeg2000:InitialCodeBlockHeight",
                                                                       TypeDesc::UINT);
    if (initial_cb_height && initial_cb_height->data())
        m_compression_parameters.cblockh_init = *(unsigned int*)initial_cb_height->data();

    const ParamValue *progression_order = m_spec.find_attribute ("jpeg2000:ProgressionOrder",
                                                                       TypeDesc::STRING);
    if (progression_order && progression_order->data()) {
        std::string prog_order((const char*)progression_order->data());
        m_compression_parameters.prog_order = get_progression_order(prog_order);
    }

    const ParamValue *compression_mode = m_spec.find_attribute ("jpeg2000:CompressionMode",
                                                                       TypeDesc::INT);
    if (compression_mode && compression_mode->data())
        m_compression_parameters.mode = *(int*)compression_mode->data();
}

OPJ_PROG_ORDER Jpeg2000Output::get_progression_order(const std::string &progression_order)
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

OIIO_PLUGIN_NAMESPACE_END
