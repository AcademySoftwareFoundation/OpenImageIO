// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// R3D SDK can be downloaded from the following site:
// https://www.red.com/download/r3d-sdk-beta
//
// The code has been tested with the version 9.0.0 Beta 1 installed in
// /opt/R3DSDKv9_0_0-BETA1 directory and setting up the variable
// export R3DSDK_ROOT="/opt/R3DSDKv9_0_0-BETA1"

#define GPU
#define CUDA

#include "OpenImageIO/platform.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
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

#if 1 || !defined(NDEBUG)  // allow R3D configuration debugging
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
        if (feature == "multiimage" || feature == "appendsubimage"
            || feature == "random_access" || feature == "ioproxy")
            return true;
        return false;
    }
    bool open(const std::string& name, ImageSpec& newspec,
              const ImageSpec& config) override;
    bool open(const std::string& name, ImageSpec& newspec) override
    {
        return open(name, newspec, ImageSpec());
    }
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
#ifdef GPU
    bool m_gpu;
    R3DSDK::DecodeStatus m_supported;
    R3DSDK::AsyncDecompressJob m_async_decompress_job;
#endif  // GPU
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



#ifdef CUDA

namespace {
R3DSDK::GpuDecoder* GPU_DECODER;
R3DSDK::REDCuda* RED_CUDA;

int CUDA_DEVICE_ID       = 0;
volatile bool decodeDone = false;
}  // namespace

class SimpleMemoryPool {
public:
    static SimpleMemoryPool* getInstance()
    {
        static SimpleMemoryPool* instance = NULL;

        if (instance == NULL) {
            std::unique_lock<std::mutex> lock(guard);
            if (instance == NULL) {
                instance = new SimpleMemoryPool();
            }
        }
        return instance;
    }

    static cudaError_t cudaMalloc(void** p, size_t size)
    {
        DBG("cudaMalloc {}\n", size);

        return getInstance()->malloc_d(p, size);
    }

    static cudaError_t cudaFree(void* p) { return getInstance()->free_d(p); }

    static cudaError_t cudaMallocArray(struct cudaArray** array,
                                       const struct cudaChannelFormatDesc* desc,
                                       size_t width, size_t height = 0,
                                       unsigned int flags = 0)
    {
        DBG("cudaMallocArray {} {} {}\n", width, height, flags);

        return getInstance()->malloc_array(array, desc, width, height, flags);
    }

    static cudaError_t
    cudaMalloc3DArray(struct cudaArray** array,
                      const struct cudaChannelFormatDesc* desc,
                      struct cudaExtent ext, unsigned int flags = 0)
    {
        return getInstance()->malloc_array_3d(array, desc, ext, flags);
    }

    static cudaError_t cudaFreeArray(cudaArray* p)
    {
        getInstance()->free_array(p);

        return cudaSuccess;
    }

    static cudaError_t cudaMallocHost(void** p, size_t size)
    {
        return getInstance()->malloc_h(p, size);
    }

    static cudaError_t cudaHostAlloc(void** p, size_t size, unsigned int flags)
    {
        return getInstance()->hostAlloc_h(p, size, flags);
    }

    static cudaError_t cudaFreeHost(void* p)
    {
        getInstance()->free_h(p);

        return cudaSuccess;
    }

private:
    static std::mutex guard;

    cudaError_t malloc_d(void** p, size_t size)
    {
        int device = 0;
        cudaGetDevice(&device);
        cudaError_t result = cudaSuccess;
        *p                 = _device.findBlock(size, device);

        if (*p == NULL) {
            result = ::cudaMalloc(p, size);
            if (result != cudaSuccess) {
                std::cerr << "Memory allocation of " << size
                          << " bytes failed: " << result << "\n";
                _device.sweep();
                _array.sweep();
                result = ::cudaMalloc(p, size);
            }
            if (result == cudaSuccess)
                _device.addBlock(*p, size, device);
        }
        return result;
    }

    cudaError_t free_d(void* p)
    {
        _device.releaseBlock(p);
        return cudaSuccess;
    }

