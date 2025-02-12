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
#include <OpenImageIO/simd.h>

#include "imageio_pvt.h"


OIIO_NAMESPACE_BEGIN


template<class Rtype, class Atype, class Btype>
static bool
min_impl(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, ROI roi,
         int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::Iterator<Rtype> r(R, roi);
        ImageBuf::ConstIterator<Atype> a(A, roi);
        ImageBuf::ConstIterator<Btype> b(B, roi);
        for (; !r.done(); ++r, ++a, ++b)
            for (int c = roi.chbegin; c < roi.chend; ++c)
                r[c] = std::min(a[c], b[c]);
    });
    return true;
}



template<class Rtype, class Atype>
static bool
min_impl(ImageBuf& R, const ImageBuf& A, cspan<float> b, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::Iterator<Rtype> r(R, roi);
        ImageBuf::ConstIterator<Atype> a(A, roi);
        for (; !r.done(); ++r, ++a)
            for (int c = roi.chbegin; c < roi.chend; ++c)
                r[c] = std::min(a[c], b[c]);
    });
    return true;
}



bool
ImageBufAlgo::min(ImageBuf& dst, Image_or_Const A_, Image_or_Const B_, ROI roi,
                  int nthreads)
{
    pvt::LoggedTimer logtime("IBA::min");
    if (A_.is_img() && B_.is_img()) {
        const ImageBuf &A(A_.img()), &B(B_.img());
        if (!IBAprep(roi, &dst, &A, &B))
            return false;
        ROI origroi = roi;
        roi.chend = std::min(roi.chend, std::min(A.nchannels(), B.nchannels()));
        bool ok;
        OIIO_DISPATCH_COMMON_TYPES3(ok, "min", min_impl, dst.spec().format,
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
        if (!IBAprep(roi, &dst, &A, IBAprep_CLAMP_MUTUAL_NCHANNELS))
            return false;
        IBA_FIX_PERCHAN_LEN_DEF(b, A.nchannels());
        bool ok;
        OIIO_DISPATCH_COMMON_TYPES2(ok, "min", min_impl, dst.spec().format,
                                    A.spec().format, dst, A, b, roi, nthreads);
        return ok;
    }
    // Remaining cases: error
    dst.errorfmt("ImageBufAlgo::min(): at least one argument must be an image");
    return false;
}



ImageBuf
ImageBufAlgo::min(Image_or_Const A, Image_or_Const B, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = min(result, A, B, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::min() error");
    return result;
}



template<class Rtype, class Atype, class Btype>
static bool
max_impl(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, ROI roi,
         int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::Iterator<Rtype> r(R, roi);
        ImageBuf::ConstIterator<Atype> a(A, roi);
        ImageBuf::ConstIterator<Btype> b(B, roi);
        for (; !r.done(); ++r, ++a, ++b)
            for (int c = roi.chbegin; c < roi.chend; ++c)
                r[c] = std::max(a[c], b[c]);
    });
    return true;
}



template<class Rtype, class Atype>
static bool
max_impl(ImageBuf& R, const ImageBuf& A, cspan<float> b, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::Iterator<Rtype> r(R, roi);
        ImageBuf::ConstIterator<Atype> a(A, roi);
        for (; !r.done(); ++r, ++a)
            for (int c = roi.chbegin; c < roi.chend; ++c)
                r[c] = std::max(a[c], b[c]);
    });
    return true;
}



bool
ImageBufAlgo::max(ImageBuf& dst, Image_or_Const A_, Image_or_Const B_, ROI roi,
                  int nthreads)
{
    pvt::LoggedTimer logtime("IBA::max");
    if (A_.is_img() && B_.is_img()) {
        const ImageBuf &A(A_.img()), &B(B_.img());
        if (!IBAprep(roi, &dst, &A, &B))
            return false;
        ROI origroi = roi;
        roi.chend = std::max(roi.chend, std::max(A.nchannels(), B.nchannels()));
        bool ok;
        OIIO_DISPATCH_COMMON_TYPES3(ok, "max", max_impl, dst.spec().format,
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
        if (!IBAprep(roi, &dst, &A, IBAprep_CLAMP_MUTUAL_NCHANNELS))
            return false;
        IBA_FIX_PERCHAN_LEN_DEF(b, A.nchannels());
        bool ok;
        OIIO_DISPATCH_COMMON_TYPES2(ok, "max", max_impl, dst.spec().format,
                                    A.spec().format, dst, A, b, roi, nthreads);
        return ok;
    }
    // Remaining cases: error
    dst.errorfmt("ImageBufAlgo::max(): at least one argument must be an image");
    return false;
}



ImageBuf
ImageBufAlgo::max(Image_or_Const A, Image_or_Const B, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = max(result, A, B, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::max() error");
    return result;
}



template<class D, class S>
static bool
clamp_(ImageBuf& dst, const ImageBuf& src, const float* min, const float* max,
       bool clampalpha01, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::ConstIterator<S> s(src, roi);
        for (ImageBuf::Iterator<D> d(dst, roi); !d.done(); ++d, ++s) {
            for (int c = roi.chbegin; c < roi.chend; ++c)
                d[c] = OIIO::clamp<float>(s[c], min[c], max[c]);
        }
        int a = src.spec().alpha_channel;
        if (clampalpha01 && a >= roi.chbegin && a < roi.chend) {
            for (ImageBuf::Iterator<D> d(dst, roi); !d.done(); ++d)
                d[a] = OIIO::clamp<float>(d[a], 0.0f, 1.0f);
        }
    });
    return true;
}



bool
ImageBufAlgo::clamp(ImageBuf& dst, const ImageBuf& src, cspan<float> min,
                    cspan<float> max, bool clampalpha01, ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::clamp");
    if (!IBAprep(roi, &dst, &src))
        return false;
    const float big = std::numeric_limits<float>::max();
    IBA_FIX_PERCHAN_LEN(min, dst.nchannels(), min.size() ? min.back() : -big,
                        -big);
    IBA_FIX_PERCHAN_LEN(max, dst.nchannels(), max.size() ? max.back() : big,
                        big);
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "clamp", clamp_, dst.spec().format,
                                src.spec().format, dst, src, min.data(),
                                max.data(), clampalpha01, roi, nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::clamp(const ImageBuf& src, cspan<float> min, cspan<float> max,
                    bool clampalpha01, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = clamp(result, src, min, max, clampalpha01, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::clamp error");
    return result;
}



template<class Rtype, class Atype, class Btype>
static bool
absdiff_impl(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, ROI roi,
             int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::Iterator<Rtype> r(R, roi);
        ImageBuf::ConstIterator<Atype> a(A, roi);
        ImageBuf::ConstIterator<Btype> b(B, roi);
        for (; !r.done(); ++r, ++a, ++b)
            for (int c = roi.chbegin; c < roi.chend; ++c)
                r[c] = std::abs(a[c] - b[c]);
    });
    return true;
}


template<class Rtype, class Atype>
static bool
absdiff_impl(ImageBuf& R, const ImageBuf& A, cspan<float> b, ROI roi,
             int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::Iterator<Rtype> r(R, roi);
        ImageBuf::ConstIterator<Atype> a(A, roi);
        for (; !r.done(); ++r, ++a)
            for (int c = roi.chbegin; c < roi.chend; ++c)
                r[c] = std::abs(a[c] - b[c]);
    });
    return true;
}



