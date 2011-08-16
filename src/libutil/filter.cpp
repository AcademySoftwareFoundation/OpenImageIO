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

#include "fmath.h"
#include "filter.h"
#include "dassert.h"


OIIO_NAMESPACE_ENTER
{

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
    bool separable (void) const { return true; }
    float xfilt (float x) const { return fabsf(x) <= m_w*0.5f ? 1.0f : 0.0f; }
    float yfilt (float y) const { return fabsf(y) <= m_h*0.5f ? 1.0f : 0.0f; }
    const std::string name (void) const { return "box"; }
};



class FilterTriangle1D : public Filter1D {
public:
    FilterTriangle1D (float width) : Filter1D(width), m_rad_inv(2.0f/width) { }
    ~FilterTriangle1D (void) { }
    float operator() (float x) const {
        return tri1d (x * m_rad_inv);
    }
    const std::string name (void) const { return "triangle"; }

    static float tri1d (float x) {
        x = fabsf(x);
        return (x < 1.0f) ? (1.0f - x) : 0.0f;
    }
private:
    float m_rad_inv;
};



class FilterTriangle2D : public Filter2D {
public:
    FilterTriangle2D (float width, float height)
        : Filter2D(width,height), m_wrad_inv(2.0f/width),
          m_hrad_inv(2.0f/height) { }
    ~FilterTriangle2D (void) { }
    float operator() (float x, float y) const {
        return FilterTriangle1D::tri1d (x * m_wrad_inv)
             * FilterTriangle1D::tri1d (y * m_hrad_inv);
    }
    bool separable (void) const { return true; }
    float xfilt (float x) const {
        return FilterTriangle1D::tri1d (x * m_wrad_inv);
    }
    float yfilt (float y) const {
        return FilterTriangle1D::tri1d (y * m_hrad_inv);
    }
    const std::string name (void) const { return "triangle"; }
private:
    float m_wrad_inv, m_hrad_inv;
};



class FilterGaussian1D : public Filter1D {
public:
    FilterGaussian1D (float width) : Filter1D(width), m_rad_inv(2.0f/width) { }
    ~FilterGaussian1D (void) { }
    float operator() (float x) const {
        return gauss1d (x * m_rad_inv);
    }
    static float gauss1d (float x) {
        x = fabsf(x);
        return (x < 1.0f) ? expf (-2.0f * (x*x)) : 0.0f;
    }
    const std::string name (void) const { return "gaussian"; }
private:
    float m_rad_inv;
};



class FilterGaussian2D : public Filter2D {
public:
    FilterGaussian2D (float width, float height) 
        : Filter2D(width,height), m_wrad_inv(2.0f/width),
          m_hrad_inv(2.0f/height) { }
    ~FilterGaussian2D (void) { }
    float operator() (float x, float y) const {
        return FilterGaussian1D::gauss1d (x * m_wrad_inv)
             * FilterGaussian1D::gauss1d (y * m_hrad_inv);
    }
    bool separable (void) const { return true; }
    float xfilt (float x) const {
        return FilterGaussian1D::gauss1d (x * m_wrad_inv);
    }
    float yfilt (float y) const {
        return FilterGaussian1D::gauss1d (y * m_hrad_inv);
    }
    const std::string name (void) const { return "gaussian"; }
private:
    float m_wrad_inv, m_hrad_inv;
};



class FilterCatmullRom1D : public Filter1D {
public:
    FilterCatmullRom1D (float width) : Filter1D(4.0f) { }
    ~FilterCatmullRom1D (void) { }
    float operator() (float x) const { return catrom1d(x); }
    const std::string name (void) const { return "catmull-rom"; }

    static float catrom1d (float x) {
        x = fabsf(x);
        float x2 = x * x;
        float x3 = x * x2;
        return (x >= 2.0f) ? 0.0f :  ((x < 1.0f) ?
                                      (3.0f * x3 - 5.0f * x2 + 2.0f) :
                                      (-x3 + 5.0f * x2 - 8.0f * x + 4.0f) );
    }
};



