// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <algorithm>
#include <cmath>
#include <memory>

#include <OpenImageIO/half.h>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/filter.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/platform.h>
#include <OpenImageIO/thread.h>

#include "imageio_pvt.h"

#include "kissfft.hh"



///////////////////////////////////////////////////////////////////////////
// Guidelines for ImageBufAlgo functions:
//
// * Signature will always be:
//       bool function (ImageBuf &dst /* result */,
//                      const ImageBuf &A, ...other inputs...,
//                      ROI roi = ROI::All(),
//                      int nthreads = 0);
//   or
//       ImageBuf function (const ImageBuf &A, ...other inputs...,
//                          ROI roi = ROI::All(),
//                          int nthreads = 0);
// * The ROI should restrict the operation to those pixels (and channels)
//   specified. Default ROI::All() means perform the operation on all
//   pixel in dst's data window.
// * It's ok to omit ROI and threads from the few functions that
//   (a) can't possibly be parallelized, and (b) do not make sense to
//   apply to anything less than the entire image.
// * Be sure to clamp the channel range to those actually used.
// * If dst is initialized, do not change any pixels outside the ROI.
//   If dst is uninitialized, redefine ROI to be the union of the input
//   images' data windows and allocate dst to be that size.
// * Try to always do the "reasonable thing" rather than be too brittle.
// * For errors (where there is no "reasonable thing"), set dst's error
//   condition using dst.error() with dst.error() and return false.
// * Always use IB::Iterators/ConstIterator, NEVER use getpixel/setpixel.
// * Use the iterator Black or Clamp wrap modes to avoid lots of special
//   cases inside the pixel loops.
// * Use OIIO_DISPATCH_* macros to call type-specialized templated
//   implementations.  It is permissible to use OIIO_DISPATCH_COMMON_TYPES_*
//   to tame the cross-product of types, especially for binary functions
//   (A,B inputs as well as R output).
///////////////////////////////////////////////////////////////////////////


OIIO_NAMESPACE_BEGIN


