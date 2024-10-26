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


template<class D, class S>
static bool
paste_(ImageBuf& dst, const ImageBuf& src, ROI dstroi, ROI srcroi, int nthreads)
{
    int relative_x = dstroi.xbegin - srcroi.xbegin;
    int relative_y = dstroi.ybegin - srcroi.ybegin;
    int relative_z = dstroi.zbegin - srcroi.zbegin;

    using namespace ImageBufAlgo;
    parallel_image(srcroi, nthreads, [&](ROI roi) {
        ROI droi(roi.xbegin + relative_x, roi.xend + relative_x,
                 roi.ybegin + relative_y, roi.yend + relative_y,
                 roi.zbegin + relative_z, roi.zend + relative_z, dstroi.chbegin,
                 dstroi.chend);
        int src_nchans = src.nchannels();
        int dst_nchans = dst.nchannels();
        ImageBuf::ConstIterator<S, D> s(src, roi);
        ImageBuf::Iterator<D, D> d(dst, droi);
        for (; !s.done(); ++s, ++d) {
            if (!d.exists())
                continue;  // Skip paste-into pixels that don't overlap dst's data
            for (int c = roi.chbegin, c_dst = droi.chbegin; c < roi.chend;
                 ++c, ++c_dst) {
                if (c_dst >= 0 && c_dst < dst_nchans)
                    d[c_dst] = c < src_nchans ? s[c] : D(0);
            }
        }
    });
    return true;
}



static bool
deep_paste_(ImageBuf& dst, const ImageBuf& src, ROI dstroi, ROI srcroi,
            int nthreads)
{
    OIIO_ASSERT(dst.deep() && src.deep());
    int relative_x = dstroi.xbegin - srcroi.xbegin;
    int relative_y = dstroi.ybegin - srcroi.ybegin;
    int relative_z = dstroi.zbegin - srcroi.zbegin;

    // Timer t;

    // First, make sure dst is allocated with enough samples for both. Note:
    // this should be fast if dst is uninitialized or already has the right
    // number of samples in the overlap regions. If not, this will probably
    // be a slow series of allocations and copies. If this is a problem, we
    // can return to optimize it somehow.
    if (!dst.initialized()) {
        dst.reset(src.spec());
    }
    // std::cout << "Reset: " << t.lap() << "\n";
    for (int z = srcroi.zbegin; z < srcroi.zend; ++z) {
        for (int y = srcroi.ybegin; y < srcroi.yend; ++y) {
            // std::cout << "y=" << y << "\n";
            for (int x = srcroi.xbegin; x < srcroi.xend; ++x) {
                dst.set_deep_samples(x + relative_x, y + relative_y,
                                     z + relative_z, src.deep_samples(x, y, z));
            }
        }
    }
    // std::cout << "set samples: " << t.lap() << "\n";

    // Now we can do the deep pixel copies in parallel.
    using namespace ImageBufAlgo;
    parallel_image(srcroi, nthreads, [&](ROI roi) {
        for (int z = roi.zbegin; z < roi.zend; ++z) {
            for (int y = roi.ybegin; y < roi.yend; ++y) {
                for (int x = roi.xbegin; x < roi.xend; ++x) {
                    dst.copy_deep_pixel(x + relative_x, y + relative_y,
                                        z + relative_z, src, x, y, z);
                }
            }
        }
    });
    // std::cout << "copy: " << t.lap() << "\n";
    return true;
}



bool
ImageBufAlgo::paste(ImageBuf& dst, int xbegin, int ybegin, int zbegin,
                    int chbegin, const ImageBuf& src, ROI srcroi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::paste");
    if (!srcroi.defined())
        srcroi = get_roi(src.spec());

    ROI dstroi(srcroi.xbegin + xbegin, srcroi.xbegin + xbegin + srcroi.width(),
               srcroi.ybegin + ybegin, srcroi.ybegin + ybegin + srcroi.height(),
               srcroi.zbegin + zbegin, srcroi.zbegin + zbegin + srcroi.depth(),
               srcroi.chbegin + chbegin,
               srcroi.chbegin + chbegin + srcroi.nchannels());
    ROI dstroi_save = dstroi;  // save the original

    // Special case for deep
    if ((dst.deep() || !dst.initialized()) && src.deep())
        return deep_paste_(dst, src, dstroi, srcroi, nthreads);

    if (!IBAprep(dstroi, &dst))
        return false;

    // do the actual copying
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "paste", paste_, dst.spec().format,
                                src.spec().format, dst, src, dstroi_save,
                                srcroi, nthreads);
    return ok;
}



