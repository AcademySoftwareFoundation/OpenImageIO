// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

/// \file
/// Implementation of ImageBufAlgo algorithms that do math on
/// single pixels at a time.

#include <cmath>
#include <iostream>
#include <limits>

#include <OpenImageIO/half.h>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/deepdata.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>

#if defined(OIIO_USE_HWY) && OIIO_USE_HWY
#    include "imagebufalgo_hwy_pvt.h"
#endif

#include "imageio_pvt.h"


OIIO_NAMESPACE_3_1_BEGIN


template<class Rtype, class Atype, class Btype>
static bool
add_impl_scalar(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, ROI roi,
                int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::Iterator<Rtype> r(R, roi);
        ImageBuf::ConstIterator<Atype> a(A, roi);
        ImageBuf::ConstIterator<Btype> b(B, roi);
        for (; !r.done(); ++r, ++a, ++b)
            for (int c = roi.chbegin; c < roi.chend; ++c)
                r[c] = a[c] + b[c];
    });
    return true;
}



template<class Rtype, class Atype>
static bool
add_impl_scalar(ImageBuf& R, const ImageBuf& A, cspan<float> b, ROI roi,
                int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::Iterator<Rtype> r(R, roi);
        ImageBuf::ConstIterator<Atype> a(A, roi);
        for (; !r.done(); ++r, ++a)
            for (int c = roi.chbegin; c < roi.chend; ++c)
                r[c] = a[c] + b[c];
    });
    return true;
}



#if OIIO_USE_HWY

// Native integer add using SaturatedAdd (scale-invariant, no float conversion)
template<class T>
static bool
add_impl_hwy_native_int(ImageBuf& R, const ImageBuf& A, const ImageBuf& B,
                        ROI roi, int nthreads)
{
    return hwy_binary_native_int_perpixel_op<T>(R, A, B, roi, nthreads,
                                               [](auto /*d*/, auto a, auto b) {
                                                   return hn::SaturatedAdd(a, b);
                                               });
}