bool
ImageBufAlgo::absdiff(ImageBuf& dst, Image_or_Const A_, Image_or_Const B_,
                      ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::absdiff");
    if (!IBAprep(roi, &dst, A_.imgptr(), B_.imgptr(), nullptr,
                 IBAprep_CLAMP_MUTUAL_NCHANNELS))
        return false;
    if (A_.is_img() && B_.is_img()) {
        const ImageBuf &A(A_.img()), &B(B_.img());
        ROI origroi = roi;
        roi.chend = std::min(roi.chend, std::min(A.nchannels(), B.nchannels()));
        bool ok;
        OIIO_DISPATCH_COMMON_TYPES3(ok, "absdiff", absdiff_impl,
                                    dst.spec().format, A.spec().format,
                                    B.spec().format, dst, A, B, roi, nthreads);
        if (roi.chend < origroi.chend && A.nchannels() != B.nchannels()) {
            // Edge case: A and B differed in nchannels, we allocated dst to be
            // the bigger of them, but adjusted roi to be the lesser. Now handle
            // the channels that got left out because they were not common to
            // all the inputs.
            OIIO_DASSERT(roi.chend <= dst.nchannels());
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
        IBA_FIX_PERCHAN_LEN_DEF(b, A.nchannels());
        bool ok;
        OIIO_DISPATCH_COMMON_TYPES2(ok, "absdiff", absdiff_impl,
                                    dst.spec().format, A.spec().format, dst, A,
                                    b, roi, nthreads);
        return ok;
    }
    // Remaining cases: error
    dst.errorfmt(
        "ImageBufAlgo::absdiff(): at least one argument must be an image");
    return false;
}



ImageBuf
ImageBufAlgo::absdiff(Image_or_Const A, Image_or_Const B, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = absdiff(result, A, B, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::absdiff() error");
    return result;
}



bool
ImageBufAlgo::abs(ImageBuf& dst, const ImageBuf& A, ROI roi, int nthreads)
{
    // Define abs in terms of absdiff(A,0.0)
    return absdiff(dst, A, 0.0f, roi, nthreads);
}



ImageBuf
ImageBufAlgo::abs(const ImageBuf& A, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = abs(result, A, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("abs error");
    return result;
}



template<class Rtype, class Atype>
static bool
pow_impl(ImageBuf& R, const ImageBuf& A, cspan<float> b, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::ConstIterator<Atype> a(A, roi);
        for (ImageBuf::Iterator<Rtype> r(R, roi); !r.done(); ++r, ++a)
            for (int c = roi.chbegin; c < roi.chend; ++c)
                r[c] = pow(a[c], b[c]);
    });
    return true;
}


bool
ImageBufAlgo::pow(ImageBuf& dst, const ImageBuf& A, cspan<float> b, ROI roi,
                  int nthreads)
{
    pvt::LoggedTimer logtime("IBA::pow");
    if (!IBAprep(roi, &dst, &A, IBAprep_CLAMP_MUTUAL_NCHANNELS))
        return false;
    IBA_FIX_PERCHAN_LEN_DEF(b, dst.nchannels());
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "pow", pow_impl, dst.spec().format,
                                A.spec().format, dst, A, b, roi, nthreads);
    return ok;
}


ImageBuf
ImageBufAlgo::pow(const ImageBuf& A, cspan<float> b, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = pow(result, A, b, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("pow error");
    return result;
}

template<class Rtype>
static bool
normalize_impl(ImageBuf& R, const ImageBuf& A, float inCenter, float outCenter,
               float scale, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::ConstIterator<Rtype> a(A, roi);
        for (ImageBuf::Iterator<Rtype> r(R, roi); !r.done(); ++r, ++a) {
            float x = a[0] - inCenter;
            float y = a[1] - inCenter;
            float z = a[2] - inCenter;

            float length = std::sqrt(x * x + y * y + z * z);

            float s = (length > 0.0f) ? scale / length : 0.0f;

            r[0] = x * s + outCenter;
            r[1] = y * s + outCenter;
            r[2] = z * s + outCenter;

            if (R.spec().nchannels == 4) {
                r[3] = a[3];
            }
        }
    });
    return true;
}

bool
ImageBufAlgo::normalize(ImageBuf& dst, const ImageBuf& src, float inCenter,
                        float outCenter, float scale, ROI roi, int nthreads)
{
    if (!ImageBufAlgo::IBAprep(roi, &dst, &src))
        return false;

    if (src.spec().nchannels != 3 && src.spec().nchannels != 4) {
        src.errorfmt("normalize can only handle 3- or 4-channel images");
        return false;
    }
    if (src.spec().nchannels < dst.spec().nchannels) {
        dst.errorfmt(
            "destination buffer can`t have more channels than the source");
        return false;
    }

    bool ok;
    OIIO_DISPATCH_COMMON_TYPES(ok, "normalize", normalize_impl,
                               dst.spec().format, dst, src, inCenter, outCenter,
                               scale, roi, nthreads);

    return ok;
}

ImageBuf
ImageBufAlgo::normalize(const ImageBuf& A, float inCenter, float outCenter,
                        float scale, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = ImageBufAlgo::normalize(result, A, inCenter, outCenter, scale,
                                      roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("normalize error");
    return result;
}



template<class D, class S>
static bool
channel_sum_(ImageBuf& dst, const ImageBuf& src, cspan<float> weights, ROI roi,
             int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::Iterator<D> d(dst, roi);
        ImageBuf::ConstIterator<S> s(src, roi);
        for (; !d.done(); ++d, ++s) {
            float sum = 0.0f;
            for (int c = roi.chbegin; c < roi.chend; ++c)
                sum += s[c] * weights[c];
            d[0] = sum;
        }
    });
    return true;
}



bool
ImageBufAlgo::channel_sum(ImageBuf& dst, const ImageBuf& src,
                          cspan<float> weights, ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::channel_sum");
    if (!roi.defined())
        roi = get_roi(src.spec());
    roi.chend      = std::min(roi.chend, src.nchannels());
    ROI dstroi     = roi;
    dstroi.chbegin = 0;
    dstroi.chend   = 1;
    if (!IBAprep(dstroi, &dst))
        return false;

    IBA_FIX_PERCHAN_LEN(weights, dst.nchannels(), 0.0f, 1.0f);

    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "channel_sum", channel_sum_,
                                dst.spec().format, src.spec().format, dst, src,
                                weights, roi, nthreads);
    return ok;
}