template<class D, class S>
static bool
copy_(ImageBuf& dst, const ImageBuf& src, ROI roi, int nthreads = 1)
{
    using namespace ImageBufAlgo;
    parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::ConstIterator<S, D> s(src, roi);
        ImageBuf::Iterator<D, D> d(dst, roi);
        for (; !d.done(); ++d, ++s) {
            for (int c = roi.chbegin; c < roi.chend; ++c)
                d[c] = s[c];
        }
    });
    return true;
}



static bool
copy_deep(ImageBuf& dst, const ImageBuf& src, ROI roi, int nthreads = 1)
{
    OIIO_ASSERT(dst.deep() && src.deep());
    using namespace ImageBufAlgo;
    parallel_image(roi, nthreads, [&](ROI roi) {
        DeepData& dstdeep(*dst.deepdata());
        const DeepData& srcdeep(*src.deepdata());
        ImageBuf::ConstIterator<float> s(src, roi);
        for (ImageBuf::Iterator<float> d(dst, roi); !d.done(); ++d, ++s) {
            int samples = s.deep_samples();
            // The caller should ALREADY have set the samples, since that
            // is not thread-safe against the copying below.
            // d.set_deep_samples (samples);
            OIIO_DASSERT(d.deep_samples() == samples);
            if (samples == 0)
                continue;
            for (int c = roi.chbegin; c < roi.chend; ++c) {
                if (dstdeep.channeltype(c) == TypeDesc::UINT32
                    && srcdeep.channeltype(c) == TypeDesc::UINT32)
                    for (int samp = 0; samp < samples; ++samp)
                        d.set_deep_value(c, samp,
                                         (uint32_t)s.deep_value_uint(c, samp));
                else
                    for (int samp = 0; samp < samples; ++samp)
                        d.set_deep_value(c, samp, (float)s.deep_value(c, samp));
            }
        }
    });
    return true;
}



