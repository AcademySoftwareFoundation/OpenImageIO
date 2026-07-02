// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <unordered_map>

#include "tagged_ptr.h"

#if defined(__has_include)
#    if __has_include(<cuda_runtime_api.h>)
#        include <cuda_runtime_api.h>
#        define TEXTURE_DEVICE_HAS_CUDA_RUNTIME 1
#    endif
#    if __has_include(<cuda/atomic>)
#        include <cuda/atomic>
#    endif
#endif

#ifndef TEXTURE_DEVICE_HAS_CUDA_RUNTIME
#    define TEXTURE_DEVICE_HAS_CUDA_RUNTIME 0
#endif

namespace texture_device {

class NullArena {
public:
    static bool use_unified_memory;

    static uint64_t g_arena_context;
    using Kernel = void (*)(int x, int y, tagged_ptr<void> data);

    // Methods an arena should implement
    static constexpr uint64_t run_tag() { return 0; }
    static constexpr uint64_t mem_tag() { return run_tag(); }

    // Allocation contract for mirrored data:
    // 1. Allocate the device arena first.
    // 2. Allocate the host arena second, passing the device pointer as
    //    `mirror`.
    // In unified mode, host alloc should detect that `mirror` is already a
    // shared pointer and skip allocating a second block.
    tagged_ptr<void> alloc(tagged_ptr<void> mirror, size_t bytes,
                           const char* purpose);
    // Non-mirrored allocation path when no counterpart pointer exists.
    tagged_ptr<void> alloc(size_t bytes, const char* purpose);
    void free(tagged_ptr<void> p);
    void copy_to(tagged_ptr<void> device, tagged_ptr<const void> host,
                 size_t bytes);
    void copy_from(tagged_ptr<void> host, tagged_ptr<const void> device,
                   size_t bytes);
    void copy_in(tagged_ptr<void> to, tagged_ptr<const void> from,
                 size_t bytes);
    // Optionally, if we intend to run things on it
    void run(int width, int height, Kernel kernel, tagged_ptr<void> data);
};

class Host : public NullArena {
public:
    static constexpr uint64_t run_tag() { return maketag("Host"); }
    static uint64_t mem_tag()
    {
        return use_unified_memory ? unified_ptr_tag() : run_tag();
    }

    ~Host();

    tagged_ptr<void> alloc(tagged_ptr<void> mirror, size_t bytes,
                           const char* purpose);
    tagged_ptr<void> alloc(size_t bytes, const char* purpose)
    {
        return alloc(nullptr, bytes, purpose);
    }
    void free(tagged_ptr<void> p);
    void copy_to(tagged_ptr<void> device, tagged_ptr<const void> host,
                 size_t bytes);
    void copy_from(tagged_ptr<void> host, tagged_ptr<const void> device,
                   size_t bytes);
    void copy_in(tagged_ptr<void> to, tagged_ptr<const void> from,
                 size_t bytes);
    template<typename T> struct Atomic : public std::atomic<T> {
        using std::atomic<T>::atomic;
        using std::atomic<T>::operator=;

        T load() const
        {
            return std::atomic<T>::load(std::memory_order_acquire);
        }
        void store(const T& v)
        {
            std::atomic<T>::store(v, std::memory_order_release);
        }
        bool cas(T& expected, const T& desired)
        {
            return std::atomic<T>::compare_exchange_strong(
                expected, desired, std::memory_order_acq_rel,
                std::memory_order_acquire);
        }
        T fetch_add(const T& v)
        {
            return std::atomic<T>::fetch_add(v, std::memory_order_acq_rel);
        }
    };

private:
    struct AllocationRecord {
        size_t bytes        = 0;
        const char* purpose = nullptr;
    };

    // Record every alloc pointer with its purpose to validate frees and leaks.
    std::unordered_map<void*, AllocationRecord> m_allocated;
};

class MockDevice : public NullArena {
public:
    static constexpr uint64_t run_tag() { return maketag("MockDevice"); }
    static uint64_t mem_tag()
    {
        return use_unified_memory ? unified_ptr_tag() : run_tag();
    }

    template<typename T> using Atomic = Host::Atomic<T>;
    using NullArena::Kernel;

    ~MockDevice();

    tagged_ptr<void> alloc(size_t bytes, const char* purpose);
    void free(tagged_ptr<void> p);
    void copy_to(tagged_ptr<void> device, tagged_ptr<const void> host,
                 size_t bytes);
    void copy_from(tagged_ptr<void> host, tagged_ptr<const void> device,
                   size_t bytes);
    void copy_in(tagged_ptr<void> to, tagged_ptr<const void> from,
                 size_t bytes);
    void run(int width, int height, Kernel kernel, tagged_ptr<void> data);

private:
    struct AllocationRecord {
        size_t bytes        = 0;
        const char* purpose = nullptr;
    };

    // Record every alloc pointer with its purpose to validate frees and leaks.
    std::unordered_map<void*, AllocationRecord> m_allocated;
};

#if TEXTURE_DEVICE_HAS_CUDA_RUNTIME

// Guidance-only sketch for a real device arena backed by CUDA runtime APIs.
// This is intentionally not wired into the current test flow.
class CudaArena : public NullArena {
public:
    static constexpr uint64_t run_tag() { return maketag("CudaDevice"); }
    static uint64_t mem_tag()
    {
        return use_unified_memory ? unified_ptr_tag() : run_tag();
    }

    template<typename T>
#    ifdef __CUDA_ARCH__
    struct Atomic : public cuda::atomic<T> {
        using cuda::atomic<T>::atomic;
        using cuda::atomic<T>::operator=;

        T load() const
        {
            return cuda::atomic<T>::load(cuda::memory_order_acquire);
        }
        void store(const T& v)
        {
            cuda::atomic<T>::store(v, cuda::memory_order_release);
        }
        bool cas(T& expected, const T& desired)
        {
            return cuda::atomic<T>::compare_exchange_strong(
                expected, desired, cuda::memory_order_acq_rel,
                cuda::memory_order_acquire);
        }
        T fetch_add(const T& v)
        {
            return cuda::atomic<T>::fetch_add(v, cuda::memory_order_acq_rel);
        }
    };
#    else
    using Atomic = Host::Atomic<T>;
#    endif
    using NullArena::Kernel;

    CudaArena() = default;
    explicit CudaArena(cudaStream_t stream)
        : m_stream(stream)
    {
    }

    ~CudaArena();

    tagged_ptr<void> alloc(size_t bytes, const char* purpose);
    void free(tagged_ptr<void> p);
    void copy_to(tagged_ptr<void> device, tagged_ptr<const void> host,
                 size_t bytes);
    void copy_from(tagged_ptr<void> host, tagged_ptr<const void> device,
                   size_t bytes);
    void copy_in(tagged_ptr<void> to, tagged_ptr<const void> from,
                 size_t bytes);
    void run(int width, int height, Kernel kernel, tagged_ptr<void> data);

    void set_stream(cudaStream_t stream) { m_stream = stream; }
    cudaStream_t stream() const { return m_stream; }

private:
    struct AllocationRecord {
        size_t bytes        = 0;
        const char* purpose = nullptr;
    };

    cudaStream_t m_stream = nullptr;
    std::unordered_map<void*, AllocationRecord> m_allocated;
};

#endif

}  // namespace texture_device
