// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <hwy/highway.h>
#include <hwy/contrib/math/math-inl.h>
#include <OpenImageIO/half.h>
#include <OpenImageIO/imageio.h>
#include <type_traits>
#include <algorithm>
#include <cstddef>

OIIO_NAMESPACE_BEGIN

// Alias for Highway's namespace for convenience
namespace hn = hwy::HWY_NAMESPACE;

// -----------------------------------------------------------------------
// Type Traits
// -----------------------------------------------------------------------

/// Determine the appropriate SIMD math type for a given result type.
/// Promotes smaller types to float, keeps double as double.
/// Note: uint32_t uses float (not double) for image processing performance.
/// In OIIO, uint32 images are normalized to 0-1 range like uint8/uint16,
/// so float precision (24-bit mantissa) is sufficient and much faster than double.
template<typename T> struct SimdMathType {
    using type = float;
};
template<> struct SimdMathType<double> {
    using type = double;
};

// -----------------------------------------------------------------------
// Load and Promote
// -----------------------------------------------------------------------

/// Load and promote source data to target SIMD type.
/// Handles type conversions from various source formats (uint8_t, int8_t, uint16_t,
/// int16_t, uint32_t, int32_t, uint64_t, int64_t, half, float, double) to the
/// target SIMD computation type.
/// @param d Highway descriptor tag defining the target SIMD type
/// @param ptr Pointer to source data (may be unaligned)
/// @return SIMD vector with promoted values
template<class D, typename SrcT>
inline auto
LoadPromote(D d, const SrcT* ptr)
{
    using MathT = typename D::T;

    if constexpr (std::is_same_v<SrcT, MathT>) {
        return hn::Load(d, ptr);
    } else if constexpr (std::is_same_v<SrcT, half>) {
        using T16 = hwy::float16_t;
        auto d16  = hn::Rebind<T16, D>();
        auto v16  = hn::Load(d16, (const T16*)ptr);
        return hn::PromoteTo(d, v16);
    } else if constexpr (std::is_same_v<SrcT, uint8_t>) {
        auto d_u8 = hn::Rebind<uint8_t, D>();
        auto v_u8 = hn::Load(d_u8, ptr);
        auto v_promoted = hn::ConvertTo(
            d, hn::PromoteTo(hn::Rebind<int32_t, D>(),
                             hn::PromoteTo(hn::Rebind<int16_t, D>(), v_u8)));
        // Normalize to 0-1 range for image operations
        return hn::Mul(v_promoted, hn::Set(d, (MathT)(1.0 / 255.0)));
    } else if constexpr (std::is_same_v<SrcT, int8_t>) {
        auto d_i8 = hn::Rebind<int8_t, D>();
        auto v_i8 = hn::Load(d_i8, ptr);
        auto v_promoted = hn::ConvertTo(
            d, hn::PromoteTo(hn::Rebind<int32_t, D>(),
                             hn::PromoteTo(hn::Rebind<int16_t, D>(), v_i8)));
        // Normalize: map [-128, 127] to [0, 1]
        auto v_shifted = hn::Add(v_promoted, hn::Set(d, (MathT)128.0));
        return hn::Mul(v_shifted, hn::Set(d, (MathT)(1.0 / 255.0)));
    } else if constexpr (std::is_same_v<SrcT, uint16_t>) {
        auto d_u16 = hn::Rebind<uint16_t, D>();
        auto v_u16 = hn::Load(d_u16, ptr);
        auto v_promoted = hn::ConvertTo(d, hn::PromoteTo(hn::Rebind<int32_t, D>(), v_u16));
        // Normalize to 0-1 range for image operations
        return hn::Mul(v_promoted, hn::Set(d, (MathT)(1.0 / 65535.0)));
    } else if constexpr (std::is_same_v<SrcT, int16_t>) {
        auto d_i16 = hn::Rebind<int16_t, D>();
        auto v_i16 = hn::Load(d_i16, ptr);
        auto v_promoted = hn::ConvertTo(d, hn::PromoteTo(hn::Rebind<int32_t, D>(), v_i16));
        // Normalize: map [-32768, 32767] to [0, 1]
        auto v_shifted = hn::Add(v_promoted, hn::Set(d, (MathT)32768.0));
        return hn::Mul(v_shifted, hn::Set(d, (MathT)(1.0 / 65535.0)));
    } else if constexpr (std::is_same_v<SrcT, uint32_t>) {
        // uint32 to float: Load, convert, and normalize to 0-1 range
        auto d_u32 = hn::Rebind<uint32_t, D>();
        auto v_u32 = hn::Load(d_u32, ptr);
        auto v_promoted = hn::ConvertTo(d, v_u32);
        // Normalize to 0-1 range for image operations
        return hn::Mul(v_promoted, hn::Set(d, (MathT)(1.0 / 4294967295.0)));
    } else if constexpr (std::is_same_v<SrcT, int32_t>) {
        // int32 to float: Load and convert directly
        auto d_i32 = hn::Rebind<int32_t, D>();
        auto v_i32 = hn::Load(d_i32, ptr);
        return hn::ConvertTo(d, v_i32);
    } else if constexpr (std::is_same_v<SrcT, uint64_t>) {
        // uint64 to float: Load and demote to uint32, then convert
        // Note: Precision loss expected for large values (>24 bits)
        auto d_u64 = hn::Rebind<uint64_t, D>();
        auto v_u64 = hn::Load(d_u64, ptr);
        auto d_u32 = hn::Rebind<uint32_t, D>();
        auto v_u32 = hn::DemoteTo(d_u32, v_u64);
        return hn::ConvertTo(d, v_u32);
    } else if constexpr (std::is_same_v<SrcT, int64_t>) {
        // int64 to float: Load and demote to int32, then convert
        auto d_i64 = hn::Rebind<int64_t, D>();
        auto v_i64 = hn::Load(d_i64, ptr);
        auto d_i32 = hn::Rebind<int32_t, D>();
        auto v_i32 = hn::DemoteTo(d_i32, v_i64);
        return hn::ConvertTo(d, v_i32);
    } else {
        return hn::Zero(d);
    }
}

