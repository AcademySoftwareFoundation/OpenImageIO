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



OIIO_NAMESPACE_BEGIN


template<class Rtype, class Atype, class Btype>
static bool
mul_impl (ImageBuf &R, const ImageBuf &A, const ImageBuf &B,
          ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        ImageBuf::Iterator<Rtype> r (R, roi);
        ImageBuf::ConstIterator<Atype> a (A, roi);
        ImageBuf::ConstIterator<Btype> b (B, roi);
        for ( ;  !r.done();  ++r, ++a, ++b)
            for (int c = roi.chbegin;  c < roi.chend;  ++c)
                r[c] = a[c] * b[c];
    });
    return true;
}



bool
ImageBufAlgo::mul (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                   ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &A, &B, NULL, IBAprep_CLAMP_MUTUAL_NCHANNELS))
        return false;
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES3 (ok, "mul", mul_impl, dst.spec().format,
                                 A.spec().format, B.spec().format,
                                 dst, A, B, roi, nthreads);
    // N.B. No need to consider the case where A and B have differing number
    // of channels. Missing channels are assumed 0, multiplication by 0 is
    // 0, so it all just works through the magic of IBAprep.
    return ok;
}



template<class Rtype, class Atype>
static bool
mul_impl (ImageBuf &R, const ImageBuf &A, const float *b,
          ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        ImageBuf::ConstIterator<Atype> a (A, roi);
        for (ImageBuf::Iterator<Rtype> r (R, roi);  !r.done();  ++r, ++a)
            for (int c = roi.chbegin;  c < roi.chend;  ++c)
                r[c] = a[c] * b[c];
    });
    return true;
}



static bool
mul_impl_deep (ImageBuf &R, const ImageBuf &A, const float *b,
               ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        // Deep case
        array_view<const TypeDesc> channeltypes (R.deepdata()->all_channeltypes());
        ImageBuf::Iterator<float> r (R, roi);
        ImageBuf::ConstIterator<float> a (A, roi);
        for ( ;  !r.done();  ++r, ++a) {
            for (int samp = 0, samples = r.deep_samples(); samp < samples; ++samp) {
                for (int c = roi.chbegin;  c < roi.chend;  ++c) {
                    if (channeltypes[c].basetype == TypeDesc::UINT32)
                        r.set_deep_value (c, samp, a.deep_value_uint(c, samp));
                    else
                        r.set_deep_value (c, samp, a.deep_value(c, samp) * b[c]);
                }
            }
        }
    });
    return true;
}



bool
ImageBufAlgo::mul (ImageBuf &dst, const ImageBuf &A, const float *b,
                   ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &A,
                   IBAprep_CLAMP_MUTUAL_NCHANNELS | IBAprep_SUPPORT_DEEP))
        return false;

    if (dst.deep()) {
        // While still serial, set up all the sample counts
        dst.deepdata()->set_all_samples (A.deepdata()->all_samples());
        return mul_impl_deep (dst, A, b, roi, nthreads);
    }

    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "mul", mul_impl, dst.spec().format,
                          A.spec().format, dst, A, b, roi, nthreads);
    return ok;
}



bool
ImageBufAlgo::mul (ImageBuf &dst, const ImageBuf &A, float b,
                   ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &A,
                   IBAprep_CLAMP_MUTUAL_NCHANNELS | IBAprep_SUPPORT_DEEP))
        return false;

    int nc = A.nchannels();
    float *vals = ALLOCA (float, nc);
    for (int c = 0;  c < nc;  ++c)
        vals[c] = b;
    return mul (dst, A, vals, roi, nthreads);
}




template<class Rtype, class Atype, class Btype>
static bool
div_impl (ImageBuf &R, const ImageBuf &A, const ImageBuf &B,
          ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        ImageBuf::Iterator<Rtype> r (R, roi);
        ImageBuf::ConstIterator<Atype> a (A, roi);
        ImageBuf::ConstIterator<Btype> b (B, roi);
        for ( ;  !r.done();  ++r, ++a, ++b)
            for (int c = roi.chbegin;  c < roi.chend;  ++c) {
                float v = b[c];
                r[c] = (v == 0.0f) ? 0.0f : (a[c] / v);
            }
    });
    return true;
}



bool
ImageBufAlgo::div (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                   ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &A, &B, NULL, IBAprep_CLAMP_MUTUAL_NCHANNELS))
        return false;
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES3 (ok, "div", div_impl, dst.spec().format,
                                 A.spec().format, B.spec().format,
                                 dst, A, B, roi, nthreads);
    return ok;
}



bool
ImageBufAlgo::div (ImageBuf &dst, const ImageBuf &A, const float *b,
                   ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &A,
                   IBAprep_CLAMP_MUTUAL_NCHANNELS | IBAprep_SUPPORT_DEEP))
        return false;

    int nc = dst.nchannels();
    float *binv = OIIO_ALLOCA (float, nc);
    for (int c = 0; c < nc; ++c)
        binv[c] = (b[c] == 0.0f) ? 0.0f : 1.0f/b[c];

    if (dst.deep()) {
        // While still serial, set up all the sample counts
        dst.deepdata()->set_all_samples (A.deepdata()->all_samples());
        return mul_impl_deep (dst, A, binv, roi, nthreads);
    }

    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "div", mul_impl, dst.spec().format,
                          A.spec().format, dst, A, binv, roi, nthreads);
    return ok;
}



bool
ImageBufAlgo::div (ImageBuf &dst, const ImageBuf &A, float b,
                   ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &A,
                   IBAprep_CLAMP_MUTUAL_NCHANNELS | IBAprep_SUPPORT_DEEP))
        return false;

    int nc = dst.nchannels();
    float *vals = ALLOCA (float, nc);
    for (int c = 0;  c < nc;  ++c)
        vals[c] = b;
    return div (dst, A, vals, roi, nthreads);
}



OIIO_NAMESPACE_END
