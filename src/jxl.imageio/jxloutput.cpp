// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause and Apache-2.0
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

#define DBG if (1)

class JxlOutput final : public ImageOutput {
public:
    JxlOutput()
    {
        fprintf(stderr, "JxlOutput()\n");
        init();
    }
    ~JxlOutput() override { close(); }
    const char* format_name(void) const override { return "jxl"; }
    int supports(string_view feature) const override
    {
        return (feature == "alpha" || feature == "nchannels" || feature == "exif" || feature == "ioproxy" || feature == "tiles");
    }
    bool open(const std::string& name, const ImageSpec& spec,
              OpenMode mode = Create) override;
    bool write_scanline(int y, int z, TypeDesc format, const void* data,
                        stride_t xstride) override;
    bool write_scanlines(int ybegin, int yend, int z, TypeDesc format,
                         const void* data, stride_t xstride,
                         stride_t ystride) override;
    bool write_tile(int x, int y, int z, TypeDesc format, const void* data,
                    stride_t xstride, stride_t ystride,
                    stride_t zstride) override;
    bool close() override;
    bool copy_image(ImageInput* in) override;

private:
    std::string m_filename;
    JxlEncoderPtr m_encoder;
    JxlResizableParallelRunnerPtr m_runner;
    JxlBasicInfo m_basic_info;
    JxlEncoderFrameSettings *m_frame_settings;
    JxlPixelFormat m_pixel_format;

    int m_next_scanline;  // Which scanline is the next to write?
    unsigned int m_dither;
    std::vector<unsigned char> m_scratch;
    std::vector<unsigned char> m_tilebuffer;

    void init(void)
    {
        ioproxy_clear();
        m_encoder = nullptr;
        m_runner = nullptr;
    }

    bool save_image();
};

OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput*
jxl_output_imageio_create()
{
    return new JxlOutput;
}

OIIO_EXPORT const char* jxl_output_extensions[] = { "jxl", nullptr };

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
                    { 0, 1073741823, 0, 1073741823, 0, 1, 0,
                      4099 }))
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

    const uint32_t threads = JxlResizableParallelRunnerSuggestThreads(m_spec.width, m_spec.height);

    m_runner = JxlResizableParallelRunnerMake(nullptr);
    if (m_runner == nullptr) {
        DBG std::cout << "JxlThreadParallelRunnerMake failed\n";
        return false;
    }

    JxlResizableParallelRunnerSetThreads(m_runner.get(), threads);
    status = JxlEncoderSetParallelRunner(m_encoder.get(), JxlResizableParallelRunner, m_runner.get());

    if (status != JXL_ENC_SUCCESS) {
        error = JxlEncoderGetError(m_encoder.get());
        DBG std::cout << "JxlEncoderSetParallelRunner failed with error " << error << "\n";
        return false;
    }

    JxlEncoderInitBasicInfo(&m_basic_info);
    m_basic_info.xsize = m_spec.width;
    m_basic_info.ysize = m_spec.height;
    m_basic_info.num_color_channels = m_spec.nchannels;
    m_basic_info.bits_per_sample = 32;
    // m_basic_info.exponent_bits_per_sample = 0;
    m_basic_info.exponent_bits_per_sample = 8;

    DBG std::cout << "m_basic_info " << m_basic_info.xsize << "×" << m_basic_info.ysize << "×" << m_basic_info.num_color_channels << "\n";

    m_frame_settings = JxlEncoderFrameSettingsCreate(m_encoder.get(), nullptr);

    // const float quality = 100.0;
    const int effort = 7;
    const int tier = 0;
    // Lossless only makes sense for integer modes
    if (m_basic_info.exponent_bits_per_sample == 0) {
        // Must preserve original profile for lossless mode
        m_basic_info.uses_original_profile = JXL_TRUE;
        JxlEncoderSetFrameDistance(m_frame_settings, 0.0);
        JxlEncoderSetFrameLossless(m_frame_settings, JXL_TRUE);
    }

    if (m_spec.tile_width && m_spec.tile_height) {
        m_tilebuffer.resize(m_spec.image_bytes());
    }

    JxlEncoderFrameSettingsSetOption(m_frame_settings, JXL_ENC_FRAME_SETTING_EFFORT, effort);

    JxlEncoderFrameSettingsSetOption(m_frame_settings, JXL_ENC_FRAME_SETTING_DECODING_SPEED, tier);

    // Codestream level should be chosen automatically given the settings
    JxlEncoderSetBasicInfo(m_encoder.get(), &m_basic_info);

    m_next_scanline = 0;

    return true;
}

bool
JxlOutput::write_scanline(int y, int z, TypeDesc format, const void* data,
                          stride_t xstride)
{
    DBG std::cout << "JxlOutput::write_scanline(y = " << y << " )\n";

    return write_scanlines(y, y + 1, z, format, data, xstride, AutoStride);
#if 0
    if (y != m_next_scanline) {
        DBG std::cout << "Attempt to write scanlines out of order\n";
        return false;
    }
    if (y > m_spec.height) {
        DBG std::cout << "Attempt to write too many scanlines\n";
        return false;
    }

    m_spec.auto_stride(xstride, format, spec().nchannels);
    const void* origdata = data;
    data = to_native_scanline(format, data, xstride, m_scratch, m_dither, y, z);
    DBG std::cout << "origdata = " << origdata << " data = " << data << "\n";
    if (data == origdata) {
        m_scratch.assign((unsigned char*)data,
                         (unsigned char*)data + m_spec.scanline_bytes());
        // data = &m_scratch[0];
    }

    m_next_scanline++;

    if (y == m_spec.height - 1) {
        save_image();
    }

    return true;
#endif
}