bool
ImageBufAlgo::IBAprep(ROI& roi, ImageBuf* dst, const ImageBuf* A,
                      const ImageBuf* B, const ImageBuf* C,
                      ImageSpec* force_spec, int prepflags)
{
    OIIO_DASSERT(dst);
    if ((A && !A->initialized()) || (B && !B->initialized())
        || (C && !C->initialized())) {
        dst->errorfmt("Uninitialized input image");
        return false;
    }

    int minchans, maxchans;
    if (dst || A || B || C) {
        minchans = 10000;
        maxchans = 1;
        if (dst->initialized()) {
            minchans = std::min(minchans, dst->spec().nchannels);
            maxchans = std::max(maxchans, dst->spec().nchannels);
        }
        if (A && A->initialized()) {
            minchans = std::min(minchans, A->spec().nchannels);
            maxchans = std::max(maxchans, A->spec().nchannels);
        }
        if (B && B->initialized()) {
            minchans = std::min(minchans, B->spec().nchannels);
            maxchans = std::max(maxchans, B->spec().nchannels);
        }
        if (C && C->initialized()) {
            minchans = std::min(minchans, C->spec().nchannels);
            maxchans = std::max(maxchans, C->spec().nchannels);
        }
        if (minchans == 10000 && roi.defined()) {
            // no images, oops, hope roi makes sense
            minchans = maxchans = roi.nchannels();
        }
    } else if (roi.defined()) {
        minchans = maxchans = roi.nchannels();
    } else {
        minchans = maxchans = 1;
    }

    if (dst->initialized()) {
        // Valid destination image.  Just need to worry about ROI.
        if (roi.defined()) {
            // Shrink-wrap ROI to the destination (including chend)
            roi = roi_intersection(roi, get_roi(dst->spec()));
        } else {
            // No ROI? Set it to all of dst's pixel window.
            roi = get_roi(dst->spec());
        }
        // If the dst is initialized but is a cached image, we'll need
        // to fully read it into allocated memory so that we're able
        // to write to it subsequently.
        dst->make_writable(true);
        // Whatever we're about to do to the image, it is almost certain
        // to make any thumbnail wrong, so just clear it.
        dst->clear_thumbnail();

        // Merge source metadata into destination if requested.
        if (prepflags & IBAprep_MERGE_METADATA) {
            if (A && A->initialized())
                dst->specmod().extra_attribs.merge(A->spec().extra_attribs);
            if (B && B->initialized())
                dst->specmod().extra_attribs.merge(B->spec().extra_attribs);
            if (C && C->initialized())
                dst->specmod().extra_attribs.merge(C->spec().extra_attribs);
        }
    } else {
        // Not an initialized destination image!
        if (!A && !roi.defined()) {
            dst->errorfmt(
                "ImageBufAlgo without any guess about region of interest");
            return false;
        }
        ROI full_roi;
        if (!roi.defined()) {
            // No ROI -- make it the union of the pixel regions of the inputs
            roi      = A->roi();
            full_roi = A->roi_full();
            if (B) {
                roi      = roi_union(roi, B->roi());
                full_roi = roi_union(full_roi, B->roi_full());
            }
            if (C) {
                roi      = roi_union(roi, C->roi());
                full_roi = roi_union(full_roi, C->roi_full());
            }
        } else {
            if (A) {
                roi.chend = std::min(roi.chend, A->nchannels());
                if (!(prepflags & IBAprep_NO_COPY_ROI_FULL))
                    full_roi = A->roi_full();
            } else {
                full_roi = roi;
            }
        }
        // Now we allocate space for dst.  Give it A's spec, but adjust
        // the dimensions to match the ROI.
        ImageSpec spec;
        if (A) {
            // If there's an input image, give dst A's spec (with
            // modifications detailed below...)
            if (force_spec) {
                spec = *force_spec;
            } else {
                // If dst is uninitialized and no force_spec was supplied,
                // make it like A, but having number of channels as large as
                // any of the inputs.
                spec = A->spec();
                if (prepflags & IBAprep_MINIMIZE_NCHANNELS)
                    spec.nchannels = minchans;
                else
                    spec.nchannels = maxchans;
                // Fix channel names and designations
                spec.default_channel_names();
                spec.alpha_channel = -1;
                spec.z_channel     = -1;
                for (int c = 0; c < spec.nchannels; ++c) {
                    if (A && A->spec().channel_name(c) != "") {
                        spec.channelnames[c] = A->spec().channel_name(c);
                        if (spec.alpha_channel < 0
                            && A->spec().alpha_channel == c)
                            spec.alpha_channel = c;
                        if (spec.z_channel < 0 && A->spec().z_channel == c)
                            spec.z_channel = c;
                    } else if (B && B->spec().channel_name(c) != "") {
                        spec.channelnames[c] = B->spec().channel_name(c);
                        if (spec.alpha_channel < 0
                            && B->spec().alpha_channel == c)
                            spec.alpha_channel = c;
                        if (spec.z_channel < 0 && B->spec().z_channel == c)
                            spec.z_channel = c;
                    } else if (C && C->spec().channel_name(c) != "") {
                        spec.channelnames[c] = C->spec().channel_name(c);
                        if (spec.alpha_channel < 0
                            && C->spec().alpha_channel == c)
                            spec.alpha_channel = c;
                        if (spec.z_channel < 0 && C->spec().z_channel == c)
                            spec.z_channel = c;
                    }
                }
            }
            // For multiple inputs, if they aren't the same data type, punt and
            // allocate a float buffer. If the user wanted something else,
            // they should have pre-allocated dst with their desired format.
            if ((B && A->spec().format != B->spec().format)
                || (prepflags & IBAprep_DST_FLOAT_PIXELS))
                spec.set_format(TypeDesc::FLOAT);
            if (C
                && (A->spec().format != C->spec().format
                    || B->spec().format != C->spec().format))
                spec.set_format(TypeDesc::FLOAT);
            // No good can come from automatically polluting an ImageBuf
            // with some other ImageBuf's tile sizes.
            spec.tile_width  = 0;
            spec.tile_height = 0;
            spec.tile_depth  = 0;
        } else if (force_spec) {
            spec = *force_spec;
        } else {
            spec.set_format(TypeDesc::FLOAT);
            spec.nchannels = roi.chend;
            spec.default_channel_names();
        }
        // Set the image dimensions based on ROI.
        set_roi(spec, roi);
        if (full_roi.defined())
            set_roi_full(spec, full_roi);
        else
            set_roi_full(spec, roi);

        // Merge source metadata into destination if requested.
        if (prepflags & IBAprep_MERGE_METADATA) {
            if (A && A->initialized())
                spec.extra_attribs.merge(A->spec().extra_attribs);
            if (B && B->initialized())
                spec.extra_attribs.merge(B->spec().extra_attribs);
            if (C && C->initialized())
                spec.extra_attribs.merge(C->spec().extra_attribs);
        }

        if (prepflags & IBAprep_NO_COPY_METADATA)
            spec.extra_attribs.clear();
        else if (!(prepflags & IBAprep_COPY_ALL_METADATA)) {
            // Since we're altering pixels, be sure that any existing SHA
            // hash of dst's pixel values is erased.
            spec.erase_attribute("oiio:SHA-1");
            std::string desc = spec.get_string_attribute("ImageDescription");
            if (desc.size()) {
                Strutil::excise_string_after_head(desc, "oiio:SHA-1=");
                spec.attribute("ImageDescription", desc);
            }
        }

        dst->reset(spec, (prepflags & IBAprep_FILL_ZERO_ALLOC)
                             ? InitializePixels::Yes
                             : InitializePixels::No);

        // If we just allocated more channels than the caller will write,
        // clear the extra channels.
        if (prepflags & IBAprep_CLAMP_MUTUAL_NCHANNELS)
            roi.chend = std::min(roi.chend, minchans);
        roi.chend = std::min(roi.chend, spec.nchannels);
        if (!dst->deep()) {
            if (roi.chbegin > 0) {
                ROI r     = roi;
                r.chbegin = 0;
                r.chend   = roi.chbegin;
                ImageBufAlgo::zero(*dst, r, 1);
            }
            if (roi.chend < dst->nchannels()) {
                ROI r     = roi;
                r.chbegin = roi.chend;
                r.chend   = dst->nchannels();
                ImageBufAlgo::zero(*dst, r, 1);
            }
        }
    }
    if (prepflags & IBAprep_CLAMP_MUTUAL_NCHANNELS)
        roi.chend = std::min(roi.chend, minchans);
    roi.chend = std::min(roi.chend, maxchans);
    if (prepflags & IBAprep_REQUIRE_ALPHA) {
        if (dst->spec().alpha_channel < 0 || (A && A->spec().alpha_channel < 0)
            || (B && B->spec().alpha_channel < 0)
            || (C && C->spec().alpha_channel < 0)) {
            dst->errorfmt("images must have alpha channels");
            return false;
        }
    }
    if (prepflags & IBAprep_REQUIRE_Z) {
        if (dst->spec().z_channel < 0 || (A && A->spec().z_channel < 0)
            || (B && B->spec().z_channel < 0)
            || (C && C->spec().z_channel < 0)) {
            dst->errorfmt("images must have depth channels");
            return false;
        }
    }
    if ((prepflags & IBAprep_REQUIRE_SAME_NCHANNELS)
        || (prepflags & IBAprep_REQUIRE_MATCHING_CHANNELS)) {
        int n = dst->spec().nchannels;
        if ((A && A->spec().nchannels != n) || (B && B->spec().nchannels != n)
            || (C && C->spec().nchannels != n)) {
            dst->errorfmt("images must have the same number of channels");
            return false;
        }
    }
    if (prepflags & IBAprep_REQUIRE_MATCHING_CHANNELS) {
        int n = dst->spec().nchannels;
        for (int c = 0; c < n; ++c) {
            string_view name = dst->spec().channel_name(c);
            if ((A && A->spec().channel_name(c) != name)
                || (B && B->spec().channel_name(c) != name)
                || (C && C->spec().channel_name(c) != name)) {
                dst->errorfmt(
                    "images must have the same channel names and order");
                return false;
            }
        }
    }
    if (prepflags & IBAprep_NO_SUPPORT_VOLUME) {
        if (dst->spec().depth > 1 || (A && A->spec().depth > 1)
            || (B && B->spec().depth > 1) || (C && C->spec().depth > 1)) {
            dst->errorfmt("volumes not supported");
            return false;
        }
    }
    if (dst->deep() || (A && A->deep()) || (B && B->deep())
        || (C && C->deep())) {
        // At least one image is deep
        if (!(prepflags & IBAprep_SUPPORT_DEEP)) {
            // Error if the operation doesn't support deep images
            dst->errorfmt("deep images not supported");
            return false;
        }
        if (!(prepflags & IBAprep_DEEP_MIXED)) {
            // Error if not all images are deep
            if (!dst->deep() || (A && !A->deep()) || (B && !B->deep())
                || (C && !C->deep())) {
                dst->errorfmt("mixed deep & flat images not supported");
                return false;
            }
        }
    }
    return true;
}