/// Load and promote partial source data to target SIMD type.
/// Same as LoadPromote but handles partial vectors (< full lane count).
/// @param d Highway descriptor tag defining the target SIMD type
/// @param ptr Pointer to source data (may be unaligned)
/// @param count Number of elements to load (must be <= lane count)
/// @return SIMD vector with promoted values (undefined in unused lanes)
template<class D, typename SrcT>
inline auto
LoadPromoteN(D d, const SrcT* ptr, size_t count)
{
    using MathT = typename D::T;

    if constexpr (std::is_same_v<SrcT, MathT>) {
        return hn::LoadN(d, ptr, count);
    } else if constexpr (std::is_same_v<SrcT, half>) {
        using T16 = hwy::float16_t;
        auto d16  = hn::Rebind<T16, D>();
        auto v16  = hn::LoadN(d16, (const T16*)ptr, count);
        return hn::PromoteTo(d, v16);
    } else if constexpr (std::is_same_v<SrcT, uint8_t>) {
        auto d_u8 = hn::Rebind<uint8_t, D>();
        auto v_u8 = hn::LoadN(d_u8, ptr, count);
        auto v_promoted = hn::ConvertTo(
            d, hn::PromoteTo(hn::Rebind<int32_t, D>(),
                             hn::PromoteTo(hn::Rebind<int16_t, D>(), v_u8)));
        // Normalize to 0-1 range for image operations
        return hn::Mul(v_promoted, hn::Set(d, (MathT)(1.0 / 255.0)));
    } else if constexpr (std::is_same_v<SrcT, int8_t>) {
        auto d_i8 = hn::Rebind<int8_t, D>();
        auto v_i8 = hn::LoadN(d_i8, ptr, count);
        auto v_promoted = hn::ConvertTo(
            d, hn::PromoteTo(hn::Rebind<int32_t, D>(),
                             hn::PromoteTo(hn::Rebind<int16_t, D>(), v_i8)));
        // Normalize: map [-128, 127] to [0, 1]
        auto v_shifted = hn::Add(v_promoted, hn::Set(d, (MathT)128.0));
        return hn::Mul(v_shifted, hn::Set(d, (MathT)(1.0 / 255.0)));
    } else if constexpr (std::is_same_v<SrcT, uint16_t>) {
        auto d_u16 = hn::Rebind<uint16_t, D>();
        auto v_u16 = hn::LoadN(d_u16, ptr, count);
        auto v_promoted = hn::ConvertTo(d, hn::PromoteTo(hn::Rebind<int32_t, D>(), v_u16));
        // Normalize to 0-1 range for image operations
        return hn::Mul(v_promoted, hn::Set(d, (MathT)(1.0 / 65535.0)));
    } else if constexpr (std::is_same_v<SrcT, int16_t>) {
        auto d_i16 = hn::Rebind<int16_t, D>();
        auto v_i16 = hn::LoadN(d_i16, ptr, count);
        auto v_promoted = hn::ConvertTo(d, hn::PromoteTo(hn::Rebind<int32_t, D>(), v_i16));
        // Normalize: map [-32768, 32767] to [0, 1]
        auto v_shifted = hn::Add(v_promoted, hn::Set(d, (MathT)32768.0));
        return hn::Mul(v_shifted, hn::Set(d, (MathT)(1.0 / 65535.0)));
    } else if constexpr (std::is_same_v<SrcT, uint32_t>) {
        // uint32 to float: Load, convert, and normalize to 0-1 range
        auto d_u32 = hn::Rebind<uint32_t, D>();
        auto v_u32 = hn::LoadN(d_u32, ptr, count);
        auto v_promoted = hn::ConvertTo(d, v_u32);
        // Normalize to 0-1 range for image operations
        return hn::Mul(v_promoted, hn::Set(d, (MathT)(1.0 / 4294967295.0)));
    } else if constexpr (std::is_same_v<SrcT, int32_t>) {
        // int32 to float: Load and convert directly
        auto d_i32 = hn::Rebind<int32_t, D>();
        auto v_i32 = hn::LoadN(d_i32, ptr, count);
        return hn::ConvertTo(d, v_i32);
    } else if constexpr (std::is_same_v<SrcT, uint64_t>) {
        // uint64 to float: Load and demote to uint32, then convert
        auto d_u64 = hn::Rebind<uint64_t, D>();
        auto v_u64 = hn::LoadN(d_u64, ptr, count);
        auto d_u32 = hn::Rebind<uint32_t, D>();
        auto v_u32 = hn::DemoteTo(d_u32, v_u64);
        return hn::ConvertTo(d, v_u32);
    } else if constexpr (std::is_same_v<SrcT, int64_t>) {
        // int64 to float: Load and demote to int32, then convert
        auto d_i64 = hn::Rebind<int64_t, D>();
        auto v_i64 = hn::LoadN(d_i64, ptr, count);
        auto d_i32 = hn::Rebind<int32_t, D>();
        auto v_i32 = hn::DemoteTo(d_i32, v_i64);
        return hn::ConvertTo(d, v_i32);
    } else {
        return hn::Zero(d);
    }
}