    cudaError_t malloc_array(struct cudaArray** array,
                             const struct cudaChannelFormatDesc* desc,
                             size_t width, size_t height = 0,
                             unsigned int flags = 0)
    {
        int device = 0;
        cudaGetDevice(&device);
        cudaError_t result = cudaSuccess;
        *array = (cudaArray*)_array.findBlock(width, height, 0, *desc, device);

        if (*array == NULL) {
            result = ::cudaMallocArray(array, desc, width, height, flags);
            if (result != cudaSuccess) {
                DBG("Memory allocation failed: {}\n", static_cast<int>(result));
                _device.sweep();
                _array.sweep();
                result = ::cudaMallocArray(array, desc, width, height, flags);
            }
            if (result == cudaSuccess)
                _array.addBlock(*array, width, height, 0, *desc, device);
        }
        return result;
    }

    cudaError_t malloc_array_3d(struct cudaArray** array,
                                const struct cudaChannelFormatDesc* desc,
                                const struct cudaExtent& ext,
                                unsigned int flags = 0)
    {
        int device = 0;
        cudaGetDevice(&device);
        cudaError_t result = cudaSuccess;
        *array = (cudaArray*)_array.findBlock(ext.width, ext.height, ext.depth,
                                              *desc, device);

        if (*array == NULL) {
            result = ::cudaMalloc3DArray(array, desc, ext, flags);
            if (result != cudaSuccess) {
                DBG("Memory allocation failed: {}\n", static_cast<int>(result));
                _device.sweep();
                _array.sweep();
                result = ::cudaMalloc3DArray(array, desc, ext, flags);
            }
            if (result == cudaSuccess)
                _array.addBlock(*array, ext.width, ext.height, ext.depth, *desc,
                                device);
        }
        return result;
    }

    void free_array(void* p) { _array.releaseBlock(p); }

    cudaError_t malloc_h(void** p, size_t size)
    {
        int device = 0;
        cudaGetDevice(&device);
        cudaError_t result = cudaSuccess;
        *p                 = _host.findBlock(size, device);

        if (*p == NULL) {
            result = ::cudaMallocHost(p, size);
            if (result != cudaSuccess) {
                DBG("Memory allocation failed: {}\n", static_cast<int>(result));
                _host.sweep();
                result = ::cudaMallocHost(p, size);
            }
            if (result == cudaSuccess)
                _host.addBlock(*p, size, device);
        }
        return result;
    }

    void free_h(void* p)
    {
        if (!_host.releaseBlock(p)) {
            _hostAlloc.releaseBlock(p);
        }
    }

    cudaError_t hostAlloc_h(void** p, size_t size, unsigned int flags)
    {
        int device = 0;
        cudaGetDevice(&device);
        cudaError_t result = cudaSuccess;
        *p                 = _hostAlloc.findBlock(size, device);

        if (*p == NULL) {
            result = ::cudaHostAlloc(p, size, flags);
            if (result != cudaSuccess) {
                DBG("Memory allocation failed: {}\n", static_cast<int>(result));
                _hostAlloc.sweep();
                result = ::cudaHostAlloc(p, size, flags);
            }
            if (result == cudaSuccess)
                _hostAlloc.addBlock(*p, size, device);
        }
        return result;
    }

    struct BLOCK {
        void* ptr;
        size_t size;
        int device;
    };

    struct ARRAY {
        void* ptr;
        size_t width;
        size_t height;
        size_t depth;
        cudaChannelFormatDesc desc;
        int device;
    };

    class Pool {
    public:
        void addBlock(void* ptr, size_t size, int device)
        {
            std::unique_lock<std::mutex> lock(_guard);

            _inUse[ptr] = { ptr, size, device };
        }

        void* findBlock(size_t size, int device)
        {
            std::unique_lock<std::mutex> lock(_guard);

            for (auto i = _free.begin(); i < _free.end(); ++i) {
                if (i->size == size && i->device == device) {
                    void* p   = i->ptr;
                    _inUse[p] = *i;
                    _free.erase(i);
                    return p;
                }
            }
            return NULL;
        }

