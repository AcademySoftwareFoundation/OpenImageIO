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

#include <OpenEXR/half.h>

#include <cmath>
#include <iostream>
#include <limits>

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/deepdata.h>
#include <OpenImageIO/dassert.h>
#include <OpenImageIO/simd.h>
#include <OpenImageIO/color.h>



OIIO_NAMESPACE_BEGIN


template<class D, class S>
static bool
clamp_ (ImageBuf &dst, const ImageBuf &src,
        const float *min, const float *max,
        bool clampalpha01, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        ImageBuf::ConstIterator<S> s (src, roi);
        for (ImageBuf::Iterator<D> d (dst, roi);  ! d.done();  ++d, ++s) {
            for (int c = roi.chbegin;  c < roi.chend;  ++c)
                d[c] = OIIO::clamp<float> (s[c], min[c], max[c]);
        }
        int a = src.spec().alpha_channel;
        if (clampalpha01 && a >= roi.chbegin && a < roi.chend) {
            for (ImageBuf::Iterator<D> d (dst, roi);  ! d.done();  ++d)
                d[a] = OIIO::clamp<float> (d[a], 0.0f, 1.0f);
        }
    });
    return true;
}



bool
ImageBufAlgo::clamp (ImageBuf &dst, const ImageBuf &src,
                     const float *min, const float *max,
                     bool clampalpha01, ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &src))
        return false;
    std::vector<float> minvec, maxvec;
    if (! min) {
        minvec.resize (dst.nchannels(), -std::numeric_limits<float>::max());
        min = &minvec[0];
    }
    if (! max) {
        maxvec.resize (dst.nchannels(), std::numeric_limits<float>::max());
        max = &maxvec[0];
    }
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "clamp", clamp_, dst.spec().format,
                          src.spec().format, dst, src,
                          min, max, clampalpha01, roi, nthreads);
    return ok;
}



bool
ImageBufAlgo::clamp (ImageBuf &dst, const ImageBuf &src,
                     float min, float max,
                     bool clampalpha01, ROI roi, int nthreads)
{
    std::vector<float> minvec (src.nchannels(), min);
    std::vector<float> maxvec (src.nchannels(), max);
    return clamp (dst, src, &minvec[0], &maxvec[0], clampalpha01, roi, nthreads);
}



template<class Rtype, class Atype, class Btype>
static bool
absdiff_impl (ImageBuf &R, const ImageBuf &A, const ImageBuf &B,
              ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        ImageBuf::Iterator<Rtype> r (R, roi);
        ImageBuf::ConstIterator<Atype> a (A, roi);
        ImageBuf::ConstIterator<Btype> b (B, roi);
        for ( ;  !r.done();  ++r, ++a, ++b)
            for (int c = roi.chbegin;  c < roi.chend;  ++c)
                r[c] = std::abs (a[c] - b[c]);
    });
    return true;
}


template<class Rtype, class Atype>
static bool
absdiff_impl (ImageBuf &R, const ImageBuf &A, const float *b,
              ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        ImageBuf::Iterator<Rtype> r (R, roi);
        ImageBuf::ConstIterator<Atype> a (A, roi);
        for ( ;  !r.done();  ++r, ++a)
            for (int c = roi.chbegin;  c < roi.chend;  ++c)
                r[c] = std::abs (a[c] - b[c]);
    });
    return true;
}




bool
ImageBufAlgo::absdiff (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                       ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &A, &B))
        return false;
    ROI origroi = roi;
    roi.chend = std::min (roi.chend, std::min (A.nchannels(), B.nchannels()));
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES3 (ok, "absdiff", absdiff_impl, dst.spec().format,
                                 A.spec().format, B.spec().format,
                                 dst, A, B, roi, nthreads);

    if (roi.chend < origroi.chend && A.nchannels() != B.nchannels()) {
        // Edge case: A and B differed in nchannels, we allocated dst to be
        // the bigger of them, but adjusted roi to be the lesser. Now handle
        // the channels that got left out because they were not common to
        // all the inputs.
        ASSERT (roi.chend <= dst.nchannels());
        roi.chbegin = roi.chend;
        roi.chend = origroi.chend;
        if (A.nchannels() > B.nchannels()) { // A exists
            abs (dst, A, roi, nthreads);
        } else { // B exists
            abs (dst, B, roi, nthreads);
        }
    }
    return ok;
}



