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
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        const ImageSpec& Rspec  = R.spec();
        const ImageSpec& Aspec  = A.spec();
        const ImageSpec& Bspec  = B.spec();
        const ImageSpec& Cspec  = C.spec();
        size_t r_pixel_bytes    = Rspec.pixel_bytes();
        size_t a_pixel_bytes    = Aspec.pixel_bytes();
        size_t b_pixel_bytes    = Bspec.pixel_bytes();
        size_t c_pixel_bytes    = Cspec.pixel_bytes();
        size_t r_scanline_bytes = Rspec.scanline_bytes();
        size_t a_scanline_bytes = Aspec.scanline_bytes();
        size_t b_scanline_bytes = Bspec.scanline_bytes();
        size_t c_scanline_bytes = Cspec.scanline_bytes();

        char* r_base       = (char*)R.localpixels();
        const char* a_base = (const char*)A.localpixels();
        const char* b_base = (const char*)B.localpixels();
        const char* c_base = (const char*)C.localpixels();

        int nchannels = roi.chend - roi.chbegin;
        bool contig   = (nchannels * sizeof(Rtype) == r_pixel_bytes)
                      && (nchannels * sizeof(ABCtype) == a_pixel_bytes)
                      && (nchannels * sizeof(ABCtype) == b_pixel_bytes)
                      && (nchannels * sizeof(ABCtype) == c_pixel_bytes);

        for (int y = roi.ybegin; y < roi.yend; ++y) {
            char* r_row = r_base + (y - R.ybegin()) * r_scanline_bytes
                          + (roi.xbegin - R.xbegin()) * r_pixel_bytes;
            const char* a_row = a_base + (y - A.ybegin()) * a_scanline_bytes
                                + (roi.xbegin - A.xbegin()) * a_pixel_bytes;
            const char* b_row = b_base + (y - B.ybegin()) * b_scanline_bytes
                                + (roi.xbegin - B.xbegin()) * b_pixel_bytes;
            const char* c_row = c_base + (y - C.ybegin()) * c_scanline_bytes
                                + (roi.xbegin - C.xbegin()) * c_pixel_bytes;

            r_row += roi.chbegin * sizeof(Rtype);
            a_row += roi.chbegin * sizeof(ABCtype);
            b_row += roi.chbegin * sizeof(ABCtype);
            c_row += roi.chbegin * sizeof(ABCtype);

            if (contig) {
                size_t n = static_cast<size_t>(roi.width()) * nchannels;
                // Use Highway SIMD for a*b+c (fused multiply-add)
                RunHwyTernaryCmd<Rtype, ABCtype>(
                    reinterpret_cast<Rtype*>(r_row),
                    reinterpret_cast<const ABCtype*>(a_row),
                    reinterpret_cast<const ABCtype*>(b_row),
                    reinterpret_cast<const ABCtype*>(c_row), n,
                    [](auto d, auto a, auto b, auto c) {
                        // a*b+c: use MulAdd if available, otherwise Mul+Add
                        return hn::MulAdd(a, b, c);
                    });
            } else {
                for (int x = 0; x < roi.width(); ++x) {
                    Rtype* r_ptr = reinterpret_cast<Rtype*>(r_row)
                                   + x * r_pixel_bytes / sizeof(Rtype);
                    const ABCtype* a_ptr
                        = reinterpret_cast<const ABCtype*>(a_row)
                          + x * a_pixel_bytes / sizeof(ABCtype);
                    const ABCtype* b_ptr
                        = reinterpret_cast<const ABCtype*>(b_row)
                          + x * b_pixel_bytes / sizeof(ABCtype);
                    const ABCtype* c_ptr
                        = reinterpret_cast<const ABCtype*>(c_row)
                          + x * c_pixel_bytes / sizeof(ABCtype);
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

    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        const ImageSpec& Rspec  = R.spec();
        const ImageSpec& Aspec  = A.spec();
        size_t r_pixel_bytes    = Rspec.pixel_bytes();
        size_t a_pixel_bytes    = Aspec.pixel_bytes();
        size_t r_scanline_bytes = Rspec.scanline_bytes();
        size_t a_scanline_bytes = Aspec.scanline_bytes();

        char* r_base       = (char*)R.localpixels();
        const char* a_base = (const char*)A.localpixels();

        int nchannels = roi.chend - roi.chbegin;
        bool contig   = (nchannels * sizeof(Rtype) == r_pixel_bytes)
                      && (nchannels * sizeof(Atype) == a_pixel_bytes);

        for (int y = roi.ybegin; y < roi.yend; ++y) {
            char* r_row = r_base + (y - R.ybegin()) * r_scanline_bytes
                          + (roi.xbegin - R.xbegin()) * r_pixel_bytes;
            const char* a_row = a_base + (y - A.ybegin()) * a_scanline_bytes
                                + (roi.xbegin - A.xbegin()) * a_pixel_bytes;

            r_row += roi.chbegin * sizeof(Rtype);
            a_row += roi.chbegin * sizeof(Atype);

            if (contig) {
                size_t n = static_cast<size_t>(roi.width()) * nchannels;
                RunHwyUnaryCmd<Rtype, Atype>(
                    reinterpret_cast<Rtype*>(r_row),
                    reinterpret_cast<const Atype*>(a_row), n,
                    [](auto d, auto va) {
                        auto one = hn::Set(d, static_cast<MathT>(1.0));
                        return hn::Sub(one, va);
                    });
            } else {
                // Non-contiguous fallback
                for (int x = 0; x < roi.width(); ++x) {
                    Rtype* r_ptr = reinterpret_cast<Rtype*>(r_row)
                                   + x * r_pixel_bytes / sizeof(Rtype);
                    const Atype* a_ptr = reinterpret_cast<const Atype*>(a_row)
                                         + x * a_pixel_bytes / sizeof(Atype);
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
