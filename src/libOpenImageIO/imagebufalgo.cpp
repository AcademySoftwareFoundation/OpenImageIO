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
#include <boost/shared_ptr.hpp>

#include <OpenEXR/ImathFun.h>
#include <OpenEXR/half.h>

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "imagebuf.h"
#include "imagebufalgo.h"
#include "imagebufalgo_util.h"
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

///////////////////////////////////////////////////////////////////////////
// Guidelines for ImageBufAlgo functions:
//
// * Signature will always be:
//       bool function (ImageBuf &R /* result */, 
//                      const ImageBuf &A, ...other input images...,
//                      ...other parameters...
//                      ROI roi = ROI::All(),
//                      int nthreads = 0);
// * The ROI should restrict the operation to those pixels (and channels)
//   specified. Default ROI::All() means perform the operation on all
//   pixel in R's data window.
// * It's ok to omit ROI and threads from the few functions that
//   (a) can't possibly be parallelized, and (b) do not make sense to
//   apply to anything less than the entire image.
// * Be sure to clamp the channel range to those actually used.
// * If R is initialized, do not change any pixels outside the ROI.
//   If R is uninitialized, redefine ROI to be the union of the input
//   images' data windows and allocate R to be that size.
// * Try to always do the "reasonable thing" rather than be too brittle.
// * For errors (where there is no "reasonable thing"), set R's error
//   condition using R.error() with R.error() and return false.
// * Always use IB::Iterators/ConstIterator, NEVER use getpixel/setpixel.
// * Use the iterator Black or Clamp wrap modes to avoid lots of special
//   cases inside the pixel loops.
// * Use OIIO_DISPATCH_* macros to call type-specialized templated
//   implemenations.  It is permissible to use OIIO_DISPATCH_COMMON_TYPES_*
//   to tame the cross-product of types, especially for binary functions
//   (A,B inputs as well as R output).
///////////////////////////////////////////////////////////////////////////


