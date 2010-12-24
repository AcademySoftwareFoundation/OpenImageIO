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

#include "colortransfer.h"

OIIO_NAMESPACE_ENTER
{

namespace {  // anonymous


/// Null (pass-thru) transfer function
///
class ColorTransfer_null : public ColorTransfer {
public:
    ColorTransfer_null () : ColorTransfer("null") { };
    ~ColorTransfer_null (void) { };
    float operator() (float x) { return x; }
};



/// Gamma transfer function
///
class ColorTransfer_gamma : public ColorTransfer {
public:
    ColorTransfer_gamma (float gamma=2.2f, float gain=1.0f) 
        : ColorTransfer("gamma"), m_gamma(gamma), m_gain(gain)
    {
        add_paramater ("gamma");
        add_paramater ("gain");
    };
    
    ~ColorTransfer_gamma (void) { };
    
    bool set (std::string name, float param) {
        if (iequals (name, "gamma"))
            m_gamma = param;
        else if (iequals (name, "gain"))
            m_gain = param;
        else return false;
        return true;
    };
    
    bool get (std::string name, float &param) {
        if (iequals (name, "gamma"))
            param = m_gamma;
        else if (iequals (name, "gain"))
            param = m_gain;
        else return false;
        return true;
    };
    
    float operator() (float x) {
        if (x < 0.0f)
            return m_gain * x;
        return std::pow (m_gain * x, m_gamma);
    };
    
private:
    float m_gamma;
    float m_gain;
};



/// sRGB transfer function which is a widely used computer monitor standard
///    http://en.wikipedia.org/wiki/SRGB
class ColorTransfer_linear_to_sRGB : public ColorTransfer {
public:
    ColorTransfer_linear_to_sRGB () : ColorTransfer("linear_to_sRGB") { };
    ~ColorTransfer_linear_to_sRGB (void) { };
    
    float operator() (float x) {
        if (x < 0.0f)
            return 0.0f;
        return (x <= 0.0031308f) ? (12.92f * x)
                                 : (1.055f * std::pow (x, 1.f/2.4f) - 0.055f);
    }
};



/// sRGB transfer function which is a widely used computer monitor standard
///    http://en.wikipedia.org/wiki/SRGB
class ColorTransfer_sRGB_to_linear : public ColorTransfer {
public:
    ColorTransfer_sRGB_to_linear () : ColorTransfer("sRGB_to_linear") { };
    ~ColorTransfer_sRGB_to_linear (void) { };
    
    float operator() (float x) {
        return (x <= 0.04045f) ? (x / 12.92f)
                               : std::pow ((x + 0.055f) / 1.055f, 2.4f);
    }
};



/// AdobeRGB transfer function
///    http://en.wikipedia.org/wiki/Adobe_RGB
///    http://www.adobe.com/digitalimag/pdfs/AdobeRGB1998.pdf
class ColorTransfer_AdobeRGB_to_linear : public ColorTransfer {
    
public:
    ColorTransfer_AdobeRGB_to_linear () : ColorTransfer("AdobeRGB_to_linear") {}
    ~ColorTransfer_AdobeRGB_to_linear (void) { };
    
    float operator() (float x) {
        if (x < 0.f)
            return 0.f;
        return std::pow (x, 2.f + (51.f / 256.f));
    }
};



class ColorTransfer_linear_to_AdobeRGB : public ColorTransfer {
    
public:
    ColorTransfer_linear_to_AdobeRGB () : ColorTransfer("linear_to_AdobeRGB") {}
    ~ColorTransfer_linear_to_AdobeRGB (void) { }
    
    float operator() (float x) {
        if (x < 0.f)
            return 0.f;
        return std::pow (x, 1.f / (2.f + (51.f / 256.f)));
    }
};



/// Rec709 transfer function which is a HDTV standard
///    http://en.wikipedia.org/wiki/Rec._709
class ColorTransfer_Rec709_to_linear : public ColorTransfer {
public:
    ColorTransfer_Rec709_to_linear () : ColorTransfer ("Rec709_to_linear") { };
    ~ColorTransfer_Rec709_to_linear (void) { };
    
    float operator() (float x) {
        if (x < 0.f)
            return 0.f;
        return (x <= 0.081f ? x / 4.5f : std::pow ((x + 0.099f) / 1.099f, 1.0f / 0.45f));
    };
};



class ColorTransfer_linear_to_Rec709 : public ColorTransfer {
public:
    ColorTransfer_linear_to_Rec709 () : ColorTransfer ("linear_to_Rec709") { };
    ~ColorTransfer_linear_to_Rec709 (void) { };
    
    float operator() (float x) {
        if (x < 0.f)
            return 0.f;
        return (x <= 0.018f) ? x * 4.5f : (std::pow (x, 0.45f) * 1.099f) - 0.099f;
    }
};




static void
compute_kodak_coeff (float refBlack, float refWhite,
                     float dispGamma, float negGamma,
                     float &black, float &white, float &gamma, float &gain,
                     float &offset)
{
    // reference black code values below 0 are not allowed
    black = refBlack;
    if (black < 0.f) black = 0.f;
        
    // reference white code values above 1023 are not allowed, and must be
    // equal or above reference black
    white = refWhite;
    if (white > 1023.f)
        white = 1023.f;
    else if (white < black)
        white = black;
        
    // compute coefficients
    gamma = 0.002f / negGamma * dispGamma / 1.7f;
    gain = 1.f / (1.f - std::pow(10.f, (black - white) * gamma));
    offset = gain - 1.f;
    
    // convert 10bit code values to float
    black /= 1023.f;
    white /= 1023.f;
    gamma *= 1023.f;
}



/// Kodak Log transfer function which is used on Kodak 10bit log data
///
/// @todo cache results into a look up table with enough detail for
/// 10bits as this function will be slightly slow at the moment.
///
/// @note the soft clip is not implemented as it makes the transfer
/// function non-reversible. This parameter was designed to help display
/// high code values on a CRT monitor. Having a non-reversible transfer
/// function isn't so good for an image processing pipeline.
class ColorTransfer_KodakLog_to_linear : public ColorTransfer {
public:
    ColorTransfer_KodakLog_to_linear ()
        : ColorTransfer("_KodakLog_to_linear"), m_refBlack(95.f),
          m_refWhite(685.f), m_dispGamma(1.7f), m_negGamma(0.6f)
    {
        add_paramater ("refBlack");
        add_paramater ("refWhite");
        add_paramater ("dispGamma");
        add_paramater ("negGamma");
    };
    
    ~ColorTransfer_KodakLog_to_linear (void) { };
    
    bool set (std::string name, float param) {
        if (iequals (name, "refBlack"))  m_refBlack = param;
        else if (iequals (name, "refWhite"))  m_refWhite = param;
        else if (iequals (name, "dispGamma")) m_dispGamma = param;
        else if (iequals (name, "negGamma"))  m_negGamma = param;
        else return false;
        // recompute coefficients
        compute_kodak_coeff (m_refBlack, m_refWhite, m_dispGamma, m_negGamma,
                             m_black, m_white, m_gamma, m_gain, m_offset);
        return true;
    }
    
    bool get (std::string name, float &param) {
        if (iequals (name, "refBlack"))  param = m_refBlack;
        else if (iequals (name, "refWhite"))  param = m_refWhite;
        else if (iequals (name, "dispGamma")) param = m_dispGamma;
        else if (iequals (name, "negGamma"))  param = m_negGamma;
        else return false;
        return true;
    }
    
    float operator() (float x) {
        return (x < (m_black + 1e-06)) ? 0.f :
            std::pow (10.f, ((x - m_white) * m_gamma)) * m_gain - m_offset;
    }
    
private:
    // transfer paramaters
    float m_refBlack;
    float m_refWhite;
    float m_dispGamma;
    float m_negGamma;
    
    // coefficents
    float m_black;
    float m_white;
    float m_gamma;
    float m_gain;
    float m_offset;
};



class ColorTransfer_linear_to_KodakLog : public ColorTransfer {
public:
    ColorTransfer_linear_to_KodakLog () 
        : ColorTransfer("linear_to_KodakLog"), m_refBlack(95.f),
          m_refWhite(685.f), m_dispGamma(1.7f), m_negGamma(0.6f)
    {
        add_paramater ("refBlack");
        add_paramater ("refWhite");
        add_paramater ("dispGamma");
        add_paramater ("negGamma");
    }
    
    ~ColorTransfer_linear_to_KodakLog (void) { };
    
    bool set (std::string name, float param) {
        if (iequals (name, "refBlack"))  m_refBlack = param;
        else if (iequals (name, "refWhite"))  m_refWhite = param;
        else if (iequals (name, "dispGamma")) m_dispGamma = param;
        else if (iequals (name, "negGamma"))  m_negGamma = param;
        else return false;
        // recompute coefficients
        compute_kodak_coeff (m_refBlack, m_refWhite, m_dispGamma, m_negGamma,
                             m_black, m_white, m_gamma, m_gain, m_offset);
        return true;
    }
    
    bool get (std::string name, float &param) {
        if (iequals (name, "refBlack"))  param = m_refBlack;
        else if (iequals (name, "refWhite"))  param = m_refWhite;
        else if (iequals (name, "dispGamma")) param = m_dispGamma;
        else if (iequals (name, "negGamma"))  param = m_negGamma;
        else return false;
        return true;
    }
    
    float operator() (float x) {
        if (x < 1e-10)
            x = 1e-10;
        x = std::log10 ((x + m_offset) / m_gain) / m_gamma + m_white;
        return (x < m_black) ? 0.f : x;
    }
    
private:
    // transfer paramaters
    float m_refBlack;
    float m_refWhite;
    float m_dispGamma;
    float m_negGamma;
    
    // coefficents
    float m_black;
    float m_white;
    float m_gamma;
    float m_gain;
    float m_offset;
};


};  // anonymous namespace



// ColorTransfer::create is a static method that, given a transfer function
// name, returns an allocated and instantiated transfer function of the correct
// implementation.
// If the name is not recongnoized, return NULL.
ColorTransfer *
ColorTransfer::create (const std::string &name)
{
    if (iequals (name, "linear_to_linear") || iequals (name, "null"))
        return new ColorTransfer_null ();
    if (iequals (name, "Gamma"))
        return new ColorTransfer_gamma ();
    if (iequals (name, "linear_to_sRGB"))
        return new ColorTransfer_linear_to_sRGB ();
    if (iequals (name, "sRGB_to_linear"))
        return new ColorTransfer_sRGB_to_linear ();
    if (iequals (name, "linear_to_AdobeRGB"))
        return new ColorTransfer_linear_to_AdobeRGB ();
    if (iequals (name, "AdobeRGB_to_linear"))
        return new ColorTransfer_AdobeRGB_to_linear ();
    if (iequals (name, "linear_to_Rec709"))
        return new ColorTransfer_linear_to_Rec709 ();
    if (iequals (name, "Rec709_to_linear"))
        return new ColorTransfer_Rec709_to_linear ();
    if (iequals (name, "linear_to_KodakLog"))
        return new ColorTransfer_linear_to_KodakLog ();
    if (iequals (name, "KodakLog_to_linear"))
        return new ColorTransfer_KodakLog_to_linear ();
    return NULL;
}


}
OIIO_NAMESPACE_EXIT
