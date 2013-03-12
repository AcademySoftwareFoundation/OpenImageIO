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

/* This header has to be included before boost/regex.hpp header
   If it is included after, there is an error
   "undefined reference to CSHA1::Update (unsigned char const*, unsigned long)"
*/
#include "SHA1.h"

/// \file
/// Implementation of ImageBufAlgo algorithms.

#include <boost/version.hpp>
#include <boost/bind.hpp>

#include <OpenEXR/ImathFun.h>
#include <OpenEXR/half.h>

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "imagebuf.h"
#include "imagebufalgo.h"
#include "dassert.h"
#include "sysutil.h"
#include "filter.h"
#include "thread.h"
#include "filesystem.h"

#ifdef USE_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

#ifdef USE_OPENSSL
#include <openssl/sha.h>
#endif


OIIO_NAMESPACE_ENTER
{

namespace
{

template<typename T>
static inline bool
fill_ (ImageBuf &dst, const float *values, ROI roi=ROI())
{
    int chbegin = roi.chbegin;
    int chend = std::min (roi.chend, dst.nchannels());
    for (ImageBuf::Iterator<T> p (dst, roi);  !p.done();  ++p)
        for (int c = chbegin, i = 0;  c < chend;  ++c, ++i)
            p[c] = values[i];
    return true;
}

}

bool
ImageBufAlgo::fill (ImageBuf &dst, const float *pixel, ROI roi)
{
    ASSERT (pixel && "fill must have a non-NULL pixel value pointer");
    if (! roi.defined())
        roi = get_roi (dst.spec());
    OIIO_DISPATCH_TYPES ("fill", fill_, dst.spec().format, dst, pixel, roi);
    return true;
}


bool
ImageBufAlgo::zero (ImageBuf &dst, ROI roi)
{
    if (! roi.defined())
        roi = get_roi (dst.spec());
    int chans = std::min (dst.nchannels(), roi.nchannels());
    float *zero = ALLOCA(float,chans);
    memset (zero, 0, chans*sizeof(float));
    return fill (dst, zero, roi);
}



bool
ImageBufAlgo::checker (ImageBuf &dst,
                       int width,
                       const float *color1,
                       const float *color2,
                       int xbegin, int xend,
                       int ybegin, int yend,
                       int zbegin, int zend)
{
    for (int k = zbegin; k < zend; k++)
        for (int j = ybegin; j < yend; j++)
            for (int i = xbegin; i < xend; i++) {
                int p = (k-zbegin)/width + (j-ybegin)/width + (i-xbegin)/width;
                if (p & 1)
                    dst.setpixel (i, j, k, color2);
                else
                    dst.setpixel (i, j, k, color1);
            }
    return true;
}



namespace {

template<class T>
bool paste_ (ImageBuf &dst, int xbegin, int ybegin,
             int zbegin, int chbegin,
             const ImageBuf &src, ROI srcroi)
{
    const ImageSpec &dstspec (dst.spec());
    if (dstspec.format.basetype != TypeDesc::FLOAT) {
        dst.error ("paste: only 'float' destination images are supported");
        return false;
    }

    ImageBuf::ConstIterator<T,float> s (src, srcroi.xbegin, srcroi.xend,
                                        srcroi.ybegin, srcroi.yend,
                                        srcroi.zbegin, srcroi.zend);
    ImageBuf::Iterator<float,float> d (dst, xbegin, xbegin+srcroi.width(),
                                       ybegin, ybegin+srcroi.height(),
                                       zbegin, zbegin+srcroi.depth());
    int src_nchans = src.nchannels ();
    int dst_nchans = dst.nchannels ();
    for ( ;  ! s.done();  ++s, ++d) {
        if (! d.exists())
            continue;  // Skip paste-into pixels that don't overlap dst's data
        if (s.exists()) {
            for (int c = srcroi.chbegin, c_dst = chbegin;
                   c < srcroi.chend;  ++c, ++c_dst) {
                if (c_dst >= 0 && c_dst < dst_nchans)
                    d[c_dst] = c < src_nchans ? s[c] : 0.0f;
            }
        } else {
            // Copying from outside src's data -- black
            for (int c = srcroi.chbegin, c_dst = chbegin;
                   c < srcroi.chend;  ++c, ++c_dst) {
                if (c_dst >= 0 && c_dst < dst_nchans)
                    d[c_dst] = 0.0f;
            }
        }
    }
    return true;
}

}  // anon namespace



bool
ImageBufAlgo::paste (ImageBuf &dst, int xbegin, int ybegin,
                     int zbegin, int chbegin,
                     const ImageBuf &src, ROI srcroi)
{
    if (! srcroi.defined())
        srcroi = get_roi(src.spec());

    // If dst is uninitialized, size it like the region
    if (!dst.initialized()) {
        std::cerr << "Allocating space\n";
        ImageSpec dst_spec = src.spec();
        dst_spec.x = srcroi.xbegin;
        dst_spec.y = srcroi.ybegin;
        dst_spec.z = srcroi.zbegin;
        dst_spec.width = srcroi.width();
        dst_spec.height = srcroi.height();
        dst_spec.depth = srcroi.depth();
        dst_spec.nchannels = srcroi.nchannels();
        dst_spec.set_format (TypeDesc::FLOAT);
        dst.alloc (dst_spec);
    }

    // do the actual copying
    OIIO_DISPATCH_TYPES ("paste", paste_, src.spec().format,
                         dst, xbegin, ybegin, zbegin, chbegin, src, srcroi);
    return false;
}




namespace {

template<class T>
bool crop_ (ImageBuf &dst, const ImageBuf &src,
            int xbegin, int xend, int ybegin, int yend,
            const float *bordercolor)
{
    int nchans = dst.nchannels();
    T *border = ALLOCA (T, nchans);
    const ImageIOParameter *p = src.spec().find_attribute ("oiio:bordercolor");
    if (p && p->type().basetype == TypeDesc::FLOAT &&
        (int)p->type().numelements() >= nchans) {
        for (int c = 0;  c < nchans;  ++c)
            border[c] = convert_type<float,T>(((float *)p->data())[0]);
    } else {
        for (int c = 0;  c < nchans;  ++c)
            border[c] = T(0);
    }

    ImageBuf::Iterator<T,T> d (dst, xbegin, xend, ybegin, yend);
    ImageBuf::ConstIterator<T,T> s (src);
    for ( ;  ! d.done();  ++d) {
        s.pos (d.x(), d.y());
        if (s.valid()) {
            for (int c = 0;  c < nchans;  ++c)
                d[c] = s[c];
        } else {
            for (int c = 0;  c < nchans;  ++c)
                d[c] = border[c];
        }
    }
    return true;
}

}  // anon namespace



bool 
ImageBufAlgo::crop (ImageBuf &dst, const ImageBuf &src,
                    int xbegin, int xend, int ybegin, int yend,
                    const float *bordercolor)
{
    ImageSpec dst_spec = src.spec();
    dst_spec.x = xbegin;
    dst_spec.y = ybegin;
    dst_spec.width = xend-xbegin;
    dst_spec.height = yend-ybegin;
    
    // create new ImageBuffer
    if (!dst.pixels_valid())
        dst.alloc (dst_spec);

    OIIO_DISPATCH_TYPES ("crop", crop_, src.spec().format,
                         dst, src, xbegin, xend, ybegin, yend, bordercolor);
    return false;
}




bool
ImageBufAlgo::channels (ImageBuf &dst, const ImageBuf &src,
                        int nchannels, const int *channelorder,
                        bool shuffle_channel_names)
{
    // DEPRECATED -- just provide link compatibility
    return channels (dst, src, nchannels, channelorder, NULL, NULL,
                     shuffle_channel_names);
}



bool
ImageBufAlgo::channels (ImageBuf &dst, const ImageBuf &src,
                        int nchannels, const int *channelorder,
                        const float *channelvalues,
                        const std::string *newchannelnames,
                        bool shuffle_channel_names)
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
    for (int c = 0;  c < nchannels;   ++c)
        inorder &= (channelorder[c] == c);
    if (nchannels == src.spec().nchannels && inorder) {
        return dst.copy (src);
    }

    // Construct a new ImageSpec that describes the desired channel ordering.
    ImageSpec newspec = src.spec();
    newspec.nchannels = nchannels;
    newspec.default_channel_names ();
    newspec.alpha_channel = -1;
    newspec.z_channel = -1;
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
        else if (csrc >= 0 && csrc < src.spec().nchannels)
            newspec.channelnames[c] = src.spec().channelnames[c];
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

    // Update the image (realloc with the new spec)
    dst.alloc (newspec);

    // Copy the channels individually
    stride_t dstxstride = AutoStride, dstystride = AutoStride, dstzstride = AutoStride;
    ImageSpec::auto_stride (dstxstride, dstystride, dstzstride,
                            newspec.format.size(), newspec.nchannels,
                            newspec.width, newspec.height);
    int channelsize = newspec.format.size();
    char *pixels = (char *) dst.pixeladdr (dst.xbegin(), dst.ybegin(),
                                           dst.zbegin());
    for (int c = 0;  c < nchannels;  ++c) {
        // Copy shuffled channels
        if (channelorder[c] >= 0 && channelorder[c] < src.spec().nchannels) {
            int csrc = channelorder[c];
            src.get_pixel_channels (src.xbegin(), src.xend(),
                                    src.ybegin(), src.yend(),
                                    src.zbegin(), src.zend(),
                                    csrc, csrc+1, newspec.format, pixels,
                                    dstxstride, dstystride, dstzstride);
        }
        // Set channels that are literals
        if (channelorder[c] < 0 && channelvalues && channelvalues[c]) {
            ROI roi = get_roi (dst.spec());
            roi.chbegin = c;
            roi.chend = c+1;
            ImageBufAlgo::fill (dst, &channelvalues[c], roi);
        }
        pixels += channelsize;
    }
    return true;
}



