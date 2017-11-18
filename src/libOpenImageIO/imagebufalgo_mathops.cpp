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
add_impl (ImageBuf &R, const ImageBuf &A, const ImageBuf &B,
          ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        ImageBuf::Iterator<Rtype> r (R, roi);
        ImageBuf::ConstIterator<Atype> a (A, roi);
        ImageBuf::ConstIterator<Btype> b (B, roi);
        for ( ;  !r.done();  ++r, ++a, ++b)
            for (int c = roi.chbegin;  c < roi.chend;  ++c)
                r[c] = a[c] + b[c];
    });
    return true;
}



template<class Rtype, class Atype>
static bool
add_impl (ImageBuf &R, const ImageBuf &A, const float *b,
          ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        if (R.deep()) {
            array_view<const TypeDesc> channeltypes (R.deepdata()->all_channeltypes());
            ImageBuf::Iterator<Rtype> r (R, roi);
            ImageBuf::ConstIterator<Atype> a (A, roi);
            for ( ;  !r.done();  ++r, ++a) {
                for (int samp = 0, samples = r.deep_samples(); samp < samples; ++samp) {
                    for (int c = roi.chbegin;  c < roi.chend;  ++c) {
                        if (channeltypes[c].basetype == TypeDesc::UINT32)
                            r.set_deep_value (c, samp, a.deep_value_uint(c, samp));
                        else
                            r.set_deep_value (c, samp, a.deep_value(c, samp) + b[c]);
                    }
                }
            }
        } else {
            ImageBuf::Iterator<Rtype> r (R, roi);
            ImageBuf::ConstIterator<Atype> a (A, roi);
            for ( ;  !r.done();  ++r, ++a)
                for (int c = roi.chbegin;  c < roi.chend;  ++c)
                    r[c] = a[c] + b[c];
        }
    });
    return true;
}



bool
ImageBufAlgo::add (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                   ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &A, &B))
        return false;
    ROI origroi = roi;
    roi.chend = std::min (roi.chend, std::min (A.nchannels(), B.nchannels()));
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES3 (ok, "add", add_impl, dst.spec().format,
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
            copy (dst, A, dst.spec().format, roi, nthreads);
        } else { // B exists
            copy (dst, B, dst.spec().format, roi, nthreads);
        }
    }
    return ok;
}



bool
ImageBufAlgo::add (ImageBuf &dst, const ImageBuf &A, const float *b,
                   ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &A,
                   IBAprep_CLAMP_MUTUAL_NCHANNELS | IBAprep_SUPPORT_DEEP))
        return false;

    if (dst.deep()) {
        // While still serial, set up all the sample counts
        dst.deepdata()->set_all_samples (A.deepdata()->all_samples());
    }

    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "add", add_impl, dst.spec().format,
                          A.spec().format, dst, A, b, roi, nthreads);
    return ok;
}



bool
ImageBufAlgo::add (ImageBuf &dst, const ImageBuf &A, float b,
                   ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &A, IBAprep_CLAMP_MUTUAL_NCHANNELS))
        return false;
    int nc = A.nchannels();
    float *vals = ALLOCA (float, nc);
    for (int c = 0;  c < nc;  ++c)
        vals[c] = b;
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "add", add_impl, dst.spec().format,
                          A.spec().format, dst, A, vals, roi, nthreads);
    return ok;
}




template<class Rtype, class Atype, class Btype>
static bool
sub_impl (ImageBuf &R, const ImageBuf &A, const ImageBuf &B,
          ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        ImageBuf::Iterator<Rtype> r (R, roi);
        ImageBuf::ConstIterator<Atype> a (A, roi);
        ImageBuf::ConstIterator<Btype> b (B, roi);
        for ( ;  !r.done();  ++r, ++a, ++b)
            for (int c = roi.chbegin;  c < roi.chend;  ++c)
                r[c] = a[c] - b[c];
    });
    return true;
}



bool
ImageBufAlgo::sub (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                   ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &A, &B))
        return false;
    ROI origroi = roi;
    roi.chend = std::min (roi.chend, std::min (A.nchannels(), B.nchannels()));
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES3 (ok, "sub", sub_impl, dst.spec().format,
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
            copy (dst, A, dst.spec().format, roi, nthreads);
        } else { // B exists
            sub (dst, dst, B, roi, nthreads);
        }
    }
    return ok;
}