class FilterCatmullRom2D : public Filter2D {
public:
    FilterCatmullRom2D (float width, float height) : Filter2D(4.0f,4.0f) { }
    ~FilterCatmullRom2D (void) { }
    float operator() (float x, float y) const {
        return FilterCatmullRom1D::catrom1d(x)
             * FilterCatmullRom1D::catrom1d(y);
    }
    bool separable (void) const { return true; }
    float xfilt (float x) const { return FilterCatmullRom1D::catrom1d(x); }
    float yfilt (float y) const { return FilterCatmullRom1D::catrom1d(y); }
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
    FilterBlackmanHarris1D (float width)
        : Filter1D(width), m_rad_inv(2.0f/width) { }
    ~FilterBlackmanHarris1D (void) { }
    float operator() (float x) const {
        return bh1d (x * m_rad_inv);
    }
    const std::string name (void) const { return "blackman-harris"; }
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
private:
    float m_rad_inv;
};



class FilterBlackmanHarris2D : public Filter2D {
public:
    FilterBlackmanHarris2D (float width, float height) 
        : Filter2D(width,height),
          m_wrad_inv(2.0f/width), m_hrad_inv(2.0f/height) { }
    ~FilterBlackmanHarris2D (void) { }
    float operator() (float x, float y) const {
        return FilterBlackmanHarris1D::bh1d (x*m_wrad_inv)
             * FilterBlackmanHarris1D::bh1d (y*m_hrad_inv);
    }
    bool separable (void) const { return true; }
    float xfilt (float x) const { return FilterBlackmanHarris1D::bh1d(x); }
    float yfilt (float y) const { return FilterBlackmanHarris1D::bh1d(y); }
    const std::string name (void) const { return "blackman-harris"; }
private:
    float m_wrad_inv, m_hrad_inv;
};



class FilterSinc1D : public Filter1D {
public:
    FilterSinc1D (float width) : Filter1D(width), m_rad(width/2.0f) { }
    ~FilterSinc1D (void) { }
    float operator() (float x) const { return sinc1d (x, m_rad); }
    const std::string name (void) const { return "sinc"; }

    static float sinc1d (float x, float rad) {
        x = fabsf(x);
        if (x > rad)
             return 0.0f;
        const float m_pi = float (M_PI);
        return (x < 0.0001f) ? 1.0f : sinf (m_pi*x)/(m_pi*x);
    }
private:
    float m_rad;
};



class FilterSinc2D : public Filter2D {
public:
    FilterSinc2D (float width, float height)
        : Filter2D(width,height),
          m_wrad(2.0f/width), m_hrad(2.0f/height) { }
    ~FilterSinc2D (void) { }
    float operator() (float x, float y) const {
        return FilterSinc1D::sinc1d(x,m_wrad) * FilterSinc1D::sinc1d(y,m_hrad);
    }
    bool separable (void) const { return true; }
    float xfilt (float x) const { return FilterSinc1D::sinc1d(x,m_wrad); }
    float yfilt (float y) const { return FilterSinc1D::sinc1d(y,m_hrad); }
    const std::string name (void) const { return "sinc"; }
private:
    float m_wrad, m_hrad;
};



class FilterLanczos3_1D : public Filter1D {
public:
    FilterLanczos3_1D (float /*width*/) : Filter1D(6.0f) { }
    ~FilterLanczos3_1D (void) { }
    float operator() (float x) const {
        return lanczos3 (x);
    }
    const std::string name (void) const { return "lanczos3"; }

    static float lanczos3 (float x) {
        const float a = 3.0f;  // Lanczos 3 lobe
        x = fabsf(x);
        if (x > a)
             return 0.0f;
        if (x < 0.0001f)
            return 1.0f;
        const float m_pi = float (M_PI);
        const float m_piinv = 1.0f / m_pi;
        const float ainv = 1.0f/a;
        float pix = m_pi * x;
        return (a*m_piinv*m_piinv)/(x*x) * sinf(pix)*sinf(pix*ainv);
    }
};