        bool releaseBlock(void* ptr)
        {
            std::unique_lock<std::mutex> lock(_guard);

            auto i = _inUse.find(ptr);

            if (i != _inUse.end()) {
                _free.push_back(i->second);
                _inUse.erase(i);
                return true;
            }
            return false;
        }

        void sweep()
        {
            std::unique_lock<std::mutex> lock(_guard);

            for (auto i = _free.begin(); i < _free.end(); ++i) {
                ::cudaFree(i->ptr);
            }
            _free.clear();
        }

    private:
        std::map<void*, BLOCK> _inUse;
        std::vector<BLOCK> _free;
        std::mutex _guard;
    };

    class ArrayPool {
    public:
        void addBlock(void* ptr, size_t width, size_t height, size_t depth,
                      const cudaChannelFormatDesc& desc, int device)
        {
            std::unique_lock<std::mutex> lock(_guard);

            _inUse[ptr] = { ptr, width, height, depth, desc, device };
        }

        void* findBlock(size_t width, size_t height, size_t depth,
                        const cudaChannelFormatDesc& desc, int device)
        {
            std::unique_lock<std::mutex> lock(_guard);

            for (auto i = _free.begin(); i < _free.end(); ++i) {
                if (i->width == width && i->height == height
                    && i->depth == depth && i->desc.x == desc.x
                    && i->desc.y == desc.y && i->desc.z == desc.z
                    && i->desc.w == desc.w && i->desc.f == desc.f
                    && i->device == device) {
                    void* p   = i->ptr;
                    _inUse[p] = *i;
                    _free.erase(i);
                    return p;
                }
            }
            return NULL;
        }

        bool releaseBlock(void* ptr)
        {
            std::unique_lock<std::mutex> lock(_guard);

            auto i = _inUse.find(ptr);

            if (i != _inUse.end()) {
                _free.push_back(i->second);

                _inUse.erase(i);

                return true;
            }
            return false;
        }

        void sweep()
        {
            std::unique_lock<std::mutex> lock(_guard);

            for (auto i = _free.begin(); i < _free.end(); ++i) {
                ::cudaFree(i->ptr);
            }
            _free.clear();
        }

    private:
        std::map<void*, ARRAY> _inUse;
        std::vector<ARRAY> _free;
        std::mutex _guard;
    };

    Pool _device;
    Pool _host;
    Pool _hostAlloc;
    ArrayPool _array;
};

std::mutex SimpleMemoryPool::guard;



