/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/

/// \file
/// Implementation of ImageBufAlgo algorithms that merely move pixels
/// or channels between images without altering their values.


#include <OpenEXR/half.h>

#include <cmath>
#include <iostream>

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/deepdata.h>
#include <OpenImageIO/thread.h>



OIIO_NAMESPACE_BEGIN


template<class D, class S>
static bool
paste_ (ImageBuf &dst, ROI dstroi,
        const ImageBuf &src, ROI srcroi, int nthreads)
{
    // N.B. Punt on parallelizing because of the subtle interplay
    // between srcroi and dstroi, the parallel_image idiom doesn't
    // handle that especially well. And it's not worth customizing for
    // this function which is inexpensive and not commonly used, and so
    // would benefit little from parallelizing. We can always revisit
    // this later. But in the mean time, we maintain the 'nthreads'
    // parameter for uniformity with the rest of IBA.
    int src_nchans = src.nchannels ();
    int dst_nchans = dst.nchannels ();
    ImageBuf::ConstIterator<S,D> s (src, srcroi);
    ImageBuf::Iterator<D,D> d (dst, dstroi);
    for ( ;  ! s.done();  ++s, ++d) {
        if (! d.exists())
            continue;  // Skip paste-into pixels that don't overlap dst's data
        for (int c = srcroi.chbegin, c_dst = dstroi.chbegin;
             c < srcroi.chend;  ++c, ++c_dst) {
            if (c_dst >= 0 && c_dst < dst_nchans)
                d[c_dst] = c < src_nchans ? s[c] : D(0);
        }
    }
    return true;
}



bool
ImageBufAlgo::paste (ImageBuf &dst, int xbegin, int ybegin,
                     int zbegin, int chbegin,
                     const ImageBuf &src, ROI srcroi, int nthreads)
{
    if (! srcroi.defined())
        srcroi = get_roi(src.spec());

    ROI dstroi (xbegin, xbegin+srcroi.width(),
                ybegin, ybegin+srcroi.height(),
                zbegin, zbegin+srcroi.depth(),
                chbegin, chbegin+srcroi.nchannels());
    ROI dstroi_save = dstroi;  // save the original
    if (! IBAprep (dstroi, &dst))
        return false;

    // do the actual copying
    bool ok;
    OIIO_DISPATCH_TYPES2 (ok, "paste", paste_, dst.spec().format, src.spec().format,
                          dst, dstroi_save, src, srcroi, nthreads);
    return ok;
}




template<class D, class S>
static bool
copy_ (ImageBuf &dst, const ImageBuf &src,
       ROI roi, int nthreads=1)
{
    using namespace ImageBufAlgo;
    parallel_image (roi, nthreads, [&](ROI roi){

    if (dst.deep()) {
        DeepData &dstdeep (*dst.deepdata());
        const DeepData &srcdeep (*src.deepdata());
        ImageBuf::ConstIterator<S,D> s (src, roi);
        for (ImageBuf::Iterator<D,D> d (dst, roi);  ! d.done();  ++d, ++s) {
            int samples = s.deep_samples ();
            // The caller should ALREADY have set the samples, since that
            // is not thread-safe against the copying below.
            // d.set_deep_samples (samples);
            DASSERT (d.deep_samples() == samples);
            if (samples == 0)
                continue;
            for (int c = roi.chbegin;  c < roi.chend;  ++c) {
                if (dstdeep.channeltype(c) == TypeDesc::UINT32 &&
                        srcdeep.channeltype(c) == TypeDesc::UINT32)
                    for (int samp = 0; samp < samples; ++samp)
                        d.set_deep_value (c, samp, (uint32_t)s.deep_value_uint(c, samp));
                else
                    for (int samp = 0; samp < samples; ++samp)
                        d.set_deep_value (c, samp, (float)s.deep_value(c, samp));
            }
        }
    } else {
        ImageBuf::ConstIterator<S,D> s (src, roi);
        ImageBuf::Iterator<D,D> d (dst, roi);
        for ( ;  ! d.done();  ++d, ++s) {
            for (int c = roi.chbegin;  c < roi.chend;  ++c)
                d[c] = s[c];
        }
    }

    });
    return true;
}


