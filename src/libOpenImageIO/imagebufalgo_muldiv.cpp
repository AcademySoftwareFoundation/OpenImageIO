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

#include "imagebufalgo_hwy_pvt.h"
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



template<class Rtype, class Atype, class Btype>
static bool
mul_impl_hwy(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, ROI roi,
             int nthreads)
{
    auto Rv = HwyPixels(R);
    auto Av = HwyPixels(A);
    auto Bv = HwyPixels(B);
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        const int nchannels = RoiNChannels(roi);
        const bool contig   = ChannelsContiguous<Rtype>(Rv, nchannels)
                            && ChannelsContiguous<Atype>(Av, nchannels)
                            && ChannelsContiguous<Btype>(Bv, nchannels);

        for (int y = roi.ybegin; y < roi.yend; ++y) {
            Rtype* r_row       = RoiRowPtr<Rtype>(Rv, y, roi);
            const Atype* a_row = RoiRowPtr<Atype>(Av, y, roi);
            const Btype* b_row = RoiRowPtr<Btype>(Bv, y, roi);

            if (contig) {
                size_t n = static_cast<size_t>(roi.width())
                           * static_cast<size_t>(nchannels);
                RunHwyCmd<Rtype, Atype, Btype>(r_row, a_row, b_row, n,
                                               [](auto d, auto a, auto b) {
                                                   return hn::Mul(a, b);
                                               });
            } else {
                for (int x = roi.xbegin; x < roi.xend; ++x) {
                    Rtype* r_ptr = ChannelPtr<Rtype>(Rv, x, y, roi.chbegin);
                    const Atype* a_ptr = ChannelPtr<Atype>(Av, x, y,
                                                           roi.chbegin);
                    const Btype* b_ptr = ChannelPtr<Btype>(Bv, x, y,
                                                           roi.chbegin);
                    for (int c = 0; c < nchannels; ++c) {
                        r_ptr[c] = static_cast<Rtype>(
                            static_cast<float>(a_ptr[c])
                            * static_cast<float>(b_ptr[c]));
                    }
                }
            }
        }
    });
    return true;
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

template<class Rtype, class Atype, class Btype>
static bool
mul_impl(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, ROI roi,
         int nthreads)
{
    if (OIIO::pvt::enable_hwy && R.localpixels() && A.localpixels()
        && B.localpixels())
        return mul_impl_hwy<Rtype, Atype, Btype>(R, A, B, roi, nthreads);
    return mul_impl_scalar<Rtype, Atype, Btype>(R, A, B, roi, nthreads);
}

template<class Rtype, class Atype>
static bool
mul_impl(ImageBuf& R, const ImageBuf& A, cspan<float> b, ROI roi, int nthreads)
{
    if (OIIO::pvt::enable_hwy && R.localpixels() && A.localpixels())
        return mul_impl_hwy<Rtype, Atype>(R, A, b, roi, nthreads);
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



template<class Rtype, class Atype, class Btype>
static bool
div_impl_hwy(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, ROI roi,
             int nthreads)
{
    auto Rv = HwyPixels(R);
    auto Av = HwyPixels(A);
    auto Bv = HwyPixels(B);
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        const int nchannels = RoiNChannels(roi);
        const bool contig   = ChannelsContiguous<Rtype>(Rv, nchannels)
                            && ChannelsContiguous<Atype>(Av, nchannels)
                            && ChannelsContiguous<Btype>(Bv, nchannels);

        for (int y = roi.ybegin; y < roi.yend; ++y) {
            Rtype* r_row       = RoiRowPtr<Rtype>(Rv, y, roi);
            const Atype* a_row = RoiRowPtr<Atype>(Av, y, roi);
            const Btype* b_row = RoiRowPtr<Btype>(Bv, y, roi);

            if (contig) {
                size_t n = static_cast<size_t>(roi.width())
                           * static_cast<size_t>(nchannels);
                RunHwyCmd<Rtype, Atype, Btype>(
                    r_row, a_row, b_row, n, [](auto d, auto a, auto b) {
                        // Check for zero division: if b == 0, return 0
                        auto zero = hn::Zero(d);
                        auto mask = hn::Eq(b, zero);
                        return hn::IfThenElse(mask, zero, hn::Div(a, b));
                    });
            } else {
                for (int x = roi.xbegin; x < roi.xend; ++x) {
                    Rtype* r_ptr = ChannelPtr<Rtype>(Rv, x, y, roi.chbegin);
                    const Atype* a_ptr = ChannelPtr<Atype>(Av, x, y,
                                                           roi.chbegin);
                    const Btype* b_ptr = ChannelPtr<Btype>(Bv, x, y,
                                                           roi.chbegin);
                    for (int c = 0; c < nchannels; ++c) {
                        float v  = static_cast<float>(b_ptr[c]);
                        r_ptr[c] = (v == 0.0f)
                                       ? static_cast<Rtype>(0.0f)
                                       : static_cast<Rtype>(
                                             static_cast<float>(a_ptr[c]) / v);
                    }
                }
            }
        }
    });
    return true;
}

template<class Rtype, class Atype, class Btype>
static bool
div_impl(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, ROI roi,
         int nthreads)
{
    if (OIIO::pvt::enable_hwy && R.localpixels() && A.localpixels()
        && B.localpixels())
        return div_impl_hwy<Rtype, Atype, Btype>(R, A, B, roi, nthreads);
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
