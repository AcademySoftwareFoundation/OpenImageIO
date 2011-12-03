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

#if USE_OCIO

#include <OpenColorIO/OpenColorIO.h>
namespace OCIO = OCIO_NAMESPACE;

OIIO_NAMESPACE_ENTER
{

bool
ColorConfig::supportsOpenColorIO ()
{
    return true;
}

class ColorConfig::Impl
{
public:
    OCIO::ConstConfigRcPtr config_;
    mutable std::string error_;
    
    Impl() { }
    
    ~Impl() { }
};



ColorConfig::ColorConfig ()
    : m_impl (new ColorConfig::Impl)
{
    try {
        getImpl()->config_ = OCIO::GetCurrentConfig();
    }
    catch(OCIO::Exception &e) {
        getImpl()->error_ = e.what();
    }
    catch(...) {
        getImpl()->error_ = "An unknown error occurred in OpenColorIO, GetCurrentConfig";
    }
}

ColorConfig::ColorConfig (const char * filename)
{
    try {
        getImpl()->config_ = OCIO::Config::CreateFromFile (filename);
    }
    catch (OCIO::Exception &e) {
        getImpl()->error_ = e.what();
    }
    catch (...)
    {
        getImpl()->error_ = "An unknown error occurred in OpenColorIO, CreateFromFile";
    }
}

ColorConfig::~ColorConfig()
{
    delete m_impl;
    m_impl = NULL;
}

bool
ColorConfig::error () const
{
    return (!getImpl()->error_.empty());
}
   
std::string
ColorConfig::geterror ()
{
    std::string olderror = getImpl()->error_;
    getImpl()->error_ = "";
    return olderror;
}

int
ColorConfig::getNumColorSpaces () const
{
    if (!getImpl()->config_) return 0;
    getImpl()->error_ = "";
    
    return getImpl()->config_->getNumColorSpaces();
}

const char *
ColorConfig::getColorSpaceNameByIndex (int index) const
{
    if (!getImpl()->config_) return "";
    getImpl()->error_ = "";
    
    return getImpl()->config_->getColorSpaceNameByIndex(index);
}




class ColorProcessor
{
    public:
    OCIO::ConstProcessorRcPtr p;
};

ColorProcessor*
ColorConfig::createColorProcessor (const char * inputColorSpace,
                                   const char * outputColorSpace) const
{
    if(!getImpl()->config_) return NULL;
    
    OCIO::ConstProcessorRcPtr p;
    
    try {
        // Get the processor corresponding to this transform.
        p = getImpl()->config_->getProcessor(inputColorSpace, outputColorSpace);
    }
    catch(OCIO::Exception &e) {
        getImpl()->error_ = e.what();
        return NULL;
    }
    catch(...) {
        getImpl()->error_ = "An unknown error occurred in OpenColorIO, getProcessor";
        return NULL;
    }
    
    getImpl()->error_ = "";
    ColorProcessor * processor = new ColorProcessor();
    processor->p = p;
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



/// Apply a transfer function to the pixel values.
/// In-place operation
/// (dst == src) is supported
/// If unpremult is specified, unpremultiply before color conversion, then
/// premultiply after the color conversion.  You'll probably want to use this
/// flag if your image contains an alpha channel
/// return true on success
/// Note: the dst image does not need to equal the src image, either in buffers
/// or bit depths.  (For example, it is commong for the src buffer to be a lower
/// bit depth image and the output image to be float).

bool
ImageBufAlgo::colorconvert (ImageBuf &dst, const ImageBuf &src,
    const ColorProcessor* processor,
    bool unpremult)
{
    // exit if the processor is NULL or a no-op
    if (!processor || (!processor->p) || processor->p->isNoOp())
        return true;
    
    ImageSpec dstspec = dst.spec();
    
    std::vector<float> scanline;
    scanline.resize(dstspec.width*4);
    memset(&scanline[0], sizeof(float)*scanline.size(), 0);
    OCIO::PackedImageDesc scanlineimg(&scanline[0], dstspec.width, 1, 4);
    
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
            processor->p->apply (scanlineimg);
            
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
    // exit if the processor is NULL or a no-op
    if (!processor || (!processor->p) || processor->p->isNoOp())
        return true;
    
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
    
    // Apply the color transformation
    processor->p->applyRGBA (rgba);
    
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
