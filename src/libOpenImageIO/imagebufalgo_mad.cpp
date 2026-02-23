// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <cmath>
#include <iostream>
#include <limits>

#if defined(_WIN32)
#    include <malloc.h>  // for alloca
#endif

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/half.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>

#if defined(OIIO_USE_HWY) && OIIO_USE_HWY
#    include "imagebufalgo_hwy_pvt.h"
#endif
#include "imageio_pvt.h"


OIIO_NAMESPACE_3_1_BEGIN



template<class Rtype, class ABCtype>
static bool
mad_impl_scalar(ImageBuf& R, const ImageBuf& A, const ImageBuf& B,
                const ImageBuf& C, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::Iterator<Rtype> r(R, roi);
        ImageBuf::ConstIterator<ABCtype> a(A, roi);
        ImageBuf::ConstIterator<ABCtype> b(B, roi);
        ImageBuf::ConstIterator<ABCtype> c(C, roi);
        for (; !r.done(); ++r, ++a, ++b, ++c) {
            for (int ch = roi.chbegin; ch < roi.chend; ++ch)
                r[ch] = a[ch] * b[ch] + c[ch];
        }
    });
    return true;
}



#if defined(OIIO_USE_HWY) && OIIO_USE_HWY
template<class Rtype, class ABCtype>
static bool
mad_impl_hwy(ImageBuf& R, const ImageBuf& A, const ImageBuf& B,
             const ImageBuf& C, ROI roi, int nthreads)
{
    auto op = [](auto /*d*/, auto a, auto b, auto c) {
        return hn::MulAdd(a, b, c);
    };

    // Special-case: RGBA images but ROI is RGB (strided channel subset). We
    // still can SIMD the RGB channels by processing full RGBA and preserving
    // alpha exactly (bitwise) from the destination.
    if (roi.chbegin == 0 && roi.chend == 3) {
        // Only support same-type float/half/double in this fast path.
        constexpr bool floaty = (std::is_same_v<Rtype, float>
                                 || std::is_same_v<Rtype, double>
                                 || std::is_same_v<Rtype, half>)
                                && std::is_same_v<Rtype, ABCtype>;
        if constexpr (floaty) {
            auto Rv = HwyPixels(R);
            auto Av = HwyPixels(A);
            auto Bv = HwyPixels(B);
            auto Cv = HwyPixels(C);
            if (Rv.nchannels >= 4 && Av.nchannels >= 4 && Bv.nchannels >= 4
                && Cv.nchannels >= 4 && ChannelsContiguous<Rtype>(Rv, 4)
                && ChannelsContiguous<ABCtype>(Av, 4)
                && ChannelsContiguous<ABCtype>(Bv, 4)
                && ChannelsContiguous<ABCtype>(Cv, 4)) {
                ROI roi4     = roi;
                roi4.chbegin = 0;
                roi4.chend   = 4;
                using MathT  = typename SimdMathType<Rtype>::type;
                const hn::ScalableTag<MathT> d;
                const size_t lanes = hn::Lanes(d);
                ImageBufAlgo::parallel_image(roi4, nthreads, [&](ROI roi4) {
                    for (int y = roi4.ybegin; y < roi4.yend; ++y) {
                        Rtype* r_row         = RoiRowPtr<Rtype>(Rv, y, roi4);
                        const ABCtype* a_row = RoiRowPtr<ABCtype>(Av, y, roi4);
                        const ABCtype* b_row = RoiRowPtr<ABCtype>(Bv, y, roi4);
                        const ABCtype* c_row = RoiRowPtr<ABCtype>(Cv, y, roi4);
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
                                const T16* c16
                                    = reinterpret_cast<const T16*>(c_row + off);
                                T16* r16 = reinterpret_cast<T16*>(r_row + off);

                                hn::Vec<decltype(d16)> ar16, ag16, ab16, aa16;
                                hn::Vec<decltype(d16)> br16, bg16, bb16, ba16;
                                hn::Vec<decltype(d16)> cr16, cg16, cb16, ca16;
                                hn::Vec<decltype(d16)> dr16, dg16, db16, da16;
                                hn::LoadInterleaved4(d16, a16, ar16, ag16, ab16,
                                                     aa16);
                                hn::LoadInterleaved4(d16, b16, br16, bg16, bb16,
                                                     ba16);
                                hn::LoadInterleaved4(d16, c16, cr16, cg16, cb16,
                                                     ca16);
                                hn::LoadInterleaved4(d16, r16, dr16, dg16, db16,
                                                     da16);
                                (void)aa16;
                                (void)ba16;
                                (void)ca16;
                                (void)dr16;
                                (void)dg16;
                                (void)db16;

                                auto rr = op(d, hn::PromoteTo(d, ar16),
                                             hn::PromoteTo(d, br16),
                                             hn::PromoteTo(d, cr16));
                                auto rg = op(d, hn::PromoteTo(d, ag16),
                                             hn::PromoteTo(d, bg16),
                                             hn::PromoteTo(d, cg16));
                                auto rb = op(d, hn::PromoteTo(d, ab16),
                                             hn::PromoteTo(d, bb16),
                                             hn::PromoteTo(d, cb16));

                                auto rr16 = hn::DemoteTo(d16, rr);
                                auto rg16 = hn::DemoteTo(d16, rg);
                                auto rb16 = hn::DemoteTo(d16, rb);
                                hn::StoreInterleaved4(rr16, rg16, rb16, da16, d16,
                                                      r16);
                            } else {
                                hn::Vec<decltype(d)> ar, ag, ab, aa;
                                hn::Vec<decltype(d)> br, bg, bb, ba;
                                hn::Vec<decltype(d)> cr, cg, cb, ca;
                                hn::Vec<decltype(d)> dr, dg, db, da;
                                hn::LoadInterleaved4(d, a_row + off, ar, ag, ab,
                                                     aa);
                                hn::LoadInterleaved4(d, b_row + off, br, bg, bb,
                                                     ba);
                                hn::LoadInterleaved4(d, c_row + off, cr, cg, cb,
                                                     ca);
                                hn::LoadInterleaved4(d, r_row + off, dr, dg, db,
                                                     da);
                                (void)aa;
                                (void)ba;
                                (void)ca;
                                (void)dr;
                                (void)dg;
                                (void)db;

                                auto rr = op(d, ar, br, cr);
                                auto rg = op(d, ag, bg, cg);
                                auto rb = op(d, ab, bb, cb);
                                hn::StoreInterleaved4(rr, rg, rb, da, d,
                                                      r_row + off);
                            }
                        }

                        for (; x < npixels; ++x) {
                            const size_t off = x * 4;
                            if constexpr (std::is_same_v<Rtype, half>) {
                                r_row[off + 0]
                                    = half((float)a_row[off + 0]
                                           * (float)b_row[off + 0]
                                           + (float)c_row[off + 0]);
                                r_row[off + 1]
                                    = half((float)a_row[off + 1]
                                           * (float)b_row[off + 1]
                                           + (float)c_row[off + 1]);
                                r_row[off + 2]
                                    = half((float)a_row[off + 2]
                                           * (float)b_row[off + 2]
                                           + (float)c_row[off + 2]);
                            } else {
                                r_row[off + 0] = a_row[off + 0] * b_row[off + 0]
                                                 + c_row[off + 0];
                                r_row[off + 1] = a_row[off + 1] * b_row[off + 1]
                                                 + c_row[off + 1];
                                r_row[off + 2] = a_row[off + 2] * b_row[off + 2]
                                                 + c_row[off + 2];
                            }
                            // Preserve alpha (off+3).
                        }
                    }
                });
                return true;
            }
        }
    }

    return hwy_ternary_perpixel_op<Rtype, ABCtype>(R, A, B, C, roi, nthreads,
                                                   op);
}
#endif  // defined(OIIO_USE_HWY) && OIIO_USE_HWY

