/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
  All Rights Reserved.
  Based on BSD-licensed software Copyright 2004 NVIDIA Corp.

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
#include <cstring>
#include <string>

#include "filter.h"
#include "fmath.h"


// Below are the implementations of several 2D filters.  They all
// inherit their interface from Filter2D.  Each must redefine two
// virtual functions:
//    char *name()                     Return the filter name
//    float operator(float,float)      Evaluate the filter
//

class FilterBox1D : public Filter1D {
public:
    FilterBox1D (float width) : Filter1D(width) { }
    ~FilterBox1D (void) { }
    float operator() (float x) const {
        return (fabsf(x) <= m_w*0.5f) ? 1.0f : 0.0f;
    }
    const std::string name (void) const { return "box"; }
};



class FilterBox2D : public Filter2D {
public:
    FilterBox2D (float width, float height) : Filter2D(width,height) { }
    ~FilterBox2D (void) { }
    float operator() (float x, float y) const {
        return (fabsf(x) <= m_w*0.5f && fabsf(y) <= m_h*0.5f) ? 1.0f : 0.0f;
    }
    const std::string name (void) const { return "box"; }
};



class FilterTriangle1D : public Filter1D {
public:
    FilterTriangle1D (float width) : Filter1D(width) { }
    ~FilterTriangle1D (void) { }
    float operator() (float x) const {
        return tri1d (x / (m_w*0.5f));
    }
    const std::string name (void) const { return "triangle"; }
private:
    static float tri1d (float x) {
        x = fabsf(x);
        return (x < 1.0f) ? (1.0f - x) : 0.0f;
    }
};



class FilterTriangle2D : public Filter2D {
public:
    FilterTriangle2D (float width, float height) : Filter2D(width,height) { }
    ~FilterTriangle2D (void) { }
    float operator() (float x, float y) const {
        return tri1d (x / (m_w*0.5f)) * tri1d (y / (m_h*0.5f));
    }
    const std::string name (void) const { return "triangle"; }
private:
    static float tri1d (float x) {
        x = fabsf(x);
        return (x < 1.0f) ? (1.0f - x) : 0.0f;
    }
};



class FilterGaussian1D : public Filter1D {
public:
    FilterGaussian1D (float width) : Filter1D(width) { }
    ~FilterGaussian1D (void) { }
    float operator() (float x) const {
        x = 2.0f * fabsf(x) / m_w;
        return (x < 1.0f) ? expf (-2.0f * (x*x)) : 0.0f;
    }
    const std::string name (void) const { return "gaussian"; }
};



class FilterGaussian2D : public Filter2D {
public:
    FilterGaussian2D (float width, float height) : Filter2D(width,height) { }
    ~FilterGaussian2D (void) { }
    float operator() (float x, float y) const {
        x = 2.0f * fabsf(x) / m_w;
        y = 2.0f * fabsf(y) / m_h;
        return (x < 1.0 && y < 1.0) ? expf (-2.0f * (x*x+y*y)) : 0.0f;
    }
    const std::string name (void) const { return "gaussian"; }
};



class FilterCatmullRom1D : public Filter1D {
public:
    FilterCatmullRom1D (float width) : Filter1D(width) { }
    ~FilterCatmullRom1D (void) { }
    float operator() (float x) const {
        x = fabsf(x);
        float x2 = x * x;
        float x3 = x * x2;
        return (x >= 2.0f) ? 0.0f :  ((x < 1.0f) ?
                                      (3.0f * x3 - 5.0f * x2 + 2.0f) :
                                      (-x3 + 5.0f * x2 - 8.0f * x + 4.0f) );
    }
    const std::string name (void) const { return "catmull-rom"; }
};