bool
ImageBufAlgo::IBAprep(ROI& roi, ImageBuf& dst, cspan<const ImageBuf*> srcs,
                      KWArgs options, ImageSpec* force_spec)
{
    // OIIO_ASSERT(dst);
    // Helper: ANY_SRC returns true if any of the source images satisfy the
    // condition cond.
#define ANY_SRC(cond)                     \
    std::any_of(srcs.begin(), srcs.end(), \
                [&](const ImageBuf* s) { return cond; })

    // Trim trailing NULLs
    while (srcs.size() && srcs.back() == nullptr)
        srcs = srcs.first(srcs.size() - 1);
    // Make sure all inputs are
    if (ANY_SRC(!s || !s->initialized())) {
        dst.errorfmt("Uninitialized input image");
        return false;
    }

    const ImageBuf* A = srcs.size() ? srcs[0] : nullptr;
    int minchans = 10000, maxchans = 1;
    if (srcs.size()) {
        minchans = 10000;
        maxchans = 1;
        if (dst.initialized()) {
            minchans = std::min(minchans, dst.spec().nchannels);
            maxchans = std::max(maxchans, dst.spec().nchannels);
        }
        for (const ImageBuf* s : srcs) {
            if (s && s->initialized()) {
                minchans = std::min(minchans, s->spec().nchannels);
                maxchans = std::max(maxchans, s->spec().nchannels);
            }
        }
        if (minchans == 10000 && roi.defined()) {
            // no images, oops, hope roi makes sense
            minchans = maxchans = roi.nchannels();
        }
    } else if (roi.defined()) {
        // No inputs, no dest, but an ROI -- use it
        minchans = maxchans = roi.nchannels();
    } else {
        // Punt -- one channel
        minchans = maxchans = 1;
    }

    if (dst.initialized()) {
        // Valid destination image.  Just need to worry about ROI.
        if (roi.defined()) {
            // Shrink-wrap ROI to the destination (including chend)
            roi = roi_intersection(roi, get_roi(dst.spec()));
        } else {
            // No ROI? Set it to all of dst's pixel window.
            roi = get_roi(dst.spec());
        }
        // If the dst is initialized but is a cached image, we'll need
        // to fully read it into allocated memory so that we're able
        // to write to it subsequently.
        dst.make_writable(true);
        // Whatever we're about to do to the image, it is almost certain
        // to make any thumbnail wrong, so just clear it.
        dst.clear_thumbnail();

        // Merge source metadata into destination if requested.
        if (options.get_int("merge_metadata")) {
            for (const ImageBuf* s : srcs)
                if (s->initialized())
                    dst.specmod().extra_attribs.merge(s->spec().extra_attribs);
        }
    } else {
        // Not an initialized destination image!
        if (srcs.empty() && !roi.defined()) {
            dst.errorfmt(
                "ImageBufAlgo without any guess about region of interest");
            return false;
        }
        ROI full_roi;
        if (!roi.defined()) {
            // No ROI specified -- make it the union of the pixel regions of
            // the inputs
            for (const ImageBuf* s : srcs) {
                roi      = roi_union(roi, s->roi());
                full_roi = roi_union(full_roi, s->roi_full());
            }
        } else if (srcs.size()) {
            // An ROI was specified and so was at least one input image. Clamp
            // its channels to the number of channels in the first input
            // image.
            roi.chend = std::min(roi.chend, A->nchannels());
            if (options.get_int("copy_roi_full", 1))
                full_roi = A->roi_full();
        } else {
            // ROI defined, but no input images. Make full_roi be the same.
            full_roi = roi;
        }

        // Now we allocate space for dst.  Give it A's spec, but adjust the
        // dimensions to match the ROI.
        ImageSpec spec;
        if (srcs.size()) {
            // If there are any input images, give dst the first one's spec
            // (with modifications detailed below...)
            if (force_spec) {
                // If the user passed a force_spec, use it.
                spec = *force_spec;
            } else {
                // If dst is uninitialized and no force_spec was supplied,
                // make it like A, but having number of channels as large as
                // any of the inputs.
                spec = A->spec();
                if (options.get_int("minimize_nchannels"))
                    spec.nchannels = minchans;
                else
                    spec.nchannels = maxchans;
                // Fix channel names and designations
                spec.default_channel_names();
                spec.alpha_channel = -1;
                spec.z_channel     = -1;
                for (const ImageBuf* s : srcs) {
                    const ImageSpec& sspec(s->spec());
                    for (int c = 0; c < spec.nchannels; ++c) {
                        if (sspec.channel_name(c) != "") {
                            spec.channelnames[c] = sspec.channel_name(c);
                            if (spec.alpha_channel < 0
                                && sspec.alpha_channel == c)
                                spec.alpha_channel = c;
                            if (spec.z_channel < 0 && sspec.z_channel == c)
                                spec.z_channel = c;
                        }
                    }
                }
            }
            // For multiple inputs, if they aren't the same data type, punt and
            // allocate a float buffer. If the user wanted something else,
            // they should have pre-allocated dst with their desired format.
            if (options.get_int("dst_float_pixels")
                || ANY_SRC(s->spec().format != srcs[0]->spec().format)) {
                spec.set_format(TypeDesc::FLOAT);
            }
            // No good can come from automatically polluting an ImageBuf
            // with some other ImageBuf's tile sizes.
            spec.tile_width  = 0;
            spec.tile_height = 0;
            spec.tile_depth  = 0;
        } else if (force_spec) {
            // No input images, but a force_spec was supplied.
            spec = *force_spec;
        } else {
            // No input images, no force_spec -- make it float and the number
            // of channels specified by the ROI.
            spec.set_format(TypeFloat);
            spec.nchannels = roi.chend;
            spec.default_channel_names();
        }

        // If the user passed a dst_format, use it.
        TypeDesc requested_format(options.get_string("dst_format"));
        if (requested_format != TypeUnknown) {
            spec.set_format(requested_format);
        }

        // Set the image dimensions based on ROI.
        set_roi(spec, roi);
        if (full_roi.defined())
            set_roi_full(spec, full_roi);
        else
            set_roi_full(spec, roi);

        // Merge source metadata into destination if requested.
        if (options.get_int("merge_metadata")) {
            for (const ImageBuf* s : srcs)
                if (s->initialized())
                    spec.extra_attribs.merge(s->spec().extra_attribs);
        }

        if (!options.get_bool("copy_metadata", true))
            spec.extra_attribs.clear();
        else if (options.get_string("copy_metadata") != "all") {
            // Since we're altering pixels, be sure that any existing SHA
            // hash of dst's pixel values is erased.
            spec.erase_attribute("oiio:SHA-1");
            std::string desc = spec.get_string_attribute("ImageDescription");
            if (desc.size()) {
                Strutil::excise_string_after_head(desc, "oiio:SHA-1=");
                spec.attribute("ImageDescription", desc);
            }
        }

        dst.reset(spec, options.get_int("fill_zero_alloc")
                            ? InitializePixels::Yes
                            : InitializePixels::No);

        // If we just allocated more channels than the caller will write,
        // clear the extra channels.
        if (options.get_int("clamp_mutual_nchannels"))
            roi.chend = std::min(roi.chend, minchans);
        roi.chend = std::min(roi.chend, spec.nchannels);
        if (!dst.deep()) {
            if (roi.chbegin > 0) {
                ROI r     = roi;
                r.chbegin = 0;
                r.chend   = roi.chbegin;
                ImageBufAlgo::zero(dst, r, 1);
            }
            if (roi.chend < dst.nchannels()) {
                ROI r     = roi;
                r.chbegin = roi.chend;
                r.chend   = dst.nchannels();
                ImageBufAlgo::zero(dst, r, 1);
            }
        }
    }
    if (options.get_int("clamp_mutual_nchannels"))
        roi.chend = std::min(roi.chend, minchans);
    roi.chend = std::min(roi.chend, maxchans);
    if (options.get_int("require_alpha")) {
        if (dst.spec().alpha_channel < 0
            || ANY_SRC(s->spec().alpha_channel < 0)) {
            dst.errorfmt("images must have alpha channels");
            return false;
        }
    }
    if (options.get_int("require_z")) {
        if (dst.spec().z_channel < 0 || ANY_SRC(s->spec().z_channel < 0)) {
            dst.errorfmt("images must have depth channels");
            return false;
        }
    }
    if (options.get_int("require_same_nchannels")
        || options.get_int("require_matching_channels")) {
        if (ANY_SRC(s->nchannels() != dst.nchannels())) {
            dst.errorfmt("images must have the same number of channels");
            return false;
        }
    }
    if (options.get_int("require_matching_channels")) {
        int n = dst.nchannels();
        for (int c = 0; c < n; ++c) {
            string_view name = dst.spec().channel_name(c);
            if (ANY_SRC(s->spec().channel_name(c) != name)) {
                dst.errorfmt(
                    "images must have the same channel names and order");
                return false;
            }
        }
    }
    if (options.get_int("support_volume", 1) == 0) {
        if (dst.spec().depth > 1 || ANY_SRC(s->spec().depth > 1)) {
            dst.errorfmt("volumes not supported");
            return false;
        }
    }
    if (dst.deep() || ANY_SRC(s->deep())) {
        // At least one image is deep
        if (!options.get_bool("support_deep")) {
            // Error if the operation doesn't support deep images
            dst.errorfmt("deep images not supported");
            return false;
        }
        if (options.get_string("support_deep") != "mixed") {
            // Error if not all images are deep
            if (!dst.deep() || ANY_SRC(!s->deep())) {
                dst.errorfmt("mixed deep & flat images not supported");
                return false;
            }
        }
    } else if (options.get_string("support_deep") == "required") {
        // Error if deep images are required but none are present
        dst.errorfmt("deep images required");
        return false;
    }
    return true;
#undef ANY_SRC
}



