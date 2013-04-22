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
/// Implementation of ImageBufAlgo algorithms that do math on 
/// single pixels at a time.

#include <boost/bind.hpp>

#include <OpenEXR/half.h>

#include <cmath>
#include <iostream>
#include <limits>

#include "imagebuf.h"
#include "imagebufalgo.h"
#include "imagebufalgo_util.h"
#include "dassert.h"
#include "sysutil.h"
#include "thread.h"



OIIO_NAMESPACE_ENTER
{


template<class D>
static bool
clamp_ (ImageBuf &dst, const float *min, const float *max,
        bool clampalpha01, ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Lots of pixels and request for multi threads? Parallelize.
        ImageBufAlgo::parallel_image (
            boost::bind(clamp_<D>, boost::ref(dst), min, max, clampalpha01,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
    for (ImageBuf::Iterator<D> d (dst, roi);  ! d.done();  ++d) {
        for (int c = roi.chbegin;  c < roi.chend;  ++c)
            d[c] = OIIO::clamp<float> (d[c], min[c], max[c]);
    }
    int a = dst.spec().alpha_channel;
    if (clampalpha01 && a >= roi.chbegin && a < roi.chend) {
        for (ImageBuf::Iterator<D> d (dst, roi);  ! d.done();  ++d) {
            d[a] = OIIO::clamp<float> (d[a], 0.0f, 1.0f);
        }
    }
    return true;
}



bool
ImageBufAlgo::clamp (ImageBuf &dst, const float *min, const float *max,
                     bool clampalpha01, ROI roi, int nthreads)
{
    IBAprep (roi, &dst);
    std::vector<float> minvec, maxvec;
    if (! min) {
        minvec.resize (dst.nchannels(), -std::numeric_limits<float>::max());
        min = &minvec[0];
    }
    if (! max) {
        maxvec.resize (dst.nchannels(), std::numeric_limits<float>::max());
        max = &maxvec[0];
    }
    OIIO_DISPATCH_TYPES ("clamp", clamp_, dst.spec().format, dst,
                         min, max, clampalpha01, roi, nthreads);
    return false;
}



bool
ImageBufAlgo::clamp (ImageBuf &dst, float min, float max,
                     bool clampalpha01, ROI roi, int nthreads)
{
    IBAprep (roi, &dst);
    std::vector<float> minvec (dst.nchannels(), min);
    std::vector<float> maxvec (dst.nchannels(), max);
    OIIO_DISPATCH_TYPES ("clamp", clamp_, dst.spec().format, dst,
                         &minvec[0], &maxvec[0], clampalpha01, roi, nthreads);
    return false;
}



// DEPRECATED version
bool
ImageBufAlgo::add (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                   int options)
{
    // Sanity checks
    
    // dst must be distinct from A and B
    if ((const void *)&A == (const void *)&dst ||
        (const void *)&B == (const void *)&dst) {
        dst.error ("destination image must be distinct from source");
        return false;
    }
    
    // all three images must have the same number of channels
    if (A.spec().nchannels != B.spec().nchannels) {
        dst.error ("channel number mismatch: %d vs. %d", 
                   A.spec().nchannels, B.spec().nchannels);
        return false;
    }
    
    // If dst has not already been allocated, set it to the right size,
    // make it unconditinally float
    if (! dst.pixels_valid()) {
        ImageSpec dstspec = A.spec();
        dstspec.set_format (TypeDesc::TypeFloat);
        dst.alloc (dstspec);
    }
    // Clear dst pixels if instructed to do so
    if (options & ADD_CLEAR_DST) {
        zero (dst);
    }
      
    ASSERT (A.spec().format == TypeDesc::FLOAT &&
            B.spec().format == TypeDesc::FLOAT &&
            dst.spec().format == TypeDesc::FLOAT);
    
    ImageBuf::ConstIterator<float,float> a (A);
    ImageBuf::ConstIterator<float,float> b (B);
    ImageBuf::Iterator<float> d (dst);
    int nchannels = A.nchannels();
    // Loop over all pixels in A
    for ( ; a.valid();  ++a) {  
        // Point the iterators for B and dst to the corresponding pixel
        if (options & ADD_RETAIN_WINDOWS) {
            b.pos (a.x(), a.y());
        } else {
            // ADD_ALIGN_WINDOWS: make B line up with A
            b.pos (a.x()-A.xbegin()+B.xbegin(), a.y()-A.ybegin()+B.ybegin());
        }
        d.pos (a.x(), b.y());
        
        if (! b.valid() || ! d.valid())
            continue;   // Skip pixels that don't align
        
        // Add the pixel
        for (int c = 0;  c < nchannels;  ++c)
              d[c] = a[c] + b[c];
    }
    
    return true;
}



template<class Rtype, class Atype, class Btype>
static bool
add_impl (ImageBuf &R, const ImageBuf &A, const ImageBuf &B,
          ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(add_impl<Rtype,Atype,Btype>,
                        boost::ref(R), boost::cref(A), boost::cref(B),
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
    ImageBuf::Iterator<Rtype> r (R, roi);
    ImageBuf::ConstIterator<Atype> a (A, roi);
    ImageBuf::ConstIterator<Btype> b (B, roi);
    for ( ;  !r.done();  ++r, ++a, ++b)
        for (int c = roi.chbegin;  c < roi.chend;  ++c)
            r[c] = a[c] + b[c];
    return true;
}



bool
ImageBufAlgo::add (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                   ROI roi, int nthreads)
{
    IBAprep (roi, &dst, &A, &B);
    OIIO_DISPATCH_COMMON_TYPES3 ("add", add_impl, dst.spec().format,
                                 A.spec().format, B.spec().format,
                                 dst, A, B, roi, nthreads);
    return true;
}



template<class Rtype>
static bool
add_inplace (ImageBuf &R, const float *val,
             ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(add_inplace<Rtype>, boost::ref(R), val,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
    ImageBuf::Iterator<Rtype> r (R, roi);
    for ( ;  !r.done();  ++r)
        for (int c = roi.chbegin;  c < roi.chend;  ++c)
            r[c] = r[c] + val[c];
    return true;
}



bool
ImageBufAlgo::add (ImageBuf &dst, const float *val, ROI roi, int nthreads)
{
    IBAprep (roi, &dst);
    OIIO_DISPATCH_TYPES ("add", add_inplace, dst.spec().format,
                         dst, val, roi, nthreads);
    return true;
}



bool
ImageBufAlgo::add (ImageBuf &R, float val, ROI roi, int nthreads)
{
    int nc = R.nchannels();
    float *vals = ALLOCA (float, nc);
    for (int c = 0;  c < nc;  ++c)
        vals[c] = val;
    return add (R, vals, roi, nthreads);
}




template<class Rtype, class Atype, class Btype>
static bool
sub_impl (ImageBuf &R, const ImageBuf &A, const ImageBuf &B,
          ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(sub_impl<Rtype,Atype,Btype>,
                        boost::ref(R), boost::cref(A), boost::cref(B),
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
    ImageBuf::Iterator<Rtype> r (R, roi);
    ImageBuf::ConstIterator<Atype> a (A, roi);
    ImageBuf::ConstIterator<Btype> b (B, roi);
    for ( ;  !r.done();  ++r, ++a, ++b)
        for (int c = roi.chbegin;  c < roi.chend;  ++c)
            r[c] = a[c] - b[c];
    return true;
}



bool
ImageBufAlgo::sub (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                   ROI roi, int nthreads)
{
    IBAprep (roi, &dst, &A, &B);
    OIIO_DISPATCH_COMMON_TYPES3 ("sub", sub_impl, dst.spec().format,
                                 A.spec().format, B.spec().format,
                                 dst, A, B, roi, nthreads);
    return true;
}



template<class Rtype, class Atype, class Btype>
static bool
mul_impl (ImageBuf &R, const ImageBuf &A, const ImageBuf &B,
          ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(mul_impl<Rtype,Atype,Btype>,
                        boost::ref(R), boost::cref(A), boost::cref(B),
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
    ImageBuf::Iterator<Rtype> r (R, roi);
    ImageBuf::ConstIterator<Atype> a (A, roi);
    ImageBuf::ConstIterator<Btype> b (B, roi);
    for ( ;  !r.done();  ++r, ++a, ++b)
        for (int c = roi.chbegin;  c < roi.chend;  ++c)
            r[c] = a[c] * b[c];
    return true;
}



bool
ImageBufAlgo::mul (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                   ROI roi, int nthreads)
{
    IBAprep (roi, &dst, &A, &B);
    OIIO_DISPATCH_COMMON_TYPES3 ("mul", mul_impl, dst.spec().format,
                                 A.spec().format, B.spec().format,
                                 dst, A, B, roi, nthreads);
    return true;
}



template<class Rtype>
static bool
mul_impl (ImageBuf &R, const float *val, ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(mul_impl<Rtype>, boost::ref(R), val,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    ImageBuf::Iterator<Rtype> r (R, roi);
    for (ImageBuf::Iterator<Rtype> r (R, roi);  !r.done();  ++r)
        for (int c = roi.chbegin;  c < roi.chend;  ++c)
            r[c] = r[c] * val[c];
    return true;
}



bool
ImageBufAlgo::mul (ImageBuf &dst, const float *val, ROI roi, int nthreads)
{
    IBAprep (roi, &dst);
    OIIO_DISPATCH_TYPES ("mul", mul_impl, dst.spec().format,
                         dst, val, roi, nthreads);
    return true;
}



bool
ImageBufAlgo::mul (ImageBuf &R, float val, ROI roi, int nthreads)
{
    int nc = R.nchannels();
    float *vals = ALLOCA (float, nc);
    for (int c = 0;  c < nc;  ++c)
        vals[c] = val;
    return mul (R, vals, roi, nthreads);
}



template<class D, class S>
static bool
channel_sum_ (ImageBuf &dst, const ImageBuf &src,
              const float *weights, ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(channel_sum_<D,S>, boost::ref(dst), boost::cref(src),
                        weights, _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    ImageBuf::Iterator<D> d (dst, roi);
    ImageBuf::ConstIterator<S> s (src, roi);
    for ( ;  !d.done();  ++d, ++s) {
        float sum = 0.0f;
        for (int c = roi.chbegin;  c < roi.chend;  ++c)
            sum += s[c] * weights[c];
        d[0] = sum;
    }
    return true;
}



bool
ImageBufAlgo::channel_sum (ImageBuf &dst, const ImageBuf &src,
                           const float *weights, ROI roi, int nthreads)
{
    if (! roi.defined())
        roi = get_roi(src.spec());
    roi.chend = std::min (roi.chend, src.nchannels());
    ROI dstroi = roi;
    dstroi.chbegin = 0;
    dstroi.chend = 1;
    IBAprep (dstroi, &dst);

    if (! weights) {
        float *local_weights = ALLOCA (float, roi.chend);
        for (int c = 0; c < roi.chend; ++c)
            local_weights[c] = 1.0f;
        weights = &local_weights[0];
    }

    OIIO_DISPATCH_TYPES2 ("channel_sum", channel_sum_,
                          dst.spec().format, src.spec().format,
                          dst, src, weights, roi, nthreads);
    return false;
}




inline float rangecompress (float x)
{
    // Formula courtesy of Sony Pictures Imageworks
    const float x1 = 1.0, a = 1.2607481479644775391;
    const float b = 0.28785100579261779785, c = -1.4042005538940429688;
    float absx = fabsf(x);
    if (absx <= x1)
        return x;
    return copysignf (a + b * logf(fabsf(c*absx + 1.0f)), x);
}



inline float rangeexpand (float y)
{
    // Formula courtesy of Sony Pictures Imageworks
    const float x1 = 1.0, a = 1.2607481479644775391;
    const float b = 0.28785100579261779785, c = -1.4042005538940429688;
    float absy = fabsf(y);
    if (absy <= x1)
        return y;
    float xIntermediate = expf ((absy - a)/b);
    // Since the compression step includes an absolute value, there are
    // two possible results here. If x < x1 it is the incorrect result,
    // so pick the other value.
    float x = (xIntermediate - 1.0f) / c;
    if (x < x1)
        x = (-xIntermediate - 1.0f) / c;
    return copysign (x, y);
}



template<class Rtype>
static bool
rangecompress_ (ImageBuf &R, bool useluma, ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(rangecompress_<Rtype>, boost::ref(R), useluma,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    const ImageSpec &Rspec (R.spec());
    int alpha_channel = Rspec.alpha_channel;
    int z_channel = Rspec.z_channel;
    if (roi.nchannels() < 3 ||
        (alpha_channel >= roi.chbegin && alpha_channel < roi.chbegin+3) ||
        (z_channel >= roi.chbegin && z_channel < roi.chbegin+3)) {
        useluma = false;  // No way to use luma
    }

    ImageBuf::Iterator<Rtype> r (R, roi);
    for (ImageBuf::Iterator<Rtype> r (R, roi);  !r.done();  ++r) {
        if (useluma) {
            float luma = 0.21264f * r[roi.chbegin] + 0.71517f * r[roi.chbegin+1] + 0.07219f * r[roi.chbegin+2];
            if (fabsf(luma) <= 1.0f)
                continue;  // Not HDR, no range compression needed
            float scale = rangecompress (luma) / luma;
            for (int c = roi.chbegin; c < roi.chend; ++c) {
                if (c == alpha_channel || c == z_channel)
                    continue;
                r[c] = r[c] * scale;
            }
        } else {
            for (int c = roi.chbegin; c < roi.chend; ++c) {
                if (c == alpha_channel || c == z_channel)
                    continue;
                r[c] = rangecompress (r[c]);
            }
        }
    }
    return true;
}



template<class Rtype>
static bool
rangeexpand_ (ImageBuf &R, bool useluma, ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(rangeexpand_<Rtype>, boost::ref(R), useluma,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    const ImageSpec &Rspec (R.spec());
    int alpha_channel = Rspec.alpha_channel;
    int z_channel = Rspec.z_channel;
    if (roi.nchannels() < 3 ||
        (alpha_channel >= roi.chbegin && alpha_channel < roi.chbegin+3) ||
        (z_channel >= roi.chbegin && z_channel < roi.chbegin+3)) {
        useluma = false;  // No way to use luma
    }

    ImageBuf::Iterator<Rtype> r (R, roi);
    for (ImageBuf::Iterator<Rtype> r (R, roi);  !r.done();  ++r) {
        if (useluma) {
            float luma = 0.21264f * r[roi.chbegin] + 0.71517f * r[roi.chbegin+1] + 0.07219f * r[roi.chbegin+2];
            if (fabsf(luma) <= 1.0f)
                continue;  // Not HDR, no range compression needed
            float scale = rangeexpand (luma) / luma;
            for (int c = roi.chbegin; c < roi.chend; ++c) {
                if (c == alpha_channel || c == z_channel)
                    continue;
                r[c] = r[c] * scale;
            }
        } else {
            for (int c = roi.chbegin; c < roi.chend; ++c) {
                if (c == alpha_channel || c == z_channel)
                    continue;
                r[c] = rangeexpand (r[c]);
            }
        }
    }
    return true;
}



bool
ImageBufAlgo::rangecompress (ImageBuf &dst, bool useluma,
                             ROI roi, int nthreads)
{
    // If the data type can't handle extended range, this is a no-op
    int basetype = dst.spec().format.basetype;
    if (basetype != TypeDesc::FLOAT && basetype != TypeDesc::HALF &&
        basetype != TypeDesc::DOUBLE)
        return true;

    IBAprep (roi, &dst);
    switch (basetype) {
    case TypeDesc::FLOAT:
        return rangecompress_<float> (dst, useluma, roi, nthreads);
    case TypeDesc::HALF:
        return rangecompress_<half> (dst, useluma, roi, nthreads);
    case TypeDesc::DOUBLE:
        return rangecompress_<double> (dst, useluma, roi, nthreads);
    default:
        return true;
    }
    return true;
}



bool
ImageBufAlgo::rangeexpand (ImageBuf &dst, bool useluma,
                           ROI roi, int nthreads)
{
    // If the data type can't handle extended range, this is a no-op
    int basetype = dst.spec().format.basetype;
    if (basetype != TypeDesc::FLOAT && basetype != TypeDesc::HALF &&
        basetype != TypeDesc::DOUBLE)
        return true;

    IBAprep (roi, &dst);
    switch (basetype) {
    case TypeDesc::FLOAT:
        return rangeexpand_<float> (dst, useluma, roi, nthreads);
    case TypeDesc::HALF:
        return rangeexpand_<half> (dst, useluma, roi, nthreads);
    case TypeDesc::DOUBLE:
        return rangeexpand_<double> (dst, useluma, roi, nthreads);
    default:
        return true;
    }
    return true;
}



template<class Rtype>
static bool
unpremult_ (ImageBuf &R, ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(unpremult_<Rtype>, boost::ref(R),
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    int alpha_channel = R.spec().alpha_channel;
    int z_channel = R.spec().z_channel;
    for (ImageBuf::Iterator<Rtype> r (R, roi);  !r.done();  ++r) {
        float alpha = r[alpha_channel];
        if (alpha == 0.0f || alpha == 1.0f)
            continue;
        for (int c = roi.chbegin;  c < roi.chend;  ++c)
            if (c != alpha_channel && c != z_channel)
                r[c] = r[c] / alpha;
    }
    return true;
}



bool
ImageBufAlgo::unpremult (ImageBuf &dst,  ROI roi, int nthreads)
{
    if (dst.spec().alpha_channel < 0)
        return true;

    IBAprep (roi, &dst);
    OIIO_DISPATCH_TYPES ("unpremult", unpremult_, dst.spec().format,
                         dst, roi, nthreads);
    return true;
}



template<class Rtype>
static bool
premult_ (ImageBuf &R, ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(premult_<Rtype>, boost::ref(R),
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    int alpha_channel = R.spec().alpha_channel;
    int z_channel = R.spec().z_channel;
    for (ImageBuf::Iterator<Rtype> r (R, roi);  !r.done();  ++r) {
        float alpha = r[alpha_channel];
        if (alpha == 1.0f)
            continue;
        for (int c = roi.chbegin;  c < roi.chend;  ++c)
            if (c != alpha_channel && c != z_channel)
                r[c] = r[c] * alpha;
    }
    return true;
}



bool
ImageBufAlgo::premult (ImageBuf &dst, ROI roi, int nthreads)
{
    if (dst.spec().alpha_channel < 0)
        return true;

    IBAprep (roi, &dst);
    OIIO_DISPATCH_TYPES ("premult", premult_, dst.spec().format,
                         dst, roi, nthreads);
    return true;
}





namespace
{

// Make sure isfinite is defined for 'half'
inline bool isfinite (half h) { return h.isFinite(); }


template<typename T>
bool fixNonFinite_ (ImageBuf &dst, ImageBufAlgo::NonFiniteFixMode mode,
                    int *pixelsFixed, ROI roi, int nthreads)
{
    if (mode != ImageBufAlgo::NONFINITE_NONE &&
        mode != ImageBufAlgo::NONFINITE_BLACK &&
        mode != ImageBufAlgo::NONFINITE_BOX3) {
        // Something went wrong
        dst.error ("fixNonFinite: unknown repair mode");
        return false;
    }

    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Lots of pixels and request for multi threads? Parallelize.
        ImageBufAlgo::parallel_image (
            boost::bind(fixNonFinite_<T>, boost::ref(dst), mode, pixelsFixed,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case

    ROI dstroi = get_roi (dst.spec());
    int count = 0;   // Number of pixels with nonfinite values

    if (mode == ImageBufAlgo::NONFINITE_NONE) {
        // Just count the number of pixels with non-finite values
        for (ImageBuf::Iterator<T,T> pixel (dst);  ! pixel.done();  ++pixel) {
            for (int c = roi.chbegin;  c < roi.chend;  ++c) {
                T value = pixel[c];
                if (! isfinite(value)) {
                    ++count;
                    break;  // only count one per pixel
                }
            }
        }
    } else if (mode == ImageBufAlgo::NONFINITE_BLACK) {
        // Replace non-finite pixels with black
        for (ImageBuf::Iterator<T,T> pixel (dst);  ! pixel.done();  ++pixel) {
            bool fixed = false;
            for (int c = roi.chbegin;  c < roi.chend;  ++c) {
                T value = pixel[c];
                if (! isfinite(value)) {
                    pixel[c] = T(0.0);
                    fixed = true;
                }
            }
            if (fixed)
                ++count;
        }
    } else if (mode == ImageBufAlgo::NONFINITE_BOX3) {
        // Replace non-finite pixels with a simple 3x3 window average
        // (the average excluding non-finite pixels, of course)
        for (ImageBuf::Iterator<T,T> pixel (dst);  ! pixel.done();  ++pixel) {
            bool fixed = false;
            for (int c = roi.chbegin;  c < roi.chend;  ++c) {
                T value = pixel[c];
                if (! isfinite (value)) {
                    int numvals = 0;
                    T sum (0.0);
                    ROI roi2 (pixel.x()-1, pixel.x()+2,
                              pixel.y()-1, pixel.y()+2,
                              pixel.z()-1, pixel.z()+2);
                    roi2 = roi_intersection (roi2, dstroi);
                    for (ImageBuf::Iterator<T,T> i(dst,roi2); !i.done(); ++i) {
                        T v = i[c];
                        if (isfinite (v)) {
                            sum += v;
                            ++numvals;
                        }
                    }
                    pixel[c] = numvals ? T(sum / numvals) : T(0.0);
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
        *(atomic_int *)pixelsFixed += count;
    }

    return true;
}

} // anon namespace



/// Fix all non-finite pixels (nan/inf) using the specified approach
bool
ImageBufAlgo::fixNonFinite (ImageBuf &src, 
                            NonFiniteFixMode mode, int *pixelsFixed,
                            ROI roi, int nthreads)
{
    // If no ROI is defined, use the data window of src.
    if (! roi.defined())
        roi = get_roi(src.spec());
    roi.chend = std::min (roi.chend, src.nchannels());

    // Initialize
    if (pixelsFixed)
        *pixelsFixed = 0;

    switch (src.spec().format.basetype) {
    case TypeDesc::FLOAT :
        return fixNonFinite_<float> (src, mode, pixelsFixed, roi, nthreads);
    case TypeDesc::HALF  :
        return fixNonFinite_<half> (src, mode, pixelsFixed, roi, nthreads);
    case TypeDesc::DOUBLE:
        return fixNonFinite_<double> (src, mode, pixelsFixed, roi, nthreads);
    default:
        // All other format types aren't capable of having nonfinite
        // pixel values.
        return true;
    }
}



// DEPRECATED 2-argument version
bool
ImageBufAlgo::fixNonFinite (ImageBuf &dst, const ImageBuf &src,
                            NonFiniteFixMode mode, int *pixelsFixed)
{
    ROI roi;
    IBAprep (roi, &dst, &src);
    if (dst.nchannels() != src.nchannels()) {
        dst.error ("channel number mismatch: %d vs. %d", 
                   dst.spec().nchannels, src.spec().nchannels);
        return false;
    }
    if ((const ImageBuf *)&dst != &src)
        if (! dst.copy (src))
            return false;
    return fixNonFinite (dst, mode, pixelsFixed, roi);
}


namespace {   // anonymous namespace

static bool
decode_over_channels (const ImageBuf &R, int &nchannels, 
                      int &alpha, int &z, int &colors)
{
    if (! R.initialized()) {
        alpha = -1;
        z = -1;
        colors = 0;
        return false;
    }
    const ImageSpec &spec (R.spec());
    alpha =  spec.alpha_channel;
    bool has_alpha = (alpha >= 0);
    z = spec.z_channel;
    bool has_z = (z >= 0);
    nchannels = spec.nchannels;
    colors = nchannels - has_alpha - has_z;
    if (! has_alpha && colors == 4) {
        // No marked alpha channel, but suspiciously 4 channel -- assume
        // it's RGBA. 
        has_alpha = true;
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
over_impl (ImageBuf &R, const ImageBuf &A, const ImageBuf &B, ROI roi,
           bool zcomp=false, bool z_zeroisinf=false)
{
    if (R.spec().format != BaseTypeFromC<Rtype>::value ||
        A.spec().format != BaseTypeFromC<Atype>::value ||
        B.spec().format != BaseTypeFromC<Btype>::value) {
        R.error ("Unsupported pixel data format combination '%s / %s / %s'",
                 R.spec().format, A.spec().format, B.spec().format);
        return false;   // double check that types match
    }

    // It's already guaranteed that R, A, and B have matching channel
    // ordering, and have an alpha channel.  So just decode one.
    int nchannels = 0, alpha_channel = 0, z_channel = 0, ncolor_channels = 0;
    decode_over_channels (R, nchannels, alpha_channel,
                          z_channel, ncolor_channels);
    bool has_z = (z_channel >= 0);

    ImageBuf::ConstIterator<Atype, float> a (A);
    ImageBuf::ConstIterator<Btype, float> b (B);
    ImageBuf::Iterator<Rtype, float> r (R, roi);
    for ( ; ! r.done(); r++) {
        a.pos (r.x(), r.y(), r.z());
        b.pos (r.x(), r.y(), r.z());

        if (! a.exists()) {
            if (! b.exists()) {
                // a and b outside their data window -- "empty" pixels
                for (int c = 0; c < nchannels; c++)
                    r[c] = 0.0f;
            } else {
                // a doesn't exist, but b does -- copy B
                for (int c = 0; c < nchannels; ++c)
                    r[c] = b[c];
            }
            continue;
        }

        if (! b.exists()) {
            // a exists, b does not -- copy A
            for (int c = 0; c < nchannels; ++c)
                r[c] = a[c];
            continue;
        }

        // At this point, a and b exist.
        float az = 0.0f, bz = 0.0f;
        bool a_is_closer = true;  // will remain true if !zcomp
        if (zcomp && has_z) {
            az = a[z_channel];
            bz = b[z_channel];
            if (z_zeroisinf) {
                if (az == 0.0f) az = std::numeric_limits<float>::max();
                if (bz == 0.0f) bz = std::numeric_limits<float>::max();
            }
            a_is_closer = (az <= bz);
        }
        if (a_is_closer) {
            // A over B
            float alpha = clamp (a[alpha_channel], 0.0f, 1.0f);
            float one_minus_alpha = 1.0f - alpha;
            for (int c = 0;  c < nchannels;  c++)
                r[c] = a[c] + one_minus_alpha * b[c];
            if (has_z)
                r[z_channel] = (alpha != 0.0) ? a[z_channel] : b[z_channel];
        } else {
            // B over A -- because we're doing a Z composite
            float alpha = clamp (b[alpha_channel], 0.0f, 1.0f);
            float one_minus_alpha = 1.0f - alpha;
            for (int c = 0;  c < nchannels;  c++)
                r[c] = b[c] + one_minus_alpha * a[c];
            r[z_channel] = (alpha != 0.0) ? b[z_channel] : a[z_channel];
        }
    }
    return true;
}

}    // anonymous namespace


bool
ImageBufAlgo::over (ImageBuf &R, const ImageBuf &A, const ImageBuf &B, ROI roi,
                    int nthreads)
{
    const ImageSpec &specR = R.spec();
    const ImageSpec &specA = A.spec();
    const ImageSpec &specB = B.spec();

    int nchannels_R, nchannels_A, nchannels_B;
    int alpha_R, alpha_A, alpha_B;
    int z_R, z_A, z_B;
    int colors_R, colors_A, colors_B;
    bool initialized_R = decode_over_channels (R, nchannels_R, alpha_R,
                                               z_R, colors_R);
    bool initialized_A = decode_over_channels (A, nchannels_A, alpha_A,
                                               z_A, colors_A);
    bool initialized_B = decode_over_channels (B, nchannels_B, alpha_B,
                                               z_B, colors_B);

    if (! initialized_A || ! initialized_B) {
        R.error ("Can't 'over' uninitialized images");
        return false;
    }

    // Fail if the input images don't have an alpha channel.
    if (alpha_A < 0 || alpha_B < 0 || (initialized_R && alpha_R < 0)) {
        R.error ("'over' requires alpha channels");
        return false;
    }
    // Fail for mismatched channel counts
    if (colors_A != colors_B || colors_A < 1) {
        R.error ("Can't 'over' images with mismatched color channel counts (%d vs %d)",
                 colors_A, colors_B);
        return false;
    }
    // Fail for unaligned alpha or z channels
    if (alpha_A != alpha_B || z_A != z_B ||
        (initialized_R && alpha_R != alpha_A) ||
        (initialized_R && z_R != z_A)) {
        R.error ("Can't 'over' images with mismatched channel order",
                 colors_A, colors_B);
        return false;
    }
    
    // At present, this operation only supports ImageBuf's containing
    // float pixel data.
    if ((initialized_R && specR.format != TypeDesc::TypeFloat) ||
        specA.format != TypeDesc::TypeFloat ||
        specB.format != TypeDesc::TypeFloat) {
        R.error ("Unsupported pixel data format combination '%s = %s over %s'",
                 specR.format, specA.format, specB.format);
        return false;
    }

    // Uninitialized R -> size it to the union of A and B.
    if (! initialized_R) {
        ImageSpec newspec = specA;
        set_roi (newspec, roi_union (get_roi(specA), get_roi(specB)));
        R.reset ("over", newspec);
    }

    // Specified ROI -> use it. Unspecified ROI -> initialize from R.
    if (! roi.defined())
        roi = get_roi (R.spec());

    parallel_image (boost::bind (over_impl<float,float,float>, boost::ref(R),
                                 boost::cref(A), boost::cref(B), _1, false, false),
                    roi, nthreads);
    return ! R.has_error();
}



bool
ImageBufAlgo::zover (ImageBuf &R, const ImageBuf &A, const ImageBuf &B,
                     bool z_zeroisinf, ROI roi, int nthreads)
{
    const ImageSpec &specR = R.spec();
    const ImageSpec &specA = A.spec();
    const ImageSpec &specB = B.spec();

    int nchannels_R, nchannels_A, nchannels_B;
    int alpha_R, alpha_A, alpha_B;
    int z_R, z_A, z_B;
    int colors_R, colors_A, colors_B;
    bool initialized_R = decode_over_channels (R, nchannels_R, alpha_R,
                                               z_R, colors_R);
    bool initialized_A = decode_over_channels (A, nchannels_A, alpha_A,
                                               z_A, colors_A);
    bool initialized_B = decode_over_channels (B, nchannels_B, alpha_B,
                                               z_B, colors_B);

    if (! initialized_A || ! initialized_B) {
        R.error ("Can't 'zover' uninitialized images");
        return false;
    }
    // Fail if the input images don't have a Z channel.
    if (z_A < 0 || z_B < 0 || (initialized_R && z_R < 0)) {
        R.error ("'zover' requires Z channels");
        return false;
    }
    // Fail if the input images don't have an alpha channel.
    if (alpha_A < 0 || alpha_B < 0 || (initialized_R && alpha_R < 0)) {
        R.error ("'zover' requires alpha channels");
        return false;
    }
    // Fail for mismatched channel counts
    if (colors_A != colors_B || colors_A < 1) {
        R.error ("Can't 'zover' images with mismatched color channel counts (%d vs %d)",
                 colors_A, colors_B);
        return false;
    }
    // Fail for unaligned alpha or z channels
    if (alpha_A != alpha_B || z_A != z_B ||
        (initialized_R && alpha_R != alpha_A) ||
        (initialized_R && z_R != z_A)) {
        R.error ("Can't 'zover' images with mismatched channel order",
                 colors_A, colors_B);
        return false;
    }
    
    // At present, this operation only supports ImageBuf's containing
    // float pixel data.
    if ((initialized_R && specR.format != TypeDesc::TypeFloat) ||
        specA.format != TypeDesc::TypeFloat ||
        specB.format != TypeDesc::TypeFloat) {
        R.error ("Unsupported pixel data format combination '%s = %s zover %s'",
                 specR.format, specA.format, specB.format);
        return false;
    }

    // Uninitialized R -> size it to the union of A and B.
    if (! initialized_R) {
        ImageSpec newspec = specA;
        set_roi (newspec, roi_union (get_roi(specA), get_roi(specB)));
        R.reset ("zover", newspec);
    }

    // Specified ROI -> use it. Unspecified ROI -> initialize from R.
    if (! roi.defined())
        roi = get_roi (R.spec());

    parallel_image (boost::bind (over_impl<float,float,float>, boost::ref(R),
                                 boost::cref(A), boost::cref(B), _1,
                                 true, z_zeroisinf),
                    roi, nthreads);
    return ! R.has_error();
}



bool
ImageBufAlgo::zover (ImageBuf &R, const ImageBuf &A, const ImageBuf &B,
                     ROI roi, int nthreads)
{
    // DEPRECATED version -- just call the new version.  This exists to 
    // avoid breaking link compatibility.  Eventually remove it at the
    // next major release.
    return zover (R, A, B, false, roi, nthreads);
}



}
OIIO_NAMESPACE_EXIT