namespace {
R3DSDK::DebayerCudaJob*
DebayerAllocate(const R3DSDK::AsyncDecompressJob* job,
                R3DSDK::ImageProcessingSettings* imageProcessingSettings,
                R3DSDK::VideoPixelType pixelType)
{
    //allocate the debayer job
    R3DSDK::DebayerCudaJob* data = RED_CUDA->createDebayerJob();

    data->raw_host_mem            = job->OutputBuffer;
    data->mode                    = job->Mode;
    data->imageProcessingSettings = imageProcessingSettings;
    data->pixelType               = pixelType;

    //create raw buffer on the CUDA device
    cudaError_t err = SimpleMemoryPool::cudaMalloc(&(data->raw_device_mem),
                                                   job->OutputBufferSize);

    if (err != cudaSuccess) {
        DBG("Failed to allocate raw frame on GPU: {}\n", static_cast<int>(err));
        RED_CUDA->releaseDebayerJob(data);
        return NULL;
    }

    data->output_device_mem_size = R3DSDK::DebayerCudaJob::ResultFrameSize(
        data);
    DBG("data->output_device_mem_size = {}\n", data->output_device_mem_size);

    //YOU MUST specify an existing buffer for the result image
    //Set DebayerCudaJob::output_device_mem_size >= result_buffer_size
    //and a pointer to the device buffer in DebayerCudaJob::output_device_mem
    err = SimpleMemoryPool::cudaMalloc(&(data->output_device_mem),
                                       data->output_device_mem_size);

    if (err != cudaSuccess) {
        DBG("Failed to allocate result frame on card {}\n",
            static_cast<int>(err));
        SimpleMemoryPool::cudaFree(data->raw_device_mem);
        RED_CUDA->releaseDebayerJob(data);
        return NULL;
    }

    return data;
}



void
DebayerFree(R3DSDK::DebayerCudaJob* job)
{
    SimpleMemoryPool::cudaFree(job->raw_device_mem);
    SimpleMemoryPool::cudaFree(job->output_device_mem);
    RED_CUDA->releaseDebayerJob(job);
}



template<typename T> class ConcurrentQueue {
private:
    std::mutex QUEUE_MUTEX;
    std::condition_variable QUEUE_CV;
    std::list<T*> QUEUE;

public:
    void push(T* job)
    {
        std::unique_lock<std::mutex> lck(QUEUE_MUTEX);
        QUEUE.push_back(job);
        QUEUE_CV.notify_all();
    }

    void pop(T*& job)
    {
        std::unique_lock<std::mutex> lck(QUEUE_MUTEX);

        while (QUEUE.size() == 0)
            QUEUE_CV.wait(lck);

        job = QUEUE.front();
        QUEUE.pop_front();
    }

    size_t size() const { return QUEUE.size(); }
};



void
CPU_callback(R3DSDK::AsyncDecompressJob* item,
             R3DSDK::DecodeStatus decodeStatus)
{
    // DBG("CPU_callback()\n");
    Strutil::print("CPU_callback()\n");
    decodeDone = true;
}



R3DSDK::REDCuda*
OpenCUDA(int& deviceId)
{
    //setup Cuda for the current thread
    cudaDeviceProp deviceProp;
    cudaError_t err = cudaChooseDevice(&deviceId, &deviceProp);
    if (err != cudaSuccess) {
        DBG("Failed to move raw frame to card {}\n", static_cast<int>(err));
        return NULL;
    }

    err = cudaSetDevice(deviceId);
    if (err != cudaSuccess) {
        DBG("Failed to move raw frame to card {}\n", static_cast<int>(err));
        return NULL;
    }

    //SETUP YOUR CUDA API FUNCTION POINTERS
    R3DSDK::EXT_CUDA_API api;
    api.cudaFree                 = SimpleMemoryPool::cudaFree;
    api.cudaFreeArray            = SimpleMemoryPool::cudaFreeArray;
    api.cudaFreeHost             = SimpleMemoryPool::cudaFreeHost;
    api.cudaFreeMipmappedArray   = ::cudaFreeMipmappedArray;
    api.cudaHostAlloc            = SimpleMemoryPool::cudaHostAlloc;
    api.cudaMalloc               = SimpleMemoryPool::cudaMalloc;
    api.cudaMalloc3D             = ::cudaMalloc3D;
    api.cudaMalloc3DArray        = SimpleMemoryPool::cudaMalloc3DArray;
    api.cudaMallocArray          = SimpleMemoryPool::cudaMallocArray;
    api.cudaMallocHost           = SimpleMemoryPool::cudaMallocHost;
    api.cudaMallocMipmappedArray = ::cudaMallocMipmappedArray;
    api.cudaMallocPitch          = ::cudaMallocPitch;


    //CREATE THE REDCuda CLASS
    return new R3DSDK::REDCuda(api);
}
}  //end anonymous namespace

#endif  // CUDA



void
R3dInput::initialize()
{
    DBG("R3dInput::initialize()\n");

    std::string library_path
        = Sysutil::getenv("OIIO_R3D_LIBRARY_PATH",
#if defined(__linux__)
                          "/opt/R3DSDKv9_0_0-BETA1/Redistributable/linux"
#elif defined(__APPLE__)
                          "/Library/R3DSDKv9_0_0-BETA1/Redistributable/mac"
#elif defined(__WINDOWS__)
                          "C:\\R3DSDKv9_0_0-BETA1\\Redistributable\\win"
#else
#    error "Unknown OS"
#endif
        );
    // initialize SDK

    unsigned int optional_components =
#ifdef CUDA
        OPTION_RED_CUDA;
#elif defined(OpenCL)
        OPTION_RED_OPENCL;
#elif defined(Metal)
        OPTION_RED_METAL;
#else
        OPTION_RED_NONE;
#endif

    R3DSDK::InitializeStatus init_status
        = R3DSDK::InitializeSdk(library_path.c_str(), optional_components);
    if (init_status != R3DSDK::ISInitializeOK) {
        R3DSDK::FinalizeSdk();
        DBG("Failed to load R3DSDK Library\n");
        return;
    }

    DBG("SDK VERSION: {}\n", R3DSDK::GetSdkVersion());
#ifdef CUDA
    // open CUDA device
    RED_CUDA = OpenCUDA(CUDA_DEVICE_ID);

    if (RED_CUDA == NULL) {
        R3DSDK::FinalizeSdk();
        DBG("Failed to initialize CUDA\n");
    }

    m_gpu = true;
#endif  // CUDA
}



