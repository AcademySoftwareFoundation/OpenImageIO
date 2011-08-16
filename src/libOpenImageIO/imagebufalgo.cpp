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

#include <OpenEXR/ImathFun.h>
#include <OpenEXR/half.h>

#include <iostream>
#include <limits>
#include <stdexcept>

#include "imagebuf.h"
#include "imagebufalgo.h"
#include "dassert.h"
#include "sysutil.h"
#include "filter.h"


OIIO_NAMESPACE_ENTER
{

namespace
{

template<typename T>
static inline void
zero_ (ImageBuf &buf)
{
    int chans = buf.nchannels();
    for (ImageBuf::Iterator<T> pixel (buf);  pixel.valid();  ++pixel)
        for (int i = 0;  i < chans;  ++i)
            pixel[i] = 0;
}

}

bool
ImageBufAlgo::zero (ImageBuf &dst)
{
    switch (dst.spec().format.basetype) {
    case TypeDesc::FLOAT : zero_<float> (dst); break;
    case TypeDesc::UINT8 : zero_<unsigned char> (dst); break;
    case TypeDesc::INT8  : zero_<char> (dst); break;
    case TypeDesc::UINT16: zero_<unsigned short> (dst); break;
    case TypeDesc::INT16 : zero_<short> (dst); break;
    case TypeDesc::UINT  : zero_<unsigned int> (dst); break;
    case TypeDesc::INT   : zero_<int> (dst); break;
    case TypeDesc::UINT64: zero_<unsigned long long> (dst); break;
    case TypeDesc::INT64 : zero_<long long> (dst); break;
    case TypeDesc::HALF  : zero_<half> (dst); break;
    case TypeDesc::DOUBLE: zero_<double> (dst); break;
    default:
        return false;
    }
    
    return true;
}


bool
ImageBufAlgo::fill (ImageBuf &dst,
                    const float *pixel)
{
    // Walk through all data in our buffer. (i.e., crop or overscan)
    // The display window is irrelevant
    for (int k = dst.spec().z; k < dst.spec().z+dst.spec().depth; k++)
        for (int j = dst.spec().y; j <  dst.spec().y+dst.spec().height; j++)
            for (int i = dst.spec().x; i < dst.spec().x+dst.spec().width ; i++)
                dst.setpixel (i, j, pixel);
    
    return true;
}


/// return true on success.
bool
ImageBufAlgo::fill (ImageBuf &dst,
                    const float *pixel,
                    int xbegin, int xend,
                    int ybegin, int yend)
{
    if (xbegin >= xend) {
        return false;
    }
    if (ybegin >= yend) {
        return false;
    }
    
    for (int j = ybegin; j < yend; j++)
        for (int i = xbegin; i < xend; i++)
            dst.setpixel (i, j, pixel);
    
    return true;
}


bool
ImageBufAlgo::fill (ImageBuf &dst,
                    const float *pixel,
                    int xbegin, int xend,
                    int ybegin, int yend,
                    int zbegin, int zend)
{

    if (xbegin >= xend) {
        return false;
    }
    if (ybegin >= yend) {
        return false;
    }
    if (zbegin >= zend) {
        return false;
    }
    
    for (int k = zbegin; k < zend; k++)
        for (int j = ybegin; j < yend; j++)
            for (int i = xbegin; i < xend; i++)
                dst.setpixel (i, j, k, pixel);
    
    return true;
}




bool 
ImageBufAlgo::crop (ImageBuf &dst, const ImageBuf &src,
                    int xbegin, int xend, int ybegin, int yend, int options) 
{
    const ImageSpec &src_spec (src.spec());
    
    //check input
    if (xbegin >= xend){
        std::cerr << "crop ERROR: xbegin should be smaller than xend \n" ;
        return false;
    }
    if (ybegin >= yend){
        std::cerr << "crop ERROR: ybegin should be smaller than yend \n" ;
        return false;
    }
    if (xbegin < 0 || xend > src_spec.full_width) {
        std::cerr << "crop ERROR: x values are out of image bounds \n" ;
        return false;
    }
    if (options == CROP_TRANS && src_spec.alpha_channel == -1) {
        std::cerr << "crop ERROR: no alpha channel present \n";
        return false;
    }		
    //manipulate the images
    
    ImageSpec dst_spec = src_spec;		
    switch (options) {
    case CROP_WINDOW:
        //mark the window
        dst_spec.x = xbegin;
	dst_spec.y = ybegin;
	dst_spec.width = xend-xbegin;
	dst_spec.height = yend-ybegin;
	break;	
    case CROP_BLACK:
    case CROP_WHITE:
    case CROP_TRANS:
	//do nothing, all meta data remains the same
	break;
    case CROP_CUT:
	dst_spec.x = 0;
	dst_spec.y = 0;
	dst_spec.width = xend-xbegin;
	dst_spec.height = yend-ybegin;
	dst_spec.full_width = dst_spec.width;
	dst_spec.full_height = dst_spec.height;
	break;
    }
    
    // create new ImageBuffer
    if (!dst.pixels_valid())
        dst.alloc (dst_spec);
    //copy the outer pixel  
    float *pixel = (float *) alloca (src.nchannels()*sizeof(float)); 
    if (options != CROP_WINDOW) {
        switch(options) {
        case CROP_BLACK:
            for (int k=0; k<src.nchannels(); k++)
                if (k != src_spec.alpha_channel)
                    pixel[k] = 0;
                else
                    pixel[k] = 1;
            break;
        case CROP_WHITE:
            for (int k=0; k<src.nchannels(); k++)
                pixel[k]=1;
            break;
        case CROP_TRANS:
            for (int k=0; k<src.nchannels(); k++)
                pixel[k]=0;
	    break;
        }
        fill (dst, pixel);
    }
    //copy the cropping area pixel
    switch(options)
    {
    case CROP_WINDOW:
    case CROP_BLACK:
    case CROP_WHITE:
    case CROP_TRANS:
	//all the data is copied
	for (int j=ybegin; j<yend; j++)
            for (int i=xbegin; i<xend; i++) {
                src.getpixel (i, j, pixel);
                dst.setpixel (i, j, pixel);
	    }
	break;
    case CROP_CUT:
	for (int j=ybegin; j<yend; j++)
            for (int i=xbegin; i<xend; i++) {
                src.getpixel (i, j, pixel);
                dst.setpixel (i-xbegin, j-ybegin, pixel);
	    }
	break;
    }
    return true;
}
    
    
    



bool
ImageBufAlgo::setNumChannels(ImageBuf &dst, const ImageBuf &src, int numChannels)
{
    // Not intended to create 0-channel images.
    if (numChannels <= 0)
        return false;
    // If we dont have a single source channel,
    // hard to know how big to make the additional channels
    if (src.spec().nchannels == 0)
        return false;
    
    if (numChannels == src.spec().nchannels) {
        dst = src;
        return true;
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
        return false;
    }
    
    // all three images must have the same number of channels
    if (A.spec().nchannels != B.spec().nchannels) {
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
ImageBufAlgo::colortransfer (ImageBuf &output, const ImageBuf &input,
                             ColorTransfer *tfunc)
{
    // copy input ImageBuf to output ImageBuf if they aren't the same.
    if (&output != &input)
        output = input;

    // exit if the transfer function is NULL
    if (tfunc == NULL)
        return true;

    // run the transfer function over the output ImageBuf
    output.transfer_pixels (tfunc);
    
    return true;
}



bool
ImageBufAlgo::computePixelStats (PixelStats  &stats, const ImageBuf &src)
{
    int nchannels = src.spec().nchannels;
    if (nchannels == 0)
        return false;

    if (src.spec().format != TypeDesc::FLOAT)
        return false;
    
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
    
    ImageBuf::ConstIterator<float> s (src);
    
    float value = 0.0;
    int c = 0;
    
    // Loop over all pixels ...
    for ( ; s.valid();  ++s) {
        for (c = 0;  c < nchannels;  ++c) {
            value = s[c];
            
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
    
    // Store results
    stats.min.resize (nchannels);
    stats.max.resize (nchannels);
    stats.avg.resize (nchannels);
    stats.stddev.resize (nchannels);
    stats.nancount.resize (nchannels);
    stats.infcount.resize (nchannels);
    stats.finitecount.resize (nchannels);
    
    for (c = 0;  c < nchannels;  ++c) {
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



bool
ImageBufAlgo::compare (const ImageBuf &A, const ImageBuf &B,
                       float failthresh, float warnthresh,
                       ImageBufAlgo::CompareResults &result)
{
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
    ASSERT (A.spec().format == TypeDesc::FLOAT &&
            B.spec().format == TypeDesc::FLOAT);
    ImageBuf::ConstIterator<float,float> a (A);
    ImageBuf::ConstIterator<float,float> b (B);
    // Break up into batches to reduce cancelation errors as the error
    // sums become too much larger than the error for individual pixels.
    const int batchsize = 4096;   // As good a guess as any
    for ( ;  a.valid();  ) {
        double batcherror = 0;
        double batch_sqrerror = 0;
        for (int i = 0;  i < batchsize && a.valid();  ++i, ++a) {
            b.pos (a.x(), a.y());  // ensure alignment
            bool warned = false, failed = false;  // For this pixel
            for (int c = 0;  c < A.spec().nchannels;  ++c) {
                float aval = a[c], bval = b[c];
                maxval = std::max (maxval, std::max (aval, bval));
                double f = fabs (aval - bval);
                batcherror += f;
                batch_sqrerror += f*f;
                if (f > result.maxerror) {
                    result.maxerror = f;
                    result.maxx = a.x();
                    result.maxy = a.y();
                    result.maxz = 0;  // FIXME -- doesn't work for volume images
                    result.maxc = c;
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
    
    bool firstpixel = true;
    int c = 0;
    
    // Iterate using the native typing (for speed).
    ImageBuf::ConstIterator<T,T> s (src);
    std::vector<T> constval (src.spec().nchannels);
    
    // Loop over all pixels ...
    for ( ; s.valid ();  ++s) {
        if(firstpixel) {
            for (c = 0;  c < nchannels;  ++c) {
                constval[c] = s[c];
            }
            if(color) {
                src.getpixel (s.x(), s.y(), s.z(), color);
            }
            firstpixel = false;
        } else {
            for (c = 0;  c < nchannels;  ++c) {
                if(constval[c]!=s[c]) {
                    return false;
                }
            }
        }
    }
    
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
    
    int c = 0;
    T constvalue;
    ImageBuf::ConstIterator<T,T> s (src);
    
    // Loop over all pixels ...
    for ( ; s.valid();  ++s) {
        constvalue = s[0];
        for (c = 1;  c < nchannels;  ++c) {
            if(s[c] != constvalue) {
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
    for (int y = src.ymin();  y <= src.ymax();  ++y) {
        src.copy_pixels (src.xbegin(), src.xend(), y, y+1,
                         src.spec().format, &tmp[0]);
        sha.Update (&tmp[0], (unsigned int) scanline_bytes);
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

    if ( //dstspec.format.basetype != TypeDesc::FLOAT ||
        nchannels != srcspec.nchannels) {
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
                        if (fabsf(w) < 1.0e-6)
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
                    if (fabsf(totalweight) >= 1.0e-6) {
                        float winv = 1.0f / totalweight;
                        for (int c = 0;  c < nchannels;  ++c)
                            p[c] *= winv;
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
                        if (fabsf(w) < 1.0e-6)
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
            if (fabsf(totalweight) < 1.0e-6) {
                // zero it out
                for (int c = 0;  c < nchannels;  ++c)
                    pel[c] = 0.0f;
            } else {
                float winv = 1.0f / totalweight;
                for (int c = 0;  c < nchannels;  ++c)
                    pel[c] *= winv;
            }
            dst.setpixel (x, y, pel);
        }
    }

    if (allocfilter)
        Filter2D::destroy (filter);
    return true;
}



template<typename SRCTYPE>
bool transform_ (ImageBuf &dst, const ImageBuf &src,
                 const ImageBufAlgo::Mapping &mapping,
                 Filter2D *filter, float xshift, float yshift)
{
    const ImageSpec &srcspec (src.spec());
    const ImageSpec &dstspec (dst.spec());
    int nchannels = dstspec.nchannels;

    std::cout << "starting transform\n";

    if (//dstspec.format.basetype != TypeDesc::FLOAT ||
        nchannels != srcspec.nchannels) {
        std::cout << "fail\n";
        return false;
    }

    bool allocfilter = (filter == NULL);
    if (allocfilter) {
        filter = Filter2D::create ("triangle", 2.0f, 2.0f);
    }
    
    if(mapping.isDstToSrcMapping)
    {
        float *pel = ALLOCA (float, nchannels);
        float filterrad = filter->width() / 2.0f;
        // radi,radj is the filter radius, as an integer, in source pixels.  We
        // will filter the source over [x-radi, x+radi] X [y-radj,y+radj].
        int radi = 0;
        int radj = 0;

        bool separable = filter->separable();

        for (int y = 0;  y < dstspec.full_height;  ++y)
        {
            
            for (int x = 0;  x < dstspec.full_width;  ++x)
            {
                // s,t are NDC space
                float s = 0, t = 0, dsdx = 1.0f, dtdx = 0, dsdy = 0, dtdy = 1.0;

                mapping.map (x-xshift, y-yshift, &s, &t, &dsdx, &dtdx, &dsdy, &dtdy);

                radi = (int) ceilf (filterrad * dsdx) + 1;
                radj = (int) ceilf (filterrad * dtdy) + 1;

                float src_yf = t;  // src_xf, src_xf are image space float coordinates
                int src_y;         // src_x, src_y are image space integer coordinates of the floor
                float src_yf_frac = floorfrac (src_yf, &src_y);

                float src_xf = s;
                int src_x;
                float src_xf_frac = floorfrac (src_xf, &src_x);

                for (int c = 0;  c < nchannels;  ++c)
                    pel[c] = 0.0f;

                float totalweight = 0.0f;

                if (separable)
                {
                    ImageBuf::ConstIterator<SRCTYPE> srcpel (src, src_x-radi, src_x+radi+1,
                                                           src_y-radi, src_y+radi+1,
                                                           0, 1, true);
                    for (int j = -radj;  j <= radj;  ++j)
                    {
                        for (int i = -radi;  i <= radi;  ++i, ++srcpel)
                        {
                        float w = filter->xfilt ( ((i-src_xf_frac) - (j-src_yf_frac) * dsdy) / dsdx )
                                   * filter->yfilt ( ((j-src_yf_frac) - (i-src_xf_frac) * dtdx) / dtdy )
                            ;
                            
                        if (fabsf(w) < 1.0e-6)
                            continue;
                                 
                            totalweight += w;
                            if (srcpel.exists())
                            {
                                for (int c = 0;  c < nchannels;  ++c)
                                    pel[c] += w * srcpel[c];
                            } else {
                                // Outside data window
                                //for (int c = 0;  c < nchannels;  ++c)
                                //    p[c] += 0;
                            }
                        }
                    }
                } else {
                    // Non-separable
                    ImageBuf::ConstIterator<SRCTYPE> srcpel (src, src_x-radi, src_x+radi+1,
                                                           src_y-radi, src_y+radi+1,
                                                           0, 1, true);
                    for (int j = -radj;  j <= radj;  ++j)
                    {
                        for (int i = -radi;  i <= radi;  ++i, ++srcpel)
                        {
                        float w = (*filter)(
                                        ((i-src_xf_frac) - (j-src_yf_frac) * dsdy) / dsdx,
                                        ((j-src_yf_frac) - (i-src_xf_frac) * dtdx) / dtdy
                                  );
                            if (fabsf(w) < 1.0e-6)
                                continue;
                            totalweight += w;
                            DASSERT (! srcpel.done());
                            if (srcpel.exists())
                            {
                                for (int c = 0;  c < nchannels;  ++c)
                                    pel[c] += w * srcpel[c];
                            }
                        }
                    }
                    DASSERT (srcpel.done());
                }

                // Rescale pel to normalize the filter, then write it to the
                // image.
                if (fabsf(totalweight) < 1.0e-6)
                {
                    // zero it out
                    for (int c = 0;  c < nchannels;  ++c)
                        pel[c] = 0.0f;
                } else {
                    float winv = 1.0f / totalweight;
                    for (int c = 0;  c < nchannels;  ++c)
                        pel[c] *= winv;
                }
                dst.setpixel (x, y, pel);
            }
        }
    }
    else //isDstToSrcMapping == false
    {
        float *srcpel = ALLOCA (float, nchannels);
        float *oldpel = ALLOCA (float, nchannels);
        float *newpel = ALLOCA (float, nchannels);
        float **w;
        
        int out_width = srcspec.full_width;
        int out_height = srcspec.full_height;
        
        w = new float*[out_height]; //creating table of pointers
        w[0] = new float[out_width*out_height]; //elements
        for(int i=1; i<out_width; i++) 
            w[i] = w[i-1] + out_height;
        
        for (int y = 0;  y < out_height;  ++y) 
            for (int x = 0;  x < out_width;  ++x)
                w[x][y] = 0;
        
        
        bool separable = filter->separable();
        float filterrad = filter->width() / 2.0f;
        
        int radi = 0;
        int radj = 0;
        int lap = (int) ceilf (filterrad) + 1;
        
        for (int y = -lap;  y < srcspec.full_height + lap;  ++y)
        {  
            for (int x = -lap;  x < srcspec.full_width + lap;  ++x)
            {
                // s,t are NDC space
                float s = 0, t = 0, dsdx = 1.0f, dtdx = 0, dsdy = 0, dtdy = 1.0;
                mapping.map (x-xshift, y-yshift, &s, &t, &dsdx, &dtdx, &dsdy, &dtdy);
                
                radi = (int) ceilf (filterrad * dsdx) + 1;
                radj = (int) ceilf (filterrad * dtdy) + 1;

                float dst_yf = t;  // src_xf, src_xf are image space float coordinates
                int dst_y;         // src_x, src_y are image space integer coordinates of the floor
                float dst_yf_frac = floorfrac (dst_yf, &dst_y);

                float dst_xf = s;
                int dst_x;
                float dst_xf_frac = floorfrac (dst_xf, &dst_x);
                
                src.getpixel(x, y, srcpel);
               
                
                for (int j = -radj;  j <= radj;  ++j)
                {
                    for (int i = -radi;  i <= radi;  ++i)
                    {   
                        if(dst_x+i < 0 || dst_x+i >= out_width || dst_y+j < 0 || dst_y+j >= out_height)
                            continue;
                        
                        float curw = 0;

                        if(separable) {
                            curw = filter->xfilt ( ((i-dst_xf_frac) - (j-dst_yf_frac) * dsdy) / dsdx) *
                                   filter->yfilt ( ((j-dst_yf_frac) - (i-dst_xf_frac) * dtdx) / dtdy);
                        } else {
                            curw = (*filter)(
                                        ((i-dst_xf_frac) - (j-dst_yf_frac) * dsdy) / dsdx,
                                        ((j-dst_yf_frac) - (i-dst_xf_frac) * dtdx) / dtdy
                                  );
                        }
                        if (fabsf(curw) < 1.0e-6)
                            continue;

                        float oldw = w[dst_x+i][dst_y+j];
                        dst.getpixel(dst_x+i, dst_y+j, oldpel);

                        /*
                        if(dst_x == 400 && dst_y == 400)
                            std::cout << j << " " << i <<
                                " curw " << curw <<
                                " oldw " << oldw <<
                                std::endl;
                        */
                        for (int c = 0;  c < nchannels;  ++c)
                                newpel[c] = (oldw * oldpel[c] + curw * srcpel[c]) / (oldw + curw);

                        dst.setpixel(dst_x+i, dst_y+j, newpel);
                        w[dst_x+i][dst_y+j] = curw + oldw;

                    }
                }                  
                             
            }
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
        std::cout << "aaaaa\n";
        return false;
    }
}



bool
ImageBufAlgo::transform (ImageBuf &dst, const ImageBuf &src,
                         const Mapping &mapping,
                         Filter2D *filter, float xshift, float yshift)
{
    switch (src.spec().format.basetype) {
    case TypeDesc::FLOAT :
        return transform_<float> (dst, src, mapping, filter, xshift, yshift);
    case TypeDesc::UINT8 :
        return transform_<unsigned char> (dst, src, mapping, filter, xshift, yshift);
    case TypeDesc::INT8  :
        return transform_<char> (dst, src, mapping, filter, xshift, yshift);
    case TypeDesc::UINT16:
        return transform_<unsigned short> (dst, src, mapping, filter, xshift, yshift);
    case TypeDesc::INT16 :
        return transform_<short> (dst, src, mapping, filter, xshift, yshift);
    case TypeDesc::UINT  :
        return transform_<unsigned int> (dst, src, mapping, filter, xshift, yshift);
    case TypeDesc::INT   :
        return transform_<int> (dst, src, mapping, filter, xshift, yshift);
    case TypeDesc::UINT64:
        return transform_<unsigned long long> (dst, src, mapping, filter, xshift, yshift);
    case TypeDesc::INT64 :
        return transform_<long long> (dst, src, mapping, filter, xshift, yshift);
    case TypeDesc::HALF  :
        return transform_<half> (dst, src, mapping, filter, xshift, yshift);
    case TypeDesc::DOUBLE:
        return transform_<double> (dst, src, mapping, filter, xshift, yshift);
    default:
        std::cout << "aaaaa\n";
        return false;
    }
}




ImageBufAlgo::RotationMapping::RotationMapping (float rotangle, float originx,
                                                float originy)
    : m_rotangle(-rotangle*M_PI / 180.0f),
      m_originx(originx), m_originy(originy),
      m_sinr(sinf(m_rotangle)), m_cosr(cosf(m_rotangle))
{
}


void
ImageBufAlgo::RotationMapping::map (float x, float y, float* s, float* t,
                                    float *dsdx, float *dtdx,
                                    float *dsdy, float *dtdy) const
{
    *s = m_originx + (x+0.5f-m_originx) * m_cosr - (y+0.5f-m_originy) * m_sinr;
    *t = m_originy + (x+0.5f-m_originx) * m_sinr + (y+0.5f-m_originy) * m_cosr;
    // Simplifying assumption: rotation doesn't change pixel "size."
    *dsdx = 1.0f;
    *dtdx = 0;
    *dsdy = 0;
    *dtdy = 1.0f;
}



void 
ImageBufAlgo::RotationMapping::outputImageSize(int *width, int *height,
                                               int srcWidth, int srcHeight) const
{
    float dsdx = 0, dsdy = 0, dtdx = 0, dtdy = 0;
    float x = 0, y = 0;
    float minx, maxx, miny, maxy;
    
    map (0, 0, &x, &y, &dsdx, &dtdx, &dsdy, &dtdy);
    maxx = x;       minx = x;
    maxy = y;       miny = y;

    map (srcWidth, 0, &x, &y, &dsdx, &dtdx, &dsdy, &dtdy);
    maxx = (x > maxx)? x : maxx;    minx = (x < minx)? x : minx;
    maxy = (y > maxy)? y : maxy;    miny = (y < miny)? y : miny;

    map (srcWidth, srcHeight, &x, &y, &dsdx, &dtdx, &dsdy, &dtdy);
    maxx = (x > maxx)? x : maxx;    minx = (x < minx)? x : minx;
    maxy = (y > maxy)? y : maxy;    miny = (y < miny)? y : miny;

    map (0, srcHeight, &x, &y, &dsdx, &dtdx, &dsdy, &dtdy);
    maxx = (x > maxx)? x : maxx;    minx = (x < minx)? x : minx;
    maxy = (y > maxy)? y : maxy;    miny = (y < miny)? y : miny;

    if (maxx > 0) maxx += 0.5f;
    else maxx -= 0.5f;

    if (minx > 0) minx += 0.5f;
    else minx -= 0.5f;

    if (maxy > 0) maxy += 0.5f;
    else maxy -= 0.5f;

    if (miny > 0) miny += 0.5f;
    else miny -= 0.5f;

    *width = (int)maxx - (int)minx;
    *height = (int)maxy - (int)miny; 
}


void
ImageBufAlgo::ResizeMapping::map (float x, float y, float* s, float* t,
                                  float *dsdx, float *dtdx, float *dsdy, float *dtdy) const
{
    *s = (x + 0.5f) / xscale;
    *t = (y + 0.5f) / yscale;
    *dsdx = 1.0f / xscale;
    *dtdx = 0;
    *dsdy = 0;
    *dtdy = 1.0f / yscale;
}


void 
ImageBufAlgo::ResizeMapping::outputImageSize(int *width, int *height,
                                             int srcWidth, int srcHeight) const
{
    *width = srcWidth * xscale + 0.5f;
    *height = srcHeight * yscale + 0.5f; 
}



ImageBufAlgo::ShearMapping::ShearMapping (float m, float n,
                                          float originx, float originy)
    : m_m(-m), m_n(n), m_originx(originx), m_originy(originy)
{ }



void
ImageBufAlgo::ShearMapping::map (float x, float y, float* s, float* t,
                                 float *dsdx, float *dtdx, float *dsdy, float *dtdy) const
{
    x += 0.5f - m_originx;
    y += 0.5f - m_originy;
    
    // making sure that we are not dividing by 0
    if (1.0f - m_m*m_n == 0)
        return;   

    float mn = m_m * m_n;
    *s = (x - m_m*y) / (1 - mn);
    *t = y - m_n*(*s) + m_originy - 0.5f;
    *s += m_originx - 0.5f;
    
    // correct formula
    *dsdx = 1.0f / (1.0f - mn);
    *dtdx = -m_n / (1.0f - mn);
    *dsdy = -m_m / (1.0f - mn);
    *dtdy = 1.0f / (1.0f - mn);
    
    // changes to the formula
    // reduces blurring
    /*
    if(m > 0 && n >0)
    {
        *dtdx = 0;//-n / (1.0f - m*n);
        *dsdy = 0;//-m / (1.0f - m*n);
    }
    // reduces aliasing when m or n parameter is high
    if(m>1)
        *dsdx += fabs(m) / 2;
    if(n>1)
        *dtdy += fabs(n) / 2;

     */ 
}



void 
ImageBufAlgo::ShearMapping::outputImageSize(int *width, int *height,
                                            int srcWidth, int srcHeight) const
{
    float dsdx = 0, dsdy = 0, dtdx = 0, dtdy = 0;
    float x = 0, y = 0;
    float minx, maxx, miny, maxy;
    
    map (0, 0, &x, &y, &dsdx, &dtdx, &dsdy, &dtdy);
    maxx = x;       minx = x;
    maxy = y;       miny = y;

    map (srcWidth, 0, &x, &y, &dsdx, &dtdx, &dsdy, &dtdy);
    maxx = (x > maxx)? x : maxx;    minx = (x < minx)? x : minx;
    maxy = (y > maxy)? y : maxy;    miny = (y < miny)? y : miny;

    map (srcWidth, srcHeight, &x, &y, &dsdx, &dtdx, &dsdy, &dtdy);
    maxx = (x > maxx)? x : maxx;    minx = (x < minx)? x : minx;
    maxy = (y > maxy)? y : maxy;    miny = (y < miny)? y : miny;

    map (0, srcHeight, &x, &y, &dsdx, &dtdx, &dsdy, &dtdy);
    maxx = (x > maxx)? x : maxx;    minx = (x < minx)? x : minx;
    maxy = (y > maxy)? y : maxy;    miny = (y < miny)? y : miny;

    if (maxx > 0) maxx += 0.5f;
    else maxx -= 0.5f;

    if (minx > 0) minx += 0.5f;
    else minx -= 0.5f;

    if (maxy > 0) maxy += 0.5f;
    else maxy -= 0.5f;

    if (miny > 0) miny += 0.5f;
    else miny -= 0.5f;

    *width = (int)maxx - (int)minx;
    *height = (int)maxy - (int)miny; 
}



ImageBufAlgo::ReflectionMapping::ReflectionMapping (float a, float b,
                                                    float originx, float originy)
    : m_a(a), m_b(b), m_originx(originx), m_originy(originy)
{ }



void
ImageBufAlgo::ReflectionMapping::map (float x, float y, float* s, float* t,
                   float *dsdx, float *dtdx, float *dsdy, float *dtdy) const
{
    x += 0.5f-m_originx;
    y += 0.5f-m_originy;
    
    // preventing from division by zero (vertical reflection)
    if (m_a == 0)
        return;
    
    y *= -1;
    
    // determining a perpendicular function g(x)=cx+d to reflection
    // f(x)=ax+b going through (x,y)
    float c = -1.0f/m_a;
    float d = y + x/m_a;

    // g(x) and f(x) intersection point
    float isy = (d*m_a - c*m_b) / (m_a - c);
    float isx = (isy - m_b) / m_a;
    
    *s =  2*isx - x + m_originx - 0.5f;
    *t = - 2*isy + y + m_originy - 0.5f; 

    // Simplifying assumption: reflection doesn't change pixel "size."
    *dsdx = 1.0f;
    *dtdx = 0;
    *dsdy = 0;
    *dtdy = 1.0f;
}



void 
ImageBufAlgo::ReflectionMapping::outputImageSize(int *width, int *height,
                                                 int srcWidth, int srcHeight) const
{
    float dsdx = 0, dsdy = 0, dtdx = 0, dtdy = 0;
    float x = 0, y = 0;
    float minx, maxx, miny, maxy;
    
    map (0, 0, &x, &y, &dsdx, &dtdx, &dsdy, &dtdy);
    maxx = x;       minx = x;
    maxy = y;       miny = y;

    map (srcWidth, 0, &x, &y, &dsdx, &dtdx, &dsdy, &dtdy);
    maxx = (x > maxx)? x : maxx;    minx = (x < minx)? x : minx;
    maxy = (y > maxy)? y : maxy;    miny = (y < miny)? y : miny;

    map (srcWidth, srcHeight, &x, &y, &dsdx, &dtdx, &dsdy, &dtdy);
    maxx = (x > maxx)? x : maxx;    minx = (x < minx)? x : minx;
    maxy = (y > maxy)? y : maxy;    miny = (y < miny)? y : miny;

    map (0, srcHeight, &x, &y, &dsdx, &dtdx, &dsdy, &dtdy);
    maxx = (x > maxx)? x : maxx;    minx = (x < minx)? x : minx;
    maxy = (y > maxy)? y : maxy;    miny = (y < miny)? y : miny;

    if (maxx > 0) maxx += 0.5f;
    else maxx -= 0.5f;

    if (minx > 0) minx += 0.5f;
    else minx -= 0.5f;

    if (maxy > 0) maxy += 0.5f;
    else maxy -= 0.5f;

    if (miny > 0) miny += 0.5f;
    else miny -= 0.5f;

    *width = (int)maxx - (int)minx;
    *height = (int)maxy - (int)miny; 
}



ImageBufAlgo::TPSMapping::TPSMapping (const std::vector<Point> &_srcPoints,
                                      const std::vector<Point> &_dstPoints)
    : srcControlPoints(_srcPoints), dstControlPoints(_dstPoints),
      ctrlpc((int)_srcPoints.size())
{
    isDstToSrcMapping = false;
    
    int dim = ctrlpc + 2;
    tpsXCoefs.resize (dim);
    tpsYCoefs.resize (dim);

    axelements.resize (dim*dim);
    ax.resize (dim); //creating table of pointers
    ax[0] = &axelements[0]; //elements
    for (int i=1; i<dim; i++) 
        ax[i] = ax[i-1] + dim;
    
    ayelements.resize (dim*dim);
    ay.resize (dim); //creating table of pointers
    ay[0] = &ayelements[0]; //elements
    for (int i=1; i<dim; i++) 
        ay[i] = ay[i-1] + dim;

    bx.resize (dim);
    by.resize (dim);
    
    calculateCoefficients();   
}



void
ImageBufAlgo::TPSMapping::calculateCoefficients()
{       
    //--- preparing matrixes for determining coefficients ---//
    int dim = ctrlpc + 2;   //matrix dimension

    for (int y = 0; y < dim; y++) {
        for (int x = 0; x < dim; x++) {
            if (y < dim - 2) {
                if (x == 0) {
                    ax[y][x] = 1;
                    ay[y][x] = 1;
                    continue;
                }

                if (x == 1) {
                    ax[y][x] = srcControlPoints[y].x;
                    ay[y][x] = srcControlPoints[y].y;
                    continue;
                }

                ax[y][x] = kernelFunction(srcControlPoints[y], srcControlPoints[x - 2]);
                ay[y][x] = kernelFunction(srcControlPoints[y], srcControlPoints[x - 2]);
            } else {
                if (x == 0 || x == 1) {
                    ax[y][x] = 0;
                    ay[y][x] = 0;
                    continue;
                } else if (y == dim - 2) {
                    ax[y][x] = 1;
                    ay[y][x] = 1;
                    continue;
                } else {
                    ax[y][x] = dstControlPoints[x - 2].x;
                    ay[y][x] = dstControlPoints[x - 2].y;
                }
            }
        }
    }

    for (int i = 0; i < dim; i++) {
        if (i >= dim - 2) {
            bx[i] = 0;
            by[i] = 0;
            continue;
        }
        bx[i] = dstControlPoints[i].x;
        by[i] = dstControlPoints[i].y;
    }

    //--- finding coefficients -----------------------------------//
    std::vector<int> indxX;
    LUDecompose(&ax[0], &indxX[0], dim);
    solveMatrix(&bx[0], &tpsXCoefs[0], &indxX[0], &ax[0], dim);

    std::vector<int> indxY;
    LUDecompose(&ay[0], &indxY[0], dim);
    solveMatrix(&by[0], &tpsYCoefs[0], &indxY[0], &ay[0], dim);
    //-------------------------------------------------------------//
}



void
ImageBufAlgo::TPSMapping::map (float x, float y, float* s, float* t,
                               float *dsdx, float *dtdx, float *dsdy, float *dtdy) const 
{
    x += 0.5f;
    y += 0.5f;
    
    int dim = ctrlpc + 2;
    
    float xSum = 0;
    float ySum = 0;
    
    Point p1;
    p1.x = x;
    p1.y = y;
    
    for (int i = 2; i < dim; i++) {
        xSum += tpsXCoefs[i] * kernelFunction(p1, srcControlPoints[i - 2]);
        ySum += tpsYCoefs[i] * kernelFunction(p1, srcControlPoints[i - 2]);
    }

    float xPrim = (tpsXCoefs[0] + tpsXCoefs[1] * x + xSum);
    float yPrim = (tpsYCoefs[0] + tpsYCoefs[1] * y + ySum);

    *s = xPrim;
    *t = yPrim;
    
    // FIXME
    *dsdx = 1.0f;
    *dtdx = 0;
    *dsdy = 0;
    *dtdy = 1.0f;
    
    // ok
    float upperS = 0, upperT = 0, nextS = 0, nextT = 0;
    x -= 0.5f;
    y -= 0.5f;
    simpleMap (x, y+1, &upperS, &upperT);
    simpleMap (x+1, y, &nextS, &nextT);
    
    *dsdx = fabs(nextS - *s);
  //  *dtdx = -(nextT - *t);
  //  *dsdy = (upperS - *s);
    *dtdy = fabs(upperT - *t);
}



void
ImageBufAlgo::TPSMapping::simpleMap (float x, float y, float* s, float* t) const
{
    x += 0.5f;
    y += 0.5f;
    
    int dim = ctrlpc + 2;
    
    float xSum = 0;
    float ySum = 0;
    
    Point p1 (x, y);
    
    for (int i = 2; i < dim; i++) {
        xSum += tpsXCoefs[i] * kernelFunction(p1, srcControlPoints[i - 2]);
        ySum += tpsYCoefs[i] * kernelFunction(p1, srcControlPoints[i - 2]);
    }

    float xPrim = (tpsXCoefs[0] + tpsXCoefs[1] * x + xSum);
    float yPrim = (tpsYCoefs[0] + tpsYCoefs[1] * y + ySum);

    *s = xPrim;
    *t = yPrim; 
}



void
ImageBufAlgo::TPSMapping::outputImageSize(int *width, int *height, int srcWidth, int srcHeight) const
{
    *width = srcWidth;
    *height = srcHeight;
}


    
float
ImageBufAlgo::TPSMapping::rSquare(Point p1, Point p2) const
{
    return (p1.x - p2.x) * (p1.x - p2.x) + (p1.y - p2.y) * (p1.y - p2.y);
}



float
ImageBufAlgo::TPSMapping::kernelFunction(Point p1, Point p2) const
{
    double r2 = rSquare(p1, p2);

    if (r2 == 0)
        return 0;

    return r2 * log(sqrt(r2));
}



/// Decompose matrix to LU form
bool
ImageBufAlgo::TPSMapping::LUDecompose(float** lu, int* indx, int dim) const
{
    int i, imax = 0, j, k;
    float big, temp;
    float* vv = new float[dim];

    for (i = 0; i < dim; i++) {
        big = 0.0f;
        for (j = 0; j < dim; j++)
            if ((temp = fabs(lu[i][j])) > big) big = temp;
        if (big == 0.0f)
            return false; //singular matrix
        vv[i] = 1.0f / big;
    }
 
    for (k = 0; k < dim; k++) {
        big = 0.0f;
        for (i = k; i < dim; i++) {
            temp = vv[i] * fabs(lu[i][k]);
            if (temp > big) {
                big = temp;
                imax = i;
            }
        }

        if (k != imax) {
            for (j = 0; j < dim; j++) {
                temp = lu[imax][j];
                lu[imax][j] = lu[k][j];
                lu[k][j] = temp;
            }
            vv[imax] = vv[k];
        }
        indx[k] = imax;

        for (i = k + 1; i < dim; i++) {
            temp = lu[i][k] /= lu[k][k];
            for (j = k + 1; j < dim; j++)
                lu[i][j] -= temp * lu[k][j];
        }
    }
    return true;
}



bool
ImageBufAlgo::TPSMapping::solveMatrix(float* b, float* x, int* indx, float** lu, int dimm) const
{
    int n = dimm;
    int i, ii = 0, ip, j;
    float sum = 0;
    for (i = 0; i < n; i++)
        x[i] = b[i];
    for (i = 0; i < n; i++) {
        ip = indx[i];
        sum = x[ip];
        x[ip] = x[i];
        if (ii != 0)
            for (j = ii - 1; j < i; j++) sum -= lu[i][j] * x[j];
        else if (sum != 0.0f)
            ii = i + 1;
        x[i] = sum;
    }
    for (i = n - 1; i >= 0; i--) {
        sum = x[i];
        for (j = i + 1; j < n; j++) sum -= lu[i][j] * x[j];
        x[i] = sum / lu[i][i];
    }
    return true;
}



}
OIIO_NAMESPACE_EXIT
