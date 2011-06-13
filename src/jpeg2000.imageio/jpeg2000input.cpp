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
#include "fmath.h"
#include "imageio.h"



OIIO_PLUGIN_NAMESPACE_BEGIN


static void openjpeg_dummy_callback(const char*, void*) {}


class Jpeg2000Input : public ImageInput {
 public:
    Jpeg2000Input () { init (); }
    virtual ~Jpeg2000Input () { close (); }
    virtual const char *format_name (void) const { return "jpeg2000"; }
    virtual bool open (const std::string &name, ImageSpec &spec);
    virtual bool close (void);
    virtual bool read_native_scanline (int y, int z, void *data);

 private:
    std::string m_filename;
    int m_maxPrecision;
    opj_image_t *m_image;
    FILE *m_file;

    void init (void);

    bool isJp2File(const int* const p_magicTable) const;

    opj_dinfo_t* create_decompressor();

    template<typename T>
    void read_scanline(int y, int z, void *data);

    uint16_t read_pixel(int p_precision, int p_PixelData);

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
        const imagesize_t scanline_size = m_spec.scanline_bytes();
        for (imagesize_t i = 0; i < scanline_size; i += 3)
        {
            T red = 1.164f * (p_scanline[i+2] - 16.0f) + 1.596f * (p_scanline[i] - 128.0f);
            T green = 1.164f * (p_scanline[i+2] - 16.0f) - 0.813 * (p_scanline[i] - 128.0f) - 0.391f * (p_scanline[i+1] - 128.0f);
            T blue = 1.164f * (p_scanline[i+2] - 16.0f) + 2.018f * (p_scanline[i+1] - 128.0f);
            p_scanline[i] = red;
            p_scanline[i+1] = green;
            p_scanline[i+2] = blue;
        } 
    }

    void setup_event_mgr(opj_dinfo_t* p_decompressor)
    {
        opj_event_mgr_t event_mgr;
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

    DLLEXPORT int jpeg2000_imageio_version = OIIO_PLUGIN_VERSION;
    DLLEXPORT ImageInput *jpeg2000_input_imageio_create () {
        return new Jpeg2000Input;
    }
    DLLEXPORT const char *jpeg2000_input_extensions[] = {
        "jp2", "j2k", "j2c", NULL
    };

OIIO_PLUGIN_EXPORTS_END


void
Jpeg2000Input::init (void)
{
    m_file = NULL;
    m_image = NULL;
}


bool
Jpeg2000Input::open (const std::string &p_name, ImageSpec &p_spec)
{
    m_filename = p_name;
    m_file = fopen(m_filename.c_str(), "rb");
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

    setup_event_mgr(decompressor);

    opj_dparameters_t parameters;
    opj_set_default_decoder_parameters(&parameters);
    opj_setup_decoder(decompressor, &parameters);

    const size_t fileLength = get_file_length(m_file);
    std::vector<uint8_t> fileContent(fileLength+1, 0);
    fread(&fileContent[0], sizeof(uint8_t), fileLength);

    opj_cio_t *cio = opj_cio_open((opj_common_ptr)decompressor, &fileContent[0], fileLength);
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

    m_maxPrecision = 0;
    for(int i = 0; i < channelCount; i++)
    {
        m_maxPrecision = std::max(m_image->comps[i].prec, m_maxPrecision);
    }

    const TypeDesc format = (m_maxPrecision <= 8) ? TypeDesc::UINT8
                                                  : TypeDesc::UINT16;


    m_spec = ImageSpec(m_image->comps[0].w, m_image->comps[0].h, channelCount, format);
    m_spec.attribute ("oiio:BitsPerSample", (unsigned int)m_maxPrecision);
    m_spec.attribute ("oiio:Orientation", (unsigned int)1);

    p_spec = m_spec;
    return true;
}


bool
Jpeg2000Input::read_native_scanline (int y, int z, void *data)
{
    if (m_spec.format == TypeDesc::UINT8)
        read_scanline<uint8_t>(y, z, data);
    else
        read_scanline<uint16_t>(y, z, data);
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
        return false;
    }
    opj_dinfo_t* dinfo = NULL;
    if (isJp2File(magic))
        dinfo = opj_create_decompress(CODEC_JP2);
    else
        dinfo = opj_create_decompress(CODEC_J2K);
    rewind(m_file);
    return dinfo;
}


inline uint16_t
Jpeg2000Input::read_pixel(int p_nativePrecision, int p_pixelData)
{
    if (p_nativePrecision == 10)
        return baseTypeConvertU10ToU16(p_pixelData);
    if (p_nativePrecision == 12)
        return baseTypeConvertU12ToU16(p_pixelData);
    return p_pixelData;
}


template<typename T>
void
Jpeg2000Input::read_scanline(int y, int z, void *data)
{
    T* scanline = static_cast<T*>(data);
    if (m_spec.nchannels == 1) {
        for (int i = 0; i < m_spec.width; i++)
        {
            scanline[i] = read_pixel(m_image->comps[0].prec, m_image->comps[0].data[y*m_spec.width + i]);
        }
        return;
    }

    for (int i = 0, j = 0; i < m_spec.width; i++)
    {
        if (y % m_image->comps[0].dy == 0 && i % m_image->comps[0].dx == 0) {
            const size_t data_offset = y/m_image->comps[0].dy * m_spec.width/m_image->comps[0].dx + i/m_image->comps[0].dx;
            scanline[j++] = read_pixel(m_image->comps[0].prec, m_image->comps[0].data[data_offset]);
        }
        else {
            scanline[j++] = 0;
        }

        if (y % m_image->comps[1].dy == 0 && i % m_image->comps[1].dx == 0) {
            const size_t data_offset = y/m_image->comps[1].dy * m_spec.width/m_image->comps[1].dx + i/m_image->comps[1].dx;
            scanline[j++] = read_pixel(m_image->comps[1].prec, m_image->comps[1].data[data_offset]);
        }
        else {
            scanline[j++] = 0;
        }

        if (y % m_image->comps[2].dy == 0 && i % m_image->comps[2].dx == 0) {
            const size_t data_offset = y/m_image->comps[2].dy * m_spec.width/m_image->comps[2].dx + i/m_image->comps[2].dx;
            scanline[j++] = read_pixel(m_image->comps[2].prec, m_image->comps[2].data[data_offset]);
        }
        else {
            scanline[j++] = 0;
        }

        if (m_spec.nchannels < 4) {
            continue;
        }

        if (y % m_image->comps[3].dy == 0 && i % m_image->comps[3].dx == 0) {
            const size_t data_offset = y/m_image->comps[3].dy * m_spec.width/m_image->comps[3].dx + i/m_image->comps[3].dx;
            scanline[j++] = read_pixel(m_image->comps[3].prec, m_image->comps[3].data[data_offset]);
        }
        else {
            scanline[j++] = 0;
        }
    }
    if (m_image->color_space == CLRSPC_SYCC)
        yuv_to_rgb(scanline);
}


OIIO_PLUGIN_NAMESPACE_END