template<class Rtype, class ABCtype>
static bool
mad_impl(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, const ImageBuf& C,
         ROI roi, int nthreads)
{
#if defined(OIIO_USE_HWY) && OIIO_USE_HWY
    if (OIIO::pvt::enable_hwy && R.localpixels() && A.localpixels()
        && B.localpixels() && C.localpixels()) {
        auto Rv = HwyPixels(R);
        auto Av = HwyPixels(A);
        auto Bv = HwyPixels(B);
        auto Cv = HwyPixels(C);
        const int nchannels = RoiNChannels(roi);
        const bool contig   = ChannelsContiguous<Rtype>(Rv, nchannels)
                            && ChannelsContiguous<ABCtype>(Av, nchannels)
                            && ChannelsContiguous<ABCtype>(Bv, nchannels)
                            && ChannelsContiguous<ABCtype>(Cv, nchannels);
        if (contig)
            return mad_impl_hwy<Rtype, ABCtype>(R, A, B, C, roi, nthreads);

        // Handle the common RGBA + RGB ROI strided case (preserving alpha).
        constexpr bool floaty_strided = (std::is_same_v<Rtype, float>
                                         || std::is_same_v<Rtype, double>
                                         || std::is_same_v<Rtype, half>)
                                        && std::is_same_v<Rtype, ABCtype>;
        if constexpr (floaty_strided) {
            if (roi.chbegin == 0 && roi.chend == 3) {
                const bool contig4 = (Rv.nchannels >= 4 && Av.nchannels >= 4
                                      && Bv.nchannels >= 4 && Cv.nchannels >= 4)
                                     && ChannelsContiguous<Rtype>(Rv, 4)
                                     && ChannelsContiguous<ABCtype>(Av, 4)
                                     && ChannelsContiguous<ABCtype>(Bv, 4)
                                     && ChannelsContiguous<ABCtype>(Cv, 4);
                if (contig4)
                    return mad_impl_hwy<Rtype, ABCtype>(R, A, B, C, roi,
                                                        nthreads);
            }
        }
    }
#endif
    return mad_impl_scalar<Rtype, ABCtype>(R, A, B, C, roi, nthreads);
}



