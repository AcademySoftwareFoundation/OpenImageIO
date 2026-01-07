// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <cmath>
#include <iostream>
#include <limits>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/half.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>

#include "imagebufalgo_hwy_pvt.h"
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



template<class Rtype, class ABCtype>
static bool
mad_impl_hwy(ImageBuf& R, const ImageBuf& A, const ImageBuf& B,
             const ImageBuf& C, ROI roi, int nthreads)
{
    auto Rv = HwyPixels(R);
    auto Av = HwyPixels(A);
    auto Bv = HwyPixels(B);
    auto Cv = HwyPixels(C);
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        const int nchannels = RoiNChannels(roi);
        const bool contig   = ChannelsContiguous<Rtype>(Rv, nchannels)
                            && ChannelsContiguous<ABCtype>(Av, nchannels)
                            && ChannelsContiguous<ABCtype>(Bv, nchannels)
                            && ChannelsContiguous<ABCtype>(Cv, nchannels);

        for (int y = roi.ybegin; y < roi.yend; ++y) {
            Rtype* r_row         = RoiRowPtr<Rtype>(Rv, y, roi);
            const ABCtype* a_row = RoiRowPtr<ABCtype>(Av, y, roi);
            const ABCtype* b_row = RoiRowPtr<ABCtype>(Bv, y, roi);
            const ABCtype* c_row = RoiRowPtr<ABCtype>(Cv, y, roi);

            if (contig) {
                size_t n = static_cast<size_t>(roi.width())
                           * static_cast<size_t>(nchannels);
                // Use Highway SIMD for a*b+c (fused multiply-add)
                RunHwyTernaryCmd<Rtype, ABCtype>(r_row, a_row, b_row, c_row, n,
                                                 [](auto d, auto a, auto b,
                                                    auto c) {
                                                     return hn::MulAdd(a, b, c);
                                                 });
            } else {
                for (int x = roi.xbegin; x < roi.xend; ++x) {
                    Rtype* r_ptr = ChannelPtr<Rtype>(Rv, x, y, roi.chbegin);
                    const ABCtype* a_ptr = ChannelPtr<ABCtype>(Av, x, y,
                                                               roi.chbegin);
                    const ABCtype* b_ptr = ChannelPtr<ABCtype>(Bv, x, y,
                                                               roi.chbegin);
                    const ABCtype* c_ptr = ChannelPtr<ABCtype>(Cv, x, y,
                                                               roi.chbegin);
                    for (int ch = 0; ch < nchannels; ++ch) {
                        r_ptr[ch] = static_cast<Rtype>(
                            static_cast<float>(a_ptr[ch])
                                * static_cast<float>(b_ptr[ch])
                            + static_cast<float>(c_ptr[ch]));
                    }
                }
            }
        }
    });
    return true;
}

template<class Rtype, class ABCtype>
static bool
mad_impl(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, const ImageBuf& C,
         ROI roi, int nthreads)
{
    if (OIIO::pvt::enable_hwy && R.localpixels() && A.localpixels()
        && B.localpixels() && C.localpixels())
        return mad_impl_hwy<Rtype, ABCtype>(R, A, B, C, roi, nthreads);
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



// Highway SIMD implementation for invert: 1 - x
template<class Rtype, class Atype>
static bool
invert_impl_hwy(ImageBuf& R, const ImageBuf& A, ROI roi, int nthreads)
{
    using MathT = typename SimdMathType<Rtype>::type;

    auto Rv = HwyPixels(R);
    auto Av = HwyPixels(A);
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        const int nchannels = RoiNChannels(roi);
        const bool contig   = ChannelsContiguous<Rtype>(Rv, nchannels)
                            && ChannelsContiguous<Atype>(Av, nchannels);

        for (int y = roi.ybegin; y < roi.yend; ++y) {
            Rtype* r_row       = RoiRowPtr<Rtype>(Rv, y, roi);
            const Atype* a_row = RoiRowPtr<Atype>(Av, y, roi);

            if (contig) {
                size_t n = static_cast<size_t>(roi.width())
                           * static_cast<size_t>(nchannels);
                RunHwyUnaryCmd<Rtype, Atype>(
                    r_row, a_row, n, [](auto d, auto va) {
                        auto one = hn::Set(d, static_cast<MathT>(1.0));
                        return hn::Sub(one, va);
                    });
            } else {
                // Non-contiguous fallback
                for (int x = roi.xbegin; x < roi.xend; ++x) {
                    Rtype* r_ptr = ChannelPtr<Rtype>(Rv, x, y, roi.chbegin);
                    const Atype* a_ptr = ChannelPtr<Atype>(Av, x, y,
                                                           roi.chbegin);
                    for (int c = 0; c < nchannels; ++c) {
                        r_ptr[c] = static_cast<Rtype>(
                            1.0f - static_cast<float>(a_ptr[c]));
                    }
                }
            }
        }
    });
    return true;
}


// Dispatcher for invert
template<class Rtype, class Atype>
static bool
invert_impl(ImageBuf& R, const ImageBuf& A, ROI roi, int nthreads)
{
    if (OIIO::pvt::enable_hwy && R.localpixels() && A.localpixels())
        return invert_impl_hwy<Rtype, Atype>(R, A, roi, nthreads);

    // Scalar fallback: use mad(A, -1.0, 1.0)
    return ImageBufAlgo::mad(R, A, -1.0, 1.0, roi, nthreads);
}


bool
ImageBufAlgo::invert(ImageBuf& dst, const ImageBuf& A, ROI roi, int nthreads)
{
    OIIO::pvt::LoggedTimer logtime("IBA::invert");
    if (!IBAprep(roi, &dst, &A))
        return false;
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "invert", invert_impl, dst.spec().format,
                                A.spec().format, dst, A, roi, nthreads);
    return ok;
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
