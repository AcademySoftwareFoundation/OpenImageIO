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
/// Promotes smaller types to float, keeps double as double, and uses
/// double for uint32_t to avoid precision loss.
template<typename T> struct SimdMathType {
    using type = float;
};
template<> struct SimdMathType<double> {
    using type = double;
};
template<> struct SimdMathType<uint32_t> {
    using type = double;
};

// -----------------------------------------------------------------------
// Load and Promote
// -----------------------------------------------------------------------

/// Load and promote source data to target SIMD type.
/// Handles type conversions from various source formats (uint8_t, uint16_t,
/// int16_t, uint32_t, half, float, double) to the target SIMD computation type.
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
        return hn::ConvertTo(
            d, hn::PromoteTo(hn::Rebind<int32_t, D>(),
                             hn::PromoteTo(hn::Rebind<int16_t, D>(), v_u8)));
    } else if constexpr (std::is_same_v<SrcT, uint16_t>) {
        auto d_u16 = hn::Rebind<uint16_t, D>();
        auto v_u16 = hn::Load(d_u16, ptr);
        return hn::ConvertTo(d, hn::PromoteTo(hn::Rebind<int32_t, D>(), v_u16));
    } else if constexpr (std::is_same_v<SrcT, int16_t>) {
        auto d_i16 = hn::Rebind<int16_t, D>();
        auto v_i16 = hn::Load(d_i16, ptr);
        return hn::ConvertTo(d, hn::PromoteTo(hn::Rebind<int32_t, D>(), v_i16));
    } else if constexpr (std::is_same_v<SrcT, uint32_t>) {
        auto d_u32 = hn::Rebind<uint32_t, D>();
        auto v_u32 = hn::Load(d_u32, ptr);
        auto d_u64 = hn::Rebind<uint64_t, D>();
        auto v_u64 = hn::PromoteTo(d_u64, v_u32);
        return hn::ConvertTo(d, v_u64);
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
        return hn::ConvertTo(
            d, hn::PromoteTo(hn::Rebind<int32_t, D>(),
                             hn::PromoteTo(hn::Rebind<int16_t, D>(), v_u8)));
    } else if constexpr (std::is_same_v<SrcT, uint16_t>) {
        auto d_u16 = hn::Rebind<uint16_t, D>();
        auto v_u16 = hn::LoadN(d_u16, ptr, count);
        return hn::ConvertTo(d, hn::PromoteTo(hn::Rebind<int32_t, D>(), v_u16));
    } else if constexpr (std::is_same_v<SrcT, uint32_t>) {
        auto d_u32 = hn::Rebind<uint32_t, D>();
        auto v_u32 = hn::LoadN(d_u32, ptr, count);
        auto d_u64 = hn::Rebind<uint64_t, D>();
        auto v_u64 = hn::PromoteTo(d_u64, v_u32);
        return hn::ConvertTo(d, v_u64);
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
        VecD v_rounded = hn::Add(v_val, hn::Set(d, (MathT)0.5));
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
    } else if constexpr (std::is_same_v<DstT, uint16_t>) {
        VecD v_val     = (VecD)v;
        VecD v_rounded = hn::Add(v_val, hn::Set(d, (MathT)0.5));
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
        VecD v_rounded = hn::Add(v_val, hn::Set(d, (MathT)0.5));
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
        VecD v_val     = (VecD)v;
        VecD v_rounded = hn::Add(v_val, hn::Set(d, (MathT)0.5));
        // double -> u32
        auto d_u64 = hn::Rebind<uint64_t, D>();
        auto v_u64 = hn::ConvertTo(d_u64, v_rounded);
        auto d_u32 = hn::Rebind<uint32_t, D>();
        auto v_u32 = hn::DemoteTo(d_u32, v_u64);
        hn::Store(v_u32, d_u32, ptr);
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
        VecD v_rounded = hn::Add(v_val, hn::Set(d, (MathT)0.5));
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
    } else if constexpr (std::is_same_v<DstT, uint16_t>) {
        VecD v_val     = (VecD)v;
        VecD v_rounded = hn::Add(v_val, hn::Set(d, (MathT)0.5));
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
        VecD v_rounded = hn::Add(v_val, hn::Set(d, (MathT)0.5));
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
        VecD v_val     = (VecD)v;
        VecD v_rounded = hn::Add(v_val, hn::Set(d, (MathT)0.5));
        auto d_u64     = hn::Rebind<uint64_t, D>();
        auto v_u64     = hn::ConvertTo(d_u64, v_rounded);
        auto d_u32     = hn::Rebind<uint32_t, D>();
        auto v_u32     = hn::DemoteTo(d_u32, v_u64);
        hn::StoreN(v_u32, d_u32, ptr, count);
    }
}

// -----------------------------------------------------------------------
// Generic Kernel Runners
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

OIIO_NAMESPACE_END
