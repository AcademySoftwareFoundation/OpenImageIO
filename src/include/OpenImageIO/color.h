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

#ifndef OPENIMAGEIO_COLOR_H
#define OPENIMAGEIO_COLOR_H

#include <OpenImageIO/export.h>
#include <OpenImageIO/oiioversion.h>
#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/fmath.h>


OIIO_NAMESPACE_BEGIN

/// The ColorProcessor encapsulates a baked color transformation, suitable for
/// application to raw pixels, or ImageBuf(s). These are generated using
/// ColorConfig::createColorProcessor, and referenced in ImageBufAlgo
/// (amongst other places)

class OIIO_API ColorProcessor;



/// Represents the set of all color transformations that are allowed.
/// If OpenColorIO is enabled at build time, this configuration is loaded
/// at runtime, allowing the user to have complete control of all color
/// transformation math. ($OCIO)  (See opencolorio.org for details).
/// If OpenColorIO is not enabled at build time, a generic color configuration
/// is provided for minimal color support.
///
/// NOTE: ColorConfig(s) and ColorProcessor(s) are potentially heavy-weight.
/// Their construction / destruction should be kept to a minimum.

class OIIO_API ColorConfig
{
public:
    /// Construct a ColorConfig using the named OCIO configuration file,
    /// or if filename is empty, to the current color configuration
    /// specified by env variable $OCIO.
    ///
    /// Multiple calls to this are potentially expensive. A ColorConfig
    /// should usually be shared by an app for its entire runtime.
    ColorConfig (string_view filename = "");
    
    ~ColorConfig();

    /// Reset the config to the named OCIO configuration file, or if
    /// filename is empty, to the current color configuration specified
    /// by env variable $OCIO. Return true for success, false if there
    /// was an error.
    ///
    /// Multiple calls to this are potentially expensive. A ColorConfig
    /// should usually be shared by an app for its entire runtime.
    bool reset (string_view filename = "");
    
    /// Has an error string occurred?
    /// (This will not affect the error state.)
    bool error () const;
    
    /// This routine will return the error string (and clear any error
    /// flags).  If no error has occurred since the last time geterror()
    /// was called, it will return an empty string.
    std::string geterror ();
    
    /// Get the number of ColorSpace(s) defined in this configuration
    int getNumColorSpaces() const;
    
    /// Query the name of the specified ColorSpace.
    const char * getColorSpaceNameByIndex (int index) const;

    /// Get the name of the color space representing the named role,
    /// or NULL if none could be identified.
    const char * getColorSpaceNameByRole (string_view role) const;

    /// Get the data type that OCIO thinks this color space is. The name
    /// may be either a color space name or a role.
    OIIO::TypeDesc getColorSpaceDataType (string_view name, int *bits) const;

    
    /// Get the number of Looks defined in this configuration
    int getNumLooks() const;
    
    /// Query the name of the specified Look.
    const char * getLookNameByIndex (int index) const;

    /// Given the specified input and output ColorSpace, construct the
    /// processor.  It is possible that this will return NULL, if the
    /// inputColorSpace doesnt exist, the outputColorSpace doesn't
    /// exist, or if the specified transformation is illegal (for
    /// example, it may require the inversion of a 3D-LUT, etc).  When
    /// the user is finished with a ColorProcess, deleteColorProcessor
    /// should be called.  ColorProcessor(s) remain valid even if the
    /// ColorConfig that created them no longer exists.
    /// 
    /// Multiple calls to this are potentially expensive, so you should
    /// call once to create a ColorProcessor to use on an entire image
    /// (or multiple images), NOT for every scanline or pixel
    /// separately!
    ColorProcessor* createColorProcessor (string_view inputColorSpace,
                                          string_view outputColorSpace,
                                          string_view context_key /* ="" */,
                                          string_view context_value="") const;
    // DEPRECATED (1.7):
    ColorProcessor* createColorProcessor (string_view inputColorSpace,
                                          string_view outputColorSpace) const;

    /// Given the named look(s), input and output color spaces, construct a
    /// color processor that applies an OCIO look transformation.  If
    /// inverse==true, construct the inverse transformation.  The
    /// context_key and context_value can optionally be used to establish
    /// extra key/value pairs in the OCIO context if they are comma-
    /// separated lists of ontext keys and values, respectively.
    ///
    /// It is possible that this will return NULL, if one of the color
    /// spaces or the look itself doesnt exist or is not allowed.  When
    /// the user is finished with a ColorProcess, deleteColorProcessor
    /// should be called.  ColorProcessor(s) remain valid even if the
    /// ColorConfig that created them no longer exists.
    /// 
    /// Multiple calls to this are potentially expensive, so you should
    /// call once to create a ColorProcessor to use on an entire image
    /// (or multiple images), NOT for every scanline or pixel
    /// separately!
    ColorProcessor* createLookTransform (string_view looks,
                                         string_view inputColorSpace,
                                         string_view outputColorSpace,
                                         bool inverse=false,
                                         string_view context_key="",
                                         string_view context_value="") const;

    /// Get the number of displays defined in this configuration
    int getNumDisplays() const;

    /// Query the name of the specified display.
    const char * getDisplayNameByIndex (int index) const;

