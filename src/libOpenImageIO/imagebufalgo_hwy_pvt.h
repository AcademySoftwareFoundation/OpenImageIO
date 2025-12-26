// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <OpenImageIO/half.h>
#include <OpenImageIO/imageio.h>
#include <algorithm>
#include <cstddef>
#include <hwy/highway.h>
#include <type_traits>

OIIO_NAMESPACE_BEGIN

namespace hn = hwy::HWY_NAMESPACE;

// -----------------------------------------------------------------------
// Type Traits
// -----------------------------------------------------------------------
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
template<class D, typename SrcT>
HWY_INLINE auto
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
    } else if constexpr (std::is_same_v<SrcT, uint32_t>) {
        // u32 -> double
        auto d_u32 = hn::Rebind<uint32_t, D>();
        auto v_u32 = hn::Load(d_u32, ptr);
        auto d_u64 = hn::Rebind<uint64_t, D>();
        auto v_u64 = hn::PromoteTo(d_u64, v_u32);
        return hn::ConvertTo(d, v_u64);
    } else {
        return hn::Zero(d);
    }
}

template<class D, typename SrcT>
HWY_INLINE auto
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
template<class D, typename DstT, typename VecT>
HWY_INLINE void
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

template<class D, typename DstT, typename VecT>
HWY_INLINE void
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
// Generic Kernel Runner
// -----------------------------------------------------------------------
template<typename Rtype, typename Atype, typename Btype, typename OpFunc>
HWY_INLINE void
RunHwyCmd(Rtype* r, const Atype* a, const Btype* b, size_t n, OpFunc op)
{
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

    // Tail
    size_t remaining = n - x;
    if (remaining > 0) {
        auto va  = LoadPromoteN(d, a + x, remaining);
        auto vb  = LoadPromoteN(d, b + x, remaining);
        auto res = op(d, va, vb);
        DemoteStoreN(d, r + x, res, remaining);
    }
}

OIIO_NAMESPACE_END
