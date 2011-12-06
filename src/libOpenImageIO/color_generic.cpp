/*
  Copyright 2010 Larry Gritz and the other authors and contributors.
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

#include <cmath>
#include <vector>
#include <string>

#include <boost/algorithm/string.hpp>
using boost::algorithm::iequals;

#include "color.h"
#include "imagebufalgo.h"

#if !USE_OCIO

OIIO_NAMESPACE_ENTER
{

namespace
{

class ColorTransfer {
public:
    ColorTransfer () {};
    virtual ~ColorTransfer (void) { };
    
    /// Evalutate the transfer function.
    virtual float operator() (float x)
    {
        return x;
    }
};

class ColorTransfer_sRGB_to_linear : public ColorTransfer {
public:
    ColorTransfer_sRGB_to_linear () : ColorTransfer() { };
    ~ColorTransfer_sRGB_to_linear () { };
    
    float operator() (float x) {
        return (x <= 0.04045f) ? (x / 12.92f)
                               : powf ((x + 0.055f) / 1.055f, 2.4f);
    }
};

class ColorTransfer_linear_to_sRGB : public ColorTransfer {
public:
    ColorTransfer_linear_to_sRGB () : ColorTransfer() { };
    ~ColorTransfer_linear_to_sRGB () { };
    
    float operator() (float x) {
        if (x < 0.0f)
            return 0.0f;
        return (x <= 0.0031308f) ? (12.92f * x)
                                 : (1.055f * powf (x, 1.f/2.4f) - 0.055f);
    }
};

} // Anon namespace


bool
ColorConfig::supportsOpenColorIO ()
{
    return false;
}

class ColorConfig::Impl
{
public:
    mutable std::string error_;
    
    Impl()
    { }
    
    ~Impl()
    { }
};



ColorConfig::ColorConfig()
    : m_impl(new ColorConfig::Impl)
{
}

ColorConfig::ColorConfig(const char * filename)
{
    getImpl()->error_ = "Custom ColorConfigs only supported with OpenColorIO.";
}

ColorConfig::~ColorConfig()
{
    delete m_impl;
    m_impl = NULL;
}

bool
ColorConfig::error() const
{
    return (!getImpl()->error_.empty());
}
   
std::string ColorConfig::geterror()
{
    std::string olderror = getImpl()->error_;
    getImpl()->error_ = "";
    return olderror;
}

int
ColorConfig::getNumColorSpaces () const
{
    return 2;
}

const char *
ColorConfig::getColorSpaceNameByIndex (int index) const
{
    getImpl()->error_ = "";
    if(index == 0) return "linear";
    if(index == 1) return "srgb";
    return "";
}

class ColorProcessor
{
    public:
    ColorTransfer* t1;
    ColorTransfer* t2;
    
    ColorProcessor():
        t1(NULL),
        t2(NULL)
    { }
    
    ~ColorProcessor()
    {
        delete t1;
        delete t2;
    }
};


ColorProcessor*
ColorConfig::createColorProcessor (const char * inputColorSpace,
                                   const char * outputColorSpace) const
{
    getImpl()->error_ = "";
    ColorTransfer* t1 = NULL;
    ColorTransfer* t2 = NULL;
    
    std::string ics(inputColorSpace);
    std::string ocs(outputColorSpace);
    
    if(ics == "linear") {
        t1 = new ColorTransfer();
    }
    else if(ics == "srgb") {
        t1 = new ColorTransfer_sRGB_to_linear();
    }
    else {
        getImpl()->error_ = "Unknown color space: " + ics;
        return NULL;
    }
    
    if(ocs == "linear") {
        t2 = new ColorTransfer();
    }
    else if(ocs == "srgb") {
        t2 = new ColorTransfer_linear_to_sRGB();
    }
    else {
        getImpl()->error_ = "Unknown color space: " + std::string(ocs);
        delete t1;
        return NULL;
    }
    
    ColorProcessor * processor = new ColorProcessor();
    processor->t1 = t1;
    processor->t2 = t2;
    return processor;
}

void
ColorConfig::deleteColorProcessor (ColorProcessor * processor)
{
    delete processor;
}



////////////////////////////////////////////////////////////////////////////////
//
// Image Processing Implementations



bool
ImageBufAlgo::colorconvert (ImageBuf &dst, const ImageBuf &src,
    const ColorProcessor* processor,
    bool unpremult)
{
    // If the processor is NULL, return false (error)
    if (!processor)
        return false;
    
    ImageSpec dstspec = dst.spec();
    
    std::vector<float> scanline(dstspec.width*4, 0.0f);
    
    // Only process up to, and including, the first 4 channels.
    // This does let us process images with fewer than 4 channels, which is the intent
    // FIXME: Instead of loading the first 4 channels, obey dstspec.alpha_channel index
    //        (but first validate that the index is set properly for normal formats)
    
    int channelsToCopy = std::min (4, dstspec.nchannels);
    
    // Walk through all data in our buffer. (i.e., crop or overscan)
    // FIXME: What about the display window?  Should this actually promote
    // the datawindow to be union of data + display? This is useful if
    // the color of black moves.  (In which case non-zero sections should
    // now be promoted).  Consider the lin->log of a roto element, where
    // black now moves to non-black
    //
    // FIXME: Use the ImageBuf::ConstIterator<T,T> s (src);   s.isValid()
    // idiom for traversal instead, to allow for more efficient tile access
    // iteration order
    
    float * dstPtr = NULL;
    const float fltmin = std::numeric_limits<float>::min();
    
    for (int k = dstspec.z; k < dstspec.z+dstspec.depth; k++) {
        for (int j = dstspec.y; j <  dstspec.y+dstspec.height; j++) {
            
            // Load the scanline
            dstPtr = &scanline[0];
            for (int i = dstspec.x; i < dstspec.x+dstspec.width ; i++) {
                src.getpixel (i, j, dstPtr, channelsToCopy);
                dstPtr += 4;
            }
            
            // Optionally unpremult
            if ((channelsToCopy>=4) && unpremult) {
                float alpha = 0.0;
                for (int i=0; i<dstspec.width; ++i) {
                    alpha = scanline[4*i+3];
                    if (alpha > fltmin) {
                        scanline[4*i+0] /= alpha;
                        scanline[4*i+1] /= alpha;
                        scanline[4*i+2] /= alpha;
                    }
                }
            }
            
            // Apply the color transformation in place
            // This is always an rgba float image, due to the conversion above.
            for(int i=0; i<dstspec.width; ++i)
            {
                scanline[4*i+0] = (*processor->t2)((*processor->t1)(scanline[4*i+0]));
                scanline[4*i+1] = (*processor->t2)((*processor->t1)(scanline[4*i+1]));
                scanline[4*i+2] = (*processor->t2)((*processor->t1)(scanline[4*i+2]));
            }
            
            // Optionally premult
            if ((channelsToCopy>=4) && unpremult) {
                float alpha = 0.0;
                for (int i=0; i<dstspec.width; ++i) {
                    alpha = scanline[4*i+3];
                    if (alpha > fltmin) {
                        scanline[4*i+0] *= alpha;
                        scanline[4*i+1] *= alpha;
                        scanline[4*i+2] *= alpha;
                    }
                }
            }
            
            // Store the scanline
            dstPtr = &scanline[0];
            for (int i = dstspec.x; i < dstspec.x+dstspec.width ; i++) {
                dst.setpixel (i, j, dstPtr, channelsToCopy);
                dstPtr += 4;
            }
        }
    }
    
    return true;
}

    
bool
ImageBufAlgo::colorconvert (float * color, int nchannels,
    const ColorProcessor* processor,
    bool unpremult)
{
    // If the processor is NULL, return false (error)
    if (!processor)
        return false;
    
    // Load the pixel
    float rgba[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    int channelsToCopy = std::min (4, nchannels);
    memcpy(rgba, color, channelsToCopy*sizeof(float));
    
    const float fltmin = std::numeric_limits<float>::min();
    
    // Optionally unpremult
    if ((channelsToCopy>=4) && unpremult) {
        float alpha = rgba[3];
        if (alpha > fltmin) {
            rgba[0] /= alpha;
            rgba[1] /= alpha;
            rgba[2] /= alpha;
        }
    }
    
    // Apply the color transformation in place
    // This is always an rgba float image, due to the conversion above.
    rgba[0] = (*processor->t2)((*processor->t1)(rgba[0]));
    rgba[1] = (*processor->t2)((*processor->t1)(rgba[1]));
    rgba[2] = (*processor->t2)((*processor->t1)(rgba[2]));
    
    // Optionally premult
    if ((channelsToCopy>=4) && unpremult) {
        float alpha = rgba[3];
        if (alpha > fltmin) {
            rgba[0] *= alpha;
            rgba[1] *= alpha;
            rgba[2] *= alpha;
        }
    }
    
    // Store the scanline
    memcpy(color, rgba, channelsToCopy*sizeof(float));
    
    return true;
}



}
OIIO_NAMESPACE_EXIT


#endif // USE_OCIO
