// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <cstdlib>
#include <mutex>

#ifdef OIIO_USE_CUDA
#    include <cuda.h>
#    include <cuda_runtime.h>
#else
#    define CUDA_VERSION 0
#endif

#include "imageio_pvt.h"
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/thread.h>


OIIO_NAMESPACE_BEGIN

// Global private data
namespace pvt {

static ComputeDevice oiio_compute_device(ComputeDevice::CPU);


ComputeDevice
compute_device()
{
    return oiio_compute_device;
}


// These MUST match the order of enum ComputeDevice
static const char* device_type_names[] = { "CPU", "CUDA" };



std::mutex compute_mutex;

// Cuda specific things
// Was Cuda support enabled at build time?
[[maybe_unused]] constexpr bool cuda_build_time_enabled = bool(CUDA_VERSION);

constexpr int cuda_build_version
    = (10000 * (CUDA_VERSION / 1000)         // major
       + 100 * ((CUDA_VERSION % 1000) / 10)  // minor
       + (CUDA_VERSION % 10));               // patch

bool cuda_supported = false;  // CUDA present at runtime and initialized
ustring cuda_device_name;
int cuda_driver_version  = 0;
int cuda_runtime_version = 0;
int cuda_compatibility   = 0;
size_t cuda_total_memory = 0;



#ifdef OIIO_USE_CUDA

thread_local std::string saved_cuda_error_message;

inline std::string
cuda_geterror()
{
    std::string e;
    std::swap(e, saved_cuda_error_message);
    return e;
}

static CUstream cuda_stream;


// This will call a Cuda function and output the proper CUDA error strings in
// the event that a CUDA host call returns an error.
#    define checkCudaErrors(call) \
        __checkCudaErrors(call, #call, __FILE__, __LINE__)
#    define CUDA_CHECK(call) __checkCudaErrors(call, #call, __FILE__, __LINE__)

static bool
__checkCudaErrors(cudaError_t err, const char* call, const char* file,
                  const int line)
{
    if (err != cudaSuccess) {
        saved_cuda_error_message += Strutil::fmt::format(
            "CUDA runtime API error {}: {} ({} @ {}:{})\n", (int)err,
            cudaGetErrorString(err), call, file, line);
    }
    return (err == cudaSuccess);
}



static void
initialize_cuda()
{
    std::lock_guard lock(compute_mutex);

    // Environment OPENIMAGEIO_CUDA=0 trumps everything else, turns off
    // Cuda functionality. We don't even initialize in this case.
    std::string env = Sysutil::getenv("OPENIMAGEIO_CUDA");
    if (env.size() && Strutil::eval_as_bool(env) == false) {
        OIIO::debugfmt("CUDA disabled by $OPENIMAGEIO_CUDA\n");
        return;
    }

    // Get number of devices supporting CUDA
    int deviceCount = 0;
    if (!checkCudaErrors(cudaGetDeviceCount(&deviceCount))) {
        return;
    }

    // Initialize CUDA
    if (!CUDA_CHECK(cudaFree(0))) {
        cuda_geterror();  // clear the error
        return;
    }

    CUDA_CHECK(cudaSetDevice(0));
    CUDA_CHECK(cudaStreamCreate(&cuda_stream));

    OIIO::debugfmt("Number of CUDA devices: {}\n", deviceCount);
    for (int dev = 0; dev < deviceCount; ++dev) {
        cudaDeviceProp deviceProp;
        CUDA_CHECK(cudaGetDeviceProperties(&deviceProp, dev));
        CUDA_CHECK(cudaDriverGetVersion(&cuda_driver_version));
        CUDA_CHECK(cudaRuntimeGetVersion(&cuda_runtime_version));
        cuda_device_name   = ustring(deviceProp.name);
        cuda_compatibility = 100 * deviceProp.major + deviceProp.minor;
        cuda_total_memory  = deviceProp.totalGlobalMem;
        OIIO::debugfmt(
            "CUDA device \"{}\": driver {}, runtime {}, Cuda compat {}\n",
            deviceProp.name, cuda_driver_version, cuda_runtime_version,
            cuda_compatibility);
        OIIO::debugfmt(" total mem {:g} MB\n",
                       cuda_total_memory / (1024.0 * 1024.0));
        break;  // only inventory the first Cuda device. FIXME?
    }
    cuda_supported = true;
}

#endif /* defined(OIIO_USE_CUDA) */



/// Initialize CUDA if it has not already been initialized. Return true if
/// CUDA facilities are available.
bool
enable_cuda()
{
#ifdef OIIO_USE_CUDA
    static std::once_flag cuda_initialized;
    std::call_once(cuda_initialized, initialize_cuda);
#endif
    return cuda_supported;
}



// Trick to fire up a CUDA device at static initialization time.
struct cuda_force_initializer {
    cuda_force_initializer() { (void)enable_cuda(); }
};
cuda_force_initializer init;



void*
device_malloc(size_t size)
{
#ifdef OIIO_USE_CUDA
    if (oiio_compute_device == ComputeDevice::CUDA) {
        char* cudaptr = nullptr;
        checkCudaErrors(cudaMalloc(&cudaptr, size));
        return cudaptr;
    }
#endif
    return malloc(size);
}



void*
device_unified_malloc(size_t size)
{
#ifdef OIIO_USE_CUDA
    if (oiio_compute_device == ComputeDevice::CUDA) {
        char* cudaptr = nullptr;
        checkCudaErrors(cudaMallocManaged(&cudaptr, size));
        return cudaptr;
    }
#endif
    return malloc(size);
}



void
device_free(void* mem)
{
#ifdef OIIO_USE_CUDA
    if (oiio_compute_device == ComputeDevice::CUDA) {
        // cudaDeviceSynchronize();
        checkCudaErrors(cudaFree(mem));
        return;
    }
#endif
    return free(mem);
}


bool
gpu_attribute(string_view name, TypeDesc type, const void* val)
{
    if (name == "gpu:device" && type == TypeString) {
        // If requesting a device by name, find the index of the name in the
        // list of device names and then request the device by index.
        const char* request = (*(const char**)val);
        int i               = 0;
        for (auto& n : device_type_names) {
            if (Strutil::iequals(request, n))
                return gpu_attribute("gpu:device", TypeInt, &i);
            ++i;
        }
        return false;
    }
    if (name == "gpu:device" && type == TypeInt) {
        ComputeDevice request = ComputeDevice(*(const int*)val);
        if (request == oiio_compute_device)
            return true;  // Already using the requested device
        if (request == ComputeDevice::CUDA) {
            if (enable_cuda()) {
                oiio_compute_device = request;
                return true;
            }
        }
        return false;  // Unsatisfiable request
    }

    // Below here needs mutual exclusion
    std::lock_guard lock(compute_mutex);

    return false;
}



bool
gpu_getattribute(string_view name, TypeDesc type, void* val)
{
    if (name == "gpu:device" && type == TypeInt) {
        *(int*)val = int(oiio_compute_device);
        return true;
    }
    if (name == "cuda:build_version" && type == TypeInt) {
        // Return encoded CUDA version as 10000*MAJOR + 100*MINOR + PATCH for
        // the version of CUDA that we compiled against.
        *(int*)val = cuda_build_version;
        return true;
    }
    if (name == "cuda:driver_version" && type == TypeInt) {
        *(int*)val = cuda_driver_version;
        return true;
    }
    if (name == "cuda:runtime_version" && type == TypeInt) {
        *(int*)val = cuda_runtime_version;
        return true;
    }
    if (name == "cuda:compatibility" && type == TypeInt) {
        *(int*)val = cuda_compatibility;
        return true;
    }
    if (name == "cuda:total_memory_MB" && type == TypeInt) {
        *(int*)val = int(cuda_total_memory >> 20);
        return true;
    }
    if (name == "cuda:device_name" && type == TypeString) {
        *(ustring*)val = cuda_device_name;
        return true;
    }
    if (name == "cuda:devices_found" && type == TypeInt) {
        *(int*)val = int(cuda_supported);
        return true;
    }

    // Below here needs mutual exclusion for safety
    std::lock_guard lock(compute_mutex);

    return false;
}


}  // end namespace pvt

OIIO_NAMESPACE_END