bool
ImageBufAlgo::copy (ImageBuf &dst, const ImageBuf &src, TypeDesc convert,
                    ROI roi, int nthreads)
{
    if (&dst == &src)   // trivial copy to self
        return true;

    roi.chend = std::min (roi.chend, src.nchannels());
    if (! dst.initialized()) {
        ImageSpec newspec = src.spec();
        if (! roi.defined())
            roi = src.roi();
        set_roi (newspec, roi);
        newspec.nchannels = roi.chend;
        if (convert != TypeUnknown)
            newspec.set_format (convert);
        dst.reset (newspec);
    }
    IBAprep (roi, &dst, &src, IBAprep_SUPPORT_DEEP);
    if (dst.deep()) {
        // If it's deep, figure out the sample allocations first, because
        // it's not thread-safe to do that simultaneously with copying the
        // values.
        ImageBuf::ConstIterator<float> s (src, roi);
        for (ImageBuf::Iterator<float> d (dst, roi);  !d.done();  ++d, ++s)
            d.set_deep_samples (s.deep_samples());
    }
    bool ok;
    OIIO_DISPATCH_TYPES2 (ok, "copy", copy_, dst.spec().format, src.spec().format,
                          dst, src, roi, nthreads);
    return ok;
}



bool
ImageBufAlgo::crop (ImageBuf &dst, const ImageBuf &src,
                    ROI roi, int nthreads)
{
    dst.clear ();
    roi.chend = std::min (roi.chend, src.nchannels());
    if (! IBAprep (roi, &dst, &src, IBAprep_SUPPORT_DEEP))
        return false;

    if (dst.deep()) {
        // If it's deep, figure out the sample allocations first, because
        // it's not thread-safe to do that simultaneously with copying the
        // values.
        ImageBuf::ConstIterator<float> s (src, roi);
        for (ImageBuf::Iterator<float> d (dst, roi);  !d.done();  ++d, ++s)
            d.set_deep_samples (s.deep_samples());
    }

    bool ok;
    OIIO_DISPATCH_TYPES2 (ok, "crop", copy_, dst.spec().format, src.spec().format,
                          dst, src, roi, nthreads);
    return ok;
}



bool 
ImageBufAlgo::cut (ImageBuf &dst, const ImageBuf &src,
                   ROI roi, int nthreads)
{
    bool ok = crop (dst, src, roi, nthreads);
    ASSERT(ok);
    if (! ok)
        return false;
    // Crop did the heavy lifting of copying the roi of pixels from src to
    // dst, but now we need to make it look like we cut that rectangle out
    // and repositioned it at the origin.
    dst.specmod().x = 0;
    dst.specmod().y = 0;
    dst.specmod().z = 0;
    dst.set_roi_full (dst.roi());
    return true;
}



template<class D, class S>
static bool
flip_ (ImageBuf &dst, const ImageBuf &src, ROI dst_roi, int nthreads)
{
    ROI src_roi_full = src.roi_full();
    ROI dst_roi_full = dst.roi_full();
    ImageBuf::ConstIterator<S, D> s (src);
    ImageBuf::Iterator<D, D> d (dst, dst_roi);
    for ( ; ! d.done(); ++d) {
        int yy = d.y() - dst_roi_full.ybegin;
        s.pos (d.x(), src_roi_full.yend-1 - yy, d.z());
        for (int c = dst_roi.chbegin; c < dst_roi.chend; ++c)
            d[c] = s[c];
    }
    return true;
}