ImageBuf
ImageBufAlgo::channel_sum(const ImageBuf& src, cspan<float> weights, ROI roi,
                          int nthreads)
{
    ImageBuf result;
    bool ok = channel_sum(result, src, weights, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("channel_sum error");
    return result;
}



inline float
rangecompress(float x)
{
    // Formula courtesy of Sony Pictures Imageworks
#if 0 /* original coeffs -- identity transform for vals < 1 */
    const float x1 = 1.0, a = 1.2607481479644775391;
    const float b = 0.28785100579261779785, c = -1.4042005538940429688;
#else /* but received wisdom is that these work better */
    const float x1 = 0.18, a = -0.54576885700225830078;
    const float b = 0.18351669609546661377, c = 284.3577880859375;
#endif

    float absx = fabsf(x);
    if (absx <= x1)
        return x;
    return copysignf(a + b * logf(fabsf(c * absx + 1.0f)), x);
}



inline float
rangeexpand(float y)
{
    // Formula courtesy of Sony Pictures Imageworks
#if 0 /* original coeffs -- identity transform for vals < 1 */
    const float x1 = 1.0, a = 1.2607481479644775391;
    const float b = 0.28785100579261779785, c = -1.4042005538940429688;
#else /* but received wisdom is that these work better */
    const float x1 = 0.18, a = -0.54576885700225830078;
    const float b = 0.18351669609546661377, c = 284.3577880859375;
#endif

    float absy = fabsf(y);
    if (absy <= x1)
        return y;
    float xIntermediate = expf((absy - a) / b);
    // Since the compression step includes an absolute value, there are
    // two possible results here. If x < x1 it is the incorrect result,
    // so pick the other value.
    float x = (xIntermediate - 1.0f) / c;
    if (x < x1)
        x = (-xIntermediate - 1.0f) / c;
    return copysign(x, y);
}



template<class Rtype, class Atype>
static bool
rangecompress_(ImageBuf& R, const ImageBuf& A, bool useluma, ROI roi,
               int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        const ImageSpec& Aspec(A.spec());
        int alpha_channel = Aspec.alpha_channel;
        int z_channel     = Aspec.z_channel;
        if (roi.nchannels() < 3
            || (alpha_channel >= roi.chbegin && alpha_channel < roi.chbegin + 3)
            || (z_channel >= roi.chbegin && z_channel < roi.chbegin + 3)) {
            useluma = false;  // No way to use luma
        }

        if (&R == &A) {
            // Special case: operate in-place
            for (ImageBuf::Iterator<Rtype> r(R, roi); !r.done(); ++r) {
                if (useluma) {
                    float luma = 0.21264f * r[roi.chbegin]
                                 + 0.71517f * r[roi.chbegin + 1]
                                 + 0.07219f * r[roi.chbegin + 2];
                    float scale = luma > 0.0f ? rangecompress(luma) / luma
                                              : 0.0f;
                    for (int c = roi.chbegin; c < roi.chend; ++c) {
                        if (c == alpha_channel || c == z_channel)
                            continue;
                        r[c] = r[c] * scale;
                    }
                } else {
                    for (int c = roi.chbegin; c < roi.chend; ++c) {
                        if (c == alpha_channel || c == z_channel)
                            continue;
                        r[c] = rangecompress(r[c]);
                    }
                }
            }
        } else {
            ImageBuf::ConstIterator<Atype> a(A, roi);
            for (ImageBuf::Iterator<Rtype> r(R, roi); !r.done(); ++r, ++a) {
                if (useluma) {
                    float luma = 0.21264f * a[roi.chbegin]
                                 + 0.71517f * a[roi.chbegin + 1]
                                 + 0.07219f * a[roi.chbegin + 2];
                    float scale = luma > 0.0f ? rangecompress(luma) / luma
                                              : 0.0f;
                    for (int c = roi.chbegin; c < roi.chend; ++c) {
                        if (c == alpha_channel || c == z_channel)
                            r[c] = a[c];
                        else
                            r[c] = a[c] * scale;
                    }
                } else {
                    for (int c = roi.chbegin; c < roi.chend; ++c) {
                        if (c == alpha_channel || c == z_channel)
                            r[c] = a[c];
                        else
                            r[c] = rangecompress(a[c]);
                    }
                }
            }
        }
    });
    return true;
}



template<class Rtype, class Atype>
static bool
rangeexpand_(ImageBuf& R, const ImageBuf& A, bool useluma, ROI roi,
             int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        const ImageSpec& Aspec(A.spec());
        int alpha_channel = Aspec.alpha_channel;
        int z_channel     = Aspec.z_channel;
        if (roi.nchannels() < 3
            || (alpha_channel >= roi.chbegin && alpha_channel < roi.chbegin + 3)
            || (z_channel >= roi.chbegin && z_channel < roi.chbegin + 3)) {
            useluma = false;  // No way to use luma
        }

        if (&R == &A) {
            // Special case: operate in-place
            for (ImageBuf::Iterator<Rtype> r(R, roi); !r.done(); ++r) {
                if (useluma) {
                    float luma = 0.21264f * r[roi.chbegin]
                                 + 0.71517f * r[roi.chbegin + 1]
                                 + 0.07219f * r[roi.chbegin + 2];
                    float scale = luma > 0.0f ? rangeexpand(luma) / luma : 0.0f;
                    for (int c = roi.chbegin; c < roi.chend; ++c) {
                        if (c == alpha_channel || c == z_channel)
                            continue;
                        r[c] = r[c] * scale;
                    }
                } else {
                    for (int c = roi.chbegin; c < roi.chend; ++c) {
                        if (c == alpha_channel || c == z_channel)
                            continue;
                        r[c] = rangeexpand(r[c]);
                    }
                }
            }
        } else {
            ImageBuf::ConstIterator<Atype> a(A, roi);
            for (ImageBuf::Iterator<Rtype> r(R, roi); !r.done(); ++r, ++a) {
                if (useluma) {
                    float luma = 0.21264f * a[roi.chbegin]
                                 + 0.71517f * a[roi.chbegin + 1]
                                 + 0.07219f * a[roi.chbegin + 2];
                    float scale = luma > 0.0f ? rangeexpand(luma) / luma : 0.0f;
                    for (int c = roi.chbegin; c < roi.chend; ++c) {
                        if (c == alpha_channel || c == z_channel)
                            r[c] = a[c];
                        else
                            r[c] = a[c] * scale;
                    }
                } else {
                    for (int c = roi.chbegin; c < roi.chend; ++c) {
                        if (c == alpha_channel || c == z_channel)
                            r[c] = a[c];
                        else
                            r[c] = rangeexpand(a[c]);
                    }
                }
            }
        }
    });
    return true;
}



bool
ImageBufAlgo::rangecompress(ImageBuf& dst, const ImageBuf& src, bool useluma,
                            ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::rangecompress");
    if (!IBAprep(roi, &dst, &src, IBAprep_CLAMP_MUTUAL_NCHANNELS))
        return false;
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "rangecompress", rangecompress_,
                                dst.spec().format, src.spec().format, dst, src,
                                useluma, roi, nthreads);
    return ok;
}