template<class Rtype, class Atype, class Btype>
static bool
add_impl_hwy(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, ROI roi,
             int nthreads)
{
    auto op = [](auto /*d*/, auto a, auto b) {
        return hn::Add(a, b);
    };

    // Special-case: RGBA images but ROI is RGB (strided channel subset). We
    // still can SIMD the RGB channels by processing full RGBA and preserving
    // alpha exactly (bitwise) from the destination.
    if (roi.chbegin == 0 && roi.chend == 3) {
        // Only support same-type float/half/double in this fast path.
        constexpr bool floaty = (std::is_same_v<Rtype, float>
                                 || std::is_same_v<Rtype, double>
                                 || std::is_same_v<Rtype, half>)
                                && std::is_same_v<Rtype, Atype>
                                && std::is_same_v<Rtype, Btype>;
        if constexpr (floaty) {
            auto Rv = HwyPixels(R);
            auto Av = HwyPixels(A);
            auto Bv = HwyPixels(B);
            if (Rv.nchannels >= 4 && Av.nchannels >= 4 && Bv.nchannels >= 4
                && ChannelsContiguous<Rtype>(Rv, 4)
                && ChannelsContiguous<Atype>(Av, 4)
                && ChannelsContiguous<Btype>(Bv, 4)) {
                ROI roi4     = roi;
                roi4.chbegin = 0;
                roi4.chend   = 4;
                using MathT  = typename SimdMathType<Rtype>::type;
                const hn::ScalableTag<MathT> d;
                const size_t lanes = hn::Lanes(d);
                ImageBufAlgo::parallel_image(roi4, nthreads, [&](ROI roi4) {
                    for (int y = roi4.ybegin; y < roi4.yend; ++y) {
                        Rtype* r_row       = RoiRowPtr<Rtype>(Rv, y, roi4);
                        const Atype* a_row = RoiRowPtr<Atype>(Av, y, roi4);
                        const Btype* b_row = RoiRowPtr<Btype>(Bv, y, roi4);
                        const size_t npixels = static_cast<size_t>(roi4.width());

                        size_t x = 0;
                        for (; x + lanes <= npixels; x += lanes) {
                            const size_t off = x * 4;
                            if constexpr (std::is_same_v<Rtype, half>) {
                                using T16  = hwy::float16_t;
                                auto d16   = hn::Rebind<T16, decltype(d)>();
                                const T16* a16
                                    = reinterpret_cast<const T16*>(a_row + off);
                                const T16* b16
                                    = reinterpret_cast<const T16*>(b_row + off);
                                T16* r16 = reinterpret_cast<T16*>(r_row + off);

                                hn::Vec<decltype(d16)> ar16, ag16, ab16, aa16;
                                hn::Vec<decltype(d16)> br16, bg16, bb16, ba16;
                                hn::Vec<decltype(d16)> dr16, dg16, db16, da16;
                                hn::LoadInterleaved4(d16, a16, ar16, ag16, ab16,
                                                     aa16);
                                hn::LoadInterleaved4(d16, b16, br16, bg16, bb16,
                                                     ba16);
                                hn::LoadInterleaved4(d16, r16, dr16, dg16, db16,
                                                     da16);
                                (void)aa16;
                                (void)ba16;
                                (void)dr16;
                                (void)dg16;
                                (void)db16;

                                auto rr = op(d, hn::PromoteTo(d, ar16),
                                             hn::PromoteTo(d, br16));
                                auto rg = op(d, hn::PromoteTo(d, ag16),
                                             hn::PromoteTo(d, bg16));
                                auto rb = op(d, hn::PromoteTo(d, ab16),
                                             hn::PromoteTo(d, bb16));

                                auto rr16 = hn::DemoteTo(d16, rr);
                                auto rg16 = hn::DemoteTo(d16, rg);
                                auto rb16 = hn::DemoteTo(d16, rb);
                                hn::StoreInterleaved4(rr16, rg16, rb16, da16, d16,
                                                      r16);
                            } else {
                                hn::Vec<decltype(d)> ar, ag, ab, aa;
                                hn::Vec<decltype(d)> br, bg, bb, ba;
                                hn::Vec<decltype(d)> dr, dg, db, da;
                                hn::LoadInterleaved4(d, a_row + off, ar, ag, ab,
                                                     aa);
                                hn::LoadInterleaved4(d, b_row + off, br, bg, bb,
                                                     ba);
                                hn::LoadInterleaved4(d, r_row + off, dr, dg, db,
                                                     da);
                                (void)aa;
                                (void)ba;
                                (void)dr;
                                (void)dg;
                                (void)db;

                                auto rr = op(d, ar, br);
                                auto rg = op(d, ag, bg);
                                auto rb = op(d, ab, bb);
                                hn::StoreInterleaved4(rr, rg, rb, da, d,
                                                      r_row + off);
                            }
                        }

                        for (; x < npixels; ++x) {
                            const size_t off = x * 4;
                            if constexpr (std::is_same_v<Rtype, half>) {
                                r_row[off + 0]
                                    = half((float)a_row[off + 0]
                                           + (float)b_row[off + 0]);
                                r_row[off + 1]
                                    = half((float)a_row[off + 1]
                                           + (float)b_row[off + 1]);
                                r_row[off + 2]
                                    = half((float)a_row[off + 2]
                                           + (float)b_row[off + 2]);
                            } else {
                                r_row[off + 0] = a_row[off + 0] + b_row[off + 0];
                                r_row[off + 1] = a_row[off + 1] + b_row[off + 1];
                                r_row[off + 2] = a_row[off + 2] + b_row[off + 2];
                            }
                            // Preserve alpha (off+3).
                        }
                    }
                });
                return true;
            }
        }
    }

    return hwy_binary_perpixel_op<Rtype, Atype, Btype>(R, A, B, roi, nthreads,
                                                       op);
}