bool
ImageBufAlgo::absdiff (ImageBuf &dst, const ImageBuf &A, const float *b,
                       ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &A, IBAprep_CLAMP_MUTUAL_NCHANNELS))
        return false;
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "absdiff", absdiff_impl, dst.spec().format,
                          A.spec().format, dst, A, b, roi, nthreads);
    return ok;
}



bool
ImageBufAlgo::absdiff (ImageBuf &dst, const ImageBuf &A, float b,
                       ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &A, IBAprep_CLAMP_MUTUAL_NCHANNELS))
        return false;
    int nc = dst.nchannels();
    float *vals = ALLOCA (float, nc);
    for (int c = 0;  c < nc;  ++c)
        vals[c] = b;
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "absdiff", absdiff_impl, dst.spec().format,
                          A.spec().format, dst, A, vals, roi, nthreads);
    return ok;
}



bool
ImageBufAlgo::abs (ImageBuf &dst, const ImageBuf &A, ROI roi, int nthreads)
{
    // Define abs in terms of absdiff(A,0.0)
    return absdiff (dst, A, 0.0f, roi, nthreads);
}




template<class Rtype, class Atype>
static bool
pow_impl (ImageBuf &R, const ImageBuf &A, const float *b,
          ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        ImageBuf::ConstIterator<Atype> a (A, roi);
        for (ImageBuf::Iterator<Rtype> r (R, roi);  !r.done();  ++r, ++a)
            for (int c = roi.chbegin;  c < roi.chend;  ++c)
                r[c] = pow (a[c], b[c]);
    });
    return true;
}


bool
ImageBufAlgo::pow (ImageBuf &dst, const ImageBuf &A, const float *b,
                   ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &A, IBAprep_CLAMP_MUTUAL_NCHANNELS))
        return false;
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "pow", pow_impl, dst.spec().format,
                          A.spec().format, dst, A, b, roi, nthreads);
    return ok;
}



bool
ImageBufAlgo::pow (ImageBuf &dst, const ImageBuf &A, float b,
                   ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &A, IBAprep_CLAMP_MUTUAL_NCHANNELS))
        return false;
    int nc = A.nchannels();
    float *vals = ALLOCA (float, nc);
    for (int c = 0;  c < nc;  ++c)
        vals[c] = b;
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "pow", pow_impl, dst.spec().format,
                          A.spec().format, dst, A, vals, roi, nthreads);
    return ok;
}





template<class D, class S>
static bool
channel_sum_ (ImageBuf &dst, const ImageBuf &src,
              const float *weights, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        ImageBuf::Iterator<D> d (dst, roi);
        ImageBuf::ConstIterator<S> s (src, roi);
        for ( ;  !d.done();  ++d, ++s) {
            float sum = 0.0f;
            for (int c = roi.chbegin;  c < roi.chend;  ++c)
                sum += s[c] * weights[c];
            d[0] = sum;
        }
    });
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
    if (! IBAprep (dstroi, &dst))
        return false;

    if (! weights) {
        float *local_weights = ALLOCA (float, roi.chend);
        for (int c = 0; c < roi.chend; ++c)
            local_weights[c] = 1.0f;
        weights = &local_weights[0];
    }

    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "channel_sum", channel_sum_,
                          dst.spec().format, src.spec().format,
                          dst, src, weights, roi, nthreads);
    return ok;
}




inline float rangecompress (float x)
{
    // Formula courtesy of Sony Pictures Imageworks
#if 0    /* original coeffs -- identity transform for vals < 1 */
    const float x1 = 1.0, a = 1.2607481479644775391;
    const float b = 0.28785100579261779785, c = -1.4042005538940429688;
#else    /* but received wisdom is that these work better */
    const float x1 = 0.18, a = -0.54576885700225830078;
    const float b = 0.18351669609546661377, c = 284.3577880859375;
#endif

    float absx = fabsf(x);
    if (absx <= x1)
        return x;
    return copysignf (a + b * logf(fabsf(c*absx + 1.0f)), x);
}