ImageBuf
ImageBufAlgo::perpixel_op(const ImageBuf& src,
                          function_view<bool(span<float>, cspan<float>)> op,
                          KWArgs options)
{
    using namespace ImageBufAlgo;
    ImageBuf result;
    ROI roi;
    if (!IBAprep(roi, result, { &src }, options))
        return result;
    int nthreads = options.get_int("nthreads", 0);
    bool ok      = true;
    if (result.pixeltype() == TypeFloat && src.pixeltype() == TypeFloat) {
        // Source image (and therefore the destination we created) have float
        // pixels, so just iterate and call the user's op.
        parallel_image(roi, nthreads, [&](ROI roi) {
            int nc = roi.nchannels();
            ImageBuf::ConstIterator<float> s(src, roi);
            ImageBuf::Iterator<float> d(result, roi);
            for (; !s.done(); ++s, ++d) {
                bool r = op(span<float>((float*)d.rawptr(),
                                        oiio_span_size_type(nc)),
                            cspan<float>((const float*)s.rawptr(),
                                         oiio_span_size_type(nc)));
                if (!r)
                    ok = false;
            }
        });
    } else {
        // Input has non-float images, so do conversions in and out.
        parallel_image(roi, nthreads, [&](ROI roi) {
            int nc      = roi.nchannels();
            float* vals = OIIO_ALLOCA(float, 2 * nc);
            span<float> spel(vals, nc);
            span<float> dpel(vals + nc, nc);
            ImageBuf::ConstIterator<float> s(src, roi);
            ImageBuf::Iterator<float> d(result, roi);
            for (; !d.done(); ++d, ++s) {
                s.store<float>(spel);
                bool r = op(dpel, spel);
                if (!r)
                    ok = false;
                d.load<float>(dpel);
            }
        });
    }
    if (!ok)
        result.errorfmt("Error in perpixel_op");
    return result;
}



ImageBuf
ImageBufAlgo::perpixel_op(
    const ImageBuf& srcA, const ImageBuf& srcB,
    function_view<bool(span<float>, cspan<float>, cspan<float>)> op,
    KWArgs options)
{
    using namespace ImageBufAlgo;
    ImageBuf result;
    ROI roi;
    if (!IBAprep(roi, result, { &srcA, &srcB }, options))
        return result;
    int nthreads = options.get_int("nthreads", 0);
    bool ok      = true;
    if (result.pixeltype() == TypeFloat && srcA.pixeltype() == TypeFloat
        && srcB.pixeltype() == TypeFloat) {
        // Source images (and therefore the destination we created) have float
        // pixels, so just iterate and call the user's op.
        parallel_image(roi, nthreads, [&](ROI roi) {
            int nc = roi.nchannels();
            ImageBuf::ConstIterator<float> sa(srcA, roi);
            ImageBuf::ConstIterator<float> sb(srcB, roi);
            ImageBuf::Iterator<float> d(result, roi);
            for (; !d.done(); ++sa, ++sb, ++d) {
                bool r = op(span<float>((float*)d.rawptr(),
                                        oiio_span_size_type(nc)),
                            cspan<float>((const float*)sa.rawptr(),
                                         oiio_span_size_type(nc)),
                            cspan<float>((const float*)sb.rawptr(),
                                         oiio_span_size_type(nc)));
                if (!r)
                    ok = false;
            }
        });
#if 0
    // EXPERIMENTAL: Do we get a speed gain specializing for other types?
    } else if (result.pixeltype() == TypeUInt8
               && srcA.pixeltype() == TypeUInt8
               && srcB.pixeltype() == TypeUInt8) {
        // Source images (and therefore the destination we created) have float
        // pixels, so just iterate and call the user's op.
        parallel_image(roi, nthreads, [&](ROI roi) {
            int nc      = roi.nchannels();
            float* vals = OIIO_ALLOCA(float, 3 * nc);
            span<float> sapel(vals, nc);
            span<float> sbpel(vals + nc, nc);
            span<float> dpel(vals + 2 * nc, nc);
            ImageBuf::ConstIterator<uint8_t> sa(srcA, roi);
            ImageBuf::ConstIterator<uint8_t> sb(srcB, roi);
            ImageBuf::Iterator<uint8_t> d(result, roi);
            for (; !d.done(); ++sa, ++sb, ++d) {
                for (int c = 0; c < nc; ++c)
                    sapel[c] = sa[c];
                for (int c = 0; c < nc; ++c)
                    sbpel[c] = sb[c];
                bool r = op(dpel, sapel, sbpel);
                if (!r)
                    ok = false;
                for (int c = 0; c < nc; ++c)
                    d[c] = dpel[c];
            }
        });
#endif
    } else {
        // Input has non-float images, so do conversions in and out.
        parallel_image(roi, nthreads, [&](ROI roi) {
            int nc      = roi.nchannels();
            float* vals = OIIO_ALLOCA(float, 3 * nc);
            span<float> sapel(vals, nc);
            span<float> sbpel(vals + nc, nc);
            span<float> dpel(vals + 2 * nc, nc);
            ImageBuf::ConstIterator<float> sa(srcA, roi);
            ImageBuf::ConstIterator<float> sb(srcB, roi);
            ImageBuf::Iterator<float> d(result, roi);
            for (; !d.done(); ++sa, ++sb, ++d) {
                sa.store<float>(sapel);
                sb.store<float>(sbpel);
                bool r = op(dpel, sapel, sbpel);
                if (!r)
                    ok = false;
                d.load<float>(dpel);
            }
        });
    }
    if (!ok)
        result.errorfmt("Error in perpixel_op");
    return result;
}