bool
ImageBufAlgo::setNumChannels(ImageBuf &dst, const ImageBuf &src, int numChannels)
{
    // Not intended to create 0-channel images.
    if (numChannels <= 0) {
        dst.error ("%d-channel images not supported", numChannels);
        return false;
    }
    // If we dont have a single source channel,
    // hard to know how big to make the additional channels
    if (src.spec().nchannels == 0) {
        dst.error ("%d-channel images not supported", src.spec().nchannels);
        return false;
    }

    if (numChannels == src.spec().nchannels) {
        return dst.copy (src);
    }
    
    // Update the ImageSpec
    // (should this be moved to a helper function in the imagespec.h?
    ImageSpec dst_spec = src.spec();
    dst_spec.nchannels = numChannels;
    
    if (numChannels < src.spec().nchannels) {
        // Reduce the number of formats, and names, if needed
        if (static_cast<int>(dst_spec.channelformats.size()) == src.spec().nchannels)
            dst_spec.channelformats.resize(numChannels);
        if (static_cast<int>(dst_spec.channelnames.size()) == src.spec().nchannels)
            dst_spec.channelnames.resize(numChannels);
        
        if (dst_spec.alpha_channel < numChannels-1) {
            dst_spec.alpha_channel = -1;
        }
        if (dst_spec.z_channel < numChannels-1) {
            dst_spec.z_channel = -1;
        }
    } else {
        // Increase the number of formats, and names, if needed
        if (static_cast<int>(dst_spec.channelformats.size()) == src.spec().nchannels) {
            for (int c = dst_spec.channelnames.size();  c < numChannels;  ++c) {
                dst_spec.channelformats.push_back(dst_spec.format);
            }
        }
        if (static_cast<int>(dst_spec.channelnames.size()) == src.spec().nchannels) {
            for (int c = dst_spec.channelnames.size();  c < numChannels;  ++c) {
                dst_spec.channelnames.push_back (Strutil::format("channel%d", c));
            }
        }
    }
    
    // Update the image (realloc with the new spec)
    dst.alloc (dst_spec);
    
    std::vector<float> pixel(numChannels, 0.0f);
    
    // Walk though the data window. I.e., the crop window in a small image
    // or the overscanned area in a large image.
    for (int k = dst_spec.z; k < dst_spec.z+dst_spec.depth; k++) {
        for (int j = dst_spec.y; j < dst_spec.y+dst_spec.height; j++) {
            for (int i = dst_spec.x; i < dst_spec.x+dst_spec.width ; i++) {
                src.getpixel (i, j, k, &pixel[0]);
                dst.setpixel (i, j, k, &pixel[0]);
            }
        }
    }
    
    return true;
}




bool
ImageBufAlgo::add (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                   int options)
{
    // Sanity checks
    
    // dst must be distinct from A and B
    if ((const void *)&A == (const void *)&dst ||
        (const void *)&B == (const void *)&dst) {
        dst.error ("destination image must be distinct from source");
        return false;
    }
    
    // all three images must have the same number of channels
    if (A.spec().nchannels != B.spec().nchannels) {
        dst.error ("channel number mismatch: %d vs. %d", 
                   A.spec().nchannels, B.spec().nchannels);
        return false;
    }
    
    // If dst has not already been allocated, set it to the right size,
    // make it unconditinally float
    if (! dst.pixels_valid()) {
        ImageSpec dstspec = A.spec();
        dstspec.set_format (TypeDesc::TypeFloat);
        dst.alloc (dstspec);
    }
    // Clear dst pixels if instructed to do so
    if (options & ADD_CLEAR_DST) {
        zero (dst);
    }
      
    ASSERT (A.spec().format == TypeDesc::FLOAT &&
            B.spec().format == TypeDesc::FLOAT &&
            dst.spec().format == TypeDesc::FLOAT);
    
    ImageBuf::ConstIterator<float,float> a (A);
    ImageBuf::ConstIterator<float,float> b (B);
    ImageBuf::Iterator<float> d (dst);
    int nchannels = A.nchannels();
    // Loop over all pixels in A
    for ( ; a.valid();  ++a) {  
        // Point the iterators for B and dst to the corresponding pixel
        if (options & ADD_RETAIN_WINDOWS) {
            b.pos (a.x(), a.y());
        } else {
            // ADD_ALIGN_WINDOWS: make B line up with A
            b.pos (a.x()-A.xbegin()+B.xbegin(), a.y()-A.ybegin()+B.ybegin());
        }
        d.pos (a.x(), b.y());
        
        if (! b.valid() || ! d.valid())
            continue;   // Skip pixels that don't align
        
        // Add the pixel
        for (int c = 0;  c < nchannels;  ++c)
              d[c] = a[c] + b[c];
    }
    
    return true;
}



namespace {

template<class Rtype>
static bool
mul_impl (ImageBuf &R, const float *val, ROI roi, int nthreads)
{
    if (nthreads == 1 || roi.npixels() < 1000) {
        // For-sure single thread case
        ImageBuf::Iterator<Rtype> r (R, roi);
        for (ImageBuf::Iterator<Rtype> r (R, roi);  !r.done();  ++r)
            for (int c = roi.chbegin;  c < roi.chend;  ++c)
                r[c] = r[c] * val[c];
    } else {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (boost::bind(mul_impl<Rtype>,
                                                  boost::ref(R), val, _1, 1),
                                      roi, nthreads);
    }
    return true;
}


} // anon namespace


bool
ImageBufAlgo::mul (ImageBuf &R, const float *val, ROI roi, int nthreads)
{
    if (! roi.defined())
        roi = get_roi (R.spec());
    roi.chend = std::min (roi.chend, R.nchannels()); // clamp
    OIIO_DISPATCH_TYPES ("mul", mul_impl, R.spec().format,
                         R, val, roi, nthreads);
    return true;
}



