// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <cmath>
#include <iostream>
#include <stdexcept>

#include <OpenImageIO/half.h>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/deepdata.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/thread.h>
#include <OpenImageIO/timer.h>

#include "imageio_pvt.h"


OIIO_NAMESPACE_BEGIN


// FIXME -- NOT CORRECT!  This code assumes sorted, non-overlapping samples.
// That is not a valid assumption in general. We will come back to fix this.
template<class DSTTYPE>
static bool
flatten_(ImageBuf& dst, const ImageBuf& src, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [=, &dst, &src](ROI roi) {
        const ImageSpec& srcspec(src.spec());
        const DeepData* dd = src.deepdata();
        int nc             = srcspec.nchannels;
        int AR_channel     = dd->AR_channel();
        int AG_channel     = dd->AG_channel();
        int AB_channel     = dd->AB_channel();
        int Z_channel      = dd->Z_channel();
        int Zback_channel  = dd->Zback_channel();
        int R_channel      = srcspec.channelindex("R");
        int G_channel      = srcspec.channelindex("G");
        int B_channel      = srcspec.channelindex("B");
        float* val         = OIIO_ALLOCA(float, nc);
        float& ARval(val[AR_channel]);
        float& AGval(val[AG_channel]);
        float& ABval(val[AB_channel]);

        for (ImageBuf::Iterator<DSTTYPE> r(dst, roi); !r.done(); ++r) {
            int x = r.x(), y = r.y(), z = r.z();
            int samps = src.deep_samples(x, y, z);
            // Clear accumulated values for this pixel (0 for colors, big for Z)
            memset(val, 0, nc * sizeof(float));
            if (Z_channel >= 0 && samps == 0)
                val[Z_channel] = 1.0e30;
            if (Zback_channel >= 0 && samps == 0)
                val[Zback_channel] = 1.0e30;
            for (int s = 0; s < samps; ++s) {
                float AR = ARval, AG = AGval, AB = ABval;  // make copies
                float alpha = (AR + AG + AB) / 3.0f;
                if (alpha >= 1.0f)
                    break;
                for (int c = 0; c < nc; ++c) {
                    float v = src.deep_value(x, y, z, c, s);
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

            for (int c = roi.chbegin; c < roi.chend; ++c)
                r[c] = val[c];
        }
    });
    return true;
}


bool
ImageBufAlgo::flatten(ImageBuf& dst, const ImageBuf& src, ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::flatten");
    if (!src.deep()) {
        // For some reason, we were asked to flatten an already-flat image.
        // So just copy it.
        return dst.copy(src);
    }

    // Construct an ideal spec for dst, which is like src but not deep.
    ImageSpec force_spec = src.spec();
    force_spec.deep      = false;
    force_spec.channelformats.clear();

    if (!IBAprep(roi, &dst, &src, NULL, &force_spec,
                 IBAprep_SUPPORT_DEEP | IBAprep_DEEP_MIXED))
        return false;
    if (dst.spec().deep) {
        dst.errorfmt("Cannot flatten to a deep image");
        return false;
    }

    const DeepData* dd = src.deepdata();
    if (dd->AR_channel() < 0 || dd->AG_channel() < 0 || dd->AB_channel() < 0) {
        dst.errorfmt("No alpha channel could be identified");
        return false;
    }

    bool ok;
    OIIO_DISPATCH_TYPES(ok, "flatten", flatten_, dst.spec().format, dst, src,
                        roi, nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::flatten(const ImageBuf& src, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = flatten(result, src, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::flatten error");
    return result;
}



bool
ImageBufAlgo::deepen(ImageBuf& dst, const ImageBuf& src, float zvalue, ROI roi,
                     int /*nthreads*/)
{
    pvt::LoggedTimer logtime("IBA::deepen");
    if (src.deep()) {
        // For some reason, we were asked to deepen an already-deep image.
        // So just copy it.
        return dst.copy(src);
        // FIXME: once paste works for deep files, this should really be
        // return paste (dst, roi.xbegin, roi.ybegin, roi.zbegin, roi.chbegin,
        //               src, roi, nthreads);
    }

    // Construct an ideal spec for dst, which is like src but deep.
    const ImageSpec& srcspec(src.spec());
    int nc               = srcspec.nchannels;
    int zback_channel    = -1;
    ImageSpec force_spec = srcspec;
    force_spec.deep      = true;
    force_spec.set_format(TypeDesc::FLOAT);
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

    if (!IBAprep(roi, &dst, &src, NULL, &force_spec,
                 IBAprep_SUPPORT_DEEP | IBAprep_DEEP_MIXED))
        return false;
    if (!dst.deep()) {
        dst.errorfmt("Cannot deepen to a flat image");
        return false;
    }

    span<float> pixel = OIIO_ALLOCA_SPAN(float, nc);

    // First, figure out which pixels get a sample and which do not
    for (int z = roi.zbegin; z < roi.zend; ++z)
        for (int y = roi.ybegin; y < roi.yend; ++y)
            for (int x = roi.xbegin; x < roi.xend; ++x) {
                bool has_sample = false;
                src.getpixel(x, y, z, pixel);
                for (int c = 0; c < nc; ++c)
                    if (c != force_spec.z_channel && c != zback_channel
                        && pixel[c] != 0.0f) {
                        has_sample = true;
                        break;
                    }
                if (!has_sample && !add_z_channel)
                    for (int c = 0; c < nc; ++c)
                        if ((c == force_spec.z_channel || c == zback_channel)
                            && (pixel[c] != 0.0f && pixel[c] < 1e30)) {
                            has_sample = true;
                            break;
                        }
                if (has_sample)
                    dst.set_deep_samples(x, y, z, 1);
            }

    // Now actually set the values
    for (int z = roi.zbegin; z < roi.zend; ++z)
        for (int y = roi.ybegin; y < roi.yend; ++y)
            for (int x = roi.xbegin; x < roi.xend; ++x) {
                if (dst.deep_samples(x, y, z) == 0)
                    continue;
                for (int c = 0; c < nc; ++c)
                    dst.set_deep_value(x, y, z, c, 0 /*sample*/,
                                       src.getchannel(x, y, z, c));
                if (add_z_channel)
                    dst.set_deep_value(x, y, z, nc, 0, zvalue);
            }

    bool ok = true;
    // FIXME -- the above doesn't split into threads. Someday, it should
    // be refactored like this:
    // OIIO_DISPATCH_COMMON_TYPES2 (ok, "deepen", deepen_,
    //                              dst.spec().format, srcspec.format,
    //                              dst, src, add_z_channel, z, roi, nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::deepen(const ImageBuf& src, float zvalue, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = deepen(result, src, zvalue, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::deepen error");
    return result;
}



bool
ImageBufAlgo::deep_merge(ImageBuf& dst, const ImageBuf& A, const ImageBuf& B,
                         bool occlusion_cull, ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::deep_merge");
    if (!A.deep() || !B.deep()) {
        // For some reason, we were asked to merge a flat image.
        dst.errorfmt("deep_merge can only be performed on deep images");
        return false;
    }
    if (!IBAprep(roi, &dst, &A, &B, NULL,
                 IBAprep_SUPPORT_DEEP | IBAprep_REQUIRE_MATCHING_CHANNELS))
        return false;
    if (!dst.deep()) {
        dst.errorfmt("Cannot deep_merge to a flat image");
        return false;
    }

    // First, set the capacity of the dst image to reserve enough space for
    // the segments of both source images, including any splits that may
    // occur.
    DeepData& dstdd(*dst.deepdata());
    const DeepData& Add(*A.deepdata());
    const DeepData& Bdd(*B.deepdata());
    int Azchan     = Add.Z_channel();
    int Azbackchan = Add.Zback_channel();
    int Bzchan     = Bdd.Z_channel();
    int Bzbackchan = Bdd.Zback_channel();
    for (int z = roi.zbegin; z < roi.zend; ++z)
        for (int y = roi.ybegin; y < roi.yend; ++y)
            for (int x = roi.xbegin; x < roi.xend; ++x) {
                int dstpixel            = dst.pixelindex(x, y, z, true);
                int Apixel              = A.pixelindex(x, y, z, true);
                int Bpixel              = B.pixelindex(x, y, z, true);
                int Asamps              = Add.samples(Apixel);
                int Bsamps              = Bdd.samples(Bpixel);
                int nsplits             = 0;
                int self_overlap_splits = 0;
                for (int s = 0; s < Asamps; ++s) {
                    float src_z     = Add.deep_value(Apixel, Azchan, s);
                    float src_zback = Add.deep_value(Apixel, Azbackchan, s);
                    for (int d = 0; d < Bsamps; ++d) {
                        float dst_z     = Bdd.deep_value(Bpixel, Bzchan, d);
                        float dst_zback = Bdd.deep_value(Bpixel, Bzbackchan, d);
                        if (src_z > dst_z && src_z < dst_zback)
                            ++nsplits;
                        if (src_zback > dst_z && src_zback < dst_zback)
                            ++nsplits;
                        if (dst_z > src_z && dst_z < src_zback)
                            ++nsplits;
                        if (dst_zback > src_z && dst_zback < src_zback)
                            ++nsplits;
                    }
                    // Check for splits src vs src -- in case they overlap!
                    for (int ss = s; ss < Asamps; ++ss) {
                        float src_z2     = Add.deep_value(Apixel, Azchan, ss);
                        float src_zback2 = Add.deep_value(Apixel, Azbackchan,
                                                          ss);
                        if (src_z2 > src_z && src_z2 < src_zback)
                            ++self_overlap_splits;
                        if (src_zback2 > src_z && src_zback2 < src_zback)
                            ++self_overlap_splits;
                        if (src_z > src_z2 && src_z < src_zback2)
                            ++self_overlap_splits;
                        if (src_zback > src_z2 && src_zback < src_zback2)
                            ++self_overlap_splits;
                    }
                }
                // Check for splits dst vs dst -- in case they overlap!
                for (int d = 0; d < Bsamps; ++d) {
                    float dst_z     = Bdd.deep_value(Bpixel, Bzchan, d);
                    float dst_zback = Bdd.deep_value(Bpixel, Bzbackchan, d);
                    for (int dd = d; dd < Bsamps; ++dd) {
                        float dst_z2     = Bdd.deep_value(Bpixel, Bzchan, dd);
                        float dst_zback2 = Bdd.deep_value(Bpixel, Bzbackchan,
                                                          dd);
                        if (dst_z2 > dst_z && dst_z2 < dst_zback)
                            ++self_overlap_splits;
                        if (dst_zback2 > dst_z && dst_zback2 < dst_zback)
                            ++self_overlap_splits;
                        if (dst_z > dst_z2 && dst_z < dst_zback2)
                            ++self_overlap_splits;
                        if (dst_zback > dst_z2 && dst_zback < dst_zback2)
                            ++self_overlap_splits;
                    }
                }

                dstdd.set_capacity(dstpixel, Asamps + Bsamps + nsplits
                                                 + self_overlap_splits);
            }

    bool ok = ImageBufAlgo::copy(dst, A, TypeDesc::UNKNOWN, roi, nthreads);

    for (int z = roi.zbegin; z < roi.zend; ++z)
        for (int y = roi.ybegin; y < roi.yend; ++y)
            for (int x = roi.xbegin; x < roi.xend; ++x) {
                int dstpixel = dst.pixelindex(x, y, z, true);
                int Bpixel   = B.pixelindex(x, y, z, true);
                OIIO_DASSERT(dstpixel >= 0);
                // OIIO_UNUSED_OK int oldcap = dstdd.capacity (dstpixel);
                dstdd.merge_deep_pixels(dstpixel, Bdd, Bpixel);
                // OIIO_DASSERT (oldcap == dstdd.capacity(dstpixel) &&
                //          "Broken: we did not preallocate enough capacity");
                if (occlusion_cull)
                    dstdd.occlusion_cull(dstpixel);
            }
    return ok;
}



ImageBuf
ImageBufAlgo::deep_merge(const ImageBuf& A, const ImageBuf& B,
                         bool occlusion_cull, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = deep_merge(result, A, B, occlusion_cull, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::deep_merge error");
    return result;
}



bool
ImageBufAlgo::deep_holdout(ImageBuf& dst, const ImageBuf& src,
                           const ImageBuf& thresh, ROI roi, int /*nthreads*/)
{
    pvt::LoggedTimer logtime("IBA::deep_holdout");
    if (!src.deep() || !thresh.deep()) {
        dst.errorfmt("deep_holdout can only be performed on deep images");
        return false;
    }
    if (!IBAprep(roi, &dst, &src, &thresh, NULL, IBAprep_SUPPORT_DEEP))
        return false;
    if (!dst.deep()) {
        dst.errorfmt("Cannot deep_holdout into a flat image");
        return false;
    }

    DeepData& dstdd(*dst.deepdata());
    const DeepData& srcdd(*src.deepdata());
    // First, reserve enough space in dst, to reduce the number of
    // allocations we'll do later.
    for (int z = roi.zbegin; z < roi.zend; ++z)
        for (int y = roi.ybegin; y < roi.yend; ++y)
            for (int x = roi.xbegin; x < roi.xend; ++x) {
                int dstpixel = dst.pixelindex(x, y, z, true);
                int srcpixel = src.pixelindex(x, y, z, true);
                if (dstpixel >= 0 && srcpixel >= 0)
                    dstdd.set_capacity(dstpixel, srcdd.capacity(srcpixel));
            }
    // Now we compute each pixel: We copy the src pixel to dst, then split
    // any samples that span the opaque threshold, and then delete any
    // samples that lie beyond the threshold.
    int Zchan     = dstdd.Z_channel();
    int Zbackchan = dstdd.Zback_channel();
    const DeepData& threshdd(*thresh.deepdata());
    for (ImageBuf::Iterator<float> r(dst, roi); !r.done(); ++r) {
        int x = r.x(), y = r.y(), z = r.z();
        int srcpixel = src.pixelindex(x, y, z, true);
        if (srcpixel < 0)
            continue;  // Nothing in this pixel
        int dstpixel = dst.pixelindex(x, y, z, true);
        dstdd.copy_deep_pixel(dstpixel, srcdd, srcpixel);
        int threshpixel = thresh.pixelindex(x, y, z, true);
        if (threshpixel < 0)
            continue;  // No threshold mask for this pixel
        float zthresh = threshdd.opaque_z(threshpixel);
        // Eliminate the samples that are entirely beyond the depth
        // threshold. Do this before the split; that makes it less
        // likely that the split will force a re-allocation.
        for (int s = 0, n = dstdd.samples(dstpixel); s < n; ++s) {
            if (dstdd.deep_value(dstpixel, Zchan, s) > zthresh) {
                dstdd.set_samples(dstpixel, s);
                break;
            }
        }
        // Now split any samples that straddle the z.
        if (dstdd.split(dstpixel, zthresh)) {
            // If a split did occur, do another discard pass.
            for (int s = 0, n = dstdd.samples(dstpixel); s < n; ++s) {
                if (dstdd.deep_value(dstpixel, Zbackchan, s) > zthresh) {
                    dstdd.set_samples(dstpixel, s);
                    break;
                }
            }
        }
    }
    return true;
}



ImageBuf
ImageBufAlgo::deep_holdout(const ImageBuf& src, const ImageBuf& thresh, ROI roi,
                           int nthreads)
{
    ImageBuf result;
    bool ok = deep_holdout(result, src, thresh, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::deep_holdout error");
    return result;
}


OIIO_NAMESPACE_END
