// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "arena.h"
#include "tagged_ptr.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace texture_device {

uint64_t NullArena::g_arena_context = Host::run_tag();
bool NullArena::use_unified_memory  = false;

uint64_t
arena_context()
{
    return NullArena::g_arena_context;
}

namespace {

    template<class AllocationMap>
    void report_leaks(const char* owner, const AllocationMap& allocated)
    {
        if (allocated.empty())
            return;

        OIIO::print(stderr,
                    "texture-device: {} leak check failed ({} allocations)\n",
                    owner, allocated.size());
        for (const auto& it : allocated) {
            const auto& rec = it.second;
            OIIO::print(stderr, "  leak ptr={:p} bytes={} purpose={}\n",
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
            OIIO::print(
                stderr,
                "texture-device: invalid free ptr={:p} (not allocated by {}::alloc)\n",
                p.get(), owner);
            std::abort();
        }
        allocated.erase(it);
    }

    inline bool is_host_memory(tagged_ptr<const void> p)
    {
        return p.tag() == Host::run_tag() || p.tag() == unified_ptr_tag();
    }

    inline bool is_host_memory(tagged_ptr<void> p)
    {
        return p.tag() == Host::run_tag() || p.tag() == unified_ptr_tag();
    }

    inline bool is_mock_device_memory(tagged_ptr<const void> p)
    {
        return p.tag() == MockDevice::run_tag() || p.tag() == unified_ptr_tag();
    }

    inline bool is_mock_device_memory(tagged_ptr<void> p)
    {
        return p.tag() == MockDevice::run_tag() || p.tag() == unified_ptr_tag();
    }

}  // namespace

Host::~Host() { report_leaks("Host", m_allocated); }

tagged_ptr<void>
Host::alloc(tagged_ptr<void> mirror, size_t bytes, const char* purpose)
{
    // Unified policy: if the managed arena already allocated a shared block,
    // Host reuses that pointer instead of allocating a second copy.
    if (use_unified_memory && mirror)
        return mirror;

    void* p = std::malloc(bytes);
    if (!p)
        return nullptr;

    m_allocated[p] = AllocationRecord { bytes, purpose };
    return { p, mem_tag() };
}

void
Host::free(tagged_ptr<void> p)
{
    if (use_unified_memory && p.tag() == unified_ptr_tag()) {
        // In unified mode, mirrored pointers are owned by another arena.
        if (m_allocated.find(p.get()) == m_allocated.end())
            return;
    }

    tracked_free(m_allocated, p, "Host");
    std::free(p.get());
}

void
Host::copy_to(tagged_ptr<void> device, tagged_ptr<const void> host,
              size_t bytes)
{
    if (!device || !host || device == host)
        return;

    // copy_to expects a device destination and host source.
    if (!is_mock_device_memory(device) || !is_host_memory(host))
        std::abort();

    std::memcpy(device.get(), host.get(), bytes);
}

void
Host::copy_from(tagged_ptr<void> host, tagged_ptr<const void> device,
                size_t bytes)
{
    if (!host || !device || host == device)
        return;

    // copy_from expects a host destination and device source.
    if (!is_host_memory(host) || !is_mock_device_memory(device))
        std::abort();

    std::memcpy(host.get(), device.get(), bytes);
}

void
Host::copy_in(tagged_ptr<void> to, tagged_ptr<const void> from, size_t bytes)
{
    if (!to || !from || to == from)
        return;

    // copy_in expects both pointers to belong to Host.
    if (!to.is(mem_tag()) || !from.is(mem_tag()))
        std::abort();

    std::memcpy(to.get(), from.get(), bytes);
}

MockDevice::~MockDevice() { report_leaks("MockDevice", m_allocated); }

tagged_ptr<void>
MockDevice::alloc(size_t bytes, const char* purpose)
{
    // In this variant the device arena owns allocation first. With unified
    // memory enabled this becomes a shared pointer tagged as unified.
    void* p = std::malloc(bytes);
    if (!p)
        return nullptr;

    m_allocated[p] = AllocationRecord { bytes, purpose };
    return { p, mem_tag() };
}

void
MockDevice::free(tagged_ptr<void> p)
{
    tracked_free(m_allocated, p, "MockDevice");
    std::free(p.get());
}

void
MockDevice::copy_to(tagged_ptr<void> device, tagged_ptr<const void> host,
                    size_t bytes)
{
    if (!device || !host || device == host)
        return;

    // copy_to expects a device destination and host source.
    if (!is_mock_device_memory(device) || !is_host_memory(host))
        std::abort();

    std::memcpy(device.get(), host.get(), bytes);
}

void
MockDevice::copy_from(tagged_ptr<void> host, tagged_ptr<const void> device,
                      size_t bytes)
{
    if (!host || !device || host == device)
        return;

    // copy_from expects a host destination and device source.
    if (!is_host_memory(host) || !is_mock_device_memory(device))
        std::abort();

    std::memcpy(host.get(), device.get(), bytes);
}

void
MockDevice::copy_in(tagged_ptr<void> to, tagged_ptr<const void> from,
                    size_t bytes)
{
    if (!to || !from || to == from)
        return;

    // copy_in expects both pointers to belong to MockDevice.
    if (!is_mock_device_memory(to) || !is_mock_device_memory(from))
        std::abort();

    std::memcpy(to.get(), from.get(), bytes);
}

void
MockDevice::run(int width, int height, Kernel kernel, tagged_ptr<void> data)
{
    struct MockDeviceExecutionGuard {
        MockDeviceExecutionGuard()
        {
            NullArena::g_arena_context = MockDevice::run_tag();
        }
        ~MockDeviceExecutionGuard()
        {
            NullArena::g_arena_context = Host::run_tag();
        }
    } guard;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x)
            kernel(x, y, data);
    }
}

