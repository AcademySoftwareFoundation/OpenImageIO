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
paste_ (ImageBuf &dst, const ImageBuf &src,
        ROI dstroi, ROI srcroi, int nthreads)
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
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "paste", paste_,
                                 dst.spec().format, src.spec().format,
                                 dst, src, dstroi_save, srcroi, nthreads);
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
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "copy", copy_, dst.spec().format, src.spec().format,
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
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "crop", copy_, dst.spec().format, src.spec().format,
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
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "circular_shift", circular_shift_,
                          dst.spec().format, src.spec().format, dst, src,
                          xshift, yshift, zshift, roi, roi, nthreads);
    return ok;
}


OIIO_NAMESPACE_END
