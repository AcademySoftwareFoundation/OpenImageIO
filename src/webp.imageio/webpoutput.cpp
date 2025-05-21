// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause and Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

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
    ~WebpOutput() override { close(); }
    const char* format_name() const override { return "webp"; }
    bool open(const std::string& name, const ImageSpec& spec,
              OpenMode mode = Create) override;
    int supports(string_view feature) const override;
    bool write_scanline(int y, int z, TypeDesc format, const void* data,
                        stride_t xstride) override;
    bool write_tile(int x, int y, int z, TypeDesc format, const void* data,
                    stride_t xstride, stride_t ystride,
                    stride_t zstride) override;
    bool close() override;

private:
    WebPPicture m_webp_picture;
    WebPConfig m_webp_config;
    std::string m_filename;
    imagesize_t m_scanline_size;
    unsigned int m_dither;
    bool m_convert_alpha;  // Do we deassociate alpha?
    std::vector<uint8_t> m_uncompressed_image;

    void init()
    {
        m_scanline_size = 0;
        ioproxy_clear();
    }
};



int
WebpOutput::supports(string_view feature) const
{
    return feature == "tiles" || feature == "alpha" || feature == "ioproxy"
           || feature == "random_access" || feature == "rewrite";
}



static int
WebpImageWriter(const uint8_t* img_data, size_t data_size,
                const WebPPicture* const webp_img)
{
    auto io = (Filesystem::IOProxy*)webp_img->custom_ptr;
    return io->write(img_data, data_size) == data_size;
}



bool
WebpOutput::open(const std::string& name, const ImageSpec& spec, OpenMode mode)
{
    if (!check_open(mode, spec, { 0, 1 << 20, 0, 1 << 20, 0, 1, 0, 4 },
                    uint64_t(OpenChecks::Disallow1or2Channel)))
        return false;

    m_filename = name;

    ioproxy_retrieve_from_config(m_spec);
    if (!ioproxy_use_or_open(name))
        return false;

    constexpr int default_lossy_quality   = 100;
    constexpr int default_lossless_effort = 70;

    // Support both 'compression=webp:value' and 'compression=lossless:value'
    // The 'webp' form indicates that lossy compression is requested.
    bool is_lossless = false;
    int quality      = default_lossy_quality;
    auto comp_qual   = m_spec.decode_compression_metadata("webp",
                                                          default_lossy_quality);
    if (Strutil::iequals(comp_qual.first, "webp")) {
        quality = OIIO::clamp(comp_qual.second, 0, 100);
    } else {
        comp_qual = m_spec.decode_compression_metadata("lossless",
                                                       default_lossless_effort);
        if (Strutil::iequals(comp_qual.first, "lossless")) {
            is_lossless = true;
            quality     = OIIO::clamp(comp_qual.second, 0, 100);
        }
    }

    if (!WebPConfigPreset(&m_webp_config, WEBP_PRESET_DEFAULT, quality)) {
        errorfmt("Couldn't initialize WebPConfig\n");
        close();
        return false;
    }

    if (!WebPPictureInit(&m_webp_picture)) {
        errorfmt("Couldn't initialize WebPPicture\n");
        close();
        return false;
    }

    // Quality/speed trade-off (0=fast, 6=slower-better)
    const int method     = m_spec.get_int_attribute("webp:method", 6);
    m_webp_config.method = OIIO::clamp(method, 0, 6);

    // Lossless encoding (0=lossy(default), 1=lossless).
    m_webp_config.lossless = int(is_lossless);

    m_webp_picture.use_argb   = m_webp_config.lossless;
    m_webp_picture.width      = m_spec.width;
    m_webp_picture.height     = m_spec.height;
    m_webp_picture.writer     = WebpImageWriter;
    m_webp_picture.custom_ptr = (void*)ioproxy();

    // forcing UINT8 format
    m_spec.set_format(TypeDesc::UINT8);
    m_dither        = m_spec.get_int_attribute("oiio:dither", 0);
    m_convert_alpha = m_spec.alpha_channel != -1
                      && !m_spec.get_int_attribute("oiio:UnassociatedAlpha", 0);

    m_scanline_size = m_spec.scanline_bytes();
    m_uncompressed_image.resize(m_spec.image_bytes(), 0);
    return true;
}


bool
WebpOutput::write_scanline(int y, int z, TypeDesc format, const void* data,
                           stride_t xstride)
{
    if (y > m_spec.height) {
        errorfmt("Attempt to write too many scanlines to {}", m_filename);
        close();
        return false;
    }
    std::vector<uint8_t> scratch;
    data = to_native_scanline(format, data, xstride, scratch, m_dither, y, z);
    memcpy(&m_uncompressed_image[y * m_scanline_size], data, m_scanline_size);

    if (y == m_spec.height - 1) {
        if (m_spec.nchannels == 4) {
            if (m_convert_alpha) {
                // WebP requires unassociated alpha, and it's sRGB.
                // Handle this all by wrapping an IB around it.
                ImageSpec specwrap(m_spec.width, m_spec.height, 4, TypeUInt8);
                ImageBuf bufwrap(specwrap,
                                 cspan<uint8_t>(m_uncompressed_image));
                ROI rgbroi(0, m_spec.width, 0, m_spec.height, 0, 1, 0, 3);
                ImageBufAlgo::pow(bufwrap, bufwrap, 2.2f, rgbroi);
                ImageBufAlgo::unpremult(bufwrap, bufwrap);
                ImageBufAlgo::pow(bufwrap, bufwrap, 1.0f / 2.2f, rgbroi);
            }

            WebPPictureImportRGBA(&m_webp_picture, m_uncompressed_image.data(),
                                  m_scanline_size);
        } else {
            WebPPictureImportRGB(&m_webp_picture, m_uncompressed_image.data(),
                                 m_scanline_size);
        }

        if (!WebPEncode(&m_webp_config, &m_webp_picture)) {
            errorfmt("Failed to encode {} as WebP image", m_filename);
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
    if (!ioproxy_opened())
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
    init();
    return ok;
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
