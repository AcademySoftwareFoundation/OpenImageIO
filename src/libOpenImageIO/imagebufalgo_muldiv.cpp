// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

/// \file
/// Implementation of ImageBufAlgo algorithms that do math on
/// single pixels at a time.

#include <cmath>
#include <iostream>
#include <limits>

#if defined(_WIN32)
#    include <malloc.h>  // for alloca
#endif

#include <OpenImageIO/half.h>

#if defined(OIIO_USE_HWY) && OIIO_USE_HWY
#    include "imagebufalgo_hwy_pvt.h"
#endif
#include <OpenImageIO/dassert.h>
#include <OpenImageIO/deepdata.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/simd.h>

#include "imageio_pvt.h"


OIIO_NAMESPACE_3_1_BEGIN


template<class Rtype, class Atype, class Btype>
static bool
scale_impl(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, ROI roi,
           int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::Iterator<Rtype> r(R, roi);
        ImageBuf::ConstIterator<Atype> a(A, roi);
        ImageBuf::ConstIterator<Btype> b(B, roi);
        for (; !r.done(); ++r, ++a, ++b)
            for (int c = roi.chbegin; c < roi.chend; ++c)
                r[c] = a[c] * b[0];
    });
    return true;
}



bool
ImageBufAlgo::scale(ImageBuf& dst, const ImageBuf& A, const ImageBuf& B,
                    KWArgs options, ROI roi, int nthreads)
{
    OIIO::pvt::LoggedTimer logtime("IBA::scale");
    bool ok = false;
    if (B.nchannels() == 1) {
        if (IBAprep(roi, &dst, &A, &B))
            OIIO_DISPATCH_COMMON_TYPES3(ok, "scale", scale_impl,
                                        dst.spec().format, A.spec().format,
                                        B.spec().format, dst, A, B, roi,
                                        nthreads);
    } else if (A.nchannels() == 1) {
        if (IBAprep(roi, &dst, &A, &B))
            OIIO_DISPATCH_COMMON_TYPES3(ok, "scale", scale_impl,
                                        dst.spec().format, B.spec().format,
                                        A.spec().format, dst, B, A, roi,
                                        nthreads);
    } else {
        dst.errorfmt(
            "ImageBufAlgo::scale(): one of the arguments must be a single channel image.");
    }

    return ok;
}



ImageBuf
ImageBufAlgo::scale(const ImageBuf& A, const ImageBuf& B, KWArgs options,
                    ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = scale(result, A, B, options, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::scale() error");
    return result;
}



template<class Rtype, class Atype, class Btype>
static bool
mul_impl_scalar(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, ROI roi,
                int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::Iterator<Rtype> r(R, roi);
        ImageBuf::ConstIterator<Atype> a(A, roi);
        ImageBuf::ConstIterator<Btype> b(B, roi);
        for (; !r.done(); ++r, ++a, ++b)
            for (int c = roi.chbegin; c < roi.chend; ++c)
                r[c] = a[c] * b[c];
    });
    return true;
}



template<class Rtype, class Atype>
static bool
mul_impl_scalar(ImageBuf& R, const ImageBuf& A, cspan<float> b, ROI roi,
                int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::ConstIterator<Atype> a(A, roi);
        for (ImageBuf::Iterator<Rtype> r(R, roi); !r.done(); ++r, ++a)
            for (int c = roi.chbegin; c < roi.chend; ++c)
                r[c] = a[c] * b[c];
    });
    return true;
}



