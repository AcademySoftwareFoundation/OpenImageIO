// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// JPEG XL

// https://jpeg.org/jpegxl/index.html
// https://jpegxl.info
// https://jpegxl.info/test-page
// https://people.csail.mit.edu/ericchan/hdr/hdr-jxl.php
// https://saklistudio.com/jxltests
// https://thorium.rocks
// https://bugs.chromium.org/p/chromium/issues/detail?id=1451807

#include <algorithm>
#include <cassert>
#include <cstdio>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/tiffutils.h>

#include <jxl/decode.h>
#include <jxl/decode_cxx.h>
#include <jxl/resizable_parallel_runner_cxx.h>

OIIO_PLUGIN_NAMESPACE_BEGIN

#define DBG if (0)

class JxlInput final : public ImageInput {
public:
    JxlInput() { init(); }
    ~JxlInput() override { close(); }
    const char* format_name(void) const override { return "jpegxl"; }
    int supports(string_view feature) const override
    {
        return (feature == "exif" || feature == "ioproxy");
    }
    bool valid_file(Filesystem::IOProxy* ioproxy) const override;

    bool open(const std::string& name, ImageSpec& spec) override;
    bool open(const std::string& name, ImageSpec& spec,
              const ImageSpec& config) override;
    bool read_native_scanline(int subimage, int miplevel, int y, int z,
                              void* data) override;
    bool close() override;

    const std::string& filename() const { return m_filename; }

private:
    std::string m_filename;
    int m_next_scanline;  // Which scanline is the next to read?
    uint32_t m_channels;
    JxlDecoderPtr m_decoder;
    JxlResizableParallelRunnerPtr m_runner;
    std::unique_ptr<ImageSpec> m_config;  // Saved copy of configuration spec
    std::vector<uint8_t> m_icc_profile;
    std::unique_ptr<uint8_t[]> m_buffer;

    void init()
    {
        ioproxy_clear();
        m_config.reset();
        m_decoder = nullptr;
        m_runner  = nullptr;
        m_buffer  = nullptr;
    }

    void close_file() { init(); }
};



// Export version number and create function symbols
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT int jpegxl_imageio_version = OIIO_PLUGIN_VERSION;



OIIO_EXPORT const char*
jpegxl_imageio_library_version()
{
    return "libjxl " OIIO_STRINGIZE(JPEGXL_MAJOR_VERSION) "." OIIO_STRINGIZE(
        JPEGXL_MINOR_VERSION) "." OIIO_STRINGIZE(JPEGXL_PATCH_VERSION);
}



OIIO_EXPORT ImageInput*
jpegxl_input_imageio_create()
{
    return new JxlInput;
}

OIIO_EXPORT const char* jpegxl_input_extensions[] = { "jxl", nullptr };

OIIO_PLUGIN_EXPORTS_END



bool
JxlInput::valid_file(Filesystem::IOProxy* ioproxy) const
{
    DBG std::cout << "JxlInput::valid_file()\n";

    // Check magic number to assure this is a JPEG file
    if (!ioproxy || ioproxy->mode() != Filesystem::IOProxy::Read)
        return false;

    uint8_t magic[128] {};
    const size_t numRead = ioproxy->pread(magic, sizeof(magic), 0);
    if (numRead != sizeof(magic))
        return false;

    JxlSignature signature = JxlSignatureCheck(magic, sizeof(magic));
    switch (signature) {
    case JXL_SIG_CODESTREAM:
    case JXL_SIG_CONTAINER: break;
    default: return false;
    }

    DBG std::cout << "JxlInput::valid_file() return true\n";
    return true;
}



bool
JxlInput::open(const std::string& name, ImageSpec& newspec,
               const ImageSpec& config)
{
    DBG std::cout << "JxlInput::open(name, newspec, config)\n";

    ioproxy_retrieve_from_config(config);
    m_config.reset(new ImageSpec(config));  // save config spec
    return open(name, newspec);
}