// -----------------------------------------------------------------------
// Demote and Store
// -----------------------------------------------------------------------

/// Demote SIMD values and store to destination type.
/// Handles type conversions from SIMD computation type (float/double) back to
/// various destination formats with proper rounding and clamping for integer types.
/// @param d Highway descriptor tag for the source SIMD type
/// @param ptr Pointer to destination data (may be unaligned)
/// @param v SIMD vector to demote and store
template<class D, typename DstT, typename VecT>
inline void
DemoteStore(D d, DstT* ptr, VecT v)
{
    using MathT = typename D::T;
    using VecD  = hn::Vec<D>;

    if constexpr (std::is_same_v<DstT, MathT>) {
        hn::Store(v, d, ptr);
    } else if constexpr (std::is_same_v<DstT, half>) {
        auto d16 = hn::Rebind<hwy::float16_t, D>();
        auto v16 = hn::DemoteTo(d16, v);
        hn::Store(v16, d16, (hwy::float16_t*)ptr);
    } else if constexpr (std::is_same_v<DstT, uint8_t>) {
        VecD v_val     = (VecD)v;
        // Denormalize from 0-1 range to 0-255 range
        VecD v_denorm  = hn::Mul(v_val, hn::Set(d, (MathT)255.0));
        VecD v_rounded = hn::Add(v_denorm, hn::Set(d, (MathT)0.5));
        VecD v_zero    = hn::Zero(d);
        VecD v_max     = hn::Set(d, (MathT)255.0);
        VecD v_clamped = hn::Max(v_rounded, v_zero);
        v_clamped      = hn::Min(v_clamped, v_max);

        auto d32   = hn::Rebind<int32_t, D>();
        auto vi32  = hn::ConvertTo(d32, v_clamped);
        auto d_i16 = hn::Rebind<int16_t, D>();
        auto v_i16 = hn::DemoteTo(d_i16, vi32);
        auto d_u8  = hn::Rebind<uint8_t, D>();
        auto v_u8  = hn::DemoteTo(d_u8, v_i16);
        hn::Store(v_u8, d_u8, ptr);
    } else if constexpr (std::is_same_v<DstT, int8_t>) {
        VecD v_val     = (VecD)v;
        // Denormalize from 0-1 range to -128-127 range
        VecD v_denorm  = hn::Mul(v_val, hn::Set(d, (MathT)255.0));
        VecD v_shifted = hn::Sub(v_denorm, hn::Set(d, (MathT)128.0));
        VecD v_rounded = hn::Add(v_shifted, hn::Set(d, (MathT)0.5));
        VecD v_min     = hn::Set(d, (MathT)-128.0);
        VecD v_max     = hn::Set(d, (MathT)127.0);
        VecD v_clamped = hn::Max(v_rounded, v_min);
        v_clamped      = hn::Min(v_clamped, v_max);

        auto d32   = hn::Rebind<int32_t, D>();
        auto vi32  = hn::ConvertTo(d32, v_clamped);
        auto d_i16 = hn::Rebind<int16_t, D>();
        auto v_i16 = hn::DemoteTo(d_i16, vi32);
        auto d_i8  = hn::Rebind<int8_t, D>();
        auto v_i8  = hn::DemoteTo(d_i8, v_i16);
        hn::Store(v_i8, d_i8, ptr);
    } else if constexpr (std::is_same_v<DstT, uint16_t>) {
        VecD v_val     = (VecD)v;
        // Denormalize from 0-1 range to 0-65535 range
        VecD v_denorm  = hn::Mul(v_val, hn::Set(d, (MathT)65535.0));
        VecD v_rounded = hn::Add(v_denorm, hn::Set(d, (MathT)0.5));
        VecD v_zero    = hn::Zero(d);
        VecD v_max     = hn::Set(d, (MathT)65535.0);
        VecD v_clamped = hn::Max(v_rounded, v_zero);
        v_clamped      = hn::Min(v_clamped, v_max);

        auto d32   = hn::Rebind<int32_t, D>();
        auto vi32  = hn::ConvertTo(d32, v_clamped);
        auto d_u16 = hn::Rebind<uint16_t, D>();
        auto v_u16 = hn::DemoteTo(d_u16, vi32);
        hn::Store(v_u16, d_u16, ptr);
    } else if constexpr (std::is_same_v<DstT, int16_t>) {
        VecD v_val     = (VecD)v;
        // Denormalize from 0-1 range to -32768-32767 range
        VecD v_denorm  = hn::Mul(v_val, hn::Set(d, (MathT)65535.0));
        VecD v_shifted = hn::Sub(v_denorm, hn::Set(d, (MathT)32768.0));
        VecD v_rounded = hn::Add(v_shifted, hn::Set(d, (MathT)0.5));
        VecD v_min     = hn::Set(d, (MathT)-32768.0);
        VecD v_max     = hn::Set(d, (MathT)32767.0);
        VecD v_clamped = hn::Max(v_rounded, v_min);
        v_clamped      = hn::Min(v_clamped, v_max);

        auto d32   = hn::Rebind<int32_t, D>();
        auto vi32  = hn::ConvertTo(d32, v_clamped);
        auto d_i16 = hn::Rebind<int16_t, D>();
        auto v_i16 = hn::DemoteTo(d_i16, vi32);
        hn::Store(v_i16, d_i16, ptr);
    } else if constexpr (std::is_same_v<DstT, uint32_t>) {
        // float -> uint32: Denormalize from 0-1 to 0-4294967295, round and convert
        VecD v_val     = (VecD)v;
        // Denormalize from 0-1 range to 0-4294967295 range
        VecD v_denorm  = hn::Mul(v_val, hn::Set(d, (MathT)4294967295.0));
        VecD v_rounded = hn::Add(v_denorm, hn::Set(d, (MathT)0.5));
        VecD v_zero    = hn::Zero(d);
        VecD v_max     = hn::Set(d, (MathT)4294967295.0);
        VecD v_clamped = hn::Max(v_rounded, v_zero);
        v_clamped      = hn::Min(v_clamped, v_max);

        auto d_u32 = hn::Rebind<uint32_t, D>();
        auto v_u32 = hn::ConvertTo(d_u32, v_clamped);
        hn::Store(v_u32, d_u32, ptr);
    } else if constexpr (std::is_same_v<DstT, int32_t>) {
        // float -> int32: Round and convert directly
        VecD v_val     = (VecD)v;
        VecD v_rounded = hn::Add(v_val, hn::Set(d, (MathT)0.5));
        VecD v_min     = hn::Set(d, (MathT)-2147483648.0);
        VecD v_max     = hn::Set(d, (MathT)2147483647.0);
        VecD v_clamped = hn::Max(v_rounded, v_min);
        v_clamped      = hn::Min(v_clamped, v_max);

        auto d_i32 = hn::Rebind<int32_t, D>();
        auto v_i32 = hn::ConvertTo(d_i32, v_clamped);
        hn::Store(v_i32, d_i32, ptr);
    } else if constexpr (std::is_same_v<DstT, uint64_t>) {
        // float -> uint64: Promote via uint32
        // Note: Precision loss expected (float has only 24-bit mantissa)
        VecD v_val     = (VecD)v;
        VecD v_rounded = hn::Add(v_val, hn::Set(d, (MathT)0.5));
        VecD v_zero    = hn::Zero(d);
        VecD v_clamped = hn::Max(v_rounded, v_zero);

        auto d_u32 = hn::Rebind<uint32_t, D>();
        auto v_u32 = hn::ConvertTo(d_u32, v_clamped);
        auto d_u64 = hn::Rebind<uint64_t, D>();
        auto v_u64 = hn::PromoteTo(d_u64, v_u32);
        hn::Store(v_u64, d_u64, ptr);
    } else if constexpr (std::is_same_v<DstT, int64_t>) {
        // float -> int64: Promote via int32
        VecD v_val     = (VecD)v;
        VecD v_rounded = hn::Add(v_val, hn::Set(d, (MathT)0.5));

        auto d_i32 = hn::Rebind<int32_t, D>();
        auto v_i32 = hn::ConvertTo(d_i32, v_rounded);
        auto d_i64 = hn::Rebind<int64_t, D>();
        auto v_i64 = hn::PromoteTo(d_i64, v_i32);
        hn::Store(v_i64, d_i64, ptr);
    }
}