bool
ImageBufAlgo::mul (ImageBuf &R, float val, ROI roi, int nthreads)
{
    int nc = R.nchannels();
    float *vals = ALLOCA (float, nc);
    for (int c = 0;  c < nc;  ++c)
        vals[c] = val;
    return mul (R, vals, roi, nthreads);
}



bool
ImageBufAlgo::computePixelStats (PixelStats &stats, const ImageBuf &src)
{
    int nchannels = src.spec().nchannels;
    if (nchannels == 0) {
        src.error ("%d-channel images not supported", nchannels);
        return false;
    }

    if (src.spec().format != TypeDesc::FLOAT && ! src.deep()) {
        src.error ("only 'float' images are supported");
        return false;
    }

    // Local storage to allow for intermediate representations which
    // are sometimes more precise than the final stats output.
    
    std::vector<float> min(nchannels);
    std::vector<float> max(nchannels);
    std::vector<long double> sum(nchannels);
    std::vector<long double> sum2(nchannels);
    std::vector<imagesize_t> nancount(nchannels);
    std::vector<imagesize_t> infcount(nchannels);
    std::vector<imagesize_t> finitecount(nchannels);
    
    // These tempsums are used as intermediate accumulation
    // variables, to allow for higher precision in the case
    // where the final sum is large, but we need to add together a
    // bunch of smaller values (that while individually small, sum
    // to a non-negligable value).
    //
    // Through experimentation, we have found that if you skip this
    // technique, in diabolical cases (gigapixel images, worst-case
    // dynamic range, compilers that don't support long doubles)
    // the precision for 'avg' is reduced to 1 part in 1e5.  This
    // will work around the issue.
    // 
    // This approach works best when the batch size is the sqrt of
    // numpixels, which makes the num batches roughly equal to the
    // number of pixels / batch.
    
    int PIXELS_PER_BATCH = std::max (1024,
            static_cast<int>(sqrt((double)src.spec().image_pixels())));
    
    std::vector<long double> tempsum(nchannels);
    std::vector<long double> tempsum2(nchannels);
    
    for (int i=0; i<nchannels; ++i) {
        min[i] = std::numeric_limits<float>::infinity();
        max[i] = -std::numeric_limits<float>::infinity();
        sum[i] = 0.0;
        sum2[i] = 0.0;
        tempsum[i] = 0.0;
        tempsum2[i] = 0.0;
        
        nancount[i] = 0;
        infcount[i] = 0;
        finitecount[i] = 0;
    }
    
    if (src.deep()) {
        // Loop over all pixels ...
        for (ImageBuf::ConstIterator<float> s(src); s.valid();  ++s) {
            int samples = s.deep_samples();
            if (! samples)
                continue;
            for (int c = 0;  c < nchannels;  ++c) {
                for (int i = 0;  i < samples;  ++i) {
                    float value = s.deep_value (c, i);
                    if (isnan (value)) {
                        ++nancount[c];
                        continue;
                    }
                    if (isinf (value)) {
                        ++infcount[c];
                        continue;
                    }
                    ++finitecount[c];
                    tempsum[c] += value;
                    tempsum2[c] += value*value;
                    min[c] = std::min (value, min[c]);
                    max[c] = std::max (value, max[c]);
                    if ((finitecount[c] % PIXELS_PER_BATCH) == 0) {
                        sum[c] += tempsum[c]; tempsum[c] = 0.0;
                        sum2[c] += tempsum2[c]; tempsum2[c] = 0.0;
                    }
                }
            }
        }
    } else {  // Non-deep case
        // Loop over all pixels ...
        for (ImageBuf::ConstIterator<float> s(src); s.valid();  ++s) {
            for (int c = 0;  c < nchannels;  ++c) {
                float value = s[c];
                if (isnan (value)) {
                    ++nancount[c];
                    continue;
                }
                if (isinf (value)) {
                    ++infcount[c];
                    continue;
                }
                ++finitecount[c];
                tempsum[c] += value;
                tempsum2[c] += value*value;
                min[c] = std::min (value, min[c]);
                max[c] = std::max (value, max[c]);
                if ((finitecount[c] % PIXELS_PER_BATCH) == 0) {
                    sum[c] += tempsum[c]; tempsum[c] = 0.0;
                    sum2[c] += tempsum2[c]; tempsum2[c] = 0.0;
                }
            }
        }
    }
    
    // Store results
    stats.min.resize (nchannels);
    stats.max.resize (nchannels);
    stats.avg.resize (nchannels);
    stats.stddev.resize (nchannels);
    stats.nancount.resize (nchannels);
    stats.infcount.resize (nchannels);
    stats.finitecount.resize (nchannels);
    
    for (int c = 0;  c < nchannels;  ++c) {
        if (finitecount[c] == 0) {
            stats.min[c] = 0.0;
            stats.max[c] = 0.0;
            stats.avg[c] = 0.0;
            stats.stddev[c] = 0.0;
        } else {
            // Add any residual tempsums into the final accumulation
            sum[c] += tempsum[c]; tempsum[c] = 0.0;
            sum2[c] += tempsum2[c]; tempsum2[c] = 0.0;
            
            double invCount = 1.0 / static_cast<double>(finitecount[c]);
            double davg = sum[c] * invCount;
            stats.min[c] = min[c];
            stats.max[c] = max[c];
            stats.avg[c] = static_cast<float>(davg);
            stats.stddev[c] = static_cast<float>(sqrt(sum2[c]*invCount - davg*davg));
        }
        
        stats.nancount[c] = nancount[c];
        stats.infcount[c] = infcount[c];
        stats.finitecount[c] = finitecount[c];
    }
    
    return true;
};



template<class BUFT>
inline void
compare_value (ImageBuf::ConstIterator<BUFT,float> &a, int chan,
               float aval, float bval, ImageBufAlgo::CompareResults &result,
               float &maxval, double &batcherror, double &batch_sqrerror,
               bool &failed, bool &warned, float failthresh, float warnthresh)
{
    maxval = std::max (maxval, std::max (aval, bval));
    double f = fabs (aval - bval);
    batcherror += f;
    batch_sqrerror += f*f;
    if (f > result.maxerror) {
        result.maxerror = f;
        result.maxx = a.x();
        result.maxy = a.y();
        result.maxz = 0;  // FIXME -- doesn't work for volume images
        result.maxc = chan;
    }
    if (! warned && f > warnthresh) {
        ++result.nwarn;
        warned = true;
    }
    if (! failed && f > failthresh) {
        ++result.nfail;
        failed = true;
    }
}



bool
ImageBufAlgo::compare (const ImageBuf &A, const ImageBuf &B,
                       float failthresh, float warnthresh,
                       ImageBufAlgo::CompareResults &result)
{
    if (A.spec().format != TypeDesc::FLOAT &&
        B.spec().format != TypeDesc::FLOAT) {
        A.error ("ImageBufAlgo::compare only works on 'float' images.");
        return false;
    }
    int npels = A.spec().width * A.spec().height * A.spec().depth;
    int nvals = npels * A.spec().nchannels;

    // Compare the two images.
    //
    double totalerror = 0;
    double totalsqrerror = 0;
    result.maxerror = 0;
    result.maxx=0, result.maxy=0, result.maxz=0, result.maxc=0;
    result.nfail = 0, result.nwarn = 0;
    float maxval = 1.0;  // max possible value
    ImageBuf::ConstIterator<float,float> a (A);
    ImageBuf::ConstIterator<float,float> b (B);
    bool deep = A.deep();
    if (B.deep() != A.deep())
        return false;
    // Break up into batches to reduce cancelation errors as the error
    // sums become too much larger than the error for individual pixels.
    const int batchsize = 4096;   // As good a guess as any
    for ( ;  a.valid();  ) {
        double batcherror = 0;
        double batch_sqrerror = 0;
        if (deep) {
            for (int i = 0;  i < batchsize && a.valid();  ++i, ++a) {
                b.pos (a.x(), a.y());  // ensure alignment
                bool warned = false, failed = false;  // For this pixel
                for (int c = 0;  c < A.spec().nchannels;  ++c)
                    for (int s = 0, e = a.deep_samples(); s < e;  ++s) {
                        compare_value (a, c, a.deep_value(c,s),
                                       b.deep_value(c,s), result, maxval,
                                       batcherror, batch_sqrerror,
                                       failed, warned, failthresh, warnthresh);
                    }
            }
        } else {  // non-deep
            for (int i = 0;  i < batchsize && a.valid();  ++i, ++a) {
                b.pos (a.x(), a.y());  // ensure alignment
                bool warned = false, failed = false;  // For this pixel
                for (int c = 0;  c < A.spec().nchannels;  ++c)
                    compare_value (a, c, a[c], b[c], result, maxval,
                                   batcherror, batch_sqrerror,
                                   failed, warned, failthresh, warnthresh);
            }
        }
        totalerror += batcherror;
        totalsqrerror += batch_sqrerror;
    }
    result.meanerror = totalerror / nvals;
    result.rms_error = sqrt (totalsqrerror / nvals);
    result.PSNR = 20.0 * log10 (maxval / result.rms_error);
    return result.nfail == 0;
}



