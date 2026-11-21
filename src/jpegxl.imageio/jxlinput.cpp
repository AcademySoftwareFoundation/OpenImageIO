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
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>

#include <OpenImageIO/color.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/tiffutils.h>

#include <jxl/color_encoding.h>
#include <jxl/decode.h>
#include <jxl/decode_cxx.h>
#include <jxl/resizable_parallel_runner_cxx.h>

OIIO_PLUGIN_NAMESPACE_BEGIN

#define DBG if (0)

// Cap on libjxl working memory: a multiple of "limits:imagesize_MB" plus a
// floor for fixed overhead. libjxl 0.13 requests up to ~4.2x the output size
// (uint8 output) plus ~15 MB.
static constexpr size_t jxl_working_mem_factor   = 5;
static constexpr size_t jxl_working_mem_floor_MB = 64;

// Allocation tracker handed to libjxl as a JxlMemoryManager.
struct JxlMemTracker {
    std::atomic<size_t> used { 0 };
    size_t limit = 0;  // 0 means unlimited
    std::atomic<bool> exceeded { false };
};



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
    uint32_t m_channels;
    JxlPixelFormat m_format;
    JxlMemTracker m_memtracker;  // must outlive m_decoder
    JxlMemoryManager m_memmgr;
    JxlDecoderPtr m_decoder;
    JxlResizableParallelRunnerPtr m_runner;
    std::unique_ptr<ImageSpec> m_config;  // Saved copy of configuration spec
    std::vector<uint8_t> m_icc_profile;
    std::unique_ptr<uint8_t[]> m_buffer;
    std::vector<uint8_t> m_input;  // Pending input bytes (file proxy only)
    size_t m_input_offset;         // File offset of the next unread byte
    size_t m_input_chunk;          // Size of the next file read
    bool m_input_set;              // Has input been given to m_decoder?
    bool m_decode_failed;          // Pixel decode attempted and failed

    void init()
    {
        ioproxy_clear();
        m_config.reset();
        m_decoder = nullptr;
        m_runner  = nullptr;
        m_buffer  = nullptr;
        m_icc_profile.clear();
        release_input();
        m_decode_failed = false;
    }

    void release_input()
    {
        m_input.clear();
        m_input.shrink_to_fit();
        m_input_offset = 0;
        m_input_chunk  = 0;
        m_input_set    = false;
    }

    // Free the decoder, its worker threads, and its input.
    void release_decoder()
    {
        m_decoder.reset();
        m_runner.reset();
        release_input();
    }

    // Create m_decoder (with the memory cap and parallel runner) and
    // subscribe it to `events`, ready to be fed from the beginning.
    bool start_decoder(int events);
    // Give the decoder more input. Return false if there is none left.
    bool feed_input();
    // Decode the full image into m_buffer, if not already done.
    bool decode_pixels();
    void report_decoder_error();

    void close_file() { init(); }
};



static void*
jxl_tracked_alloc(void* opaque, size_t size)
{
    constexpr size_t header = alignof(std::max_align_t);
    auto tracker            = static_cast<JxlMemTracker*>(opaque);
    if (size > std::numeric_limits<size_t>::max() - header)
        return nullptr;
    size_t used = tracker->used.fetch_add(size) + size;
    if (tracker->limit && used > tracker->limit) {
        tracker->used.fetch_sub(size);
        tracker->exceeded = true;
        return nullptr;
    }
    auto p = static_cast<char*>(malloc(size + header));
    if (!p) {
        tracker->used.fetch_sub(size);
        return nullptr;
    }
    memcpy(p, &size, sizeof(size));
    return p + header;
}