/// Demote and store partial SIMD values to destination type.
/// Same as DemoteStore but handles partial vectors (< full lane count).
/// @param d Highway descriptor tag for the source SIMD type
/// @param ptr Pointer to destination data (may be unaligned)
/// @param v SIMD vector to demote and store
/// @param count Number of elements to store (must be <= lane count)
template<class D, typename DstT, typename VecT>
inline void
DemoteStoreN(D d, DstT* ptr, VecT v, size_t count)
{
    using MathT = typename D::T;
    using VecD  = hn::Vec<D>;

    if constexpr (std::is_same_v<DstT, MathT>) {
        hn::StoreN(v, d, ptr, count);
    } else if constexpr (std::is_same_v<DstT, half>) {
        auto d16 = hn::Rebind<hwy::float16_t, D>();
        auto v16 = hn::DemoteTo(d16, v);
        hn::StoreN(v16, d16, (hwy::float16_t*)ptr, count);
    } else if constexpr (std::is_same_v<DstT, uint8_t>) {
        VecD v_val     = (VecD)v;
        // Denormalize from 0-1 range to 0-255 range
        VecD v_denorm  = hn::Mul(v_val, hn::Set(d, (MathT)255.0));
        VecD v_rounded = hn::Add(v_denorm, hn::Set(d, (MathT)0.5));
        VecD v_zero    = hn::Zero(d);
        VecD v_max     = hn::Set(d, (MathT)255.0);
        VecD v_clamped = hn::Max(v_rounded, v_zero);
        v_clamped      = hn::Min(v_clamped, v_max);

        auto d32   = hn::Rebind<int32_t, D>();
        auto vi32  = hn::ConvertTo(d32, v_clamped);
        auto d_i16 = hn::Rebind<int16_t, D>();
        auto v_i16 = hn::DemoteTo(d_i16, vi32);
        auto d_u8  = hn::Rebind<uint8_t, D>();
        auto v_u8  = hn::DemoteTo(d_u8, v_i16);
        hn::StoreN(v_u8, d_u8, ptr, count);
    } else if constexpr (std::is_same_v<DstT, int8_t>) {
        VecD v_val     = (VecD)v;
        // Denormalize from 0-1 range to -128-127 range
        VecD v_denorm  = hn::Mul(v_val, hn::Set(d, (MathT)255.0));
        VecD v_shifted = hn::Sub(v_denorm, hn::Set(d, (MathT)128.0));
        VecD v_rounded = hn::Add(v_shifted, hn::Set(d, (MathT)0.5));
        VecD v_min     = hn::Set(d, (MathT)-128.0);
        VecD v_max     = hn::Set(d, (MathT)127.0);
        VecD v_clamped = hn::Max(v_rounded, v_min);
        v_clamped      = hn::Min(v_clamped, v_max);

        auto d32   = hn::Rebind<int32_t, D>();
        auto vi32  = hn::ConvertTo(d32, v_clamped);
        auto d_i16 = hn::Rebind<int16_t, D>();
        auto v_i16 = hn::DemoteTo(d_i16, vi32);
        auto d_i8  = hn::Rebind<int8_t, D>();
        auto v_i8  = hn::DemoteTo(d_i8, v_i16);
        hn::StoreN(v_i8, d_i8, ptr, count);
    } else if constexpr (std::is_same_v<DstT, uint16_t>) {
        VecD v_val     = (VecD)v;
        // Denormalize from 0-1 range to 0-65535 range
        VecD v_denorm  = hn::Mul(v_val, hn::Set(d, (MathT)65535.0));
        VecD v_rounded = hn::Add(v_denorm, hn::Set(d, (MathT)0.5));
        VecD v_zero    = hn::Zero(d);
        VecD v_max     = hn::Set(d, (MathT)65535.0);
        VecD v_clamped = hn::Max(v_rounded, v_zero);
        v_clamped      = hn::Min(v_clamped, v_max);

        auto d32   = hn::Rebind<int32_t, D>();
        auto vi32  = hn::ConvertTo(d32, v_clamped);
        auto d_u16 = hn::Rebind<uint16_t, D>();
        auto v_u16 = hn::DemoteTo(d_u16, vi32);
        hn::StoreN(v_u16, d_u16, ptr, count);
    } else if constexpr (std::is_same_v<DstT, int16_t>) {
        VecD v_val     = (VecD)v;
        // Denormalize from 0-1 range to -32768-32767 range
        VecD v_denorm  = hn::Mul(v_val, hn::Set(d, (MathT)65535.0));
        VecD v_shifted = hn::Sub(v_denorm, hn::Set(d, (MathT)32768.0));
        VecD v_rounded = hn::Add(v_shifted, hn::Set(d, (MathT)0.5));
        VecD v_min     = hn::Set(d, (MathT)-32768.0);
        VecD v_max     = hn::Set(d, (MathT)32767.0);
        VecD v_clamped = hn::Max(v_rounded, v_min);
        v_clamped      = hn::Min(v_clamped, v_max);

        auto d32   = hn::Rebind<int32_t, D>();
        auto vi32  = hn::ConvertTo(d32, v_clamped);
        auto d_i16 = hn::Rebind<int16_t, D>();
        auto v_i16 = hn::DemoteTo(d_i16, vi32);
        hn::StoreN(v_i16, d_i16, ptr, count);
    } else if constexpr (std::is_same_v<DstT, uint32_t>) {
        // float -> uint32: Denormalize from 0-1 to 0-4294967295, round and convert
        VecD v_val     = (VecD)v;
        // Denormalize from 0-1 range to 0-4294967295 range
        VecD v_denorm  = hn::Mul(v_val, hn::Set(d, (MathT)4294967295.0));
        VecD v_rounded = hn::Add(v_denorm, hn::Set(d, (MathT)0.5));
        VecD v_zero    = hn::Zero(d);
        VecD v_max     = hn::Set(d, (MathT)4294967295.0);
        VecD v_clamped = hn::Max(v_rounded, v_zero);
        v_clamped      = hn::Min(v_clamped, v_max);

        auto d_u32 = hn::Rebind<uint32_t, D>();
        auto v_u32 = hn::ConvertTo(d_u32, v_clamped);
        hn::StoreN(v_u32, d_u32, ptr, count);
    } else if constexpr (std::is_same_v<DstT, int32_t>) {
        // float -> int32: Round and convert directly
        VecD v_val     = (VecD)v;
        VecD v_rounded = hn::Add(v_val, hn::Set(d, (MathT)0.5));
        VecD v_min     = hn::Set(d, (MathT)-2147483648.0);
        VecD v_max     = hn::Set(d, (MathT)2147483647.0);
        VecD v_clamped = hn::Max(v_rounded, v_min);
        v_clamped      = hn::Min(v_clamped, v_max);

        auto d_i32 = hn::Rebind<int32_t, D>();
        auto v_i32 = hn::ConvertTo(d_i32, v_clamped);
        hn::StoreN(v_i32, d_i32, ptr, count);
    } else if constexpr (std::is_same_v<DstT, uint64_t>) {
        // float -> uint64: Promote via uint32
        VecD v_val     = (VecD)v;
        VecD v_rounded = hn::Add(v_val, hn::Set(d, (MathT)0.5));
        VecD v_zero    = hn::Zero(d);
        VecD v_clamped = hn::Max(v_rounded, v_zero);

        auto d_u32 = hn::Rebind<uint32_t, D>();
        auto v_u32 = hn::ConvertTo(d_u32, v_clamped);
        auto d_u64 = hn::Rebind<uint64_t, D>();
        auto v_u64 = hn::PromoteTo(d_u64, v_u32);
        hn::StoreN(v_u64, d_u64, ptr, count);
    } else if constexpr (std::is_same_v<DstT, int64_t>) {
        // float -> int64: Promote via int32
        VecD v_val     = (VecD)v;
        VecD v_rounded = hn::Add(v_val, hn::Set(d, (MathT)0.5));

        auto d_i32 = hn::Rebind<int32_t, D>();
        auto v_i32 = hn::ConvertTo(d_i32, v_rounded);
        auto d_i64 = hn::Rebind<int64_t, D>();
        auto v_i64 = hn::PromoteTo(d_i64, v_i32);
        hn::StoreN(v_i64, d_i64, ptr, count);
    }
}