template<class Rtype, class Atype>
static bool
add_impl_hwy(ImageBuf& R, const ImageBuf& A, cspan<float> b, ROI roi,
             int nthreads)
{
    using SimdType
        = std::conditional_t<std::is_same_v<Rtype, double>, double, float>;
    auto Rv = HwyPixels(R);
    auto Av = HwyPixels(A);
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        for (int y = roi.ybegin; y < roi.yend; ++y) {
            std::byte* r_row       = PixelBase(Rv, roi.xbegin, y);
            const std::byte* a_row = PixelBase(Av, roi.xbegin, y);
            for (int x = roi.xbegin; x < roi.xend; ++x) {
                const size_t xoff = static_cast<size_t>(x - roi.xbegin);
                Rtype* r_ptr      = reinterpret_cast<Rtype*>(
                    r_row + xoff * Rv.pixel_bytes);
                const Atype* a_ptr = reinterpret_cast<const Atype*>(
                    a_row + xoff * Av.pixel_bytes);
                for (int c = roi.chbegin; c < roi.chend; ++c) {
                    r_ptr[c] = (Rtype)((SimdType)a_ptr[c] + (SimdType)b[c]);
                }
            }
        }
    });
    return true;
}
#endif  // defined(OIIO_USE_HWY) && OIIO_USE_HWY

template<class Rtype, class Atype, class Btype>
static bool
add_impl(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, ROI roi,
         int nthreads)
{
#if defined(OIIO_USE_HWY) && OIIO_USE_HWY
    if (OIIO::pvt::enable_hwy && R.localpixels() && A.localpixels()
        && B.localpixels()) {
        auto Rv = HwyPixels(R);
        auto Av = HwyPixels(A);
        auto Bv = HwyPixels(B);
        const int nchannels = RoiNChannels(roi);
        const bool contig   = ChannelsContiguous<Rtype>(Rv, nchannels)
                            && ChannelsContiguous<Atype>(Av, nchannels)
                            && ChannelsContiguous<Btype>(Bv, nchannels);
        if (contig) {
            // Use native integer path for scale-invariant add when all types
            // match and are integer types (much faster: 6-12x vs 3-5x with
            // float conversion).
            constexpr bool all_same = std::is_same_v<Rtype, Atype>
                                      && std::is_same_v<Atype, Btype>;
            constexpr bool is_integer = std::is_integral_v<Rtype>;
            if constexpr (all_same && is_integer)
                return add_impl_hwy_native_int<Rtype>(R, A, B, roi, nthreads);
            return add_impl_hwy<Rtype, Atype, Btype>(R, A, B, roi, nthreads);
        }

        // Handle the common RGBA + RGB ROI strided case (preserving alpha).
        constexpr bool floaty_strided = (std::is_same_v<Rtype, float>
                                         || std::is_same_v<Rtype, double>
                                         || std::is_same_v<Rtype, half>)
                                        && std::is_same_v<Rtype, Atype>
                                        && std::is_same_v<Rtype, Btype>;
        if constexpr (floaty_strided) {
            if (roi.chbegin == 0 && roi.chend == 3) {
                const bool contig4 = (Rv.nchannels >= 4 && Av.nchannels >= 4
                                      && Bv.nchannels >= 4)
                                     && ChannelsContiguous<Rtype>(Rv, 4)
                                     && ChannelsContiguous<Atype>(Av, 4)
                                     && ChannelsContiguous<Btype>(Bv, 4);
                if (contig4)
                    return add_impl_hwy<Rtype, Atype, Btype>(R, A, B, roi,
                                                             nthreads);
            }
        }
    }
#endif
    return add_impl_scalar<Rtype, Atype, Btype>(R, A, B, roi, nthreads);
}

template<class Rtype, class Atype>
static bool
add_impl(ImageBuf& R, const ImageBuf& A, cspan<float> b, ROI roi, int nthreads)
{
#if defined(OIIO_USE_HWY) && OIIO_USE_HWY
    if (OIIO::pvt::enable_hwy && R.localpixels() && A.localpixels())
        return add_impl_hwy<Rtype, Atype>(R, A, b, roi, nthreads);
#endif
    return add_impl_scalar<Rtype, Atype>(R, A, b, roi, nthreads);
}

