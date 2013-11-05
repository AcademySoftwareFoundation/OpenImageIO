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

#include "imagebuf.h"
#include "imagebufalgo.h"
#include "imagebufalgo_util.h"
#include "thread.h"



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
    OIIO_DISPATCH_TYPES2 ("paste", paste_, dst.spec().format, src.spec().format,
                          dst, dstroi_save, src, srcroi, nthreads);
    return false;
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
    OIIO_DISPATCH_TYPES2 ("crop", crop_, dst.spec().format, src.spec().format,
                          dst, src, roi, nthreads);
    return false;
}



template<class D, class S>
static bool
flip_ (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    ImageBuf::ConstIterator<S, D> s (src, roi);
    ImageBuf::Iterator<D, D> d (dst, roi);
    for ( ; ! d.done(); ++d) {
        s.pos (d.x(), roi.yend-1 - (d.y() - roi.ybegin), d.z());
        for (int c = roi.chbegin; c < roi.chend; ++c)
            d[c] = s[c];
    }
    return true;
}


bool
ImageBufAlgo::flip(ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    IBAprep (roi, &dst, &src);
    OIIO_DISPATCH_TYPES2 ("flip", flip_,
                          dst.spec().format, src.spec().format, dst, src,
                          roi, nthreads);
    return false;
}



template<class D, class S>
static bool
flop_ (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    ImageBuf::ConstIterator<S, D> s (src, roi);
    ImageBuf::Iterator<D, D> d (dst, roi);
    for ( ; ! d.done(); ++d) {
        s.pos (roi.xend-1 - (d.x() - roi.xbegin), d.y(), d.z());
        for (int c = roi.chbegin; c < roi.chend; ++c)
            d[c] = s[c];
    }
    return true;
}


bool
ImageBufAlgo::flop (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    IBAprep (roi, &dst, &src);
    OIIO_DISPATCH_TYPES2 ("flop", flop_,
                          dst.spec().format, src.spec().format, dst, src,
                          roi, nthreads);
    return false;
}



template<class D, class S>
static bool
flipflop_ (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads)
{
    // Serial case
    ImageBuf::ConstIterator<S, D> s (src, roi);
    ImageBuf::Iterator<D, D> d (dst, roi);
    for ( ; !d.done(); ++d) {
        s.pos (roi.xend-1 - (d.x() - roi.xbegin),
               roi.yend-1 - (d.y() - roi.ybegin), d.z());
        for (int c = roi.chbegin; c < roi.chend; ++c)
            d[c] = s[c];
    }
    return true;
}


bool
ImageBufAlgo::flipflop (ImageBuf &dst, const ImageBuf &src,
                        ROI roi, int nthreads)
{
    IBAprep (roi, &dst, &src);
    OIIO_DISPATCH_TYPES2 ("flipflop", flipflop_,
                          dst.spec().format, src.spec().format, dst, src,
                          roi, nthreads);
    return false;
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
    IBAprep (dst_roi, &dst);
    OIIO_DISPATCH_TYPES2 ("transpose", transpose_, dst.spec().format,
                          src.spec().format, dst, src, roi, nthreads);
    return false;
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
        int dx = dstroi.xbegin + ((s.x() - dstroi.xbegin + xshift) % width);
        int dy = dstroi.ybegin + ((s.y() - dstroi.ybegin + yshift) % height);
        int dz = dstroi.zbegin + ((s.z() - dstroi.zbegin + zshift) % depth);
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
    OIIO_DISPATCH_TYPES2 ("circular_shift", circular_shift_,
                          dst.spec().format, src.spec().format, dst, src,
                          xshift, yshift, zshift, roi, roi, nthreads);
    return false;
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
    newspec.alpha_channel = -1;
    newspec.z_channel = -1;
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

    // Update the image (realloc with the new spec)
    dst.alloc (newspec);

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

    OIIO_DISPATCH_TYPES ("channel_append", channel_append_impl,
                         A.spec().format, dst, A, B, roi, nthreads);
    return true;
}



}
OIIO_NAMESPACE_EXIT
