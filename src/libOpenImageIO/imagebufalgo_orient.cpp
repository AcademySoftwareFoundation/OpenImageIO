// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <cmath>
#include <iostream>

#include <OpenImageIO/half.h>

#include <OpenImageIO/deepdata.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/thread.h>

#include "imageio_pvt.h"


OIIO_NAMESPACE_BEGIN


template<class D, class S = D>
static bool
flip_(ImageBuf& dst, const ImageBuf& src, ROI dst_roi, int /*nthreads*/)
{
    ROI src_roi_full = src.roi_full();
    ROI dst_roi_full = dst.roi_full();
    ImageBuf::ConstIterator<S, D> s(src);
    ImageBuf::Iterator<D, D> d(dst, dst_roi);
    for (; !d.done(); ++d) {
        int yy = d.y() - dst_roi_full.ybegin;
        s.pos(d.x(), src_roi_full.yend - 1 - yy, d.z());
        for (int c = dst_roi.chbegin; c < dst_roi.chend; ++c)
            d[c] = s[c];
    }
    return true;
}


bool
ImageBufAlgo::flip(ImageBuf& dst, const ImageBuf& src, ROI roi, int nthreads)
{
    if (&dst == &src) {  // Handle in-place operation
        ImageBuf tmp;
        tmp.swap(const_cast<ImageBuf&>(src));
        return flip(dst, tmp, roi, nthreads);
    }
    pvt::LoggedTimer logtime("IBA::flip");

    ROI src_roi      = roi.defined() ? roi : src.roi();
    ROI src_roi_full = src.roi_full();
    int offset       = src_roi.ybegin - src_roi_full.ybegin;
    int start        = src_roi_full.yend - offset - src_roi.height();
    ROI dst_roi(src_roi.xbegin, src_roi.xend, start, start + src_roi.height(),
                src_roi.zbegin, src_roi.zend, src_roi.chbegin, src_roi.chend);
    OIIO_DASSERT(dst_roi.width() == src_roi.width()
                 && dst_roi.height() == src_roi.height());

    // Compute the destination ROI, it's the source ROI reflected across
    // the midline of the display window.
    if (!IBAprep(dst_roi, &dst, &src))
        return false;
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "flip", flip_, dst.spec().format,
                                src.spec().format, dst, src, dst_roi, nthreads);
    return ok;
}



template<class D, class S = D>
static bool
flop_(ImageBuf& dst, const ImageBuf& src, ROI dst_roi, int /*nthreads*/)
{
    ROI src_roi_full = src.roi_full();
    ROI dst_roi_full = dst.roi_full();
    ImageBuf::ConstIterator<S, D> s(src);
    ImageBuf::Iterator<D, D> d(dst, dst_roi);
    for (; !d.done(); ++d) {
        int xx = d.x() - dst_roi_full.xbegin;
        s.pos(src_roi_full.xend - 1 - xx, d.y(), d.z());
        for (int c = dst_roi.chbegin; c < dst_roi.chend; ++c)
            d[c] = s[c];
    }
    return true;
}