#if defined(OIIO_USE_HWY) && OIIO_USE_HWY
// Native integer sub using SaturatedSub (scale-invariant, no float conversion)
template<class T>
static bool
sub_impl_hwy_native_int(ImageBuf& R, const ImageBuf& A, const ImageBuf& B,
                        ROI roi, int nthreads)
{
    return hwy_binary_native_int_perpixel_op<T>(R, A, B, roi, nthreads,
                                               [](auto /*d*/, auto a, auto b) {
                                                   return hn::SaturatedSub(a, b);
                                               });
}

template<class Rtype, class Atype, class Btype>
static bool
sub_impl_hwy(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, ROI roi,
             int nthreads)
{
    auto op = [](auto /*d*/, auto a, auto b) {
        return hn::Sub(a, b);
    };

    // Special-case: RGBA images but ROI is RGB (strided channel subset). We
    // still can SIMD the RGB channels by processing full RGBA and preserving
    // alpha exactly (bitwise) from the destination.
    if (roi.chbegin == 0 && roi.chend == 3) {
        // Only support same-type float/half/double in this fast path.
        constexpr bool floaty = (std::is_same_v<Rtype, float>
                                 || std::is_same_v<Rtype, double>
                                 || std::is_same_v<Rtype, half>)
                                && std::is_same_v<Rtype, Atype>
                                && std::is_same_v<Rtype, Btype>;
        if constexpr (floaty) {
            auto Rv = HwyPixels(R);
            auto Av = HwyPixels(A);
            auto Bv = HwyPixels(B);
            if (Rv.nchannels >= 4 && Av.nchannels >= 4 && Bv.nchannels >= 4
                && ChannelsContiguous<Rtype>(Rv, 4)
                && ChannelsContiguous<Atype>(Av, 4)
                && ChannelsContiguous<Btype>(Bv, 4)) {
                ROI roi4     = roi;
                roi4.chbegin = 0;
                roi4.chend   = 4;
                using MathT  = typename SimdMathType<Rtype>::type;
                const hn::ScalableTag<MathT> d;
                const size_t lanes = hn::Lanes(d);
                ImageBufAlgo::parallel_image(roi4, nthreads, [&](ROI roi4) {
                    for (int y = roi4.ybegin; y < roi4.yend; ++y) {
                        Rtype* r_row       = RoiRowPtr<Rtype>(Rv, y, roi4);
                        const Atype* a_row = RoiRowPtr<Atype>(Av, y, roi4);
                        const Btype* b_row = RoiRowPtr<Btype>(Bv, y, roi4);
                        const size_t npixels = static_cast<size_t>(roi4.width());

                        size_t x = 0;
                        for (; x + lanes <= npixels; x += lanes) {
                            const size_t off = x * 4;
                            if constexpr (std::is_same_v<Rtype, half>) {
                                using T16  = hwy::float16_t;
                                auto d16   = hn::Rebind<T16, decltype(d)>();
                                const T16* a16
                                    = reinterpret_cast<const T16*>(a_row + off);
                                const T16* b16
                                    = reinterpret_cast<const T16*>(b_row + off);
                                T16* r16 = reinterpret_cast<T16*>(r_row + off);

                                hn::Vec<decltype(d16)> ar16, ag16, ab16, aa16;
                                hn::Vec<decltype(d16)> br16, bg16, bb16, ba16;
                                hn::Vec<decltype(d16)> dr16, dg16, db16, da16;
                                hn::LoadInterleaved4(d16, a16, ar16, ag16, ab16,
                                                     aa16);
                                hn::LoadInterleaved4(d16, b16, br16, bg16, bb16,
                                                     ba16);
                                hn::LoadInterleaved4(d16, r16, dr16, dg16, db16,
                                                     da16);
                                (void)aa16;
                                (void)ba16;
                                (void)dr16;
                                (void)dg16;
                                (void)db16;

                                auto rr = op(d, hn::PromoteTo(d, ar16),
                                             hn::PromoteTo(d, br16));
                                auto rg = op(d, hn::PromoteTo(d, ag16),
                                             hn::PromoteTo(d, bg16));
                                auto rb = op(d, hn::PromoteTo(d, ab16),
                                             hn::PromoteTo(d, bb16));

                                auto rr16 = hn::DemoteTo(d16, rr);
                                auto rg16 = hn::DemoteTo(d16, rg);
                                auto rb16 = hn::DemoteTo(d16, rb);
                                hn::StoreInterleaved4(rr16, rg16, rb16, da16, d16,
                                                      r16);
                            } else {
                                hn::Vec<decltype(d)> ar, ag, ab, aa;
                                hn::Vec<decltype(d)> br, bg, bb, ba;
                                hn::Vec<decltype(d)> dr, dg, db, da;
                                hn::LoadInterleaved4(d, a_row + off, ar, ag, ab,
                                                     aa);
                                hn::LoadInterleaved4(d, b_row + off, br, bg, bb,
                                                     ba);
                                hn::LoadInterleaved4(d, r_row + off, dr, dg, db,
                                                     da);
                                (void)aa;
                                (void)ba;
                                (void)dr;
                                (void)dg;
                                (void)db;

                                auto rr = op(d, ar, br);
                                auto rg = op(d, ag, bg);
                                auto rb = op(d, ab, bb);
                                hn::StoreInterleaved4(rr, rg, rb, da, d,
                                                      r_row + off);
                            }
                        }

                        for (; x < npixels; ++x) {
                            const size_t off = x * 4;
                            if constexpr (std::is_same_v<Rtype, half>) {
                                r_row[off + 0]
                                    = half((float)a_row[off + 0]
                                           - (float)b_row[off + 0]);
                                r_row[off + 1]
                                    = half((float)a_row[off + 1]
                                           - (float)b_row[off + 1]);
                                r_row[off + 2]
                                    = half((float)a_row[off + 2]
                                           - (float)b_row[off + 2]);
                            } else {
                                r_row[off + 0] = a_row[off + 0] - b_row[off + 0];
                                r_row[off + 1] = a_row[off + 1] - b_row[off + 1];
                                r_row[off + 2] = a_row[off + 2] - b_row[off + 2];
                            }
                            // Preserve alpha (off+3).
                        }
                    }
                });
                return true;
            }
        }
    }

    return hwy_binary_perpixel_op<Rtype, Atype, Btype>(R, A, B, roi, nthreads,
                                                       op);
}
#endif  // defined(OIIO_USE_HWY) && OIIO_USE_HWY