namespace
{

template<typename T>
static inline bool
isConstantColor_ (const ImageBuf &src, float *color)
{
    int nchannels = src.nchannels();
    if (nchannels == 0)
        return true;
    
    // Iterate using the native typing (for speed).
    ImageBuf::ConstIterator<T,T> s (src);
    if (! s.valid())
        return true;

    // Store the first pixel
    std::vector<T> constval (nchannels);
    for (int c = 0;  c < nchannels;  ++c)
        constval[c] = s[c];

    // Loop over all pixels ...
    for ( ; s.valid ();  ++s) {
        for (int c = 0;  c < nchannels;  ++c)
            if (constval[c] != s[c])
                return false;
    }
    
    if (color)
        src.getpixel (src.xbegin(), src.ybegin(), src.zbegin(), color);
    return true;
}

}

bool
ImageBufAlgo::isConstantColor (const ImageBuf &src, float *color)
{
    OIIO_DISPATCH_TYPES ("isConstantColor", isConstantColor_,
                         src.spec().format, src, color);
};



template<typename T>
static inline bool
isConstantChannel_ (const ImageBuf &src, int channel, float val)
{
    if (channel < 0 || channel >= src.nchannels())
        return false;  // that channel doesn't exist in the image

    T v = convert_type<float,T> (val);
    for (ImageBuf::ConstIterator<T,T> s(src);  s.valid();  ++s)
        if (s[channel] != v)
            return false;
    return true;
}


bool
ImageBufAlgo::isConstantChannel (const ImageBuf &src, int channel, float val)
{
    OIIO_DISPATCH_TYPES ("isConstantChannel", isConstantChannel_,
                         src.spec().format, src, channel, val);
};

namespace
{

template<typename T>
static inline bool
isMonochrome_ (const ImageBuf &src, int dummy)
{
    int nchannels = src.nchannels();
    if (nchannels < 2) return true;
    
    // Loop over all pixels ...
    for (ImageBuf::ConstIterator<T,T> s(src);  s.valid();  ++s) {
        T constvalue = s[0];
        for (int c = 1;  c < nchannels;  ++c) {
            if (s[c] != constvalue) {
                return false;
            }
        }
    }
    
    return true;
}

}


bool
ImageBufAlgo::isMonochrome(const ImageBuf &src)
{
    OIIO_DISPATCH_TYPES ("isMonochrome", isMonochrome_, src.spec().format,
                         src, 0);
};



namespace {

std::string
simplePixelHashSHA1 (const ImageBuf &src,
                     const std::string & extrainfo, ROI roi)
{
    if (! roi.defined())
        roi = get_roi (src.spec());

    bool localpixels = src.localpixels();
    imagesize_t scanline_bytes = roi.width() * src.spec().pixel_bytes();
    ASSERT (scanline_bytes < std::numeric_limits<unsigned int>::max());
    // Do it a few scanlines at a time
    int chunk = std::max (1, int(16*1024*1024/scanline_bytes));

    std::vector<unsigned char> tmp;
    if (! localpixels)
        tmp.resize (chunk*scanline_bytes);

#ifdef USE_OPENSSL
    // If OpenSSL was available at build time, use its SHA-1
    // implementation, which is about 20% faster than CSHA1.
    SHA_CTX sha;
    SHA1_Init (&sha);

    for (int z = roi.zbegin, zend=roi.zend;  z < zend;  ++z) {
        for (int y = roi.ybegin, yend=roi.yend;  y < yend;  y += chunk) {
            int y1 = std::min (y+chunk, yend);
            if (localpixels) {
                SHA1_Update (&sha, src.pixeladdr (roi.xbegin, y, z),
                            (unsigned int) scanline_bytes*(y1-y));
            } else {
                src.get_pixels (roi.xbegin, roi.xend, y, y1, z, z+1,
                                src.spec().format, &tmp[0]);
                SHA1_Update (&sha, &tmp[0], (unsigned int) scanline_bytes*(y1-y));
            }
        }
    }
    
    // If extra info is specified, also include it in the sha computation
    if (!extrainfo.empty())
        SHA1_Update (&sha, extrainfo.c_str(), extrainfo.size());

    unsigned char md[SHA_DIGEST_LENGTH];
    char hash_digest[2*SHA_DIGEST_LENGTH+1];
    SHA1_Final (md, &sha);
    for (int i = 0;  i < SHA_DIGEST_LENGTH;  ++i)
        sprintf (hash_digest+2*i, "%02X", (int)md[i]);
    hash_digest[2*SHA_DIGEST_LENGTH] = 0;
    return std::string (hash_digest);
    
#else
    // Fall back on CSHA1 if OpenSSL was not available or if 
    CSHA1 sha;
    sha.Reset ();
    
    for (int z = roi.zbegin, zend=roi.zend;  z < zend;  ++z) {
        for (int y = roi.ybegin, yend=roi.yend;  y < yend;  y += chunk) {
            int y1 = std::min (y+chunk, yend);
            if (localpixels) {
                sha.Update ((const unsigned char *)src.pixeladdr (roi.xbegin, y, z),
                            (unsigned int) scanline_bytes*(y1-y));
            } else {
                src.get_pixels (roi.xbegin, roi.xend, y, y1, z, z+1,
                                src.spec().format, &tmp[0]);
                sha.Update (&tmp[0], (unsigned int) scanline_bytes*(y1-y));
            }
        }
    }
    
    // If extra info is specified, also include it in the sha computation
    if (!extrainfo.empty()) {
        sha.Update ((const unsigned char*) extrainfo.c_str(), extrainfo.size());
    }
    
    sha.Final ();
    std::string hash_digest;
    sha.ReportHashStl (hash_digest, CSHA1::REPORT_HEX_SHORT);

    return hash_digest;
#endif
}



// Wrapper to single-threadedly SHA1 hash a region in blocks and store
// the results in a designated place.
static void
sha1_hasher (const ImageBuf *src, ROI roi, int blocksize,
             std::string *results, int firstresult)
{
    ROI broi = roi;
    for (int y = roi.ybegin; y < roi.yend; y += blocksize) {
        broi.ybegin = y;
        broi.yend = std::min (y+blocksize, roi.yend);
        std::string s = simplePixelHashSHA1 (*src, "", broi);
        results[firstresult++] = s;
    }
}

} // anon namespace



