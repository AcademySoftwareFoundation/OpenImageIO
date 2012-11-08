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



OIIO_NAMESPACE_ENTER
{

namespace
{

template<typename T>
static inline void
fill_ (ImageBuf &dst, const float *values, ROI roi=ROI())
{
    int chbegin = roi.chbegin;
    int chend = std::min (roi.chend, dst.nchannels());
    for (ImageBuf::Iterator<T> p (dst, roi);  !p.done();  ++p)
        for (int c = chbegin, i = 0;  c < chend;  ++c, ++i)
            p[c] = values[i];
}

}

bool
ImageBufAlgo::fill (ImageBuf &dst, const float *pixel, ROI roi)
{
    ASSERT (pixel && "fill must have a non-NULL pixel value pointer");
    switch (dst.spec().format.basetype) {
    case TypeDesc::FLOAT : fill_<float> (dst, pixel, roi); break;
    case TypeDesc::UINT8 : fill_<unsigned char> (dst, pixel, roi); break;
    case TypeDesc::UINT16: fill_<unsigned short> (dst, pixel, roi); break;
    case TypeDesc::HALF  : fill_<half> (dst, pixel, roi); break;
    case TypeDesc::INT8  : fill_<char> (dst, pixel, roi); break;
    case TypeDesc::INT16 : fill_<short> (dst, pixel, roi); break;
    case TypeDesc::UINT  : fill_<unsigned int> (dst, pixel, roi); break;
    case TypeDesc::INT   : fill_<int> (dst, pixel, roi); break;
    case TypeDesc::UINT64: fill_<unsigned long long> (dst, pixel, roi); break;
    case TypeDesc::INT64 : fill_<long long> (dst, pixel, roi); break;
    case TypeDesc::DOUBLE: fill_<double> (dst, pixel, roi); break;
    default:
        dst.error ("Unsupported pixel data format '%s'", dst.spec().format);
        return false;
    }
    
    return true;
}


bool
ImageBufAlgo::zero (ImageBuf &dst, ROI roi)
{
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

};  // anon namespace



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

    // do the actual copying
    switch (src.spec().format.basetype) {
    case TypeDesc::FLOAT :
        return crop_<float> (dst, src, xbegin, xend, ybegin, yend, bordercolor);
        break;
    case TypeDesc::UINT8 :
        return crop_<unsigned char> (dst, src, xbegin, xend, ybegin, yend, bordercolor);
        break;
    case TypeDesc::INT8  :
        return crop_<char> (dst, src, xbegin, xend, ybegin, yend, bordercolor);
        break;
    case TypeDesc::UINT16:
        return crop_<unsigned short> (dst, src, xbegin, xend, ybegin, yend, bordercolor);
        break;
    case TypeDesc::INT16 :
        return crop_<short> (dst, src, xbegin, xend, ybegin, yend, bordercolor);
        break;
    case TypeDesc::UINT  :
        return crop_<unsigned int> (dst, src, xbegin, xend, ybegin, yend, bordercolor);
        break;
    case TypeDesc::INT   :
        return crop_<int> (dst, src, xbegin, xend, ybegin, yend, bordercolor);
        break;
    case TypeDesc::UINT64:
        return crop_<unsigned long long> (dst, src, xbegin, xend, ybegin, yend, bordercolor);
        break;
    case TypeDesc::INT64 :
        return crop_<long long> (dst, src, xbegin, xend, ybegin, yend, bordercolor);
        break;
    case TypeDesc::HALF  :
        return crop_<half> (dst, src, xbegin, xend, ybegin, yend, bordercolor);
        break;
    case TypeDesc::DOUBLE:
        return crop_<double> (dst, src, xbegin, xend, ybegin, yend, bordercolor);
        break;
    default:
        dst.error ("Unsupported pixel data format '%s'", src.spec().format);
        return false;
    }
    
    ASSERT (0);
    return false;
}