class FilterCatmullRom2D : public Filter2D {
public:
    FilterCatmullRom2D (float width, float height) : Filter2D(width,height) { }
    ~FilterCatmullRom2D (void) { }
    float operator() (float x, float y) const {
        return catrom1d(x) * catrom1d(y);
    }
    const std::string name (void) const { return "catmull-rom"; }
private :
    static float catrom1d (float x) {
        x = fabsf(x);
        float x2 = x * x;
        float x3 = x * x2;
        return (x >= 2.0f) ? 0.0f :  ((x < 1.0f) ?
                                      (3.0f * x3 - 5.0f * x2 + 2.0f) :
                                      (-x3 + 5.0f * x2 - 8.0f * x + 4.0f) );
    }
};



class FilterBlackmanHarris1D : public Filter1D {
public:
    FilterBlackmanHarris1D (float width) : Filter1D(width) { }
    ~FilterBlackmanHarris1D (void) { }
    float operator() (float x) const {
        return bh1d (x / (m_w*0.5f));
    }
    const std::string name (void) const { return "blackman-harris"; }
private:
    static float bh1d (float x) {
	if (x < -1.0f || x > 1.0f)  // Early out if outside filter range
            return 0.0f;
        // Compute BH.  Straight from classic BH paper, but the usual
        // formula assumes that the filter is centered at 0.5, so scale:
        x = (x + 1.0f) * 0.5f;
        const float A0 =  0.35875f;
        const float A1 = -0.48829f;
        const float A2 =  0.14128f;
        const float A3 = -0.01168f;
        const float m_pi = float (M_PI);
        return A0 + A1 * cosf(2.f * m_pi * x) 
             + A2 * cosf(4.f * m_pi * x) + A3 * cosf(6.f * m_pi * x);
    }
};



class FilterBlackmanHarris2D : public Filter2D {
public:
    FilterBlackmanHarris2D (float width, float height) 
        : Filter2D(width,height), bx(width), by(height) { }
    ~FilterBlackmanHarris2D (void) { }
    float operator() (float x, float y) const {
        return bx(x) * by(y);
    }
    const std::string name (void) const { return "blackman-harris"; }
private:
    FilterBlackmanHarris1D bx, by;
};



class FilterSinc1D : public Filter1D {
public:
    FilterSinc1D (float width) : Filter1D(width) { }
    ~FilterSinc1D (void) { }
    float operator() (float x) const {
        x = fabsf(x);
        if (x > 0.5f*m_w)
             return 0.0f;
        const float m_pi = float (M_PI);
        return (x < 0.0001f) ? 1.0f : sinf (m_pi*x)/(m_pi*x);
    }
    const std::string name (void) const { return "sinc"; }
};



class FilterSinc2D : public Filter2D {
public:
    FilterSinc2D (float width, float height)
        : Filter2D(width,height), sx(width), sy(height)
    { }
    ~FilterSinc2D (void) { }
    float operator() (float x, float y) const {
        return sx(x) * sy(y);
    }
    const std::string name (void) const { return "sinc"; }
private:
    FilterSinc1D sx, sy;
};



class FilterMitchell1D : public Filter1D {
public:
    FilterMitchell1D (float width) : Filter1D(width) { }
    ~FilterMitchell1D (void) { }
    float operator() (float x) const {
        x = fabsf (x / (m_w * 0.5f));
        if (x > 1.0f)
            return 0.0f;
        // Computation stright out of the classic Mitchell paper.
        // In the paper, the range is -2 to 2, so we rescale:
        x *= 2.0f;
        float x2 = x*x;
        const float B = 1.0f/3.0f;
        const float C = 1.0f/3.0f;
        const float SIXTH = 1.0f/6.0f;
        if (x >= 1.0f)
            return ((-B - 6.0f*C)*x*x2 + (6.0f*B + 30.0f*C)*x2 +
                    (-12.0f*B - 48.0f*C)*x + (8.0f*B + 24.0f*C)) * SIXTH;
        else
            return ((12.0f - 9.0f*B - 6.0f*C)*x*x2 + 
                    (-18.0f + 12.0f*B + 6.0f*C)*x2 + (6.0f - 2.0f*B)) * SIXTH;
    }
    const std::string name (void) const { return "mitchell"; }
};



