// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// R3D SDK can be downloaded from the following site:
// https://www.red.com/download/r3d-sdk
//
// The code has been tested with the version 8.5.1 installed in
// /opt/R3DSDKv8_5_1 directory and setting up the variable
// export R3DSDK_ROOT="/opt/R3DSDKv8_5_1"

#include <algorithm>
#include <cassert>
#include <cstdio>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/tiffutils.h>

#include <R3DSDK.h>
#ifdef GPU
#    ifdef CUDA
#        include <R3DSDKCuda.h>
#    endif  // CUDA
#    ifdef OpenCL
#        include <R3DSDKOpenCL.h>
#    endif  // OpenCL
#    ifdef Metal
#        include <R3DSDKMetal.h>
#    endif  // Metal
#endif      // GPU
#include <R3DSDKDefinitions.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef GPU
#    ifdef CUDA
#        include <cuda_runtime.h>
#    endif  // CUDA
#endif      // GPU
#include <vector>

#include <condition_variable>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <string_view>
#include <thread>
#include <time.h>
#include <unistd.h>
#include <vector>

OIIO_PLUGIN_NAMESPACE_BEGIN

#if 0 || !defined(NDEBUG) /* allow R3D configuration debugging */
static bool r3d_debug = Strutil::stoi(Sysutil::getenv("OIIO_R3D_DEBUG"));
#    define DBG(...)   \
        if (r3d_debug) \
        Strutil::print(__VA_ARGS__)
#else
#    define DBG(...)
#endif

class R3dInput final : public ImageInput {
public:
    R3dInput()
    {
        initialize();
        reset();
    }
    ~R3dInput() override
    {
        close();
        terminate();
    }
    const char* format_name(void) const override { return "r3d"; }
    int supports(string_view feature) const override
    {
        return (feature == "ioproxy");
    }
    bool open(const std::string& name, ImageSpec& spec) override;
    bool open(const std::string& name, ImageSpec& spec,
              const ImageSpec& config) override;
    bool seek_subimage(int subimage, int miplevel) override;
    bool read_native_scanline(int subimage, int miplevel, int y, int z,
                              void* data) override;
    void read_frame(int pos);
    int current_subimage(void) const override
    {
        lock_guard lock(*this);
        return m_subimage;
    }
    bool close() override;

    const std::string& filename() const { return m_filename; }

    bool seek(int pos);
    double fps() const;
    int64_t time_stamp(int pos) const;

private:
    std::string m_filename;
    std::unique_ptr<ImageSpec> m_config;  // Saved copy of configuration spec
    R3DSDK::Clip* m_clip;
    R3DSDK::VideoDecodeJob m_job;
    unsigned char* m_image_buffer;
    int m_frames;
    int m_channels;
    float m_fps;
    int m_subimage;
    int64_t m_nsubimages;
    int m_last_search_pos;
    int m_last_decoded_pos;
    bool m_read_frame;
    int m_next_scanline;

    void initialize();
    void reset()
    {
        DBG("R3dInput::reset()\n");

        ioproxy_clear();
        m_config.reset();
        m_clip             = nullptr;
        m_next_scanline    = 0;
        m_read_frame       = false;
        m_subimage         = 0;
        m_last_search_pos  = 0;
        m_last_decoded_pos = 0;
        m_image_buffer     = nullptr;
    }

    void close_file() { reset(); }
    void terminate();
};

// Export version number and create function symbols
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT int r3d_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_EXPORT const char*
r3d_imageio_library_version()
{
    // Note: SDK version can differ from the actual library loaded
    return "R3D 8.5.1";
}

OIIO_EXPORT ImageInput*
r3d_input_imageio_create()
{
    return new R3dInput;
}

OIIO_EXPORT const char* r3d_input_extensions[] = { "r3d", nullptr };

OIIO_PLUGIN_EXPORTS_END



void
R3dInput::initialize()
{
    DBG("R3dInput::initialize()\n");

    std::string library_path
        = Sysutil::getenv("OIIO_R3D_LIBRARY_PATH",
#if defined(__linux__)
                          "/opt/R3DSDKv8_5_1/Redistributable/linux"
#elif defined(__APPLE__)
                          "/Library/R3DSDKv8_5_1/Redistributable/mac"
#elif defined(__WINDOWS__)
                          "C:\\R3DSDKv8_5_1\\Redistributable\\win"
#else
#    error "Unknown OS"
#endif
        );
    // initialize SDK
    // R3DSDK::InitializeStatus init_status = R3DSDK::InitializeSdk(".", OPTION_RED_CUDA);
    R3DSDK::InitializeStatus init_status
        = R3DSDK::InitializeSdk(library_path.c_str(), OPTION_RED_NONE);
    if (init_status != R3DSDK::ISInitializeOK) {
        R3DSDK::FinalizeSdk();
        DBG("Failed to load R3DSDK Library\n");
        return;
    }

    DBG("SDK VERSION: {}\n", R3DSDK::GetSdkVersion());
#ifdef GPU
    // open CUDA device
    RED_CUDA = OpenCUDA(CUDA_DEVICE_ID);

    if (RED_CUDA == NULL) {
        R3DSDK::FinalizeSdk();
        DBG("Failed to initialize CUDA\n");
    }
#endif  // GPU
}



bool
R3dInput::open(const std::string& name, ImageSpec& newspec,
               const ImageSpec& config)
{
    DBG("R3dInput::open(name, newspec, config)\n");

    ioproxy_retrieve_from_config(config);
    m_config.reset(new ImageSpec(config));  // save config spec
    return open(name, newspec);
}