// -----------------------------------------------------------------------
// Native Integer Kernel Runners (No Type Conversion)
// -----------------------------------------------------------------------

/// Execute a unary SIMD operation on native integer arrays (no type promotion).
/// For scale-invariant operations like abs, where int_op(a) == denorm(float_op(norm(a))).
/// Much faster than promotion path - operates directly on integer SIMD vectors.
/// @param r Destination array (same type as source)
/// @param a Source array
/// @param n Number of elements to process
/// @param op Lambda/functor taking (descriptor, vector) and returning result vector
///           Example: [](auto d, auto va) { return hn::Abs(va); }
template <typename T, typename OpFunc>
inline void RunHwyUnaryNativeInt(T* r, const T* a, size_t n, OpFunc op) {
    const hn::ScalableTag<T> d;
    size_t x = 0;
    size_t lanes = hn::Lanes(d);
    for (; x + lanes <= n; x += lanes) {
        auto va = hn::Load(d, a + x);
        auto res = op(d, va);
        hn::Store(res, d, r + x);
    }
    size_t remaining = n - x;
    if (remaining > 0) {
        auto va = hn::LoadN(d, a + x, remaining);
        auto res = op(d, va);
        hn::StoreN(res, d, r + x, remaining);
    }
}

/// Execute a binary SIMD operation on native integer arrays (no type promotion).
/// For scale-invariant operations like saturated add, min, max, where:
/// int_op(a, b) == denorm(float_op(norm(a), norm(b))).
/// Much faster than promotion path - no conversion overhead.
/// @param r Destination array (same type as sources)
/// @param a First source array
/// @param b Second source array
/// @param n Number of elements to process
/// @param op Lambda/functor taking (descriptor, vector_a, vector_b) and returning result
///           Example: [](auto d, auto va, auto vb) { return hn::SaturatedAdd(va, vb); }
template <typename T, typename OpFunc>
inline void RunHwyBinaryNativeInt(T* r, const T* a, const T* b, size_t n, OpFunc op) {
    const hn::ScalableTag<T> d;
    size_t x = 0;
    size_t lanes = hn::Lanes(d);
    for (; x + lanes <= n; x += lanes) {
        auto va = hn::Load(d, a + x);
        auto vb = hn::Load(d, b + x);
        auto res = op(d, va, vb);
        hn::Store(res, d, r + x);
    }
    size_t remaining = n - x;
    if (remaining > 0) {
        auto va = hn::LoadN(d, a + x, remaining);
        auto vb = hn::LoadN(d, b + x, remaining);
        auto res = op(d, va, vb);
        hn::StoreN(res, d, r + x, remaining);
    }
}