template<class Rtype, class ABCtype>
static bool
mad_impl_ici(ImageBuf& R, const ImageBuf& A, cspan<float> b, const ImageBuf& C,
             ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::Iterator<Rtype> r(R, roi);
        ImageBuf::ConstIterator<ABCtype> a(A, roi);
        ImageBuf::ConstIterator<ABCtype> c(C, roi);
        for (; !r.done(); ++r, ++a, ++c) {
            for (int ch = roi.chbegin; ch < roi.chend; ++ch)
                r[ch] = a[ch] * b[ch] + c[ch];
        }
    });
    return true;
}



template<class Rtype, class Atype>
static bool
mad_impl_icc(ImageBuf& R, const ImageBuf& A, cspan<float> b, cspan<float> c,
             ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::Iterator<Rtype> r(R, roi);
        ImageBuf::ConstIterator<Atype> a(A, roi);
        for (; !r.done(); ++r, ++a)
            for (int ch = roi.chbegin; ch < roi.chend; ++ch)
                r[ch] = a[ch] * b[ch] + c[ch];
    });
    return true;
}



template<class Rtype, class Atype>
static bool
mad_impl_iic(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, cspan<float> c,
             ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::Iterator<Rtype> r(R, roi);
        ImageBuf::ConstIterator<Atype> a(A, roi);
        ImageBuf::ConstIterator<Atype> b(B, roi);
        for (; !r.done(); ++r, ++a, ++b)
            for (int ch = roi.chbegin; ch < roi.chend; ++ch)
                r[ch] = a[ch] * b[ch] + c[ch];
    });
    return true;
}