class FilterLanczos3_2D : public Filter2D {
public:
    FilterLanczos3_2D (float /*width*/, float /*height*/)
        : Filter2D(6.0f,6.0f)
    { }
    ~FilterLanczos3_2D (void) { }
    float operator() (float x, float y) const {
        return FilterLanczos3_1D::lanczos3(x) * FilterLanczos3_1D::lanczos3(y);
    }
    bool separable (void) const { return true; }
    float xfilt (float x) const { return FilterLanczos3_1D::lanczos3(x); }
    float yfilt (float y) const { return FilterLanczos3_1D::lanczos3(y); }
    const std::string name (void) const { return "lanczos3"; }
};



class FilterRadialLanczos3_2D : public FilterLanczos3_2D {
public:
    FilterRadialLanczos3_2D (float /*width*/, float /*height*/)
        : FilterLanczos3_2D(6.0f,6.0f)
    { }
    float operator() (float x, float y) const {
        return FilterLanczos3_1D::lanczos3(sqrtf(x*x + y*y));
    }
    bool separable (void) const { return false; }
    const std::string name (void) const { return "radial-lanczos3"; }
};



class FilterMitchell1D : public Filter1D {
public:
    FilterMitchell1D (float width) : Filter1D(width) { }
    ~FilterMitchell1D (void) { }
    float operator() (float x) const {
        return mitchell1d (x * m_rad_inv);
    }
    const std::string name (void) const { return "mitchell"; }

    static float mitchell1d (float x) {
        x = fabsf (x);
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
private:
    float m_rad_inv;
};



class FilterMitchell2D : public Filter2D {
public:
    FilterMitchell2D (float width, float height) 
        : Filter2D(width,height),
          m_wrad_inv(2.0f/width), m_hrad_inv(2.0f/height) { }
    ~FilterMitchell2D (void) { }
    float operator() (float x, float y) const {
        return FilterMitchell1D::mitchell1d (x * m_wrad_inv)
             * FilterMitchell1D::mitchell1d (y * m_hrad_inv);
    }
    bool separable (void) const { return true; }
    float xfilt (float x) const {
        return FilterMitchell1D::mitchell1d (x * m_wrad_inv);
    }
    float yfilt (float y) const {
        return FilterMitchell1D::mitchell1d (y * m_hrad_inv);
    }
    const std::string name (void) const { return "mitchell"; }
private:
    float m_wrad_inv, m_hrad_inv;
};



// B-spline filter from Stark et al, JGT 10(1)
class FilterBSpline1D : public Filter1D {
public:
    FilterBSpline1D (float width)
        : Filter1D(width), m_wscale(4.0f/width)
    { }
    ~FilterBSpline1D (void) { }
    float operator() (float x) const {
        return bspline1d (x*m_wscale);
    }
    const std::string name (void) const { return "b-spline"; }

