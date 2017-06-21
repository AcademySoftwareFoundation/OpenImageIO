/*
  Copyright 2013 Larry Gritz and the other authors and contributors.
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


#include <OpenEXR/half.h>

#include <cmath>
#include <iostream>
#include <stdexcept>

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/deepdata.h>
#include <OpenImageIO/dassert.h>
#include <OpenImageIO/thread.h>
#include <OpenImageIO/fmath.h>


OIIO_NAMESPACE_BEGIN


// FIXME -- NOT CORRECT!  This code assumes sorted, non-overlapping samples.
// That is not a valid assumption in general. We will come back to fix this.
template<class DSTTYPE>
static bool
flatten_ (ImageBuf &dst, const ImageBuf &src, 
          ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [=,&dst,&src](ROI roi){
        const ImageSpec &srcspec (src.spec());
        const DeepData *dd = src.deepdata();
        int nc = srcspec.nchannels;
        int AR_channel = dd->AR_channel();
        int AG_channel = dd->AG_channel();
        int AB_channel = dd->AB_channel();
        int Z_channel = dd->Z_channel();
        int Zback_channel = dd->Zback_channel();
        int R_channel = srcspec.channelindex ("R");
        int G_channel = srcspec.channelindex ("G");
        int B_channel = srcspec.channelindex ("B");
        float *val = ALLOCA (float, nc);
        float &ARval (val[AR_channel]);
        float &AGval (val[AG_channel]);
        float &ABval (val[AB_channel]);

        for (ImageBuf::Iterator<DSTTYPE> r (dst, roi);  !r.done();  ++r) {
            int x = r.x(), y = r.y(), z = r.z();
            int samps = src.deep_samples (x, y, z);
            // Clear accumulated values for this pixel (0 for colors, big for Z)
            memset (val, 0, nc*sizeof(float));
            if (Z_channel >= 0 && samps == 0)
                val[Z_channel] = 1.0e30;
            if (Zback_channel >= 0 && samps == 0)
                val[Zback_channel] = 1.0e30;
            for (int s = 0;  s < samps;  ++s) {
                float AR = ARval, AG = AGval, AB = ABval;  // make copies
                float alpha = (AR + AG + AB) / 3.0f;
                if (alpha >= 1.0f)
                    break;
                for (int c = 0;  c < nc;  ++c) {
                    float v = src.deep_value (x, y, z, c, s);
                    if (c == Z_channel || c == Zback_channel)
                        val[c] *= alpha;  // because Z are not premultiplied
                    float a;
                    if (c == R_channel)
                        a = AR;
                    else if (c == G_channel)
                        a = AG;
                    else if (c == B_channel)
                        a = AB;
                    else
                        a = alpha;
                    val[c] += (1.0f - a) * v;
                }
            }

            for (int c = roi.chbegin;  c < roi.chend;  ++c)
                r[c] = val[c];
        }
    });
    return true;
}


bool
ImageBufAlgo::flatten (ImageBuf &dst, const ImageBuf &src,
                       ROI roi, int nthreads)
{
    if (! src.deep()) {
        // For some reason, we were asked to flatten an already-flat image.
        // So just copy it.
        return dst.copy (src);
    }

    // Construct an ideal spec for dst, which is like src but not deep.
    ImageSpec force_spec = src.spec();
    force_spec.deep = false;
    force_spec.channelformats.clear();

    if (! IBAprep (roi, &dst, &src, NULL, &force_spec,
                   IBAprep_SUPPORT_DEEP | IBAprep_DEEP_MIXED))
        return false;
    if (dst.spec().deep) {
        dst.error ("Cannot flatten to a deep image");
        return false;
    }

    const DeepData *dd = src.deepdata();
    if (dd->AR_channel() < 0 || dd->AG_channel() < 0 || dd->AB_channel() < 0) {
        dst.error ("No alpha channel could be identified");
        return false;
    }

    bool ok;
    OIIO_DISPATCH_TYPES (ok, "flatten", flatten_, dst.spec().format,
                         dst, src, roi, nthreads);
    return ok;
}



bool
ImageBufAlgo::deepen (ImageBuf &dst, const ImageBuf &src, float zvalue,
                      ROI roi, int nthreads)
{
    if (src.deep()) {
        // For some reason, we were asked to deepen an already-deep image.
        // So just copy it.
        return dst.copy (src);
        // FIXME: once paste works for deep files, this should really be
        // return paste (dst, roi.xbegin, roi.ybegin, roi.zbegin, roi.chbegin,
        //               src, roi, nthreads);
    }

    // Construct an ideal spec for dst, which is like src but deep.
    const ImageSpec &srcspec (src.spec());
    int nc = srcspec.nchannels;
    int zback_channel = -1;
    ImageSpec force_spec = srcspec;
    force_spec.deep = true;
    force_spec.set_format (TypeDesc::FLOAT);
    force_spec.channelformats.clear();
    for (int c = 0; c < nc; ++c) {
        if (force_spec.channelnames[c] == "Z")
            force_spec.z_channel = c;
        else if (force_spec.channelnames[c] == "Zback")
            zback_channel = c;
    }
    bool add_z_channel = (force_spec.z_channel < 0);
    if (add_z_channel) {
        // No z channel? Make one.
        force_spec.z_channel = force_spec.nchannels++;
        force_spec.channelnames.emplace_back("Z");
    }

    if (! IBAprep (roi, &dst, &src, NULL, &force_spec,
                   IBAprep_SUPPORT_DEEP | IBAprep_DEEP_MIXED))
        return false;
    if (! dst.deep()) {
        dst.error ("Cannot deepen to a flat image");
        return false;
    }

    float *pixel = OIIO_ALLOCA (float, nc);

    // First, figure out which pixels get a sample and which do not
    for (int z = roi.zbegin; z < roi.zend; ++z)
    for (int y = roi.ybegin; y < roi.yend; ++y)
    for (int x = roi.xbegin; x < roi.xend; ++x) {
        bool has_sample = false;
        src.getpixel (x, y, z, pixel);
        for (int c = 0; c < nc; ++c)
            if (c != force_spec.z_channel && c != zback_channel
                  && pixel[c] != 0.0f) {
                has_sample = true;
                break;
            }
        if (! has_sample && ! add_z_channel)
            for (int c = 0; c < nc; ++c)
                if ((c == force_spec.z_channel || c == zback_channel)
                    && (pixel[c] != 0.0f && pixel[c] < 1e30)) {
                    has_sample = true;
                    break;
                }
        if (has_sample)
            dst.set_deep_samples (x, y, z, 1);
    }

    // Now actually set the values
    for (int z = roi.zbegin; z < roi.zend; ++z)
    for (int y = roi.ybegin; y < roi.yend; ++y)
    for (int x = roi.xbegin; x < roi.xend; ++x) {
        if (dst.deep_samples (x, y, z) == 0)
            continue;
        for (int c = 0; c < nc; ++c)
            dst.set_deep_value (x, y, z, c, 0 /*sample*/,
                                src.getchannel (x, y, z, c));
        if (add_z_channel)
            dst.set_deep_value (x, y, z, nc, 0, zvalue);
    }

    bool ok = true;
    // FIXME -- the above doesn't split into threads. Someday, it should
    // be refactored like this:
    // OIIO_DISPATCH_COMMON_TYPES2 (ok, "deepen", deepen_,
    //                              dst.spec().format, srcspec.format,
    //                              dst, src, add_z_channel, z, roi, nthreads);
    return ok;
}



