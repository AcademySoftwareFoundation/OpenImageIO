/*
  Copyright 2018 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/

#include <cstdlib>
#include <cstdio>
#include <mutex>

#ifdef OIIO_USE_CUDA
// #include <cuda.h>
#include <cuda_runtime.h>
#endif

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/thread.h>
#include "imageio_pvt.h"


OIIO_NAMESPACE_BEGIN

// Global private data
namespace pvt {

spin_mutex cuda_mutex;
bool cuda_supported = false;
std::string cuda_device_name;
int cuda_driver_version = 0;
int cuda_runtime_version = 0;
int cuda_compatibility = 0;
size_t cuda_total_memory = 0;



#ifdef OIIO_USE_CUDA

// This will output the proper CUDA error strings in the event that a
// CUDA host call returns an error
#define checkCudaErrors(err)  __checkCudaErrors (err, __FILE__, __LINE__)

inline bool __checkCudaErrors(cudaError_t err, const char *file, const int line)
{
    if (cudaSuccess != err) {
        Strutil::fprintf (stderr, "Cuda error %d (%s) at %s:%d\n",
                          (int)err, cudaGetErrorString(err), file, line);
    }
    return true;
    return (err == cudaSuccess);
}



static void
initialize_cuda ()
{
    // Environment OPENIMAGEIO_CUDA=0 trumps everything else, turns off
    // Cuda functionality.
    const char *env = getenv ("OPENIMAGEIO_CUDA");
    if (env && strtol(env,NULL,10) == 0)
        return;

    // if (! checkCudaErrors (cuInit (0)))
    //     return;

    // Get number of devices supporting CUDA
    int deviceCount = 0;
    if (! checkCudaErrors (cudaGetDeviceCount(&deviceCount))) {
        return;
    }

    OIIO::debug ("Number of Cuda devices: %d\n", deviceCount);
#if 0
    for (int dev = 0; dev < deviceCount; ++dev) {
        CUdevice device;
        cudaGetDevice (&device, dev);
        cudaSetDevice(dev);
        cudaDeviceProp deviceProp;
        cudaGetDeviceProperties(&deviceProp, dev);
        cuda_device_name = deviceProp.name;
        cuDriverGetVersion (&cuda_driver_version);
        cudaRuntimeGetVersion (&cuda_runtime_version);
        cuda_compatibility = 100 * deviceProp.major + deviceProp.minor;
        cuda_total_memory = deviceProp.totalGlobalMem;
        OIIO::debug ("Cuda device \"%s\": driver %s, runtime %s, Cuda compat %s\n",
                     cuda_device_name, cuda_driver_version,
                     cuda_runtime_version, cuda_compatibility);
        OIIO::debug (" total mem %g MB\n", cuda_total_memory/(1024.0*1024.0));
        break;  // only inventory the first Cuda device. FIXME? 
    }
#endif
    cuda_supported = true;
}

#endif /* defined(OIIO_USE_CUDA) */



bool
openimageio_cuda ()
{
    if (! use_cuda)
        return false;
#ifdef OIIO_USE_CUDA
    static std::once_flag cuda_initialized;
    std::call_once (cuda_initialized, initialize_cuda);
#endif
    return cuda_supported;
}


struct cuda_force_initializer {
    cuda_force_initializer() { (void) openimageio_cuda(); }
};
cuda_force_initializer init;



void* cuda_malloc (size_t size)
{
#ifdef OIIO_USE_CUDA
    if (use_cuda) {
        char *cudaptr = nullptr;
        checkCudaErrors (cudaMallocManaged (&cudaptr, size));
        cudaDeviceSynchronize();
        return cudaptr;
    }
#endif
    return malloc (size);
}



void cuda_free (void *mem)
{
#ifdef OIIO_USE_CUDA
    if (use_cuda) {
        cudaDeviceSynchronize();
        checkCudaErrors (cudaFree (mem));
        return;
    }
#endif
    return free (mem);
}


} // end namespace pvt

OIIO_NAMESPACE_END
