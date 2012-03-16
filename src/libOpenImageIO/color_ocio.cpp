/*
  Copyright 2012 Larry Gritz and the other authors and contributors.
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

#include "strutil.h"
#include "color.h"
#include "imagebufalgo.h"

#ifdef USE_OCIO
#include <OpenColorIO/OpenColorIO.h>
namespace OCIO = OCIO_NAMESPACE;
#endif


OIIO_NAMESPACE_ENTER
{


bool
ColorConfig::supportsOpenColorIO ()
{
#ifdef USE_OCIO
    return true;
#else
    return false;
#endif
}



// Hidden implementation of ColorConfig
class ColorConfig::Impl
{
public:
#ifdef USE_OCIO
    OCIO::ConstConfigRcPtr config_;
#endif
    mutable std::string error_;
    std::vector<std::pair<std::string,int> > colorspaces;
    std::string linear_alias;  // Alias for a scene-linear color space

    Impl() { }
    ~Impl() { }
    void inventory ();
    void add (const std::string &name, int index) {
        colorspaces.push_back (std::pair<std::string,int> (name, index));
    }
};



// ColorConfig utility to take inventory of the color spaces available.
// It sets up knowledge of "linear", "sRGB", "Rec709",
// even if the underlying OCIO configuration lacks them.
void
ColorConfig::Impl::inventory ()
{
#ifdef USE_OCIO
    if (config_) {
        for (int i = 0, e = config_->getNumColorSpaces();  i < e;  ++i) {
            std::string name = config_->getColorSpaceNameByIndex(i);
            add (name, i);
        }
        OCIO::ConstColorSpaceRcPtr lin = config_->getColorSpace ("scene_linear");
        if (lin)
            linear_alias = lin->getName();
    }
    if (colorspaces.size())
        return;   // If any were defined, we're done
#endif

    // If there was no configuration, or we didn't compile with OCIO
    // support at all, register a few basic names we know about.
    add ("linear", 0);
    add ("sRGB", 1);
    add ("Rec709", 2);
}



ColorConfig::ColorConfig ()
    : m_impl (new ColorConfig::Impl)
{
#ifdef USE_OCIO
    OCIO::SetLoggingLevel (OCIO::LOGGING_LEVEL_NONE);
    try {
        getImpl()->config_ = OCIO::GetCurrentConfig();
    }
    catch(OCIO::Exception &e) {
        getImpl()->error_ = e.what();
    }
    catch(...) {
        getImpl()->error_ = "An unknown error occurred in OpenColorIO, GetCurrentConfig";
    }
#endif

    getImpl()->inventory ();

    // If we populated our own, remove any errors.
    if (getNumColorSpaces() && !getImpl()->error_.empty())
        getImpl()->error_.clear();
}



ColorConfig::ColorConfig (const char * filename)
    : m_impl (new ColorConfig::Impl)
{
#ifdef USE_OCIO
    OCIO::SetLoggingLevel (OCIO::LOGGING_LEVEL_NONE);
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
#endif
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
    return (int) getImpl()->colorspaces.size();
}



const char *
ColorConfig::getColorSpaceNameByIndex (int index) const
{
    return getImpl()->colorspaces[index].first.c_str();
}



const char *
ColorConfig::getColorSpaceNameByRole (const char *role) const
{
#if USE_OCIO
    if (getImpl()->config_) {
        OCIO::ConstColorSpaceRcPtr c = getImpl()->config_->getColorSpace (role);
        // Catch special case of obvious name synonyms
        if (!c && Strutil::iequals(role,"linear"))
            c = getImpl()->config_->getColorSpace ("scene_linear");
        if (!c && Strutil::iequals(role,"scene_linear"))
            c = getImpl()->config_->getColorSpace ("linear");
        if (c)
            return c->getName();
    }
#endif

    // No OCIO at build time, or no OCIO configuration at run time
    if (Strutil::iequals (role, "linear") || Strutil::iequals (role, "scene_linear"))
        return "linear";

    return NULL;  // Dunno what role
}



// Abstract wrapper class for objects that will apply color transformations.
class ColorProcessor
{
public:
    ColorProcessor () {};
    virtual ~ColorProcessor (void) { };
    virtual bool isNoOp() const { return false; }
    virtual bool hasChannelCrosstalk() const { return false; }
    virtual void apply (float *data, int width, int height, int channels,
                        stride_t chanstride, stride_t xstride,
                        stride_t ystride) const = 0;
};



#ifdef USE_OCIO
// Custom ColorProcessor that wraps an OpenColorIO Processor.
class ColorProcessor_OCIO : public ColorProcessor
{
public:
    ColorProcessor_OCIO (OCIO::ConstProcessorRcPtr p) : m_p(p) {};
    virtual ~ColorProcessor_OCIO (void) { };
    
    virtual bool isNoOp() const { return m_p->isNoOp(); }
    virtual bool hasChannelCrosstalk() const {
        return m_p->hasChannelCrosstalk();
    }
    virtual void apply (float *data, int width, int height, int channels,
                        stride_t chanstride, stride_t xstride,
                        stride_t ystride) const
    {
        OCIO::PackedImageDesc pid (data, width, height, channels,
                                   chanstride, xstride, ystride);
        m_p->apply (pid);
    }

private:
    OCIO::ConstProcessorRcPtr m_p;
};
#endif


// ColorProcessor that hard-codes sRGB-to-linear
class ColorProcessor_sRGB_to_linear : public ColorProcessor {
public:
    ColorProcessor_sRGB_to_linear () : ColorProcessor() { };
    ~ColorProcessor_sRGB_to_linear () { };

    virtual void apply (float *data, int width, int height, int channels,
                        stride_t chanstride, stride_t xstride,
                        stride_t ystride) const
    {
        if (channels > 3)
            channels = 3;
        for (int y = 0;  y < height;  ++y) {
            char *d = (char *)data + y*ystride;
            for (int x = 0;  x < width;  ++x, d += xstride)
                for (int c = 0;  c < channels;  ++c)
                    *(float *)d = sRGB_to_linear (*(float *)d);
        }
    }
};


// ColorProcessor that hard-codes linear-to-sRGB
class ColorProcessor_linear_to_sRGB : public ColorProcessor {
public:
    ColorProcessor_linear_to_sRGB () : ColorProcessor() { };
    ~ColorProcessor_linear_to_sRGB () { };
    
    virtual void apply (float *data, int width, int height, int channels,
                        stride_t chanstride, stride_t xstride,
                        stride_t ystride) const
    {
        if (channels > 3)
            channels = 3;
        for (int y = 0;  y < height;  ++y) {
            char *d = (char *)data + y*ystride;
            for (int x = 0;  x < width;  ++x, d += xstride)
                for (int c = 0;  c < channels;  ++c)
                    ((float *)d)[c] = linear_to_sRGB (((float *)d)[c]);
        }
    }
};



// ColorProcessor that hard-codes Rec709-to-linear
class ColorProcessor_Rec709_to_linear : public ColorProcessor {
public:
    ColorProcessor_Rec709_to_linear () : ColorProcessor() { };
    ~ColorProcessor_Rec709_to_linear () { };

    virtual void apply (float *data, int width, int height, int channels,
                        stride_t chanstride, stride_t xstride,
                        stride_t ystride) const
    {
        if (channels > 3)
            channels = 3;
        for (int y = 0;  y < height;  ++y) {
            char *d = (char *)data + y*ystride;
            for (int x = 0;  x < width;  ++x, d += xstride)
                for (int c = 0;  c < channels;  ++c)
                    *(float *)d = Rec709_to_linear (*(float *)d);
        }
    }
};


// ColorProcessor that hard-codes linear-to-Rec709
class ColorProcessor_linear_to_Rec709 : public ColorProcessor {
public:
    ColorProcessor_linear_to_Rec709 () : ColorProcessor() { };
    ~ColorProcessor_linear_to_Rec709 () { };
    
    virtual void apply (float *data, int width, int height, int channels,
                        stride_t chanstride, stride_t xstride,
                        stride_t ystride) const
    {
        if (channels > 3)
            channels = 3;
        for (int y = 0;  y < height;  ++y) {
            char *d = (char *)data + y*ystride;
            for (int x = 0;  x < width;  ++x, d += xstride)
                for (int c = 0;  c < channels;  ++c)
                    ((float *)d)[c] = linear_to_Rec709 (((float *)d)[c]);
        }
    }
};



ColorProcessor*
ColorConfig::createColorProcessor (const char * inputColorSpace,
                                   const char * outputColorSpace) const
{
#ifdef USE_OCIO
    // Ask OCIO to make a Processor that can handle the requested
    // transformation.
    if (getImpl()->config_) {
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
        return new ColorProcessor_OCIO(p);
    }
#endif

    // Either not compiled with OCIO support, or no OCIO configuration
    // was found at all.  There are a few color conversions we know
    // about even in such dire conditions.
    if (Strutil::iequals(inputColorSpace,"linear") &&
        Strutil::iequals(outputColorSpace,"sRGB")) {
        return new ColorProcessor_linear_to_sRGB;
    }
    if (Strutil::iequals(inputColorSpace,"sRGB") &&
        Strutil::iequals(outputColorSpace,"linear")) {
        return new ColorProcessor_sRGB_to_linear;
    }
    if (Strutil::iequals(inputColorSpace,"linear") &&
        Strutil::iequals(outputColorSpace,"Rec709")) {
        return new ColorProcessor_linear_to_Rec709;
    }
    if (Strutil::iequals(inputColorSpace,"Rec709") &&
        Strutil::iequals(outputColorSpace,"linear")) {
        // No OCIO, or the OCIO config doesn't know linear->sRGB
        return new ColorProcessor_Rec709_to_linear;
    }

    return NULL;    // if we get this far, we've failed
}



void
ColorConfig::deleteColorProcessor (ColorProcessor * processor)
{
    delete processor;
}



//////////////////////////////////////////////////////////////////////////
//
// Image Processing Implementations


bool
ImageBufAlgo::colorconvert (ImageBuf &dst, const ImageBuf &src,
                            const ColorProcessor* processor, bool unpremult)
{
    // If the processor is NULL, return false (error)
    if (!processor)
        return false;

    // If the processor is a no-op, no work needs to be done. Early exit.
    if (processor->isNoOp())
        return true;
    
    const ImageSpec &dstspec = dst.spec();
    // Temporary space to hold one RGBA scanline
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
    
    // If the processor has crosstalk, and we'll be using it, we should
    // reset the channels to 0 before loading each scanline.
    bool clearScanline = (channelsToCopy<4 && 
                          (processor->hasChannelCrosstalk() || unpremult));
    
    for (int k = dstspec.z; k < dstspec.z+dstspec.depth; k++) {
        for (int j = dstspec.y; j <  dstspec.y+dstspec.height; j++) {
            // Clear the scanline
            if (clearScanline) {
                memset (&scanline[0], 0, sizeof(float)*scanline.size());
            }
            
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
            processor->apply (&scanline[0], dstspec.width, 1, 4,
                              sizeof(float), 4*sizeof(float),
                              dstspec.width*4*sizeof(float));
            
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
                            const ColorProcessor* processor, bool unpremult)
{
    // If the processor is NULL, return false (error)
    if (!processor)
        return false;
    
    // If the processor is a no-op, no work needs to be done. Early exit.
    if (processor->isNoOp())
        return true;
    
    // Load the pixel
    float rgba[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    int channelsToCopy = std::min (4, nchannels);
    memcpy(rgba, color, channelsToCopy*sizeof (float));
    
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
    processor->apply (rgba, 1, 1, 4,
                      sizeof(float), 4*sizeof(float), 4*sizeof(float));
    
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
