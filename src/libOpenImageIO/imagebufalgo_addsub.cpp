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

#include "imageio_pvt.h"


OIIO_NAMESPACE_BEGIN


template<class Rtype, class Atype, class Btype>
static bool
add_impl(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, ROI roi,
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
add_impl(ImageBuf& R, const ImageBuf& A, cspan<float> b, ROI roi, int nthreads)
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
    pvt::LoggedTimer logtime("IBA::add");
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
sub_impl(ImageBuf& R, const ImageBuf& A, const ImageBuf& B, ROI roi,
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
    pvt::LoggedTimer logtime("IBA::sub");
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


OIIO_NAMESPACE_END
