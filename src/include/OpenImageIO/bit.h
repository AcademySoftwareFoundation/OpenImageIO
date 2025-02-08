// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#pragma once

#include <OpenImageIO/export.h>
#include <OpenImageIO/oiioversion.h>
#include <OpenImageIO/platform.h>


OIIO_NAMESPACE_BEGIN


/// Standards-compliant bit cast of two equally sized types. This is used
/// equivalently to C++20 std::bit_cast, but it works prior to C++20 and
/// it has the right decorators to work with Cuda.
/// @version 2.4.1
template<typename To, typename From>
OIIO_NODISCARD OIIO_FORCEINLINE OIIO_HOSTDEVICE To
bitcast(const From& from) noexcept
{
    static_assert(sizeof(From) == sizeof(To),
                  "bit_cast must be between objects of the same size");
    // NOTE: this is the only standards compliant way of doing this type of
    // casting. This seems to generate optimal code for gcc, clang, MSVS, and
    // icx, for both scalar code and vectorized loops, but icc fails to
    // vectorize without the intrinsics overrides below.
    //
    // If we ever find the memcpy isn't doing the job, we should try
    // gcc/clang's __builtin_bit_cast and see if that's any better. Some day
    // this may all be replaced with C++20 std::bit_cast, but we should not do
    // so without checking that it works ok for vectorized loops.
    To result;
    memcpy((void*)&result, &from, sizeof(From));
    return result;
}

#if defined(__INTEL_COMPILER)
// For Intel icc, using the memcpy implementation above will cause a loop with
// a bitcast to fail to vectorize, but using the intrinsics below will allow
// it to vectorize. For icx, as well as gcc and clang, the same optimal code
// is generated (even in a vectorized loop) for memcpy. We can probably remove
// these intrinsics once we drop support for icc.
template<>
OIIO_NODISCARD OIIO_FORCEINLINE uint32_t
bitcast<uint32_t, float>(const float& val) noexcept
{
    return static_cast<uint32_t>(_castf32_u32(val));
}

template<>
OIIO_NODISCARD OIIO_FORCEINLINE int32_t
bitcast<int32_t, float>(const float& val) noexcept
{
    return static_cast<int32_t>(_castf32_u32(val));
}

template<>
OIIO_NODISCARD OIIO_FORCEINLINE float
bitcast<float, uint32_t>(const uint32_t& val) noexcept
{
    return _castu32_f32(val);
}

template<>
OIIO_NODISCARD OIIO_FORCEINLINE float
bitcast<float, int32_t>(const int32_t& val) noexcept
{
    return _castu32_f32(val);
}
#endif


OIIO_NODISCARD OIIO_FORCEINLINE OIIO_HOSTDEVICE int
bitcast_to_int(float x)
{
    return bitcast<int, float>(x);
}

OIIO_NODISCARD OIIO_FORCEINLINE OIIO_HOSTDEVICE float
bitcast_to_float(int x)
{
    return bitcast<float, int>(x);
}



/// Change endian-ness of a 16, 32, or 64 bit value by reversing the bytes.
/// This is a pre-C++23 (and Cuda-capable) version of std::byteswap. This
/// should work for any of short, unsigned short, int, unsigned int, float,
/// long long, pointers.
template<class T>
OIIO_NODISCARD inline OIIO_HOSTDEVICE T
byteswap(T n)
{
    unsigned char* c = reinterpret_cast<unsigned char*>(&n);
    if (sizeof(T) == 2) {
        std::swap(c[0], c[1]);
    } else if (sizeof(T) == 4) {
        std::swap(c[0], c[3]);
        std::swap(c[1], c[2]);
    } else if (sizeof(T) == 8) {
        std::swap(c[0], c[7]);
        std::swap(c[1], c[6]);
        std::swap(c[2], c[5]);
        std::swap(c[3], c[4]);
    }
    return n;
}



#if (OIIO_GNUC_VERSION || OIIO_ANY_CLANG     \
     || OIIO_INTEL_CLASSIC_COMPILER_VERSION) \
    && !defined(__CUDACC__)