bool
ImageBufAlgo::mad(ImageBuf& dst, Image_or_Const A_, Image_or_Const B_,
                  Image_or_Const C_, ROI roi, int nthreads)
{
    OIIO::pvt::LoggedTimer logtime("IBA::mad");

    // Canonicalize so that if one of A,B is a constant, A is an image.
    if (A_.is_val() && B_.is_img())  // canonicalize to A_img, B_val
        A_.swap(B_);
    // Get pointers to any image. At least one of A or B must be an image.
    const ImageBuf *A = A_.imgptr(), *B = B_.imgptr(), *C = C_.imgptr();
    if (!A) {
        dst.errorfmt(
            "ImageBufAlgo::mad(): at least one of the first two arguments must be an image");
        return false;
    }
    // All of the arguments that are images need to be initialized
    if ((A && !A->initialized()) || (B && !B->initialized())
        || (C && !C->initialized())) {
        dst.errorfmt("Uninitialized input image");
        return false;
    }

    // To avoid the full cross-product of dst/A/B/C types, force any of
    // A,B,C that are images to all be the same data type, copying if we
    // have to.
    TypeDesc abc_type
        = TypeDesc::basetype_merge(A ? A->spec().format : TypeUnknown,
                                   B ? B->spec().format : TypeUnknown,
                                   C ? C->spec().format : TypeUnknown);
    ImageBuf Anew, Bnew, Cnew;
    if (A && A->spec().format != abc_type) {
        Anew.copy(*A, abc_type);
        A = &Anew;
    }
    if (B && B->spec().format != abc_type) {
        Bnew.copy(*B, abc_type);
        B = &Bnew;
    }
    if (C && C->spec().format != abc_type) {
        Cnew.copy(*C, abc_type);
        C = &Cnew;
    }

    if (!IBAprep(roi, &dst, A, B ? B : C, C))
        return false;

    // Note: A is always an image. That leaves 4 cases to deal with.
    bool ok;
    if (B) {
        if (C) {
            OIIO_DISPATCH_COMMON_TYPES2(ok, "mad", mad_impl, dst.spec().format,
                                        abc_type, dst, *A, *B, *C, roi,
                                        nthreads);
        } else {  // C not an image
            cspan<float> c(C_.val());
            IBA_FIX_PERCHAN_LEN_DEF(c, dst.nchannels());
            OIIO_DISPATCH_COMMON_TYPES2(ok, "mad", mad_impl_iic,
                                        dst.spec().format, abc_type, dst, *A,
                                        *B, c, roi, nthreads);
        }
    } else {  // B is not an image
        cspan<float> b(B_.val());
        IBA_FIX_PERCHAN_LEN_DEF(b, dst.nchannels());
        if (C) {
            OIIO_DISPATCH_COMMON_TYPES2(ok, "mad", mad_impl_ici,
                                        dst.spec().format, abc_type, dst, *A, b,
                                        *C, roi, nthreads);
        } else {  // C not an image
            cspan<float> c(C_.val());
            IBA_FIX_PERCHAN_LEN_DEF(c, dst.nchannels());
            OIIO_DISPATCH_COMMON_TYPES2(ok, "mad", mad_impl_icc,
                                        dst.spec().format, abc_type, dst, *A, b,
                                        c, roi, nthreads);
        }
    }
    return ok;
}



ImageBuf
ImageBufAlgo::mad(Image_or_Const A, Image_or_Const B, Image_or_Const C, ROI roi,
                  int nthreads)
{
    ImageBuf result;
    bool ok = mad(result, A, B, C, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::mad() error");
    return result;
}



bool
ImageBufAlgo::invert(ImageBuf& dst, const ImageBuf& A, ROI roi, int nthreads)
{
    // Calculate invert as simply 1-A == A*(-1)+1
    return mad(dst, A, -1.0, 1.0, roi, nthreads);
}


ImageBuf
ImageBufAlgo::invert(const ImageBuf& A, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = invert(result, A, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("invert error");
    return result;
}


OIIO_NAMESPACE_3_1_END
