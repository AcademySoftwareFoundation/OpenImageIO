// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <algorithm>
#include <cassert>
#include <cstdio>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/tiffutils.h>

#include <R3DSDK.h>
#include <R3DSDKCuda.h>
#include <R3DSDKDefinitions.h>

#include <cuda_runtime.h>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <mutex>
#include <thread>
#include <condition_variable>
#include <functional>
#include <list>
#include <map>
#include <vector>
#include <unistd.h>
#include <time.h>
#include <iostream>

unsigned char * AlignedMalloc(size_t & sizeNeeded)
{
    // alloc 15 bytes more to make sure we can align the buffer in case it isn't
    unsigned char * buffer = (unsigned char *)malloc(sizeNeeded + 15U);

    if (!buffer)
        return nullptr;

    sizeNeeded = 0U;

    // cast to a 32-bit or 64-bit (depending on platform) integer so we can do the math
    uintptr_t ptr = (uintptr_t)buffer;

    // check if it's already aligned, if it is we're done
    if ((ptr % 16U) == 0U)
      return buffer;

    // calculate how many bytes we need
    sizeNeeded = 16U - (ptr % 16U);

    return buffer + sizeNeeded;
}

OIIO_PLUGIN_NAMESPACE_BEGIN

#define DBG if (1)

class R3dInput final : public ImageInput {
public:
    R3dInput() { initialize(); reset(); }
    ~R3dInput() override { close(); terminate(); }
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
    R3DSDK::Clip *m_clip;
    R3DSDK::VideoDecodeJob m_job;
    unsigned char *m_image_buffer;
    size_t m_adjusted;
    int m_frames;
    int m_channels;
    float m_fps;
    int m_subimage;
    int64_t m_nsubimages;
    int m_last_search_pos;
    int m_last_decoded_pos;
    bool m_read_frame;
    int m_next_scanline;
    // std::vector<uint16_t> m_pixels;

