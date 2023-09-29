// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

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


template<class Rtype, class Atype>
static bool
minchan_impl(ImageBuf& R, const ImageBuf& A, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::Iterator<Rtype> r(R, roi);
        ImageBuf::ConstIterator<Atype> a(A, roi);
        for (; !r.done(); ++r, ++a) {
            float val = a[roi.chbegin];
            for (int c = roi.chbegin + 1; c < roi.chend; ++c)
                val = std::min(val, a[c]);
            r[0] = val;
        }
    });
    return true;
}



bool
ImageBufAlgo::minchan(ImageBuf& dst, const ImageBuf& src, ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::minchan");
    if (!roi.defined())
        roi = get_roi(src.spec());
    roi.chend      = std::min(roi.chend, src.nchannels());
    ROI dstroi     = roi;
    dstroi.chbegin = 0;
    dstroi.chend   = 1;
    if (!IBAprep(dstroi, &dst))
        return false;

    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "minchan", minchan_impl, dst.spec().format,
                                src.spec().format, dst, src, roi, nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::minchan(const ImageBuf& src, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = minchan(result, src, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::minchan() error");
    return result;
}



template<class Rtype, class Atype>
static bool
maxchan_impl(ImageBuf& R, const ImageBuf& A, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::Iterator<Rtype> r(R, roi);
        ImageBuf::ConstIterator<Atype> a(A, roi);
        for (; !r.done(); ++r, ++a) {
            float val = a[roi.chbegin];
            for (int c = roi.chbegin + 1; c < roi.chend; ++c)
                val = std::max(val, a[c]);
            r[0] = val;
        }
    });
    return true;
}



bool
ImageBufAlgo::maxchan(ImageBuf& dst, const ImageBuf& src, ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::maxchan");
    if (!roi.defined())
        roi = get_roi(src.spec());
    roi.chend      = std::min(roi.chend, src.nchannels());
    ROI dstroi     = roi;
    dstroi.chbegin = 0;
    dstroi.chend   = 1;
    if (!IBAprep(dstroi, &dst))
        return false;

    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "maxchan", maxchan_impl, dst.spec().format,
                                src.spec().format, dst, src, roi, nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::maxchan(const ImageBuf& src, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = maxchan(result, src, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::maxchan() error");
    return result;
}


OIIO_NAMESPACE_END