// CPU gcc and compatible can use these intrinsics, 8-15x faster

template<>
OIIO_NODISCARD inline uint16_t
byteswap(uint16_t f)
{
    return __builtin_bswap16(f);
}

template<>
OIIO_NODISCARD inline uint32_t
byteswap(uint32_t f)
{
    return __builtin_bswap32(f);
}

template<>
OIIO_NODISCARD inline uint64_t
byteswap(uint64_t f)
{
    return __builtin_bswap64(f);
}

template<>
OIIO_NODISCARD inline int16_t
byteswap(int16_t f)
{
    return __builtin_bswap16(f);
}

template<>
OIIO_NODISCARD inline int32_t
byteswap(int32_t f)
{
    return __builtin_bswap32(f);
}

template<>
OIIO_NODISCARD inline int64_t
byteswap(int64_t f)
{
    return __builtin_bswap64(f);
}

template<>
OIIO_NODISCARD inline float
byteswap(float f)
{
    return bitcast<float>(byteswap(bitcast<uint32_t>(f)));
}

template<>
OIIO_NODISCARD inline double
byteswap(double f)
{
    return bitcast<double>(byteswap(bitcast<uint64_t>(f)));
}

#elif defined(_MSC_VER) && !defined(__CUDACC__)
// CPU MSVS can use these intrinsics

template<>
OIIO_NODISCARD inline uint16_t
byteswap(uint16_t f)
{
    return _byteswap_ushort(f);
}

template<>
OIIO_NODISCARD inline uint32_t
byteswap(uint32_t f)
{
    return _byteswap_ulong(f);
}

template<>
OIIO_NODISCARD inline uint64_t
byteswap(uint64_t f)
{
    return _byteswap_uint64(f);
}

template<>
OIIO_NODISCARD inline int16_t
byteswap(int16_t f)
{
    return _byteswap_ushort(f);
}

template<>
OIIO_NODISCARD inline int32_t
byteswap(int32_t f)
{
    return _byteswap_ulong(f);
}

template<>
OIIO_NODISCARD inline int64_t
byteswap(int64_t f)
{
    return _byteswap_uint64(f);
}
#endif



/// Bitwise circular rotation left by `s` bits (for any unsigned integer
/// type).  For info on the C++20 std::rotl(), see
/// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p0553r4.html
// FIXME: this should be constexpr, but we're leaving that out for now
// because the Cuda specialization uses an intrinsic that isn't constexpr.
// Come back to this later when more of the Cuda library is properly
// constexpr.
template<class T>
OIIO_NODISCARD OIIO_FORCEINLINE OIIO_HOSTDEVICE T
rotl(T x, int s) noexcept
{
    static_assert(std::is_unsigned<T>::value && std::is_integral<T>::value,
                  "rotl only works for unsigned integer types");
    return (x << s) | (x >> ((sizeof(T) * 8) - s));
}


#if defined(__CUDA_ARCH__) && __CUDA_ARCH__ >= 320
// Cuda has an intrinsic for 32 bit unsigned int rotation
// FIXME: This should be constexpr, but __funnelshift_lc seems not to be
// marked as such.
template<>
OIIO_NODISCARD OIIO_FORCEINLINE OIIO_HOSTDEVICE uint32_t
rotl(uint32_t x, int s) noexcept
{
    return __funnelshift_lc(x, x, s);
}
#endif



// Old names -- DEPRECATED(2.1)
OIIO_DEPRECATED("use rotl() instead (2.1)")
OIIO_FORCEINLINE OIIO_HOSTDEVICE uint32_t
rotl32(uint32_t x, int k)
{
#if defined(__CUDA_ARCH__) && __CUDA_ARCH__ >= 320
    return __funnelshift_lc(x, x, k);
#else
    return (x << k) | (x >> (32 - k));
#endif
}

OIIO_DEPRECATED("use rotl() instead (2.1)")
OIIO_FORCEINLINE OIIO_HOSTDEVICE uint64_t
rotl64(uint64_t x, int k)
{
    return (x << k) | (x >> (64 - k));
}



OIIO_NAMESPACE_END