bool
ImageBufAlgo::channels (ImageBuf &dst, const ImageBuf &src,
                        int nchannels, const int *channelorder,
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
    if (shuffle_channel_names) {
        newspec.alpha_channel = -1;
        newspec.z_channel = -1;
        for (int c = 0; c < nchannels;  ++c) {
            int csrc = channelorder[c];
            if (csrc >= 0 && csrc < src.spec().nchannels) {
                newspec.channelnames[c] = src.spec().channelnames[csrc];
                if (csrc == src.spec().alpha_channel)
                    newspec.alpha_channel = c;
                if (csrc == src.spec().z_channel)
                    newspec.z_channel = c;
            }
        }
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
        if (channelorder[c] >= 0 && channelorder[c] < src.spec().nchannels) {
            int csrc = channelorder[c];
            src.get_pixel_channels (src.xbegin(), src.xend(),
                                    src.ybegin(), src.yend(),
                                    src.zbegin(), src.zend(),
                                    csrc, csrc+1, newspec.format, pixels,
                                    dstxstride, dstystride, dstzstride);
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
    switch (src.spec().format.basetype) {
    case TypeDesc::FLOAT : return isConstantColor_<float> (src, color); break;
    case TypeDesc::UINT8 : return isConstantColor_<unsigned char> (src, color); break;
    case TypeDesc::INT8  : return isConstantColor_<char> (src, color); break;
    case TypeDesc::UINT16: return isConstantColor_<unsigned short> (src, color); break;
    case TypeDesc::INT16 : return isConstantColor_<short> (src, color); break;
    case TypeDesc::UINT  : return isConstantColor_<unsigned int> (src, color); break;
    case TypeDesc::INT   : return isConstantColor_<int> (src, color); break;
    case TypeDesc::UINT64: return isConstantColor_<unsigned long long> (src, color); break;
    case TypeDesc::INT64 : return isConstantColor_<long long> (src, color); break;
    case TypeDesc::HALF  : return isConstantColor_<half> (src, color); break;
    case TypeDesc::DOUBLE: return isConstantColor_<double> (src, color); break;
    default:
        src.error ("Unsupported pixel data format '%s'", src.spec().format);
        return false;
    }
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
    switch (src.spec().format.basetype) {
    case TypeDesc::FLOAT : return isConstantChannel_<float> (src, channel, val); break;
    case TypeDesc::UINT8 : return isConstantChannel_<unsigned char> (src, channel, val); break;
    case TypeDesc::INT8  : return isConstantChannel_<char> (src, channel, val); break;
    case TypeDesc::UINT16: return isConstantChannel_<unsigned short> (src, channel, val); break;
    case TypeDesc::INT16 : return isConstantChannel_<short> (src, channel, val); break;
    case TypeDesc::UINT  : return isConstantChannel_<unsigned int> (src, channel, val); break;
    case TypeDesc::INT   : return isConstantChannel_<int> (src, channel, val); break;
    case TypeDesc::UINT64: return isConstantChannel_<unsigned long long> (src, channel, val); break;
    case TypeDesc::INT64 : return isConstantChannel_<long long> (src, channel, val); break;
    case TypeDesc::HALF  : return isConstantChannel_<half> (src, channel, val); break;
    case TypeDesc::DOUBLE: return isConstantChannel_<double> (src, channel, val); break;
    default:
        src.error ("Unsupported pixel data format '%s'", src.spec().format);
        return false;
    }
};

namespace
{

template<typename T>
static inline bool
isMonochrome_ (const ImageBuf &src)
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
    switch (src.spec().format.basetype) {
    case TypeDesc::FLOAT : return isMonochrome_<float> (src); break;
    case TypeDesc::UINT8 : return isMonochrome_<unsigned char> (src); break;
    case TypeDesc::INT8  : return isMonochrome_<char> (src); break;
    case TypeDesc::UINT16: return isMonochrome_<unsigned short> (src); break;
    case TypeDesc::INT16 : return isMonochrome_<short> (src); break;
    case TypeDesc::UINT  : return isMonochrome_<unsigned int> (src); break;
    case TypeDesc::INT   : return isMonochrome_<int> (src); break;
    case TypeDesc::UINT64: return isMonochrome_<unsigned long long> (src); break;
    case TypeDesc::INT64 : return isMonochrome_<long long> (src); break;
    case TypeDesc::HALF  : return isMonochrome_<half> (src); break;
    case TypeDesc::DOUBLE: return isMonochrome_<double> (src); break;
    default:
        src.error ("Unsupported pixel data format '%s'", src.spec().format);
        return false;
    }
};

std::string
ImageBufAlgo::computePixelHashSHA1(const ImageBuf &src,
                                   const std::string & extrainfo)
{
    std::string hash_digest;
    
    CSHA1 sha;
    sha.Reset ();
    
    // Do one scanline at a time, to keep to < 2^32 bytes each
    imagesize_t scanline_bytes = src.spec().scanline_bytes();
    ASSERT (scanline_bytes < std::numeric_limits<unsigned int>::max());
    std::vector<unsigned char> tmp (scanline_bytes);
    for (int z = src.zmin(), zend=src.zend();  z < zend;  ++z) {
        for (int y = src.ymin(), yend=src.yend();  y < yend;  ++y) {
            src.get_pixels (src.xbegin(), src.xend(), y, y+1, z, z+1,
                            src.spec().format, &tmp[0]);
            sha.Update (&tmp[0], (unsigned int) scanline_bytes);
        }
    }
    
    // If extra info is specified, also include it in the sha computation
    if(!extrainfo.empty()) {
        sha.Update ((const unsigned char*) extrainfo.c_str(), extrainfo.size());
    }
    
    sha.Final ();
    sha.ReportHashStl (hash_digest, CSHA1::REPORT_HEX_SHORT);
    
    return hash_digest;
}

std::string
ImageBufAlgo::computePixelHashSHA1(const ImageBuf &src)
{
    return computePixelHashSHA1 (src, "");
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
                                                           0, 1, true);
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
                                                       0, 1, true);
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
    switch (src.spec().format.basetype) {
    case TypeDesc::FLOAT :
        return resize_<float> (dst, src, xbegin, xend, ybegin, yend, filter);
    case TypeDesc::UINT8 :
        return resize_<unsigned char> (dst, src, xbegin, xend, ybegin, yend, filter);
    case TypeDesc::INT8  :
        return resize_<char> (dst, src, xbegin, xend, ybegin, yend, filter);
    case TypeDesc::UINT16:
        return resize_<unsigned short> (dst, src, xbegin, xend, ybegin, yend, filter);
    case TypeDesc::INT16 :
        return resize_<short> (dst, src, xbegin, xend, ybegin, yend, filter);
    case TypeDesc::UINT  :
        return resize_<unsigned int> (dst, src, xbegin, xend, ybegin, yend, filter);
    case TypeDesc::INT   :
        return resize_<int> (dst, src, xbegin, xend, ybegin, yend, filter);
    case TypeDesc::UINT64:
        return resize_<unsigned long long> (dst, src, xbegin, xend, ybegin, yend, filter);
    case TypeDesc::INT64 :
        return resize_<long long> (dst, src, xbegin, xend, ybegin, yend, filter);
    case TypeDesc::HALF  :
        return resize_<half> (dst, src, xbegin, xend, ybegin, yend, filter);
    case TypeDesc::DOUBLE:
        return resize_<double> (dst, src, xbegin, xend, ybegin, yend, filter);
    default:
        dst.error ("Unsupported pixel data format '%s'", src.spec().format);
        return false;
    }

    ASSERT (0);
    return false;
}