bool
ImageBufAlgo::flip(ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    if (&dst == &src) {    // Handle in-place operation
        ImageBuf tmp;
        tmp.swap (const_cast<ImageBuf&>(src));
        return flip (dst, tmp, roi, nthreads);
    }

    ROI src_roi = roi.defined() ? roi : src.roi();
    ROI src_roi_full = src.roi_full();
    int offset = src_roi.ybegin - src_roi_full.ybegin;
    int start = src_roi_full.yend - offset - src_roi.height();
    ROI dst_roi (src_roi.xbegin, src_roi.xend,
                 start, start+src_roi.height(),
                 src_roi.zbegin, src_roi.zend,
                 src_roi.chbegin, src_roi.chend);
    ASSERT (dst_roi.width() == src_roi.width() &&
            dst_roi.height() == src_roi.height());

    // Compute the destination ROI, it's the source ROI reflected across
    // the midline of the display window.
    if (! IBAprep (dst_roi, &dst, &src))
        return false;
    bool ok;
    OIIO_DISPATCH_TYPES2 (ok, "flip", flip_,
                          dst.spec().format, src.spec().format,
                          dst, src, dst_roi, nthreads);
    return ok;
}



template<class D, class S>
static bool
flop_ (ImageBuf &dst, const ImageBuf &src, ROI dst_roi, int nthreads)
{
    ROI src_roi_full = src.roi_full();
    ROI dst_roi_full = dst.roi_full();
    ImageBuf::ConstIterator<S, D> s (src);
    ImageBuf::Iterator<D, D> d (dst, dst_roi);
    for ( ; ! d.done(); ++d) {
        int xx = d.x() - dst_roi_full.xbegin;
        s.pos (src_roi_full.xend-1 - xx, d.y(), d.z());
        for (int c = dst_roi.chbegin; c < dst_roi.chend; ++c)
            d[c] = s[c];
    }
    return true;
}


bool
ImageBufAlgo::flop(ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    if (&dst == &src) {    // Handle in-place operation
        ImageBuf tmp;
        tmp.swap (const_cast<ImageBuf&>(src));
        return flop (dst, tmp, roi, nthreads);
    }

    ROI src_roi = roi.defined() ? roi : src.roi();
    ROI src_roi_full = src.roi_full();
    int offset = src_roi.xbegin - src_roi_full.xbegin;
    int start = src_roi_full.xend - offset - src_roi.width();
    ROI dst_roi (start, start+src_roi.width(),
                 src_roi.ybegin, src_roi.yend,
                 src_roi.zbegin, src_roi.zend,
                 src_roi.chbegin, src_roi.chend);
    ASSERT (dst_roi.width() == src_roi.width() &&
            dst_roi.height() == src_roi.height());

    // Compute the destination ROI, it's the source ROI reflected across
    // the midline of the display window.
    if (! IBAprep (dst_roi, &dst, &src))
        return false;
    bool ok;
    OIIO_DISPATCH_TYPES2 (ok, "flop", flop_,
                          dst.spec().format, src.spec().format,
                          dst, src, dst_roi, nthreads);
    return ok;
}



template<class D, class S>
static bool
rotate90_ (ImageBuf &dst, const ImageBuf &src, ROI dst_roi, int nthreads)
{
    ROI dst_roi_full = dst.roi_full();
    ImageBuf::ConstIterator<S, D> s (src);
    ImageBuf::Iterator<D, D> d (dst, dst_roi);
    for ( ; ! d.done(); ++d) {
        s.pos (d.y(),
               dst_roi_full.xend - d.x() - 1,
               d.z());
        for (int c = dst_roi.chbegin; c < dst_roi.chend; ++c)
            d[c] = s[c];
    }
    return true;
}