bool
ImageBufAlgo::rangeexpand(ImageBuf& dst, const ImageBuf& src, bool useluma,
                          ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::rangeexpand");
    if (!IBAprep(roi, &dst, &src, IBAprep_CLAMP_MUTUAL_NCHANNELS))
        return false;
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "rangeexpand", rangeexpand_,
                                dst.spec().format, src.spec().format, dst, src,
                                useluma, roi, nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::rangecompress(const ImageBuf& src, bool useluma, ROI roi,
                            int nthreads)
{
    ImageBuf result;
    bool ok = rangecompress(result, src, useluma, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::rangecompress() error");
    return result;
}



ImageBuf
ImageBufAlgo::rangeexpand(const ImageBuf& src, bool useluma, ROI roi,
                          int nthreads)
{
    ImageBuf result;
    bool ok = rangeexpand(result, src, useluma, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::rangeexpand() error");
    return result;
}



template<class Rtype, class Atype>
static bool
unpremult_(ImageBuf& R, const ImageBuf& A, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        int alpha_channel = A.spec().alpha_channel;
        int z_channel     = A.spec().z_channel;
        if (&R == &A) {
            for (ImageBuf::Iterator<Rtype> r(R, roi); !r.done(); ++r) {
                float alpha = r[alpha_channel];
                if (alpha == 0.0f || alpha == 1.0f)
                    continue;
                for (int c = roi.chbegin; c < roi.chend; ++c)
                    if (c != alpha_channel && c != z_channel)
                        r[c] = r[c] / alpha;
            }
        } else {
            ImageBuf::ConstIterator<Atype> a(A, roi);
            for (ImageBuf::Iterator<Rtype> r(R, roi); !r.done(); ++r, ++a) {
                float alpha = a[alpha_channel];
                if (alpha == 0.0f || alpha == 1.0f) {
                    for (int c = roi.chbegin; c < roi.chend; ++c)
                        r[c] = a[c];
                    continue;
                }
                for (int c = roi.chbegin; c < roi.chend; ++c)
                    if (c != alpha_channel && c != z_channel)
                        r[c] = a[c] / alpha;
                    else
                        r[c] = a[c];
            }
        }
    });
    return true;
}



bool
ImageBufAlgo::unpremult(ImageBuf& dst, const ImageBuf& src, ROI roi,
                        int nthreads)
{
    pvt::LoggedTimer logtime("IBA::unpremult");
    if (!IBAprep(roi, &dst, &src, IBAprep_CLAMP_MUTUAL_NCHANNELS))
        return false;
    if (src.spec().alpha_channel < 0
        // Wise?  || src.spec().get_int_attribute("oiio:UnassociatedAlpha") != 0
    ) {
        // If there is no alpha channel, just *copy* instead of dividing
        // by alpha.
        if (&dst != &src)
            return paste(dst, src.spec().x, src.spec().y, src.spec().z,
                         roi.chbegin, src, roi, nthreads);
        return true;
    }
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "unpremult", unpremult_, dst.spec().format,
                                src.spec().format, dst, src, roi, nthreads);
    // Mark the output as having unassociated alpha
    dst.specmod().attribute("oiio:UnassociatedAlpha", 1);
    return ok;
}



ImageBuf
ImageBufAlgo::unpremult(const ImageBuf& src, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = unpremult(result, src, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::unpremult() error");
    return result;
}



template<class Rtype, class Atype>
static bool
premult_(ImageBuf& R, const ImageBuf& A, bool preserve_alpha0, ROI roi,
         int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        int alpha_channel = A.spec().alpha_channel;
        int z_channel     = A.spec().z_channel;
        if (&R == &A) {
            for (ImageBuf::Iterator<Rtype> r(R, roi); !r.done(); ++r) {
                float alpha = r[alpha_channel];
                if (alpha == 1.0f || (preserve_alpha0 && alpha == 0.0f))
                    continue;
                for (int c = roi.chbegin; c < roi.chend; ++c)
                    if (c != alpha_channel && c != z_channel)
                        r[c] = r[c] * alpha;
            }
        } else {
            ImageBuf::ConstIterator<Atype> a(A, roi);
            for (ImageBuf::Iterator<Rtype> r(R, roi); !r.done(); ++r, ++a) {
                float alpha   = a[alpha_channel];
                bool justcopy = alpha == 1.0f
                                || (preserve_alpha0 && alpha == 0.0f);
                if (justcopy) {
                    for (int c = roi.chbegin; c < roi.chend; ++c)
                        r[c] = a[c];
                    continue;
                }
                for (int c = roi.chbegin; c < roi.chend; ++c)
                    if (c != alpha_channel && c != z_channel)
                        r[c] = a[c] * alpha;
                    else
                        r[c] = a[c];
            }
        }
    });
    return true;
}



bool
ImageBufAlgo::premult(ImageBuf& dst, const ImageBuf& src, ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::premult");
    if (!IBAprep(roi, &dst, &src, IBAprep_CLAMP_MUTUAL_NCHANNELS))
        return false;
    if (src.spec().alpha_channel < 0) {
        if (&dst != &src)
            return paste(dst, src.spec().x, src.spec().y, src.spec().z,
                         roi.chbegin, src, roi, nthreads);
        return true;
    }
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "premult", premult_, dst.spec().format,
                                src.spec().format, dst, src, false, roi,
                                nthreads);
    // Clear the output of any prior marking of associated alpha
    dst.specmod().erase_attribute("oiio:UnassociatedAlpha");
    return ok;
}



ImageBuf
ImageBufAlgo::premult(const ImageBuf& src, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = premult(result, src, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::premult() error");
    return result;
}



bool
ImageBufAlgo::repremult(ImageBuf& dst, const ImageBuf& src, ROI roi,
                        int nthreads)
{
    pvt::LoggedTimer logtime("IBA::repremult");
    if (!IBAprep(roi, &dst, &src, IBAprep_CLAMP_MUTUAL_NCHANNELS))
        return false;
    if (src.spec().alpha_channel < 0) {
        if (&dst != &src)
            return paste(dst, src.spec().x, src.spec().y, src.spec().z,
                         roi.chbegin, src, roi, nthreads);
        return true;
    }
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "repremult", premult_, dst.spec().format,
                                src.spec().format, dst, src, true, roi,
                                nthreads);
    // Clear the output of any prior marking of associated alpha
    dst.specmod().erase_attribute("oiio:UnassociatedAlpha");
    return ok;
}



ImageBuf
ImageBufAlgo::repremult(const ImageBuf& src, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = repremult(result, src, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::repremult() error");
    return result;
}



// Helper: Are all elements of span s holding value v?
template<typename T>
inline bool
allspan(cspan<T> s, const T& v)
{
    return s.size() && std::all_of(s.cbegin(), s.cend(), [&](const T& e) {
               return e == v;
           });
}



template<class D, class S>
static bool
contrast_remap_(ImageBuf& dst, const ImageBuf& src, cspan<float> black,
                cspan<float> white, cspan<float> min, cspan<float> max,
                cspan<float> scontrast, cspan<float> sthresh, ROI roi,
                int nthreads)
{
    bool same_black_white = (black == white);
    float* bwdiffinv      = OIIO_ALLOCA(float, roi.chend);
    for (int c = roi.chbegin; c < roi.chend; ++c)
        bwdiffinv[c] = 1.0f / (white[c] - black[c]);
    bool use_sigmoid = !allspan(scontrast, 1.0f);
    bool do_minmax   = !(allspan(min, 0.0f) && allspan(max, 1.0f));

    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        if (same_black_white) {
            // Special case -- black & white are the same value, which is
            // just a binary threshold.
            ImageBuf::ConstIterator<S> s(src, roi);
            for (ImageBuf::Iterator<D> d(dst, roi); !d.done(); ++d, ++s) {
                for (int c = roi.chbegin; c < roi.chend; ++c)
                    d[c] = (s[c] < black[c] ? min[c] : max[c]);
            }
            return;
        }

        // First do the linear stretch
        float* r = OIIO_ALLOCA(float, roi.chend);  // temp result
        ImageBuf::ConstIterator<S> s(src, roi);
        float* y     = OIIO_ALLOCA(float, roi.chend);
        float* denom = OIIO_ALLOCA(float, roi.chend);
        for (ImageBuf::Iterator<D> d(dst, roi); !d.done(); ++d, ++s) {
            for (int c = roi.chbegin; c < roi.chend; ++c)
                r[c] = (s[c] - black[c]) * bwdiffinv[c];

            // Apply the sigmoid if needed
            // See http://www.imagemagick.org/Usage/color_mods/#sigmoidal
            // for a description of the shaping function.
            if (use_sigmoid) {
                // Sorry about the lack of clarity, we're working hard to
                // minimize computation.
                for (int c = roi.chbegin; c < roi.chend; ++c) {
                    y[c]     = 1.0f / (1.0f + expf(scontrast[c] * sthresh[c]));
                    denom[c] = 1.0f
                                   / (1.0f
                                      + expf(scontrast[c] * (sthresh[c] - 1.0f)))
                               - y[c];
                }
                for (int c = roi.chbegin; c < roi.chend; ++c) {
                    float x = 1.0f
                              / (1.0f
                                 + expf(scontrast[c] * (sthresh[c] - r[c])));
                    r[c] = (x - y[c]) / denom[c];
                }
            }

            // remap output range if needed
            if (do_minmax) {
                for (int c = roi.chbegin; c < roi.chend; ++c)
                    r[c] = lerp(min[c], max[c], r[c]);
            }
            for (int c = roi.chbegin; c < roi.chend; ++c)
                d[c] = r[c];
        }
    });
    return true;
}



