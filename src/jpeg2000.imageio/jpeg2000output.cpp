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
#include "filesystem.h"
#include "fmath.h"
#include "imageio.h"

OIIO_PLUGIN_NAMESPACE_BEGIN


static void openjpeg_dummy_callback(const char*, void*) {}

class Jpeg2000Output : public ImageOutput {
 public:
    Jpeg2000Output () { init (); }
    virtual ~Jpeg2000Output () { close (); }
    virtual const char *format_name (void) const { return "jpeg2000"; }
    virtual bool supports (const std::string &feature) const { return false; }
    virtual bool open (const std::string &name, const ImageSpec &spec,
                       OpenMode mode=Create);
    virtual bool close ();
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride);
 private:
    std::string m_filename;
    FILE *m_file;
    opj_cparameters_t m_compression_parameters;
    opj_image_t *m_image;

    void init (void)
    {
        m_file = NULL;
        m_image = NULL;
    }

    opj_image_t* create_jpeg2000_image();

    void init_components(opj_image_cmptparm_t *components, int precision);

    opj_cinfo_t* create_compressor();

    bool save_image();

    uint16_t write_pixel(int p_nativePrecision, int p_pixelData);

    template<typename T>
    void write_scanline(int y, int z, const void *data);

    void setup_cinema_compression(OPJ_RSIZ_CAPABILITIES p_rsizCap);

    void setup_compression_params();

    OPJ_PROG_ORDER get_progression_order(const std::string& progression_order);
};


// Obligatory material to make this a recognizeable imageio plugin
OIIO_PLUGIN_EXPORTS_BEGIN

    DLLEXPORT ImageOutput *jpeg2000_output_imageio_create () {
        return new Jpeg2000Output;
    }
    DLLEXPORT const char *jpeg2000_output_extensions[] = {
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

    m_spec = spec;
    m_filename = name;

    m_file = fopen (m_filename.c_str(), "wb");
    if (m_file == NULL) {
        error ("Unable to open file \"%s\"", m_filename.c_str());
        return false;
    }

    m_image = create_jpeg2000_image();
    return true;
}



bool
Jpeg2000Output::write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride)
{
    if (y > m_spec.height) {
        error ("Attempt to write too many scanlines to %s", m_filename.c_str());
        close ();
        return false;
    }

    std::vector<uint8_t> scratch;
    data = to_native_scanline (format, data, xstride, scratch);
    if (m_spec.format == TypeDesc::UINT8)
        write_scanline<uint8_t>(y, z, data);
    else
        write_scanline<uint16_t>(y, z, data);

    if (y == m_spec.height - 1)
        save_image();

    return true;
}


bool
Jpeg2000Output::close ()
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



bool
Jpeg2000Output::save_image()
{
    opj_cinfo_t* compressor = create_compressor();
    if (!compressor)
        return false;

    opj_event_mgr_t event_mgr;
    event_mgr.error_handler = openjpeg_dummy_callback;
    event_mgr.warning_handler = openjpeg_dummy_callback;
    event_mgr.info_handler = openjpeg_dummy_callback;
    opj_set_event_mgr((opj_common_ptr)compressor, &event_mgr, NULL);

    opj_setup_encoder(compressor, &m_compression_parameters, m_image);

    opj_cio_t *cio = opj_cio_open((opj_common_ptr)compressor, NULL, 0);

    opj_encode(compressor, cio, m_image, NULL);

    fwrite(cio->buffer, 1, cio_tell(cio), m_file);

    opj_destroy_compress(compressor);
    opj_cio_close(cio);

    return true;
}