// -----------------------------------------------------------------------
// Generic Kernel Runners (With Type Conversion)
// -----------------------------------------------------------------------

/// Execute a unary SIMD operation on an array.
/// Processes array elements in SIMD batches, handling type promotion/demotion
/// and partial vectors at the end.
/// @param r Destination array
/// @param a Source array
/// @param n Number of elements to process
/// @param op Lambda/functor taking (descriptor, vector) and returning result vector
///           Example: [](auto d, auto va) { return hn::Sqrt(va); }
template <typename Rtype, typename Atype, typename OpFunc>
inline void RunHwyUnaryCmd(Rtype* r, const Atype* a, size_t n, OpFunc op) {
    using MathT = typename SimdMathType<Rtype>::type;
    const hn::ScalableTag<MathT> d;
    size_t x = 0;
    size_t lanes = hn::Lanes(d);
    for (; x + lanes <= n; x += lanes) {
        auto va = LoadPromote(d, a + x);
        auto res = op(d, va);
        DemoteStore(d, r + x, res);
    }
    size_t remaining = n - x;
    if (remaining > 0) {
        auto va = LoadPromoteN(d, a + x, remaining);
        auto res = op(d, va);
        DemoteStoreN(d, r + x, res, remaining);
    }
}

/// Execute a binary SIMD operation on two arrays.
/// Processes array elements in SIMD batches, handling type promotion/demotion
/// and partial vectors at the end.
/// @param r Destination array
/// @param a First source array
/// @param b Second source array
/// @param n Number of elements to process
/// @param op Lambda/functor taking (descriptor, vector_a, vector_b) and returning result
///           Example: [](auto d, auto va, auto vb) { return hn::Add(va, vb); }
template <typename Rtype, typename Atype, typename Btype, typename OpFunc>
inline void RunHwyCmd(Rtype* r, const Atype* a, const Btype* b, size_t n, OpFunc op) {
    using MathT = typename SimdMathType<Rtype>::type;
    const hn::ScalableTag<MathT> d;
    size_t x     = 0;
    size_t lanes = hn::Lanes(d);
    for (; x + lanes <= n; x += lanes) {
        auto va  = LoadPromote(d, a + x);
        auto vb  = LoadPromote(d, b + x);
        auto res = op(d, va, vb);
        DemoteStore(d, r + x, res);
    }
    size_t remaining = n - x;
    if (remaining > 0) {
        auto va  = LoadPromoteN(d, a + x, remaining);
        auto vb  = LoadPromoteN(d, b + x, remaining);
        auto res = op(d, va, vb);
        DemoteStoreN(d, r + x, res, remaining);
    }
}