bool
ImageBufAlgo::contrast_remap(ImageBuf& dst, const ImageBuf& src,
                             cspan<float> black, cspan<float> white,
                             cspan<float> min, cspan<float> max,
                             cspan<float> scontrast, cspan<float> sthresh,
                             ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::contrast_remap");
    if (!IBAprep(roi, &dst, &src))
        return false;
    // Force all the input spans to have values for all channels.
    int n = dst.nchannels();
    IBA_FIX_PERCHAN_LEN(black, n, black.size() ? black.back() : 0.0f, 0.0f);
    IBA_FIX_PERCHAN_LEN(white, n, white.size() ? white.back() : 1.0f, 1.0f);
    IBA_FIX_PERCHAN_LEN(min, n, min.size() ? min.back() : 0.0f, 0.0f);
    IBA_FIX_PERCHAN_LEN(max, n, max.size() ? max.back() : 1.0f, 1.0f);
    IBA_FIX_PERCHAN_LEN(scontrast, n,
                        scontrast.size() ? scontrast.back() : 1.0f, 1.0f);
    IBA_FIX_PERCHAN_LEN(sthresh, n, sthresh.size() ? sthresh.back() : 0.5f,
                        0.5f);
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "contrast_remap", contrast_remap_,
                                dst.spec().format, src.spec().format, dst, src,
                                black, white, min, max, scontrast, sthresh, roi,
                                nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::contrast_remap(const ImageBuf& src, cspan<float> black,
                             cspan<float> white, cspan<float> min,
                             cspan<float> max, cspan<float> scontrast,
                             cspan<float> sthresh, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = contrast_remap(result, src, black, white, min, max, scontrast,
                             sthresh, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::contrast_remap error");
    return result;
}



template<class Rtype, class Atype>
static bool
saturate_(ImageBuf& R, const ImageBuf& A, float scale, int firstchannel,
          ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        // Gross simplification: assume linear sRGB primaries. Ick -- but
        // what else to do if we don't really know the color space or its
        // characteristics?
        // FIXME: come back to this.
        const simd::vfloat3 weights(0.2126f, 0.7152f, 0.0722f);
        ImageBuf::ConstIterator<Atype> a(A, roi);
        for (ImageBuf::Iterator<Rtype> r(R, roi); !r.done(); ++r, ++a) {
            for (int c = roi.chbegin; c < firstchannel; ++c)
                r[c] = a[c];
            simd::vfloat3 rgb(a[firstchannel], a[firstchannel + 1],
                              a[firstchannel + 2]);
            simd::vfloat3 luma  = simd::vdot(rgb, weights);
            rgb                 = lerp(luma, rgb, scale);
            r[firstchannel]     = rgb[0];
            r[firstchannel + 1] = rgb[1];
            r[firstchannel + 2] = rgb[2];
            for (int c = firstchannel + 3; c < roi.chend; ++c)
                r[c] = a[c];
        }
    });
    return true;
}



bool
ImageBufAlgo::saturate(ImageBuf& dst, const ImageBuf& src, float scale,
                       int firstchannel, ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::saturate");
    if (!IBAprep(roi, &dst, &src, IBAprep_CLAMP_MUTUAL_NCHANNELS))
        return false;

    // Some basic error checking on whether the channel set makes sense
    int alpha_channel = src.spec().alpha_channel;
    int z_channel     = src.spec().z_channel;
    if (roi.chend - firstchannel < 3) {
        dst.errorfmt(
            "ImageBufAlgo::saturate can only work on 3 channels at a time. "
            "You specified starting at channel {} of a {}-channel ROI, that's not enough.",
            firstchannel, roi.nchannels());
        return false;
    }
    if (alpha_channel >= firstchannel && alpha_channel < firstchannel + 3) {
        dst.errorfmt(
            "ImageBufAlgo::saturate cannot operate alpha channels "
            "and you asked saturate to operate on channels {}-{}. Alpha is channel {}.",
            firstchannel, firstchannel + 2, alpha_channel);
        return false;
    }
    if (z_channel >= firstchannel && z_channel < firstchannel + 3) {
        dst.errorfmt(
            "ImageBufAlgo::saturate cannot operate z channels "
            "and you asked saturate to operate on channels {}-{}. Z is channel {}.",
            firstchannel, firstchannel + 2, z_channel);
        return false;
    }

    bool ok = true;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "saturate", saturate_, dst.spec().format,
                                src.spec().format, dst, src, scale,
                                firstchannel, roi, nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::saturate(const ImageBuf& src, float scale, int firstchannel,
                       ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = saturate(result, src, scale, firstchannel, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::saturate() error");
    return result;
}



template<class D, class S>
static bool
color_map_(ImageBuf& dst, const ImageBuf& src, int srcchannel, int nknots,
           int channels, cspan<float> knots, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        if (srcchannel < 0 && src.nchannels() < 3)
            srcchannel = 0;
        roi.chend = std::min(roi.chend, channels);
        ImageBuf::Iterator<D> d(dst, roi);
        ImageBuf::ConstIterator<S> s(src, roi);
        for (; !d.done(); ++d, ++s) {
            float x = srcchannel < 0
                          ? 0.2126f * s[0] + 0.7152f * s[1] + 0.0722f * s[2]
                          : s[srcchannel];
            for (int c = roi.chbegin; c < roi.chend; ++c) {
                span_strided<const float> k(knots.data() + c, nknots, channels);
                d[c] = interpolate_linear(x, k);
            }
        }
    });
    return true;
}



