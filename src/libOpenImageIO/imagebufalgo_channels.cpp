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
/// Implementation of ImageBufAlgo algorithms that merely move pixels
/// or channels between images without altering their values.


#include <OpenEXR/half.h>

#include <cmath>
#include <iostream>

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/deepdata.h>
#include <OpenImageIO/thread.h>



OIIO_NAMESPACE_BEGIN


template<typename DSTTYPE>
static bool
channels_ (ImageBuf &dst, const ImageBuf &src,
           const int *channelorder, const float *channelvalues,
           ROI roi, int nthreads=0)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        int nchannels = src.nchannels();
        ImageBuf::ConstIterator<DSTTYPE> s (src, roi);
        ImageBuf::Iterator<DSTTYPE> d (dst, roi);
        for (  ;  ! s.done();  ++s, ++d) {
            for (int c = roi.chbegin;  c < roi.chend;  ++c) {
                int cc = channelorder[c];
                if (cc >= 0 && cc < nchannels)
                    d[c] = s[cc];
                else if (channelvalues)
                    d[c] = channelvalues[c];
            }
        }
    });
    return true;
}



bool
ImageBufAlgo::channels (ImageBuf &dst, const ImageBuf &src,
                        int nchannels, const int *channelorder,
                        const float *channelvalues,
                        const std::string *newchannelnames,
                        bool shuffle_channel_names, int nthreads)
{
    // Not intended to create 0-channel images.
    if (nchannels <= 0) {
        dst.error ("%d-channel images not supported", nchannels);
        return false;
    }
    // If we dont have a single source channel,
    // hard to know how big to make the additional channels
    if (src.spec().nchannels == 0) {
        dst.error ("%d-channel images not supported", src.spec().nchannels);
        return false;
    }

    // If channelorder is NULL, it will be interpreted as
    // {0, 1, ..., nchannels-1}.
    int *local_channelorder = NULL;
    if (! channelorder) {
        local_channelorder = ALLOCA (int, nchannels);
        for (int c = 0;  c < nchannels;  ++c)
            local_channelorder[c] = c;
        channelorder = local_channelorder;
    }

    // If this is the identity transformation, just do a simple copy
    bool inorder = true;
    for (int c = 0;  c < nchannels;  ++c) {
        inorder &= (channelorder[c] == c);
        if (newchannelnames && newchannelnames[c].size() &&
                c < int(src.spec().channelnames.size()))
            inorder &= (newchannelnames[c] == src.spec().channelnames[c]);
    }
    if (nchannels == src.spec().nchannels && inorder) {
        return dst.copy (src);
    }

    // Construct a new ImageSpec that describes the desired channel ordering.
    ImageSpec newspec = src.spec();
    newspec.nchannels = nchannels;
    newspec.default_channel_names ();
    newspec.channelformats.clear();
    newspec.alpha_channel = -1;
    newspec.z_channel = -1;
    bool all_same_type = true;
    for (int c = 0; c < nchannels;  ++c) {
        int csrc = channelorder[c];
        // If the user gave an explicit name for this channel, use it...
        if (newchannelnames && newchannelnames[c].size())
            newspec.channelnames[c] = newchannelnames[c];
        // otherwise, if shuffle_channel_names, use the channel name of
        // the src channel we're using (otherwise stick to the default name)
        else if (shuffle_channel_names &&
                 csrc >= 0 && csrc < src.spec().nchannels)
            newspec.channelnames[c] = src.spec().channelnames[csrc];
        // otherwise, use the name of the source in that slot
        else if (csrc >= 0 && csrc < src.spec().nchannels) {
            newspec.channelnames[c] = src.spec().channelnames[csrc];
        }
        TypeDesc type = src.spec().channelformat(csrc);
        newspec.channelformats.push_back (type);
        if (type != newspec.channelformats.front())
            all_same_type = false;
        // Use the names (or designation of the src image, if
        // shuffle_channel_names is true) to deduce the alpha and z channels.
        if ((shuffle_channel_names && csrc == src.spec().alpha_channel) ||
              Strutil::iequals (newspec.channelnames[c], "A") ||
              Strutil::iequals (newspec.channelnames[c], "alpha"))
            newspec.alpha_channel = c;
        if ((shuffle_channel_names && csrc == src.spec().z_channel) ||
              Strutil::iequals (newspec.channelnames[c], "Z"))
            newspec.z_channel = c;
    }
    if (all_same_type)                      // clear per-chan formats if
        newspec.channelformats.clear();     // they're all the same

    // Update the image (realloc with the new spec)
    dst.reset (newspec);

    if (dst.deep()) {
        // Deep case:
        ASSERT (src.deep() && src.deepdata() && dst.deepdata());
        const DeepData &srcdata (*src.deepdata());
        DeepData &dstdata (*dst.deepdata());
        // The earlier dst.alloc() already called dstdata.init()
        for (int p = 0, npels = (int)newspec.image_pixels(); p < npels; ++p)
            dstdata.set_samples (p, srcdata.samples(p));
        for (int p = 0, npels = (int)newspec.image_pixels(); p < npels; ++p) {
            if (! dstdata.samples(p))
                continue;   // no samples for this pixel
            for (int c = 0;  c < newspec.nchannels;  ++c) {
                int csrc = channelorder[c];
                if (csrc < 0) {
                    // Replacing the channel with a new value
                    float val = channelvalues ? channelvalues[c] : 0.0f;
                    for (int s = 0, ns = dstdata.samples(p); s < ns; ++s)
                        dstdata.set_deep_value (p, c, s, val);
                } else {
                    if (dstdata.channeltype(c) == TypeDesc::UINT)
                        for (int s = 0, ns = dstdata.samples(p); s < ns; ++s)
                            dstdata.set_deep_value (p, c, s,
                                          srcdata.deep_value_uint(p,csrc,s));
                    else
                        for (int s = 0, ns = dstdata.samples(p); s < ns; ++s)
                            dstdata.set_deep_value (p, c, s,
                                          srcdata.deep_value(p,csrc,s));
                }
            }
        }
        return true;
    }
    // Below is the non-deep case

    bool ok;
    OIIO_DISPATCH_TYPES (ok, "channels", channels_,
                         dst.spec().format, dst, src,
                         channelorder, channelvalues, dst.roi(), nthreads);
    return ok;
}