namespace
{

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
        
        ImageBuf::Iterator<SRCTYPE> pixel (dst);
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
        
        ImageBuf::Iterator<SRCTYPE> pixel (dst);
        
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
                    
                    ImageBuf::Iterator<SRCTYPE> it (dst, top, bottom, left, right);
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
         // This use of float here is on purpose to allow for simpler
         // implementations that work on all data types
        return fixNonFinite_<float> (dst, src, mode, pixelsFixed);
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

// Fully type-specialized version of over.
template<class Rtype, class Atype, class Btype>
bool
over_impl (ImageBuf &R, const ImageBuf &A, const ImageBuf &B, ROI roi)
{
    if (R.spec().format != BaseTypeFromC<Rtype>::value ||
        A.spec().format != BaseTypeFromC<Atype>::value ||
        B.spec().format != BaseTypeFromC<Btype>::value) {
        R.error ("Unsupported pixel data format combination '%s / %s / %s'",
                 R.spec().format, A.spec().format, B.spec().format);
        return false;   // double check that types match
    }

    // Output image R.
    const ImageSpec &specR = R.spec();
    int channels_R = specR.nchannels;

    // Input image A.
    const ImageSpec &specA = A.spec();
    int alpha_index_A =  specA.alpha_channel;
    int has_alpha_A = (alpha_index_A >= 0);
    int channels_A = specA.nchannels;

    // Input image B.
    const ImageSpec &specB = B.spec();
    int alpha_index_B =  specB.alpha_channel;
    int has_alpha_B = (alpha_index_B >= 0);
    int channels_B = specB.nchannels;

    int channels_AB = std::min (channels_A, channels_B);

    ImageBuf::ConstIterator<Atype, float> a (A);
    ImageBuf::ConstIterator<Btype, float> b (B);
    ImageBuf::Iterator<Rtype, float> r (R, roi);
    for ( ; ! r.done(); r++) {
        a.pos (r.x(), r.y(), r.z());
        b.pos (r.x(), r.y(), r.z());

        if (! a.valid()) {
            if (! b.valid()) {
                // a and b invalid.
                for (int c = 0; c < channels_R; c++) { r[c] = 0.0f; }
            } else {
                // a invalid, b valid.
                for (int c = 0; c < channels_B; c++) { r[c] = b[c]; }
                if (! has_alpha_B) { r[3] = 1.0f; }
            }
            continue;
        }

        if (! b.valid()) {
            // a valid, b invalid.
            for (int c = 0; c < channels_A; c++) { r[c] = a[c]; }
            if (! has_alpha_A) { r[3] = 1.0f; }
            continue;
        }

        // At this point, a and b are valid.
        float alpha_A = has_alpha_A 
                        ? clamp (a[alpha_index_A], 0.0f, 1.0f) : 1.0f;
        float one_minus_alpha_A = 1.0f - alpha_A;
        for (int c = 0;  c < channels_AB;  c++)
            r[c] = a[c] + one_minus_alpha_A * b[c];
        if (channels_R != channels_AB) {
            // R has 4 channels, A or B has 3 channels -> alpha channel is 3.
            r[3] = alpha_A + one_minus_alpha_A * (has_alpha_B ? b[3] : 1.0f);
        }
    }
    return true;
}

}    // anonymous namespace