#if TEXTURE_DEVICE_HAS_CUDA_RUNTIME

CudaArena::~CudaArena()
{
    // Match Host/MockDevice semantics: destructor surfaces leaked allocations
    // instead of silently cleaning them up.
    report_leaks("CudaArena", m_allocated);
}

tagged_ptr<void>
CudaArena::alloc(size_t bytes, const char* purpose)
{
#    if TEXTURE_DEVICE_ENABLE_CUDA_SKETCH_IMPL
    void* p = nullptr;
    // Unified policy: cudaMallocManaged yields one address visible to host and
    // device; otherwise we keep the classic device-only cudaMalloc path.
    cudaError_t err = use_unified_memory
                          ? cudaMallocManaged(&p, bytes, cudaMemAttachGlobal)
                          : cudaMalloc(&p, bytes);
    if (err != cudaSuccess)
        return nullptr;
    m_allocated[p] = AllocationRecord { bytes, purpose };
    return { p, mem_tag() };
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
        OIIO::print(
            stderr,
            "texture-device: invalid free ptr={:p} (not allocated by CudaArena::alloc)\n",
            p.get());
        std::abort();
    }
    if (cudaFree(p.get()) != cudaSuccess) {
        OIIO::print(
            stderr,
            "texture-device: cudaFree failed in CudaArena::free ptr={:p}\n",
            p.get());
        std::abort();
    }
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
    if (!device || !host || device == host)
        return;

    cudaError_t err = cudaMemcpyAsync(device.get(), host.get(), bytes,
                                      use_unified_memory
                                          ? cudaMemcpyDefault
                                          : cudaMemcpyHostToDevice,
                                      m_stream);
    if (err != cudaSuccess || cudaStreamSynchronize(m_stream) != cudaSuccess)
        std::abort();
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
    if (!device || !host || device == host)
        return;

    cudaError_t err = cudaMemcpyAsync(host.get(), device.get(), bytes,
                                      use_unified_memory
                                          ? cudaMemcpyDefault
                                          : cudaMemcpyDeviceToHost,
                                      m_stream);
    if (err != cudaSuccess || cudaStreamSynchronize(m_stream) != cudaSuccess)
        std::abort();
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
    if (!to || !from || to == from)
        return;

    cudaError_t err = cudaMemcpyAsync(to.get(), from.get(), bytes,
                                      use_unified_memory
                                          ? cudaMemcpyDefault
                                          : cudaMemcpyDeviceToDevice,
                                      m_stream);
    if (err != cudaSuccess || cudaStreamSynchronize(m_stream) != cudaSuccess)
        std::abort();
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