bool
ImageBufAlgo::rotate90 (ImageBuf &dst, const ImageBuf &src,
                        ROI roi, int nthreads)
{
    if (&dst == &src) {    // Handle in-place operation
        ImageBuf tmp;
        tmp.swap (const_cast<ImageBuf&>(src));
        return rotate90 (dst, tmp, roi, nthreads);
    }

    ROI src_roi = roi.defined() ? roi : src.roi();
    ROI src_roi_full = src.roi_full();

    // Rotated full ROI swaps width and height, and keeps its origin
    // where the original origin was.
    ROI dst_roi_full (src_roi_full.xbegin, src_roi_full.xbegin+src_roi_full.height(),
                      src_roi_full.ybegin, src_roi_full.ybegin+src_roi_full.width(),
                      src_roi_full.zbegin, src_roi_full.zend,
                      src_roi_full.chbegin, src_roi_full.chend);

    ROI dst_roi (src_roi_full.yend-src_roi.yend,
                 src_roi_full.yend-src_roi.ybegin,
                 src_roi.xbegin, src_roi.xend,
                 src_roi.zbegin, src_roi.zend,
                 src_roi.chbegin, src_roi.chend);
    ASSERT (dst_roi.width() == src_roi.height() &&
            dst_roi.height() == src_roi.width());

    bool dst_initialized = dst.initialized();
    if (! IBAprep (dst_roi, &dst, &src))
        return false;
    if (! dst_initialized)
        dst.set_roi_full (dst_roi_full);

    bool ok;
    OIIO_DISPATCH_TYPES2 (ok, "rotate90", rotate90_,
                          dst.spec().format, src.spec().format,
                          dst, src, dst_roi, nthreads);
    return ok;
}



template<class D, class S>
static bool
rotate180_ (ImageBuf &dst, const ImageBuf &src, ROI dst_roi, int nthreads)
{
    ROI src_roi_full = src.roi_full();
    ROI dst_roi_full = dst.roi_full();
    ImageBuf::ConstIterator<S, D> s (src);
    ImageBuf::Iterator<D, D> d (dst, dst_roi);
    for ( ; ! d.done(); ++d) {
        int xx = d.x() - dst_roi_full.xbegin;
        int yy = d.y() - dst_roi_full.ybegin;
        s.pos (src_roi_full.xend-1 - xx,
               src_roi_full.yend-1 - yy,
               d.z());
        for (int c = dst_roi.chbegin; c < dst_roi.chend; ++c)
            d[c] = s[c];
    }
    return true;
}


bool
ImageBufAlgo::rotate180 (ImageBuf &dst, const ImageBuf &src,
                         ROI roi, int nthreads)
{
    if (&dst == &src) {    // Handle in-place operation
        ImageBuf tmp;
        tmp.swap (const_cast<ImageBuf&>(src));
        return rotate180 (dst, tmp, roi, nthreads);
    }

    ROI src_roi = roi.defined() ? roi : src.roi();
    ROI src_roi_full = src.roi_full();
    int xoffset = src_roi.xbegin - src_roi_full.xbegin;
    int xstart = src_roi_full.xend - xoffset - src_roi.width();
    int yoffset = src_roi.ybegin - src_roi_full.ybegin;
    int ystart = src_roi_full.yend - yoffset - src_roi.height();
    ROI dst_roi (xstart, xstart+src_roi.width(),
                 ystart, ystart+src_roi.height(),
                 src_roi.zbegin, src_roi.zend,
                 src_roi.chbegin, src_roi.chend);
    ASSERT (dst_roi.width() == src_roi.width() &&
            dst_roi.height() == src_roi.height());

    // Compute the destination ROI, it's the source ROI reflected across
    // the midline of the display window.
    if (! IBAprep (dst_roi, &dst, &src))
        return false;
    bool ok;
    OIIO_DISPATCH_TYPES2 (ok, "rotate180", rotate180_,
                          dst.spec().format, src.spec().format,
                          dst, src, dst_roi, nthreads);
    return ok;
}