/// Execute a ternary SIMD operation on three arrays.
/// Processes array elements in SIMD batches, handling type promotion/demotion
/// and partial vectors at the end.
/// @param r Destination array
/// @param a First source array
/// @param b Second source array
/// @param c Third source array
/// @param n Number of elements to process
/// @param op Lambda/functor taking (descriptor, vector_a, vector_b, vector_c) and returning result
///           Example: [](auto d, auto va, auto vb, auto vc) { return hn::MulAdd(va, vb, vc); }
template <typename Rtype, typename ABCtype, typename OpFunc>
inline void RunHwyTernaryCmd(Rtype* r, const ABCtype* a, const ABCtype* b, const ABCtype* c, size_t n, OpFunc op) {
    using MathT = typename SimdMathType<Rtype>::type;
    const hn::ScalableTag<MathT> d;
    size_t x     = 0;
    size_t lanes = hn::Lanes(d);
    for (; x + lanes <= n; x += lanes) {
        auto va  = LoadPromote(d, a + x);
        auto vb  = LoadPromote(d, b + x);
        auto vc  = LoadPromote(d, c + x);
        auto res = op(d, va, vb, vc);
        DemoteStore(d, r + x, res);
    }
    size_t remaining = n - x;
    if (remaining > 0) {
        auto va  = LoadPromoteN(d, a + x, remaining);
        auto vb  = LoadPromoteN(d, b + x, remaining);
        auto vc  = LoadPromoteN(d, c + x, remaining);
        auto res = op(d, va, vb, vc);
        DemoteStoreN(d, r + x, res, remaining);
    }
}

// -----------------------------------------------------------------------
// Interleaved Channel Load/Store Helpers
// -----------------------------------------------------------------------

/// Load 4 interleaved channels (RGBA) with type promotion.
/// For matching types, uses Highway's native LoadInterleaved4.
/// For type promotion, loads and manually deinterleaves.
/// @param d Highway descriptor tag for the target SIMD type
/// @param ptr Pointer to interleaved RGBA data (R0,G0,B0,A0,R1,G1,B1,A1,...)
/// @return Tuple of (R, G, B, A) SIMD vectors in promoted type
template<class D, typename SrcT>
inline auto
LoadInterleaved4Promote(D d, const SrcT* ptr)
{
    using MathT = typename D::T;
    using Vec   = hn::Vec<D>;

    if constexpr (std::is_same_v<SrcT, MathT>) {
        // No promotion needed - use Highway's optimized LoadInterleaved4
        Vec r, g, b, a;
        hn::LoadInterleaved4(d, ptr, r, g, b, a);
        return std::make_tuple(r, g, b, a);
    } else if constexpr (std::is_same_v<SrcT, half>) {
        // Special handling for half type - convert through hwy::float16_t
        using T16 = hwy::float16_t;
        auto d16  = hn::Rebind<T16, D>();

        // Load interleaved half data as float16_t
        hn::Vec<decltype(d16)> r16, g16, b16, a16;
        hn::LoadInterleaved4(d16, (const T16*)ptr, r16, g16, b16, a16);

        // Promote to computation type
        Vec r_vec = hn::PromoteTo(d, r16);
        Vec g_vec = hn::PromoteTo(d, g16);
        Vec b_vec = hn::PromoteTo(d, b16);
        Vec a_vec = hn::PromoteTo(d, a16);

        return std::make_tuple(r_vec, g_vec, b_vec, a_vec);
    } else {
        // Generic type promotion - deinterleave manually with normalization
        const size_t N = hn::Lanes(d);
        SrcT r_src[hn::MaxLanes(d)];
        SrcT g_src[hn::MaxLanes(d)];
        SrcT b_src[hn::MaxLanes(d)];
        SrcT a_src[hn::MaxLanes(d)];

        for (size_t i = 0; i < N; ++i) {
            r_src[i] = ptr[i * 4 + 0];
            g_src[i] = ptr[i * 4 + 1];
            b_src[i] = ptr[i * 4 + 2];
            a_src[i] = ptr[i * 4 + 3];
        }

        // Use LoadPromote for proper normalization of integer types
        auto r_vec = LoadPromote(d, r_src);
        auto g_vec = LoadPromote(d, g_src);
        auto b_vec = LoadPromote(d, b_src);
        auto a_vec = LoadPromote(d, a_src);

        return std::make_tuple(r_vec, g_vec, b_vec, a_vec);
    }
}