bool
R3dInput::open(const std::string& name, ImageSpec& newspec,
               const ImageSpec& config)
{
    DBG("R3dInput::open(name, newspec, config)\n");

    ioproxy_retrieve_from_config(config);
    m_config.reset(new ImageSpec(config));  // save config spec

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

    int hint = 0;
    if (m_config) {
        hint = m_config->get_int_attribute("oiio:hint");
        DBG("hint = {}\n", hint);
    }

    R3DSDK::VideoDecodeMode mode = R3DSDK::DECODE_FULL_RES_PREMIUM;
    int scale                    = 1;

    switch (hint) {
    case 0:
        mode  = R3DSDK::DECODE_FULL_RES_PREMIUM;
        scale = 1;
        break;
    case 1:
        mode  = R3DSDK::DECODE_HALF_RES_GOOD;
        scale = 2;
        break;
    case 2:
        mode  = R3DSDK::DECODE_QUARTER_RES_GOOD;
        scale = 4;
        break;
    case 3:
        mode  = R3DSDK::DECODE_EIGHT_RES_GOOD;
        scale = 8;
        break;
    case 4:
        mode  = R3DSDK::DECODE_SIXTEENTH_RES_GOOD;
        scale = 16;
        break;
    }

    // calculate how much ouput memory we're going to need
    size_t width  = m_clip->Width() / scale;
    size_t height = m_clip->Height() / scale;

    DBG("{}×{}\n", width, height);

    m_channels = 3;

    DBG("Video frame count {}\n", m_clip->VideoFrameCount());

    m_frames     = m_clip->VideoFrameCount();
    m_nsubimages = m_frames;

    DBG("Video framerate {}\n", m_clip->VideoAudioFramerate());

    m_fps = m_clip->VideoAudioFramerate();

    DBG("File list count {}\n", m_clip->FileListCount());

    // three channels (RGB) in 16-bit (2 bytes) requires this much memory:
    size_t memNeeded = m_channels * width * height * sizeof(uint16_t);

    // alloc this memory 16-byte aligned
    m_image_buffer = static_cast<unsigned char*>(aligned_malloc(memNeeded, 16));

    if (m_image_buffer == nullptr) {
        DBG("Failed to allocate {} bytes of memory for output image\n",
            static_cast<int>(memNeeded));

        return false;
    }

#ifdef GPU
    if (m_gpu) {
        // open GPU decoder
        GPU_DECODER = new R3DSDK::GpuDecoder();
        GPU_DECODER->Open();

        m_supported = GPU_DECODER->DecodeSupportedForClip(*m_clip);
    }

    if (m_supported == R3DSDK::DSDecodeOK) {
        m_async_decompress_job.Clip = m_clip;

        m_async_decompress_job.Mode = mode;

        // letting the decoder know how big the buffer is
        m_async_decompress_job.OutputBufferSize
            = R3DSDK::AsyncDecoder::GetSizeBufferNeeded(m_async_decompress_job);

        DBG("OutputBufferSize = {}\n", m_async_decompress_job.OutputBufferSize);

        m_async_decompress_job.OutputBuffer = static_cast<unsigned char*>(
            aligned_malloc(m_async_decompress_job.OutputBufferSize, 16));

        // Interleaved RGB decoding in 16-bits per pixel
        // m_async_decompress_job.PixelType
        //     = R3DSDK::PixelType_16Bit_RGB_Interleaved;
        // m_async_decompress_job.BytesPerRow = m_channels * width
        //                                      * sizeof(uint16_t);

        // m_async_decompress_job.ImageProcessing = NULL;
        // m_async_decompress_job.HdrProcessing   = NULL;
        // m_async_decompress_job.Callback = CPU_callback;
    } else
#endif  // GPU
    {
        // letting the decoder know how big the buffer is
        m_job.OutputBufferSize = memNeeded;

        m_job.Mode = mode;

        // store the image here
        m_job.OutputBuffer = m_image_buffer;

        // Interleaved RGB decoding in 16-bits per pixel
        m_job.PixelType   = R3DSDK::PixelType_16Bit_RGB_Interleaved;
        m_job.BytesPerRow = m_channels * width * sizeof(uint16_t);

        m_job.ImageProcessing = NULL;
        m_job.HdrProcessing   = NULL;
    }

    m_spec = ImageSpec(width, height, m_channels, TypeDesc::UINT16);

    int frame_rate_numerator = m_clip->MetadataItemAsInt(
        R3DSDK::RMD_FRAMERATE_NUMERATOR);
    int frame_rate_denominator = m_clip->MetadataItemAsInt(
        R3DSDK::RMD_FRAMERATE_DENOMINATOR);
    int frame_rate[2] = { frame_rate_numerator, frame_rate_denominator };

    bool record_frame_rate_exists = m_clip->MetadataExists(
        R3DSDK::RMD_RECORD_FRAMERATE_NUMERATOR);
    if (record_frame_rate_exists) {
        int record_frame_rate_numerator = m_clip->MetadataItemAsInt(
            R3DSDK::RMD_RECORD_FRAMERATE_NUMERATOR);
        int record_frame_rate_denominator = m_clip->MetadataItemAsInt(
            R3DSDK::RMD_RECORD_FRAMERATE_DENOMINATOR);
        int record_frame_rate[2] = { record_frame_rate_numerator,
                                     record_frame_rate_denominator };
        m_spec.attribute("FramesPerSecond", TypeRational, &record_frame_rate);
    } else {
        m_spec.attribute("FramesPerSecond", TypeRational, &frame_rate);
    }

    m_spec.attribute("oiio:Movie", true);
    m_spec.attribute("oiio:subimages", int(m_frames));
    m_spec.attribute("oiio:BitsPerSample", 16);
#ifdef GPU
    m_spec.attribute("oiio:GPU", m_gpu);
#endif  // GPU

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

#ifdef GPU
    if (m_gpu && (m_supported == R3DSDK::DSDecodeOK)) {
        m_async_decompress_job.VideoFrameNo = pos;
        m_async_decompress_job.VideoTrackNo = 0;
        m_async_decompress_job.Callback     = CPU_callback;

        decodeDone = false;

        int device = CUDA_DEVICE_ID;
        cudaStream_t stream;
        cudaError_t err;

        err = cudaStreamCreate(&stream);
        if (err != cudaSuccess) {
            DBG("Failed to create stream {}\n", static_cast<int>(err));
            return;
        }

        R3DSDK::DecodeStatus decode_status = GPU_DECODER->DecodeForGpuSdk(
            m_async_decompress_job);
        if (decode_status != R3DSDK::DSDecodeOK) {
            DBG("Failed to decode frame {} with status {}\n", pos,
                static_cast<int>(decode_status));
            cudaStreamDestroy(stream);
            return;
        }

        while (!decodeDone) {
            usleep(1000);
        }

        R3DSDK::ImageProcessingSettings* ips
            = new R3DSDK::ImageProcessingSettings();
        m_async_decompress_job.Clip->GetDefaultImageProcessingSettings(*ips);

        const R3DSDK::VideoPixelType pixelType
            = R3DSDK::PixelType_16Bit_RGB_Interleaved;

        R3DSDK::DebayerCudaJob* debayer_cuda_job
            = DebayerAllocate(&m_async_decompress_job, ips, pixelType);
        if (debayer_cuda_job == nullptr) {
            delete ips;
            cudaStreamDestroy(stream);
            return;
        }

        m_async_decompress_job.PrivateData = debayer_cuda_job;

        DBG("debayer_cuda_job = {}\n", static_cast<void*>(debayer_cuda_job));
        DBG("  raw_host_mem = {}\n",
            static_cast<void*>(debayer_cuda_job->raw_host_mem));
        DBG("  raw_device_mem = {}\n",
            static_cast<void*>(debayer_cuda_job->raw_device_mem));
        DBG("  output_device_mem_size = {}\n",
            debayer_cuda_job->output_device_mem_size);
        DBG("  output_device_mem = {}\n", debayer_cuda_job->output_device_mem);
        DBG("  mode = {}\n", static_cast<uint32_t>(debayer_cuda_job->mode));
        DBG("  pixelType = {}\n",
            static_cast<uint32_t>(debayer_cuda_job->pixelType));

        R3DSDK::REDCuda::Status status
            = RED_CUDA->processAsync(device, stream, debayer_cuda_job, err);

        if (status != R3DSDK::REDCuda::Status_Ok) {
            DBG("Failed to process frame, error {}\n",
                static_cast<int>(status));
            delete debayer_cuda_job->imageProcessingSettings;
            debayer_cuda_job->imageProcessingSettings = NULL;
            DebayerFree(debayer_cuda_job);

            cudaStreamDestroy(stream);
            return;
        }

        debayer_cuda_job->completeAsync();

        size_t result_buffer_size = R3DSDK::DebayerCudaJob::ResultFrameSize(
            debayer_cuda_job);

        DBG("result_buffer_size = {}\n", result_buffer_size);

        //allocate the result buffer in host memory.
        if (result_buffer_size != debayer_cuda_job->output_device_mem_size) {
            DBG("Result buffer size does not match expected size: Expected: {} Actual: {}\n",
                result_buffer_size, debayer_cuda_job->output_device_mem_size);
        }

        if (m_image_buffer != nullptr) {
            //read the GPU buffer back to the host memory result buffer. - Note this is not always the optimal way to read back. (Use pinned memory in a real app)
            cudaError_t err = cudaMemcpy(m_image_buffer,
                                         debayer_cuda_job->output_device_mem,
                                         result_buffer_size,
                                         cudaMemcpyDeviceToHost);
            if (err != cudaSuccess) {
                DBG("Failed to read result frame from card {}\n",
                    static_cast<int>(err));
            } else {
                //ensure the read is complete.
                err = cudaDeviceSynchronize();
                if (err != cudaSuccess) {
                    DBG("Failed to finish after reading result frame from card {}\n",
                        static_cast<int>(err));
                }
            }
        }

        delete debayer_cuda_job->imageProcessingSettings;
        debayer_cuda_job->imageProcessingSettings = NULL;
        DebayerFree(debayer_cuda_job);

        cudaStreamDestroy(stream);
    } else
#endif  // GPU
    {
        R3DSDK::DecodeStatus decode_status = m_clip->DecodeVideoFrame(pos,
                                                                      m_job);
        if (decode_status != R3DSDK::DSDecodeOK) {
            DBG("Failed to decode frame {}\n", pos);
        }
    }

    m_last_search_pos  = pos;
    m_last_decoded_pos = pos;
    m_read_frame       = true;
    m_next_scanline    = 0;
}



bool
R3dInput::seek_subimage(int subimage, int miplevel)
{
    // DBG("R3dInput::seek_subimage({}, {})\n", subimage, miplevel);

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
    // DBG("R3dInput::read_native_scanline({}, {}, {})\n", subimage, miplevel, y);

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
    lock_guard lock(*this);
    DBG("R3dInput::close()\n");
    DBG("m_filename = {}\n", m_filename);

    if (m_clip) {
        // delete m_clip;
        m_clip = nullptr;
    }

    if (m_image_buffer) {
        aligned_free(m_image_buffer);
        m_image_buffer = nullptr;
    }

    if (m_gpu) {
        if (GPU_DECODER) {
            GPU_DECODER->Close();
            delete GPU_DECODER;
            GPU_DECODER = nullptr;
        }
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