template<class D, class S>
static bool
rotate270_ (ImageBuf &dst, const ImageBuf &src, ROI dst_roi, int nthreads)
{
    ROI dst_roi_full = dst.roi_full();
    ImageBuf::ConstIterator<S, D> s (src);
    ImageBuf::Iterator<D, D> d (dst, dst_roi);
    for ( ; ! d.done(); ++d) {
        s.pos (dst_roi_full.yend - d.y() - 1,
               d.x(),
               d.z());
        for (int c = dst_roi.chbegin; c < dst_roi.chend; ++c)
            d[c] = s[c];
    }
    return true;
}


bool
ImageBufAlgo::rotate270 (ImageBuf &dst, const ImageBuf &src,
                         ROI roi, int nthreads)
{
    if (&dst == &src) {    // Handle in-place operation
        ImageBuf tmp;
        tmp.swap (const_cast<ImageBuf&>(src));
        return rotate270 (dst, tmp, roi, nthreads);
    }

    ROI src_roi = roi.defined() ? roi : src.roi();
    ROI src_roi_full = src.roi_full();

    // Rotated full ROI swaps width and height, and keeps its origin
    // where the original origin was.
    ROI dst_roi_full (src_roi_full.xbegin, src_roi_full.xbegin+src_roi_full.height(),
                      src_roi_full.ybegin, src_roi_full.ybegin+src_roi_full.width(),
                      src_roi_full.zbegin, src_roi_full.zend,
                      src_roi_full.chbegin, src_roi_full.chend);

    ROI dst_roi (src_roi.ybegin, src_roi.yend,
                 src_roi_full.xend-src_roi.xend,
                 src_roi_full.xend-src_roi.xbegin,
                 src_roi.zbegin, src_roi.zend,
                 src_roi.chbegin, src_roi.chend);

    ASSERT (dst_roi.width() == src_roi.height() &&
            dst_roi.height() == src_roi.width());

    bool dst_initialized = dst.initialized();
    if (! IBAprep (dst_roi, &dst, &src))
        return false;
    if (! dst_initialized)
        dst.set_roi_full (dst_roi_full);

    bool ok;
    OIIO_DISPATCH_TYPES2 (ok, "rotate270", rotate270_,
                          dst.spec().format, src.spec().format,
                          dst, src, dst_roi, nthreads);
    return ok;
}



bool
ImageBufAlgo::reorient (ImageBuf &dst, const ImageBuf &src, int nthreads)
{
    ImageBuf tmp;
    bool ok = false;
    switch (src.orientation()) {
    case 1:
        ok = dst.copy (src);
        break;
    case 2:
        ok = ImageBufAlgo::flop (dst, src);
        break;
    case 3:
        ok = ImageBufAlgo::rotate180 (dst, src);
        break;
    case 4:
        ok = ImageBufAlgo::flip (dst, src);
        break;
    case 5:
        ok = ImageBufAlgo::rotate270 (tmp, src);
        if (ok)
            ok = ImageBufAlgo::flop (dst, tmp);
        else
            dst.error ("%s", tmp.geterror());
        break;
    case 6:
        ok = ImageBufAlgo::rotate90 (dst, src);
        break;
    case 7:
        ok = ImageBufAlgo::flip (tmp, src);
        if (ok)
            ok = ImageBufAlgo::rotate90 (dst, tmp);
        else
            dst.error ("%s", tmp.geterror());
        break;
    case 8:
        ok = ImageBufAlgo::rotate270 (dst, src);
        break;
    }
    dst.set_orientation (1);
    return ok;
}



template<typename DSTTYPE, typename SRCTYPE>
static bool
transpose_ (ImageBuf &dst, const ImageBuf &src,
            ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        ImageBuf::ConstIterator<SRCTYPE,DSTTYPE> s (src, roi);
        ImageBuf::Iterator<DSTTYPE,DSTTYPE> d (dst);
        for (  ;  ! s.done();  ++s) {
            d.pos (s.y(), s.x(), s.z());
            if (! d.exists())
                continue;
            for (int c = roi.chbegin;  c < roi.chend;  ++c)
                d[c] = s[c];
        }
    });
    return true;
}