bool
ImageBufAlgo::color_map(ImageBuf& dst, const ImageBuf& src, int srcchannel,
                        int nknots, int channels, cspan<float> knots, ROI roi,
                        int nthreads)
{
    pvt::LoggedTimer logtime("IBA::color_map");
    if (srcchannel >= src.nchannels()) {
        dst.errorfmt("invalid source channel selected");
        return false;
    }
    if (nknots < 2 || std::ssize(knots) < (nknots * channels)) {
        dst.errorfmt("not enough knot values supplied");
        return false;
    }
    if (!roi.defined())
        roi = get_roi(src.spec());
    roi.chend      = std::min(roi.chend, src.nchannels());
    ROI dstroi     = roi;
    dstroi.chbegin = 0;
    dstroi.chend   = channels;
    if (!IBAprep(dstroi, &dst))
        return false;
    dstroi.chend = std::min(channels, dst.nchannels());

    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "color_map", color_map_, dst.spec().format,
                                src.spec().format, dst, src, srcchannel, nknots,
                                channels, knots, dstroi, nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::color_map(const ImageBuf& src, int srcchannel, int nknots,
                        int channels, cspan<float> knots, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = color_map(result, src, srcchannel, nknots, channels, knots, roi,
                        nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::color_map() error");
    return result;
}



// The color maps for magma, inferno, plasma, and viridis are from
// Matplotlib, written by Nathaniel Smith & Stefan van der Walt, and are
// public domain (http://creativecommons.org/publicdomain/zero/1.0/) The
// originals can be found here: https://github.com/bids/colormap
// These color maps were specially designed to be (a) perceptually uniform,
// (b) strictly increasing in luminance, (c) looking good when converted
// to grayscale for printing, (d) useful even for people with various forms
// of color blindness. They are therefore superior to most of the ad-hoc
// visualization color maps used elsewhere, including the original ones used
// in OIIO.
//
// LG has altered the original maps by converting from sRGB to a linear
// response (since that's how OIIO wants to operate), and also decimated the
// arrays from 256 entries to 17 entries (fine, since we interpolate).
// clang-format off
static const float magma_data[] = {
    0.000113f, 0.000036f, 0.001073f, 0.003066f, 0.002406f, 0.016033f, 0.012176f,
    0.005476f, 0.062265f, 0.036874f, 0.005102f, 0.146314f, 0.081757f, 0.006177f,
    0.200758f, 0.143411f, 0.011719f, 0.218382f, 0.226110f, 0.019191f, 0.221188f,
    0.334672f, 0.027718f, 0.212689f, 0.471680f, 0.037966f, 0.191879f, 0.632894f,
    0.053268f, 0.159870f, 0.795910f, 0.083327f, 0.124705f, 0.913454f, 0.146074f,
    0.106311f, 0.970011f, 0.248466f, 0.120740f, 0.991142f, 0.384808f, 0.167590f,
    0.992958f, 0.553563f, 0.247770f, 0.982888f, 0.756759f, 0.367372f, 0.970800f,
    0.980633f, 0.521749f
};
static const float inferno_data[] = {
    0.000113f, 0.000036f, 0.001073f, 0.003275f, 0.002178f, 0.017634f, 0.015183f,
    0.003697f, 0.068760f, 0.046307f, 0.002834f, 0.130327f, 0.095494f, 0.005137f,
    0.154432f, 0.163601f, 0.009920f, 0.156097f, 0.253890f, 0.016282f, 0.143715f,
    0.367418f, 0.024893f, 0.119982f, 0.500495f, 0.038279f, 0.089117f, 0.642469f,
    0.061553f, 0.057555f, 0.776190f, 0.102517f, 0.031141f, 0.883568f, 0.169990f,
    0.012559f, 0.951614f, 0.271639f, 0.002704f, 0.972636f, 0.413571f, 0.005451f,
    0.943272f, 0.599923f, 0.035112f, 0.884900f, 0.822282f, 0.140466f, 0.973729f,
    0.996282f, 0.373522f
};
static const float plasma_data[] = {
    0.003970f, 0.002307f, 0.240854f, 0.031078f, 0.001421f, 0.307376f, 0.073167f,
    0.000740f, 0.356714f, 0.132456f, 0.000066f, 0.388040f, 0.209330f, 0.000928f,
    0.390312f, 0.300631f, 0.005819f, 0.358197f, 0.399925f, 0.017084f, 0.301977f,
    0.501006f, 0.036122f, 0.240788f, 0.600808f, 0.063814f, 0.186921f, 0.698178f,
    0.101409f, 0.142698f, 0.790993f, 0.151134f, 0.106347f, 0.874354f, 0.216492f,
    0.076152f, 0.940588f, 0.302179f, 0.051495f, 0.980469f, 0.413691f, 0.032625f,
    0.984224f, 0.556999f, 0.020728f, 0.942844f, 0.738124f, 0.018271f, 0.868931f,
    0.944416f, 0.015590f
};
static const float viridis_data[] = {
    0.057951f, 0.000377f, 0.088657f, 0.064791f, 0.009258f, 0.145340f, 0.063189f,
    0.025975f, 0.198994f, 0.054539f, 0.051494f, 0.237655f, 0.043139f, 0.084803f,
    0.258811f, 0.032927f, 0.124348f, 0.268148f, 0.025232f, 0.169666f, 0.271584f,
    0.019387f, 0.221569f, 0.270909f, 0.014846f, 0.281323f, 0.263855f, 0.013529f,
    0.349530f, 0.246357f, 0.021457f, 0.425216f, 0.215605f, 0.049317f, 0.505412f,
    0.172291f, 0.112305f, 0.585164f, 0.121207f, 0.229143f, 0.657992f, 0.070438f,
    0.417964f, 0.717561f, 0.029928f, 0.683952f, 0.762557f, 0.009977f, 0.984709f,
    0.799651f, 0.018243f
};


// "Turbo" color map Copyright 2019 Google LLC.
// SPDX-License-Identifier: Apache-2.0
// Author: Anton Mikhailov
// https://gist.github.com/mikhailov-work/6a308c20e494d9e0ccc29036b28faa7a
// Altered by LG to convert from sRGB to linear and decimate the table to
// 17 entries.
//
// Turbo is also pretty nice, in similar ways to the matplotlib palettes,
// except for not being monotonically increasing in luminance.
static const float turbo_data[] = {
    0.03006f, 0.00619f, 0.04403f,  0.05131f, 0.05183f, 0.35936f,
    0.06204f, 0.14820f, 0.77017f,  0.05440f, 0.29523f, 0.99718f,
    0.02160f, 0.50023f, 0.83380f,  0.00892f, 0.72094f, 0.54189f,
    0.03205f, 0.88790f, 0.31235f,  0.15318f, 0.98683f, 0.12310f,
    0.37185f, 0.97738f, 0.04454f,  0.61188f, 0.83681f, 0.03455f,
    0.85432f, 0.62499f, 0.04203f,  0.98447f, 0.41196f, 0.03420f,
    0.96310f, 0.20754f, 0.01503f,  0.82971f, 0.08083f, 0.00438f,
    0.63144f, 0.02851f, 0.00140f,  0.39907f, 0.00776f, 0.00033f,
    0.19564f, 0.00123f, 0.00082f
};

// clang-format on



bool
ImageBufAlgo::color_map(ImageBuf& dst, const ImageBuf& src, int srcchannel,
                        string_view mapname, ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::color_map");
    if (srcchannel >= src.nchannels()) {
        dst.errorfmt("invalid source channel selected");
        return false;
    }
    cspan<float> knots;
    if (mapname == "magma") {
        knots = cspan<float>(magma_data);
    } else if (mapname == "inferno") {
        knots = cspan<float>(inferno_data);
    } else if (mapname == "plasma") {
        knots = cspan<float>(plasma_data);
    } else if (mapname == "viridis") {
        knots = cspan<float>(viridis_data);
    } else if (mapname == "turbo") {
        knots = cspan<float>(turbo_data);
    } else if (mapname == "blue-red" || mapname == "red-blue"
               || mapname == "bluered" || mapname == "redblue") {
        static const float k[] = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f };
        knots                  = cspan<float>(k);
    } else if (mapname == "spectrum") {
        static const float k[] = { 0.0f,  0.0f, 0.05f, 0.0f, 0.0f,
                                   0.75f, 0.0f, 0.5f,  0.0f, 0.5f,
                                   0.5f,  0.0f, 1.0f,  0.0f, 0.0f };
        knots                  = cspan<float>(k);
    } else if (mapname == "heat") {
        static const float k[] = { 0.0f,  0.0f,  0.0f, 0.05f, 0.0f,
                                   0.0f,  0.25f, 0.0f, 0.0f,  0.75f,
                                   0.75f, 0.0f,  1.0f, 1.0f,  1.0f };
        knots                  = cspan<float>(k);
    } else {
        dst.errorfmt("Unknown map name \"{}\"", mapname);
        return false;
    }
    return color_map(dst, src, srcchannel, int(knots.size() / 3), 3, knots, roi,
                     nthreads);
}


ImageBuf
ImageBufAlgo::color_map(const ImageBuf& src, int srcchannel,
                        string_view mapname, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = color_map(result, src, srcchannel, mapname, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::color_map() error");
    return result;
}



namespace {

using std::isfinite;

// Make sure isfinite is defined for 'half'
inline bool
isfinite(half h)
{
    return h.isFinite();
}


template<typename T>
bool
fixNonFinite_(ImageBuf& dst, ImageBufAlgo::NonFiniteFixMode mode,
              int* pixelsFixed, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ROI dstroi = get_roi(dst.spec());
        int count  = 0;  // Number of pixels with nonfinite values

        if (mode == ImageBufAlgo::NONFINITE_NONE
            || mode == ImageBufAlgo::NONFINITE_ERROR) {
            // Just count the number of pixels with non-finite values
            for (ImageBuf::Iterator<T, T> pixel(dst, roi); !pixel.done();
                 ++pixel) {
                for (int c = roi.chbegin; c < roi.chend; ++c) {
                    T value = pixel[c];
                    if (!isfinite(value)) {
                        ++count;
                        break;  // only count one per pixel
                    }
                }
            }
        } else if (mode == ImageBufAlgo::NONFINITE_BLACK) {
            // Replace non-finite pixels with black
            for (ImageBuf::Iterator<T, T> pixel(dst, roi); !pixel.done();
                 ++pixel) {
                bool fixed = false;
                for (int c = roi.chbegin; c < roi.chend; ++c) {
                    T value = pixel[c];
                    if (!isfinite(value)) {
                        pixel[c] = T(0.0);
                        fixed    = true;
                    }
                }
                if (fixed)
                    ++count;
            }
        } else if (mode == ImageBufAlgo::NONFINITE_BOX3) {
            // Replace non-finite pixels with a simple 3x3 window average
            // (the average excluding non-finite pixels, of course)
            for (ImageBuf::Iterator<T, T> pixel(dst, roi); !pixel.done();
                 ++pixel) {
                bool fixed = false;
                for (int c = roi.chbegin; c < roi.chend; ++c) {
                    T value = pixel[c];
                    if (!isfinite(value)) {
                        int numvals = 0;
                        T sum(0.0);
                        ROI roi2(pixel.x() - 1, pixel.x() + 2, pixel.y() - 1,
                                 pixel.y() + 2, pixel.z() - 1, pixel.z() + 2);
                        roi2 = roi_intersection(roi2, dstroi);
                        for (ImageBuf::Iterator<T, T> i(dst, roi2); !i.done();
                             ++i) {
                            T v = i[c];
                            if (isfinite(v)) {
                                sum += v;
                                ++numvals;
                            }
                        }
                        pixel[c] = numvals ? T(sum / numvals) : T(0.0);
                        fixed    = true;
                    }
                }
                if (fixed)
                    ++count;
            }
        }

        if (pixelsFixed) {
            // Update pixelsFixed atomically -- that's what makes this whole
            // function thread-safe.
            *(atomic_int*)pixelsFixed += count;
        }
    });
    return true;
}


bool
fixNonFinite_deep_(ImageBuf& dst, ImageBufAlgo::NonFiniteFixMode mode,
                   int* pixelsFixed, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        int count = 0;  // Number of pixels with nonfinite values
        if (mode == ImageBufAlgo::NONFINITE_NONE
            || mode == ImageBufAlgo::NONFINITE_ERROR) {
            // Just count the number of pixels with non-finite values
            for (ImageBuf::Iterator<float> pixel(dst, roi); !pixel.done();
                 ++pixel) {
                int samples = pixel.deep_samples();
                if (samples == 0)
                    continue;
                bool bad = false;
                for (int samp = 0; samp < samples && !bad; ++samp)
                    for (int c = roi.chbegin; c < roi.chend; ++c) {
                        float value = pixel.deep_value(c, samp);
                        if (!isfinite(value)) {
                            ++count;
                            bad = true;
                            break;
                        }
                    }
            }
        } else {
            // We don't know what to do with BOX3, so just always set to black.
            // Replace non-finite pixels with black
            for (ImageBuf::Iterator<float> pixel(dst, roi); !pixel.done();
                 ++pixel) {
                int samples = pixel.deep_samples();
                if (samples == 0)
                    continue;
                bool fixed = false;
                for (int samp = 0; samp < samples; ++samp)
                    for (int c = roi.chbegin; c < roi.chend; ++c) {
                        float value = pixel.deep_value(c, samp);
                        if (!isfinite(value)) {
                            pixel.set_deep_value(c, samp, 0.0f);
                            fixed = true;
                        }
                    }
                if (fixed)
                    ++count;
            }
        }
        if (pixelsFixed) {
            // Update pixelsFixed atomically -- that's what makes this whole
            // function thread-safe.
            *(atomic_int*)pixelsFixed += count;
        }
    });
    return true;
}

}  // namespace



