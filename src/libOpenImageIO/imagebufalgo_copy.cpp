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


#include <boost/bind.hpp>

#include <OpenEXR/half.h>

#include <cmath>
#include <iostream>

#include "OpenImageIO/imagebuf.h"
#include "OpenImageIO/imagebufalgo.h"
#include "OpenImageIO/imagebufalgo_util.h"
#include "OpenImageIO/thread.h"



OIIO_NAMESPACE_ENTER
{


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
    IBAprep (dstroi, &dst);

    // do the actual copying
    bool ok;
    OIIO_DISPATCH_TYPES2 (ok, "paste", paste_, dst.spec().format, src.spec().format,
                          dst, dstroi_save, src, srcroi, nthreads);
    return ok;
}




template<class D, class S>
static bool
crop_ (ImageBuf &dst, const ImageBuf &src,
       ROI roi, int nthreads=1)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Lots of pixels and request for multi threads? Parallelize.
        ImageBufAlgo::parallel_image (
            boost::bind(crop_<D,S>, boost::ref(dst), boost::cref(src),
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
    ImageBuf::ConstIterator<S,D> s (src, roi);
    ImageBuf::Iterator<D,D> d (dst, roi);
    for ( ;  ! d.done();  ++d, ++s) {
        for (int c = roi.chbegin;  c < roi.chend;  ++c)
            d[c] = s[c];
    }
    return true;
}



bool 
ImageBufAlgo::crop (ImageBuf &dst, const ImageBuf &src,
                    ROI roi, int nthreads)
{
    dst.clear ();
    roi.chend = std::min (roi.chend, src.nchannels());
    IBAprep (roi, &dst, &src);
    bool ok;
    OIIO_DISPATCH_TYPES2 (ok, "crop", crop_, dst.spec().format, src.spec().format,
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
    IBAprep (dst_roi, &dst, &src);
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
    IBAprep (dst_roi, &dst, &src);
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
    IBAprep (dst_roi, &dst, &src);
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
    IBAprep (dst_roi, &dst, &src);
    bool ok;
    OIIO_DISPATCH_TYPES2 (ok, "rotate180", rotate180_,
                          dst.spec().format, src.spec().format,
                          dst, src, dst_roi, nthreads);
    return ok;
}


bool
ImageBufAlgo::flipflop (ImageBuf &dst, const ImageBuf &src,
                        ROI roi, int nthreads)
{
    return rotate180 (dst, src, roi, nthreads);
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
    IBAprep (dst_roi, &dst, &src);
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
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(transpose_<DSTTYPE,SRCTYPE>,
                        boost::ref(dst), boost::cref(src),
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
    ImageBuf::ConstIterator<SRCTYPE,DSTTYPE> s (src, roi);
    ImageBuf::Iterator<DSTTYPE,DSTTYPE> d (dst);
    for (  ;  ! s.done();  ++s) {
        d.pos (s.y(), s.x(), s.z());
        if (! d.exists())
            continue;
        for (int c = roi.chbegin;  c < roi.chend;  ++c)
            d[c] = s[c];
    }
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
    IBAprep (dst_roi, &dst);
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
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(circular_shift_<DSTTYPE,SRCTYPE>,
                        boost::ref(dst), boost::cref(src),
                        xshift, yshift, zshift,
                        dstroi, _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
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
    return true;
}



bool
ImageBufAlgo::circular_shift (ImageBuf &dst, const ImageBuf &src,
                              int xshift, int yshift, int zshift,
                              ROI roi, int nthreads)
{
    IBAprep (roi, &dst, &src);
    bool ok;
    OIIO_DISPATCH_TYPES2 (ok, "circular_shift", circular_shift_,
                          dst.spec().format, src.spec().format, dst, src,
                          xshift, yshift, zshift, roi, roi, nthreads);
    return ok;
}



bool
ImageBufAlgo::channels (ImageBuf &dst, const ImageBuf &src,
                        int nchannels, const int *channelorder,
                        const float *channelvalues,
                        const std::string *newchannelnames,
                        bool shuffle_channel_names)
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
    for (int c = 0;  c < nchannels;   ++c)
        inorder &= (channelorder[c] == c);
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
        dst.deep_alloc ();
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
                } else if (dstdata.channeltype(c) == srcdata.channeltype(csrc)) {
                    // Same channel types -- copy all samples at once
                    memcpy (dstdata.channel_ptr(p,c), srcdata.channel_ptr(p,csrc),
                            dstdata.samples(p) * srcdata.channeltype(csrc).size());
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

    // Copy the channels individually
    stride_t dstxstride = AutoStride, dstystride = AutoStride, dstzstride = AutoStride;
    ImageSpec::auto_stride (dstxstride, dstystride, dstzstride,
                            newspec.format.size(), newspec.nchannels,
                            newspec.width, newspec.height);
    int channelsize = newspec.format.size();
    char *pixels = (char *) dst.pixeladdr (dst.xbegin(), dst.ybegin(),
                                           dst.zbegin());
    for (int c = 0;  c < nchannels;  ++c) {
        // Copy shuffled channels
        if (channelorder[c] >= 0 && channelorder[c] < src.spec().nchannels) {
            int csrc = channelorder[c];
            src.get_pixel_channels (src.xbegin(), src.xend(),
                                    src.ybegin(), src.yend(),
                                    src.zbegin(), src.zend(),
                                    csrc, csrc+1, newspec.format, pixels,
                                    dstxstride, dstystride, dstzstride);
        }
        // Set channels that are literals
        if (channelorder[c] < 0 && channelvalues && channelvalues[c]) {
            ROI roi = get_roi (dst.spec());
            roi.chbegin = c;
            roi.chend = c+1;
            ImageBufAlgo::fill (dst, &channelvalues[0], roi);
        }
        pixels += channelsize;
    }
    return true;
}



template<class ABtype>
static bool
channel_append_impl (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                     ROI roi, int nthreads)
{
    if (nthreads == 1 || roi.npixels() < 1000) {
        int na = A.nchannels(), nb = B.nchannels();
        int n = std::min (dst.nchannels(), na+nb);
        ImageBuf::Iterator<float> r (dst, roi);
        ImageBuf::ConstIterator<ABtype> a (A, roi);
        ImageBuf::ConstIterator<ABtype> b (B, roi);
        for (;  !r.done();  ++r) {
            a.pos (r.x(), r.y(), r.z());
            b.pos (r.x(), r.y(), r.z());
            for (int c = 0; c < n; ++c) {
                if (c < na)
                    r[c] = a.exists() ? a[c] : 0.0f;
                else
                    r[c] = b.exists() ? b[c-na] : 0.0f;
            }
        }
    } else {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind (channel_append_impl<ABtype>, boost::ref(dst),
                         boost::cref(A), boost::cref(B), _1, 1),
            roi, nthreads);
    }
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
        dstspec.set_format (TypeDesc::TypeFloat);
        // Append the channel descriptions
        dstspec.nchannels = A.spec().nchannels + B.spec().nchannels;
        for (int c = 0;  c < B.spec().nchannels;  ++c) {
            std::string name = B.spec().channelnames[c];
            // Eliminate duplicates
            if (std::find(dstspec.channelnames.begin(), dstspec.channelnames.end(), name) != dstspec.channelnames.end())
                name = Strutil::format ("channel%d", A.spec().nchannels+c);
            dstspec.channelnames.push_back (name);
        }
        if (dstspec.alpha_channel < 0 && B.spec().alpha_channel >= 0)
            dstspec.alpha_channel = B.spec().alpha_channel + A.nchannels();
        if (dstspec.z_channel < 0 && B.spec().z_channel >= 0)
            dstspec.z_channel = B.spec().z_channel + A.nchannels();
        set_roi (dstspec, roi);
        dst.reset (dstspec);
    }

    // For now, only support float destination, and equivalent A and B
    // types.
    if (dst.spec().format != TypeDesc::FLOAT ||
        A.spec().format != B.spec().format) {
        dst.error ("Unable to perform channel_append of %s, %s -> %s",
                   A.spec().format, B.spec().format, dst.spec().format);
        return false;
    }

    bool ok;
    OIIO_DISPATCH_TYPES (ok, "channel_append", channel_append_impl,
                         A.spec().format, dst, A, B, roi, nthreads);
    return ok;
}



}
OIIO_NAMESPACE_EXIT