    void initialize();
    void reset()
    {
        DBG std::cout << "R3dInput::reset()\n";

        ioproxy_clear();
        m_config.reset();
        m_clip = nullptr;
        m_next_scanline    = 0;
        m_read_frame       = false;
        m_subimage         = 0;
        m_last_search_pos  = 0;
        m_last_decoded_pos = 0;
        m_image_buffer = nullptr;
        m_adjusted = 0;
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
    return "R3D";
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
    DBG std::cout << "R3dInput::initialize()\n";

    // initialize SDK
    // R3DSDK::InitializeStatus init_status = R3DSDK::InitializeSdk(".", OPTION_RED_CUDA);
    R3DSDK::InitializeStatus init_status = R3DSDK::InitializeSdk("/opt/R3DSDKv8_5_1/Redistributable/linux", OPTION_RED_NONE);
    if (init_status != R3DSDK::ISInitializeOK)
    {
        R3DSDK::FinalizeSdk();
        DBG std::cout << "Failed to load R3DSDK Lib: " << init_status << "\n";
        return;
    }

    DBG std::cout << "SDK VERSION: " << R3DSDK::GetSdkVersion() << "\n";
#ifdef GPU
    // open CUDA device
    RED_CUDA = OpenCUDA(CUDA_DEVICE_ID);

    if (RED_CUDA == NULL)
    {
        R3DSDK::FinalizeSdk();
        DBG std::cout << "Failed to initialize CUDA\n";
    }
#endif // GPU
}



bool
R3dInput::open(const std::string& name, ImageSpec& newspec,
               const ImageSpec& config)
{
    DBG std::cout << "R3dInput::open(name, newspec, config)\n";

    ioproxy_retrieve_from_config(config);
    m_config.reset(new ImageSpec(config));  // save config spec
    return open(name, newspec);
}



bool
R3dInput::open(const std::string& name, ImageSpec& newspec)
{
    DBG std::cout << "R3dInput::open(name, newspec)\n";

    m_filename = name;

    DBG std::cout << "m_filename = " << m_filename << "\n";

    // load the clip
    m_clip = new R3DSDK::Clip(m_filename.c_str());

    // let the user know if this failed
    if (m_clip->Status() != R3DSDK::LSClipLoaded)
    {
        DBG std::cout << "Error loading " << m_filename << "\n";

        delete m_clip;
        m_clip = nullptr;
        return false;
    }

    DBG std::cout << "Loaded " << m_filename << "\n";

    // calculate how much ouput memory we're going to need
    size_t width = m_clip->Width();
    size_t height = m_clip->Height();

    m_channels = 3;

    DBG std::cout << "Video frame count " << m_clip->VideoFrameCount() << "\n";

    m_frames = m_clip->VideoFrameCount();
    m_nsubimages = m_frames;

    DBG std::cout << "Video framerate " << m_clip->VideoAudioFramerate() << "\n";

    m_fps    = m_clip->VideoAudioFramerate();

    // three channels (RGB) in 16-bit (2 bytes) requires this much memory:
    size_t memNeeded = width * height * m_channels * sizeof(uint16_t);

    // make a copy for AlignedMalloc (it will change it)
    m_adjusted = memNeeded;

    // alloc this memory 16-byte aligned
    m_image_buffer = AlignedMalloc(m_adjusted);

    if (m_image_buffer == NULL)
    {
        DBG std::cout << "Failed to allocate " << static_cast<int>(memNeeded) << " bytes of memory for output image\n";

        return false;
    }

    m_job.BytesPerRow = width * sizeof(uint16_t);

    // letting the decoder know how big the buffer is (we do that here
    // since AlignedMalloc below will overwrite the value in this
    m_job.OutputBufferSize = memNeeded;

    m_job.Mode = R3DSDK::DECODE_FULL_RES_PREMIUM;

    // store the image here
    m_job.OutputBuffer = m_image_buffer;

    // store the image in a 16-bit planar RGB format
    m_job.PixelType = R3DSDK::PixelType_16Bit_RGB_Planar;
    // Interleaved RGB decoding in 16-bits per pixel
    // m_job.PixelType = R3DSDK::PixelType_16Bit_RGB_Interleaved;

    m_spec = ImageSpec(width, height, m_channels, TypeDesc::UINT16);
    m_spec.attribute("FramesPerSecond", TypeFloat, &m_fps);
    m_spec.attribute("oiio:Movie", true);
    m_spec.attribute("oiio:subimages", int(m_frames));
    m_spec.attribute("oiio:BitsPerSample", 16);

    newspec = m_spec;
    m_next_scanline = 0;
    return true;
}



void
R3dInput::read_frame(int pos)
{
    DBG std::cout << "R3dInput::read_frame(" << pos << ")\n";

    if (m_last_decoded_pos + 1 != pos) {
        seek(pos);
    }

    R3DSDK::DecodeStatus status = m_clip->DecodeVideoFrame(pos, m_job);
    if (status != R3DSDK::DSDecodeOK) {
        DBG std::cout << "Failed to decode frame " << pos << "\n";
    }

    m_last_search_pos = pos;
    m_last_decoded_pos = pos;
    m_read_frame = true;
    m_next_scanline = 0;
}



bool
R3dInput::seek_subimage(int subimage, int miplevel)
{
    DBG std::cout << "R3dInput::seek_subimage(" << subimage << ", " << miplevel << ")\n";

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
    DBG std::cout << "R3dInput::read_native_scanline(, , " << y << ")\n";

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

    uint16_t *r, *g, *b;
    uint16_t *d = (uint16_t *)data;
    size_t scanline_size = m_spec.width * sizeof(uint16_t);
    size_t plane_size = scanline_size * m_spec.height;

    r = (uint16_t *)((uint8_t*)m_image_buffer + y * scanline_size);
    g = (uint16_t *)((uint8_t*)m_image_buffer + y * scanline_size + plane_size);
    b = (uint16_t *)((uint8_t*)m_image_buffer + y * scanline_size + 2 * plane_size);

    for (int x = 0; x < m_spec.width; x++)
    {
        *d++ = r[x];
        *d++ = g[x];
        *d++ = b[x];
    }
    
    return true;
}



bool
R3dInput::seek(int frame)
{
    DBG std::cout << "R3dInput::seek(" << frame << ")\n";
    return true;
}



int64_t
R3dInput::time_stamp(int frame) const
{
    DBG std::cout << "R3dInput::time_stamp(" << frame << ")\n";
    return 0;
}



double
R3dInput::fps() const
{
    DBG std::cout << "R3dInput::fps()\n";
    return (double) m_fps;
}



bool
R3dInput::close()
{
    DBG std::cout << "R3dInput::close()\n";

    if (m_clip) {
        delete m_clip;
        m_clip = nullptr;
    }
    if (m_image_buffer) {
        free(m_image_buffer - m_adjusted);
        m_image_buffer = nullptr;
        m_adjusted = 0;
    }
    reset();  // Reset to initial state
    return true;
}



void
R3dInput::terminate()
{
    DBG std::cout << "R3dInput::terminate()\n";
    R3DSDK::FinalizeSdk();
}

OIIO_PLUGIN_NAMESPACE_END