class FilterMitchell2D : public Filter2D {
public:
    FilterMitchell2D (float width, float height) 
        : Filter2D(width,height), mx(width), my(height)
    { }
    ~FilterMitchell2D (void) { }
    float operator() (float x, float y) const {
        return mx(x) * my(y);
    }
    const std::string name (void) const { return "mitchell"; }
private:
    FilterMitchell1D mx, my;
};



// B-spline filter from Stark et al, JGT 10(1)
class FilterBSpline1D : public Filter1D {
public:
    FilterBSpline1D (float width)
        : Filter1D(width), wscale(4.0f/width)
    { }
    ~FilterBSpline1D (void) { }
    float operator() (float x) const {
        x = fabsf (x*wscale);
        if (x <= 1.0f)
            return b1 (1.0f-x);
        else if (x < 2.0f)
            return b0 (2.0f-x);
        else return 0.0f;
    }
    const std::string name (void) const { return "b-spline"; }
private:
    float wscale;  // width scale factor
    float b0 (float t) const { return t*t*t / 6.0f; }
    float b1 (float t) const {
        return 0.5f * t * (t * (1.0f - t) + 1.0f) + 1.0f/6.0f;
    }
};



class FilterBSpline2D : public Filter2D {
public:
    FilterBSpline2D (float width, float height) 
        : Filter2D(width,height), bx(width), by(height)
    { }
    ~FilterBSpline2D (void) { }
    float operator() (float x, float y) const {
        return bx(x) * by(y);
    }
    const std::string name (void) const { return "b-spline"; }
private:
    FilterBSpline1D bx, by;
};



class FilterDisk2D : public Filter2D {
public:
    FilterDisk2D (float width, float height) : Filter2D(width,height) { }
    ~FilterDisk2D (void) { }
    float operator() (float x, float y) const {
        x /= (m_w*0.5f);
        y /= (m_h*0.5f);
        return ((x*x+y*y) < 1.0f) ? 1.0f : 0.0f;
    }
    const std::string name (void) const { return "disk"; }
};



// Filter1D::create is the static method that, given a filter name,
// width, and height, returns an allocated and instantiated filter of
// the correct implementation.  If the name is not recognized, return
// NULL.
Filter1D *
Filter1D::create (const std::string &filtername, float width)
{
    if (filtername == "box")
        return new FilterBox1D (width);
    if (filtername == "triangle")
        return new FilterTriangle1D (width);
    if (filtername == "gaussian")
        return new FilterGaussian1D (width);
    if (filtername == "catmull-rom")
        return new FilterCatmullRom1D (width);
    if (filtername == "blackman-harris")
        return new FilterBlackmanHarris1D (width);
    if (filtername == "sinc")
        return new FilterSinc1D (width);
    if (filtername == "mitchell")
        return new FilterMitchell1D (width);
    if (filtername == "b-spline")
        return new FilterBSpline1D (width);
    return NULL;
}



// Filter2D::create is the static method that, given a filter name,
// width, and height, returns an allocated and instantiated filter of
// the correct implementation.  If the name is not recognized, return
// NULL.
Filter2D *
Filter2D::create (const std::string &filtername, float width, float height)
{
    if (filtername == "box")
        return new FilterBox2D (width, height);
    if (filtername == "triangle")
        return new FilterTriangle2D (width, height);
    if (filtername == "gaussian")
        return new FilterGaussian2D (width, height);
    if (filtername == "catmull-rom")
        return new FilterCatmullRom2D (width, height);
    if (filtername == "blackman-harris")
        return new FilterBlackmanHarris2D (width, height);
    if (filtername == "sinc")
        return new FilterSinc2D (width, height);
    if (filtername == "mitchell")
        return new FilterMitchell2D (width, height);
    if (filtername == "disk")
        return new FilterDisk2D (width, height);
    if (filtername == "b-spline")
        return new FilterBSpline2D (width, height);
    return NULL;
}