opj_image_t*
Jpeg2000Output::create_jpeg2000_image()
{
    setup_compression_params();

    OPJ_COLOR_SPACE color_space = CLRSPC_SRGB;
    if (m_spec.nchannels == 1)
        color_space = CLRSPC_GRAY;

    int precision = 16;
    const ImageIOParameter *prec = m_spec.find_attribute ("oiio:BitsPerSample",
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


opj_cinfo_t*
Jpeg2000Output::create_compressor()
{
    std::string ext = Filesystem::extension(m_filename);
    opj_cinfo_t *compressor = NULL;
    if (ext == ".j2k")
        compressor = opj_create_compress(CODEC_J2K);
    else if (ext == ".jp2")
        compressor = opj_create_compress(CODEC_JP2);

    return compressor;
}


template<typename T>
void
Jpeg2000Output::write_scanline(int y, int z, const void *data)
{
    const T* scanline = static_cast<const T*>(data);
    const size_t scanline_pos =  y * m_spec.width;
    if (m_spec.nchannels == 1) {
        for (int i = 0; i < m_spec.width; i++)
        {
            m_image->comps[0].data[scanline_pos + i] = write_pixel(m_image->comps[0].prec, scanline[i]);
        }
        return;
    }

    for (int i = 0, j = 0; i < m_spec.width; i++)
    {
        m_image->comps[0].data[scanline_pos + i] = write_pixel(m_image->comps[0].prec, scanline[j++]);
        m_image->comps[1].data[scanline_pos + i] = write_pixel(m_image->comps[0].prec, scanline[j++]);
        m_image->comps[2].data[scanline_pos + i] = write_pixel(m_image->comps[0].prec, scanline[j++]);

        if (m_spec.nchannels < 4)
            continue;

        m_image->comps[3].data[scanline_pos + i] = write_pixel(m_image->comps[0].prec, scanline[j++]);
    }
}

inline uint16_t
Jpeg2000Output::write_pixel(int p_nativePrecision, int p_pixelData)
{
    if (p_nativePrecision == 10)
        return p_pixelData >> 6;
    if (p_nativePrecision == 12)
        return p_pixelData >> 4;
    return p_pixelData;
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

    m_compression_parameters.prog_order = CPRL;

    m_compression_parameters.roi_compno = -1;

    m_compression_parameters.subsampling_dx = 1;
    m_compression_parameters.subsampling_dy = 1;

    m_compression_parameters.irreversible = 1;

    m_compression_parameters.cp_rsiz = p_rsizCap;
    if (p_rsizCap == CINEMA4K) {
        m_compression_parameters.cp_cinema = CINEMA4K_24;
        m_compression_parameters.POC[0].tile  = 1; 
        m_compression_parameters.POC[0].resno0  = 0; 
        m_compression_parameters.POC[0].compno0 = 0;
        m_compression_parameters.POC[0].layno1  = 1;
        m_compression_parameters.POC[0].resno1  = m_compression_parameters.numresolution-1;
        m_compression_parameters.POC[0].compno1 = 3;
        m_compression_parameters.POC[0].prg1 = CPRL;
        m_compression_parameters.POC[1].tile  = 1;
        m_compression_parameters.POC[1].resno0  = m_compression_parameters.numresolution-1; 
        m_compression_parameters.POC[1].compno0 = 0;
        m_compression_parameters.POC[1].layno1  = 1;
        m_compression_parameters.POC[1].resno1  = m_compression_parameters.numresolution;
        m_compression_parameters.POC[1].compno1 = 3;
        m_compression_parameters.POC[1].prg1 = CPRL;
    }
    else if (p_rsizCap == CINEMA2K) {
        m_compression_parameters.cp_cinema = CINEMA2K_24;
    }
}


void Jpeg2000Output::setup_compression_params()
{
    opj_set_default_encoder_parameters(&m_compression_parameters);
    m_compression_parameters.tcp_rates[0] = 0;
    m_compression_parameters.tcp_numlayers++;
    m_compression_parameters.cp_disto_alloc = 1;

    const ImageIOParameter *is_cinema2k = m_spec.find_attribute ("jpeg2000:Cinema2K",
                                                                 TypeDesc::UINT);
    if (is_cinema2k)
        setup_cinema_compression(CINEMA2K);

    const ImageIOParameter *is_cinema4k = m_spec.find_attribute ("jpeg2000:Cinema4K",
                                                                 TypeDesc::UINT);
    if (is_cinema4k)
        setup_cinema_compression(CINEMA4K);

    const ImageIOParameter *initial_cb_width = m_spec.find_attribute ("jpeg2000:InitialCodeBlockWidth",
                                                                      TypeDesc::UINT);
    if (initial_cb_width && initial_cb_width->data())
        m_compression_parameters.cblockw_init = *(unsigned int*)initial_cb_width->data();

    const ImageIOParameter *initial_cb_height = m_spec.find_attribute ("jpeg2000:InitialCodeBlockHeight",
                                                                       TypeDesc::UINT);
    if (initial_cb_height && initial_cb_height->data())
        m_compression_parameters.cblockh_init = *(unsigned int*)initial_cb_height->data();

    const ImageIOParameter *progression_order = m_spec.find_attribute ("jpeg2000:ProgressionOrder",
                                                                       TypeDesc::STRING);
    if (progression_order && progression_order->data()) {
        std::string prog_order((const char*)progression_order->data());
        m_compression_parameters.prog_order = get_progression_order(prog_order);
    }

    const ImageIOParameter *compression_mode = m_spec.find_attribute ("jpeg2000:CompressionMode",
                                                                       TypeDesc::INT);
    if (compression_mode && compression_mode->data())
        m_compression_parameters.mode = *(int*)compression_mode->data();
}

OPJ_PROG_ORDER Jpeg2000Output::get_progression_order(const std::string &progression_order)
{
    if (progression_order == "LRCP")
        return LRCP;
    else if (progression_order == "RLCP")
        return RLCP;
    else if (progression_order == "RPCL")
        return RPCL;
    else if (progression_order == "PCRL")
        return PCRL;
    else if (progression_order == "PCRL")
        return CPRL;
    return PROG_UNKNOWN;
}

OIIO_PLUGIN_NAMESPACE_END