OIIO_NAMESPACE_ENTER
{


// Convenient helper struct to bundle a 3-int describing a block size.
struct Dim3 {
    int x, y, z;
    Dim3 (int x, int y=1, int z=1) : x(x), y(y), z(z) { }
};



void
ImageBufAlgo::IBAprep (ROI &roi, ImageBuf *dst,
                       const ImageBuf *A, const ImageBuf *B)
{
    if (dst->initialized()) {
        // Valid destination image.  Just need to worry about ROI.
        if (roi.defined()) {
            // Shrink-wrap ROI to the destination (including chend)
            roi = roi_intersection (roi, get_roi(dst->spec()));
        } else {
            // No ROI? Set it to all of dst's pixel window.
            roi = get_roi (dst->spec());
        }
    } else {
        // Not an initialized destination image!
        ASSERT ((A || roi.defined()) &&
                "ImageBufAlgo without any guess about region of interest");
        ROI full_roi;
        if (! roi.defined()) {
            // No ROI -- make it the union of the pixel regions of the inputs
            roi = get_roi (A->spec());
            full_roi = get_roi_full (A->spec());
            if (B) {
                roi = roi_union (roi, get_roi (B->spec()));
                full_roi = roi_union (full_roi, get_roi_full (B->spec()));
            }
        } else {
            roi.chend = std::min (roi.chend, A->nchannels());
            full_roi = roi;
        }
        // Now we allocate space for dst.  Give it A's spec, but adjust
        // the dimensions to match the ROI.
        ImageSpec spec;
        if (A) {
            // If there's an input image, give dst A's spec (with
            // modifications detailed below...)
            spec = A->spec();
            // For two inputs, if they aren't the same data type, punt and
            // allocate a float buffer. If the user wanted something else,
            // they should have pre-allocated dst with their desired format.
            if (B && A->spec().format != B->spec().format)
                spec.set_format (TypeDesc::FLOAT);
        } else {
            spec.set_format (TypeDesc::FLOAT);
            spec.nchannels = roi.chend;
            spec.default_channel_names ();
        }
        // Set the image dimensions based on ROI.
        set_roi (spec, roi);
        if (full_roi.defined())
            set_roi_full (spec, full_roi);
        else
            set_roi_full (spec, roi);
        dst->alloc (spec);
    }
}



template<typename T>
static bool
fill_ (ImageBuf &dst, const float *values, ROI roi=ROI(), int nthreads=1)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Lots of pixels and request for multi threads? Parallelize.
        ImageBufAlgo::parallel_image (
            boost::bind(fill_<T>, boost::ref(dst), values,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
    for (ImageBuf::Iterator<T> p (dst, roi);  !p.done();  ++p)
        for (int c = roi.chbegin;  c < roi.chend;  ++c)
            p[c] = values[c];
    return true;
}


bool
ImageBufAlgo::fill (ImageBuf &dst, const float *pixel, ROI roi, int nthreads)
{
    ASSERT (pixel && "fill must have a non-NULL pixel value pointer");
    IBAprep (roi, &dst);
    OIIO_DISPATCH_TYPES ("fill", fill_, dst.spec().format,
                         dst, pixel, roi, nthreads);
    return true;
}


bool
ImageBufAlgo::zero (ImageBuf &dst, ROI roi, int nthreads)
{
    IBAprep (roi, &dst);
    float *zero = ALLOCA(float,roi.chend);
    memset (zero, 0, roi.chend*sizeof(float));
    return fill (dst, zero, roi, nthreads);
}



template<typename T>
static bool
checker_ (ImageBuf &dst, Dim3 size,
          const float *color1, const float *color2,
          Dim3 offset,
          ROI roi, int nthreads=1)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Lots of pixels and request for multi threads? Parallelize.
        ImageBufAlgo::parallel_image (
            boost::bind(checker_<T>, boost::ref(dst),
                        size, color1, color2, offset,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
    for (ImageBuf::Iterator<T> p (dst, roi);  !p.done();  ++p) {
        int v = (p.z()-offset.z)/size.z + (p.y()-offset.y)/size.y
              + (p.x()-offset.x)/size.x;
        if (v & 1)
            for (int c = roi.chbegin;  c < roi.chend;  ++c)
                p[c] = color2[c];
        else
            for (int c = roi.chbegin;  c < roi.chend;  ++c)
                p[c] = color1[c];
    }
    return true;
}



bool
ImageBufAlgo::checker (ImageBuf &dst, int width, int height, int depth,
                       const float *color1, const float *color2,
                       int xoffset, int yoffset, int zoffset,
                       ROI roi, int nthreads)
{
    IBAprep (roi, &dst);
    OIIO_DISPATCH_TYPES ("checker", checker_, dst.spec().format,
                         dst, Dim3(width, height, depth), color1, color2,
                         Dim3(xoffset, yoffset, zoffset), roi, nthreads);
    return true;
}

/// DEPRECATED as of 1.2
bool
ImageBufAlgo::checker (ImageBuf &dst, int width,
                       const float *color1, const float *color2,
                       int xbegin, int xend, int ybegin, int yend,
                       int zbegin, int zend)
{
    return checker (dst, width, width, width, color1, color2, 0, 0, 0,
                    ROI(xbegin,xend,ybegin,yend,zbegin,zend), 0);
}



template<class D, class S>
static bool
paste_ (ImageBuf &dst, ROI dstroi,
        const ImageBuf &src, ROI srcroi, int nthreads)
{
    // N.B. Punt on parallelizing because of the subtle interplay
    // between srcroi and dstroi, the parallel_image idiom doesn't
    // handle that especially well. And it's not worth customizing for
    // this function which is inexpensive and not commonly used, and so
    // would benefit little from parallelizing. We can always revisit
    // this later. But in the mean time, we maintain the 'nthreads'
    // parameter for uniformity with the rest of IBA.
    int src_nchans = src.nchannels ();
    int dst_nchans = dst.nchannels ();
    ImageBuf::ConstIterator<S,D> s (src, srcroi);
    ImageBuf::Iterator<D,D> d (dst, dstroi);
    for ( ;  ! s.done();  ++s, ++d) {
        if (! d.exists())
            continue;  // Skip paste-into pixels that don't overlap dst's data
        for (int c = srcroi.chbegin, c_dst = dstroi.chbegin;
             c < srcroi.chend;  ++c, ++c_dst) {
            if (c_dst >= 0 && c_dst < dst_nchans)
                d[c_dst] = c < src_nchans ? s[c] : D(0);
        }
    }
    return true;
}



bool
ImageBufAlgo::paste (ImageBuf &dst, int xbegin, int ybegin,
                     int zbegin, int chbegin,
                     const ImageBuf &src, ROI srcroi, int nthreads)
{
    if (! srcroi.defined())
        srcroi = get_roi(src.spec());

    ROI dstroi (xbegin, xbegin+srcroi.width(),
                ybegin, ybegin+srcroi.height(),
                zbegin, zbegin+srcroi.depth(),
                chbegin, chbegin+srcroi.nchannels());
    ROI dstroi_save = dstroi;  // save the original
    IBAprep (dstroi, &dst);

    // do the actual copying
    OIIO_DISPATCH_TYPES2 ("paste", paste_, dst.spec().format, src.spec().format,
                          dst, dstroi_save, src, srcroi, nthreads);
    return false;
}




template<class D, class S>
static bool
crop_ (ImageBuf &dst, const ImageBuf &src,
       ROI roi, int nthreads=1)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Lots of pixels and request for multi threads? Parallelize.
        ImageBufAlgo::parallel_image (
            boost::bind(crop_<D,S>, boost::ref(dst), boost::cref(src),
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
    ImageBuf::ConstIterator<S,D> s (src, roi);
    ImageBuf::Iterator<D,D> d (dst, roi);
    for ( ;  ! d.done();  ++d, ++s) {
        for (int c = roi.chbegin;  c < roi.chend;  ++c)
            d[c] = s[c];
    }
    return true;
}



bool 
ImageBufAlgo::crop (ImageBuf &dst, const ImageBuf &src,
                    ROI roi, int nthreads)
{
    dst.clear ();
    roi.chend = std::min (roi.chend, src.nchannels());
    IBAprep (roi, &dst, &src);
    OIIO_DISPATCH_TYPES2 ("crop", crop_, dst.spec().format, src.spec().format,
                          dst, src, roi, nthreads);
    return false;
}




template<class D>
static bool
clamp_ (ImageBuf &dst, const float *min, const float *max,
        bool clampalpha01, ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Lots of pixels and request for multi threads? Parallelize.
        ImageBufAlgo::parallel_image (
            boost::bind(clamp_<D>, boost::ref(dst), min, max, clampalpha01,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
    for (ImageBuf::Iterator<D> d (dst, roi);  ! d.done();  ++d) {
        for (int c = roi.chbegin;  c < roi.chend;  ++c)
            d[c] = OIIO::clamp<float> (d[c], min[c], max[c]);
    }
    int a = dst.spec().alpha_channel;
    if (clampalpha01 && a >= roi.chbegin && a < roi.chend) {
        for (ImageBuf::Iterator<D> d (dst, roi);  ! d.done();  ++d) {
            d[a] = OIIO::clamp<float> (d[a], 0.0f, 1.0f);
        }
    }
    return true;
}



bool
ImageBufAlgo::clamp (ImageBuf &dst, const float *min, const float *max,
                     bool clampalpha01, ROI roi, int nthreads)
{
    IBAprep (roi, &dst);
    std::vector<float> minvec, maxvec;
    if (! min) {
        minvec.resize (dst.nchannels(), -std::numeric_limits<float>::max());
        min = &minvec[0];
    }
    if (! max) {
        maxvec.resize (dst.nchannels(), std::numeric_limits<float>::max());
        max = &maxvec[0];
    }
    OIIO_DISPATCH_TYPES ("clamp", clamp_, dst.spec().format, dst,
                         min, max, clampalpha01, roi, nthreads);
    return false;
}



bool
ImageBufAlgo::clamp (ImageBuf &dst, float min, float max,
                     bool clampalpha01, ROI roi, int nthreads)
{
    IBAprep (roi, &dst);
    std::vector<float> minvec (dst.nchannels(), min);
    std::vector<float> maxvec (dst.nchannels(), max);
    OIIO_DISPATCH_TYPES ("clamp", clamp_, dst.spec().format, dst,
                         &minvec[0], &maxvec[0], clampalpha01, roi, nthreads);
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
            ImageBufAlgo::fill (dst, &channelvalues[0], roi);
        }
        pixels += channelsize;
    }
    return true;
}



bool
ImageBufAlgo::setNumChannels(ImageBuf &dst, const ImageBuf &src, int numChannels)
{
    return ImageBufAlgo::channels (dst, src, numChannels, NULL, NULL, NULL, true);
}



template<class ABtype>
static bool
channel_append_impl (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                     ROI roi, int nthreads)
{
    if (nthreads == 1 || roi.npixels() < 1000) {
        int na = A.nchannels(), nb = B.nchannels();
        int n = std::min (dst.nchannels(), na+nb);
        ImageBuf::Iterator<float> r (dst, roi);
        ImageBuf::ConstIterator<ABtype> a (A, roi);
        ImageBuf::ConstIterator<ABtype> b (B, roi);
        for (;  !r.done();  ++r) {
            a.pos (r.x(), r.y(), r.z());
            b.pos (r.x(), r.y(), r.z());
            for (int c = 0; c < n; ++c) {
                if (c < na)
                    r[c] = a.exists() ? a[c] : 0.0f;
                else
                    r[c] = b.exists() ? b[c-na] : 0.0f;
            }
        }
    } else {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind (channel_append_impl<ABtype>, boost::ref(dst),
                         boost::cref(A), boost::cref(B), _1, 1),
            roi, nthreads);
    }
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
        dstspec.set_format (TypeDesc::TypeFloat);
        // Append the channel descriptions
        dstspec.nchannels = A.spec().nchannels + B.spec().nchannels;
        for (int c = 0;  c < B.spec().nchannels;  ++c) {
            std::string name = B.spec().channelnames[c];
            // Eliminate duplicates
            if (std::find(dstspec.channelnames.begin(), dstspec.channelnames.end(), name) != dstspec.channelnames.end())
                name = Strutil::format ("channel%d", A.spec().nchannels+c);
            dstspec.channelnames.push_back (name);
        }
        if (dstspec.alpha_channel < 0 && B.spec().alpha_channel >= 0)
            dstspec.alpha_channel = B.spec().alpha_channel + A.nchannels();
        if (dstspec.z_channel < 0 && B.spec().z_channel >= 0)
            dstspec.z_channel = B.spec().z_channel + A.nchannels();
        set_roi (dstspec, roi);
        dst.reset (dst.name(), dstspec);
    }

    // For now, only support float destination, and equivalent A and B
    // types.
    if (dst.spec().format != TypeDesc::FLOAT ||
        A.spec().format != B.spec().format) {
        dst.error ("Unable to perform channel_append of %s, %s -> %s",
                   A.spec().format, B.spec().format, dst.spec().format);
        return false;
    }

    OIIO_DISPATCH_TYPES ("channel_append", channel_append_impl,
                         A.spec().format, dst, A, B, roi, nthreads);
    return true;
}



// DEPRECATED version
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



bool
ImageBufAlgo::add (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B,
                   ROI roi, int nthreads)
{
    IBAprep (roi, &dst, &A, &B);
    OIIO_DISPATCH_COMMON_TYPES3 ("add", add_impl, dst.spec().format,
                                 A.spec().format, B.spec().format,
                                 dst, A, B, roi, nthreads);
    return true;
}



template<class Rtype>
static bool
add_inplace (ImageBuf &R, const float *val,
             ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(add_inplace<Rtype>, boost::ref(R), val,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
    ImageBuf::Iterator<Rtype> r (R, roi);
    for ( ;  !r.done();  ++r)
        for (int c = roi.chbegin;  c < roi.chend;  ++c)
            r[c] = r[c] + val[c];
    return true;
}



bool
ImageBufAlgo::add (ImageBuf &dst, const float *val, ROI roi, int nthreads)
{
    IBAprep (roi, &dst);
    OIIO_DISPATCH_TYPES ("add", add_inplace, dst.spec().format,
                         dst, val, roi, nthreads);
    return true;
}



bool
ImageBufAlgo::add (ImageBuf &R, float val, ROI roi, int nthreads)
{
    int nc = R.nchannels();
    float *vals = ALLOCA (float, nc);
    for (int c = 0;  c < nc;  ++c)
        vals[c] = val;
    return add (R, vals, roi, nthreads);
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
    IBAprep (roi, &dst, &A, &B);
    OIIO_DISPATCH_COMMON_TYPES3 ("sub", sub_impl, dst.spec().format,
                                 A.spec().format, B.spec().format,
                                 dst, A, B, roi, nthreads);
    return true;
}



template<class Rtype>
static bool
mul_impl (ImageBuf &R, const float *val, ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(mul_impl<Rtype>, boost::ref(R), val,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    ImageBuf::Iterator<Rtype> r (R, roi);
    for (ImageBuf::Iterator<Rtype> r (R, roi);  !r.done();  ++r)
        for (int c = roi.chbegin;  c < roi.chend;  ++c)
            r[c] = r[c] * val[c];
    return true;
}



bool
ImageBufAlgo::mul (ImageBuf &dst, const float *val, ROI roi, int nthreads)
{
    IBAprep (roi, &dst);
    OIIO_DISPATCH_TYPES ("mul", mul_impl, dst.spec().format,
                         dst, val, roi, nthreads);
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



inline float rangecompress (float x)
{
    // Formula courtesy of Sony Pictures Imageworks
    const float x1 = 1.0, a = 1.2607481479644775391;
    const float b = 0.28785100579261779785, c = -1.4042005538940429688;
    float absx = fabsf(x);
    if (absx <= x1)
        return x;
    return copysignf (a + b * logf(fabsf(c*absx + 1.0f)), x);
}



inline float rangeexpand (float y)
{
    // Formula courtesy of Sony Pictures Imageworks
    const float x1 = 1.0, a = 1.2607481479644775391;
    const float b = 0.28785100579261779785, c = -1.4042005538940429688;
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



template<class Rtype>
static bool
rangecompress_ (ImageBuf &R, bool useluma, ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(rangecompress_<Rtype>, boost::ref(R), useluma,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    const ImageSpec &Rspec (R.spec());
    int alpha_channel = Rspec.alpha_channel;
    int z_channel = Rspec.z_channel;
    if (roi.nchannels() < 3 ||
        (alpha_channel >= roi.chbegin && alpha_channel < roi.chbegin+3) ||
        (z_channel >= roi.chbegin && z_channel < roi.chbegin+3)) {
        useluma = false;  // No way to use luma
    }

    ImageBuf::Iterator<Rtype> r (R, roi);
    for (ImageBuf::Iterator<Rtype> r (R, roi);  !r.done();  ++r) {
        if (useluma) {
            float luma = 0.21264f * r[roi.chbegin] + 0.71517f * r[roi.chbegin+1] + 0.07219f * r[roi.chbegin+2];
            if (fabsf(luma) <= 1.0f)
                continue;  // Not HDR, no range compression needed
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
    return true;
}



template<class Rtype>
static bool
rangeexpand_ (ImageBuf &R, bool useluma, ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(rangeexpand_<Rtype>, boost::ref(R), useluma,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    const ImageSpec &Rspec (R.spec());
    int alpha_channel = Rspec.alpha_channel;
    int z_channel = Rspec.z_channel;
    if (roi.nchannels() < 3 ||
        (alpha_channel >= roi.chbegin && alpha_channel < roi.chbegin+3) ||
        (z_channel >= roi.chbegin && z_channel < roi.chbegin+3)) {
        useluma = false;  // No way to use luma
    }

    ImageBuf::Iterator<Rtype> r (R, roi);
    for (ImageBuf::Iterator<Rtype> r (R, roi);  !r.done();  ++r) {
        if (useluma) {
            float luma = 0.21264f * r[roi.chbegin] + 0.71517f * r[roi.chbegin+1] + 0.07219f * r[roi.chbegin+2];
            if (fabsf(luma) <= 1.0f)
                continue;  // Not HDR, no range compression needed
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
    return true;
}



bool
ImageBufAlgo::rangecompress (ImageBuf &dst, bool useluma,
                             ROI roi, int nthreads)
{
    // If the data type can't handle extended range, this is a no-op
    int basetype = dst.spec().format.basetype;
    if (basetype != TypeDesc::FLOAT && basetype != TypeDesc::HALF &&
        basetype != TypeDesc::DOUBLE)
        return true;

    IBAprep (roi, &dst);
    switch (basetype) {
    case TypeDesc::FLOAT:
        return rangecompress_<float> (dst, useluma, roi, nthreads);
    case TypeDesc::HALF:
        return rangecompress_<half> (dst, useluma, roi, nthreads);
    case TypeDesc::DOUBLE:
        return rangecompress_<double> (dst, useluma, roi, nthreads);
    default:
        return true;
    }
    return true;
}



bool
ImageBufAlgo::rangeexpand (ImageBuf &dst, bool useluma,
                           ROI roi, int nthreads)
{
    // If the data type can't handle extended range, this is a no-op
    int basetype = dst.spec().format.basetype;
    if (basetype != TypeDesc::FLOAT && basetype != TypeDesc::HALF &&
        basetype != TypeDesc::DOUBLE)
        return true;

    IBAprep (roi, &dst);
    switch (basetype) {
    case TypeDesc::FLOAT:
        return rangeexpand_<float> (dst, useluma, roi, nthreads);
    case TypeDesc::HALF:
        return rangeexpand_<half> (dst, useluma, roi, nthreads);
    case TypeDesc::DOUBLE:
        return rangeexpand_<double> (dst, useluma, roi, nthreads);
    default:
        return true;
    }
    return true;
}



template<class Rtype>
static bool
unpremult_ (ImageBuf &R, ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(unpremult_<Rtype>, boost::ref(R),
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    int alpha_channel = R.spec().alpha_channel;
    int z_channel = R.spec().z_channel;
    for (ImageBuf::Iterator<Rtype> r (R, roi);  !r.done();  ++r) {
        float alpha = r[alpha_channel];
        if (alpha == 0.0f || alpha == 1.0f)
            continue;
        for (int c = roi.chbegin;  c < roi.chend;  ++c)
            if (c != alpha_channel && c != z_channel)
                r[c] = r[c] / alpha;
    }
    return true;
}



bool
ImageBufAlgo::unpremult (ImageBuf &dst,  ROI roi, int nthreads)
{
    if (dst.spec().alpha_channel < 0)
        return true;

    IBAprep (roi, &dst);
    OIIO_DISPATCH_TYPES ("unpremult", unpremult_, dst.spec().format,
                         dst, roi, nthreads);
    return true;
}



template<class Rtype>
static bool
premult_ (ImageBuf &R, ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(premult_<Rtype>, boost::ref(R),
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    int alpha_channel = R.spec().alpha_channel;
    int z_channel = R.spec().z_channel;
    for (ImageBuf::Iterator<Rtype> r (R, roi);  !r.done();  ++r) {
        float alpha = r[alpha_channel];
        if (alpha == 1.0f)
            continue;
        for (int c = roi.chbegin;  c < roi.chend;  ++c)
            if (c != alpha_channel && c != z_channel)
                r[c] = r[c] * alpha;
    }
    return true;
}



bool
ImageBufAlgo::premult (ImageBuf &dst, ROI roi, int nthreads)
{
    if (dst.spec().alpha_channel < 0)
        return true;

    IBAprep (roi, &dst);
    OIIO_DISPATCH_TYPES ("premult", premult_, dst.spec().format,
                         dst, roi, nthreads);
    return true;
}



inline void
reset (ImageBufAlgo::PixelStats &p, int nchannels)
{
    const float inf = std::numeric_limits<float>::infinity();
    p.min.clear ();          p.min.resize (nchannels, inf);
    p.max.clear ();          p.max.resize (nchannels, -inf);
    p.avg.clear ();          p.avg.resize (nchannels);
    p.stddev.clear ();       p.stddev.resize (nchannels);
    p.nancount.clear ();     p.nancount.resize (nchannels, 0);
    p.infcount.clear ();     p.infcount.resize (nchannels, 0);
    p.finitecount.clear ();  p.finitecount.resize (nchannels, 0);
    p.sum.clear ();          p.sum.resize (nchannels, 0.0);
    p.sum2.clear ();         p.sum2.resize (nchannels, 0.0);
}


inline void
merge (ImageBufAlgo::PixelStats &sum, const ImageBufAlgo::PixelStats &p)
{
    ASSERT (sum.min.size() == p.min.size());
    for (size_t c = 0, e = sum.min.size(); c < e;  ++c) {
        sum.min[c] = std::min (sum.min[c], p.min[c]);
        sum.max[c] = std::max (sum.max[c], p.max[c]);
        sum.nancount[c] += p.nancount[c];
        sum.infcount[c] += p.infcount[c];
        sum.finitecount[c] += p.finitecount[c];
        sum.sum[c] += p.sum[c];
        sum.sum2[c] += p.sum2[c];
    }
}


inline void
val (ImageBufAlgo::PixelStats &p, int c, float value)
{
    if (isnan (value)) {
        ++p.nancount[c];
        return;
    }
    if (isinf (value)) {
        ++p.infcount[c];
        return;
    }
    ++p.finitecount[c];
    p.sum[c] += value;
    p.sum2[c] += value*value;
    p.min[c] = std::min (value, p.min[c]);
    p.max[c] = std::max (value, p.max[c]);
}



inline void
finalize (ImageBufAlgo::PixelStats &p)
{
    for (size_t c = 0, e = p.min.size();  c < e;  ++c) {
        if (p.finitecount[c] == 0) {
            p.min[c] = 0.0;
            p.max[c] = 0.0;
            p.avg[c] = 0.0;
            p.stddev[c] = 0.0;
        } else {
            double Count = static_cast<double>(p.finitecount[c]);
            double davg = p.sum[c] / Count;
            p.avg[c] = static_cast<float>(davg);
            p.stddev[c] = static_cast<float>(sqrt(p.sum2[c]/Count - davg*davg));
        }
    }
}



template <class T>
static bool
computePixelStats_ (const ImageBuf &src, ImageBufAlgo::PixelStats &stats,
                    ROI roi, int nthreads)
{
    if (! roi.defined())
        roi = get_roi (src.spec());
    else
        roi.chend = std::min (roi.chend, src.nchannels());

    int nchannels = src.spec().nchannels;

    // Use local storage for smaller batches, then merge the batches
    // into the final results.  This preserves precision for large
    // images, where the running total may be too big to incorporate the
    // contributions of individual pixel values without losing
    // precision.
    //
    // This approach works best when the batch size is the sqrt of
    // numpixels, which makes the num batches roughly equal to the
    // number of pixels / batch.
    ImageBufAlgo::PixelStats tmp;
    reset (tmp, nchannels);
    reset (stats, nchannels);
    
    int PIXELS_PER_BATCH = std::max (1024,
            static_cast<int>(sqrt((double)src.spec().image_pixels())));
    
    if (src.deep()) {
        // Loop over all pixels ...
        for (ImageBuf::ConstIterator<T> s(src, roi); ! s.done();  ++s) {
            int samples = s.deep_samples();
            if (! samples)
                continue;
            for (int c = roi.chbegin;  c < roi.chend;  ++c) {
                for (int i = 0;  i < samples;  ++i) {
                    float value = s.deep_value (c, i);
                    val (tmp, c, value);
                    if ((tmp.finitecount[c] % PIXELS_PER_BATCH) == 0) {
                        merge (stats, tmp);
                        reset (tmp, nchannels);
                    }
                }
            }
        }
    } else {  // Non-deep case
        // Loop over all pixels ...
        for (ImageBuf::ConstIterator<T> s(src, roi); ! s.done();  ++s) {
            for (int c = roi.chbegin;  c < roi.chend;  ++c) {
                float value = s[c];
                val (tmp, c, value);
                if ((tmp.finitecount[c] % PIXELS_PER_BATCH) == 0) {
                    merge (stats, tmp);
                    reset (tmp, nchannels);
                }
            }
        }
    }

    // Merge anything left over
    merge (stats, tmp);

    // Compute final results
    finalize (stats);
    
    return true;
};



bool
ImageBufAlgo::computePixelStats (PixelStats &stats, const ImageBuf &src,
                                 ROI roi, int nthreads)
{
    if (! roi.defined())
        roi = get_roi (src.spec());
    else
        roi.chend = std::min (roi.chend, src.nchannels());
    int nchannels = src.spec().nchannels;
    if (nchannels == 0) {
        src.error ("%d-channel images not supported", nchannels);
        return false;
    }

    OIIO_DISPATCH_TYPES ("computePixelStats", computePixelStats_,
                         src.spec().format, src, stats, roi, nthreads);
    return false;
}



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
        result.maxz = a.z();
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



template <class Atype, class Btype>
static bool
compare_ (const ImageBuf &A, const ImageBuf &B,
          float failthresh, float warnthresh,
          ImageBufAlgo::CompareResults &result,
          ROI roi, int nthreads)
{
    imagesize_t npels = roi.npixels();
    imagesize_t nvals = npels * roi.nchannels();
    int Achannels = A.nchannels(), Bchannels = B.nchannels();

    // Compare the two images.
    //
    double totalerror = 0;
    double totalsqrerror = 0;
    result.maxerror = 0;
    result.maxx=0, result.maxy=0, result.maxz=0, result.maxc=0;
    result.nfail = 0, result.nwarn = 0;
    float maxval = 1.0;  // max possible value

    ImageBuf::ConstIterator<Atype> a (A, roi, ImageBuf::WrapBlack);
    ImageBuf::ConstIterator<Btype> b (B, roi, ImageBuf::WrapBlack);
    bool deep = A.deep();
    // Break up into batches to reduce cancelation errors as the error
    // sums become too much larger than the error for individual pixels.
    const int batchsize = 4096;   // As good a guess as any
    for ( ;  ! a.done();  ) {
        double batcherror = 0;
        double batch_sqrerror = 0;
        if (deep) {
            for (int i = 0;  i < batchsize && !a.done();  ++i, ++a, ++b) {
                bool warned = false, failed = false;  // For this pixel
                for (int c = roi.chbegin;  c < roi.chend;  ++c)
                    for (int s = 0, e = a.deep_samples(); s < e;  ++s) {
                        compare_value (a, c, a.deep_value(c,s),
                                       b.deep_value(c,s), result, maxval,
                                       batcherror, batch_sqrerror,
                                       failed, warned, failthresh, warnthresh);
                    }
            }
        } else {  // non-deep
            for (int i = 0;  i < batchsize && !a.done();  ++i, ++a, ++b) {
                bool warned = false, failed = false;  // For this pixel
                for (int c = roi.chbegin;  c < roi.chend;  ++c)
                    compare_value (a, c, c < Achannels ? a[c] : 0.0f,
                                   c < Bchannels ? b[c] : 0.0f,
                                   result, maxval, batcherror, batch_sqrerror,
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



bool
ImageBufAlgo::compare (const ImageBuf &A, const ImageBuf &B,
                       float failthresh, float warnthresh,
                       ImageBufAlgo::CompareResults &result,
                       ROI roi, int nthreads)
{
    // If no ROI is defined, use the union of the data windows of the two
    // images.
    if (! roi.defined())
        roi = roi_union (get_roi(A.spec()), get_roi(B.spec()));
    roi.chend = std::min (roi.chend, std::max(A.nchannels(), B.nchannels()));

    // Deep and non-deep images cannot be compared
    if (B.deep() != A.deep())
        return false;

    OIIO_DISPATCH_TYPES2 ("compare", compare_,
                          A.spec().format, B.spec().format,
                          A, B, failthresh, warnthresh, result,
                          roi, nthreads);
    // FIXME - The nthreads argument is for symmetry with the rest of
    // ImageBufAlgo and for future expansion. But for right now, we
    // don't actually split by threads.  Maybe later.
    return false;
}



template<typename T>
static inline bool
isConstantColor_ (const ImageBuf &src, float *color,
                  ROI roi, int nthreads)
{
    // Iterate using the native typing (for speed).
    std::vector<T> constval (roi.nchannels());
    ImageBuf::ConstIterator<T,T> s (src, roi);
    for (int c = roi.chbegin;  c < roi.chend;  ++c)
        constval[c] = s[c];

    // Loop over all pixels ...
    for ( ; ! s.done();  ++s) {
        for (int c = roi.chbegin;  c < roi.chend;  ++c)
            if (constval[c] != s[c])
                return false;
    }
    
    if (color) {
        ImageBuf::ConstIterator<T,float> s (src, roi);
        for (int c = 0;  c < roi.chbegin; ++c)
            color[c] = 0.0f;
        for (int c = roi.chbegin; c < roi.chend; ++c)
            color[c] = s[c];
        for (int c = roi.chend;  c < src.nchannels(); ++c)
            color[c] = 0.0f;
    }

    return true;
}



bool
ImageBufAlgo::isConstantColor (const ImageBuf &src, float *color,
                               ROI roi, int nthreads)
{
    // If no ROI is defined, use the data window of src.
    if (! roi.defined())
        roi = get_roi(src.spec());
    roi.chend = std::min (roi.chend, src.nchannels());

    if (roi.nchannels() == 0)
        return true;
    
    OIIO_DISPATCH_TYPES ("isConstantColor", isConstantColor_,
                         src.spec().format, src, color, roi, nthreads);
    // FIXME -  The nthreads argument is for symmetry with the rest of
    // ImageBufAlgo and for future expansion. But for right now, we
    // don't actually split by threads.  Maybe later.
};



template<typename T>
static inline bool
isConstantChannel_ (const ImageBuf &src, int channel, float val,
                    ROI roi, int nthreads)
{

    T v = convert_type<float,T> (val);
    for (ImageBuf::ConstIterator<T,T> s(src, roi);  !s.done();  ++s)
        if (s[channel] != v)
            return false;
    return true;
}


bool
ImageBufAlgo::isConstantChannel (const ImageBuf &src, int channel, float val,
                                 ROI roi, int nthreads)
{
    // If no ROI is defined, use the data window of src.
    if (! roi.defined())
        roi = get_roi(src.spec());

    if (channel < 0 || channel >= src.nchannels())
        return false;  // that channel doesn't exist in the image

    OIIO_DISPATCH_TYPES ("isConstantChannel", isConstantChannel_,
                         src.spec().format, src, channel, val, roi, nthreads);
    // FIXME -  The nthreads argument is for symmetry with the rest of
    // ImageBufAlgo and for future expansion. But for right now, we
    // don't actually split by threads.  Maybe later.
};



template<typename T>
static inline bool
isMonochrome_ (const ImageBuf &src, ROI roi, int nthreads)
{
    int nchannels = src.nchannels();
    if (nchannels < 2) return true;
    
    // Loop over all pixels ...
    for (ImageBuf::ConstIterator<T,T> s(src, roi);  ! s.done();  ++s) {
        T constvalue = s[roi.chbegin];
        for (int c = roi.chbegin+1;  c < roi.chend;  ++c)
            if (s[c] != constvalue)
                return false;
    }
    return true;
}



bool
ImageBufAlgo::isMonochrome (const ImageBuf &src, ROI roi, int nthreads)
{
    // If no ROI is defined, use the data window of src.
    if (! roi.defined())
        roi = get_roi(src.spec());
    roi.chend = std::min (roi.chend, src.nchannels());
    if (roi.nchannels() < 2)
        return true;  // 1 or fewer channels are always "monochrome"

    OIIO_DISPATCH_TYPES ("isMonochrome", isMonochrome_, src.spec().format,
                         src, roi, nthreads);
    // FIXME -  The nthreads argument is for symmetry with the rest of
    // ImageBufAlgo and for future expansion. But for right now, we
    // don't actually split by threads.  Maybe later.
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
    if (blocksize <= 0 || blocksize >= roi.height())
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




template<typename DSTTYPE, typename SRCTYPE>
static bool
resize_ (ImageBuf &dst, const ImageBuf &src,
         Filter2D *filter, ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Lots of pixels and request for multi threads? Parallelize.
        ImageBufAlgo::parallel_image (
            boost::bind(resize_<DSTTYPE,SRCTYPE>, boost::ref(dst),
                        boost::cref(src), filter,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case

    const ImageSpec &srcspec (src.spec());
    const ImageSpec &dstspec (dst.spec());
    int nchannels = dstspec.nchannels;

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
    int radi = (int) ceilf (filterrad/xratio);
    int radj = (int) ceilf (filterrad/yratio);

    bool separable = filter->separable();
    float *column = NULL;
    if (separable) {
        // Allocate one column for the first horizontal filter pass
        column = ALLOCA (float, (2 * radj + 1) * nchannels);
    }

#if 0
    std::cerr << "Resizing " << srcspec.full_width << "x" << srcspec.full_height
              << " to " << dstspec.full_width << "x" << dstspec.full_height << "\n";
    std::cerr << "ratios = " << xratio << ", " << yratio << "\n";
    std::cerr << "examining src filter support radius of " << radi << " x " << radj << " pixels\n";
    std::cerr << "dst range " << roi << "\n";
    std::cerr << "separable filter\n";
#endif

    ImageBuf::Iterator<DSTTYPE> out (dst, roi);
    for (int y = roi.ybegin;  y < roi.yend;  ++y) {
        // s,t are NDC space
        float t = (y+0.5f)*dstpixelheight;
        // src_xf, src_xf are image space float coordinates
        float src_yf = srcfy + t * srcfh - 0.5f;
        // src_x, src_y are image space integer coordinates of the floor
        int src_y;
        float src_yf_frac = floorfrac (src_yf, &src_y);
        for (int x = roi.xbegin;  x < roi.xend;  ++x) {
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
                    int yy = src_y+j;
                    ImageBuf::ConstIterator<SRCTYPE> srcpel (src, src_x-radi, src_x+radi+1,
                                                             yy, yy+1, 0, 1);
                    for (int i = -radi;  i <= radi;  ++i, ++srcpel) {
                        float w = filter->xfilt (xratio * (i-src_xf_frac));
                        if (w != 0.0f && srcpel.exists()) {
                            for (int c = 0;  c < nchannels;  ++c)
                                p[c] += w * srcpel[c];
                            totalweight += w;
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
                    int yy = src_y+j;
                    if (yy >= src.ymin() && yy <= src.ymax()) {
                        float w = filter->yfilt (yratio * (j-src_yf_frac));
                        totalweight += w;
                        for (int c = 0;  c < nchannels;  ++c)
                            pel[c] += w * p[c];
                    }
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
                        DASSERT (! srcpel.done());
                        if (srcpel.exists()) {
                            for (int c = 0;  c < nchannels;  ++c)
                                pel[c] += w * srcpel[c];
                            totalweight += w;
                        }
                    }
                }
                DASSERT (srcpel.done());
            }

            // Rescale pel to normalize the filter, then write it to the
            // image.
            DASSERT (out.x() == x && out.y() == y);
            if (totalweight == 0.0f) {
                // zero it out
                for (int c = 0;  c < nchannels;  ++c)
                    out[c] = 0.0f;
            } else {
                for (int c = 0;  c < nchannels;  ++c)
                    out[c] = pel[c] / totalweight;
            }
            ++out;
        }
    }

    return true;
}



bool
ImageBufAlgo::resize (ImageBuf &dst, const ImageBuf &src,
                      Filter2D *filter, ROI roi, int nthreads)
{
    IBAprep (roi, &dst, &src);
    if (dst.nchannels() != src.nchannels()) {
        dst.error ("channel number mismatch: %d vs. %d", 
                   dst.spec().nchannels, src.spec().nchannels);
        return false;
    }

    // Set up a shared pointer with custom deleter to make sure any
    // filter we allocate here is properly destroyed.
    boost::shared_ptr<Filter2D> filterptr ((Filter2D*)NULL, Filter2D::destroy);
    bool allocfilter = (filter == NULL);
    if (allocfilter) {
        // If no filter was provided, punt and just linearly interpolate.
        const ImageSpec &srcspec (src.spec());
        const ImageSpec &dstspec (dst.spec());
        float wratio = float(dstspec.full_width) / float(srcspec.full_width);
        float hratio = float(dstspec.full_height) / float(srcspec.full_height);
        float w = 2.0f * std::max (1.0f, wratio);
        float h = 2.0f * std::max (1.0f, hratio);
        filter = Filter2D::create ("triangle", w, h);
        filterptr.reset (filter);
    }

    OIIO_DISPATCH_TYPES2 ("resize", resize_,
                          dst.spec().format, src.spec().format,
                          dst, src, filter, roi, nthreads);

    return false;
}



// DEPRECATED as of 1.2
bool
ImageBufAlgo::resize (ImageBuf &dst, const ImageBuf &src,
                      int xbegin, int xend, int ybegin, int yend,
                      Filter2D *filter)
{
    return resize (dst, src, filter, ROI (xbegin, xend, ybegin, yend, 0, 1));
}



template<typename DSTTYPE, typename SRCTYPE>
static bool
resample_ (ImageBuf &dst, const ImageBuf &src, bool interpolate,
           ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Lots of pixels and request for multi threads? Parallelize.
        ImageBufAlgo::parallel_image (
            boost::bind(resample_<DSTTYPE,SRCTYPE>, boost::ref(dst),
                        boost::cref(src), interpolate,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case

    const ImageSpec &srcspec (src.spec());
    const ImageSpec &dstspec (dst.spec());

    // Local copies of the source image window, converted to float
    float srcfx = srcspec.full_x;
    float srcfy = srcspec.full_y;
    float srcfw = srcspec.full_width;
    float srcfh = srcspec.full_height;

    float dstpixelwidth = 1.0f / (float)dstspec.full_width;
    float dstpixelheight = 1.0f / (float)dstspec.full_height;
    int nchannels = src.nchannels();
    float *pel = ALLOCA (float, nchannels);

    ImageBuf::Iterator<DSTTYPE> out (dst, roi);
    ImageBuf::ConstIterator<SRCTYPE> srcpel (src);
    for (int y = roi.ybegin;  y < roi.yend;  ++y) {
        // s,t are NDC space
        float t = (y+0.5f)*dstpixelheight;
        // src_xf, src_xf are image space float coordinates
        float src_yf = srcfy + t * srcfh - 0.5f;
        // src_x, src_y are image space integer coordinates of the floor
        int src_y;
        (void) floorfrac (src_yf, &src_y);
        for (int x = roi.xbegin;  x < roi.xend;  ++x) {
            float s = (x+0.5f)*dstpixelwidth;
            float src_xf = srcfx + s * srcfw - 0.5f;
            int src_x;
            (void) floorfrac (src_xf, &src_x);

            if (interpolate) {
                src.interppixel (src_xf, src_yf, pel);
                for (int c = roi.chbegin; c < roi.chend; ++c)
                    out[c] = pel[c];
            } else {
                srcpel.pos (src_x, src_y, 0);
                for (int c = roi.chbegin; c < roi.chend; ++c)
                    out[c] = srcpel[c];
            }
            ++out;
        }
    }

    return true;
}



bool
ImageBufAlgo::resample (ImageBuf &dst, const ImageBuf &src,
                        bool interpolate, ROI roi, int nthreads)
{
    IBAprep (roi, &dst, &src);
    if (dst.nchannels() != src.nchannels()) {
        dst.error ("channel number mismatch: %d vs. %d", 
                   dst.spec().nchannels, src.spec().nchannels);
        return false;
    }
    OIIO_DISPATCH_TYPES2 ("resample", resample_,
                          dst.spec().format, src.spec().format,
                          dst, src, interpolate, roi, nthreads);
    return false;
}



template<typename DSTTYPE, typename SRCTYPE>
static bool
convolve_ (ImageBuf &dst, const ImageBuf &src, const ImageBuf &kernel,
           bool normalize, ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Lots of pixels and request for multi threads? Parallelize.
        ImageBufAlgo::parallel_image (
            boost::bind(convolve_<DSTTYPE,SRCTYPE>, boost::ref(dst),
                        boost::cref(src), boost::cref(kernel), normalize,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case

    float scale = 1.0f;
    if (normalize) {
        scale = 0.0f;
        for (ImageBuf::ConstIterator<float> k (kernel); ! k.done(); ++k)
            scale += k[0];
        scale = 1.0f / scale;
    }

    float *sum = ALLOCA (float, roi.chend);
    ROI kroi = get_roi (kernel.spec());
    ImageBuf::Iterator<DSTTYPE> d (dst, roi);
    ImageBuf::ConstIterator<DSTTYPE> s (src, roi, ImageBuf::WrapClamp);
    for ( ; ! d.done();  ++d) {

        for (int c = roi.chbegin; c < roi.chend; ++c)
            sum[c] = 0.0f;

        for (ImageBuf::ConstIterator<float> k (kernel, kroi); !k.done(); ++k) {
            float kval = k[0];
            s.pos (d.x() + k.x(), d.y() + k.y(), d.z() + k.z());
            for (int c = roi.chbegin; c < roi.chend; ++c)
                sum[c] += kval * s[c];
        }
        
        for (int c = roi.chbegin; c < roi.chend; ++c)
            d[c] = scale * sum[c];
    }

    return true;
}



bool
ImageBufAlgo::convolve (ImageBuf &dst, const ImageBuf &src,
                        const ImageBuf &kernel, bool normalize,
                        ROI roi, int nthreads)
{
    IBAprep (roi, &dst, &src);
    if (dst.nchannels() != src.nchannels()) {
        dst.error ("channel number mismatch: %d vs. %d", 
                   dst.spec().nchannels, src.spec().nchannels);
        return false;
    }
    OIIO_DISPATCH_TYPES2 ("convolve", convolve_,
                          dst.spec().format, src.spec().format,
                          dst, src, kernel, normalize, roi, nthreads);
    return false;
}



bool
ImageBufAlgo::make_kernel (ImageBuf &dst, const char *name,
                           float width, float height, bool normalize)
{
    int w = std::max (1, (int)ceilf(width));
    int h = std::max (1, (int)ceilf(height));
    // Round up size to odd
    if (! (w & 1))
        ++w;
    if (! (h & 1))
        ++h;
    ImageSpec spec (w, h, 1 /*channels*/, TypeDesc::FLOAT);
    spec.x = -w/2;
    spec.y = -h/2;
    spec.full_x = spec.x;
    spec.full_y = spec.y;
    spec.full_width = spec.width;
    spec.full_height = spec.height;
    dst.alloc (spec);

    boost::shared_ptr<Filter2D> filter ((Filter2D*)NULL, Filter2D::destroy);
    filter.reset (Filter2D::create (name, width, height));
    if (filter.get()) {
        float sum = 0;
        for (ImageBuf::Iterator<float> p (dst);  ! p.done();  ++p) {
            float val = (*filter)((float)p.x(), (float)p.y());
            p[0] = val;
            sum += val;
        }
        for (ImageBuf::Iterator<float> p (dst);  ! p.done();  ++p)
            p[0] = p[0] / sum;
    } else {
        // No filter -- make a box
        float val = normalize ? 1.0f / ((w*h)) : 1.0f;
        for (ImageBuf::Iterator<float> p (dst);  ! p.done();  ++p)
            p[0] = val;
        dst.error ("Unknown kernel \"%s\"", name);
        return false;
    }
    return true;
}



// Helper function for unsharp mask to perform the thresholding
static bool
threshold_to_zero (ImageBuf &dst, float threshold,
                   ROI roi, int nthreads)
{
    ASSERT (dst.spec().format.basetype == TypeDesc::FLOAT);

    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Lots of pixels and request for multi threads? Parallelize.
        ImageBufAlgo::parallel_image (
            boost::bind(threshold_to_zero, boost::ref(dst), threshold,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
    for (ImageBuf::Iterator<float> p (dst, roi);  ! p.done();  ++p)
        for (int c = roi.chbegin;  c < roi.chend;  ++c)
            if (fabsf(p[c]) < threshold)
                p[c] = 0.0f;

    return true;
}



bool
ImageBufAlgo::unsharp_mask (ImageBuf &dst, const ImageBuf &src,
                            const char *kernel, float width,
                            float contrast, float threshold,
                            ROI roi, int nthreads)
{
    IBAprep (roi, &dst, &src);
    if (dst.nchannels() != src.nchannels()) {
        dst.error ("channel number mismatch: %d vs. %d", 
                   dst.spec().nchannels, src.spec().nchannels);
        return false;
    }

    // Blur the source image, store in Blurry
    ImageBuf K ("kernel");
    if (! make_kernel (K, kernel, width, width)) {
        dst.error ("%s", K.geterror());
        return false;
    }
    ImageSpec BlurrySpec = src.spec();
    BlurrySpec.set_format (TypeDesc::FLOAT);  // force float
    ImageBuf Blurry ("blurry", BlurrySpec);
    if (! convolve (Blurry, src, K, true, roi, nthreads)) {
        dst.error ("%s", Blurry.geterror());
        return false;
    }

    // Compute the difference between the source image and the blurry
    // version.  (We store it in the same buffer we used for the difference
    // image.)
    ImageBuf &Diff (Blurry);
    bool ok = sub (Diff, src, Blurry, roi, nthreads);

    if (ok && threshold > 0.0f)
        ok = threshold_to_zero (Diff, threshold, roi, nthreads);

    // Scale the difference image by the contrast
    if (ok)
        ok = mul (Diff, contrast, roi, nthreads);
    if (! ok) {
        dst.error ("%s", Diff.geterror());
        return false;
    }

    // Add the scaled difference to the original, to get the final answer
    ok = add (dst, src, Diff, roi, nthreads);

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
    if (mode != ImageBufAlgo::NONFINITE_NONE &&
        mode != ImageBufAlgo::NONFINITE_BLACK &&
        mode != ImageBufAlgo::NONFINITE_BOX3) {
        // Something went wrong
        dst.error ("fixNonFinite: unknown repair mode");
        return false;
    }

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
ImageBufAlgo::fixNonFinite (ImageBuf &src, 
                            NonFiniteFixMode mode, int *pixelsFixed,
                            ROI roi, int nthreads)
{
    // If no ROI is defined, use the data window of src.
    if (! roi.defined())
        roi = get_roi(src.spec());
    roi.chend = std::min (roi.chend, src.nchannels());

    // Initialize
    if (pixelsFixed)
        *pixelsFixed = 0;

    switch (src.spec().format.basetype) {
    case TypeDesc::FLOAT :
        return fixNonFinite_<float> (src, mode, pixelsFixed, roi, nthreads);
    case TypeDesc::HALF  :
        return fixNonFinite_<half> (src, mode, pixelsFixed, roi, nthreads);
    case TypeDesc::DOUBLE:
        return fixNonFinite_<double> (src, mode, pixelsFixed, roi, nthreads);
    default:
        // All other format types aren't capable of having nonfinite
        // pixel values.
        return true;
    }
}



// DEPRECATED 2-argument version
bool
ImageBufAlgo::fixNonFinite (ImageBuf &dst, const ImageBuf &src,
                            NonFiniteFixMode mode, int *pixelsFixed)
{
    ROI roi;
    IBAprep (roi, &dst, &src);
    if (dst.nchannels() != src.nchannels()) {
        dst.error ("channel number mismatch: %d vs. %d", 
                   dst.spec().nchannels, src.spec().nchannels);
        return false;
    }
    if ((const ImageBuf *)&dst != &src)
        if (! dst.copy (src))
            return false;
    return fixNonFinite (dst, mode, pixelsFixed, roi);
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



// Helper for fillholes_pp: for any nonzero alpha pixels in dst, divide
// all components by alpha.
static bool
divide_by_alpha (ImageBuf &dst, ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Lots of pixels and request for multi threads? Parallelize.
        ImageBufAlgo::parallel_image (
            boost::bind(divide_by_alpha, boost::ref(dst),
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
    const ImageSpec &spec (dst.spec());
    ASSERT (spec.format == TypeDesc::FLOAT);
    int nc = spec.nchannels;
    int ac = spec.alpha_channel;
    for (ImageBuf::Iterator<float> d (dst, roi);  ! d.done();  ++d) {
        float alpha = d[ac];
        if (alpha != 0.0f) {
            for (int c = 0; c < nc; ++c)
                d[c] = d[c] / alpha;
        }
    }
    return true;
}



bool
ImageBufAlgo::fillholes_pushpull (ImageBuf &dst, const ImageBuf &src,
                                  ROI roi, int nthreads)
{
    IBAprep (roi, &dst, &src);
    const ImageSpec &dstspec (dst.spec());
    if (dstspec.nchannels != src.nchannels()) {
        dst.error ("channel number mismatch: %d vs. %d", 
                   dstspec.nchannels, src.spec().nchannels);
        return false;
    }
    if (dstspec.alpha_channel < 0 ||
        dstspec.alpha_channel != src.spec().alpha_channel) {
        dst.error ("Must have alpha channels");
        return false;
    }

    // We generate a bunch of temp images to form an image pyramid.
    // These give us a place to stash them and make sure they are
    // auto-deleted when the function exits.
    std::vector<boost::shared_ptr<ImageBuf> > pyramid;

    // First, make a writeable copy of the original image (converting
    // to float as a convenience) as the top level of the pyramid.
    ImageSpec topspec = src.spec();
    topspec.set_format (TypeDesc::FLOAT);
    ImageBuf *top = new ImageBuf ("top.exr", topspec);
    paste (*top, 0, 0, 0, 0, src);
    pyramid.push_back (boost::shared_ptr<ImageBuf>(top));

    // Construct the rest of the pyramid by successive x/2 resizing and
    // then dividing nonzero alpha pixels by their alpha (this "spreads
    // out" the defined part of the image).
    int w = src.spec().width, h = src.spec().height;
    while (w > 1 || h > 1) {
        w = std::max (1, w/2);
        h = std::max (1, h/2);
        ImageSpec smallspec (w, h, src.nchannels(), TypeDesc::FLOAT);
        std::string name = Strutil::format ("small%d.exr", (int)pyramid.size());
        ImageBuf *small = new ImageBuf (name, smallspec);
        ImageBufAlgo::resize (*small, *pyramid.back());
        divide_by_alpha (*small, get_roi(smallspec), nthreads);
        pyramid.push_back (boost::shared_ptr<ImageBuf>(small));
        //debug small->save();
    }

    // Now pull back up the pyramid by doing an alpha composite of level
    // i over a resized level i+1, thus filling in the alpha holes.  By
    // time we get to the top, pixels whose original alpha are
    // unchanged, those with alpha < 1 are replaced by the blended
    // colors of the higher pyramid levels.
    for (int i = (int)pyramid.size()-2;  i >= 0;  --i) {
        ImageBuf &big(*pyramid[i]), &small(*pyramid[i+1]);
        ImageBuf blowup ("bigger", big.spec());
        ImageBufAlgo::resize (blowup, small);
        ImageBufAlgo::over (big, big, blowup);
        //debug big.save (Strutil::format ("after%d.exr", i));
    }

    // Now copy the completed base layer of the pyramid back to the
    // original requested output.
    paste (dst, dstspec.x, dstspec.y, dstspec.z, 0, *pyramid[0]);

    return true;
}


}
OIIO_NAMESPACE_EXIT
