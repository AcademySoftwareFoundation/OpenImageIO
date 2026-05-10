// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "arena.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace texture_device {

uint64_t g_tagged_ptr_context = ptrtag("Host");

namespace {

    template<class AllocationMap>
    void report_leaks(const char* owner, const AllocationMap& allocated)
    {
        if (allocated.empty())
            return;

        std::fprintf(stderr,
                     "texture-device: %s leak check failed (%zu allocations)\n",
                     owner, allocated.size());
        for (const auto& it : allocated) {
            const auto& rec = it.second;
            std::fprintf(stderr, "  leak ptr=%p bytes=%zu purpose=%s\n",
                         it.first, rec.bytes,
                         rec.purpose ? rec.purpose : "(null purpose)");
        }
        std::abort();
    }

    template<class AllocationMap>
    void tracked_free(AllocationMap& allocated, tagged_ptr<void> p,
                      const char* owner)
    {
        if (!p)
            return;

        auto it = allocated.find(p.get());
        if (it == allocated.end()) {
            std::fprintf(
                stderr,
                "texture-device: invalid free ptr=%p (not allocated by %s::alloc)\n",
                p.get(), owner);
            std::abort();
        }
        allocated.erase(it);
        std::free(p.get());
    }

}  // namespace

Host::~Host() { report_leaks("Host", m_allocated); }

tagged_ptr<void>
Host::alloc(size_t bytes, const char* purpose)
{
    void* p = std::malloc(bytes);
    if (!p)
        return nullptr;

    m_allocated[p] = AllocationRecord { bytes, purpose };
    return { p, "Host" };
}

void
Host::free(tagged_ptr<void> p)
{
    tracked_free(m_allocated, p, "Host");
}

void
Host::copy_to(tagged_ptr<void> device, tagged_ptr<const void> host,
              size_t bytes)
{
    if (!device || !host)
        return;

    // copy_to expects a device destination and host source.
    if (!device.is("MockDevice") || host.is("MockDevice"))
        std::abort();

    std::memcpy(device.get(), host.get(), bytes);
}

void
Host::copy_from(tagged_ptr<void> host, tagged_ptr<const void> device,
                size_t bytes)
{
    if (!host || !device)
        return;

    // copy_from expects a host destination and device source.
    if (host.is("MockDevice") || !device.is("MockDevice"))
        std::abort();

    std::memcpy(host.get(), device.get(), bytes);
}

void
Host::copy_in(tagged_ptr<void> to, tagged_ptr<const void> from, size_t bytes)
{
    if (!to || !from)
        return;

    // copy_in expects both pointers to belong to Host.
    if (!to.is("Host") || !from.is("Host"))
        std::abort();

    std::memcpy(to.get(), from.get(), bytes);
}

MockDevice::~MockDevice() { report_leaks("MockDevice", m_allocated); }

tagged_ptr<void>
MockDevice::alloc(size_t bytes, const char* purpose)
{
    void* p = std::malloc(bytes);
    if (!p)
        return nullptr;

    m_allocated[p] = AllocationRecord { bytes, purpose };
    return { p, "MockDevice" };
}

void
MockDevice::free(tagged_ptr<void> p)
{
    tracked_free(m_allocated, p, "MockDevice");
}

void
MockDevice::copy_to(tagged_ptr<void> device, tagged_ptr<const void> host,
                    size_t bytes)
{
    if (!device || !host)
        return;

    // copy_to expects a device destination and host source.
    if (!device.is("MockDevice") || host.is("MockDevice"))
        std::abort();

    std::memcpy(device.get(), host.get(), bytes);
}

void
MockDevice::copy_from(tagged_ptr<void> host, tagged_ptr<const void> device,
                      size_t bytes)
{
    if (!host || !device)
        return;

    // copy_from expects a host destination and device source.
    if (host.is("MockDevice") || !device.is("MockDevice"))
        std::abort();

    std::memcpy(host.get(), device.get(), bytes);
}

void
MockDevice::copy_in(tagged_ptr<void> to, tagged_ptr<const void> from,
                    size_t bytes)
{
    if (!to || !from)
        return;

    // copy_in expects both pointers to belong to MockDevice.
    if (!to.is("MockDevice") || !from.is("MockDevice"))
        std::abort();

    std::memcpy(to.get(), from.get(), bytes);
}