/// Fix all non-finite pixels (nan/inf) using the specified approach
bool
ImageBufAlgo::fixNonFinite(ImageBuf& dst, const ImageBuf& src,
                           NonFiniteFixMode mode, int* pixelsFixed, ROI roi,
                           int nthreads)
{
    pvt::LoggedTimer logtime("IBA::fixNonFinite");
    if (mode != ImageBufAlgo::NONFINITE_NONE
        && mode != ImageBufAlgo::NONFINITE_BLACK
        && mode != ImageBufAlgo::NONFINITE_BOX3
        && mode != ImageBufAlgo::NONFINITE_ERROR) {
        // Something went wrong
        dst.errorfmt("fixNonFinite: unknown repair mode");
        return false;
    }

    if (!IBAprep(roi, &dst, &src, IBAprep_SUPPORT_DEEP))
        return false;

    // Initialize
    bool ok = true;
    int pixelsFixed_local;
    if (!pixelsFixed)
        pixelsFixed = &pixelsFixed_local;
    *pixelsFixed = 0;

    // Start by copying dst to src, if they aren't the same image
    if (&dst != &src)
        ok &= ImageBufAlgo::copy(dst, src, TypeDesc::UNKNOWN, roi, nthreads);

    if (dst.deep())
        ok &= fixNonFinite_deep_(dst, mode, pixelsFixed, roi, nthreads);
    else if (src.spec().format.basetype == TypeDesc::FLOAT)
        ok &= fixNonFinite_<float>(dst, mode, pixelsFixed, roi, nthreads);
    else if (src.spec().format.basetype == TypeDesc::HALF)
        ok &= fixNonFinite_<half>(dst, mode, pixelsFixed, roi, nthreads);
    else if (src.spec().format.basetype == TypeDesc::DOUBLE)
        ok &= fixNonFinite_<double>(dst, mode, pixelsFixed, roi, nthreads);
    // All other format types aren't capable of having nonfinite
    // pixel values, so the copy was enough.

    if (mode == ImageBufAlgo::NONFINITE_ERROR && *pixelsFixed) {
        dst.errorfmt("Nonfinite pixel values found");
        ok = false;
    }
    return ok;
}