static void
jxl_tracked_free(void* opaque, void* address)
{
    constexpr size_t header = alignof(std::max_align_t);
    if (!address)
        return;
    auto p = static_cast<char*>(address) - header;
    size_t size;
    memcpy(&size, p, sizeof(size));
    static_cast<JxlMemTracker*>(opaque)->used.fetch_sub(size);
    free(p);
}



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
JxlInput::start_decoder(int events)
{
    m_decoder.reset();
    release_input();
    m_memtracker.exceeded = false;
    size_t limit_MB = std::max(0,
                               OIIO::get_int_attribute("limits:imagesize_MB"));
    m_memtracker.limit
        = limit_MB
              ? (limit_MB * jxl_working_mem_factor + jxl_working_mem_floor_MB)
                    << 20
              : 0;
    m_memmgr.opaque = &m_memtracker;
    m_memmgr.alloc  = jxl_tracked_alloc;
    m_memmgr.free   = jxl_tracked_free;

    m_decoder = JxlDecoderMake(&m_memmgr);
    if (!m_decoder) {
        errorfmt("Could not create JPEG XL decoder");
        return false;
    }
    if (!m_runner) {
        m_runner = JxlResizableParallelRunnerMake(nullptr);
        if (!m_runner) {
            errorfmt("Could not create JPEG XL parallel runner");
            return false;
        }
    }
    if (JxlDecoderSetParallelRunner(m_decoder.get(), JxlResizableParallelRunner,
                                    m_runner.get())
            != JXL_DEC_SUCCESS
        || JxlDecoderSubscribeEvents(m_decoder.get(), events)
               != JXL_DEC_SUCCESS) {
        errorfmt("Could not initialize JPEG XL decoder");
        return false;
    }
    return true;
}