inline float rangeexpand (float y)
{
    // Formula courtesy of Sony Pictures Imageworks
#if 0    /* original coeffs -- identity transform for vals < 1 */
    const float x1 = 1.0, a = 1.2607481479644775391;
    const float b = 0.28785100579261779785, c = -1.4042005538940429688;
#else    /* but received wisdom is that these work better */
    const float x1 = 0.18, a = -0.54576885700225830078;
    const float b = 0.18351669609546661377, c = 284.3577880859375;
#endif

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



template<class Rtype, class Atype>
static bool
rangecompress_ (ImageBuf &R, const ImageBuf &A,
                bool useluma, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        const ImageSpec &Aspec (A.spec());
        int alpha_channel = Aspec.alpha_channel;
        int z_channel = Aspec.z_channel;
        if (roi.nchannels() < 3 ||
            (alpha_channel >= roi.chbegin && alpha_channel < roi.chbegin+3) ||
            (z_channel >= roi.chbegin && z_channel < roi.chbegin+3)) {
            useluma = false;  // No way to use luma
        }

        if (&R == &A) {
            // Special case: operate in-place
            for (ImageBuf::Iterator<Rtype> r (R, roi);  !r.done();  ++r) {
                if (useluma) {
                    float luma = 0.21264f * r[roi.chbegin] + 0.71517f * r[roi.chbegin+1] + 0.07219f * r[roi.chbegin+2];
                    float scale = luma > 0.0f ? rangecompress (luma) / luma : 0.0f;
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
        } else {
            ImageBuf::ConstIterator<Atype> a (A, roi);
            for (ImageBuf::Iterator<Rtype> r (R, roi);  !r.done();  ++r, ++a) {
                if (useluma) {
                    float luma = 0.21264f * a[roi.chbegin] + 0.71517f * a[roi.chbegin+1] + 0.07219f * a[roi.chbegin+2];
                    float scale = luma > 0.0f ? rangecompress (luma) / luma : 0.0f;
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
                            r[c] = rangecompress (a[c]);
                    }
                }
            }
        }
    });
    return true;
}



template<class Rtype, class Atype>
static bool
rangeexpand_ (ImageBuf &R, const ImageBuf &A,
              bool useluma, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        const ImageSpec &Aspec (A.spec());
        int alpha_channel = Aspec.alpha_channel;
        int z_channel = Aspec.z_channel;
        if (roi.nchannels() < 3 ||
            (alpha_channel >= roi.chbegin && alpha_channel < roi.chbegin+3) ||
            (z_channel >= roi.chbegin && z_channel < roi.chbegin+3)) {
            useluma = false;  // No way to use luma
        }

        if (&R == &A) {
            // Special case: operate in-place
            for (ImageBuf::Iterator<Rtype> r (R, roi);  !r.done();  ++r) {
                if (useluma) {
                    float luma = 0.21264f * r[roi.chbegin] + 0.71517f * r[roi.chbegin+1] + 0.07219f * r[roi.chbegin+2];
                    float scale = luma > 0.0f ? rangeexpand (luma) / luma : 0.0f;
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
        } else {
            ImageBuf::ConstIterator<Atype> a (A, roi);
            for (ImageBuf::Iterator<Rtype> r (R, roi);  !r.done();  ++r, ++a) {
                if (useluma) {
                    float luma = 0.21264f * a[roi.chbegin] + 0.71517f * a[roi.chbegin+1] + 0.07219f * a[roi.chbegin+2];
                    float scale = luma > 0.0f ? rangeexpand (luma) / luma : 0.0f;
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
                            r[c] = rangeexpand (a[c]);
                    }
                }
            }
        }
    });
    return true;
}



bool
ImageBufAlgo::rangecompress (ImageBuf &dst, const ImageBuf &src,
                             bool useluma, ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &src, IBAprep_CLAMP_MUTUAL_NCHANNELS))
        return false;
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "rangecompress", rangecompress_,
                          dst.spec().format, src.spec().format,
                          dst, src, useluma, roi, nthreads);
    return ok;
}



bool
ImageBufAlgo::rangeexpand (ImageBuf &dst, const ImageBuf &src,
                           bool useluma, ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &src, IBAprep_CLAMP_MUTUAL_NCHANNELS))
        return false;
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "rangeexpand", rangeexpand_,
                          dst.spec().format, src.spec().format,
                          dst, src, useluma, roi, nthreads);
    return ok;
}



