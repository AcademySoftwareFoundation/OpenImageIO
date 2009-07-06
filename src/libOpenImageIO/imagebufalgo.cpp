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

#include "imagebuf.h"

#include "dassert.h"


namespace OpenImageIO {
namespace ImageBufAlgo {
        
bool 
crop (ImageBuf &dst, const ImageBuf &src,
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
}
    
    
    
    
bool
add (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B, int options)
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


}; // end namespace ImageBufAlgo
}; // end namespace OpenImageIO