void
MockDevice::run(int width, int height, Kernel kernel, tagged_ptr<void> data)
{
    struct MockDeviceExecutionGuard {
        MockDeviceExecutionGuard()
        {
            g_tagged_ptr_context = ptrtag("MockDevice");
        }
        ~MockDeviceExecutionGuard() { g_tagged_ptr_context = ptrtag("Host"); }
    } guard;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x)
            kernel(x, y, data);
    }
}

#if TEXTURE_DEVICE_HAS_CUDA_RUNTIME

CudaArena::~CudaArena()
{
#    if TEXTURE_DEVICE_ENABLE_CUDA_SKETCH_IMPL
    // FIXME: this frees tracked allocations but does not clear m_allocated,
    // so report_leaks below will still abort if entries remain.
    for (const auto& it : m_allocated) {
        cudaFree(it.first);
    }
#    endif
    report_leaks("CudaArena", m_allocated);
}

tagged_ptr<void>
CudaArena::alloc(size_t bytes, const char* purpose)
{
#    if TEXTURE_DEVICE_ENABLE_CUDA_SKETCH_IMPL
    void* p         = nullptr;
    cudaError_t err = cudaMalloc(&p, bytes);
    if (err != cudaSuccess)
        return nullptr;
    m_allocated[p] = AllocationRecord { bytes, purpose };
    return { p, "CudaDevice" };
#    else
    (void)bytes;
    (void)purpose;
    return nullptr;
#    endif
}

void
CudaArena::free(tagged_ptr<void> p)
{
#    if TEXTURE_DEVICE_ENABLE_CUDA_SKETCH_IMPL
    if (!p)
        return;
    auto it = m_allocated.find(p.get());
    if (it == m_allocated.end()) {
        std::fprintf(
            stderr,
            "texture-device: invalid free ptr=%p (not allocated by CudaArena::alloc)\n",
            p.get());
        std::abort();
    }
    // FIXME: check cudaFree return and surface failures.
    cudaFree(p.get());
    m_allocated.erase(it);
#    else
    (void)p;
#    endif
}

void
CudaArena::copy_to(tagged_ptr<void> device, tagged_ptr<const void> host,
                   size_t bytes)
{
#    if TEXTURE_DEVICE_ENABLE_CUDA_SKETCH_IMPL
    if (!device || !host)
        return;
    // FIXME: check cudaMemcpyAsync return and decide synchronization policy.
    cudaMemcpyAsync(device.get(), host.get(), bytes, cudaMemcpyHostToDevice,
                    m_stream);
#    else
    (void)device;
    (void)host;
    (void)bytes;
#    endif
}

void
CudaArena::copy_from(tagged_ptr<void> host, tagged_ptr<const void> device,
                     size_t bytes)
{
#    if TEXTURE_DEVICE_ENABLE_CUDA_SKETCH_IMPL
    if (!host || !device)
        return;
    // FIXME: check cudaMemcpyAsync return and decide synchronization policy.
    cudaMemcpyAsync(host.get(), device.get(), bytes, cudaMemcpyDeviceToHost,
                    m_stream);
#    else
    (void)host;
    (void)device;
    (void)bytes;
#    endif
}

void
CudaArena::copy_in(tagged_ptr<void> to, tagged_ptr<const void> from,
                   size_t bytes)
{
#    if TEXTURE_DEVICE_ENABLE_CUDA_SKETCH_IMPL
    if (!to || !from)
        return;
    // FIXME: check cudaMemcpyAsync return and decide synchronization policy.
    cudaMemcpyAsync(to.get(), from.get(), bytes, cudaMemcpyDeviceToDevice,
                    m_stream);
#    else
    (void)to;
    (void)from;
    (void)bytes;
#    endif
}

void
CudaArena::run(int width, int height, Kernel kernel, tagged_ptr<void> data)
{
#    if TEXTURE_DEVICE_ENABLE_CUDA_SKETCH_IMPL
    // The current Kernel signature is host-callable. A real CUDA path needs a
    // separate __global__ entrypoint and launch configuration.
#    endif
    (void)width;
    (void)height;
    (void)kernel;
    (void)data;
}

#endif

}  // namespace texture_device