template<class Rtype, class Atype>
static bool
unpremult_ (ImageBuf &R, const ImageBuf &A, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        int alpha_channel = A.spec().alpha_channel;
        int z_channel = A.spec().z_channel;
        if (&R == &A) {
            for (ImageBuf::Iterator<Rtype> r (R, roi);  !r.done();  ++r) {
                float alpha = r[alpha_channel];
                if (alpha == 0.0f || alpha == 1.0f)
                    continue;
                for (int c = roi.chbegin;  c < roi.chend;  ++c)
                    if (c != alpha_channel && c != z_channel)
                        r[c] = r[c] / alpha;
            }
        } else {
            ImageBuf::ConstIterator<Atype> a (A, roi);
            for (ImageBuf::Iterator<Rtype> r (R, roi);  !r.done();  ++r, ++a) {
                float alpha = a[alpha_channel];
                if (alpha == 0.0f || alpha == 1.0f) {
                    for (int c = roi.chbegin;  c < roi.chend;  ++c)
                        r[c] = a[c];
                    continue;
                }
                for (int c = roi.chbegin;  c < roi.chend;  ++c)
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
ImageBufAlgo::unpremult (ImageBuf &dst, const ImageBuf &src,
                         ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &src, IBAprep_CLAMP_MUTUAL_NCHANNELS))
        return false;
    if (src.spec().alpha_channel < 0 
        // Wise?  || src.spec().get_int_attribute("oiio:UnassociatedAlpha") != 0
        ) {
        // If there is no alpha channel, just *copy* instead of dividing
        // by alpha.
        if (&dst != &src)
            return paste (dst, src.spec().x, src.spec().y, src.spec().z,
                          roi.chbegin, src, roi, nthreads);
        return true;
    }
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "unpremult", unpremult_, dst.spec().format,
                          src.spec().format, dst, src, roi, nthreads);
    // Mark the output as having unassociated alpha
    dst.specmod().attribute ("oiio:UnassociatedAlpha", 1);
    return ok;
}



template<class Rtype, class Atype>
static bool
premult_ (ImageBuf &R, const ImageBuf &A, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        int alpha_channel = A.spec().alpha_channel;
        int z_channel = A.spec().z_channel;
            if (&R == &A) {
            for (ImageBuf::Iterator<Rtype> r (R, roi);  !r.done();  ++r) {
                float alpha = r[alpha_channel];
                if (alpha == 1.0f)
                    continue;
                for (int c = roi.chbegin;  c < roi.chend;  ++c)
                    if (c != alpha_channel && c != z_channel)
                        r[c] = r[c] * alpha;
            }
        } else {
            ImageBuf::ConstIterator<Atype> a (A, roi);
            for (ImageBuf::Iterator<Rtype> r (R, roi);  !r.done();  ++r, ++a) {
                float alpha = a[alpha_channel];
                for (int c = roi.chbegin;  c < roi.chend;  ++c)
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
ImageBufAlgo::premult (ImageBuf &dst, const ImageBuf &src,
                       ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &src, IBAprep_CLAMP_MUTUAL_NCHANNELS))
        return false;
    if (src.spec().alpha_channel < 0) {
        if (&dst != &src)
            return paste (dst, src.spec().x, src.spec().y, src.spec().z,
                          roi.chbegin, src, roi, nthreads);
        return true;
    }
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "premult", premult_, dst.spec().format,
                          src.spec().format, dst, src, roi, nthreads);
    // Clear the output of any prior marking of associated alpha
    dst.specmod().erase_attribute ("oiio:UnassociatedAlpha");
    return ok;
}



template<class D, class S>
static bool
color_map_ (ImageBuf &dst, const ImageBuf &src,
            int srcchannel, int nknots, int channels,
            array_view<const float> knots,
            ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        if (srcchannel < 0 && src.nchannels() < 3)
            srcchannel = 0;
        roi.chend = std::min (roi.chend, channels);
        ImageBuf::Iterator<D> d (dst, roi);
        ImageBuf::ConstIterator<S> s (src, roi);
        for ( ;  !d.done();  ++d, ++s) {
            float x = srcchannel < 0 ? 0.2126f*s[0] + 0.7152f*s[1] + 0.0722f*s[2]
                                     : s[srcchannel];
            for (int c = roi.chbegin;  c < roi.chend;  ++c) {
                array_view_strided<const float> k (knots.data()+c, nknots, channels);
                d[c] = interpolate_linear (x, k);
            }
        }
    });
    return true;
}