bool
R3dInput::open(const std::string& name, ImageSpec& newspec)
{
    DBG("R3dInput::open(name, newspec)\n");

    m_filename = name;

    DBG("m_filename = {}\n", m_filename);

    // load the clip
    m_clip = new R3DSDK::Clip(m_filename.c_str());

    // let the user know if this failed
    if (m_clip->Status() != R3DSDK::LSClipLoaded) {
        DBG("Error loading {}\n", m_filename);

        delete m_clip;
        m_clip = nullptr;
        return false;
    }

    DBG("Loaded {}\n", m_filename);

    // calculate how much ouput memory we're going to need
    size_t width  = m_clip->Width();
    size_t height = m_clip->Height();

    m_channels = 3;

    DBG("Video frame count {}\n", m_clip->VideoFrameCount());

    m_frames     = m_clip->VideoFrameCount();
    m_nsubimages = m_frames;

    DBG("Video framerate {}\n", m_clip->VideoAudioFramerate());

    m_fps = m_clip->VideoAudioFramerate();

    // three channels (RGB) in 16-bit (2 bytes) requires this much memory:
    size_t memNeeded = m_channels * width * height * sizeof(uint16_t);

    // alloc this memory 16-byte aligned
    m_image_buffer = static_cast<unsigned char*>(aligned_malloc(memNeeded, 16));

    if (m_image_buffer == NULL) {
        DBG("Failed to allocate {} bytes of memory for output image\n",
            static_cast<int>(memNeeded));

        return false;
    }

    // letting the decoder know how big the buffer is (we do that here
    // since AlignedMalloc below will overwrite the value in this
    m_job.OutputBufferSize = memNeeded;

    m_job.Mode = R3DSDK::DECODE_FULL_RES_PREMIUM;

    // store the image here
    m_job.OutputBuffer = m_image_buffer;

    // Interleaved RGB decoding in 16-bits per pixel
    m_job.PixelType   = R3DSDK::PixelType_16Bit_RGB_Interleaved;
    m_job.BytesPerRow = m_channels * width * sizeof(uint16_t);

    m_spec = ImageSpec(width, height, m_channels, TypeDesc::UINT16);
    m_spec.attribute("FramesPerSecond", TypeFloat, &m_fps);
    m_spec.attribute("oiio:Movie", true);
    m_spec.attribute("oiio:subimages", int(m_frames));
    m_spec.attribute("oiio:BitsPerSample", 16);

    newspec         = m_spec;
    m_next_scanline = 0;
    return true;
}



void
R3dInput::read_frame(int pos)
{
    DBG("R3dInput::read_frame({})\n", pos);

    if (m_last_decoded_pos + 1 != pos) {
        seek(pos);
    }

    R3DSDK::DecodeStatus status = m_clip->DecodeVideoFrame(pos, m_job);
    if (status != R3DSDK::DSDecodeOK) {
        DBG("Failed to decode frame {}\n", pos);
    }

    m_last_search_pos  = pos;
    m_last_decoded_pos = pos;
    m_read_frame       = true;
    m_next_scanline    = 0;
}



bool
R3dInput::seek_subimage(int subimage, int miplevel)
{
    DBG("R3dInput::seek_subimage({}, {})\n", subimage, miplevel);

    if (subimage < 0 || subimage >= m_nsubimages || miplevel > 0) {
        return false;
    }
    if (subimage == m_subimage) {
        return true;
    }
    m_subimage   = subimage;
    m_read_frame = false;
    return true;
}



bool
R3dInput::read_native_scanline(int subimage, int miplevel, int y, int /*z*/,
                               void* data)
{
    DBG("R3dInput::read_native_scanline({}, {}, {})\n", subimage, miplevel, y);

    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;
    if (!m_read_frame) {
        read_frame(m_subimage);
    }
    if (y < 0 || y >= m_spec.height)  // out of range scanline
        return false;
    if (m_next_scanline > y) {
        // User is trying to read an earlier scanline than the one we're
        // up to.  Easy fix: close the file and re-open.
        // Don't forget to save and restore any configuration settings.
        ImageSpec configsave;
        if (m_config)
            configsave = *m_config;
        ImageSpec dummyspec;
        int subimage = current_subimage();
        if (!close() || !open(m_filename, dummyspec, configsave)
            || !seek_subimage(subimage, 0))
            return false;  // Somehow, the re-open failed
        OIIO_DASSERT(m_next_scanline == 0 && current_subimage() == subimage);
    }

    return copy_image(
        m_spec.nchannels, m_spec.width, 1, m_spec.depth,
        m_image_buffer + y * m_spec.nchannels * m_spec.width * sizeof(uint16_t),
        m_spec.nchannels * sizeof(uint16_t), AutoStride, AutoStride, AutoStride,
        data, AutoStride, AutoStride, AutoStride);
}



bool
R3dInput::seek(int frame)
{
    DBG("R3dInput::seek({})\n", frame);
    return true;
}



int64_t
R3dInput::time_stamp(int frame) const
{
    DBG("R3dInput::time_stamp({})\n", frame);
    return 0;
}



double
R3dInput::fps() const
{
    DBG("R3dInput::fps()\n");
    return (double)m_fps;
}



bool
R3dInput::close()
{
    DBG("R3dInput::close()\n");

    if (m_clip) {
        delete m_clip;
        m_clip = nullptr;
    }
    if (m_image_buffer) {
        aligned_free(m_image_buffer);
        m_image_buffer = nullptr;
    }
    reset();  // Reset to initial state
    return true;
}



void
R3dInput::terminate()
{
    DBG("R3dInput::terminate()\n");
    R3DSDK::FinalizeSdk();
}

OIIO_PLUGIN_NAMESPACE_END