bool
ImageBufAlgo::over (ImageBuf &R, const ImageBuf &A, const ImageBuf &B, ROI roi,
                    int nthreads)
{
    // Output image R.
    const ImageSpec &specR = R.spec();
    int alpha_R =  specR.alpha_channel;
    int has_alpha_R = (alpha_R >= 0);
    int channels_R = specR.nchannels;
    int non_alpha_R = channels_R - has_alpha_R;
    bool initialized_R = R.initialized();

    // Input image A.
    const ImageSpec &specA = A.spec();
    int alpha_A =  specA.alpha_channel;
    int has_alpha_A = (alpha_A >= 0);
    int channels_A = specA.nchannels;
    int non_alpha_A = has_alpha_A ? (channels_A - 1) : 3;
    bool A_not_34 = channels_A != 3 && channels_A != 4;

    // Input image B.
    const ImageSpec &specB = B.spec();
    int alpha_B =  specB.alpha_channel;
    int has_alpha_B = (alpha_B >= 0);
    int channels_B = specB.nchannels;
    int non_alpha_B = has_alpha_B ? (channels_B - 1) : 3;
    bool B_not_34 = channels_B != 3 && channels_B != 4;

    // At present, this operation only supports ImageBuf's containing
    // float pixel data.
    if (R.spec().format != TypeDesc::TypeFloat ||
        A.spec().format != TypeDesc::TypeFloat ||
        B.spec().format != TypeDesc::TypeFloat) {
        R.error ("Unsupported pixel data format combination '%s / %s / %s'",
                   R.spec().format, A.spec().format, B.spec().format);
        return false;
    }

    // Fail if the input images have a Z channel.
    if (specA.z_channel >= 0 || specB.z_channel >= 0) {
        R.error ("'over' does not support Z channels");
        return false;
    }

    // If input images A and B have different number of non-alpha channels
    // then return false.
    if (non_alpha_A != non_alpha_B) {
        R.error ("inputs had different numbers of color channels");
        return false;
    }

    // A or B has number of channels different than 3 and 4, and it does
    // not have an alpha channel.
    if ((A_not_34 && !has_alpha_A) || (B_not_34 && !has_alpha_B)) {
        R.error ("inputs must have alpha channels (or be implicitly RGB or RGBA)");
        return false;
    }

    // A or B has zero or one channel -> return false.
    if (channels_A <= 1 || channels_B <= 1) {
        R.error ("unsupported number of channels");
        return false;
    }

    // Initialized R -> use as allocated.  
    // Uninitialized R -> size it to the union of A and B.
    ImageSpec newspec = ImageSpec ();
    ROI union_AB = roi_union (get_roi(specA), get_roi(specB));
    set_roi (newspec, union_AB);
    if ((! has_alpha_A && ! has_alpha_B)
        || (has_alpha_A && ! has_alpha_B && alpha_A == channels_A - 1)
        || (! has_alpha_A && has_alpha_B && alpha_B == channels_B - 1)) {
        if (! initialized_R) {
            newspec.nchannels = 4;
            newspec.alpha_channel =  3;
            R.reset ("over", newspec);
        } else {
            if (non_alpha_R != 3 || alpha_R != 3) {
                R.error ("unsupported channel layout");
                return false;
            }
        }
    } else if (has_alpha_A && has_alpha_B && alpha_A == alpha_B) {
        if (! initialized_R) {
            newspec.nchannels = channels_A;
            newspec.alpha_channel =  alpha_A;
            R.reset ("over", newspec);
        } else {
            if (non_alpha_R != non_alpha_A || alpha_R != alpha_A) {
                R.error ("unsupported channel layout");
                return false;
            }
        }
    } else {
        R.error ("unsupported channel layout");
        return false;
    }

    // Specified ROI -> use it. Unspecified ROI -> initialize from R.
    if (! roi.defined())
        roi = get_roi (R.spec());

    parallel_image (boost::bind (over_impl<float,float,float>, boost::ref(R),
                                 boost::cref(A), boost::cref(B), _1),
                           roi, nthreads);
    return ! R.has_error();
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