bool
ImageBufAlgo::sub (ImageBuf &dst, const ImageBuf &A, const float *b,
                   ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &A,
                   IBAprep_CLAMP_MUTUAL_NCHANNELS | IBAprep_SUPPORT_DEEP))
        return false;

    if (dst.deep()) {
        // While still serial, set up all the sample counts
        dst.deepdata()->set_all_samples (A.deepdata()->all_samples());
    }

    int nc = A.nchannels();
    float *vals = ALLOCA (float, nc);
    for (int c = 0;  c < nc;  ++c)
        vals[c] = -b[c];
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "sub", add_impl, dst.spec().format,
                          A.spec().format, dst, A, vals, roi, nthreads);
    return ok;
}



bool
ImageBufAlgo::sub (ImageBuf &dst, const ImageBuf &A, float b,
                   ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &A, IBAprep_CLAMP_MUTUAL_NCHANNELS))
        return false;
    int nc = A.nchannels();
    float *vals = ALLOCA (float, nc);
    for (int c = 0;  c < nc;  ++c)
        vals[c] = -b;
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "sub", add_impl, dst.spec().format,
                          A.spec().format, dst, A, vals, roi, nthreads);
    return ok;
}



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
        if (R.deep()) {
            // Deep case
            array_view<const TypeDesc> channeltypes (R.deepdata()->all_channeltypes());
            ImageBuf::Iterator<Rtype> r (R, roi);
            ImageBuf::ConstIterator<Atype> a (A, roi);
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
        } else {
            ImageBuf::ConstIterator<Atype> a (A, roi);
            for (ImageBuf::Iterator<Rtype> r (R, roi);  !r.done();  ++r, ++a)
                for (int c = roi.chbegin;  c < roi.chend;  ++c)
                    r[c] = a[c] * b[c];
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
    if (! IBAprep (roi, &dst, &A, IBAprep_CLAMP_MUTUAL_NCHANNELS))
        return false;
    int nc = A.nchannels();
    float *vals = ALLOCA (float, nc);
    for (int c = 0;  c < nc;  ++c)
        vals[c] = b;
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "mul", mul_impl, dst.spec().format,
                          A.spec().format, dst, A, vals, roi, nthreads);
    return ok;
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

    if (dst.deep()) {
        // While still serial, set up all the sample counts
        dst.deepdata()->set_all_samples (A.deepdata()->all_samples());
    }

    int nc = dst.nchannels();
    float *binv = OIIO_ALLOCA (float, nc);
    for (int c = 0; c < nc; ++c)
        binv[c] = (b[c] == 0.0f) ? 0.0f : 1.0f/b[c];
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "div", mul_impl, dst.spec().format,
                          A.spec().format, dst, A, binv, roi, nthreads);
    return ok;
}



bool
ImageBufAlgo::div (ImageBuf &dst, const ImageBuf &A, float b,
                   ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &A, IBAprep_CLAMP_MUTUAL_NCHANNELS))
        return false;
    b = (b == 0.0f) ? 1.0f : 1.0f/b;
    int nc = dst.nchannels();
    float *binv = ALLOCA (float, nc);
    for (int c = 0;  c < nc;  ++c)
        binv[c] = b;
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "div", mul_impl, dst.spec().format,
                          A.spec().format, dst, A, binv, roi, nthreads);
    return ok;
}



template<class Rtype, class ABCtype>
static bool
mad_impl (ImageBuf &R, const ImageBuf &A, const ImageBuf &B, const ImageBuf &C,
          ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        if (   (is_same<Rtype,float>::value || is_same<Rtype,half>::value)
            && (is_same<ABCtype,float>::value || is_same<ABCtype,half>::value)
            // && R.localpixels() // has to be, because it's writeable
            && A.localpixels() && B.localpixels() && C.localpixels()
            // && R.contains_roi(roi)  // has to be, because IBAPrep
            && A.contains_roi(roi) && B.contains_roi(roi) && C.contains_roi(roi)
            && roi.chbegin == 0 && roi.chend == R.nchannels()
            && roi.chend == A.nchannels() && roi.chend == B.nchannels()
            && roi.chend == C.nchannels()) {
            // Special case when all inputs are either float or half, with in-
            // memory contiguous data and we're operating on the full channel
            // range: skip iterators: For these circumstances, we can operate on
            // the raw memory very efficiently. Otherwise, we will need the
            // magic of the the Iterators (and pay the price).
            int nxvalues = roi.width() * R.nchannels();
            for (int z = roi.zbegin; z < roi.zend; ++z)
                for (int y = roi.ybegin; y < roi.yend; ++y) {
                    Rtype         *rraw =         (Rtype *) R.pixeladdr (roi.xbegin, y, z);
                    const ABCtype *araw = (const ABCtype *) A.pixeladdr (roi.xbegin, y, z);
                    const ABCtype *braw = (const ABCtype *) B.pixeladdr (roi.xbegin, y, z);
                    const ABCtype *craw = (const ABCtype *) C.pixeladdr (roi.xbegin, y, z);
                    DASSERT (araw && braw && craw);
                    // The straightforward loop auto-vectorizes very well,
                    // there's no benefit to using explicit SIMD here.
                    for (int x = 0; x < nxvalues; ++x)
                        rraw[x] = araw[x] * braw[x] + craw[x];
                    // But if you did want to explicitly vectorize, this is
                    // how it would look:
                    // int simdend = nxvalues & (~3); // how many float4's?
                    // for (int x = 0; x < simdend; x += 4) {
                    //     simd::float4 a_simd(araw+x), b_simd(braw+x), c_simd(craw+x);
                    //     simd::float4 r_simd = a_simd * b_simd + c_simd;
                    //     r_simd.store (rraw+x);
                    // }
                    // for (int x = simdend; x < nxvalues; ++x)
                    //     rraw[x] = araw[x] * braw[x] + craw[x];
                }
        } else {
            ImageBuf::Iterator<Rtype> r (R, roi);
            ImageBuf::ConstIterator<ABCtype> a (A, roi);
            ImageBuf::ConstIterator<ABCtype> b (B, roi);
            ImageBuf::ConstIterator<ABCtype> c (C, roi);
            for ( ;  !r.done();  ++r, ++a, ++b, ++c) {
                for (int ch = roi.chbegin;  ch < roi.chend;  ++ch)
                    r[ch] = a[ch] * b[ch] + c[ch];
            }
        }
    });
    return true;
}