#if defined(OIIO_USE_HWY) && OIIO_USE_HWY
template<class Rtype, class Atype, class Btype>
static bool
mul_impl_hwy(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, ROI roi,
             int nthreads)
{
    auto op = [](auto /*d*/, auto a, auto b) {
        return hn::Mul(a, b);
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
                                           * (float)b_row[off + 0]);
                                r_row[off + 1]
                                    = half((float)a_row[off + 1]
                                           * (float)b_row[off + 1]);
                                r_row[off + 2]
                                    = half((float)a_row[off + 2]
                                           * (float)b_row[off + 2]);
                            } else {
                                r_row[off + 0] = a_row[off + 0] * b_row[off + 0];
                                r_row[off + 1] = a_row[off + 1] * b_row[off + 1];
                                r_row[off + 2] = a_row[off + 2] * b_row[off + 2];
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
mul_impl_hwy(ImageBuf& R, const ImageBuf& A, cspan<float> b, ROI roi,
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
                    r_ptr[c] = (Rtype)((SimdType)a_ptr[c] * (SimdType)b[c]);
                }
            }
        }
    });
    return true;
}
#endif  // defined(OIIO_USE_HWY) && OIIO_USE_HWY

template<class Rtype, class Atype, class Btype>
static bool
mul_impl(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, ROI roi,
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
        if (contig)
            return mul_impl_hwy<Rtype, Atype, Btype>(R, A, B, roi, nthreads);

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
                    return mul_impl_hwy<Rtype, Atype, Btype>(R, A, B, roi,
                                                             nthreads);
            }
        }
    }
#endif
    return mul_impl_scalar<Rtype, Atype, Btype>(R, A, B, roi, nthreads);
}

template<class Rtype, class Atype>
static bool
mul_impl(ImageBuf& R, const ImageBuf& A, cspan<float> b, ROI roi, int nthreads)
{
#if defined(OIIO_USE_HWY) && OIIO_USE_HWY
    if (OIIO::pvt::enable_hwy && R.localpixels() && A.localpixels())
        return mul_impl_hwy<Rtype, Atype>(R, A, b, roi, nthreads);
#endif
    return mul_impl_scalar<Rtype, Atype>(R, A, b, roi, nthreads);
}

static bool
mul_impl_deep(ImageBuf& R, const ImageBuf& A, cspan<float> b, ROI roi,
              int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        // Deep case
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
                        r.set_deep_value(c, samp, a.deep_value(c, samp) * b[c]);
                }
            }
        }
    });
    return true;
}



bool
ImageBufAlgo::mul(ImageBuf& dst, Image_or_Const A_, Image_or_Const B_, ROI roi,
                  int nthreads)
{
    OIIO::pvt::LoggedTimer logtime("IBA::mul");
    if (A_.is_img() && B_.is_img()) {
        const ImageBuf &A(A_.img()), &B(B_.img());
        if (!IBAprep(roi, &dst, &A, &B, IBAprep_CLAMP_MUTUAL_NCHANNELS))
            return false;
        bool ok;
        OIIO_DISPATCH_COMMON_TYPES3(ok, "mul", mul_impl, dst.spec().format,
                                    A.spec().format, B.spec().format, dst, A, B,
                                    roi, nthreads);
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
            return mul_impl_deep(dst, A, b, roi, nthreads);
        }
        bool ok;
        OIIO_DISPATCH_COMMON_TYPES2(ok, "mul", mul_impl, dst.spec().format,
                                    A.spec().format, dst, A, b, roi, nthreads);
        return ok;
    }
    // Remaining cases: error
    dst.errorfmt("ImageBufAlgo::mul(): at least one argument must be an image");
    return false;
}



ImageBuf
ImageBufAlgo::mul(Image_or_Const A, Image_or_Const B, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = mul(result, A, B, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::mul() error");
    return result;
}



template<class Rtype, class Atype, class Btype>
static bool
div_impl_scalar(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, ROI roi,
                int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::Iterator<Rtype> r(R, roi);
        ImageBuf::ConstIterator<Atype> a(A, roi);
        ImageBuf::ConstIterator<Btype> b(B, roi);
        for (; !r.done(); ++r, ++a, ++b)
            for (int c = roi.chbegin; c < roi.chend; ++c) {
                float v = b[c];
                r[c]    = (v == 0.0f) ? 0.0f : (a[c] / v);
            }
    });
    return true;
}