bool
ImageBufAlgo::deep_merge (ImageBuf &dst, const ImageBuf &A,
                          const ImageBuf &B, bool occlusion_cull,
                          ROI roi, int nthreads)
{
    if (! A.deep() || ! B.deep()) {
        // For some reason, we were asked to merge a flat image.
        dst.error ("deep_merge can only be performed on deep images");
        return false;
    }
    if (! IBAprep (roi, &dst, &A, &B, NULL,
                   IBAprep_SUPPORT_DEEP | IBAprep_REQUIRE_MATCHING_CHANNELS))
        return false;
    if (! dst.deep()) {
        dst.error ("Cannot deep_merge to a flat image");
        return false;
    }

    // First, set the capacity of the dst image to reserve enough space for
    // the segments of both source images. It may be that more insertions
    // are needed, due to overlaps, but those will be compartively fewer
    // than doing reallocations for every single sample.
    DeepData &dstdd (*dst.deepdata());
    const DeepData &Add (*A.deepdata());
    const DeepData &Bdd (*B.deepdata());
    for (int z = roi.zbegin; z < roi.zend; ++z)
    for (int y = roi.ybegin; y < roi.yend; ++y)
    for (int x = roi.xbegin; x < roi.xend; ++x) {
        int dstpixel = dst.pixelindex (x, y, z, true);
        int Apixel = A.pixelindex (x, y, z, true);
        int Bpixel = B.pixelindex (x, y, z, true);
        dstdd.set_capacity (dstpixel, Add.capacity(Apixel) + Bdd.capacity(Bpixel));
    }

    bool ok = ImageBufAlgo::copy (dst, A, TypeDesc::UNKNOWN, roi, nthreads);

    for (int z = roi.zbegin; z < roi.zend; ++z)
    for (int y = roi.ybegin; y < roi.yend; ++y)
    for (int x = roi.xbegin; x < roi.xend; ++x) {
        int dstpixel = dst.pixelindex (x, y, z, true);
        int Bpixel = B.pixelindex (x, y, z, true);
        DASSERT (dstpixel >= 0);
        dstdd.merge_deep_pixels (dstpixel, Bdd, Bpixel);
        if (occlusion_cull)
            dstdd.occlusion_cull (dstpixel);
    }
    return ok;
}



