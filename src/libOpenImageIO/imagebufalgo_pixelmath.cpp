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

#include "OpenImageIO/imagebuf.h"
#include "OpenImageIO/imagebufalgo.h"
#include "OpenImageIO/imagebufalgo_util.h"
#include "OpenImageIO/dassert.h"



OIIO_NAMESPACE_ENTER
{


template<class D, class S>
static bool
clamp_ (ImageBuf &dst, const ImageBuf &src,
        const float *min, const float *max,
        bool clampalpha01, ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Lots of pixels and request for multi threads? Parallelize.
        ImageBufAlgo::parallel_image (
            boost::bind(clamp_<D,S>, boost::ref(dst), boost::cref(src),
                        min, max, clampalpha01,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
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
    OIIO_DISPATCH_TYPES2 (ok, "clamp", clamp_, dst.spec().format,
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



template<class Rtype, class Atype>
static bool
add_impl (ImageBuf &R, const ImageBuf &A, const float *b,
          ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(add_impl<Rtype,Atype>,
                        boost::ref(R), boost::cref(A), b,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
    ImageBuf::Iterator<Rtype> r (R, roi);
    ImageBuf::ConstIterator<Atype> a (A, roi);
    for ( ;  !r.done();  ++r, ++a)
        for (int c = roi.chbegin;  c < roi.chend;  ++c)
            r[c] = a[c] + b[c];
    return true;
}



bool
ImageBufAlgo::add (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                   ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &A, &B, NULL, IBAprep_CLAMP_MUTUAL_NCHANNELS))
        return false;
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES3 (ok, "add", add_impl, dst.spec().format,
                                 A.spec().format, B.spec().format,
                                 dst, A, B, roi, nthreads);
    return ok;
}



bool
ImageBufAlgo::add (ImageBuf &dst, const ImageBuf &A, const float *b,
                   ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &A, IBAprep_CLAMP_MUTUAL_NCHANNELS))
        return false;
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
    if (! IBAprep (roi, &dst, &A, &B, NULL, IBAprep_CLAMP_MUTUAL_NCHANNELS))
        return false;
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES3 (ok, "sub", sub_impl, dst.spec().format,
                                 A.spec().format, B.spec().format,
                                 dst, A, B, roi, nthreads);
    return ok;
}



bool
ImageBufAlgo::sub (ImageBuf &dst, const ImageBuf &A, const float *b,
                   ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &A, IBAprep_CLAMP_MUTUAL_NCHANNELS))
        return false;
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
absdiff_impl (ImageBuf &R, const ImageBuf &A, const ImageBuf &B,
              ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(absdiff_impl<Rtype,Atype,Btype>,
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
            r[c] = std::abs (a[c] - b[c]);
    return true;
}


template<class Rtype, class Atype>
static bool
absdiff_impl (ImageBuf &R, const ImageBuf &A, const float *b,
              ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(absdiff_impl<Rtype,Atype>,
                        boost::ref(R), boost::cref(A), b,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
    ImageBuf::Iterator<Rtype> r (R, roi);
    ImageBuf::ConstIterator<Atype> a (A, roi);
    for ( ;  !r.done();  ++r, ++a)
        for (int c = roi.chbegin;  c < roi.chend;  ++c)
            r[c] = std::abs (a[c] - b[c]);
    return true;
}




bool
ImageBufAlgo::absdiff (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                       ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &A, &B, NULL, IBAprep_CLAMP_MUTUAL_NCHANNELS))
        return false;
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES3 (ok, "absdiff", absdiff_impl, dst.spec().format,
                                 A.spec().format, B.spec().format,
                                 dst, A, B, roi, nthreads);
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
    int nc = A.nchannels();
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
    if (! IBAprep (roi, &dst, &A, &B, NULL, IBAprep_CLAMP_MUTUAL_NCHANNELS))
        return false;
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES3 (ok, "mul", mul_impl, dst.spec().format,
                                 A.spec().format, B.spec().format,
                                 dst, A, B, roi, nthreads);
    return ok;
}



template<class Rtype, class Atype>
static bool
mul_impl (ImageBuf &R, const ImageBuf &A, const float *b,
          ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(mul_impl<Rtype,Atype>, boost::ref(R), boost::cref(A), b,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    ImageBuf::ConstIterator<Atype> a (A, roi);
    for (ImageBuf::Iterator<Rtype> r (R, roi);  !r.done();  ++r, ++a)
        for (int c = roi.chbegin;  c < roi.chend;  ++c)
            r[c] = a[c] * b[c];
    return true;
}


bool
ImageBufAlgo::mul (ImageBuf &dst, const ImageBuf &A, const float *b,
                   ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &A, IBAprep_CLAMP_MUTUAL_NCHANNELS))
        return false;
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
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(div_impl<Rtype,Atype,Btype>,
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
        for (int c = roi.chbegin;  c < roi.chend;  ++c) {
            float v = b[c];
            r[c] = (v == 0.0f) ? 0.0f : (a[c] / v);
        }
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
    if (! IBAprep (roi, &dst, &A, IBAprep_CLAMP_MUTUAL_NCHANNELS))
        return false;
    int nc = dst.nchannels();
    float *binv = OIIO_ALLOCA (float, nc);
    for (int c = 0; c < nc; ++c)
        binv[c] = (b[c] == 0.0f) ? 1.0f : 1.0f/b[c];
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
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(mad_impl<Rtype,ABCtype>, boost::ref(R),
                        boost::cref(A), boost::cref(B), boost::cref(C),
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
    ImageBuf::Iterator<Rtype> r (R, roi);
    ImageBuf::ConstIterator<ABCtype> a (A, roi);
    ImageBuf::ConstIterator<ABCtype> b (B, roi);
    ImageBuf::ConstIterator<ABCtype> c (C, roi);
    for ( ;  !r.done();  ++r, ++a, ++b, ++c)
        for (int ch = roi.chbegin;  ch < roi.chend;  ++ch)
            r[ch] = a[ch] * b[ch] + c[ch];
    return true;
}



template<class Rtype, class Atype>
static bool
mad_implf (ImageBuf &R, const ImageBuf &A, const float *b, const float *c,
          ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(mad_implf<Rtype,Atype>, boost::ref(R),
                        boost::cref(A), b, c,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
    ImageBuf::Iterator<Rtype> r (R, roi);
    ImageBuf::ConstIterator<Atype> a (A, roi);
    for ( ;  !r.done();  ++r, ++a)
        for (int ch = roi.chbegin;  ch < roi.chend;  ++ch)
            r[ch] = a[ch] * b[ch] + c[ch];
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



template<class Rtype, class Atype>
static bool
pow_impl (ImageBuf &R, const ImageBuf &A, const float *b,
          ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(pow_impl<Rtype,Atype>, boost::ref(R), boost::cref(A), b,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    ImageBuf::ConstIterator<Atype> a (A, roi);
    for (ImageBuf::Iterator<Rtype> r (R, roi);  !r.done();  ++r, ++a)
        for (int c = roi.chbegin;  c < roi.chend;  ++c)
            r[c] = pow (a[c], b[c]);
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
    if (! IBAprep (dstroi, &dst))
        return false;

    if (! weights) {
        float *local_weights = ALLOCA (float, roi.chend);
        for (int c = 0; c < roi.chend; ++c)
            local_weights[c] = 1.0f;
        weights = &local_weights[0];
    }

    bool ok;
    OIIO_DISPATCH_TYPES2 (ok, "channel_sum", channel_sum_,
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
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(rangecompress_<Rtype,Atype>, boost::ref(R),
                        boost::cref(A), useluma,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

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
    } else {
        ImageBuf::ConstIterator<Atype> a (A, roi);
        for (ImageBuf::Iterator<Rtype> r (R, roi);  !r.done();  ++r, ++a) {
            if (useluma) {
                float luma = 0.21264f * r[roi.chbegin] + 0.71517f * r[roi.chbegin+1] + 0.07219f * r[roi.chbegin+2];
                float scale = rangecompress (luma) / luma;
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
    return true;
}



template<class Rtype, class Atype>
static bool
rangeexpand_ (ImageBuf &R, const ImageBuf &A,
              bool useluma, ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(rangeexpand_<Rtype,Atype>, boost::ref(R), 
                        boost::cref(A), useluma,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

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
    } else {
        ImageBuf::ConstIterator<Atype> a (A, roi);
        for (ImageBuf::Iterator<Rtype> r (R, roi);  !r.done();  ++r, ++a) {
            if (useluma) {
                float luma = 0.21264f * r[roi.chbegin] + 0.71517f * r[roi.chbegin+1] + 0.07219f * r[roi.chbegin+2];
                float scale = rangeexpand (luma) / luma;
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
    return true;
}



bool
ImageBufAlgo::rangecompress (ImageBuf &dst, const ImageBuf &src,
                             bool useluma, ROI roi, int nthreads)
{
    if (! dst.initialized()) {
        dst.error ("in-place rangecompress requires the ImageBuf to be initialized");
        return false;
    }
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
    if (! dst.initialized()) {
        dst.error ("in-place rangeexpand requires the ImageBuf to be initialized");
        return false;
    }
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
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(unpremult_<Rtype,Atype>, boost::ref(R), boost::cref(A),
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

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
    return true;
}



bool
ImageBufAlgo::unpremult (ImageBuf &dst, const ImageBuf &src,
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
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "unpremult", unpremult_, dst.spec().format,
                          src.spec().format, dst, src, roi, nthreads);
    return ok;
}



template<class Rtype, class Atype>
static bool
premult_ (ImageBuf &R, const ImageBuf &A, ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(premult_<Rtype,Atype>, boost::ref(R), boost::cref(A),
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

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
    return ok;
}




namespace
{

// Make sure isfinite is defined for 'half'
inline bool isfinite (half h) { return h.isFinite(); }


template<typename T>
bool fixNonFinite_ (ImageBuf &dst, ImageBufAlgo::NonFiniteFixMode mode,
                    int *pixelsFixed, ROI roi, int nthreads)
{
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
ImageBufAlgo::fixNonFinite (ImageBuf &dst, const ImageBuf &src,
                            NonFiniteFixMode mode, int *pixelsFixed,
                            ROI roi, int nthreads)
{
    if (mode != ImageBufAlgo::NONFINITE_NONE &&
        mode != ImageBufAlgo::NONFINITE_BLACK &&
        mode != ImageBufAlgo::NONFINITE_BOX3) {
        // Something went wrong
        dst.error ("fixNonFinite: unknown repair mode");
        return false;
    }

    if (! IBAprep (roi, &dst, &src))
        return false;

    // Initialize
    if (pixelsFixed)
        *pixelsFixed = 0;

    // Start by copying dst to src, if they aren't the same image
    if (&dst != &src)
        ImageBufAlgo::paste (dst, roi.xbegin, roi.ybegin, roi.zbegin, 0,
                             src, roi, nthreads);

    switch (src.spec().format.basetype) {
    case TypeDesc::FLOAT :
        return fixNonFinite_<float> (dst, mode, pixelsFixed, roi, nthreads);
    case TypeDesc::HALF  :
        return fixNonFinite_<half> (dst, mode, pixelsFixed, roi, nthreads);
    case TypeDesc::DOUBLE:
        return fixNonFinite_<double> (dst, mode, pixelsFixed, roi, nthreads);
    default:
        // All other format types aren't capable of having nonfinite
        // pixel values, so the copy was enough.
        return true;
    }
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
over_impl (ImageBuf &R, const ImageBuf &A, const ImageBuf &B,
           bool zcomp, bool z_zeroisinf, ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(over_impl<Rtype,Atype,Btype>,
                        boost::ref(R), boost::cref(A), boost::cref(B),
                        zcomp, z_zeroisinf, _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case...

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



}
OIIO_NAMESPACE_EXIT
