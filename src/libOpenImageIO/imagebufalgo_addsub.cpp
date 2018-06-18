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
#include "imageio_pvt.h"



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
add_impl (ImageBuf &R, const ImageBuf &A, cspan<float> b,
          ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        ImageBuf::Iterator<Rtype> r (R, roi);
        ImageBuf::ConstIterator<Atype> a (A, roi);
        for ( ;  !r.done();  ++r, ++a)
            for (int c = roi.chbegin;  c < roi.chend;  ++c)
                r[c] = a[c] + b[c];
    });
    return true;
}



static bool
add_impl_deep (ImageBuf &R, const ImageBuf &A, cspan<float> b,
               ROI roi, int nthreads)
{
    ASSERT (R.deep());
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        cspan<TypeDesc> channeltypes (R.deepdata()->all_channeltypes());
        ImageBuf::Iterator<float> r (R, roi);
        ImageBuf::ConstIterator<float> a (A, roi);
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
    });
    return true;
}



bool
ImageBufAlgo::add (ImageBuf &dst, Image_or_Const A_, Image_or_Const B_,
                   ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::add");
    if (A_.is_img() && B_.is_img()) {
        const ImageBuf &A(A_.img()), &B(B_.img());
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
    if (A_.is_val() && B_.is_img())  // canonicalize to A_img, B_val
        A_.swap (B_);
    if (A_.is_img() && B_.is_val()) {
        const ImageBuf &A (A_.img());
        cspan<float> b = B_.val();
        if (! IBAprep (roi, &dst, &A,
                       IBAprep_CLAMP_MUTUAL_NCHANNELS | IBAprep_SUPPORT_DEEP))
            return false;
        IBA_FIX_PERCHAN_LEN_DEF (b, A.nchannels());
        if (dst.deep()) {
            // While still serial, set up all the sample counts
            dst.deepdata()->set_all_samples (A.deepdata()->all_samples());
            return add_impl_deep (dst, A, b, roi, nthreads);
        }
        bool ok;
        OIIO_DISPATCH_COMMON_TYPES2 (ok, "add", add_impl, dst.spec().format,
                              A.spec().format, dst, A, b, roi, nthreads);
        return ok;
    }
    // Remaining cases: error
    dst.error ("ImageBufAlgo::add(): at least one argument must be an image");
    return false;
}



ImageBuf
ImageBufAlgo::add (Image_or_Const A, Image_or_Const B, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = add (result, A, B, roi, nthreads);
    if (!ok && !result.has_error())
        result.error ("ImageBufAlgo::add() error");
    return result;
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
ImageBufAlgo::sub (ImageBuf &dst, Image_or_Const A_, Image_or_Const B_,
                   ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::sub");
    if (A_.is_img() && B_.is_img()) {
        const ImageBuf &A(A_.img()), &B(B_.img());
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
                copy (dst, B, dst.spec().format, roi, nthreads);
            }
        }
        return ok;
    }
    if (A_.is_val() && B_.is_img())  // canonicalize to A_img, B_val
        A_.swap (B_);
    if (A_.is_img() && B_.is_val()) {
        const ImageBuf &A (A_.img());
        cspan<float> b = B_.val();
        if (! IBAprep (roi, &dst, &A,
                       IBAprep_CLAMP_MUTUAL_NCHANNELS | IBAprep_SUPPORT_DEEP))
            return false;
        IBA_FIX_PERCHAN_LEN_DEF (b, A.nchannels());
        // Negate b (into a copy)
        int nc = A.nchannels();
        float *vals = ALLOCA (float, nc);
        for (int c = 0;  c < nc;  ++c)
            vals[c] = -b[c];
        b = cspan<float>(vals, nc);
        if (dst.deep()) {
            // While still serial, set up all the sample counts
            dst.deepdata()->set_all_samples (A.deepdata()->all_samples());
            return add_impl_deep (dst, A, b, roi, nthreads);
        }
        bool ok;
        OIIO_DISPATCH_COMMON_TYPES2 (ok, "sub", add_impl, dst.spec().format,
                              A.spec().format, dst, A, b, roi, nthreads);
        return ok;
    }
    // Remaining cases: error
    dst.error ("ImageBufAlgo::sub(): at least one argument must be an image");
    return false;
}



ImageBuf
ImageBufAlgo::sub (Image_or_Const A, Image_or_Const B, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = sub (result, A, B, roi, nthreads);
    if (!ok && !result.has_error())
        result.error ("ImageBufAlgo::sub() error");
    return result;
}


OIIO_NAMESPACE_END