/// Store 4 interleaved channels (RGBA) with type demotion.
/// For matching types, uses Highway's native StoreInterleaved4.
/// For type demotion, manually interleaves and stores.
/// @param d Highway descriptor tag for the source SIMD type
/// @param ptr Pointer to destination interleaved RGBA data
/// @param r Red channel SIMD vector
/// @param g Green channel SIMD vector
/// @param b Blue channel SIMD vector
/// @param a Alpha channel SIMD vector
template<class D, typename DstT, typename VecT>
inline void
StoreInterleaved4Demote(D d, DstT* ptr, VecT r, VecT g, VecT b, VecT a)
{
    using MathT = typename D::T;

    if constexpr (std::is_same_v<DstT, MathT>) {
        // No demotion needed - use Highway's optimized StoreInterleaved4
        hn::StoreInterleaved4(r, g, b, a, d, ptr);
    } else if constexpr (std::is_same_v<DstT, half>) {
        // Special handling for half type - convert through hwy::float16_t
        using T16 = hwy::float16_t;
        auto d16  = hn::Rebind<T16, D>();

        // Demote to float16_t
        auto r16 = hn::DemoteTo(d16, r);
        auto g16 = hn::DemoteTo(d16, g);
        auto b16 = hn::DemoteTo(d16, b);
        auto a16 = hn::DemoteTo(d16, a);

        // Store interleaved float16_t data
        hn::StoreInterleaved4(r16, g16, b16, a16, d16, (T16*)ptr);
    } else {
        // Generic type demotion - use DemoteStore for each channel then interleave
        const size_t N = hn::Lanes(d);

        // Temporary arrays for demoted values
        DstT r_demoted[hn::MaxLanes(d)];
        DstT g_demoted[hn::MaxLanes(d)];
        DstT b_demoted[hn::MaxLanes(d)];
        DstT a_demoted[hn::MaxLanes(d)];

        // Use DemoteStoreN to properly denormalize integer types
        DemoteStoreN(d, r_demoted, r, N);
        DemoteStoreN(d, g_demoted, g, N);
        DemoteStoreN(d, b_demoted, b, N);
        DemoteStoreN(d, a_demoted, a, N);

        // Interleave the demoted values
        for (size_t i = 0; i < N; ++i) {
            ptr[i * 4 + 0] = r_demoted[i];
            ptr[i * 4 + 1] = g_demoted[i];
            ptr[i * 4 + 2] = b_demoted[i];
            ptr[i * 4 + 3] = a_demoted[i];
        }
    }
}

// -----------------------------------------------------------------------
// Rangecompress/Rangeexpand SIMD Kernels
// -----------------------------------------------------------------------

/// Apply rangecompress formula to a SIMD vector.
/// Formula (courtesy Sony Pictures Imageworks):
///   if (|x| <= 0.18) return x
///   else return copysign(a + b * log(c * |x| + 1), x)
/// where a = -0.545768857, b = 0.183516696, c = 284.357788
/// @param d Highway descriptor tag
/// @param x Input SIMD vector
/// @return Compressed SIMD vector
template<class D, typename VecT>
inline auto
rangecompress_simd(D d, VecT x)
{
    using T = typename D::T;

    // Constants from Sony Pictures Imageworks
    constexpr T x1 = static_cast<T>(0.18);
    constexpr T a  = static_cast<T>(-0.54576885700225830078);
    constexpr T b  = static_cast<T>(0.18351669609546661377);
    constexpr T c  = static_cast<T>(284.3577880859375);

    auto abs_x            = hn::Abs(x);
    auto mask_passthrough = hn::Le(abs_x, hn::Set(d, x1));

    // compressed = a + b * log(c * |x| + 1.0)
    auto c_vec = hn::Set(d, c);
    auto one   = hn::Set(d, static_cast<T>(1.0));
    auto temp  = hn::MulAdd(c_vec, abs_x, one);  // c * |x| + 1.0
    auto log_val     = hn::Log(d, temp);
    auto b_vec       = hn::Set(d, b);
    auto a_vec       = hn::Set(d, a);
    auto compressed  = hn::MulAdd(b_vec, log_val, a_vec);  // a + b * log

    // Apply sign of original x
    auto result = hn::CopySign(compressed, x);

    // If |x| <= x1, return x; else return compressed
    return hn::IfThenElse(mask_passthrough, x, result);
}

/// Apply rangeexpand formula to a SIMD vector (inverse of rangecompress).
/// Formula:
///   if (|y| <= 0.18) return y
///   else x = exp((|y| - a) / b); x = (x - 1) / c
///        if x < 0.18 then x = (-x_intermediate - 1) / c
///        return copysign(x, y)
/// @param d Highway descriptor tag
/// @param y Input SIMD vector (compressed values)
/// @return Expanded SIMD vector
template<class D, typename VecT>
inline auto
rangeexpand_simd(D d, VecT y)
{
    using T = typename D::T;

    // Constants (same as rangecompress)
    constexpr T x1 = static_cast<T>(0.18);
    constexpr T a  = static_cast<T>(-0.54576885700225830078);
    constexpr T b  = static_cast<T>(0.18351669609546661377);
    constexpr T c  = static_cast<T>(284.3577880859375);

    auto abs_y            = hn::Abs(y);
    auto mask_passthrough = hn::Le(abs_y, hn::Set(d, x1));

    // x_intermediate = exp((|y| - a) / b)
    auto a_vec        = hn::Set(d, a);
    auto b_vec        = hn::Set(d, b);
    auto intermediate = hn::Div(hn::Sub(abs_y, a_vec), b_vec);  // (|y| - a) / b
    auto x_intermediate = hn::Exp(d, intermediate);

    // x = (x_intermediate - 1.0) / c
    auto one   = hn::Set(d, static_cast<T>(1.0));
    auto c_vec = hn::Set(d, c);
    auto x     = hn::Div(hn::Sub(x_intermediate, one), c_vec);

    // If x < x1, use alternate solution: (-x_intermediate - 1.0) / c
    auto mask_alternate = hn::Lt(x, hn::Set(d, x1));
    auto x_alternate    = hn::Div(hn::Sub(hn::Neg(x_intermediate), one), c_vec);
    x                   = hn::IfThenElse(mask_alternate, x_alternate, x);

    // Apply sign of input y
    auto result = hn::CopySign(x, y);

    return hn::IfThenElse(mask_passthrough, y, result);
}

OIIO_NAMESPACE_END
