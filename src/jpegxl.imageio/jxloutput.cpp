// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <cassert>
#include <cstdio>
#include <vector>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/tiffutils.h>

#include <jxl/encode.h>
#include <jxl/encode_cxx.h>
#include <jxl/resizable_parallel_runner_cxx.h>

OIIO_PLUGIN_NAMESPACE_BEGIN

#define DBG if (0)

class JxlOutput final : public ImageOutput {
public:
    JxlOutput() { init(); }
    ~JxlOutput() override { close(); }
    const char* format_name(void) const override { return "jpegxl"; }
    int supports(string_view feature) const override
    {
        return (feature == "alpha" || feature == "nchannels"
                || feature == "exif" || feature == "ioproxy"
                || feature == "tiles");
    }
    bool open(const std::string& name, const ImageSpec& spec,
              OpenMode mode = Create) override;
    bool write_scanline(int y, int z, TypeDesc format, const void* data,
                        stride_t xstride) override;
    bool write_scanlines(int ybegin, int yend, int z, TypeDesc format,
                         const void* data, stride_t xstride = AutoStride,
                         stride_t ystride = AutoStride) override;
    bool write_tile(int x, int y, int z, TypeDesc format, const void* data,
                    stride_t xstride, stride_t ystride,
                    stride_t zstride) override;
    bool close() override;

private:
    std::string m_filename;
    JxlEncoderPtr m_encoder;
    JxlResizableParallelRunnerPtr m_runner;
    JxlBasicInfo m_basic_info;
    JxlEncoderFrameSettings* m_frame_settings;
    JxlPixelFormat m_pixel_format;

    unsigned int m_dither;
    std::vector<unsigned char> m_scratch;
    std::vector<unsigned char> m_tilebuffer;
    std::vector<float> m_pixels;

    void init(void)
    {
        ioproxy_clear();
        m_encoder = nullptr;
        m_runner  = nullptr;
    }

    bool save_image();
};



OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput*
jpegxl_output_imageio_create()
{
    return new JxlOutput;
}



OIIO_EXPORT const char* jpegxl_output_extensions[] = { "jxl", nullptr };

OIIO_PLUGIN_EXPORTS_END