template<typename DSTTYPE, typename SRCTYPE>
static bool
convolve_(ImageBuf& dst, const ImageBuf& src, const ImageBuf& kernel,
          bool normalize, ROI roi, int nthreads)
{
    using namespace ImageBufAlgo;
    OIIO_DASSERT(kernel.spec().format == TypeDesc::FLOAT && kernel.localpixels()
                 && "kernel should be float and in local memory");
    parallel_image(roi, nthreads, [&](ROI roi) {
        ROI kroi   = kernel.roi();
        int kchans = kernel.nchannels();

        float scale = 1.0f;
        if (normalize) {
            scale = 0.0f;
            for (ImageBuf::ConstIterator<float> k(kernel); !k.done(); ++k)
                scale += k[0];
            scale = 1.0f / scale;
        }
        float* sum = OIIO_ALLOCA(float, roi.chend);

        ImageBuf::Iterator<DSTTYPE> d(dst, roi);
        ImageBuf::ConstIterator<SRCTYPE> s(src, roi, ImageBuf::WrapClamp);
        for (; !d.done(); ++d) {
            for (int c = roi.chbegin; c < roi.chend; ++c)
                sum[c] = 0.0f;
            const float* k = (const float*)kernel.localpixels();
            s.rerange(d.x() + kroi.xbegin, d.x() + kroi.xend,
                      d.y() + kroi.ybegin, d.y() + kroi.yend,
                      d.z() + kroi.zbegin, d.z() + kroi.zend,
                      ImageBuf::WrapClamp);
            for (; !s.done(); ++s, k += kchans) {
                for (int c = roi.chbegin; c < roi.chend; ++c)
                    sum[c] += k[0] * s[c];
            }
            for (int c = roi.chbegin; c < roi.chend; ++c)
                d[c] = scale * sum[c];
        }
    });
    return true;
}