bool
JxlOutput::write_scanlines(int ybegin, int yend, int z, TypeDesc format,
                           const void* data, stride_t xstride,
                           stride_t ystride)
{
    DBG std::cout << "JxlOutput::write_scanlines(ybegin = " << ybegin << ", yend = " << yend <<", ...)\n";

    if (yend == m_spec.height) {
        save_image();
    }
#if 0
    yend                      = std::min(yend, spec().y + spec().height);
    bool native               = (format == TypeDesc::UNKNOWN);
    imagesize_t scanlinebytes = spec().scanline_bytes(true);
    size_t pixel_bytes        = m_spec.pixel_bytes(true);
    if (native && xstride == AutoStride)
        xstride = (stride_t)pixel_bytes;
    stride_t zstride = AutoStride;
    m_spec.auto_stride(xstride, ystride, zstride, format, m_spec.nchannels,
                       m_spec.width, m_spec.height);

    const imagesize_t limit = 16 * 1024
                              * 1024;  // Allocate 16 MB, or 1 scanline
    int chunk = std::max(1, int(limit / scanlinebytes));

    bool ok = true;
    for (; ok && ybegin < yend; ybegin += chunk) {
        int y1         = std::min(ybegin + chunk, yend);
        int nscanlines = y1 - ybegin;
        const void* d  = to_native_rectangle(m_spec.x, m_spec.x + m_spec.width,
                                             ybegin, y1, z, z + 1, format, data,
                                             xstride, ystride, zstride,
                                             m_scratch);

        // Compute where OpenEXR needs to think the full buffers starts.
        // OpenImageIO requires that 'data' points to where client stored
        // the bytes to be written, but OpenEXR's frameBuffer.insert() wants
        // where the address of the "virtual framebuffer" for the whole
        // image.
        char* buf = (char*)d - m_spec.x * pixel_bytes - ybegin * scanlinebytes;
        try {
            Imf::FrameBuffer frameBuffer;
            size_t chanoffset = 0;
            for (int c = 0; c < m_spec.nchannels; ++c) {
                size_t chanbytes = m_spec.channelformat(c).size();
                frameBuffer.insert(m_spec.channelnames[c].c_str(),
                                   Imf::Slice(m_pixeltype[c], buf + chanoffset,
                                              pixel_bytes, scanlinebytes));
                chanoffset += chanbytes;
            }
            if (m_output_scanline) {
                m_output_scanline->setFrameBuffer(frameBuffer);
                m_output_scanline->writePixels(nscanlines);
            } else if (m_scanline_output_part) {
                m_scanline_output_part->setFrameBuffer(frameBuffer);
                m_scanline_output_part->writePixels(nscanlines);
            } else {
                errorf("Attempt to write scanlines to a non-scanline file.");
                return false;
            }
        } catch (const std::exception& e) {
            errorf("Failed OpenEXR write: %s", e.what());
            return false;
        } catch (...) {  // catch-all for edge cases or compiler bugs
            errorf("Failed OpenEXR write: unknown exception");
            return false;
        }

        data = (const char*)data + ystride * nscanlines;
    }
#endif
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
    std::vector<uint8_t> compressed;
    bool ok = true;

    DBG std::cout << "JxlOutput::save_image()\n";

    m_pixel_format = { m_basic_info.num_color_channels, JXL_TYPE_FLOAT, JXL_NATIVE_ENDIAN, 0 };

    const size_t pixels_size = m_basic_info.xsize * m_basic_info.ysize * m_basic_info.num_color_channels * ((m_basic_info.bits_per_sample + 7) >> 3);

    m_scratch.resize(pixels_size);

    const void* data = m_scratch.data();
    size_t size = m_scratch.size();

    DBG std::cout << "data = " << data << " size = " << size << "\n";

    // status = JxlEncoderAddImageFrame(m_frame_settings, &m_pixel_format, data, pixels_size);
    status = JxlEncoderAddImageFrame(m_frame_settings, &m_pixel_format, data, size);
    DBG std::cout << "status = " << status << "\n";
    if (status != JXL_ENC_SUCCESS) {
        DBG std::cout << "JxlEncoderAddImageFrame failed.\n";
        return false;
    }

    // No more image frames nor metadata boxes to add
    DBG std::cout << "calling JxlEncoderCloseInput()\n";
    JxlEncoderCloseInput(m_encoder.get());

    compressed.clear();
    compressed.resize(4096);
    uint8_t* next_out = compressed.data();
    size_t avail_out = compressed.size() - (next_out - compressed.data());
    JxlEncoderStatus result = JXL_ENC_NEED_MORE_OUTPUT;
    while (result == JXL_ENC_NEED_MORE_OUTPUT) {
        DBG std::cout << "calling JxlEncoderProcessOutput()\n";
        result = JxlEncoderProcessOutput(m_encoder.get(), &next_out, &avail_out);
        DBG std::cout << "result = " << result << "\n";
        if (result == JXL_ENC_NEED_MORE_OUTPUT) {
            size_t offset = next_out - compressed.data();
            compressed.resize(compressed.size() * 2);
            next_out = compressed.data() + offset;
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
#if 0
    if (m_spec.tile_width) {
        // Handle tile emulation -- output the buffered pixels
        OIIO_ASSERT(m_tilebuffer.size());
        ok &= write_scanlines(m_spec.y, m_spec.y + m_spec.height, 0,
                              m_spec.format, &m_tilebuffer[0]);
        m_tilebuffer.clear();
        m_tilebuffer.shrink_to_fit();
    }
#endif
    init();
    return ok;
}

bool
JxlOutput::copy_image(ImageInput* in)
{
    if (in && !strcmp(in->format_name(), "jxl")) {
        return true;
    }

    return ImageOutput::copy_image(in);
}

OIIO_PLUGIN_NAMESPACE_END