std::string
ImageBufAlgo::computePixelHashSHA1 (const ImageBuf &src,
                                    const std::string & extrainfo,
                                    ROI roi, int blocksize, int nthreads)
{
    if (! roi.defined())
        roi = get_roi (src.spec());

    // Fall back to whole-image hash for only one block
    if (blocksize < 0 || blocksize >= roi.height())
        return simplePixelHashSHA1 (src, extrainfo, roi);

    // Request for 0 threads means "use the OIIO global thread count"
    if (nthreads <= 0)
        OIIO::getattribute ("threads", nthreads);

    int nblocks = (roi.height()+blocksize-1) / blocksize;
    std::vector<std::string> results (nblocks);
    if (nthreads <= 1) {
        sha1_hasher (&src, roi, blocksize, &results[0], 0);
    } else {
        // parallel case
        boost::thread_group threads;
        int blocks_per_thread = (nblocks+nthreads-1) / nthreads;
        ROI broi = roi;
        for (int b = 0, t = 0;  b < nblocks;  b += blocks_per_thread, ++t) {
            int y = roi.ybegin + b*blocksize;
            if (y >= roi.yend)
                break;
            broi.ybegin = y;
            broi.yend = std::min (y+blocksize*blocks_per_thread, roi.yend);
            threads.add_thread (new boost::thread (sha1_hasher, &src, broi,
                                                   blocksize, &results[0], b));
        }
        threads.join_all ();
    }

#ifdef USE_OPENSSL
    // If OpenSSL was available at build time, use its SHA-1
    // implementation, which is about 20% faster than CSHA1.
    SHA_CTX sha;
    SHA1_Init (&sha);
    for (int b = 0;  b < nblocks;  ++b)
        SHA1_Update (&sha, results[b].c_str(), results[b].size());
    if (extrainfo.size())
        SHA1_Update (&sha, extrainfo.c_str(), extrainfo.size());
    unsigned char md[SHA_DIGEST_LENGTH];
    char hash_digest[2*SHA_DIGEST_LENGTH+1];
    SHA1_Final (md, &sha);
    for (int i = 0;  i < SHA_DIGEST_LENGTH;  ++i)
        sprintf (hash_digest+2*i, "%02X", (int)md[i]);
    hash_digest[2*SHA_DIGEST_LENGTH] = 0;
    return std::string (hash_digest);
#else
    // Fall back on CSHA1 if OpenSSL was not available or if 
    CSHA1 sha;
    sha.Reset ();
    for (int b = 0;  b < nblocks;  ++b)
        sha.Update ((const unsigned char *)results[b].c_str(), results[b].size());
    if (extrainfo.size())
        sha.Update ((const unsigned char *)extrainfo.c_str(), extrainfo.size());
    sha.Final ();
    std::string hash_digest;
    sha.ReportHashStl (hash_digest, CSHA1::REPORT_HEX_SHORT);
    return hash_digest;
#endif
}




namespace { // anonymous namespace

template<typename SRCTYPE>
bool resize_ (ImageBuf &dst, const ImageBuf &src,
              int xbegin, int xend, int ybegin, int yend,
              Filter2D *filter)
{
    const ImageSpec &srcspec (src.spec());
    const ImageSpec &dstspec (dst.spec());
    int nchannels = dstspec.nchannels;

    if (dstspec.format.basetype != TypeDesc::FLOAT) {
        dst.error ("only 'float' images are supported");
        return false;
    }
    if (nchannels != srcspec.nchannels) {
        dst.error ("channel number mismatch: %d vs. %d", 
                   dst.spec().nchannels, src.spec().nchannels);
        return false;
    }

    bool allocfilter = (filter == NULL);
    if (allocfilter) {
        // If no filter was provided, punt and just linearly interpolate
        filter = Filter2D::create ("triangle", 2.0f, 2.0f);
    }

    // Local copies of the source image window, converted to float
    float srcfx = srcspec.full_x;
    float srcfy = srcspec.full_y;
    float srcfw = srcspec.full_width;
    float srcfh = srcspec.full_height;

    // Ratios of dst/src size.  Values larger than 1 indicate that we
    // are maximizing (enlarging the image), and thus want to smoothly
    // interpolate.  Values less than 1 indicate that we are minimizing
    // (shrinking the image), and thus want to properly filter out the
    // high frequencies.
    float xratio = float(dstspec.full_width) / srcfw; // 2 upsize, 0.5 downsize
    float yratio = float(dstspec.full_height) / srcfh;

    float dstpixelwidth = 1.0f / (float)dstspec.full_width;
    float dstpixelheight = 1.0f / (float)dstspec.full_height;
    float *pel = ALLOCA (float, nchannels);
    float filterrad = filter->width() / 2.0f;
    // radi,radj is the filter radius, as an integer, in source pixels.  We
    // will filter the source over [x-radi, x+radi] X [y-radj,y+radj].
    int radi = (int) ceilf (filterrad/xratio) + 1;
    int radj = (int) ceilf (filterrad/yratio) + 1;

#if 0
    std::cerr << "Resizing " << srcspec.full_width << "x" << srcspec.full_height
              << " to " << dstspec.full_width << "x" << dstspec.full_height << "\n";
    std::cerr << "ratios = " << xratio << ", " << yratio << "\n";
    std::cerr << "examining src filter support radius of " << radi << " x " << radj << " pixels\n";
    std::cerr << "dst range " << xbegin << ' ' << xend << " x " << ybegin << ' ' << yend << "\n";
#endif

    bool separable = filter->separable();
    float *column = NULL;
    if (separable) {
        // Allocate one column for the first horizontal filter pass
        column = ALLOCA (float, (2 * radj + 1) * nchannels);
    }

    for (int y = ybegin;  y < yend;  ++y) {
        // s,t are NDC space
        float t = (y+0.5f)*dstpixelheight;
        // src_xf, src_xf are image space float coordinates
        float src_yf = srcfy + t * srcfh - 0.5f;
        // src_x, src_y are image space integer coordinates of the floor
        int src_y;
        float src_yf_frac = floorfrac (src_yf, &src_y);
        for (int x = xbegin;  x < xend;  ++x) {
            float s = (x+0.5f)*dstpixelwidth;
            float src_xf = srcfx + s * srcfw - 0.5f;
            int src_x;
            float src_xf_frac = floorfrac (src_xf, &src_x);
            for (int c = 0;  c < nchannels;  ++c)
                pel[c] = 0.0f;
            float totalweight = 0.0f;
            if (separable) {
                // First, filter horizontally
                memset (column, 0, (2*radj+1)*nchannels*sizeof(float));
                float *p = column;
                for (int j = -radj;  j <= radj;  ++j, p += nchannels) {
                    totalweight = 0.0f;
                    int yclamped = Imath::clamp (src_y+j, src.ymin(), src.ymax());
                    ImageBuf::ConstIterator<SRCTYPE> srcpel (src, src_x-radi, src_x+radi+1,
                                                           yclamped, yclamped+1,
                                                           0, 1);
                    for (int i = -radi;  i <= radi;  ++i, ++srcpel) {
                        float w = filter->xfilt (xratio * (i-src_xf_frac));
                        if (w == 0.0f)
                            continue;
                        totalweight += w;
                        if (srcpel.exists()) {
                            for (int c = 0;  c < nchannels;  ++c)
                                p[c] += w * srcpel[c];
                        } else {
                            // Outside data window -- construct a clamped
                            // iterator for just that pixel
                            int xclamped = Imath::clamp (src_x+i, src.xmin(), src.xmax());
                            ImageBuf::ConstIterator<SRCTYPE> clamped = srcpel;
                            clamped.pos (xclamped, yclamped);
                            for (int c = 0;  c < nchannels;  ++c)
                                p[c] += w * clamped[c];
                        }
                    }
                    if (totalweight != 0.0f) {
                        for (int c = 0;  c < nchannels;  ++c)
                            p[c] /= totalweight;
                    }
                }
                // Now filter vertically
                totalweight = 0.0f;
                p = column;
                for (int j = -radj;  j <= radj;  ++j, p += nchannels) {
                    float w = filter->yfilt (yratio * (j-src_yf_frac));
                    totalweight += w;
                    for (int c = 0;  c < nchannels;  ++c)
                        pel[c] += w * p[c];
                }

            } else {
                // Non-separable
                ImageBuf::ConstIterator<SRCTYPE> srcpel (src, src_x-radi, src_x+radi+1,
                                                       src_y-radi, src_y+radi+1,
                                                       0, 1);
                for (int j = -radj;  j <= radj;  ++j) {
                    for (int i = -radi;  i <= radi;  ++i, ++srcpel) {
                        float w = (*filter)(xratio * (i-src_xf_frac),
                                            yratio * (j-src_yf_frac));
                        if (w == 0.0f)
                            continue;
                        totalweight += w;
                        DASSERT (! srcpel.done());
                        if (srcpel.exists()) {
                            for (int c = 0;  c < nchannels;  ++c)
                                pel[c] += w * srcpel[c];
                        } else {
                            // Outside data window -- construct a clamped
                            // iterator for just that pixel
                            ImageBuf::ConstIterator<SRCTYPE> clamped = srcpel;
                            clamped.pos (Imath::clamp (srcpel.x(), src.xmin(), src.xmax()),
                                         Imath::clamp (srcpel.y(), src.ymin(), src.ymax()));
                            for (int c = 0;  c < nchannels;  ++c)
                                pel[c] += w * clamped[c];
                        }
                    }
                }
                DASSERT (srcpel.done());
            }

            // Rescale pel to normalize the filter, then write it to the
            // image.
            if (totalweight == 0.0f) {
                // zero it out
                for (int c = 0;  c < nchannels;  ++c)
                    pel[c] = 0.0f;
            } else {
                for (int c = 0;  c < nchannels;  ++c)
                    pel[c] /= totalweight;
            }
            dst.setpixel (x, y, pel);
        }
    }

    if (allocfilter)
        Filter2D::destroy (filter);
    return true;
}

} // end anonymous namespace


