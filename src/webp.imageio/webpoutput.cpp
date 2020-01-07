// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

#include <cstdio>

#include <webp/encode.h>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>

OIIO_PLUGIN_NAMESPACE_BEGIN

namespace webp_pvt {


class WebpOutput final : public ImageOutput {
public:
    WebpOutput() { init(); }
    virtual ~WebpOutput() { close(); }
    virtual const char* format_name() const override { return "webp"; }
    virtual bool open(const std::string& name, const ImageSpec& spec,
                      OpenMode mode = Create) override;
    virtual int supports(string_view feature) const override;
    virtual bool write_scanline(int y, int z, TypeDesc format, const void* data,
                                stride_t xstride) override;
    virtual bool write_tile(int x, int y, int z, TypeDesc format,
                            const void* data, stride_t xstride,
                            stride_t ystride, stride_t zstride) override;
    virtual bool close() override;

private:
    WebPPicture m_webp_picture;
    WebPConfig m_webp_config;
    std::string m_filename;
    FILE* m_file;
    int m_scanline_size;
    unsigned int m_dither;
    std::vector<uint8_t> m_uncompressed_image;

    void init()
    {
        m_scanline_size = 0;
        m_file          = NULL;
    }
};



int
WebpOutput::supports(string_view feature) const
{
    return feature == "tiles" || feature == "alpha"
           || feature == "random_access" || feature == "rewrite";
}



static int
WebpImageWriter(const uint8_t* img_data, size_t data_size,
                const WebPPicture* const webp_img)
{
    FILE* out_file = (FILE*)webp_img->custom_ptr;
    size_t wb      = fwrite(img_data, data_size, sizeof(uint8_t), out_file);
    if (wb != sizeof(uint8_t)) {
        //FIXME Bad write occurred
    }

    return 1;
}



bool
WebpOutput::open(const std::string& name, const ImageSpec& spec, OpenMode mode)
{
    if (mode != Create) {
        errorf("%s does not support subimages or MIP levels", format_name());
        return false;
    }

    // saving 'name' and 'spec' for later use
    m_filename = name;
    m_spec     = spec;

    if (m_spec.nchannels != 3 && m_spec.nchannels != 4) {
        errorf("%s does not support %d-channel images\n", format_name(),
               m_spec.nchannels);
        return false;
    }

    m_file = Filesystem::fopen(m_filename, "wb");
    if (!m_file) {
        errorf("Could not open \"%s\"", m_filename);
        return false;
    }

    if (!WebPPictureInit(&m_webp_picture)) {
        errorf("Couldn't initialize WebPPicture\n");
        close();
        return false;
    }

    m_webp_picture.width      = m_spec.width;
    m_webp_picture.height     = m_spec.height;
    m_webp_picture.writer     = WebpImageWriter;
    m_webp_picture.custom_ptr = (void*)m_file;

    if (!WebPConfigInit(&m_webp_config)) {
        errorf("Couldn't initialize WebPPicture\n");
        close();
        return false;
    }

    auto compqual = m_spec.decode_compression_metadata("webp", 100);
    if (Strutil::iequals(compqual.first, "webp")) {
        m_webp_config.method  = 6;
        m_webp_config.quality = OIIO::clamp(compqual.second, 1, 100);
    } else {
        // If compression name wasn't "webp", don't trust the quality
        // metric, just use the default.
        m_webp_config.method  = 6;
        m_webp_config.quality = 100;
    }

    // forcing UINT8 format
    m_spec.set_format(TypeDesc::UINT8);
    m_dither = m_spec.get_int_attribute("oiio:dither", 0);

    m_scanline_size        = m_spec.width * m_spec.nchannels;
    const int image_buffer = m_spec.height * m_scanline_size;
    m_uncompressed_image.resize(image_buffer, 0);
    return true;
}


bool
WebpOutput::write_scanline(int y, int z, TypeDesc format, const void* data,
                           stride_t xstride)
{
    if (y > m_spec.height) {
        errorf("Attempt to write too many scanlines to %s", m_filename);
        close();
        return false;
    }
    std::vector<uint8_t> scratch;
    data = to_native_scanline(format, data, xstride, scratch, m_dither, y, z);
    memcpy(&m_uncompressed_image[y * m_scanline_size], data, m_scanline_size);

    if (y == m_spec.height - 1) {
        if (m_spec.nchannels == 4) {
            // WebP requires unassociated alpha, and it's sRGB.
            // Handle this all by wrapping an IB around it.
            ImageSpec specwrap(m_spec.width, m_spec.height, 4, TypeUInt8);
            ImageBuf bufwrap(specwrap, &m_uncompressed_image[0]);
            ROI rgbroi(0, m_spec.width, 0, m_spec.height, 0, 1, 0, 3);
            ImageBufAlgo::pow(bufwrap, bufwrap, 2.2f, rgbroi);
            ImageBufAlgo::unpremult(bufwrap, bufwrap);
            ImageBufAlgo::pow(bufwrap, bufwrap, 1.0f / 2.2f, rgbroi);
            WebPPictureImportRGBA(&m_webp_picture, &m_uncompressed_image[0],
                                  m_scanline_size);
        } else {
            WebPPictureImportRGB(&m_webp_picture, &m_uncompressed_image[0],
                                 m_scanline_size);
        }
        if (!WebPEncode(&m_webp_config, &m_webp_picture)) {
            errorf("Failed to encode %s as WebP image", m_filename);
            close();
            return false;
        }
    }
    return true;
}



bool
WebpOutput::write_tile(int x, int y, int z, TypeDesc format, const void* data,
                       stride_t xstride, stride_t ystride, stride_t zstride)
{
    // Emulate tiles by buffering the whole image
    return copy_tile_to_image_buffer(x, y, z, format, data, xstride, ystride,
                                     zstride, &m_uncompressed_image[0]);
}



bool
WebpOutput::close()
{
    if (!m_file)
        return true;  // already closed

    bool ok = true;
    if (m_spec.tile_width) {
        // We've been emulating tiles; now dump as scanlines.
        OIIO_DASSERT(m_uncompressed_image.size());
        ok &= write_scanlines(m_spec.y, m_spec.y + m_spec.height, 0,
                              m_spec.format, &m_uncompressed_image[0]);
        std::vector<uint8_t>().swap(m_uncompressed_image);
    }

    WebPPictureFree(&m_webp_picture);
    fclose(m_file);
    m_file = NULL;
    return true;
}


}  // namespace webp_pvt


OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput*
webp_output_imageio_create()
{
    return new webp_pvt::WebpOutput;
}
OIIO_EXPORT const char* webp_output_extensions[] = { "webp", nullptr };

OIIO_PLUGIN_EXPORTS_END

OIIO_PLUGIN_NAMESPACE_END