template<class Rtype, class Atype>
static bool
mad_implf (ImageBuf &R, const ImageBuf &A, const float *b, const float *c,
          ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        ImageBuf::Iterator<Rtype> r (R, roi);
        ImageBuf::ConstIterator<Atype> a (A, roi);
        for ( ;  !r.done();  ++r, ++a)
            for (int ch = roi.chbegin;  ch < roi.chend;  ++ch)
                r[ch] = a[ch] * b[ch] + c[ch];
    });
    return true;
}



bool
ImageBufAlgo::mad (ImageBuf &dst, const ImageBuf &A_, const ImageBuf &B_,
                   const ImageBuf &C_, ROI roi, int nthreads)
{
    const ImageBuf *A = &A_, *B = &B_, *C = &C_;
    if (!A->initialized() || !B->initialized() || !C->initialized()) {
        dst.error ("Uninitialized input image");
        return false;
    }

    // To avoid the full cross-product of dst/A/B/C types, force A,B,C to
    // all be the same data type, copying if we have to.
    TypeDesc abc_type = type_merge (A->spec().format, B->spec().format,
                                    C->spec().format);
    ImageBuf Anew, Bnew, Cnew;
    if (A->spec().format != abc_type) {
        Anew.copy (*A, abc_type);
        A = &Anew;
    }
    if (B->spec().format != abc_type) {
        Bnew.copy (*B, abc_type);
        B = &Bnew;
    }
    if (C->spec().format != abc_type) {
        Cnew.copy (*C, abc_type);
        C = &Cnew;
    }
    ASSERT (A->spec().format == B->spec().format &&
            A->spec().format == C->spec().format);

    if (! IBAprep (roi, &dst, A, B, C))
        return false;
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "mad", mad_impl, dst.spec().format,
                                 abc_type, dst, *A, *B, *C, roi, nthreads);
    return ok;
}



bool
ImageBufAlgo::mad (ImageBuf &dst, const ImageBuf &A, const float *B,
                   const float *C, ROI roi, int nthreads)
{
    if (!A.initialized()) {
        dst.error ("Uninitialized input image");
        return false;
    }
    if (! IBAprep (roi, &dst, &A))
        return false;
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "mad", mad_implf, dst.spec().format,
                                 A.spec().format, dst, A, B, C,
                                 roi, nthreads);
    return ok;
}



bool
ImageBufAlgo::mad (ImageBuf &dst, const ImageBuf &A, float b,
                   float c, ROI roi, int nthreads)
{
    if (!A.initialized()) {
        dst.error ("Uninitialized input image");
        return false;
    }
    if (! IBAprep (roi, &dst, &A))
        return false;
    std::vector<float> B (roi.chend, b);
    std::vector<float> C (roi.chend, c);
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "mad", mad_implf, dst.spec().format,
                                 A.spec().format, dst, A, &B[0], &C[0],
                                 roi, nthreads);
    return ok;
}



bool
ImageBufAlgo::invert (ImageBuf &dst, const ImageBuf &A,
                      ROI roi, int nthreads)
{
    // Calculate invert as simply 1-A == A*(-1)+1
    return mad (dst, A, -1.0, 1.0, roi, nthreads);
}


OIIO_NAMESPACE_END