bool
ImageBufAlgo::transpose (ImageBuf &dst, const ImageBuf &src,
                         ROI roi, int nthreads)
{
    if (! roi.defined())
        roi = get_roi (src.spec());
    roi.chend = std::min (roi.chend, src.nchannels());
    ROI dst_roi (roi.ybegin, roi.yend, roi.xbegin, roi.xend,
                 roi.zbegin, roi.zend, roi.chbegin, roi.chend);
    bool dst_initialized = dst.initialized();
    if (! IBAprep (dst_roi, &dst))
        return false;
    if (! dst_initialized) {
        ROI r = src.roi_full();
        ROI dst_roi_full (r.ybegin, r.yend, r.xbegin, r.xend,
                          r.zbegin, r.zend, r.chbegin, r.chend);
        dst.set_roi_full (dst_roi_full);
    }
    bool ok;
    OIIO_DISPATCH_TYPES2 (ok, "transpose", transpose_, dst.spec().format,
                          src.spec().format, dst, src, roi, nthreads);
    return ok;
}



template<typename DSTTYPE, typename SRCTYPE>
static bool
circular_shift_ (ImageBuf &dst, const ImageBuf &src,
                 int xshift, int yshift, int zshift,
                 ROI dstroi, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        int width = dstroi.width(), height = dstroi.height(), depth = dstroi.depth();
        ImageBuf::ConstIterator<SRCTYPE,DSTTYPE> s (src, roi);
        ImageBuf::Iterator<DSTTYPE,DSTTYPE> d (dst);
        for (  ;  ! s.done();  ++s) {
            int dx = s.x() + xshift;  OIIO::wrap_periodic (dx, dstroi.xbegin, width);
            int dy = s.y() + yshift;  OIIO::wrap_periodic (dy, dstroi.ybegin, height);
            int dz = s.z() + zshift;  OIIO::wrap_periodic (dz, dstroi.zbegin, depth);
            d.pos (dx, dy, dz);
            if (! d.exists())
                continue;
            for (int c = roi.chbegin;  c < roi.chend;  ++c)
                d[c] = s[c];
        }
    });
    return true;
}



bool
ImageBufAlgo::circular_shift (ImageBuf &dst, const ImageBuf &src,
                              int xshift, int yshift, int zshift,
                              ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &src))
        return false;
    bool ok;
    OIIO_DISPATCH_TYPES2 (ok, "circular_shift", circular_shift_,
                          dst.spec().format, src.spec().format, dst, src,
                          xshift, yshift, zshift, roi, roi, nthreads);
    return ok;
}



template<typename DSTTYPE>
static bool
channels_ (ImageBuf &dst, const ImageBuf &src,
           const int *channelorder, const float *channelvalues,
           ROI roi, int nthreads=0)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        int nchannels = src.nchannels();
        ImageBuf::ConstIterator<DSTTYPE> s (src, roi);
        ImageBuf::Iterator<DSTTYPE> d (dst, roi);
        for (  ;  ! s.done();  ++s, ++d) {
            for (int c = roi.chbegin;  c < roi.chend;  ++c) {
                int cc = channelorder[c];
                if (cc >= 0 && cc < nchannels)
                    d[c] = s[cc];
                else if (channelvalues)
                    d[c] = channelvalues[c];
            }
        }
    });
    return true;
}



