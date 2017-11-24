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
#include <OpenImageIO/dassert.h>



OIIO_NAMESPACE_BEGIN



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