bool
ImageBufAlgo::deep_holdout (ImageBuf &dst, const ImageBuf &src,
                            const ImageBuf &thresh,
                            ROI roi, int nthreads)
{
    if (! src.deep() || ! thresh.deep()) {
        dst.error ("deep_holdout can only be performed on deep images");
        return false;
    }
    if (! IBAprep (roi, &dst, &src, &thresh, NULL, &src.spec(),
                   IBAprep_SUPPORT_DEEP))
        return false;
    if (! dst.deep()) {
        dst.error ("Cannot deep_holdout into a flat image");
        return false;
    }

    DeepData &dstdd (*dst.deepdata());
    // First, reserve enough space in dst, to reduce the number of
    // allocations we'll do later.
    {
        ImageBuf::ConstIterator<float> s (src, roi);
        for (ImageBuf::Iterator<float> r (dst, roi); !r.done(); ++r, ++s) {
            if (r.exists() && s.exists()) {
                int dstpixel = dst.pixelindex (r.x(), r.y(), r.z(), true);
                dstdd.set_capacity (dstpixel, s.deep_samples());
            }
        }
    }

    // Now we compute each pixel
    int dst_Zchan = dstdd.Z_channel();
    int dst_ZBackchan = dstdd.Zback_channel();
    const DeepData &srcdd (*src.deepdata());
    const DeepData &threshdd (*thresh.deepdata());
    int thresh_Zchan = threshdd.Z_channel();
    int thresh_Zbackchan = threshdd.Zback_channel();
    int thresh_ARchan = threshdd.AR_channel();
    int thresh_AGchan = threshdd.AG_channel();
    int thresh_ABchan = threshdd.AB_channel();

    // Figure out which chans need adjustment. Exclude non-color chans
    bool *adjustchan = OIIO_ALLOCA (bool, dstdd.channels());
    for (int c = 0; c < dstdd.channels(); ++c) {
        adjustchan[c] = (c != dst_Zchan && c != dst_ZBackchan &&
                         dstdd.channeltype(c) != TypeDesc::UINT32);
    }

    // Because we want to split thresh against dst, we need a temporary
    // thresh pixel. Make a deepdata that's one pixel big for this purpose.
    DeepData threshtmp;
    threshtmp.init (1, threshdd.channels(), threshdd.all_channeltypes(),
                    threshdd.all_channelnames());

    for (ImageBuf::Iterator<float> r (dst, roi);  !r.done();  ++r) {
        // Start by copying src pixel to result. If there's no src
        // samples, we're done.
        int x = r.x(), y = r.y(), z = r.z();
        int srcpixel = src.pixelindex (x, y, z, true);
        int dstpixel = dst.pixelindex (x, y, z, true);
        if (srcpixel < 0 || dstpixel < 0 || ! srcdd.samples(srcpixel))
            continue;
        dstdd.copy_deep_pixel (dstpixel, srcdd, srcpixel);

        // Copy the threshold image pixel into our scratch space. If there
        // are no samples in the threshold image, we're done.
        threshtmp.copy_deep_pixel (0, threshdd, thresh.pixelindex (x, y, z, true));
        if (threshtmp.samples(0) == 0)
            continue;

        // Eliminate the samples that are entirely beyond the depth
        // threshold. Do this before the split; that makes it less likely
        // that the split will force a re-allocation.
        float zthresh = threshtmp.opaque_z (0);
        dstdd.cull_behind (dstpixel, zthresh);

        // Split against all depths in the threshold image
        for (int s = 0, ns = threshtmp.samples(0); s < ns; ++s) {
            float z = threshtmp.deep_value (0, thresh_Zchan, s);
            float zback = threshtmp.deep_value (0, thresh_Zbackchan, s);
            dstdd.split (dstpixel, z);
            if (zback != z)
                dstdd.split (dstpixel, zback);
        }
        for (int s = 0, ns = dstdd.samples(dstpixel); s < ns; ++s) {
            float z = dstdd.deep_value (s, dst_Zchan, s);
            float zback = dstdd.deep_value (s, dst_ZBackchan, s);
            threshtmp.split (0, z);
            if (zback != z)
                threshtmp.split (0, zback);
        }

        // Now walk the lists and adjust opacities
        int threshsamps = threshtmp.samples(0);
        int dstsamples = dstdd.samples (dstpixel);
        float transparency = 1.0f;  // accumulated transparency

        for (int d = 0, t = 0; d < dstsamples;) {
            // d and t are the sample numbers of the next sample to consider
            // for the dst and thresh, respectively.

            // If we've passed full opacity, remove all subsequent dst
            // samples and we're done.
            if (transparency < 1.0e-6) {
                dstdd.erase_samples (dstpixel, d, dstsamples-d);
                break;
            }

            float dz  = dstdd.deep_value (dstpixel, dst_Zchan, d);
            float tz  = t < threshsamps ? threshtmp.deep_value (0, thresh_Zchan, t) : 1e38;

            // If there's a threshold sample in front, or overlapping,
            // adjust the accumulated transparency and advance the threshold
            // sample.
            if (t < threshsamps && tz <= dz) {
                float alpha = (threshtmp.deep_value (0, thresh_ARchan, t) +
                               threshtmp.deep_value (0, thresh_AGchan, t) +
                               threshtmp.deep_value (0, thresh_ABchan, t)) / 3.0f;
                transparency *= OIIO::clamp (1.0f - alpha, 0.0f, 1.0f);
                ++t;
                continue;
            }

            // If we have no more threshold samples, or if the next threshold
            // sample is behind the next dst sample, adjust the dest sample
            // values by the accumulated alpha, and move to the next one.
            for (int c = 0, nc = dstdd.channels(); c < nc; ++c) {
                if (adjustchan[c]) {
                    float v = dstdd.deep_value (dstpixel, c, d);
                    dstdd.set_deep_value (dstpixel, c, d, v*transparency);
                }
            }
            ++d;
        }
    }
    return true;
}