bool
ImageBufAlgo::color_map (ImageBuf &dst, const ImageBuf &src,
                         int srcchannel, int nknots, int channels,
                         array_view<const float> knots,
                         ROI roi, int nthreads)
{
    if (srcchannel >= src.nchannels()) {
        dst.error ("invalid source channel selected");
        return false;
    }
    if (nknots < 2 || knots.size() < size_t(nknots*channels)) {
        dst.error ("not enough knot values supplied");
        return false;
    }
    if (! roi.defined())
        roi = get_roi(src.spec());
    roi.chend = std::min (roi.chend, src.nchannels());
    ROI dstroi = roi;
    dstroi.chbegin = 0;
    dstroi.chend = channels;
    if (! IBAprep (dstroi, &dst))
        return false;
    dstroi.chend = std::min (channels, dst.nchannels());

    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "color_map", color_map_,
                          dst.spec().format, src.spec().format,
                          dst, src, srcchannel, nknots, channels, knots,
                          dstroi, nthreads);
    return ok;
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
static const float magma_data[] = {
    0.000113, 0.000036, 0.001073,  0.003066, 0.002406, 0.016033,
    0.012176, 0.005476, 0.062265,  0.036874, 0.005102, 0.146314,
    0.081757, 0.006177, 0.200758,  0.143411, 0.011719, 0.218382,
    0.226110, 0.019191, 0.221188,  0.334672, 0.027718, 0.212689,
    0.471680, 0.037966, 0.191879,  0.632894, 0.053268, 0.159870,
    0.795910, 0.083327, 0.124705,  0.913454, 0.146074, 0.106311,
    0.970011, 0.248466, 0.120740,  0.991142, 0.384808, 0.167590,
    0.992958, 0.553563, 0.247770,  0.982888, 0.756759, 0.367372,
    0.970800, 0.980633, 0.521749 };
static const float inferno_data[] = {
    0.000113, 0.000036, 0.001073,  0.003275, 0.002178, 0.017634,
    0.015183, 0.003697, 0.068760,  0.046307, 0.002834, 0.130327,
    0.095494, 0.005137, 0.154432,  0.163601, 0.009920, 0.156097,
    0.253890, 0.016282, 0.143715,  0.367418, 0.024893, 0.119982,
    0.500495, 0.038279, 0.089117,  0.642469, 0.061553, 0.057555,
    0.776190, 0.102517, 0.031141,  0.883568, 0.169990, 0.012559,
    0.951614, 0.271639, 0.002704,  0.972636, 0.413571, 0.005451,
    0.943272, 0.599923, 0.035112,  0.884900, 0.822282, 0.140466,
    0.973729, 0.996282, 0.373522, };
static const float plasma_data[] = {
    0.003970, 0.002307, 0.240854,  0.031078, 0.001421, 0.307376,
    0.073167, 0.000740, 0.356714,  0.132456, 0.000066, 0.388040,
    0.209330, 0.000928, 0.390312,  0.300631, 0.005819, 0.358197,
    0.399925, 0.017084, 0.301977,  0.501006, 0.036122, 0.240788,
    0.600808, 0.063814, 0.186921,  0.698178, 0.101409, 0.142698,
    0.790993, 0.151134, 0.106347,  0.874354, 0.216492, 0.076152,
    0.940588, 0.302179, 0.051495,  0.980469, 0.413691, 0.032625,
    0.984224, 0.556999, 0.020728,  0.942844, 0.738124, 0.018271,
    0.868931, 0.944416, 0.015590, };
static const float viridis_data[] = {
    0.057951, 0.000377, 0.088657,  0.064791, 0.009258, 0.145340,
    0.063189, 0.025975, 0.198994,  0.054539, 0.051494, 0.237655,
    0.043139, 0.084803, 0.258811,  0.032927, 0.124348, 0.268148,
    0.025232, 0.169666, 0.271584,  0.019387, 0.221569, 0.270909,
    0.014846, 0.281323, 0.263855,  0.013529, 0.349530, 0.246357,
    0.021457, 0.425216, 0.215605,  0.049317, 0.505412, 0.172291,
    0.112305, 0.585164, 0.121207,  0.229143, 0.657992, 0.070438,
    0.417964, 0.717561, 0.029928,  0.683952, 0.762557, 0.009977,
    0.984709, 0.799651, 0.018243, };



