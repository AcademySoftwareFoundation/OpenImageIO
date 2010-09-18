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
/// Implementation of ImageBuf class.

#include <iostream>
#include <limits>

#include "imagebuf.h"
using namespace OpenImageIO;
using namespace ImageBufAlgo;

#include "dassert.h"


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
        dst.fill(pixel);
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
        dst.zero ();
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