    /// Get the number of views for a given display defined in this configuration
    int getNumViews (string_view display) const;

    /// Query the name of the specified view for the specified display
    const char * getViewNameByIndex (string_view display, int index) const;

    /// Query the name of the default display
    const char * getDefaultDisplayName() const;

    /// Query the name of the default view for the specified display
    const char * getDefaultViewName (string_view display) const;

    /// Construct a processor to transform from the given color space
    /// to the color space of the given display and view. You may optionally
    /// override the looks that are, by default, used with the display/view
    /// combination. Looks is a potentially comma (or colon) delimited list
    /// of lookNames, where +/- prefixes are optionally allowed to denote
    /// forward/inverse transformation (and forward is assumed in the
    /// absence of either). It is possible to remove all looks from the
    /// display by passing an empty string. The context_key and context_value
    /// can optionally be used to establish extra key/value pair in the OCIO
    /// context if they are comma-separated lists of context keys and
    /// values, respectively.
    ///
    /// It is possible that this will return NULL, if one of the color
    /// spaces or the display or view doesn't exist or is not allowed.  When
    /// the user is finished with a ColorProcess, deleteColorProcessor
    /// should be called.  ColorProcessor(s) remain valid even if the
    /// ColorConfig that created them no longer exists.
    ///
    /// Multiple calls to this are potentially expensive, so you should
    /// call once to create a ColorProcessor to use on an entire image
    /// (or multiple images), NOT for every scanline or pixel
    /// separately!
    ColorProcessor* createDisplayTransform (string_view display,
                                            string_view view,
                                            string_view inputColorSpace,
                                            string_view looks="",
                                            string_view context_key="",
                                            string_view context_value="") const;

    /// Construct a processor to perform color transforms determined by an
    /// OpenColorIO FileTransform.
    ///
    /// It is possible that this will return NULL, if the FileTransform
    /// doesn't exist or is not allowed.  When the user is finished with a
    /// ColorProcess, deleteColorProcessor should be called.
    /// ColorProcessor(s) remain valid even if the ColorConfig that created
    /// them no longer exists.
    ///
    /// Multiple calls to this are potentially expensive, so you should
    /// call once to create a ColorProcessor to use on an entire image
    /// (or multiple images), NOT for every scanline or pixel
    /// separately!
    ColorProcessor* createFileTransform (string_view name,
                                         bool inverse=false) const;

    /// Given a string (like a filename), look for the longest, right-most
    /// colorspace substring that appears. Returns "" if no such color space
    /// is found. (This is just a wrapper around OCIO's
    /// ColorConfig::parseColorSpaceFromString.)
    string_view parseColorSpaceFromString (string_view str) const;

    /// Delete the specified ColorProcessor
    static void deleteColorProcessor(ColorProcessor * processor);
    
    /// Return if OpenImageIO was built with OCIO support
    static bool supportsOpenColorIO();
    
private:
    ColorConfig(const ColorConfig &);
    ColorConfig& operator= (const ColorConfig &);
    
    class Impl;
    friend class Impl;
    Impl * m_impl;
    Impl * getImpl() { return m_impl; }
    const Impl * getImpl() const { return m_impl; }
};



/// Utility -- convert sRGB value to linear
///    http://en.wikipedia.org/wiki/SRGB
inline float sRGB_to_linear (float x)
{
    return (x <= 0.04045f) ? (x * (1.0f/12.92f))
                           : powf ((x + 0.055f) * (1.0f / 1.055f), 2.4f);
}


#ifndef __CUDA_ARCH__
inline simd::vfloat4 sRGB_to_linear (const simd::vfloat4& x)
{
    return simd::select (x <= 0.04045f, x * (1.0f/12.92f),
                         fast_pow_pos (madd (x, (1.0f / 1.055f), 0.055f*(1.0f/1.055f)), 2.4f));
}
#endif

/// Utility -- convert linear value to sRGB
inline float linear_to_sRGB (float x)
{
    return (x <= 0.0031308f) ? (12.92f * x)
                             : (1.055f * powf (x, 1.f/2.4f) - 0.055f);
}


#ifndef __CUDA_ARCH__
/// Utility -- convert linear value to sRGB
inline simd::vfloat4 linear_to_sRGB (const simd::vfloat4& x)
{
    // x = simd::max (x, simd::vfloat4::Zero());
    return simd::select (x <= 0.0031308f, 12.92f * x,
                         madd (1.055f, fast_pow_pos (x, 1.f/2.4f),  -0.055f));
}
#endif


/// Utility -- convert Rec709 value to linear
///    http://en.wikipedia.org/wiki/Rec._709
inline float Rec709_to_linear (float x)
{
    if (x < 0.081f)
        return x * (1.0f/4.5f);
    else
        return powf ((x + 0.099f) * (1.0f/1.099f), (1.0f/0.45f));
}

/// Utility -- convert linear value to Rec709
inline float linear_to_Rec709 (float x)
{
    if (x < 0.018f)
        return x * 4.5f;
    else
        return 1.099f * powf(x, 0.45f) - 0.099f;
}


OIIO_NAMESPACE_END

#endif // OPENIMAGEIO_COLOR_H
