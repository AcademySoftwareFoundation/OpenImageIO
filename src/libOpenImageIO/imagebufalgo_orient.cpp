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

#include <OpenEXR/ImathFun.h>
#include <OpenEXR/half.h>

#include <iostream>
#include <limits>

#include "imagebuf.h"
#include "imagebufalgo.h"
#include "dassert.h"
#include <stdexcept>

OIIO_NAMESPACE_ENTER
{
namespace
{

bool
TransformImageSpec(ImageSpec & spec, ImageBufAlgo::AlignedTransform t)
{
    
    if (t == ImageBufAlgo::TRANSFORM_NONE) {
        return true;
    }
    else if (t == ImageBufAlgo::TRANSFORM_FLIP) {
        spec.y = spec.full_y + spec.full_height - spec.y - spec.height;
    }
    else if (t == ImageBufAlgo::TRANSFORM_FLOP) {
        spec.x = spec.full_x + spec.full_width - spec.x - spec.width;
    }
    else if (t == ImageBufAlgo::TRANSFORM_FLIPFLOP) {
        spec.x = spec.full_x + spec.full_width - spec.x - spec.width;
        spec.y = spec.full_y + spec.full_height - spec.y - spec.height;
    }
    else {
        return false;
    }
    
    return true;
}

// FIXME: Reorganize these to use iterators, at the native bit type.
// This does a needless conversion to float and back (per pixel)

// FIXME: These works for non-zero data windows, but
// does it work for non-origin display windows?

void
FlipImageData (ImageBuf &dst, const ImageBuf &src)
{
    const ImageSpec & spec = dst.spec();
    const int c0 = spec.full_y + spec.full_height - 1;
    
    std::vector<float> pixel (dst.spec().nchannels, 0.0f);
    
    // Walk though the output data window...
    for (int k = spec.z; k < spec.z+spec.depth; k++) {
        for (int j = spec.y; j < spec.y+spec.height; j++) {
            int jIn = c0 - j;
            
            for (int i = spec.x; i < spec.x+spec.width ; i++) {
                src.getpixel (i, jIn, k, &pixel[0]);
                dst.setpixel (i, j, k, &pixel[0]);
            }
        }
    }
}

void
FlopImageData (ImageBuf &dst, const ImageBuf &src)
{
    const ImageSpec & spec = dst.spec();
    const int c1 = spec.full_x + spec.full_width - 1;
    
    std::vector<float> pixel (dst.spec().nchannels, 0.0f);
    
    // Walk though the output data window...
    for (int k = spec.z; k < spec.z+spec.depth; k++) {
        for (int j = spec.y; j < spec.y+spec.height; j++) {
            for (int i = spec.x; i < spec.x+spec.width ; i++) {
                int iIn = c1 - i;
                src.getpixel (iIn, j, k, &pixel[0]);
                dst.setpixel (i, j, k, &pixel[0]);
            }
        }
    }
}

void
FlipFlopImageData (ImageBuf &dst, const ImageBuf &src)
{
    const ImageSpec & spec = dst.spec();
    const int c0 = spec.full_y + spec.full_height - 1;
    const int c1 = spec.full_x + spec.full_width - 1;
    
    std::vector<float> pixel (dst.spec().nchannels, 0.0f);
    
    // Walk though the output data window...
    for (int k = spec.z; k < spec.z+spec.depth; k++) {
        for (int j = spec.y; j < spec.y+spec.height; j++) {
            int jIn = c0 - j;
            for (int i = spec.x; i < spec.x+spec.width ; i++) {
                int iIn = c1 - i;
                src.getpixel (iIn, jIn, k, &pixel[0]);
                dst.setpixel (i, j, k, &pixel[0]);
            }
        }
    }
}

} // Anonymous Namespace



bool
ImageBufAlgo::transform (ImageBuf &dst, const ImageBuf &src, AlignedTransform t)
{
    if (t == TRANSFORM_NONE) {
        return dst.copy (src);
    }
    
    ImageSpec dst_spec (src.spec());
    if(!TransformImageSpec(dst_spec, t)) {
        return false;
    }
    
    // Update the image (realloc with the new spec)
    dst.alloc (dst_spec);
    
    if (t == TRANSFORM_FLIP) {
        FlipImageData (dst, src);
        return true;
    }
    else if (t == TRANSFORM_FLOP) {
        FlopImageData (dst, src);
        return true;
    }
    else if (t == TRANSFORM_FLIPFLOP) {
        FlipFlopImageData (dst, src);
        return true;
    }
    
    return false;
}


}
OIIO_NAMESPACE_EXIT