bool
ImageBufAlgo::channels (ImageBuf &dst, const ImageBuf &src,
                        int nchannels, const int *channelorder,
                        const float *channelvalues,
                        const std::string *newchannelnames,
                        bool shuffle_channel_names, int nthreads)
{
    // Not intended to create 0-channel images.
    if (nchannels <= 0) {
        dst.error ("%d-channel images not supported", nchannels);
        return false;
    }
    // If we dont have a single source channel,
    // hard to know how big to make the additional channels
    if (src.spec().nchannels == 0) {
        dst.error ("%d-channel images not supported", src.spec().nchannels);
        return false;
    }

    // If channelorder is NULL, it will be interpreted as
    // {0, 1, ..., nchannels-1}.
    int *local_channelorder = NULL;
    if (! channelorder) {
        local_channelorder = ALLOCA (int, nchannels);
        for (int c = 0;  c < nchannels;  ++c)
            local_channelorder[c] = c;
        channelorder = local_channelorder;
    }

    // If this is the identity transformation, just do a simple copy
    bool inorder = true;
    for (int c = 0;  c < nchannels;  ++c) {
        inorder &= (channelorder[c] == c);
        if (newchannelnames && newchannelnames[c].size() &&
                c < int(src.spec().channelnames.size()))
            inorder &= (newchannelnames[c] == src.spec().channelnames[c]);
    }
    if (nchannels == src.spec().nchannels && inorder) {
        return dst.copy (src);
    }

    // Construct a new ImageSpec that describes the desired channel ordering.
    ImageSpec newspec = src.spec();
    newspec.nchannels = nchannels;
    newspec.default_channel_names ();
    newspec.channelformats.clear();
    newspec.alpha_channel = -1;
    newspec.z_channel = -1;
    bool all_same_type = true;
    for (int c = 0; c < nchannels;  ++c) {
        int csrc = channelorder[c];
        // If the user gave an explicit name for this channel, use it...
        if (newchannelnames && newchannelnames[c].size())
            newspec.channelnames[c] = newchannelnames[c];
        // otherwise, if shuffle_channel_names, use the channel name of
        // the src channel we're using (otherwise stick to the default name)
        else if (shuffle_channel_names &&
                 csrc >= 0 && csrc < src.spec().nchannels)
            newspec.channelnames[c] = src.spec().channelnames[csrc];
        // otherwise, use the name of the source in that slot
        else if (csrc >= 0 && csrc < src.spec().nchannels) {
            newspec.channelnames[c] = src.spec().channelnames[csrc];
        }
        TypeDesc type = src.spec().channelformat(csrc);
        newspec.channelformats.push_back (type);
        if (type != newspec.channelformats.front())
            all_same_type = false;
        // Use the names (or designation of the src image, if
        // shuffle_channel_names is true) to deduce the alpha and z channels.
        if ((shuffle_channel_names && csrc == src.spec().alpha_channel) ||
              Strutil::iequals (newspec.channelnames[c], "A") ||
              Strutil::iequals (newspec.channelnames[c], "alpha"))
            newspec.alpha_channel = c;
        if ((shuffle_channel_names && csrc == src.spec().z_channel) ||
              Strutil::iequals (newspec.channelnames[c], "Z"))
            newspec.z_channel = c;
    }
    if (all_same_type)                      // clear per-chan formats if
        newspec.channelformats.clear();     // they're all the same

    // Update the image (realloc with the new spec)
    dst.reset (newspec);

    if (dst.deep()) {
        // Deep case:
        ASSERT (src.deep() && src.deepdata() && dst.deepdata());
        const DeepData &srcdata (*src.deepdata());
        DeepData &dstdata (*dst.deepdata());
        // The earlier dst.alloc() already called dstdata.init()
        for (int p = 0, npels = (int)newspec.image_pixels(); p < npels; ++p)
            dstdata.set_samples (p, srcdata.samples(p));
        for (int p = 0, npels = (int)newspec.image_pixels(); p < npels; ++p) {
            if (! dstdata.samples(p))
                continue;   // no samples for this pixel
            for (int c = 0;  c < newspec.nchannels;  ++c) {
                int csrc = channelorder[c];
                if (csrc < 0) {
                    // Replacing the channel with a new value
                    float val = channelvalues ? channelvalues[c] : 0.0f;
                    for (int s = 0, ns = dstdata.samples(p); s < ns; ++s)
                        dstdata.set_deep_value (p, c, s, val);
                } else {
                    if (dstdata.channeltype(c) == TypeDesc::UINT)
                        for (int s = 0, ns = dstdata.samples(p); s < ns; ++s)
                            dstdata.set_deep_value (p, c, s,
                                          srcdata.deep_value_uint(p,csrc,s));
                    else
                        for (int s = 0, ns = dstdata.samples(p); s < ns; ++s)
                            dstdata.set_deep_value (p, c, s,
                                          srcdata.deep_value(p,csrc,s));
                }
            }
        }
        return true;
    }
    // Below is the non-deep case

    bool ok;
    OIIO_DISPATCH_TYPES (ok, "channels", channels_,
                         dst.spec().format, dst, src,
                         channelorder, channelvalues, dst.roi(), nthreads);
    return ok;
}



