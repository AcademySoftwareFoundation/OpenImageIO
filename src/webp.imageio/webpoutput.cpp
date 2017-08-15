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
#include <webp/encode.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>

OIIO_PLUGIN_NAMESPACE_BEGIN

namespace webp_pvt {


class WebpOutput final : public ImageOutput
{
 public:
    WebpOutput(){ init(); }
    virtual ~WebpOutput(){ close(); }
    virtual const char* format_name () const { return "webp"; }
    virtual bool open (const std::string &name, const ImageSpec &spec,
                       OpenMode mode=Create);
    virtual int supports (string_view feature) const;
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride);
    virtual bool write_tile (int x, int y, int z, TypeDesc format,
                             const void *data, stride_t xstride,
                             stride_t ystride, stride_t zstride);
    virtual bool close();

 private:
    WebPPicture m_webp_picture;
    WebPConfig m_webp_config;
    std::string m_filename;
    FILE *m_file;
    int m_scanline_size;
    unsigned int m_dither;
    std::vector<uint8_t> m_uncompressed_image;

    void init()
    {
        m_scanline_size = 0;
        m_file = NULL;
    }
};



int
WebpOutput::supports (string_view feature) const
{
    return feature == "tiles"
        || feature == "alpha"
        || feature == "random_access"
        || feature == "rewrite";
}



static int WebpImageWriter(const uint8_t* img_data, size_t data_size,
                           const WebPPicture* const webp_img)
{
    FILE *out_file = (FILE*)webp_img->custom_ptr;
    size_t wb = fwrite (img_data, data_size, sizeof(uint8_t), out_file);
	if (wb != sizeof(uint8_t)) {
		//FIXME Bad write occurred
	}

    return 1;
}



bool
WebpOutput::open (const std::string &name, const ImageSpec &spec,
                 OpenMode mode)
{
    if (mode != Create) {
        error ("%s does not support subimages or MIP levels", format_name());
        return false;
    }

    // saving 'name' and 'spec' for later use
    m_filename = name;
    m_spec = spec;

    if (m_spec.nchannels != 3 && m_spec.nchannels != 4) {
        error ("%s does not support %d-channel images\n",
               format_name(), m_spec.nchannels);
        return false;
    }

    m_file = Filesystem::fopen (m_filename, "wb");
    if (!m_file) {
        error ("Unable to open file \"%s\"", m_filename.c_str ());
        return false;
    }

    if (!WebPPictureInit(&m_webp_picture))
    {
        error("Couldn't initialize WebPPicture\n");
        close();
        return false;
    }

    m_webp_picture.width = m_spec.width;
    m_webp_picture.height = m_spec.height;
    m_webp_picture.writer = WebpImageWriter;
    m_webp_picture.custom_ptr = (void*)m_file;

    if (!WebPConfigInit(&m_webp_config))
    {
        error("Couldn't initialize WebPPicture\n");
        close();
        return false;
    }

    m_webp_config.method = 6;
    int compression_quality = 100;
    const ParamValue *qual = m_spec.find_attribute ("CompressionQuality",
                                                          TypeDesc::INT);
    if (qual)
    {
        compression_quality = *static_cast<const int*>(qual->data());
    }
    m_webp_config.quality = compression_quality;
    
    // forcing UINT8 format
    m_spec.set_format (TypeDesc::UINT8);
    m_dither = m_spec.get_int_attribute ("oiio:dither", 0);

    m_scanline_size = m_spec.width * m_spec.nchannels;
    const int image_buffer = m_spec.height * m_scanline_size;
    m_uncompressed_image.resize(image_buffer, 0);
    return true;
}


bool
WebpOutput::write_scanline (int y, int z, TypeDesc format,
                            const void *data, stride_t xstride)
{
    if (y > m_spec.height)
    {
        error ("Attempt to write too many scanlines to %s", m_filename.c_str());
        close ();
        return false;
    }
    std::vector<uint8_t> scratch;
    data = to_native_scanline (format, data, xstride, scratch,
                               m_dither, y, z);
    memcpy(&m_uncompressed_image[y*m_scanline_size], data, m_scanline_size);

    if (y == m_spec.height - 1)
    {
        if (m_spec.nchannels == 4)
        {
            WebPPictureImportRGBA(&m_webp_picture, &m_uncompressed_image[0], m_scanline_size);
        }
        else
        {
            WebPPictureImportRGB(&m_webp_picture, &m_uncompressed_image[0], m_scanline_size);
        }
        if (!WebPEncode(&m_webp_config, &m_webp_picture))
        {
            error ("Failed to encode %s as WebP image", m_filename.c_str());
            close();
            return false;
        }
    }
    return true;    
}



bool
WebpOutput::write_tile (int x, int y, int z, TypeDesc format,
                        const void *data, stride_t xstride,
                        stride_t ystride, stride_t zstride)
{
    // Emulate tiles by buffering the whole image
    return copy_tile_to_image_buffer (x, y, z, format, data, xstride,
                                      ystride, zstride, &m_uncompressed_image[0]);
}



bool
WebpOutput::close()
{
    if (! m_file)
        return true;   // already closed

    bool ok = true;
    if (m_spec.tile_width) {
        // We've been emulating tiles; now dump as scanlines.
        ASSERT (m_uncompressed_image.size());
        ok &= write_scanlines (m_spec.y, m_spec.y+m_spec.height, 0,
                               m_spec.format, &m_uncompressed_image[0]);
        std::vector<uint8_t>().swap (m_uncompressed_image);
    }

    WebPPictureFree(&m_webp_picture);
    fclose(m_file);
    m_file = NULL;
    return true;
}


} // namespace webp_pvt


OIIO_PLUGIN_EXPORTS_BEGIN

    OIIO_EXPORT ImageOutput *webp_output_imageio_create () {
        return new webp_pvt::WebpOutput;
    }
    OIIO_EXPORT const char *webp_output_extensions[] = {
        "webp", NULL
    };

OIIO_PLUGIN_EXPORTS_END

OIIO_PLUGIN_NAMESPACE_END