bool
ImageBufAlgo::convolve(ImageBuf& dst, const ImageBuf& src,
                       const ImageBuf& kernel, bool normalize, ROI roi,
                       int nthreads)
{
    pvt::LoggedTimer logtime("IBA::convolve");
    if (!IBAprep(roi, &dst, &src, IBAprep_REQUIRE_SAME_NCHANNELS))
        return false;
    bool ok;
    // Ensure that the kernel is float and in local memory
    const ImageBuf* K = &kernel;
    ImageBuf Ktmp;
    if (kernel.spec().format != TypeDesc::FLOAT || !kernel.localpixels()) {
        Ktmp.copy(kernel, TypeDesc::FLOAT);
        K = &Ktmp;
    }
    OIIO_DISPATCH_COMMON_TYPES2(ok, "convolve", convolve_, dst.spec().format,
                                src.spec().format, dst, src, *K, normalize, roi,
                                nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::convolve(const ImageBuf& src, const ImageBuf& kernel,
                       bool normalize, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = convolve(result, src, kernel, normalize, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::convolve() error");
    return result;
}



inline float
binomial(int n, int k)
{
    float p = 1;
    for (int i = 1; i <= k; ++i)
        p *= float(n - (k - i)) / i;
    return p;
}


ImageBuf
ImageBufAlgo::make_kernel(string_view name, float width, float height,
                          float depth, bool normalize)
{
    auto filter = Filter2D::create_shared(name, width, height);
    if (filter) {
        if (width == 0.0f)
            width = filter->width();
        if (height == 0.0f)
            height = filter->height();
    } else {
        if (width == 0.0f)
            width = 4.0f;
        if (height == 0.0f)
            height = 4.0f;
    }
    int w = std::max(1, (int)ceilf(width));
    int h = std::max(1, (int)ceilf(height));
    int d = std::max(1, (int)ceilf(depth));
    // Round up size to odd
    w |= 1;
    h |= 1;
    d |= 1;
    ImageSpec spec(w, h, 1 /*channels*/, TypeDesc::FLOAT);
    spec.depth       = d;
    spec.x           = -w / 2;
    spec.y           = -h / 2;
    spec.z           = -d / 2;
    spec.full_x      = spec.x;
    spec.full_y      = spec.y;
    spec.full_z      = spec.z;
    spec.full_width  = spec.width;
    spec.full_height = spec.height;
    spec.full_depth  = spec.depth;
    ImageBuf dst(spec);

    if (filter) {
        // Named continuous filter from filter.h
        for (ImageBuf::Iterator<float> p(dst); !p.done(); ++p)
            p[0] = (*filter)((float)p.x(), (float)p.y());
    } else if (name == "binomial") {
        // Binomial filter
        float* wfilter = OIIO_ALLOCA(float, width);
        for (int i = 0; i < width; ++i)
            wfilter[i] = binomial(width - 1, i);
        float* hfilter = (height == width) ? wfilter
                                           : OIIO_ALLOCA(float, height);
        if (height != width)
            for (int i = 0; i < height; ++i)
                hfilter[i] = binomial(height - 1, i);
        float* dfilter = OIIO_ALLOCA(float, depth);
        if (depth == 1)
            dfilter[0] = 1;
        else
            for (int i = 0; i < depth; ++i)
                dfilter[i] = binomial(depth - 1, i);
        for (ImageBuf::Iterator<float> p(dst); !p.done(); ++p)
            p[0] = wfilter[p.x() - spec.x] * hfilter[p.y() - spec.y]
                   * dfilter[p.z() - spec.z];
    } else if (Strutil::iequals(name, "laplacian") && w == 3 && h == 3
               && d == 1) {
        const float vals[9] = { 0, 1, 0, 1, -4, 1, 0, 1, 0 };
        dst.set_pixels(dst.roi(), make_cspan(vals), sizeof(float),
                       h * sizeof(float));
        normalize = false;  // sums to zero, so don't normalize it */
    } else {
        // No filter -- make a box
        float val = normalize ? 1.0f / ((w * h * d)) : 1.0f;
        for (ImageBuf::Iterator<float> p(dst); !p.done(); ++p)
            p[0] = val;
        dst.errorfmt("Unknown kernel \"{}\" {}x{}", name, width, height);
        return dst;
    }
    if (normalize) {
        float sum = 0;
        for (ImageBuf::Iterator<float> p(dst); !p.done(); ++p)
            sum += p[0];
        if (sum != 0.0f) /* don't normalize a 0-sum kernel */
            for (ImageBuf::Iterator<float> p(dst); !p.done(); ++p)
                p[0] = p[0] / sum;
    }
    return dst;
}



template<class Rtype>
static bool
unsharp_impl(ImageBuf& dst, const ImageBuf& blr, const ImageBuf& src,
             const float contrast, const float threshold, ROI roi, int nthreads)
{
    OIIO_DASSERT(dst.spec().nchannels == src.spec().nchannels
                 && dst.spec().nchannels == blr.spec().nchannels);

    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::ConstIterator<Rtype> s(src, roi);
        ImageBuf::ConstIterator<float> b(blr, roi);
        for (ImageBuf::Iterator<Rtype> d(dst, roi); !d.done(); ++s, ++d, ++b) {
            for (int c = roi.chbegin; c < roi.chend; ++c) {
                const float diff     = s[c] - b[c];
                const float abs_diff = fabsf(diff);
                if (abs_diff > threshold) {
                    d[c] = s[c] + contrast * diff;
                } else {
                    d[c] = s[c];
                }
            }
        }
    });
    return true;
}



bool
ImageBufAlgo::unsharp_mask(ImageBuf& dst, const ImageBuf& src,
                           string_view kernel, float width, float contrast,
                           float threshold, ROI roi, int nthreads)
{
    // N.B. Don't log time, it will get caught by the constituent parts
    if (!IBAprep(roi, &dst, &src,
                 IBAprep_REQUIRE_SAME_NCHANNELS | IBAprep_NO_SUPPORT_VOLUME))
        return false;

    // Blur the source image, store in Blurry
    ImageSpec BlurrySpec = src.spec();
    BlurrySpec.set_format(TypeDesc::FLOAT);  // force float
    ImageBuf fst_pass(BlurrySpec);
    ImageBuf Blurry(BlurrySpec);

    if (kernel == "median") {
        median_filter(Blurry, src, ceilf(width), 0, roi, nthreads);
    } else if (width > 3.0) {
        ImageBuf K  = make_kernel(kernel, 1, width);
        ImageBuf Kt = ImageBufAlgo::transpose(K);
        if (K.has_error()) {
            dst.errorfmt("{}", K.geterror());
            return false;
        }
        if (!convolve(fst_pass, src, K, true, roi, nthreads)) {
            dst.errorfmt("{}", fst_pass.geterror());
            return false;
        }
        if (!convolve(Blurry, fst_pass, Kt, true, roi, nthreads)) {
            dst.errorfmt("{}", Blurry.geterror());
            return false;
        }
    } else {
        ImageBuf K = make_kernel(kernel, width, width);
        if (K.has_error()) {
            dst.errorfmt("{}", K.geterror());
            return false;
        }
        if (!convolve(Blurry, src, K, true, roi, nthreads)) {
            dst.errorfmt("{}", Blurry.geterror());
            return false;
        }
    }

    bool ok;
    OIIO_DISPATCH_COMMON_TYPES(ok, "unsharp_mask", unsharp_impl,
                               dst.spec().format, dst, Blurry, src, contrast,
                               threshold, roi, nthreads);

    return ok;
}



ImageBuf
ImageBufAlgo::unsharp_mask(const ImageBuf& src, string_view kernel, float width,
                           float contrast, float threshold, ROI roi,
                           int nthreads)
{
    ImageBuf result;
    bool ok = unsharp_mask(result, src, kernel, width, contrast, threshold, roi,
                           nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::unsharp_mask() error");
    return result;
}



bool
ImageBufAlgo::laplacian(ImageBuf& dst, const ImageBuf& src, ROI roi,
                        int nthreads)
{
    // N.B.: Don't log time, convolve will catch it
    if (!IBAprep(roi, &dst, &src,
                 IBAprep_REQUIRE_SAME_NCHANNELS | IBAprep_NO_SUPPORT_VOLUME))
        return false;

    ImageBuf K = make_kernel("laplacian", 3, 3);
    if (K.has_error()) {
        dst.errorfmt("{}", K.geterror());
        return false;
    }
    // K.write ("K.exr");
    bool ok = convolve(dst, src, K, false, roi, nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::laplacian(const ImageBuf& src, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = laplacian(result, src, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::laplacian() error");
    return result;
}



template<class Rtype, class Atype>
static bool
median_filter_impl(ImageBuf& R, const ImageBuf& A, int width, int height,
                   ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        if (width < 1)
            width = 1;
        if (height < 1)
            height = width;
        int w_2        = std::max(1, width / 2);
        int h_2        = std::max(1, height / 2);
        int windowsize = width * height;
        int nchannels  = R.nchannels();
        float** chans  = OIIO_ALLOCA(float*, nchannels);
        for (int c = 0; c < nchannels; ++c)
            chans[c] = OIIO_ALLOCA(float, windowsize);

        ImageBuf::ConstIterator<Atype> a(A, roi);
        for (ImageBuf::Iterator<Rtype> r(R, roi); !r.done(); ++r) {
            a.rerange(r.x() - w_2, r.x() - w_2 + width, r.y() - h_2,
                      r.y() - h_2 + height, r.z(), r.z() + 1,
                      ImageBuf::WrapClamp);
            int n = 0;
            for (; !a.done(); ++a) {
                if (a.exists()) {
                    for (int c = 0; c < nchannels; ++c)
                        chans[c][n] = a[c];
                    ++n;
                }
            }
            if (n) {
                int mid = n / 2;
                for (int c = 0; c < nchannels; ++c) {
                    std::sort(chans[c] + 0, chans[c] + n);
                    r[c] = chans[c][mid];
                }
            } else {
                for (int c = 0; c < nchannels; ++c)
                    r[c] = 0.0f;
            }
        }
    });
    return true;
}



bool
ImageBufAlgo::median_filter(ImageBuf& dst, const ImageBuf& src, int width,
                            int height, ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::median_filter");
    if (!IBAprep(roi, &dst, &src,
                 IBAprep_REQUIRE_SAME_NCHANNELS | IBAprep_NO_SUPPORT_VOLUME))
        return false;

    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "median_filter", median_filter_impl,
                                dst.spec().format, src.spec().format, dst, src,
                                width, height, roi, nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::median_filter(const ImageBuf& src, int width, int height, ROI roi,
                            int nthreads)
{
    ImageBuf result;
    bool ok = median_filter(result, src, width, height, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::median_filter() error");
    return result;
}



enum MorphOp { MorphDilate, MorphErode };

template<class Rtype, class Atype>
static bool
morph_impl(ImageBuf& R, const ImageBuf& A, int width, int height, MorphOp op,
           ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        if (width < 1)
            width = 1;
        if (height < 1)
            height = width;
        int w_2       = std::max(1, width / 2);
        int h_2       = std::max(1, height / 2);
        int nchannels = R.nchannels();
        float* vals   = OIIO_ALLOCA(float, nchannels);
        ImageBuf::ConstIterator<Atype> a(A, roi);
        for (ImageBuf::Iterator<Rtype> r(R, roi); !r.done(); ++r) {
            a.rerange(r.x() - w_2, r.x() - w_2 + width, r.y() - h_2,
                      r.y() - h_2 + height, r.z(), r.z() + 1,
                      ImageBuf::WrapClamp);
            if (op == MorphDilate) {
                for (int c = 0; c < nchannels; ++c)
                    vals[c] = -std::numeric_limits<float>::max();
                for (; !a.done(); ++a) {
                    if (a.exists()) {
                        for (int c = 0; c < nchannels; ++c)
                            vals[c] = std::max(vals[c], a[c]);
                    }
                }
            } else if (op == MorphErode) {
                for (int c = 0; c < nchannels; ++c)
                    vals[c] = std::numeric_limits<float>::max();
                for (; !a.done(); ++a) {
                    if (a.exists()) {
                        for (int c = 0; c < nchannels; ++c)
                            vals[c] = std::min(vals[c], a[c]);
                    }
                }
            } else {
                OIIO_ASSERT(0 && "Unknown morphological operator");
            }
            for (int c = 0; c < nchannels; ++c)
                r[c] = vals[c];
        }
    });
    return true;
}



bool
ImageBufAlgo::dilate(ImageBuf& dst, const ImageBuf& src, int width, int height,
                     ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::dilate");
    if (!IBAprep(roi, &dst, &src,
                 IBAprep_REQUIRE_SAME_NCHANNELS | IBAprep_NO_SUPPORT_VOLUME))
        return false;

    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "dilate", morph_impl, dst.spec().format,
                                src.spec().format, dst, src, width, height,
                                MorphDilate, roi, nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::dilate(const ImageBuf& src, int width, int height, ROI roi,
                     int nthreads)
{
    ImageBuf result;
    bool ok = dilate(result, src, width, height, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::dilate() error");
    return result;
}



bool
ImageBufAlgo::erode(ImageBuf& dst, const ImageBuf& src, int width, int height,
                    ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::erode");
    if (!IBAprep(roi, &dst, &src,
                 IBAprep_REQUIRE_SAME_NCHANNELS | IBAprep_NO_SUPPORT_VOLUME))
        return false;

    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "erode", morph_impl, dst.spec().format,
                                src.spec().format, dst, src, width, height,
                                MorphErode, roi, nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::erode(const ImageBuf& src, int width, int height, ROI roi,
                    int nthreads)
{
    ImageBuf result;
    bool ok = erode(result, src, width, height, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::erode() error");
    return result;
}



// Helper function: fft of the horizontal rows
static bool
hfft_(ImageBuf& dst, const ImageBuf& src, bool inverse, bool unitary, ROI roi,
      int nthreads)
{
    OIIO_ASSERT(dst.spec().format.basetype == TypeDesc::FLOAT
                && src.spec().format.basetype == TypeDesc::FLOAT
                && dst.spec().nchannels == 2 && src.spec().nchannels == 2
                && dst.roi() == src.roi()
                && (dst.storage() == ImageBuf::LOCALBUFFER
                    || dst.storage() == ImageBuf::APPBUFFER)
                && (src.storage() == ImageBuf::LOCALBUFFER
                    || src.storage() == ImageBuf::APPBUFFER));

    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        int width     = roi.width();
        float rescale = sqrtf(1.0f / width);
        kissfft<float> F(width, inverse);
        for (int z = roi.zbegin; z < roi.zend; ++z) {
            for (int y = roi.ybegin; y < roi.yend; ++y) {
                std::complex<float>*s, *d;
                s = (std::complex<float>*)src.pixeladdr(roi.xbegin, y, z);
                d = (std::complex<float>*)dst.pixeladdr(roi.xbegin, y, z);
                F.transform(s, d);
                if (unitary)
                    for (int x = 0; x < width; ++x)
                        d[x] *= rescale;
            }
        }
    });
    return true;
}



bool
ImageBufAlgo::fft(ImageBuf& dst, const ImageBuf& src, ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::fft");
    if (src.spec().depth > 1) {
        dst.errorfmt("ImageBufAlgo::fft does not support volume images");
        return false;
    }
    if (!roi.defined())
        roi = roi_union(get_roi(src.spec()), get_roi_full(src.spec()));
    roi.chend = roi.chbegin + 1;  // One channel only

    // Construct a spec that describes the result
    ImageSpec spec = src.spec();
    spec.width = spec.full_width = roi.width();
    spec.height = spec.full_height = roi.height();
    spec.depth = spec.full_depth = 1;
    spec.x = spec.full_x = 0;
    spec.y = spec.full_y = 0;
    spec.z = spec.full_z = 0;
    spec.set_format(TypeDesc::FLOAT);
    spec.channelformats.clear();
    spec.nchannels = 2;
    spec.channelnames.clear();
    spec.channelnames.emplace_back("real");
    spec.channelnames.emplace_back("imag");

    // And a spec that describes the transposed intermediate
    ImageSpec specT = spec;
    std::swap(specT.width, specT.height);
    std::swap(specT.full_width, specT.full_height);

    // Resize dst
    dst.reset(spec);

    // Copy src to a 2-channel (for "complex") float buffer
    ImageBuf A(spec);
    if (src.nchannels() < 2) {
        // If we're pasting fewer than 2 channels, zero out channel 1.
        ROI r     = roi;
        r.chbegin = 1;
        r.chend   = 2;
        zero(A, r);
    }
    if (!ImageBufAlgo::paste(A, 0, 0, 0, 0, src, roi, nthreads)) {
        dst.errorfmt("{}", A.geterror());
        return false;
    }

    // FFT the rows (into temp buffer B).
    ImageBuf B(spec);
    hfft_(B, A, false /*inverse*/, true /*unitary*/, get_roi(B.spec()),
          nthreads);

    // Transpose and shift back to A
    A.clear();
    ImageBufAlgo::transpose(A, B, ROI::All(), nthreads);

    // FFT what was originally the columns (back to B)
    B.reset(specT);
    hfft_(B, A, false /*inverse*/, true /*unitary*/, get_roi(A.spec()),
          nthreads);

    // Transpose again, into the dest
    ImageBufAlgo::transpose(dst, B, ROI::All(), nthreads);

    return true;
}



bool
ImageBufAlgo::ifft(ImageBuf& dst, const ImageBuf& src, ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::ifft");
    if (src.nchannels() != 2 || src.spec().format != TypeDesc::FLOAT) {
        dst.errorfmt("ifft can only be done on 2-channel float images");
        return false;
    }
    if (src.spec().depth > 1) {
        dst.errorfmt("ImageBufAlgo::ifft does not support volume images");
        return false;
    }

    if (!roi.defined())
        roi = roi_union(get_roi(src.spec()), get_roi_full(src.spec()));
    roi.chbegin = 0;
    roi.chend   = 2;

    // Construct a spec that describes the result
    ImageSpec spec = src.spec();
    spec.width = spec.full_width = roi.width();
    spec.height = spec.full_height = roi.height();
    spec.depth = spec.full_depth = 1;
    spec.x = spec.full_x = 0;
    spec.y = spec.full_y = 0;
    spec.z = spec.full_z = 0;
    spec.set_format(TypeDesc::FLOAT);
    spec.channelformats.clear();
    spec.nchannels = 2;
    spec.channelnames.clear();
    spec.channelnames.emplace_back("real");
    spec.channelnames.emplace_back("imag");

    // Inverse FFT the rows (into temp buffer B).
    ImageBuf B(spec);
    hfft_(B, src, true /*inverse*/, true /*unitary*/, get_roi(B.spec()),
          nthreads);

    // Transpose and shift back to A
    ImageBuf A;
    ImageBufAlgo::transpose(A, B, ROI::All(), nthreads);

    // Inverse FFT what was originally the columns (back to B)
    B.reset(A.spec());
    hfft_(B, A, true /*inverse*/, true /*unitary*/, get_roi(A.spec()),
          nthreads);

    // Transpose again, into the dst, in the process throw out the
    // imaginary part and go back to a single (real) channel.
    spec.nchannels = 1;
    spec.channelnames.clear();
    spec.channelnames.emplace_back("R");
    dst.reset(spec);
    ROI Broi   = get_roi(B.spec());
    Broi.chend = 1;
    ImageBufAlgo::transpose(dst, B, Broi, nthreads);

    return true;
}


ImageBuf
ImageBufAlgo::fft(const ImageBuf& src, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = fft(result, src, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::fft() error");
    return result;
}


ImageBuf
ImageBufAlgo::ifft(const ImageBuf& src, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = ifft(result, src, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::ifft() error");
    return result;
}



template<class Rtype, class Atype>
static bool
polar_to_complex_impl(ImageBuf& R, const ImageBuf& A, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::ConstIterator<Atype> a(A, roi);
        for (ImageBuf::Iterator<Rtype> r(R, roi); !r.done(); ++r, ++a) {
            float amp   = a[0];
            float phase = a[1];
            float sine, cosine;
            sincos(phase, &sine, &cosine);
            r[0] = amp * cosine;
            r[1] = amp * sine;
        }
    });
    return true;
}



template<class Rtype, class Atype>
static bool
complex_to_polar_impl(ImageBuf& R, const ImageBuf& A, ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        ImageBuf::ConstIterator<Atype> a(A, roi);
        for (ImageBuf::Iterator<Rtype> r(R, roi); !r.done(); ++r, ++a) {
            float real  = a[0];
            float imag  = a[1];
            float phase = std::atan2(imag, real);
            if (phase < 0.0f)
                phase += float(2.0 * M_PI);
            r[0] = hypotf(real, imag);
            r[1] = phase;
        }
    });
    return true;
}



bool
ImageBufAlgo::polar_to_complex(ImageBuf& dst, const ImageBuf& src, ROI roi,
                               int nthreads)
{
    pvt::LoggedTimer logtime("IBA::polar_to_complex");
    if (src.nchannels() != 2) {
        dst.errorfmt("polar_to_complex can only be done on 2-channel");
        return false;
    }

    if (!IBAprep(roi, &dst, &src))
        return false;
    if (dst.nchannels() != 2) {
        dst.errorfmt("polar_to_complex can only be done on 2-channel");
        return false;
    }
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "polar_to_complex", polar_to_complex_impl,
                                dst.spec().format, src.spec().format, dst, src,
                                roi, nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::polar_to_complex(const ImageBuf& src, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = polar_to_complex(result, src, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::polar_to_complex() error");
    return result;
}



bool
ImageBufAlgo::complex_to_polar(ImageBuf& dst, const ImageBuf& src, ROI roi,
                               int nthreads)
{
    pvt::LoggedTimer logtime("IBA::complex_to_polar");
    if (src.nchannels() != 2) {
        dst.errorfmt("complex_to_polar can only be done on 2-channel");
        return false;
    }

    if (!IBAprep(roi, &dst, &src))
        return false;
    if (dst.nchannels() != 2) {
        dst.errorfmt("complex_to_polar can only be done on 2-channel");
        return false;
    }
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "complex_to_polar", complex_to_polar_impl,
                                dst.spec().format, src.spec().format, dst, src,
                                roi, nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::complex_to_polar(const ImageBuf& src, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = complex_to_polar(result, src, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::complex_to_polar() error");
    return result;
}



// Helper for fillholes_pp: for any nonzero alpha pixels in dst, divide
// all components by alpha.
static bool
divide_by_alpha(ImageBuf& dst, ROI roi, int nthreads)
{
    OIIO_ASSERT(dst.spec().format == TypeDesc::FLOAT);
    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        const ImageSpec& spec(dst.spec());
        int nc = spec.nchannels;
        int ac = spec.alpha_channel;
        for (ImageBuf::Iterator<float> d(dst, roi); !d.done(); ++d) {
            float alpha = d[ac];
            if (alpha != 0.0f) {
                for (int c = 0; c < nc; ++c)
                    d[c] = d[c] / alpha;
            }
        }
    });
    return true;
}



bool
ImageBufAlgo::fillholes_pushpull(ImageBuf& dst, const ImageBuf& src, ROI roi,
                                 int nthreads)
{
    // N.B. Don't log time, it will be caught by the constituent parts
    const int req = (IBAprep_REQUIRE_SAME_NCHANNELS | IBAprep_REQUIRE_ALPHA
                     | IBAprep_NO_SUPPORT_VOLUME);
    if (!IBAprep(roi, &dst, &src, req))
        return false;
    // We generate a bunch of temp images to form an image pyramid.
    // These give us a place to stash them and make sure they are
    // auto-deleted when the function exits.
    std::vector<std::shared_ptr<ImageBuf>> pyramid;

    // First, make a writable copy of the original image (converting
    // to float as a convenience) as the top level of the pyramid.
    ImageSpec topspec = src.spec();
    topspec.set_format(TypeDesc::FLOAT);
    ImageBuf* top = new ImageBuf(topspec);
    paste(*top, topspec.x, topspec.y, topspec.z, 0, src);
    pyramid.emplace_back(top);

    // Construct the rest of the pyramid by successive x/2 resizing and
    // then dividing nonzero alpha pixels by their alpha (this "spreads
    // out" the defined part of the image).
    int w = src.spec().width, h = src.spec().height;
    while (w > 1 || h > 1) {
        w = std::max(1, w / 2);
        h = std::max(1, h / 2);
        ImageSpec smallspec(w, h, src.nchannels(), TypeDesc::FLOAT);
        smallspec.alpha_channel = topspec.alpha_channel;
        ImageBuf* small         = new ImageBuf(smallspec);
        ImageBufAlgo::resize(*small, *pyramid.back(),
                             { { "filtername", "triangle" } });
        divide_by_alpha(*small, get_roi(smallspec), nthreads);
        pyramid.emplace_back(small);
        // small->write(Strutil::fmt::format("push{:04d}.exr", small->spec().width));
    }

    // Now pull back up the pyramid by doing an alpha composite of level
    // i over a resized level i+1, thus filling in the alpha holes.  By
    // time we get to the top, pixels whose original alpha are
    // unchanged, those with alpha < 1 are replaced by the blended
    // colors of the higher pyramid levels.
    for (int i = (int)pyramid.size() - 2; i >= 0; --i) {
        ImageBuf &big(*pyramid[i]), &small(*pyramid[i + 1]);
        ImageBuf blowup(big.spec());
        ImageBufAlgo::resize(blowup, small, { { "filtername", "triangle" } });
        ImageBufAlgo::over(big, big, blowup);
        // big.write(Strutil::sprintf("pull{:04}.exr", big.spec().width));
    }

    // Now copy the completed base layer of the pyramid back to the
    // original requested output.
    paste(dst, src.spec().x, src.spec().y, src.spec().z, 0, *pyramid[0]);

    return true;
}



ImageBuf
ImageBufAlgo::fillholes_pushpull(const ImageBuf& src, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = fillholes_pushpull(result, src, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::fillholes_pushpull() error");
    return result;
}


OIIO_NAMESPACE_END