template<class Rtype, class ABtype>
static bool
channel_append_impl (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                     ROI roi, int nthreads)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        int na = A.nchannels(), nb = B.nchannels();
        int n = std::min (dst.nchannels(), na+nb);
        ImageBuf::Iterator<Rtype> r (dst, roi);
        ImageBuf::ConstIterator<ABtype> a (A, roi);
        ImageBuf::ConstIterator<ABtype> b (B, roi);
        for (;  !r.done();  ++r, ++a, ++b) {
            for (int c = 0; c < n; ++c) {
                if (c < na)
                    r[c] = a.exists() ? a[c] : 0.0f;
                else
                    r[c] = b.exists() ? b[c-na] : 0.0f;
            }
        }
    });
    return true;
}


bool
ImageBufAlgo::channel_append (ImageBuf &dst, const ImageBuf &A,
                              const ImageBuf &B, ROI roi,
                              int nthreads)
{
    // If the region is not defined, set it to the union of the valid
    // regions of the two source images.
    if (! roi.defined())
        roi = roi_union (get_roi (A.spec()), get_roi (B.spec()));

    // If dst has not already been allocated, set it to the right size,
    // make it unconditinally float.
    if (! dst.pixels_valid()) {
        ImageSpec dstspec = A.spec();
        dstspec.set_format (TypeFloat);
        // Append the channel descriptions
        dstspec.nchannels = A.spec().nchannels + B.spec().nchannels;
        for (int c = 0;  c < B.spec().nchannels;  ++c) {
            std::string name = B.spec().channelnames[c];
            // It's a duplicate channel name. This will wreak havoc for
            // OpenEXR, so we need to choose a unique name.
            if (std::find(dstspec.channelnames.begin(), dstspec.channelnames.end(), name) != dstspec.channelnames.end()) {
                // First, let's see if the original image had a subimage
                // name and use that.
                std::string subname = B.spec().get_string_attribute("oiio:subimagename");
                if (subname.size())
                    name = subname + "." + name;
            }
            if (std::find(dstspec.channelnames.begin(), dstspec.channelnames.end(), name) != dstspec.channelnames.end()) {
                // If it's still a duplicate, fall back on a totally
                // artificial name that contains the channel number.
                name = Strutil::format ("channel%d", A.spec().nchannels+c);
            }
            dstspec.channelnames.push_back (name);
        }
        if (dstspec.alpha_channel < 0 && B.spec().alpha_channel >= 0)
            dstspec.alpha_channel = B.spec().alpha_channel + A.nchannels();
        if (dstspec.z_channel < 0 && B.spec().z_channel >= 0)
            dstspec.z_channel = B.spec().z_channel + A.nchannels();
        set_roi (dstspec, roi);
        dst.reset (dstspec);
    }

    // For now, only support A and B having the same type.
    if (A.spec().format != B.spec().format) {
        dst.error ("Unable to perform channel_append of %s, %s -> %s",
                   A.spec().format, B.spec().format, dst.spec().format);
        return false;
    }

    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "channel_append", channel_append_impl,
                          dst.spec().format, A.spec().format,
                          dst, A, B, roi, nthreads);
    return ok;
}



OIIO_NAMESPACE_END