bool
ImageBufAlgo::deep_cull (ImageBuf &dst, const ImageBuf &src,
                            const ImageBuf &thresh,
                            ROI roi, int nthreads)
{
    if (! src.deep() || ! thresh.deep()) {
        dst.error ("deep_cull can only be performed on deep images");
        return false;
    }
    if (! IBAprep (roi, &dst, &src, &thresh, NULL, &src.spec(),
                   IBAprep_SUPPORT_DEEP))
        return false;
    if (! dst.deep()) {
        dst.error ("Cannot deep_cull into a flat image");
        return false;
    }

    DeepData &dstdd (*dst.deepdata());
    const DeepData &srcdd (*src.deepdata());
    // First, reserve enough space in dst, to reduce the number of
    // allocations we'll do later.
    {
        ImageBuf::ConstIterator<float> s (src, roi);
        for (ImageBuf::Iterator<float> r (dst, roi); !r.done(); ++r, ++s) {
            if (r.exists() && s.exists()) {
                int dstpixel = dst.pixelindex (r.x(), r.y(), r.z(), true);
                dstdd.set_capacity (dstpixel, s.deep_samples());
            }
        }
    }
    // Now we compute each pixel: We copy the src pixel to dst, then split
    // any samples that span the opaque threshold, and then delete any
    // samples that lie beyond the threshold.
    const DeepData &threshdd (*thresh.deepdata());
    for (ImageBuf::Iterator<float> r (dst, roi);  !r.done();  ++r) {
        if (!r.exists())
            continue;
        int x = r.x(), y = r.y(), z = r.z();
        int srcpixel = src.pixelindex (x, y, z, true);
        int dstpixel = dst.pixelindex (x, y, z, true);
        if (srcpixel < 0 || srcdd.samples(srcpixel) == 0)
            continue;
        dstdd.copy_deep_pixel (dstpixel, srcdd, srcpixel);
        int threshpixel = thresh.pixelindex (x, y, z, true);
        if (threshpixel < 0)
            continue;
        float zthresh = threshdd.opaque_z (threshpixel);
        // Eliminate the samples that are entirely beyond the depth
        // threshold. Do this before the split; that makes it less
        // likely that the split will force a re-allocation.
        dstdd.cull_behind (dstpixel, zthresh);
        // Now split any samples that straddle the z, and do another
        // discard if the split really occurred.
        if (dstdd.split (dstpixel, zthresh))
            dstdd.cull_behind (dstpixel, zthresh);
    }
    return true;
}


OIIO_NAMESPACE_END