ImageBuf
ImageBufAlgo::fixNonFinite(const ImageBuf& src, NonFiniteFixMode mode,
                           int* pixelsFixed, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = fixNonFinite(result, src, mode, pixelsFixed, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::fixNonFinite() error");
    return result;
}



static bool
decode_over_channels(const ImageBuf& R, int& nchannels, int& alpha, int& z,
                     int& colors)
{
    if (!R.initialized()) {
        alpha  = -1;
        z      = -1;
        colors = 0;
        return false;
    }
    const ImageSpec& spec(R.spec());
    alpha          = spec.alpha_channel;
    bool has_alpha = (alpha >= 0);
    z              = spec.z_channel;
    bool has_z     = (z >= 0);
    nchannels      = spec.nchannels;
    colors         = nchannels - has_alpha - has_z;
    if (!has_alpha && colors == 4) {
        // No marked alpha channel, but suspiciously 4 channel -- assume
        // it's RGBA.
        colors -= 1;
        // Assume alpha is the highest channel that's not z
        alpha = nchannels - 1;
        if (alpha == z)
            --alpha;
    }
    return true;
}



// Fully type-specialized version of over.
template<class Rtype, class Atype, class Btype>
static bool
over_impl(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, bool zcomp,
          bool z_zeroisinf, ROI roi, int nthreads)
{
    // It's already guaranteed that R, A, and B have matching channel
    // ordering, and have an alpha channel.  So just decode one.
    int nchannels = 0, alpha_channel = 0, z_channel = 0, ncolor_channels = 0;
    decode_over_channels(R, nchannels, alpha_channel, z_channel,
                         ncolor_channels);
    bool has_z = (z_channel >= 0);

    ImageBufAlgo::parallel_image(roi, nthreads, [=, &R, &A, &B](ROI roi) {
        ImageBuf::ConstIterator<Atype> a(A, roi);
        ImageBuf::ConstIterator<Btype> b(B, roi);
        ImageBuf::Iterator<Rtype> r(R, roi);
        for (; !r.done(); ++r, ++a, ++b) {
            float az = 0.0f, bz = 0.0f;
            bool a_is_closer = true;  // will remain true if !zcomp
            if (zcomp && has_z) {
                az = a[z_channel];
                bz = b[z_channel];
                if (z_zeroisinf) {
                    if (az == 0.0f)
                        az = std::numeric_limits<float>::max();
                    if (bz == 0.0f)
                        bz = std::numeric_limits<float>::max();
                }
                a_is_closer = (az <= bz);
            }
            if (a_is_closer) {
                // A over B
                float alpha           = clamp(a[alpha_channel], 0.0f, 1.0f);
                float one_minus_alpha = 1.0f - alpha;
                for (int c = roi.chbegin; c < roi.chend; c++)
                    r[c] = a[c] + one_minus_alpha * b[c];
                if (has_z)
                    r[z_channel] = (alpha != 0.0) ? a[z_channel] : b[z_channel];
            } else {
                // B over A -- because we're doing a Z composite
                float alpha           = clamp(b[alpha_channel], 0.0f, 1.0f);
                float one_minus_alpha = 1.0f - alpha;
                for (int c = roi.chbegin; c < roi.chend; c++)
                    r[c] = b[c] + one_minus_alpha * a[c];
                r[z_channel] = (alpha != 0.0) ? b[z_channel] : a[z_channel];
            }
        }
    });

    return true;
}



// Special case -- 4 channel RGBA float, in-memory buffer, no wrapping.
// Use loops and SIMD.
static bool
over_impl_rgbafloat(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, ROI roi,
                    int nthreads)
{
    using namespace simd;
    OIIO_DASSERT(A.localpixels() && B.localpixels()
                 && A.spec().format == TypeFloat && A.nchannels() == 4
                 && B.spec().format == TypeFloat && B.nchannels() == 4
                 && A.spec().alpha_channel == 3 && A.spec().z_channel < 0
                 && B.spec().alpha_channel == 3 && B.spec().z_channel < 0);
    // const int nchannels = 4, alpha_channel = 3;
    ImageBufAlgo::parallel_image(roi, nthreads, [=, &R, &A, &B](ROI roi) {
        vfloat4 zero = vfloat4::Zero();
        vfloat4 one  = vfloat4::One();
        int w        = roi.width();
        for (int z = roi.zbegin; z < roi.zend; ++z) {
            for (int y = roi.ybegin; y < roi.yend; ++y) {
                float* r       = (float*)R.pixeladdr(roi.xbegin, y, z);
                const float* a = (const float*)A.pixeladdr(roi.xbegin, y, z);
                const float* b = (const float*)B.pixeladdr(roi.xbegin, y, z);
                for (int x = 0; x < w; ++x, r += 4, a += 4, b += 4) {
                    vfloat4 a_simd(a);
                    vfloat4 b_simd(b);
                    vfloat4 alpha           = shuffle<3>(a_simd);
                    vfloat4 one_minus_alpha = one - clamp(alpha, zero, one);
                    vfloat4 result          = a_simd + one_minus_alpha * b_simd;
                    result.store(r);
                }
            }
        }
    });
    return true;
}



bool
ImageBufAlgo::over(ImageBuf& dst, const ImageBuf& A, const ImageBuf& B, ROI roi,
                   int nthreads)
{
    pvt::LoggedTimer logtime("IBA::over");
    if (!IBAprep(roi, &dst, &A, &B, NULL,
                 IBAprep_REQUIRE_ALPHA | IBAprep_REQUIRE_SAME_NCHANNELS))
        return false;

    if (A.localpixels() && B.localpixels() && A.spec().format == TypeFloat
        && A.nchannels() == 4 && B.spec().format == TypeFloat
        && B.nchannels() == 4 && A.spec().alpha_channel == 3
        && A.spec().z_channel < 0 && B.spec().alpha_channel == 3
        && B.spec().z_channel < 0 && A.roi().contains(roi)
        && B.roi().contains(roi) && roi.chbegin == 0 && roi.chend == 4) {
        // Easy case -- both buffers are float, 4 channels, alpha is
        // channel[3], no special z channel, and pixel data windows
        // completely cover the roi. This reduces to a simpler case we can
        // handle without iterators and taking advantage of SIMD.
        return over_impl_rgbafloat(dst, A, B, roi, nthreads);
    }

    bool ok;
    OIIO_DISPATCH_COMMON_TYPES3(ok, "over", over_impl, dst.spec().format,
                                A.spec().format, B.spec().format, dst, A, B,
                                false, false, roi, nthreads);
    return ok && !dst.has_error();
}



ImageBuf
ImageBufAlgo::over(const ImageBuf& A, const ImageBuf& B, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = over(result, A, B, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::over() error");
    return result;
}



bool
ImageBufAlgo::zover(ImageBuf& dst, const ImageBuf& A, const ImageBuf& B,
                    bool z_zeroisinf, ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::zover");
    if (!IBAprep(roi, &dst, &A, &B, NULL,
                 IBAprep_REQUIRE_ALPHA | IBAprep_REQUIRE_Z
                     | IBAprep_REQUIRE_SAME_NCHANNELS))
        return false;
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES3(ok, "zover", over_impl, dst.spec().format,
                                A.spec().format, B.spec().format, dst, A, B,
                                true, z_zeroisinf, roi, nthreads);
    return ok && !dst.has_error();
}



ImageBuf
ImageBufAlgo::zover(const ImageBuf& A, const ImageBuf& B, bool z_zeroisinf,
                    ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = zover(result, A, B, z_zeroisinf, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::zover() error");
    return result;
}


OIIO_NAMESPACE_END
