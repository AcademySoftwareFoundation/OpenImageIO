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

#include <jxl/decode.h>
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
    bool write_tiles(int xbegin, int xend, int ybegin, int yend, int zbegin,
                     int zend, TypeDesc format, const void* data,
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
    std::vector<unsigned char> m_scanbuffer;  // hack

    void init(void)
    {
        ioproxy_clear();
        m_encoder = nullptr;
        m_runner  = nullptr;
    }

    bool save_image(const void* data);
    bool save_metadata(ImageSpec& m_spec, JxlEncoderPtr& m_encoder);
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

    switch (m_spec.format.basetype) {
    case TypeDesc::UINT8:
    case TypeDesc::UINT16: m_spec.set_format(m_spec.format); break;
    case TypeDesc::UINT32: m_spec.set_format(TypeDesc::UINT16); break;
    case TypeDesc::HALF:
    case TypeDesc::FLOAT: m_spec.set_format(m_spec.format); break;
    case TypeDesc::DOUBLE: m_spec.set_format(TypeDesc::FLOAT); break;
    default: errorfmt("Unsupported data type {}", m_spec.format); return false;
    }

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
    m_basic_info.xsize = m_spec.width;
    m_basic_info.ysize = m_spec.height;

    switch (m_spec.format.basetype) {
    case TypeDesc::UINT8:
        m_basic_info.bits_per_sample          = 8;
        m_basic_info.exponent_bits_per_sample = 0;
        break;
    case TypeDesc::UINT16:
    case TypeDesc::UINT32:
        m_basic_info.bits_per_sample          = 16;
        m_basic_info.exponent_bits_per_sample = 0;
        break;
    case TypeDesc::HALF:
        m_basic_info.bits_per_sample          = 16;
        m_basic_info.exponent_bits_per_sample = 5;
        break;
    case TypeDesc::FLOAT:
    case TypeDesc::DOUBLE:
        m_basic_info.bits_per_sample          = 32;
        m_basic_info.exponent_bits_per_sample = 8;
        break;
    default: errorfmt("Unsupported data type {}", m_spec.format); return false;
    }

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

    // JpegXL Compression Settings

    bool lossless = true;

    // Distance Mutually exclusive with quality
    if (m_spec.find_attribute("jpegxl:distance")) {
        const float distance = m_spec.get_float_attribute("jpegxl:distance",
                                                          0.0f);

        m_basic_info.uses_original_profile = distance == 0.0f ? JXL_TRUE
                                                              : JXL_FALSE;
        JxlEncoderSetFrameDistance(m_frame_settings, distance);
        JxlEncoderSetFrameLossless(m_frame_settings,
                                   distance == 0.0f ? JXL_TRUE : JXL_FALSE);

        lossless = distance == 0.0f;

        DBG std::cout << "Compression distance set to " << distance << "\n";

    } else {
        auto compqual = m_spec.decode_compression_metadata("jpegxl", 100);
        if (Strutil::iequals(compqual.first, "jpegxl")) {
            if (compqual.second == 100) {
                m_basic_info.uses_original_profile = JXL_TRUE;
                JxlEncoderSetFrameDistance(m_frame_settings, 0.0);
                JxlEncoderSetFrameLossless(m_frame_settings, JXL_TRUE);

                lossless = true;
            } else {
                m_basic_info.uses_original_profile = JXL_FALSE;
                JxlEncoderSetFrameDistance(
                    m_frame_settings,
                    1.0f / static_cast<float>(compqual.second));
                JxlEncoderSetFrameLossless(m_frame_settings, JXL_FALSE);
            }
        } else {  // default to lossless
            m_basic_info.uses_original_profile = JXL_TRUE;
            JxlEncoderSetFrameDistance(m_frame_settings, 0.0);
            JxlEncoderSetFrameLossless(m_frame_settings, JXL_TRUE);

            lossless = true;
        }

        DBG std::cout << "compression set to " << compqual.second << "\n";
    }

    const int effort = m_spec.get_int_attribute("jpegxl:effort", 7);
    const int speed  = m_spec.get_int_attribute("jpegxl:speed", 0);
    JxlEncoderFrameSettingsSetOption(m_frame_settings,
                                     JXL_ENC_FRAME_SETTING_EFFORT, effort);

    JxlEncoderFrameSettingsSetOption(m_frame_settings,
                                     JXL_ENC_FRAME_SETTING_DECODING_SPEED,
                                     speed);

    // Preprocessing (maybe not works yet)
    if (m_spec.find_attribute("jpegxl:photon_noise_iso") && !lossless) {
        JxlEncoderFrameSettingsSetFloatOption(
            m_frame_settings, JXL_ENC_FRAME_SETTING_PHOTON_NOISE,
            m_spec.get_float_attribute("jpegxl:photon_noise_iso", 0.0f));

        DBG std::cout << "Photon noise set to "
                      << m_spec.get_float_attribute("jpegxl:photon_noise_iso",
                                                    0.0f)
                      << "\n";
    }

    // Codestream level should be chosen automatically given the settings
    JxlEncoderSetBasicInfo(m_encoder.get(), &m_basic_info);

    if (m_basic_info.num_extra_channels > 0) {
        for (uint32_t i = 0; i < m_basic_info.num_extra_channels; i++) {
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

    // add data to m_scanbuffer
    m_scanbuffer.insert(m_scanbuffer.end(), (unsigned char*)data,
                        (unsigned char*)data + nvals * m_spec.format.size());

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
JxlOutput::write_tiles(int xbegin, int xend, int ybegin, int yend, int zbegin,
                       int zend, TypeDesc format, const void* data,
                       stride_t xstride, stride_t ystride, stride_t zstride)
{
    DBG std::cout << "JxlOutput::write_tiles()\n";

    // Call the parent class default implementation of write_tiles, which
    // will loop over the tiles and write each one individually.
    return ImageOutput::write_tiles(xbegin, xend, ybegin, yend, zbegin, zend,
                                    format, data, xstride, ystride, zstride);
};



bool
JxlOutput::save_metadata(ImageSpec& m_spec, JxlEncoderPtr& encoder)
{
    DBG std::cout << "JxlOutput::save_metadata()\n";

    // Write EXIF info
    std::vector<char> exif = { 0, 0, 0, 0 };
    encode_exif(m_spec, exif);

    // Write XMP packet, if we have anything
    std::string xmp = encode_xmp(m_spec, true);

    // Write IPTC IIM metadata tags, if we have anything
    std::vector<char> iptc;
    encode_iptc_iim(m_spec, iptc);

    bool use_boxes     = m_spec.get_int_attribute("jpegxl:use_boxes", 1) == 1;
    int compress_boxes = m_spec.get_int_attribute("jpegxl:compress_boxes", 1);

    if (use_boxes) {
        if (JXL_ENC_SUCCESS != JxlEncoderUseBoxes(m_encoder.get())) {
            JxlEncoderError error = JxlEncoderGetError(m_encoder.get());
            errorfmt("JxlEncoderUseBoxes() failed {}.", (int)error);
            return false;
        }
        DBG std::cerr << "JxlEncoderUseBoxes() ok\n";

        // Exif
        std::vector<uint8_t>* exif_data = nullptr;
        if (!exif.empty())
            exif_data = reinterpret_cast<std::vector<uint8_t>*>(&exif);

        // XMP
        std::vector<uint8_t> xmp_data;

        if (!xmp.empty()) {
            xmp_data.insert(xmp_data.end(), xmp.c_str(),
                            xmp.c_str() + xmp.length());
        }

        // IPTC
        std::vector<uint8_t>* iptc_data = nullptr;
        if (!iptc.empty()) {
            static char photoshop[] = "Photoshop 3.0";
            std::vector<char> head(photoshop,
                                   photoshop + strlen(photoshop) + 1);
            static char _8BIM[] = "8BIM";
            head.insert(head.end(), _8BIM, _8BIM + 4);
            head.push_back(4);  // 0x0404
            head.push_back(4);
            head.push_back(0);  // four bytes of zeroes
            head.push_back(0);
            head.push_back(0);
            head.push_back(0);
            head.push_back((char)(iptc.size() >> 8));  // size of block
            head.push_back((char)(iptc.size() & 0xff));
            iptc.insert(iptc.begin(), head.begin(), head.end());

            iptc_data = reinterpret_cast<std::vector<uint8_t>*>(&iptc);
        }

        // Jumbf
        std::vector<uint8_t>* jumbf_data = nullptr;

        struct BoxInfo {
            const char* type;
            const std::vector<uint8_t>* bytes;
            const bool enable;
        };

        const BoxInfo boxes[]
            = { { "Exif", exif_data,
                  m_spec.get_int_attribute("jpegxl:exif_box", 1) == 1 },
                { "xml ", &xmp_data,
                  m_spec.get_int_attribute("jpegxl:xmp_box", 1) == 1 },
                { "jumb", jumbf_data,
                  m_spec.get_int_attribute("jpegxl:jumb_box", 0) == 1 },
                { "xml ", iptc_data,
                  m_spec.get_int_attribute("jpegxl:iptc_box", 0) == 1 } };

        for (const auto& box : boxes) {
            // check if box_data is not nullptr
            if (box.enable) {
                if (!box.bytes) {
                    DBG std::cerr << "Box data is nullptr.\n";
                    continue;
                }
                if (!box.bytes->empty()) {
                    if (JXL_ENC_SUCCESS
                        != JxlEncoderAddBox(m_encoder.get(), box.type,
                                            box.bytes->data(),
                                            box.bytes->size(),
                                            compress_boxes)) {
                        errorfmt("JxlEncoderAddBox() failed {}.", box.type);
                        return false;
                    }
                }
            }
        }
        JxlEncoderCloseBoxes(m_encoder.get());
    }

    return true;
}



bool
JxlOutput::save_image(const void* data)
{
    JxlEncoderStatus status;
    JxlEncoderError error;
    std::vector<uint8_t> compressed;
    bool ok = true;

    JxlDataType jxl_type = JXL_TYPE_FLOAT;
    size_t jxl_bytes     = 1;

    DBG std::cout << "JxlOutput::save_image()\n";

    switch (m_spec.format.basetype) {
    case TypeDesc::UINT8:
        jxl_type  = JXL_TYPE_UINT8;
        jxl_bytes = 1;
        break;
    case TypeDesc::UINT16:
    case TypeDesc::UINT32:
        jxl_type  = JXL_TYPE_UINT16;
        jxl_bytes = 2;
        break;
    case TypeDesc::HALF:
        jxl_type  = JXL_TYPE_FLOAT16;
        jxl_bytes = 2;
        break;
    case TypeDesc::FLOAT:
    case TypeDesc::DOUBLE:
        jxl_type  = JXL_TYPE_FLOAT;
        jxl_bytes = 4;
        break;
    default: errorfmt("Unsupported data type {}", m_spec.format); return false;
    }

    m_pixel_format = { m_basic_info.num_color_channels
                           + m_basic_info.num_extra_channels,
                       jxl_type, JXL_NATIVE_ENDIAN, 0 };

    const size_t pixels_size = m_basic_info.xsize * m_basic_info.ysize
                               * (m_basic_info.num_color_channels
                                  + m_basic_info.num_extra_channels);

    size_t size = pixels_size * jxl_bytes;

    DBG std::cout << "data = " << data << " size = " << size << "\n";

    // Write EXIF info
    bool metadata_success = save_metadata(m_spec, m_encoder);
    if (!metadata_success) {
        error = JxlEncoderGetError(m_encoder.get());
        errorfmt("save_metadata failed with error {}", (int)error);
        return false;
    }

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

    //save_image();
    save_image(m_scanbuffer.data());

    init();
    return ok;
}

OIIO_PLUGIN_NAMESPACE_END