template<class Rtype, class Atype, class Btype>
static bool
sub_impl(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, ROI roi,
         int nthreads)
{
#if defined(OIIO_USE_HWY) && OIIO_USE_HWY
    if (OIIO::pvt::enable_hwy && R.localpixels() && A.localpixels()
        && B.localpixels()) {
        auto Rv = HwyPixels(R);
        auto Av = HwyPixels(A);
        auto Bv = HwyPixels(B);
        const int nchannels = RoiNChannels(roi);
        const bool contig   = ChannelsContiguous<Rtype>(Rv, nchannels)
                            && ChannelsContiguous<Atype>(Av, nchannels)
                            && ChannelsContiguous<Btype>(Bv, nchannels);
        if (contig) {
            // Use native integer path for scale-invariant sub when all types
            // match and are integer types (much faster: 6-12x vs 3-5x with
            // float conversion).
            constexpr bool all_same = std::is_same_v<Rtype, Atype>
                                      && std::is_same_v<Atype, Btype>;
            constexpr bool is_integer = std::is_integral_v<Rtype>;
            if constexpr (all_same && is_integer)
                return sub_impl_hwy_native_int<Rtype>(R, A, B, roi, nthreads);
            return sub_impl_hwy<Rtype, Atype, Btype>(R, A, B, roi, nthreads);
        }

        // Handle the common RGBA + RGB ROI strided case (preserving alpha).
        constexpr bool floaty_strided = (std::is_same_v<Rtype, float>
                                         || std::is_same_v<Rtype, double>
                                         || std::is_same_v<Rtype, half>)
                                        && std::is_same_v<Rtype, Atype>
                                        && std::is_same_v<Rtype, Btype>;
        if constexpr (floaty_strided) {
            if (roi.chbegin == 0 && roi.chend == 3) {
                const bool contig4 = (Rv.nchannels >= 4 && Av.nchannels >= 4
                                      && Bv.nchannels >= 4)
                                     && ChannelsContiguous<Rtype>(Rv, 4)
                                     && ChannelsContiguous<Atype>(Av, 4)
                                     && ChannelsContiguous<Btype>(Bv, 4);
                if (contig4)
                    return sub_impl_hwy<Rtype, Atype, Btype>(R, A, B, roi,
                                                             nthreads);
            }
        }
    }
#endif
    return sub_impl_scalar<Rtype, Atype, Btype>(R, A, B, roi, nthreads);
}