bool
ImageBufAlgo::color_map (ImageBuf &dst, const ImageBuf &src,
                         int srcchannel, string_view mapname,
                         ROI roi, int nthreads)
{
    if (srcchannel >= src.nchannels()) {
        dst.error ("invalid source channel selected");
        return false;
    }
    array_view<const float> knots;
    if (mapname == "magma") {
        knots = array_view<const float> (magma_data);
    } else if (mapname == "inferno") {
        knots = array_view<const float> (inferno_data);
    } else if (mapname == "plasma") {
        knots = array_view<const float> (plasma_data);
    } else if (mapname == "viridis") {
        knots = array_view<const float> (viridis_data);
    } else if (mapname == "blue-red" || mapname == "red-blue" ||
          mapname == "bluered" || mapname == "redblue") {
        static const float k[] = { 0.0f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f };
        knots = array_view<const float> (k);
    } else if (mapname == "spectrum") {
        static const float k[] = { 0, 0, 0.05,     0, 0, 0.75,   0, 0.5, 0,
                                   0.5, 0.5, 0,   1, 0, 0 };
        knots = array_view<const float> (k);
    } else if (mapname == "heat") {
        static const float k[] = { 0, 0, 0,  0.05, 0, 0,  0.25, 0, 0,
                                   0.75, 0.75, 0,  1,1,1 };
        knots = array_view<const float> (k);
    } else {
        dst.error ("Unknown map name \"%s\"", mapname);
        return false;
    }
    return color_map (dst, src, srcchannel, int(knots.size()/3), 3, knots,
                      roi, nthreads);
}