bool
JxlOutput::open(const std::string& name, const ImageSpec& newspec,
                OpenMode mode)
{
    JxlEncoderStatus status;
    JxlEncoderError error;

    DBG std::cout << "JxlOutput::open(name, newspec, mode)\n";

    // Save name and spec for later use
    m_filename = name;

    if (!check_open(mode, newspec,
                    { 0, 1073741823, 0, 1073741823, 0, 1, 0, 4099 }))
        return false;

    DBG std::cout << "m_filename = " << m_filename << "\n";

    ioproxy_retrieve_from_config(m_spec);
    if (!ioproxy_use_or_open(name)) {
        DBG std::cout << "ioproxy_use_or_open returned false\n";
        return false;
    }

    m_spec.set_format(TypeFloat);

    m_dither = (m_spec.format == TypeDesc::UINT8)
                   ? m_spec.get_int_attribute("oiio:dither", 0)
                   : 0;

    m_encoder = JxlEncoderMake(nullptr);
    if (m_encoder == nullptr) {
        DBG std::cout << "JxlEncoderMake failed\n";
        return false;
    }

    JxlEncoderAllowExpertOptions(m_encoder.get());

    const uint32_t threads
        = JxlResizableParallelRunnerSuggestThreads(m_spec.width, m_spec.height);

    m_runner = JxlResizableParallelRunnerMake(nullptr);
    if (m_runner == nullptr) {
        DBG std::cout << "JxlThreadParallelRunnerMake failed\n";
        return false;
    }

    JxlResizableParallelRunnerSetThreads(m_runner.get(), threads);
    status = JxlEncoderSetParallelRunner(m_encoder.get(),
                                         JxlResizableParallelRunner,
                                         m_runner.get());

    if (status != JXL_ENC_SUCCESS) {
        error = JxlEncoderGetError(m_encoder.get());
        errorfmt("JxlEncoderSetParallelRunner failed with error {}",
                 (int)error);
        return false;
    }

    JxlEncoderInitBasicInfo(&m_basic_info);

    DBG std::cout << "m_spec " << m_spec.width << "×" << m_spec.height << "×"
                  << m_spec.nchannels << "\n";
    m_basic_info.xsize           = m_spec.width;
    m_basic_info.ysize           = m_spec.height;
    m_basic_info.bits_per_sample = 32;
    // m_basic_info.exponent_bits_per_sample = 0;
    m_basic_info.exponent_bits_per_sample = 8;

    if (m_spec.nchannels >= 4) {
        m_basic_info.num_color_channels = 3;
        m_basic_info.num_extra_channels = m_spec.nchannels - 3;
        m_basic_info.alpha_bits         = m_basic_info.bits_per_sample;
        m_basic_info.alpha_exponent_bits = m_basic_info.exponent_bits_per_sample;
    } else {
        m_basic_info.num_color_channels = m_spec.nchannels;
    }

    DBG std::cout << "m_basic_info " << m_basic_info.xsize << "×"
                  << m_basic_info.ysize << "×"
                  << m_basic_info.num_color_channels << "\n";

    m_frame_settings = JxlEncoderFrameSettingsCreate(m_encoder.get(), nullptr);

    // const float quality = 100.0;
    const int effort = 7;
    const int tier   = 0;
    // Lossless only makes sense for integer modes
    if (m_basic_info.exponent_bits_per_sample == 0) {
        // Must preserve original profile for lossless mode
        m_basic_info.uses_original_profile = JXL_TRUE;
        JxlEncoderSetFrameDistance(m_frame_settings, 0.0);
        JxlEncoderSetFrameLossless(m_frame_settings, JXL_TRUE);
    }

    JxlEncoderFrameSettingsSetOption(m_frame_settings,
                                     JXL_ENC_FRAME_SETTING_EFFORT, effort);

    JxlEncoderFrameSettingsSetOption(m_frame_settings,
                                     JXL_ENC_FRAME_SETTING_DECODING_SPEED,
                                     tier);

    // Codestream level should be chosen automatically given the settings
    JxlEncoderSetBasicInfo(m_encoder.get(), &m_basic_info);

    if (m_basic_info.num_extra_channels > 0) {
        for (int i = 0; i < m_basic_info.num_extra_channels; i++) {
            JxlExtraChannelType type = JXL_CHANNEL_ALPHA;
            JxlExtraChannelInfo extra_channel_info;

            JxlEncoderInitExtraChannelInfo(type, &extra_channel_info);

            extra_channel_info.bits_per_sample = m_basic_info.alpha_bits;
            extra_channel_info.exponent_bits_per_sample
                = m_basic_info.alpha_exponent_bits;
            // extra_channel_info.alpha_premultiplied = premultiply;

            status = JxlEncoderSetExtraChannelInfo(m_encoder.get(), i,
                                                   &extra_channel_info);
            if (status != JXL_ENC_SUCCESS) {
                error = JxlEncoderGetError(m_encoder.get());
                errorfmt("JxlEncoderSetExtraChannelInfo failed with error {}",
                         (int)error);
                return false;
            }
        }
    }

    if (m_spec.tile_width && m_spec.tile_height) {
        m_tilebuffer.resize(m_spec.image_bytes());
    }

    return true;
}



bool
JxlOutput::write_scanline(int y, int z, TypeDesc format, const void* data,
                          stride_t xstride)
{
    DBG std::cout << "JxlOutput::write_scanline(y = " << y << " )\n";

    return write_scanlines(y, y + 1, z, format, data, xstride, AutoStride);
}



bool
JxlOutput::write_scanlines(int ybegin, int yend, int z, TypeDesc format,
                           const void* data, stride_t xstride, stride_t ystride)
{
    DBG std::cout << "JxlOutput::write_scanlines(ybegin = " << ybegin
                  << ", yend = " << yend << ", ...)\n";

    stride_t zstride = AutoStride;
    m_spec.auto_stride(xstride, ystride, zstride, format, m_spec.nchannels,
                       m_spec.width, m_spec.height);
    size_t npixels = size_t(m_spec.width) * size_t(yend - ybegin);
    size_t nvals   = npixels * size_t(m_spec.nchannels);

    data = to_native_rectangle(m_spec.x, m_spec.x + m_spec.width, ybegin, yend,
                               z, z + 1, format, data, xstride, ystride,
                               zstride, m_scratch, m_dither, 0, ybegin, z);

    DBG std::cout << "data = " << data << " nvals = " << nvals << "\n";

    std::vector<float>::iterator m_it
        = m_pixels.begin() + m_spec.width * ybegin * m_spec.nchannels;

    m_pixels.insert(m_it, (float*)data, (float*)data + nvals);

    return true;
}