bool
ImageBufAlgo::resize (ImageBuf &dst, const ImageBuf &src,
                      int xbegin, int xend, int ybegin, int yend,
                      Filter2D *filter)
{
    OIIO_DISPATCH_TYPES ("resize", resize_, src.spec().format,
                         dst, src, xbegin, xend, ybegin, yend, filter);
    return false;
}

namespace
{

// Make sure isfinite is defined for 'half'
inline bool isfinite (half h) { return h.isFinite(); }


template<typename SRCTYPE>
bool fixNonFinite_ (ImageBuf &dst, const ImageBuf &src,
                    ImageBufAlgo::NonFiniteFixMode mode,
                    int * pixelsFixed)
{
    if (mode == ImageBufAlgo::NONFINITE_NONE) {
        if (! dst.copy (src))
            return false;
        if (pixelsFixed) *pixelsFixed = 0;
        return true;
    }
    else if (mode == ImageBufAlgo::NONFINITE_BLACK) {
        // Replace non-finite pixels with black
        int count = 0;
        int nchannels = src.spec().nchannels;
        
        // Copy the input to the output
        if (! dst.copy (src))
            return false;
        
        ImageBuf::Iterator<SRCTYPE,SRCTYPE> pixel (dst);
        while (pixel.valid()) {
            bool fixed = false;
            for (int c = 0;  c < nchannels;  ++c) {
                SRCTYPE value = pixel[c];
                if (! isfinite(value)) {
                    (*pixel)[c] = 0.0;
                    fixed = true;
                }
            }
            
            if (fixed) ++count;
            ++pixel;
        }
        
        if (pixelsFixed) *pixelsFixed = count;
        return true;
    }
    else if (mode == ImageBufAlgo::NONFINITE_BOX3) {
        // Replace non-finite pixels with a simple 3x3 window average
        // (the average excluding non-finite pixels, of course)
        // 
        // Warning: There is an inherent bug in this approach when src == dst
        // As you progress across the image, the output buffer is also used
        // as the input so there will be a directionality preference in the filling
        // (I.e., updated values will be used only in the traversal direction).
        // One can visualize this by disabling the isfinite check.

        int count = 0;
        int nchannels = src.spec().nchannels;
        const int boxwidth = 1;
        
        // Copy the input to the output
        if (! dst.copy (src))
            return false;
        
        ImageBuf::Iterator<SRCTYPE,SRCTYPE> pixel (dst);
        
        while (pixel.valid()) {
            bool fixed = false;
            
            for (int c = 0;  c < nchannels;  ++c) {
                SRCTYPE value = pixel[c];
                if (! isfinite (value)) {
                    int numvals = 0;
                    SRCTYPE sum = 0.0;
                    
                    int top    = pixel.x() - boxwidth;
                    int bottom = pixel.x() + boxwidth;
                    int left   = pixel.y() - boxwidth;
                    int right  = pixel.y() + boxwidth;
                    
                    ImageBuf::Iterator<SRCTYPE,SRCTYPE> it (dst, top, bottom, left, right);
                    while (it.valid()) {
                        SRCTYPE v = it[c];
                        if (isfinite (v)) {
                            sum += v;
                            numvals ++;
                        }
                        ++it;
                    }
                    
                    if (numvals>0) {
                        (*pixel)[c] = sum/numvals;
                        fixed = true;
                    }
                    else {
                        (*pixel)[c] = 0.0;
                        fixed = true;
                    }
                }
            }
            
            if (fixed) ++count;
            ++pixel;
        }
        
        if (pixelsFixed) *pixelsFixed = count;
        return true;
    }
    
    return false;
}

} // anon namespace



/// Fix all non-finite pixels (nan/inf) using the specified approach
bool
ImageBufAlgo::fixNonFinite (ImageBuf &dst, const ImageBuf &src,
                            NonFiniteFixMode mode, int * pixelsFixed)
{
    switch (src.spec().format.basetype) {
    case TypeDesc::FLOAT :
        return fixNonFinite_<float> (dst, src, mode, pixelsFixed);
    case TypeDesc::HALF  :
        return fixNonFinite_<half> (dst, src, mode, pixelsFixed);
    case TypeDesc::DOUBLE:
        return fixNonFinite_<double> (dst, src, mode, pixelsFixed);
    default:
        break;
    }
    
    // Non-float images cannot have non-finite pixels,
    // so all we have to do is copy the image and return
    if (! dst.copy (src))
        return false;
    if (pixelsFixed) *pixelsFixed = 0;
    return true;
}



namespace {   // anonymous namespace

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
over_impl (ImageBuf &R, const ImageBuf &A, const ImageBuf &B, ROI roi,
           bool zcomp=false, bool z_zeroisinf=false)
{
    if (R.spec().format != BaseTypeFromC<Rtype>::value ||
        A.spec().format != BaseTypeFromC<Atype>::value ||
        B.spec().format != BaseTypeFromC<Btype>::value) {
        R.error ("Unsupported pixel data format combination '%s / %s / %s'",
                 R.spec().format, A.spec().format, B.spec().format);
        return false;   // double check that types match
    }

    // It's already guaranteed that R, A, and B have matching channel
    // ordering, and have an alpha channel.  So just decode one.
    int nchannels = 0, alpha_channel = 0, z_channel = 0, ncolor_channels = 0;
    decode_over_channels (R, nchannels, alpha_channel,
                          z_channel, ncolor_channels);
    bool has_z = (z_channel >= 0);

    ImageBuf::ConstIterator<Atype, float> a (A);
    ImageBuf::ConstIterator<Btype, float> b (B);
    ImageBuf::Iterator<Rtype, float> r (R, roi);
    for ( ; ! r.done(); r++) {
        a.pos (r.x(), r.y(), r.z());
        b.pos (r.x(), r.y(), r.z());

        if (! a.exists()) {
            if (! b.exists()) {
                // a and b outside their data window -- "empty" pixels
                for (int c = 0; c < nchannels; c++)
                    r[c] = 0.0f;
            } else {
                // a doesn't exist, but b does -- copy B
                for (int c = 0; c < nchannels; ++c)
                    r[c] = b[c];
            }
            continue;
        }

        if (! b.exists()) {
            // a exists, b does not -- copy A
            for (int c = 0; c < nchannels; ++c)
                r[c] = a[c];
            continue;
        }

        // At this point, a and b exist.
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
            for (int c = 0;  c < nchannels;  c++)
                r[c] = a[c] + one_minus_alpha * b[c];
            if (has_z)
                r[z_channel] = (alpha != 0.0) ? a[z_channel] : b[z_channel];
        } else {
            // B over A -- because we're doing a Z composite
            float alpha = clamp (b[alpha_channel], 0.0f, 1.0f);
            float one_minus_alpha = 1.0f - alpha;
            for (int c = 0;  c < nchannels;  c++)
                r[c] = b[c] + one_minus_alpha * a[c];
            r[z_channel] = (alpha != 0.0) ? b[z_channel] : a[z_channel];
        }
    }
    return true;
}

}    // anonymous namespace