namespace
{

// Make sure isfinite is defined for 'half'
inline bool isfinite (half h) { return h.isFinite(); }


template<typename T>
bool fixNonFinite_ (ImageBuf &dst, ImageBufAlgo::NonFiniteFixMode mode,
                    int *pixelsFixed, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        ROI dstroi = get_roi (dst.spec());
        int count = 0;   // Number of pixels with nonfinite values
    
        if (mode == ImageBufAlgo::NONFINITE_NONE ||
            mode == ImageBufAlgo::NONFINITE_ERROR) {
            // Just count the number of pixels with non-finite values
            for (ImageBuf::Iterator<T,T> pixel (dst, roi);  ! pixel.done();  ++pixel) {
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
            for (ImageBuf::Iterator<T,T> pixel (dst, roi);  ! pixel.done();  ++pixel) {
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
            for (ImageBuf::Iterator<T,T> pixel (dst, roi);  ! pixel.done();  ++pixel) {
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
    });
    return true;
}


bool fixNonFinite_deep_ (ImageBuf &dst, ImageBufAlgo::NonFiniteFixMode mode,
                         int *pixelsFixed, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        int count = 0;   // Number of pixels with nonfinite values
        if (mode == ImageBufAlgo::NONFINITE_NONE ||
            mode == ImageBufAlgo::NONFINITE_ERROR) {
            // Just count the number of pixels with non-finite values
            for (ImageBuf::Iterator<float> pixel (dst, roi);  ! pixel.done();  ++pixel) {
                int samples = pixel.deep_samples ();
                if (samples == 0)
                    continue;
                bool bad = false;
                for (int samp = 0; samp < samples && !bad; ++samp)
                    for (int c = roi.chbegin;  c < roi.chend;  ++c) {
                        float value = pixel.deep_value (c, samp);
                        if (! isfinite(value)) {
                            ++count;
                            bad = true;
                            break;
                        }
                    }
            }
        } else {
            // We don't know what to do with BOX3, so just always set to black.
            // Replace non-finite pixels with black
            for (ImageBuf::Iterator<float> pixel (dst, roi);  ! pixel.done();  ++pixel) {
                int samples = pixel.deep_samples ();
                if (samples == 0)
                    continue;
                bool fixed = false;
                for (int samp = 0; samp < samples; ++samp)
                    for (int c = roi.chbegin;  c < roi.chend;  ++c) {
                        float value = pixel.deep_value (c, samp);
                        if (! isfinite(value)) {
                            pixel.set_deep_value (c, samp, 0.0f);
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
    });
    return true;
}

} // anon namespace



/// Fix all non-finite pixels (nan/inf) using the specified approach
bool
ImageBufAlgo::fixNonFinite (ImageBuf &dst, const ImageBuf &src,
                            NonFiniteFixMode mode, int *pixelsFixed,
                            ROI roi, int nthreads)
{
    if (mode != ImageBufAlgo::NONFINITE_NONE &&
        mode != ImageBufAlgo::NONFINITE_BLACK &&
        mode != ImageBufAlgo::NONFINITE_BOX3 &&
        mode != ImageBufAlgo::NONFINITE_ERROR) {
        // Something went wrong
        dst.error ("fixNonFinite: unknown repair mode");
        return false;
    }

    if (! IBAprep (roi, &dst, &src, IBAprep_SUPPORT_DEEP))
        return false;

    // Initialize
    bool ok = true;
    int pixelsFixed_local;
    if (! pixelsFixed)
        pixelsFixed = &pixelsFixed_local;
    *pixelsFixed = 0;

    // Start by copying dst to src, if they aren't the same image
    if (&dst != &src)
        ok &= ImageBufAlgo::copy (dst, src, TypeDesc::UNKNOWN, roi, nthreads);

    if (dst.deep())
        ok &= fixNonFinite_deep_ (dst, mode, pixelsFixed, roi, nthreads);
    else if (src.spec().format.basetype == TypeDesc::FLOAT)
        ok &= fixNonFinite_<float> (dst, mode, pixelsFixed, roi, nthreads);
    else if (src.spec().format.basetype == TypeDesc::HALF)
        ok &= fixNonFinite_<half> (dst, mode, pixelsFixed, roi, nthreads);
    else if (src.spec().format.basetype == TypeDesc::DOUBLE)
        ok &= fixNonFinite_<double> (dst, mode, pixelsFixed, roi, nthreads);
    // All other format types aren't capable of having nonfinite
    // pixel values, so the copy was enough.

    if (mode == ImageBufAlgo::NONFINITE_ERROR && *pixelsFixed) {
        dst.error ("Nonfinite pixel values found");
        ok = false;
    }
    return ok;
}




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
over_impl (ImageBuf &R, const ImageBuf &A, const ImageBuf &B,
           bool zcomp, bool z_zeroisinf, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        // It's already guaranteed that R, A, and B have matching channel
        // ordering, and have an alpha channel.  So just decode one.
        int nchannels = 0, alpha_channel = 0, z_channel = 0, ncolor_channels = 0;
        decode_over_channels (R, nchannels, alpha_channel,
                              z_channel, ncolor_channels);
        bool has_z = (z_channel >= 0);

        ImageBuf::ConstIterator<Atype> a (A, roi);
        ImageBuf::ConstIterator<Btype> b (B, roi);
        ImageBuf::Iterator<Rtype> r (R, roi);
        for ( ; ! r.done(); ++r, ++a, ++b) {
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
                for (int c = roi.chbegin;  c < roi.chend;  c++)
                    r[c] = a[c] + one_minus_alpha * b[c];
                if (has_z)
                    r[z_channel] = (alpha != 0.0) ? a[z_channel] : b[z_channel];
            } else {
                // B over A -- because we're doing a Z composite
                float alpha = clamp (b[alpha_channel], 0.0f, 1.0f);
                float one_minus_alpha = 1.0f - alpha;
                for (int c = roi.chbegin;  c < roi.chend;  c++)
                    r[c] = b[c] + one_minus_alpha * a[c];
                r[z_channel] = (alpha != 0.0) ? b[z_channel] : a[z_channel];
            }
        }
    });

    return true;
}



bool
ImageBufAlgo::over (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                    ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &A, &B, NULL,
                   IBAprep_REQUIRE_ALPHA | IBAprep_REQUIRE_SAME_NCHANNELS))
        return false;
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES3 (ok, "over", over_impl, dst.spec().format,
                                 A.spec().format, B.spec().format,
                                 dst, A, B, false, false, roi, nthreads);
    return ok && ! dst.has_error();
}



bool
ImageBufAlgo::zover (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                     bool z_zeroisinf, ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &A, &B, NULL,
                   IBAprep_REQUIRE_ALPHA | IBAprep_REQUIRE_Z |
                   IBAprep_REQUIRE_SAME_NCHANNELS))
        return false;
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES3 (ok, "zover", over_impl, dst.spec().format,
                                 A.spec().format, B.spec().format,
                                 dst, A, B, true, z_zeroisinf, roi, nthreads);
    return ok && ! dst.has_error();
}



OIIO_NAMESPACE_END