bool
JxlInput::feed_input()
{
    Filesystem::IOProxy* io = ioproxy();
    if (string_view(io->proxytype()) == "memreader") {
        if (m_input_set)
            return false;
        auto buffer = static_cast<Filesystem::IOMemReader*>(io)->buffer();
        if (JxlDecoderSetInput(m_decoder.get(), buffer.data(), buffer.size())
            != JXL_DEC_SUCCESS)
            return false;
        JxlDecoderCloseInput(m_decoder.get());
        m_input_set = true;
        return true;
    }

    // File proxy: read in growing chunks so that a header-only open reads
    // little more than the header.
    size_t filesize = io->size();
    if (m_input_offset >= filesize)
        return false;
    size_t remaining = m_input_set ? JxlDecoderReleaseInput(m_decoder.get())
                                   : 0;
    remaining        = std::min(remaining, m_input.size());
    m_input_chunk    = std::min(std::max(m_input_chunk * 2, size_t(65536)),
                                size_t(16) << 20);
    size_t chunk     = std::min(m_input_chunk, filesize - m_input_offset);
    if (remaining)
        memmove(m_input.data(), m_input.data() + m_input.size() - remaining,
                remaining);
    m_input.resize(remaining + chunk);
    if (io->pread(m_input.data() + remaining, chunk, m_input_offset) != chunk) {
        errorfmt("Failed to read {} bytes from \"{}\"", chunk, m_filename);
        return false;
    }
    m_input_offset += chunk;
    if (JxlDecoderSetInput(m_decoder.get(), m_input.data(), m_input.size())
        != JXL_DEC_SUCCESS)
        return false;
    if (m_input_offset >= filesize)
        JxlDecoderCloseInput(m_decoder.get());
    m_input_set = true;
    return true;
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
        close();
        return false;
    }

    if (!valid_file(m_io)) {
        DBG std::cout << "JxlInput::valid_file() return false\n";
        errorfmt("Possible corrupt file, "
                 "JPEG XL signature verification failed\n");
        close();
        return false;
    }

    // Only decode the headers here. Pixels are decoded on first read.
    if (!start_decoder(JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING)) {
        close();
        return false;
    }

    JxlBasicInfo info;
    JxlDataType jxl_data_type;
    TypeDesc m_data_type;
    JxlColorEncoding color_encoding {};
    bool have_color_encoding = false;
    bool got_basic_info      = false;
    bool got_color_encoding  = false;

    while (!got_color_encoding) {
        JxlDecoderStatus status = JxlDecoderProcessInput(m_decoder.get());
        DBG std::cout << "JxlDecoderProcessInput() returned " << status << "\n";

        if (status == JXL_DEC_ERROR) {
            DBG std::cout << "JXL_DEC_ERROR\n";
            report_decoder_error();
            close();
            return false;
        } else if (status == JXL_DEC_NEED_MORE_INPUT) {
            DBG std::cout << "JXL_DEC_NEED_MORE_INPUT\n";
            if (!feed_input()) {
                if (!has_error())
                    errorfmt("Possible corrupt file, JPEG XL header is "
                             "truncated\n");
                close();
                return false;
            }
        } else if (status == JXL_DEC_BASIC_INFO) {
            DBG std::cout << "JXL_DEC_BASIC_INFO\n";

            // Get the basic information about the image
            if (JXL_DEC_SUCCESS
                != JxlDecoderGetBasicInfo(m_decoder.get(), &info)) {
                errorfmt("JxlDecoderGetBasicInfo failed\n");
                close();
                return false;
            }

            // Need to check how we can support bfloat16 if jpegxl supports it
            bool is_float = info.exponent_bits_per_sample > 0;

            if (info.bits_per_sample <= 8) {
                jxl_data_type = JXL_TYPE_UINT8;
                m_data_type   = TypeDesc::UINT8;
            } else if (info.bits_per_sample <= 16) {
                jxl_data_type = is_float ? JXL_TYPE_FLOAT16 : JXL_TYPE_UINT16;
                m_data_type   = is_float ? TypeDesc::HALF : TypeDesc::UINT16;
            } else if (info.bits_per_sample <= 32) {
                jxl_data_type = JXL_TYPE_FLOAT;
                m_data_type   = TypeDesc::FLOAT;
            } else {
                errorfmt("Unsupported bits per sample\n");
                close();
                return false;
            }

            m_channels = info.num_color_channels + info.num_extra_channels;
            m_format   = { m_channels, jxl_data_type, JXL_NATIVE_ENDIAN, 0 };

            // Validate dimensions/channel count before the decoder proceeds
            // further and may attempt huge allocations on corrupt inputs.
            m_spec = ImageSpec(info.xsize, info.ysize, m_channels, m_data_type);
            if (!check_open(m_spec, { 0, (1 << 30) - 1, 0, (1 << 30) - 1, 0, 1,
                                      0, 4099 })) {
                close();
                return false;
            }
            got_basic_info = true;
        } else if (status == JXL_DEC_COLOR_ENCODING) {
            DBG std::cout << "JXL_DEC_COLOR_ENCODING\n";

            // Get the ICC color profile of the pixel data
            size_t icc_size;

            if (JXL_DEC_SUCCESS
                != JxlDecoderGetICCProfileSize(m_decoder.get(),
                                               JXL_COLOR_PROFILE_TARGET_DATA,
                                               &icc_size)) {
                errorfmt("JxlDecoderGetICCProfileSize failed\n");
                close();
                return false;
            }
            m_icc_profile.resize(icc_size);
            if (JXL_DEC_SUCCESS
                != JxlDecoderGetColorAsICCProfile(m_decoder.get(),
                                                  JXL_COLOR_PROFILE_TARGET_DATA,
                                                  m_icc_profile.data(),
                                                  m_icc_profile.size())) {
                errorfmt("JxlDecoderGetColorAsICCProfile failed\n");
                close();
                return false;
            }

            // Get the color encoding of the pixel data
            // This will return JXL_DEC_ERR for a valid file without color
            // encoding information, so don't report an error.
            if (JXL_DEC_SUCCESS
                == JxlDecoderGetColorAsEncodedProfile(
                    m_decoder.get(), JXL_COLOR_PROFILE_TARGET_DATA,
                    &color_encoding)) {
                have_color_encoding = true;
            }
            got_color_encoding = true;
        } else if (status == JXL_DEC_SUCCESS) {
            DBG std::cout << "JXL_DEC_SUCCESS\n";
            break;
        } else {
            errorfmt("Unknown decoder status\n");
            close();
            return false;
        }
    }

    if (!got_basic_info) {
        errorfmt("Possible corrupt file, no JPEG XL basic info found\n");
        close();
        return false;
    }

    // Release the header decoder and its input until pixels are needed.
    release_decoder();

    // Read ICC profile
    if (m_icc_profile.size() && m_icc_profile.data()) {
        m_spec.attribute("ICCProfile",
                         TypeDesc(TypeDesc::UINT8, m_icc_profile.size()),
                         m_icc_profile.data());
        std::string errormsg;

        bool ok = decode_icc_profile(cspan<uint8_t>(m_icc_profile.data(),
                                                    m_icc_profile.size()),
                                     m_spec, errormsg);

        if (!ok && OIIO::get_int_attribute("imageinput:strict")) {
            errorfmt("Possible corrupt file, could not decode ICC profile: {}\n",
                     errormsg);
            close();
            return false;
        }
    }

    // Read CICP from color encoding. Custom primaries, custom white point and
    // arbitrary gamma not supported currently.
    if (have_color_encoding && color_encoding.primaries != JXL_PRIMARIES_CUSTOM
        && color_encoding.white_point != JXL_WHITE_POINT_CUSTOM
        && color_encoding.transfer_function != JXL_TRANSFER_FUNCTION_GAMMA) {
        int color_primaries = color_encoding.primaries;
        // JxlPrimaries enum only covers P3 primaries as value 11 and not 12
        // but CICP has separate code values based on white point.
        if (color_primaries == JXL_PRIMARIES_P3
            && color_encoding.white_point == JXL_WHITE_POINT_D65) {
            color_primaries = 12;
        }
        const int cicp[4] = { color_primaries, color_encoding.transfer_function,
                              0 /* RGB */, 1 /* Full range */ };
        m_spec.attribute("CICP", TypeDesc(TypeDesc::INT, 4), cicp);
        const ColorConfig& colorconfig(ColorConfig::default_colorconfig());
        string_view interop_id = colorconfig.get_color_interop_id(cicp);
        if (!interop_id.empty())
            m_spec.attribute("oiio:ColorSpace", interop_id);
    }

    newspec = m_spec;
    return true;
}