template<class Rtype, class Atype, class Btype>
static bool
channel_append_impl (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                     ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        int na = A.nchannels(), nb = B.nchannels();
        int n = std::min (dst.nchannels(), na+nb);
        ImageBuf::Iterator<Rtype> r (dst, roi);
        ImageBuf::ConstIterator<Atype> a (A, roi);
        ImageBuf::ConstIterator<Btype> b (B, roi);
        for (;  !r.done();  ++r, ++a, ++b) {
            for (int c = 0; c < n; ++c) {
                if (c < na)
                    r[c] = a.exists() ? a[c] : 0.0f;
                else
                    r[c] = b.exists() ? b[c-na] : 0.0f;
            }
        }
    });
    return true;
}


bool
ImageBufAlgo::channel_append (ImageBuf &dst, const ImageBuf &A,
                              const ImageBuf &B, ROI roi,
                              int nthreads)
{
    // If the region is not defined, set it to the union of the valid
    // regions of the two source images.
    if (! roi.defined())
        roi = roi_union (get_roi (A.spec()), get_roi (B.spec()));

    // If dst has not already been allocated, set it to the right size,
    // make it unconditinally float.
    if (! dst.pixels_valid()) {
        ImageSpec dstspec = A.spec();
        dstspec.set_format (TypeFloat);
        // Append the channel descriptions
        dstspec.nchannels = A.spec().nchannels + B.spec().nchannels;
        for (int c = 0;  c < B.spec().nchannels;  ++c) {
            std::string name = B.spec().channelnames[c];
            // It's a duplicate channel name. This will wreak havoc for
            // OpenEXR, so we need to choose a unique name.
            if (std::find(dstspec.channelnames.begin(), dstspec.channelnames.end(), name) != dstspec.channelnames.end()) {
                // First, let's see if the original image had a subimage
                // name and use that.
                std::string subname = B.spec().get_string_attribute("oiio:subimagename");
                if (subname.size())
                    name = subname + "." + name;
            }
            if (std::find(dstspec.channelnames.begin(), dstspec.channelnames.end(), name) != dstspec.channelnames.end()) {
                // If it's still a duplicate, fall back on a totally
                // artificial name that contains the channel number.
                name = Strutil::format ("channel%d", A.spec().nchannels+c);
            }
            dstspec.channelnames.push_back (name);
        }
        if (dstspec.alpha_channel < 0 && B.spec().alpha_channel >= 0)
            dstspec.alpha_channel = B.spec().alpha_channel + A.nchannels();
        if (dstspec.z_channel < 0 && B.spec().z_channel >= 0)
            dstspec.z_channel = B.spec().z_channel + A.nchannels();
        set_roi (dstspec, roi);
        dst.reset (dstspec);
    }

    bool ok;
    OIIO_DISPATCH_COMMON_TYPES3 (ok, "channel_append", channel_append_impl,
                          dst.spec().format, A.spec().format, B.spec().format,
                          dst, A, B, roi, nthreads);
    return ok;
}



OIIO_NAMESPACE_END