bool
ImageBufAlgo::copy(ImageBuf& dst, const ImageBuf& src, TypeDesc convert,
                   ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::copy");
    if (&dst == &src)  // trivial copy to self
        return true;

    roi.chend = std::min(roi.chend, src.nchannels());
    if (!dst.initialized()) {
        ImageSpec newspec = src.spec();
        if (!roi.defined())
            roi = src.roi();
        set_roi(newspec, roi);
        newspec.nchannels = roi.chend;
        if (convert != TypeUnknown)
            newspec.set_format(convert);
        dst.reset(newspec);
    }
    IBAprep(roi, &dst, &src, IBAprep_SUPPORT_DEEP);
    if (dst.deep()) {
        // If it's deep, figure out the sample allocations first, because
        // it's not thread-safe to do that simultaneously with copying the
        // values.
        ImageBuf::ConstIterator<float> s(src, roi);
        for (ImageBuf::Iterator<float> d(dst, roi); !d.done(); ++d, ++s)
            d.set_deep_samples(s.deep_samples());
        return copy_deep(dst, src, roi, nthreads);
    }

    if (src.localpixels() && src.roi().contains(roi)) {
        // Easy case -- if the buffer is already fully in memory and the roi
        // is completely contained in the pixel window, this reduces to a
        // parallel_convert_image, which is both threaded and already
        // handles many special cases.
        return parallel_convert_image(
            roi.nchannels(), roi.width(), roi.height(), roi.depth(),
            src.pixeladdr(roi.xbegin, roi.ybegin, roi.zbegin, roi.chbegin),
            src.spec().format, src.pixel_stride(), src.scanline_stride(),
            src.z_stride(),
            dst.pixeladdr(roi.xbegin, roi.ybegin, roi.zbegin, roi.chbegin),
            dst.spec().format, dst.pixel_stride(), dst.scanline_stride(),
            dst.z_stride(), nthreads);
    }

    bool ok;
    OIIO_DISPATCH_TYPES2(ok, "copy", copy_, dst.spec().format,
                         src.spec().format, dst, src, roi, nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::copy(const ImageBuf& src, TypeDesc convert, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = copy(result, src, convert, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::copy() error");
    return result;
}



bool
ImageBufAlgo::crop(ImageBuf& dst, const ImageBuf& src, ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::crop");
    dst.clear();
    roi.chend = std::min(roi.chend, src.nchannels());
    if (!IBAprep(roi, &dst, &src, IBAprep_SUPPORT_DEEP))
        return false;

    if (dst.deep()) {
        // If it's deep, figure out the sample allocations first, because
        // it's not thread-safe to do that simultaneously with copying the
        // values.
        ImageBuf::ConstIterator<float> s(src, roi);
        for (ImageBuf::Iterator<float> d(dst, roi); !d.done(); ++d, ++s)
            d.set_deep_samples(s.deep_samples());
        return copy_deep(dst, src, roi, nthreads);
    }

    if (src.localpixels() && src.roi().contains(roi)) {
        // Easy case -- if the buffer is already fully in memory and the roi
        // is completely contained in the pixel window, this reduces to a
        // parallel_convert_image, which is both threaded and already
        // handles many special cases.
        return parallel_convert_image(
            roi.nchannels(), roi.width(), roi.height(), roi.depth(),
            src.pixeladdr(roi.xbegin, roi.ybegin, roi.zbegin, roi.chbegin),
            src.spec().format, src.pixel_stride(), src.scanline_stride(),
            src.z_stride(),
            dst.pixeladdr(roi.xbegin, roi.ybegin, roi.zbegin, roi.chbegin),
            dst.spec().format, dst.pixel_stride(), dst.scanline_stride(),
            dst.z_stride(), nthreads);
    }

    bool ok;
    OIIO_DISPATCH_TYPES2(ok, "crop", copy_, dst.spec().format,
                         src.spec().format, dst, src, roi, nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::crop(const ImageBuf& src, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = crop(result, src, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::crop() error");
    return result;
}



bool
ImageBufAlgo::cut(ImageBuf& dst, const ImageBuf& src, ROI roi, int nthreads)
{
    // pvt::LoggedTimer logtime("IBA::cut");
    // Don't log, because all the work is inside crop, which already logs
    bool ok = crop(dst, src, roi, nthreads);
    if (!ok)
        return false;
    // Crop did the heavy lifting of copying the roi of pixels from src to
    // dst, but now we need to make it look like we cut that rectangle out
    // and repositioned it at the origin.
    dst.specmod().x = 0;
    dst.specmod().y = 0;
    dst.specmod().z = 0;
    dst.set_roi_full(dst.roi());
    return true;
}



ImageBuf
ImageBufAlgo::cut(const ImageBuf& src, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = cut(result, src, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::cut() error");
    return result;
}



template<typename DSTTYPE, typename SRCTYPE>
static bool
circular_shift_(ImageBuf& dst, const ImageBuf& src, int xshift, int yshift,
                int zshift, ROI dstroi, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        int width = dstroi.width(), height = dstroi.height(),
            depth = dstroi.depth();
        ImageBuf::ConstIterator<SRCTYPE, DSTTYPE> s(src, roi);
        ImageBuf::Iterator<DSTTYPE, DSTTYPE> d(dst);
        for (; !s.done(); ++s) {
            int dx = s.x() + xshift;
            OIIO::wrap_periodic(dx, dstroi.xbegin, width);
            int dy = s.y() + yshift;
            OIIO::wrap_periodic(dy, dstroi.ybegin, height);
            int dz = s.z() + zshift;
            OIIO::wrap_periodic(dz, dstroi.zbegin, depth);
            d.pos(dx, dy, dz);
            if (!d.exists())
                continue;
            for (int c = roi.chbegin; c < roi.chend; ++c)
                d[c] = s[c];
        }
    });
    return true;
}



bool
ImageBufAlgo::circular_shift(ImageBuf& dst, const ImageBuf& src, int xshift,
                             int yshift, int zshift, ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::circular_shift");
    if (!IBAprep(roi, &dst, &src))
        return false;
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "circular_shift", circular_shift_,
                                dst.spec().format, src.spec().format, dst, src,
                                xshift, yshift, zshift, roi, roi, nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::circular_shift(const ImageBuf& src, int xshift, int yshift,
                             int zshift, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = circular_shift(result, src, xshift, yshift, zshift, roi,
                             nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::circular_shift() error");
    return result;
}



OIIO_NAMESPACE_END