void
JxlInput::report_decoder_error()
{
    if (m_memtracker.exceeded)
        errorfmt(
            "JPEG XL decoder exceeded its {} MB working memory limit (derived from \"limits:imagesize_MB\").\n"
            "If this is a valid file, raise the OIIO attribute \"limits:imagesize_MB\".",
            m_memtracker.limit >> 20);
    else
        errorfmt("JPEG XL decoder error");
}



bool
JxlInput::decode_pixels()
{
    if (m_buffer)
        return true;
    if (m_decode_failed) {
        errorfmt("JPEG XL pixel decode previously failed");
        return false;
    }
    m_decode_failed = true;  // cleared on success

    auto fail = [&]() {
        release_decoder();
        m_buffer.reset();
        return false;
    };

    if (!start_decoder(JXL_DEC_FULL_IMAGE))
        return fail();
    JxlResizableParallelRunnerSetThreads(
        m_runner.get(),
        JxlResizableParallelRunnerSuggestThreads(m_spec.width, m_spec.height));

    for (;;) {
        JxlDecoderStatus status = JxlDecoderProcessInput(m_decoder.get());
        if (status == JXL_DEC_ERROR) {
            report_decoder_error();
            return fail();
        } else if (status == JXL_DEC_NEED_MORE_INPUT) {
            if (!feed_input()) {
                if (!has_error())
                    errorfmt("Possible corrupt file, JPEG XL image data is "
                             "truncated");
                return fail();
            }
        } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
            size_t buffer_size;
            if (JXL_DEC_SUCCESS
                != JxlDecoderImageOutBufferSize(m_decoder.get(), &m_format,
                                                &buffer_size)) {
                errorfmt("JxlDecoderImageOutBufferSize failed\n");
                return fail();
            }
            size_t expected_size = m_spec.image_bytes(true);
            if (buffer_size != expected_size) {
                errorfmt("Invalid out buffer size {} {}\n", buffer_size,
                         expected_size);
                return fail();
            }
            // Animations re-request a buffer per frame; keep the last frame.
            if (!m_buffer)
                m_buffer.reset(new (std::nothrow) uint8_t[buffer_size]);
            if (!m_buffer) {
                errorfmt("Could not allocate {} bytes for JPEG XL image",
                         buffer_size);
                return fail();
            }
            if (JXL_DEC_SUCCESS
                != JxlDecoderSetImageOutBuffer(m_decoder.get(), &m_format,
                                               m_buffer.get(), buffer_size)) {
                errorfmt("JxlDecoderSetImageOutBuffer failed\n");
                return fail();
            }
        } else if (status == JXL_DEC_FULL_IMAGE) {
            // More frames may follow for an animation.
        } else if (status == JXL_DEC_SUCCESS) {
            break;
        } else {
            errorfmt("Unknown decoder status\n");
            return fail();
        }
    }

    if (!m_buffer) {
        errorfmt("Possible corrupt file, no JPEG XL image data decoded\n");
        return fail();
    }

    // Free the decoder's working memory, threads, and input now that pixels
    // are ready.
    release_decoder();
    m_decode_failed = false;
    return true;
}



bool
JxlInput::read_native_scanline(int subimage, int miplevel, int y, int /*z*/,
                               void* data)
{
    DBG std::cout << "JxlInput::read_native_scanline(, , " << y << ")\n";
    size_t scanline_size = m_spec.scanline_bytes(true);

    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;
    if (y < 0 || y >= m_spec.height)  // out of range scanline
        return false;
    if (!decode_pixels())
        return false;

    memcpy(data, m_buffer.get() + y * scanline_size, scanline_size);

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