bool
ImageBufAlgo::over (ImageBuf &R, const ImageBuf &A, const ImageBuf &B, ROI roi,
                    int nthreads)
{
    const ImageSpec &specR = R.spec();
    const ImageSpec &specA = A.spec();
    const ImageSpec &specB = B.spec();

    int nchannels_R, nchannels_A, nchannels_B;
    int alpha_R, alpha_A, alpha_B;
    int z_R, z_A, z_B;
    int colors_R, colors_A, colors_B;
    bool initialized_R = decode_over_channels (R, nchannels_R, alpha_R,
                                               z_R, colors_R);
    bool initialized_A = decode_over_channels (A, nchannels_A, alpha_A,
                                               z_A, colors_A);
    bool initialized_B = decode_over_channels (B, nchannels_B, alpha_B,
                                               z_B, colors_B);

    if (! initialized_A || ! initialized_B) {
        R.error ("Can't 'over' uninitialized images");
        return false;
    }

    // Fail if the input images don't have an alpha channel.
    if (alpha_A < 0 || alpha_B < 0 || (initialized_R && alpha_R < 0)) {
        R.error ("'over' requires alpha channels");
        return false;
    }
    // Fail for mismatched channel counts
    if (colors_A != colors_B || colors_A < 1) {
        R.error ("Can't 'over' images with mismatched color channel counts (%d vs %d)",
                 colors_A, colors_B);
        return false;
    }
    // Fail for unaligned alpha or z channels
    if (alpha_A != alpha_B || z_A != z_B ||
        (initialized_R && alpha_R != alpha_A) ||
        (initialized_R && z_R != z_A)) {
        R.error ("Can't 'over' images with mismatched channel order",
                 colors_A, colors_B);
        return false;
    }
    
    // At present, this operation only supports ImageBuf's containing
    // float pixel data.
    if ((initialized_R && specR.format != TypeDesc::TypeFloat) ||
        specA.format != TypeDesc::TypeFloat ||
        specB.format != TypeDesc::TypeFloat) {
        R.error ("Unsupported pixel data format combination '%s = %s over %s'",
                 specR.format, specA.format, specB.format);
        return false;
    }

    // Uninitialized R -> size it to the union of A and B.
    if (! initialized_R) {
        ImageSpec newspec = specA;
        set_roi (newspec, roi_union (get_roi(specA), get_roi(specB)));
        R.reset ("over", newspec);
    }

    // Specified ROI -> use it. Unspecified ROI -> initialize from R.
    if (! roi.defined())
        roi = get_roi (R.spec());

    parallel_image (boost::bind (over_impl<float,float,float>, boost::ref(R),
                                 boost::cref(A), boost::cref(B), _1, false, false),
                    roi, nthreads);
    return ! R.has_error();
}



bool
ImageBufAlgo::zover (ImageBuf &R, const ImageBuf &A, const ImageBuf &B,
                     bool z_zeroisinf, ROI roi, int nthreads)
{
    const ImageSpec &specR = R.spec();
    const ImageSpec &specA = A.spec();
    const ImageSpec &specB = B.spec();

    int nchannels_R, nchannels_A, nchannels_B;
    int alpha_R, alpha_A, alpha_B;
    int z_R, z_A, z_B;
    int colors_R, colors_A, colors_B;
    bool initialized_R = decode_over_channels (R, nchannels_R, alpha_R,
                                               z_R, colors_R);
    bool initialized_A = decode_over_channels (A, nchannels_A, alpha_A,
                                               z_A, colors_A);
    bool initialized_B = decode_over_channels (B, nchannels_B, alpha_B,
                                               z_B, colors_B);

    if (! initialized_A || ! initialized_B) {
        R.error ("Can't 'zover' uninitialized images");
        return false;
    }
    // Fail if the input images don't have a Z channel.
    if (z_A < 0 || z_B < 0 || (initialized_R && z_R < 0)) {
        R.error ("'zover' requires Z channels");
        return false;
    }
    // Fail if the input images don't have an alpha channel.
    if (alpha_A < 0 || alpha_B < 0 || (initialized_R && alpha_R < 0)) {
        R.error ("'zover' requires alpha channels");
        return false;
    }
    // Fail for mismatched channel counts
    if (colors_A != colors_B || colors_A < 1) {
        R.error ("Can't 'zover' images with mismatched color channel counts (%d vs %d)",
                 colors_A, colors_B);
        return false;
    }
    // Fail for unaligned alpha or z channels
    if (alpha_A != alpha_B || z_A != z_B ||
        (initialized_R && alpha_R != alpha_A) ||
        (initialized_R && z_R != z_A)) {
        R.error ("Can't 'zover' images with mismatched channel order",
                 colors_A, colors_B);
        return false;
    }
    
    // At present, this operation only supports ImageBuf's containing
    // float pixel data.
    if ((initialized_R && specR.format != TypeDesc::TypeFloat) ||
        specA.format != TypeDesc::TypeFloat ||
        specB.format != TypeDesc::TypeFloat) {
        R.error ("Unsupported pixel data format combination '%s = %s zover %s'",
                 specR.format, specA.format, specB.format);
        return false;
    }

    // Uninitialized R -> size it to the union of A and B.
    if (! initialized_R) {
        ImageSpec newspec = specA;
        set_roi (newspec, roi_union (get_roi(specA), get_roi(specB)));
        R.reset ("zover", newspec);
    }

    // Specified ROI -> use it. Unspecified ROI -> initialize from R.
    if (! roi.defined())
        roi = get_roi (R.spec());

    parallel_image (boost::bind (over_impl<float,float,float>, boost::ref(R),
                                 boost::cref(A), boost::cref(B), _1,
                                 true, z_zeroisinf),
                    roi, nthreads);
    return ! R.has_error();
}



bool
ImageBufAlgo::zover (ImageBuf &R, const ImageBuf &A, const ImageBuf &B,
                     ROI roi, int nthreads)
{
    // DEPRECATED version -- just call the new version.  This exists to 
    // avoid breaking link compatibility.  Eventually remove it at the
    // next major release.
    return zover (R, A, B, false, roi, nthreads);
}



#ifdef USE_FREETYPE
namespace { // anon
static mutex ft_mutex;
static FT_Library ft_library = NULL;
static bool ft_broken = false;
#if defined(__linux__) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
const char *default_font_name = "cour";
#elif defined (__APPLE__)
const char *default_font_name = "Courier New";
#elif defined (_WIN32)
const char *default_font_name = "Courier";
#else
const char *default_font_name = "cour";
#endif
} // anon namespace
#endif