bool
JxlOutput::write_tile(int x, int y, int z, TypeDesc format, const void* data,
                      stride_t xstride, stride_t ystride, stride_t zstride)
{
    DBG std::cout << "JxlOutput::write_tile()\n";

    // Emulate tiles by buffering the whole image
    return copy_tile_to_image_buffer(x, y, z, format, data, xstride, ystride,
                                     zstride, &m_tilebuffer[0]);
}



bool
JxlOutput::save_image()
{
    JxlEncoderStatus status;
    JxlEncoderError error;
    std::vector<uint8_t> compressed;
    bool ok = true;

    DBG std::cout << "JxlOutput::save_image()\n";

    m_pixel_format = { m_basic_info.num_color_channels
                           + m_basic_info.num_extra_channels,
                       JXL_TYPE_FLOAT, JXL_NATIVE_ENDIAN, 0 };

    const size_t pixels_size = m_basic_info.xsize * m_basic_info.ysize
                               * (m_basic_info.num_color_channels
                                  + m_basic_info.num_extra_channels);

    m_pixels.resize(pixels_size);

    const void* data = m_pixels.data();
    size_t size      = m_pixels.size() * sizeof(float);

    DBG std::cout << "data = " << data << " size = " << size << "\n";

    status = JxlEncoderAddImageFrame(m_frame_settings, &m_pixel_format, data,
                                     size);
    DBG std::cout << "status = " << status << "\n";
    if (status != JXL_ENC_SUCCESS) {
        error = JxlEncoderGetError(m_encoder.get());
        errorfmt("JxlEncoderAddImageFrame failed with error {}", (int)error);
        return false;
    }

    // No more image frames nor metadata boxes to add
    DBG std::cout << "calling JxlEncoderCloseInput()\n";
    JxlEncoderCloseInput(m_encoder.get());

    compressed.clear();
    compressed.resize(4096);
    uint8_t* next_out = compressed.data();
    size_t avail_out  = compressed.size() - (next_out - compressed.data());
    JxlEncoderStatus result = JXL_ENC_NEED_MORE_OUTPUT;
    while (result == JXL_ENC_NEED_MORE_OUTPUT) {
        DBG std::cout << "calling JxlEncoderProcessOutput()\n";
        result = JxlEncoderProcessOutput(m_encoder.get(), &next_out,
                                         &avail_out);
        DBG std::cout << "result = " << result << "\n";
        if (result == JXL_ENC_NEED_MORE_OUTPUT) {
            size_t offset = next_out - compressed.data();
            compressed.resize(compressed.size() * 2);
            next_out  = compressed.data() + offset;
            avail_out = compressed.size() - offset;
        }
    }
    compressed.resize(next_out - compressed.data());
    if (result != JXL_ENC_SUCCESS) {
        DBG std::cout << "JxlEncoderProcessOutput failed.\n";
        return false;
    }

    DBG std::cout << "compressed.size() = " << compressed.size() << "\n";

    if (!iowrite(compressed.data(), 1, compressed.size())) {
        DBG std::cout << "iowrite failed.\n";
        return false;
    }

    DBG std::cout << "JxlOutput::save_image() return ok\n";
    return ok;
}



bool
JxlOutput::close()
{
    bool ok = true;

    DBG std::cout << "JxlOutput::close()\n";

    if (!ioproxy_opened()) {  // Already closed
        init();
        return true;
    }

    if (m_spec.tile_width) {
        // Handle tile emulation -- output the buffered pixels
        OIIO_ASSERT(m_tilebuffer.size());
        ok &= write_scanlines(m_spec.y, m_spec.y + m_spec.height, 0,
                              m_spec.format, &m_tilebuffer[0]);
        std::vector<unsigned char>().swap(m_tilebuffer);
    }

    save_image();

    init();
    return ok;
}

OIIO_PLUGIN_NAMESPACE_END