#if defined(OIIO_USE_HWY) && OIIO_USE_HWY
template<class Rtype, class Atype, class Btype>
static bool
div_impl_hwy(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, ROI roi,
             int nthreads)
{
    auto op = [](auto d, auto a, auto b) {
        const auto zero = hn::Zero(d);
        const auto nz   = hn::Ne(b, zero);
        const auto one  = hn::Set(d, 1);
        const auto safe_b = hn::IfThenElse(nz, b, one);
        const auto q      = hn::Div(a, safe_b);
        return hn::IfThenElse(nz, q, zero);
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
                                const float denom0 = (float)b_row[off + 0];
                                const float denom1 = (float)b_row[off + 1];
                                const float denom2 = (float)b_row[off + 2];
                                r_row[off + 0]
                                    = (denom0 == 0.0f)
                                          ? half(0.0f)
                                          : half((float)a_row[off + 0] / denom0);
                                r_row[off + 1]
                                    = (denom1 == 0.0f)
                                          ? half(0.0f)
                                          : half((float)a_row[off + 1] / denom1);
                                r_row[off + 2]
                                    = (denom2 == 0.0f)
                                          ? half(0.0f)
                                          : half((float)a_row[off + 2] / denom2);
                            } else {
                                const auto denom0 = b_row[off + 0];
                                const auto denom1 = b_row[off + 1];
                                const auto denom2 = b_row[off + 2];
                                r_row[off + 0]
                                    = (denom0 == 0) ? 0 : (a_row[off + 0] / denom0);
                                r_row[off + 1]
                                    = (denom1 == 0) ? 0 : (a_row[off + 1] / denom1);
                                r_row[off + 2]
                                    = (denom2 == 0) ? 0 : (a_row[off + 2] / denom2);
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
div_impl(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, ROI roi,
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
        if (contig)
            return div_impl_hwy<Rtype, Atype, Btype>(R, A, B, roi, nthreads);

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
                    return div_impl_hwy<Rtype, Atype, Btype>(R, A, B, roi,
                                                             nthreads);
            }
        }
    }
#endif
    return div_impl_scalar<Rtype, Atype, Btype>(R, A, B, roi, nthreads);
}



bool
ImageBufAlgo::div(ImageBuf& dst, Image_or_Const A_, Image_or_Const B_, ROI roi,
                  int nthreads)
{
    OIIO::pvt::LoggedTimer logtime("IBA::div");
    if (A_.is_img() && B_.is_img()) {
        const ImageBuf &A(A_.img()), &B(B_.img());
        if (!IBAprep(roi, &dst, &A, &B, IBAprep_CLAMP_MUTUAL_NCHANNELS))
            return false;
        bool ok;
        OIIO_DISPATCH_COMMON_TYPES3(ok, "div", div_impl, dst.spec().format,
                                    A.spec().format, B.spec().format, dst, A, B,
                                    roi, nthreads);
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

        IBA_FIX_PERCHAN_LEN_DEF(b, dst.nchannels());
        int nc      = dst.nchannels();
        float* binv = OIIO_ALLOCA(float, nc);
        for (int c = 0; c < nc; ++c)
            binv[c] = (b[c] == 0.0f) ? 0.0f : 1.0f / b[c];
        b = cspan<float>(binv, nc);  // re-wrap

        if (dst.deep()) {
            // While still serial, set up all the sample counts
            dst.deepdata()->set_all_samples(A.deepdata()->all_samples());
            return mul_impl_deep(dst, A, b, roi, nthreads);
        }
        bool ok;
        OIIO_DISPATCH_COMMON_TYPES2(ok, "div", mul_impl, dst.spec().format,
                                    A.spec().format, dst, A, b, roi, nthreads);
        return ok;
    }
    // Remaining cases: error
    dst.errorfmt("ImageBufAlgo::div(): at least one argument must be an image");
    return false;
}



ImageBuf
ImageBufAlgo::div(Image_or_Const A, Image_or_Const B, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = div(result, A, B, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::div() error");
    return result;
}



OIIO_NAMESPACE_3_1_END