    static float bspline1d (float x) {
        x = fabsf (x);
        if (x <= 1.0f)
            return b1 (1.0f-x);
        else if (x < 2.0f)
            return b0 (2.0f-x);
        else return 0.0f;
    }
private:
    float m_wscale;  // width scale factor
    static float b0 (float t) { return t*t*t / 6.0f; }
    static float b1 (float t) {
        return 0.5f * t * (t * (1.0f - t) + 1.0f) + 1.0f/6.0f;
    }
};



class FilterBSpline2D : public Filter2D {
public:
    FilterBSpline2D (float width, float height) 
        : Filter2D(width,height), m_wscale(4.0f/width), m_hscale(4.0f/height)
    { }
    ~FilterBSpline2D (void) { }
    float operator() (float x, float y) const {
        return FilterBSpline1D::bspline1d (x * m_wscale)
             * FilterBSpline1D::bspline1d (y * m_hscale);
    }
    bool separable (void) const { return true; }
    float xfilt (float x) const {
        return FilterBSpline1D::bspline1d(x*m_wscale);
    }
    float yfilt (float y) const {
        return FilterBSpline1D::bspline1d(y*m_hscale);
    }
    const std::string name (void) const { return "b-spline"; }
private:
    float m_wscale, m_hscale;
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



namespace {
FilterDesc filter1d_list[] = {
    // name             dim width fixedwidth scalable separable
    { "box",             1,   1,    false,    true,     true },
    { "triangle",        1,   2,    false,    true,     true },
    { "gaussian",        1,   2,    false,    true,     true },
    { "catrom",          1,   4,    false,    false,    true },
    { "blackman-harris", 1,   3,    false,    true,     true },
    { "sinc",            1,   6,    false,    false,    true },
    { "lanczos3",        1,   6,    false,    false,    true },
    { "mitchell",        1,   3,    false,    true,     true },
    { "bspline",         1,   4,    false,    true,     true }
};
}

int
Filter1D::num_filters ()
{
    return sizeof(filter1d_list)/sizeof(filter1d_list[0]);
}

void
Filter1D::get_filterdesc (int filternum, FilterDesc *filterdesc)
{
    ASSERT (filternum >= 0 && filternum < num_filters());
    *filterdesc = filter1d_list[filternum];
}



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
    if (filtername == "catmull-rom" || filtername == "catrom")
        return new FilterCatmullRom1D (width);
    if (filtername == "blackman-harris")
        return new FilterBlackmanHarris1D (width);
    if (filtername == "sinc")
        return new FilterSinc1D (width);
    if (filtername == "lanczos3" || filtername == "lanczos")
        return new FilterLanczos3_1D (width);
    if (filtername == "mitchell")
        return new FilterMitchell1D (width);
    if (filtername == "b-spline" || filtername == "bspline")
        return new FilterBSpline1D (width);
    return NULL;
}



void
Filter1D::destroy (Filter1D *filt)
{
    delete filt;
}



static FilterDesc filter2d_list[] = {
    // name             dim width fixedwidth scalable separable
    { "box",             2,   1,    false,    true,     true  },
    { "triangle",        2,   2,    false,    true,     true  },
    { "gaussian",        2,   2,    false,    true,     true  },
    { "catrom",          2,   4,    true,     false,    true  },
    { "blackman-harris", 2,   3,    false,    true,     true  },
    { "sinc",            2,   6,    false,    false,    true  },
    { "lanczos3",        2,   6,    true,     false,    true  },
    { "radial-lanczos3", 2,   6,    true,     false,    false },
    { "mitchell",        2,   3,    false,    true,     true  },
    { "bspline",         2,   4,    false,    true,     true  },
    { "disk",            2,   1,    false,    true,     false }
};

int
Filter2D::num_filters ()
{
    return sizeof(filter2d_list)/sizeof(filter2d_list[0]);
}

void
Filter2D::get_filterdesc (int filternum, FilterDesc *filterdesc)
{
    ASSERT (filternum >= 0 && filternum < num_filters());
    *filterdesc = filter2d_list[filternum];
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
    if (filtername == "catmull-rom" || filtername == "catrom")
        return new FilterCatmullRom2D (width, height);
    if (filtername == "blackman-harris")
        return new FilterBlackmanHarris2D (width, height);
    if (filtername == "sinc")
        return new FilterSinc2D (width, height);
    if (filtername == "lanczos3" || filtername == "lanczos")
        return new FilterLanczos3_2D (width, height);
    if (filtername == "radial-lanczos3" || filtername == "radial-lanczos")
        return new FilterRadialLanczos3_2D (width, height);
    if (filtername == "mitchell")
        return new FilterMitchell2D (width, height);
    if (filtername == "b-spline" || filtername == "bspline")
        return new FilterBSpline2D (width, height);
    if (filtername == "disk")
        return new FilterDisk2D (width, height);
    return NULL;
}



void
Filter2D::destroy (Filter2D *filt)
{
    delete filt;
}


}
OIIO_NAMESPACE_EXIT
