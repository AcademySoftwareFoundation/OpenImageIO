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

#if OIIO_USE_HWY
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



#if OIIO_USE_HWY
template<class Rtype, class ABCtype>
static bool
mad_impl_hwy(ImageBuf& R, const ImageBuf& A, const ImageBuf& B,
             const ImageBuf& C, ROI roi, int nthreads)
{
    auto op = [](auto /*d*/, auto a, auto b, auto c) {
        return hn::MulAdd(a, b, c);
    };

    if (hwy_ternary_perpixel_op_rgba_rgb_roi<Rtype, ABCtype>(R, A, B, C, roi,
                                                             nthreads, op))
        return true;

    return hwy_ternary_perpixel_op<Rtype, ABCtype>(R, A, B, C, roi, nthreads,
                                                   op);
}
#endif  // OIIO_USE_HWY

template<class Rtype, class ABCtype>
static bool
mad_impl(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, const ImageBuf& C,
         ROI roi, int nthreads)
{
#if OIIO_USE_HWY
    if (OIIO::pvt::enable_hwy && HwySupports<Rtype>(R, roi)
        && HwySupports<ABCtype>(A, roi) && HwySupports<ABCtype>(B, roi)
        && HwySupports<ABCtype>(C, roi)) {
        // All-channel case
        return mad_impl_hwy<Rtype, ABCtype>(R, A, B, C, roi, nthreads);
    }
#    if 0
    if (OIIO::pvt::enable_hwy && HwySupports<Rtype>(R, roi, 4)
        && HwySupports<Atype>(A, roi, 4) && HwySupports<Btype>(B, roi, 4)
        && HwySupports<Btype>(C, roi, 4)
        && (roi.chbegin == 0 && roi.chend == 3)) {
        // Handle the common RGBA + RGB ROI strided case (preserving alpha).
        return mad_impl_hwy<Rtype, ABCtype>(R, A, B, C, roi, nthreads);
    }
#    endif
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