bool
ImageBufAlgo::render_text (ImageBuf &R, int x, int y, const std::string &text,
                           int fontsize, const std::string &font_,
                           const float *textcolor)
{
#ifdef USE_FREETYPE
    // If we know FT is broken, don't bother trying again
    if (ft_broken)
        return false;

    // Thread safety
    lock_guard ft_lock (ft_mutex);
    int error = 0;

    // If FT not yet initialized, do it now.
    if (! ft_library) {
        error = FT_Init_FreeType (&ft_library);
        if (error) {
            ft_broken = true;
            R.error ("Could not initialize FreeType for font rendering");
            return false;
        }
    }

    // A set of likely directories for fonts to live, across several systems.
    std::vector<std::string> search_dirs;
    const char *home = getenv ("HOME");
    if (home && *home) {
        std::string h (home);
        search_dirs.push_back (h + "/fonts");
        search_dirs.push_back (h + "/Fonts");
        search_dirs.push_back (h + "/Library/Fonts");
    }
    search_dirs.push_back ("/usr/share/fonts");
    search_dirs.push_back ("/Library/Fonts");
    search_dirs.push_back ("C:/Windows/Fonts");
    search_dirs.push_back ("/opt/local/share/fonts");

    // Try to find the font.  Experiment with several extensions
    std::string font = font_;
    if (font.empty())
        font = default_font_name;
    if (! Filesystem::is_regular (font)) {
        // Font specified is not a full path
        std::string f;
        static const char *extensions[] = { "", ".ttf", ".pfa", ".pfb", NULL };
        for (int i = 0;  f.empty() && extensions[i];  ++i)
            f = Filesystem::searchpath_find (font+extensions[i],
                                             search_dirs, true, true);
        if (! f.empty())
            font = f;
    }

    FT_Face face;      // handle to face object
    error = FT_New_Face (ft_library, font.c_str(), 0 /* face index */, &face);
    if (error) {
        R.error ("Could not set font face to \"%s\"", font);
        return false;  // couldn't open the face
    }

    error = FT_Set_Pixel_Sizes (face,        // handle to face object
                                0,           // pixel_width
                                fontsize);   // pixel_heigh
    if (error) {
        FT_Done_Face (face);
        R.error ("Could not set font size to %d", fontsize);
        return false;  // couldn't set the character size
    }

    FT_GlyphSlot slot = face->glyph;  // a small shortcut
    int nchannels = R.spec().nchannels;
    float *pixelcolor = ALLOCA (float, nchannels);
    if (! textcolor) {
        float *localtextcolor = ALLOCA (float, nchannels);
        for (int c = 0;  c < nchannels;  ++c)
            localtextcolor[c] = 1.0f;
        textcolor = localtextcolor;
    }

    for (size_t n = 0, e = text.size();  n < e;  ++n) {
        // load glyph image into the slot (erase previous one)
        error = FT_Load_Char (face, text[n], FT_LOAD_RENDER);
        if (error)
            continue;  // ignore errors
        // now, draw to our target surface
        for (int j = 0;  j < slot->bitmap.rows; ++j) {
            int ry = y + j - slot->bitmap_top;
            for (int i = 0;  i < slot->bitmap.width; ++i) {
                int rx = x + i + slot->bitmap_left;
                float b = slot->bitmap.buffer[slot->bitmap.pitch*j+i] / 255.0f;
                R.getpixel (rx, ry, pixelcolor);
                for (int c = 0;  c < nchannels;  ++c)
                    pixelcolor[c] = b*textcolor[c] + (1.0f-b) * pixelcolor[c];
                R.setpixel (rx, ry, pixelcolor);
            }
        }
        // increment pen position
        x += slot->advance.x >> 6;
    }

    FT_Done_Face (face);
    return true;

#else
    R.error ("OpenImageIO was not compiled with FreeType for font rendering");
    return false;   // Font rendering not supported
#endif
}


namespace { // anonymous namespace

/// histogram_impl -----------------------------------------------------------
/// Fully type-specialized version of histogram.
///
/// Pixel values in min->max range are mapped to 0->(bins-1) range, so that
/// each value is placed in the appropriate bin. The formula used is:
/// y = (x-min) * bins/(max-min), where y is the value in the 0->(bins-1)
/// range and x is the value in the min->max range. There is one special
/// case x==max for which the formula is not used and x is assigned to the
/// last bin at position (bins-1) in the vector histogram.
/// --------------------------------------------------------------------------
template<class Atype>
bool
histogram_impl (const ImageBuf &A, int channel,
                std::vector<imagesize_t> &histogram, int bins,
                float min, float max, imagesize_t *submin,
                imagesize_t *supermax, ROI roi)
{
    // Double check A's type.
    if (A.spec().format != BaseTypeFromC<Atype>::value) {
        A.error ("Unsupported pixel data format '%s'", A.spec().format);
        return false;
    }

    // Initialize.
    ImageBuf::ConstIterator<Atype, float> a (A, roi);
    float ratio = bins / (max-min);
    int bins_minus_1 = bins-1;
    bool submin_ok = submin != NULL;
    bool supermax_ok = supermax != NULL;
    if (submin_ok)
        *submin = 0;
    if (supermax_ok)
        *supermax = 0;
    histogram.assign(bins, 0);

    // Compute histogram.
    for ( ; ! a.done(); a++) {
        float c = a[channel];
        if (c >= min && c < max) {
            // Map range min->max to 0->(bins-1).
            histogram[ (int) ((c-min) * ratio) ]++;
        } else if (c == max) {
            histogram[bins_minus_1]++;
        } else {
            if (submin_ok && c < min)
                (*submin)++;
            else if (supermax_ok)
                (*supermax)++;
        }
    }
    return true;
}

} // anonymous namespace



bool
ImageBufAlgo::histogram (const ImageBuf &A, int channel,
                         std::vector<imagesize_t> &histogram, int bins,
                         float min, float max, imagesize_t *submin,
                         imagesize_t *supermax, ROI roi)
{
    if (A.spec().format != TypeDesc::TypeFloat) {
        A.error ("Unsupported pixel data format '%s'", A.spec().format);
        return false;
    }

    if (A.nchannels() == 0) {
        A.error ("Input image must have at least 1 channel");
        return false;
    }

    if (channel < 0 || channel >= A.nchannels()) {
        A.error ("Invalid channel %d for input image with channels 0 to %d",
                  channel, A.nchannels()-1);
        return false;
    }

    if (bins < 1) {
        A.error ("The number of bins must be at least 1");
        return false;
    }

    if (max <= min) {
        A.error ("Invalid range, min must be strictly smaller than max");
        return false;
    }

    // Specified ROI -> use it. Unspecified ROI -> initialize from A.
    if (! roi.defined())
        roi = get_roi (A.spec());

    histogram_impl<float> (A, channel, histogram, bins, min, max,
                                  submin, supermax, roi);

    return ! A.has_error();
}



bool
ImageBufAlgo::histogram_draw (ImageBuf &R,
                              const std::vector<imagesize_t> &histogram)
{
    // Fail if there are no bins to draw.
    int bins = histogram.size();
    if (bins == 0) {
        R.error ("There are no bins to draw, the histogram is empty");
        return false;
    }

    // Check R and modify it if needed.
    int height = R.spec().height;
    if (R.spec().format != TypeDesc::TypeFloat || R.nchannels() != 1 ||
        R.spec().width != bins) {
        ImageSpec newspec = ImageSpec (bins, height, 1, TypeDesc::FLOAT);
        R.reset ("dummy", newspec);
    }

    // Fill output image R with white color.
    ImageBuf::Iterator<float, float> r (R);
    for ( ; ! r.done(); ++r)
        r[0] = 1;

    // Draw histogram left->right, bottom->up.
    imagesize_t max = *std::max_element (histogram.begin(), histogram.end());
    for (int b = 0; b < bins; b++) {
        int bin_height = (int) ((float)histogram[b]/(float)max*height + 0.5f);
        if (bin_height != 0) {
            // Draw one bin at column b.
            for (int j = 1; j <= bin_height; j++) {
                int row = height - j;
                r.pos (b, row);
                r[0] = 0;
            }
        }
    }
    return true;
}

}
OIIO_NAMESPACE_EXIT