static bool
add_impl_deep(ImageBuf& R, const ImageBuf& A, cspan<float> b, ROI roi,
              int nthreads)
{
    OIIO_ASSERT(R.deep());
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        cspan<TypeDesc> channeltypes(R.deepdata()->all_channeltypes());
        ImageBuf::Iterator<float> r(R, roi);
        ImageBuf::ConstIterator<float> a(A, roi);
        for (; !r.done(); ++r, ++a) {
            for (int samp = 0, samples = r.deep_samples(); samp < samples;
                 ++samp) {
                for (int c = roi.chbegin; c < roi.chend; ++c) {
                    if (channeltypes[c].basetype == TypeDesc::UINT32)
                        r.set_deep_value(c, samp, a.deep_value_uint(c, samp));
                    else
                        r.set_deep_value(c, samp, a.deep_value(c, samp) + b[c]);
                }
            }
        }
    });
    return true;
}



bool
ImageBufAlgo::add(ImageBuf& dst, Image_or_Const A_, Image_or_Const B_, ROI roi,
                  int nthreads)
{
    OIIO::pvt::LoggedTimer logtime("IBA::add");
    if (A_.is_img() && B_.is_img()) {
        const ImageBuf &A(A_.img()), &B(B_.img());
        if (!IBAprep(roi, &dst, &A, &B))
            return false;
        ROI origroi = roi;
        roi.chend = std::min(roi.chend, std::min(A.nchannels(), B.nchannels()));
        bool ok;
        OIIO_DISPATCH_COMMON_TYPES3(ok, "add", add_impl, dst.spec().format,
                                    A.spec().format, B.spec().format, dst, A, B,
                                    roi, nthreads);
        if (roi.chend < origroi.chend && A.nchannels() != B.nchannels()) {
            // Edge case: A and B differed in nchannels, we allocated dst to be
            // the bigger of them, but adjusted roi to be the lesser. Now handle
            // the channels that got left out because they were not common to
            // all the inputs.
            OIIO_ASSERT(roi.chend <= dst.nchannels());
            roi.chbegin = roi.chend;
            roi.chend   = origroi.chend;
            if (A.nchannels() > B.nchannels()) {  // A exists
                copy(dst, A, dst.spec().format, roi, nthreads);
            } else {  // B exists
                copy(dst, B, dst.spec().format, roi, nthreads);
            }
        }
        return ok;
    }
    if (A_.is_val() && B_.is_img())  // canonicalize to A_img, B_val
        A_.swap(B_);
    if (A_.is_img() && B_.is_val()) {
        const ImageBuf& A(A_.img());
        cspan<float> b = B_.val();
        if (!IBAprep(roi, &dst, &A,
                     IBAprep_CLAMP_MUTUAL_NCHANNELS | IBAprep_SUPPORT_DEEP))
            return false;
        IBA_FIX_PERCHAN_LEN_DEF(b, A.nchannels());
        if (dst.deep()) {
            // While still serial, set up all the sample counts
            dst.deepdata()->set_all_samples(A.deepdata()->all_samples());
            return add_impl_deep(dst, A, b, roi, nthreads);
        }
        bool ok;
        OIIO_DISPATCH_COMMON_TYPES2(ok, "add", add_impl, dst.spec().format,
                                    A.spec().format, dst, A, b, roi, nthreads);
        return ok;
    }
    // Remaining cases: error
    dst.errorfmt("ImageBufAlgo::add(): at least one argument must be an image");
    return false;
}