bool
JxlInput::open(const std::string& name, ImageSpec& newspec)
{
    DBG std::cout << "JxlInput::open(name, newspec)\n";

    m_filename = name;

    DBG std::cout << "m_filename = " << m_filename << "\n";

    if (!ioproxy_use_or_open(name)) {
        DBG std::cout << "ioproxy_use_or_open returned false\n";
        return false;
    }

    Filesystem::IOProxy* m_io = ioproxy();
    std::string proxytype     = m_io->proxytype();
    if (proxytype != "file" && proxytype != "memreader") {
        errorfmt("JPEG XL reader can't handle proxy type {}", proxytype);
        return false;
    }

    m_decoder = JxlDecoderMake(nullptr);
    if (m_decoder == nullptr) {
        DBG std::cout << "JxlDecoderMake failed\n";
        return false;
    }

    m_runner = JxlResizableParallelRunnerMake(nullptr);
    if (m_runner == nullptr) {
        DBG std::cout << "JxlThreadParallelRunnerMake failed\n";
        return false;
    }

    JxlDecoderStatus status = JxlDecoderSetParallelRunner(
        m_decoder.get(), JxlResizableParallelRunner, m_runner.get());
    if (status != JXL_DEC_SUCCESS) {
        DBG std::cout << "JxlDecoderSetParallelRunner failed\n";
        return false;
    }

    status
        = JxlDecoderSubscribeEvents(m_decoder.get(),
                                    JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING
                                        | JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE);
    if (status != JXL_DEC_SUCCESS) {
        DBG std::cout << "JxlDecoderSubscribeEvents failed\n";
        return false;
    }

    std::unique_ptr<uint8_t[]> jxl;

    DBG std::cout << "proxytype = " << proxytype << "\n";
    if (proxytype == "file") {
        size_t size = m_io->size();
        DBG std::cout << "size = " << size << "\n";
        jxl.reset(new uint8_t[size]);
        size_t result = m_io->read(jxl.get(), size);
        DBG std::cout << "result = " << result << "\n";

        status = JxlDecoderSetInput(m_decoder.get(), jxl.get(), size);
        if (status != JXL_DEC_SUCCESS) {
            DBG std::cout << "JxlDecoderSetInput() returned " << status << "\n";
            return false;
        }
        JxlDecoderCloseInput(m_decoder.get());

    } else {
        auto buffer = reinterpret_cast<Filesystem::IOMemReader*>(m_io)->buffer();
        status = JxlDecoderSetInput(m_decoder.get(),
                                    const_cast<unsigned char*>(buffer.data()),
                                    buffer.size());
        if (status != JXL_DEC_SUCCESS) {
            return false;
        }
    }

    JxlBasicInfo info;
    JxlPixelFormat format;
    JxlDataType jxl_data_type;
    TypeDesc m_data_type;

    for (;;) {
        JxlDecoderStatus status = JxlDecoderProcessInput(m_decoder.get());
        DBG std::cout << "JxlDecoderProcessInput() returned " << status << "\n";

        if (status == JXL_DEC_ERROR) {
            DBG std::cout << "JXL_DEC_ERROR\n";

            errorfmt("JPEG XL decoder error");
            return false;
        } else if (status == JXL_DEC_NEED_MORE_INPUT) {
            DBG std::cout << "JXL_DEC_NEED_MORE_INPUT\n";

            errorfmt("JPEG XL decoder error, already provided all input\n");
            return false;
        } else if (status == JXL_DEC_BASIC_INFO) {
            DBG std::cout << "JXL_DEC_BASIC_INFO\n";

            // Get the basic information about the image
            if (JXL_DEC_SUCCESS
                != JxlDecoderGetBasicInfo(m_decoder.get(), &info)) {
                errorfmt("JxlDecoderGetBasicInfo failed\n");
                return false;
            }

            // Need to check how we can support bfloat16 if jpegxl supports it
            bool is_float = info.exponent_bits_per_sample > 0;

            switch (info.bits_per_sample) {
            case 8:
                jxl_data_type = JXL_TYPE_UINT8;
                m_data_type   = TypeDesc::UINT8;
                break;
            case 16:
                jxl_data_type = is_float ? JXL_TYPE_FLOAT16 : JXL_TYPE_UINT16;
                m_data_type   = is_float ? TypeDesc::HALF : TypeDesc::UINT16;
                break;
            case 32:
                jxl_data_type = JXL_TYPE_FLOAT;
                m_data_type   = TypeDesc::FLOAT;
                break;
            default: errorfmt("Unsupported bits per sample\n"); return false;
            }

            format = { m_channels, jxl_data_type, JXL_NATIVE_ENDIAN, 0 };

            format.num_channels = info.num_color_channels
                                  + info.num_extra_channels;
            m_channels = info.num_color_channels + info.num_extra_channels;
            JxlResizableParallelRunnerSetThreads(
                m_runner.get(),
                JxlResizableParallelRunnerSuggestThreads(info.xsize,
                                                         info.ysize));
        } else if (status == JXL_DEC_COLOR_ENCODING) {
            DBG std::cout << "JXL_DEC_COLOR_ENCODING\n";

            // Get the ICC color profile of the pixel data
            size_t icc_size;

            if (JXL_DEC_SUCCESS
                != JxlDecoderGetICCProfileSize(m_decoder.get(),
                                               JXL_COLOR_PROFILE_TARGET_DATA,
                                               &icc_size)) {
                errorfmt("JxlDecoderGetICCProfileSize failed\n");
                return false;
            }
            m_icc_profile.resize(icc_size);
            if (JXL_DEC_SUCCESS
                != JxlDecoderGetColorAsICCProfile(m_decoder.get(),
                                                  JXL_COLOR_PROFILE_TARGET_DATA,
                                                  m_icc_profile.data(),
                                                  m_icc_profile.size())) {
                errorfmt("JxlDecoderGetColorAsICCProfile failed\n");
                return false;
            }
        } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
            DBG std::cout << "JXL_DEC_NEED_IMAGE_OUT_BUFFER\n";

            size_t buffer_size;
            if (JXL_DEC_SUCCESS
                != JxlDecoderImageOutBufferSize(m_decoder.get(), &format,
                                                &buffer_size)) {
                errorfmt("JxlDecoderImageOutBufferSize failed\n");
                return false;
            }
            if (buffer_size
                != info.xsize * info.ysize * m_channels * info.bits_per_sample
                       / 8) {
                errorfmt("Invalid out buffer size {} {}\n", buffer_size,
                         info.xsize * info.ysize * m_channels
                             * info.bits_per_sample / 8);
                return false;
            }

            m_buffer.reset(new uint8_t[buffer_size]);

            if (JXL_DEC_SUCCESS
                != JxlDecoderSetImageOutBuffer(m_decoder.get(), &format,
                                               m_buffer.get(), buffer_size)) {
                errorfmt("JxlDecoderSetImageOutBuffer failed\n");
                return false;
            }
        } else if (status == JXL_DEC_FULL_IMAGE) {
            DBG std::cout << "JXL_DEC_FULL_IMAGE\n";

            // Nothing to do. Do not yet return. If the image is an animation, more
            // full frames may be decoded. This example only keeps the last one.
        } else if (status == JXL_DEC_FRAME) {
            DBG std::cout << "JXL_DEC_FRAME\n";

        } else if (status == JXL_DEC_SUCCESS) {
            DBG std::cout << "JXL_DEC_SUCCESS\n";

            // All decoding successfully finished.
            // It's not required to call JxlDecoderReleaseInput(m_decoder.get()) here since
            // the decoder will be destroyed.
            break;
        } else {
            errorfmt("Unknown decoder status\n");
            return false;
        }
    }

    m_spec = ImageSpec(info.xsize, info.ysize, m_channels, m_data_type);

    newspec = m_spec;
    return true;
}



bool
JxlInput::read_native_scanline(int subimage, int miplevel, int y, int /*z*/,
                               void* data)
{
    DBG std::cout << "JxlInput::read_native_scanline(, , " << y << ")\n";
    size_t scanline_size = m_spec.width * m_channels * m_spec.channel_bytes();
    // size_t scanline_size = m_spec.width * m_channels * sizeof(uint8_t);

    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;
    if (y < 0 || y >= m_spec.height)  // out of range scanline
        return false;

    memcpy(data, (void*)(m_buffer.get() + y * scanline_size), scanline_size);

    return true;
}



bool
JxlInput::close()
{
    DBG std::cout << "JxlInput::close()\n";

    if (ioproxy_opened()) {
        close_file();
    }

    m_buffer.reset();
    init();  // Reset to initial state
    return true;
}

OIIO_PLUGIN_NAMESPACE_END
