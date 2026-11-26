// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

#include <OpenImageIO/half.h>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/deepdata.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/thread.h>
#include <OpenImageIO/timer.h>

#include "imageio_pvt.h"


OIIO_NAMESPACE_3_1_BEGIN


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
    OIIO::pvt::LoggedTimer logtime("IBA::flatten");
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
                 IBAprep_SUPPORT_DEEP | IBAprep_DEEP_MIXED
                     | IBAprep_CLAMP_MUTUAL_NCHANNELS))
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
    OIIO::pvt::LoggedTimer logtime("IBA::deepen");
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
        if (force_spec.channel_name(c) == "Z")
            force_spec.z_channel = c;
        else if (force_spec.channel_name(c) == "Zback")
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
    OIIO::pvt::LoggedTimer logtime("IBA::deep_merge");
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
    // the segments of both source images, including all splits expected to
    // occur, so that pixels need not be reallocated as they are merged.
    DeepData& dstdd(*dst.deepdata());
    const DeepData& Add(*A.deepdata());
    const DeepData& Bdd(*B.deepdata());
    int zchan     = dstdd.Z_channel();
    int zbackchan = dstdd.Zback_channel();
    // Samples can only be split if they are extended
    bool splittable    = (zchan >= 0 && zbackchan >= 0 && zchan != zbackchan);
    int64_t maxsamples = pvt::deep_split_limit(dstdd.samplesize());
    std::vector<float> zs, zbacks;
    // Retrieve all of a pixel's values of a depth channel, as they will be
    // once converted to dst's type for that channel.
    DeepData scratch;
    auto get_depths = [&](const DeepData& dd, int pixel, int chan,
                          std::vector<float>& vals) {
        bool convert = (dd.channeltype(chan) != dstdd.channeltype(chan));
        if (convert) {
            scratch.init(1, 1, dstdd.channeltype(chan), std::string("Z"));
            scratch.set_samples(0, 1);
        }
        for (int s = 0, n = dd.samples(pixel); s < n; ++s) {
            float v = dd.deep_value(pixel, chan, s);
            if (convert) {
                scratch.set_deep_value(0, 0, 0, v);
                v = scratch.deep_value(0, 0, 0);
            }
            vals.push_back(v);
        }
    };
    for (int z = roi.zbegin; z < roi.zend; ++z)
        for (int y = roi.ybegin; y < roi.yend; ++y)
            for (int x = roi.xbegin; x < roi.xend; ++x) {
                int dstpixel  = dst.pixelindex(x, y, z, true);
                int Apixel    = A.pixelindex(x, y, z, true);
                int Bpixel    = B.pixelindex(x, y, z, true);
                int Asamps    = Add.samples(Apixel);
                int Bsamps    = Bdd.samples(Bpixel);
                int64_t total = int64_t(Asamps) + int64_t(Bsamps);
                // If either pixel is empty, the other is merely copied
                if (Asamps && Bsamps && splittable) {
                    zs.clear();
                    zbacks.clear();
                    get_depths(Add, Apixel, zchan, zs);
                    get_depths(Bdd, Bpixel, zchan, zs);
                    get_depths(Add, Apixel, zbackchan, zbacks);
                    get_depths(Bdd, Bpixel, zbackchan, zbacks);
                    auto depths = pvt::deep_split_depths(zs, zbacks);
                    auto next = pvt::deep_split_next(depths,
                                                     dstdd.channeltype(zchan));
                    total     = pvt::deep_split_count(depths, next, zs, zbacks,
                                                      maxsamples);
                }
                // This is only an estimate; merge_deep_pixels has the final
                // say on what exceeds the limit, so don't reserve for those.
                if (total > maxsamples)
                    total = std::min(int64_t(Asamps) + int64_t(Bsamps),
                                     int64_t(std::numeric_limits<int>::max()));
                dstdd.set_capacity(dstpixel, int(total));
            }

    if (!ImageBufAlgo::copy(dst, A, TypeDesc::UNKNOWN, roi, nthreads))
        return false;

    for (int z = roi.zbegin; z < roi.zend; ++z)
        for (int y = roi.ybegin; y < roi.yend; ++y)
            for (int x = roi.xbegin; x < roi.xend; ++x) {
                int dstpixel = dst.pixelindex(x, y, z, true);
                int Bpixel   = B.pixelindex(x, y, z, true);
                OIIO_DASSERT(dstpixel >= 0);
                // OIIO_UNUSED_OK int oldcap = dstdd.capacity (dstpixel);

                // Cast to int64_t so this calls the bool overload,
                // not the DEPRECATED(3.2) void one.
                bool ok_dd = dstdd.merge_deep_pixels(int64_t(dstpixel), Bdd,
                                                     int64_t(Bpixel));
                if (!ok_dd) {
                    dst.errorfmt(
                        "deep_merge: pixel ({}, {}, {}) could not be merged, "
                        "or would need more than {} samples after splitting. "
                        "If this is a valid image, raise the OIIO attribute "
                        "\"limits:imagesize_MB\".",
                        x, y, z, maxsamples);
                    return false;
                }
                // OIIO_DASSERT (oldcap == dstdd.capacity(dstpixel) &&
                //          "Broken: we did not preallocate enough capacity");
                if (occlusion_cull)
                    dstdd.occlusion_cull(dstpixel);
            }
    return true;
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
    OIIO::pvt::LoggedTimer logtime("IBA::deep_holdout");
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
        bool ok      = dstdd.copy_deep_pixel(dstpixel, srcdd, srcpixel);
        if (!ok)
            return false;
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


OIIO_NAMESPACE_3_1_END