ImageBuf
ImageBufAlgo::add(Image_or_Const A, Image_or_Const B, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = add(result, A, B, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::add() error");
    return result;
}



template<class Rtype, class Atype, class Btype>
static bool
sub_impl_scalar(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, ROI roi,
                int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::Iterator<Rtype> r(R, roi);
        ImageBuf::ConstIterator<Atype> a(A, roi);
        ImageBuf::ConstIterator<Btype> b(B, roi);
        for (; !r.done(); ++r, ++a, ++b)
            for (int c = roi.chbegin; c < roi.chend; ++c)
                r[c] = a[c] - b[c];
    });
    return true;
}



bool
ImageBufAlgo::sub(ImageBuf& dst, Image_or_Const A_, Image_or_Const B_, ROI roi,
                  int nthreads)
{
    OIIO::pvt::LoggedTimer logtime("IBA::sub");
    if (A_.is_img() && B_.is_img()) {
        const ImageBuf &A(A_.img()), &B(B_.img());
        if (!IBAprep(roi, &dst, &A, &B))
            return false;
        ROI origroi = roi;
        roi.chend = std::min(roi.chend, std::min(A.nchannels(), B.nchannels()));
        bool ok;
        OIIO_DISPATCH_COMMON_TYPES3(ok, "sub", sub_impl, dst.spec().format,
                                    A.spec().format, B.spec().format, dst, A, B,
                                    roi, nthreads);
        if (roi.chend < origroi.chend && A.nchannels() != B.nchannels()) {
            // Edge case: A and B differed in nchannels, we allocated dst to be
            // the bigger of them, but adjusted roi to be the lesser. Now handle
            // the channels that got left out because they were not common to
            // all the inputs.
            OIIO_ASSERT(roi.chend <= dst.nchannels());
            roi.chbegin = roi.chend;
            roi.chend   = origroi.chend;
            if (A.nchannels() > B.nchannels()) {  // A exists
                copy(dst, A, dst.spec().format, roi, nthreads);
            } else {  // B exists
                copy(dst, B, dst.spec().format, roi, nthreads);
            }
        }
        return ok;
    }
    if (A_.is_val() && B_.is_img())  // canonicalize to A_img, B_val
        A_.swap(B_);
    if (A_.is_img() && B_.is_val()) {
        const ImageBuf& A(A_.img());
        cspan<float> b = B_.val();
        if (!IBAprep(roi, &dst, &A,
                     IBAprep_CLAMP_MUTUAL_NCHANNELS | IBAprep_SUPPORT_DEEP))
            return false;
        IBA_FIX_PERCHAN_LEN_DEF(b, A.nchannels());
        // Negate b (into a copy)
        int nc      = A.nchannels();
        float* vals = OIIO_ALLOCA(float, nc);
        for (int c = 0; c < nc; ++c)
            vals[c] = -b[c];
        b = cspan<float>(vals, nc);
        if (dst.deep()) {
            // While still serial, set up all the sample counts
            dst.deepdata()->set_all_samples(A.deepdata()->all_samples());
            return add_impl_deep(dst, A, b, roi, nthreads);
        }
        bool ok;
        OIIO_DISPATCH_COMMON_TYPES2(ok, "sub", add_impl, dst.spec().format,
                                    A.spec().format, dst, A, b, roi, nthreads);
        return ok;
    }
    // Remaining cases: error
    dst.errorfmt("ImageBufAlgo::sub(): at least one argument must be an image");
    return false;
}



ImageBuf
ImageBufAlgo::sub(Image_or_Const A, Image_or_Const B, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = sub(result, A, B, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::sub() error");
    return result;
}


OIIO_NAMESPACE_3_1_END
