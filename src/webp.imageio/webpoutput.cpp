// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <cstdio>

#include <webp/encode.h>
#include <webp/mux.h>

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
    // Used to write animations
    WebPMux* m_mux       = nullptr;
    bool m_pending_write = false;
    // Used to make sure that we don't call WebPPictureFree on an uninitialized WebPPicture.
    bool m_picture_initialized = false;
    float m_default_fps        = 1.0f;

    void init()
    {
        m_scanline_size       = 0;
        m_pending_write       = false;
        m_picture_initialized = false;
        ioproxy_clear();
    }

    bool write_complete_data();
    bool append_subimage(const std::string& name, const ImageSpec& spec);
    bool finish_subimage();
    void cleanup();
};



int
WebpOutput::supports(string_view feature) const
{
    return feature == "tiles" || feature == "alpha" || feature == "ioproxy"
           || feature == "random_access" || feature == "rewrite"
           || feature == "multiimage" || feature == "appendsubimage";
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
    if (mode == AppendSubimage) {
        return append_subimage(name, spec);
    }

    if (!check_open(mode, spec, { 0, 16383, 0, 16383, 0, 1, 0, 4 },
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
    auto comp_qual = m_spec.decode_compression_metadata("webp",
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
        cleanup();
        return false;
    }

    if (!WebPPictureInit(&m_webp_picture)) {
        errorfmt("Couldn't initialize WebPPicture\n");
        cleanup();
        return false;
    }
    m_picture_initialized = true;

    // Quality/speed trade-off (0=fast, 6=slower-better)
    const int method     = m_spec.get_int_attribute("webp:method", 6);
    m_webp_config.method = OIIO::clamp(method, 0, 6);

    // Lossless encoding (0=lossy(default), 1=lossless).
    m_webp_config.lossless = int(is_lossless);

    m_webp_picture.use_argb = m_webp_config.lossless;
    m_webp_picture.width    = m_spec.width;
    m_webp_picture.height   = m_spec.height;

    // forcing UINT8 format
    m_spec.set_format(TypeDesc::UINT8);
    m_dither        = m_spec.get_int_attribute("oiio:dither", 0);
    m_convert_alpha = m_spec.alpha_channel != -1
                      && !m_spec.get_int_attribute("oiio:UnassociatedAlpha", 0);

    m_scanline_size = m_spec.scanline_bytes();
    m_uncompressed_image.resize(m_spec.image_bytes(), 0);
    m_default_fps   = m_spec.get_float_attribute("FramesPerSecond", 1.0f);
    m_pending_write = true;
    return true;
}


bool
WebpOutput::write_complete_data()
{
    // Check if we have an optional ICC Profile to write.
    std::vector<uint8_t> icc_profile = get_colorspace_icc_profile(m_spec);
    const bool has_icc_data          = icc_profile.size() > 0;

    // If we have ICC data, encode to memory first. This is required in order
    // to use the WebPMux assembly API below.
    WebPMemoryWriter wrt;
    if (has_icc_data || m_mux) {
        WebPMemoryWriterInit(&wrt);
        m_webp_picture.writer     = WebPMemoryWrite;
        m_webp_picture.custom_ptr = &wrt;
    } else {
        m_webp_picture.writer     = WebpImageWriter;
        m_webp_picture.custom_ptr = (void*)ioproxy();
    }

    if (m_spec.nchannels == 4) {
        if (m_convert_alpha) {
            // WebP requires unassociated alpha, and it's sRGB.
            // Handle this all by wrapping an IB around it.
            ImageSpec specwrap(m_spec.width, m_spec.height, 4, TypeUInt8);
            ImageBuf bufwrap(specwrap, cspan<uint8_t>(m_uncompressed_image));
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
        if (has_icc_data || m_mux) {
            WebPMemoryWriterClear(&wrt);
        }
        return false;
    }

    // If m_mux is already non-null, we are busy creating an animation
    if (m_mux) {
        WebPMuxFrameInfo frame = {};
        frame.bitstream        = { wrt.mem, wrt.size };

        const float fps = m_spec.get_float_attribute("FramesPerSecond",
                                                     m_default_fps);
        // WebP stores frame durations in milliseconds
        const float duration = fps <= 0.0f ? 0.0f : 1000.0f / fps;
        // libwebp defines MAX_DURATION as 2^24 (see format_constant.h)
        frame.duration = int(OIIO::clamp(duration, 0.0f, float(0xffffff)));



        // For WebPMuxPushFrame, frame.id should be WEBP_CHUNK_ANMF
        frame.id             = WEBP_CHUNK_ANMF;
        frame.dispose_method = WEBP_MUX_DISPOSE_NONE;
        // Replace the previous frame with this one, rather than blending them together.
        frame.blend_method = WEBP_MUX_NO_BLEND;

        bool ok = WebPMuxPushFrame(m_mux, &frame, true) == WEBP_MUX_OK;
        WebPMemoryWriterClear(&wrt);
        if (!ok)
            errorfmt("Failed to add frame to WebP animation {}", m_filename);
        return ok;
    }

    // If there's no ICC data to write, we are done at this point.
    bool ok = true;

    // Otherwise, assemble the final WebP package and write it out.
    if (has_icc_data) {
        WebPMux* mux = WebPMuxNew();

        WebPData image_data = { wrt.mem, wrt.size };
        WebPMuxSetImage(mux, &image_data, false);

        WebPData icc_chunk = { icc_profile.data(), icc_profile.size() };
        WebPMuxSetChunk(mux, "ICCP", &icc_chunk, false);

        WebPData assembly;
        if (WebPMuxAssemble(mux, &assembly) != WEBP_MUX_OK) {
            errorfmt("Failed to assemble {} as WebP image", m_filename);
            WebPMuxDelete(mux);
            WebPMemoryWriterClear(&wrt);
            return false;
        }

        ok = ioproxy()->write(assembly.bytes, assembly.size) == assembly.size;

        WebPDataClear(&assembly);
        WebPMuxDelete(mux);
        WebPMemoryWriterClear(&wrt);
    }

    return ok;
}



bool
WebpOutput::append_subimage(const std::string& name, const ImageSpec& spec)
{
    if (!m_pending_write || name != m_filename) {
        errorfmt("No WebP image open for appending to {}", name);
        return false;
    }
    if (spec.width != m_spec.width || spec.height != m_spec.height
        || spec.nchannels != m_spec.nchannels) {
        errorfmt(
            "WebP animation frames must have the same dimensions and channels");
        return false;
    }

    if (!m_mux) {
        m_mux = WebPMuxNew();
        if (!m_mux) {
            errorfmt("Couldn't initialize WebP animation mux");
            cleanup();
            return false;
        }
        WebPMuxAnimParams params = {};
        params.loop_count        = OIIO::clamp(
            m_spec.get_int_attribute("oiio:LoopCount",
                                     m_spec.get_int_attribute("webp:LoopCount",
                                                              0)),
            0, 65535);

        if (WebPMuxSetCanvasSize(m_mux, m_spec.width, m_spec.height)
                != WEBP_MUX_OK
            || WebPMuxSetAnimationParams(m_mux, &params) != WEBP_MUX_OK) {
            errorfmt("Couldn't initialize WebP animation");
            cleanup();
            return false;
        }

        std::vector<uint8_t> icc_profile = get_colorspace_icc_profile(m_spec);
        if (icc_profile.size() > 0) {
            WebPData chunk = { icc_profile.data(), icc_profile.size() };

            if (WebPMuxSetChunk(m_mux, "ICCP", &chunk, true) != WEBP_MUX_OK) {
                errorfmt("Couldn't add ICC profile to WebP animation");
                cleanup();
                return false;
            }
        }
    }

    // Finish writing the previous frame
    if (!finish_subimage()) {
        cleanup();
        return false;
    }

    // Start Current frame
    if (!check_open(AppendSubimage, spec, { 0, 16383, 0, 16383, 0, 1, 0, 4 },
                    uint64_t(OpenChecks::Disallow1or2Channel))) {
        cleanup();
        return false;
    }

    m_spec.set_format(TypeDesc::UINT8);
    m_dither        = m_spec.get_int_attribute("oiio:dither", 0);
    m_convert_alpha = m_spec.alpha_channel != -1
                      && !m_spec.get_int_attribute("oiio:UnassociatedAlpha", 0);
    m_uncompressed_image.assign(m_spec.image_bytes(), 0);
    m_pending_write = true;
    return true;
}



bool
WebpOutput::finish_subimage()
{
    if (!m_pending_write)
        return true;

    if (!m_uncompressed_image.size())
        return false;

    if (!write_complete_data())
        return false;
    m_pending_write = false;
    return true;
}


bool
WebpOutput::write_scanline(int y, int z, TypeDesc format, const void* data,
                           stride_t xstride)
{
    if (y < 0 || y >= m_spec.height) {
        errorfmt("Attempt to write scanline {} (outside of range [0, {}[) to {}",
                 y, m_spec.height, m_filename);
        cleanup();
        return false;
    }
    std::vector<uint8_t> scratch;
    data = to_native_scanline(format, data, xstride, scratch, m_dither, y, z);
    memcpy(&m_uncompressed_image[y * m_scanline_size], data, m_scanline_size);
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
    }

    // Write the complete image (or last frame) data on close, not during
    // write_scanline. This allows scanlines to be written in any order.
    if (ok)
        ok = finish_subimage();

    if (ok && m_mux) {
        WebPData assembly = {};
        if (WebPMuxAssemble(m_mux, &assembly) != WEBP_MUX_OK) {
            errorfmt("Failed to assemble {} as WebP animation", m_filename);
            ok = false;
        } else {
            ok = ioproxy()->write(assembly.bytes, assembly.size)
                 == assembly.size;
        }
        WebPDataClear(&assembly);
    }

    cleanup();
    return ok;
}


void
WebpOutput::cleanup()
{
    if (m_mux)
        WebPMuxDelete(m_mux);
    m_mux = nullptr;
    std::vector<uint8_t>().swap(m_uncompressed_image);
    if (m_picture_initialized) {
        WebPPictureFree(&m_webp_picture);
    }
    init();
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