bool
ImageBufAlgo::flop(ImageBuf& dst, const ImageBuf& src, ROI roi, int nthreads)
{
    if (&dst == &src) {  // Handle in-place operation
        ImageBuf tmp;
        tmp.swap(const_cast<ImageBuf&>(src));
        return flop(dst, tmp, roi, nthreads);
    }

    pvt::LoggedTimer logtime("IBA::flop");
    ROI src_roi      = roi.defined() ? roi : src.roi();
    ROI src_roi_full = src.roi_full();
    int offset       = src_roi.xbegin - src_roi_full.xbegin;
    int start        = src_roi_full.xend - offset - src_roi.width();
    ROI dst_roi(start, start + src_roi.width(), src_roi.ybegin, src_roi.yend,
                src_roi.zbegin, src_roi.zend, src_roi.chbegin, src_roi.chend);
    OIIO_DASSERT(dst_roi.width() == src_roi.width()
                 && dst_roi.height() == src_roi.height());

    // Compute the destination ROI, it's the source ROI reflected across
    // the midline of the display window.
    if (!IBAprep(dst_roi, &dst, &src))
        return false;
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "flop", flop_, dst.spec().format,
                                src.spec().format, dst, src, dst_roi, nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::flip(const ImageBuf& src, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = flip(result, src, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::flip() error");
    return result;
}



ImageBuf
ImageBufAlgo::flop(const ImageBuf& src, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = flop(result, src, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::flop() error");
    return result;
}



template<class D, class S = D>
static bool
rotate90_(ImageBuf& dst, const ImageBuf& src, ROI dst_roi, int /*nthreads*/)
{
    ROI dst_roi_full = dst.roi_full();
    ImageBuf::ConstIterator<S, D> s(src);
    ImageBuf::Iterator<D, D> d(dst, dst_roi);
    for (; !d.done(); ++d) {
        s.pos(d.y(), dst_roi_full.xend - d.x() - 1, d.z());
        for (int c = dst_roi.chbegin; c < dst_roi.chend; ++c)
            d[c] = s[c];
    }
    return true;
}


bool
ImageBufAlgo::rotate90(ImageBuf& dst, const ImageBuf& src, ROI roi,
                       int nthreads)
{
    if (&dst == &src) {  // Handle in-place operation
        ImageBuf tmp;
        tmp.swap(const_cast<ImageBuf&>(src));
        return rotate90(dst, tmp, roi, nthreads);
    }

    pvt::LoggedTimer logtime("IBA::rotate90");
    ROI src_roi      = roi.defined() ? roi : src.roi();
    ROI src_roi_full = src.roi_full();

    // Rotated full ROI swaps width and height, and keeps its origin
    // where the original origin was.
    ROI dst_roi_full(src_roi_full.xbegin,
                     src_roi_full.xbegin + src_roi_full.height(),
                     src_roi_full.ybegin,
                     src_roi_full.ybegin + src_roi_full.width(),
                     src_roi_full.zbegin, src_roi_full.zend,
                     src_roi_full.chbegin, src_roi_full.chend);

    ROI dst_roi(src_roi_full.yend - src_roi.yend,
                src_roi_full.yend - src_roi.ybegin, src_roi.xbegin,
                src_roi.xend, src_roi.zbegin, src_roi.zend, src_roi.chbegin,
                src_roi.chend);
    OIIO_DASSERT(dst_roi.width() == src_roi.height()
                 && dst_roi.height() == src_roi.width());

    bool dst_initialized = dst.initialized();
    if (!IBAprep(dst_roi, &dst, &src))
        return false;
    if (!dst_initialized)
        dst.set_roi_full(dst_roi_full);

    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "rotate90", rotate90_, dst.spec().format,
                                src.spec().format, dst, src, dst_roi, nthreads);
    return ok;
}



template<class D, class S = D>
static bool
rotate180_(ImageBuf& dst, const ImageBuf& src, ROI dst_roi, int /*nthreads*/)
{
    ROI src_roi_full = src.roi_full();
    ROI dst_roi_full = dst.roi_full();
    ImageBuf::ConstIterator<S, D> s(src);
    ImageBuf::Iterator<D, D> d(dst, dst_roi);
    for (; !d.done(); ++d) {
        int xx = d.x() - dst_roi_full.xbegin;
        int yy = d.y() - dst_roi_full.ybegin;
        s.pos(src_roi_full.xend - 1 - xx, src_roi_full.yend - 1 - yy, d.z());
        for (int c = dst_roi.chbegin; c < dst_roi.chend; ++c)
            d[c] = s[c];
    }
    return true;
}


bool
ImageBufAlgo::rotate180(ImageBuf& dst, const ImageBuf& src, ROI roi,
                        int nthreads)
{
    if (&dst == &src) {  // Handle in-place operation
        ImageBuf tmp;
        tmp.swap(const_cast<ImageBuf&>(src));
        return rotate180(dst, tmp, roi, nthreads);
    }

    pvt::LoggedTimer logtime("IBA::rotate180");
    ROI src_roi      = roi.defined() ? roi : src.roi();
    ROI src_roi_full = src.roi_full();
    int xoffset      = src_roi.xbegin - src_roi_full.xbegin;
    int xstart       = src_roi_full.xend - xoffset - src_roi.width();
    int yoffset      = src_roi.ybegin - src_roi_full.ybegin;
    int ystart       = src_roi_full.yend - yoffset - src_roi.height();
    ROI dst_roi(xstart, xstart + src_roi.width(), ystart,
                ystart + src_roi.height(), src_roi.zbegin, src_roi.zend,
                src_roi.chbegin, src_roi.chend);
    OIIO_DASSERT(dst_roi.width() == src_roi.width()
                 && dst_roi.height() == src_roi.height());

    // Compute the destination ROI, it's the source ROI reflected across
    // the midline of the display window.
    if (!IBAprep(dst_roi, &dst, &src))
        return false;
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "rotate180", rotate180_, dst.spec().format,
                                src.spec().format, dst, src, dst_roi, nthreads);
    return ok;
}



template<class D, class S = D>
static bool
rotate270_(ImageBuf& dst, const ImageBuf& src, ROI dst_roi, int /*nthreads*/)
{
    ROI dst_roi_full = dst.roi_full();
    ImageBuf::ConstIterator<S, D> s(src);
    ImageBuf::Iterator<D, D> d(dst, dst_roi);
    for (; !d.done(); ++d) {
        s.pos(dst_roi_full.yend - d.y() - 1, d.x(), d.z());
        for (int c = dst_roi.chbegin; c < dst_roi.chend; ++c)
            d[c] = s[c];
    }
    return true;
}


bool
ImageBufAlgo::rotate270(ImageBuf& dst, const ImageBuf& src, ROI roi,
                        int nthreads)
{
    if (&dst == &src) {  // Handle in-place operation
        ImageBuf tmp;
        tmp.swap(const_cast<ImageBuf&>(src));
        return rotate270(dst, tmp, roi, nthreads);
    }

    pvt::LoggedTimer logtime("IBA::rotate270");
    ROI src_roi      = roi.defined() ? roi : src.roi();
    ROI src_roi_full = src.roi_full();

    // Rotated full ROI swaps width and height, and keeps its origin
    // where the original origin was.
    ROI dst_roi_full(src_roi_full.xbegin,
                     src_roi_full.xbegin + src_roi_full.height(),
                     src_roi_full.ybegin,
                     src_roi_full.ybegin + src_roi_full.width(),
                     src_roi_full.zbegin, src_roi_full.zend,
                     src_roi_full.chbegin, src_roi_full.chend);

    ROI dst_roi(src_roi.ybegin, src_roi.yend, src_roi_full.xend - src_roi.xend,
                src_roi_full.xend - src_roi.xbegin, src_roi.zbegin,
                src_roi.zend, src_roi.chbegin, src_roi.chend);

    OIIO_DASSERT(dst_roi.width() == src_roi.height()
                 && dst_roi.height() == src_roi.width());

    bool dst_initialized = dst.initialized();
    if (!IBAprep(dst_roi, &dst, &src))
        return false;
    if (!dst_initialized)
        dst.set_roi_full(dst_roi_full);

    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "rotate270", rotate270_, dst.spec().format,
                                src.spec().format, dst, src, dst_roi, nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::rotate90(const ImageBuf& src, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = rotate90(result, src, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::rotate90() error");
    return result;
}



ImageBuf
ImageBufAlgo::rotate180(const ImageBuf& src, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = rotate180(result, src, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::rotate180() error");
    return result;
}



ImageBuf
ImageBufAlgo::rotate270(const ImageBuf& src, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = rotate270(result, src, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::rotate270() error");
    return result;
}



bool
ImageBufAlgo::reorient(ImageBuf& dst, const ImageBuf& src, int nthreads)
{
    ImageBuf tmp;
    bool ok = false;
    switch (src.orientation()) {
    case 1: ok = dst.copy(src); break;
    case 2: ok = ImageBufAlgo::flop(dst, src, ROI(), nthreads); break;
    case 3: ok = ImageBufAlgo::rotate180(dst, src, ROI(), nthreads); break;
    case 4: ok = ImageBufAlgo::flip(dst, src, ROI(), nthreads); break;
    case 5:
        ok = ImageBufAlgo::rotate270(tmp, src, ROI(), nthreads);
        if (ok)
            ok = ImageBufAlgo::flop(dst, tmp, ROI(), nthreads);
        else
            dst.errorfmt("{}", tmp.geterror());
        break;
    case 6: ok = ImageBufAlgo::rotate90(dst, src, ROI(), nthreads); break;
    case 7:
        ok = ImageBufAlgo::flip(tmp, src, ROI(), nthreads);
        if (ok)
            ok = ImageBufAlgo::rotate90(dst, tmp, ROI(), nthreads);
        else
            dst.errorfmt("{}", tmp.geterror());
        break;
    case 8: ok = ImageBufAlgo::rotate270(dst, src, ROI(), nthreads); break;
    }
    dst.set_orientation(1);
    return ok;
}



ImageBuf
ImageBufAlgo::reorient(const ImageBuf& src, int nthreads)
{
    ImageBuf result;
    bool ok = reorient(result, src, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::reorient() error");
    return result;
}



template<typename DSTTYPE, typename SRCTYPE = DSTTYPE>
static bool
transpose_(ImageBuf& dst, const ImageBuf& src, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::ConstIterator<SRCTYPE, DSTTYPE> s(src, roi);
        ImageBuf::Iterator<DSTTYPE, DSTTYPE> d(dst);
        for (; !s.done(); ++s) {
            d.pos(s.y(), s.x(), s.z());
            if (!d.exists())
                continue;
            for (int c = roi.chbegin; c < roi.chend; ++c)
                d[c] = s[c];
        }
    });
    return true;
}



bool
ImageBufAlgo::transpose(ImageBuf& dst, const ImageBuf& src, ROI roi,
                        int nthreads)
{
    pvt::LoggedTimer logtime("IBA::transpose");
    if (!roi.defined())
        roi = get_roi(src.spec());
    roi.chend = std::min(roi.chend, src.nchannels());
    ROI dst_roi(roi.ybegin, roi.yend, roi.xbegin, roi.xend, roi.zbegin,
                roi.zend, roi.chbegin, roi.chend);
    bool dst_initialized = dst.initialized();
    ParamValue options[] = { { "dst_format", src.spec().format.c_str() } };
    if (!IBAprep(dst_roi, dst, {}, options))
        return false;
    if (!dst_initialized) {
        ROI r = src.roi_full();
        ROI dst_roi_full(r.ybegin, r.yend, r.xbegin, r.xend, r.zbegin, r.zend,
                         r.chbegin, r.chend);
        dst.set_roi_full(dst_roi_full);
    }
    bool ok;
    if (dst.spec().format == src.spec().format) {
        OIIO_DISPATCH_TYPES(ok, "transpose", transpose_, dst.spec().format, dst,
                            src, roi, nthreads);
    } else {
        OIIO_DISPATCH_COMMON_TYPES2(ok, "transpose", transpose_,
                                    dst.spec().format, src.spec().format, dst,
                                    src, roi, nthreads);
    }
    return ok;
}


ImageBuf
ImageBufAlgo::transpose(const ImageBuf& src, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = transpose(result, src, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::transpose() error");
    return result;
}

OIIO_NAMESPACE_END
