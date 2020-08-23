// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md


// Filter results comparison
//     pow2 downres (4K -> 128 tested)
//     - katana 2.7.12
//     - ImageMagick 6.3.6
//     - prman 16.0 txmake
//
// box: oiio, prman, katana, imagemagick match
// lanczos3: oiio, katana, imagemagick match.  prman is far sharper
//   (perhaps lanczos2?). Note that Nuke calls this filter "lanczos6" (they
//   measure full width).
// sinc: oiio, prman match.  Katana is slightly softer. imagemagick is much softer
// blackman harris:  all differ. In order of decreasing sharpness... imagemagick, oiio, prman
// catrom: oiio, imagemagick, prman match
// gaussian: prman, katana match (gaussian3).  imgmagick, oiio are sharper (gaussian2)



#include <cmath>
#include <cstring>
#include <iostream>
#include <string>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/filter.h>
#include <OpenImageIO/fmath.h>


OIIO_NAMESPACE_BEGIN

// Below are the implementations of several 2D filters.  They all
// inherit their interface from Filter2D.  Each must redefine two
// virtual functions:
//    char *name()                     Return the filter name
//    float operator(float,float)      Evaluate the filter
//

class FilterBox1D : public Filter1D {
public:
    FilterBox1D(float width)
        : Filter1D(width)
    {
    }
    ~FilterBox1D(void) {}
    float operator()(float x) const
    {
        return (fabsf(x) <= m_w * 0.5f) ? 1.0f : 0.0f;
    }
    string_view name(void) const { return "box"; }
};



class FilterBox2D : public Filter2D {
public:
    FilterBox2D(float width, float height)
        : Filter2D(width, height)
    {
    }
    ~FilterBox2D(void) {}
    float operator()(float x, float y) const
    {
        return (fabsf(x) <= m_w * 0.5f && fabsf(y) <= m_h * 0.5f) ? 1.0f : 0.0f;
    }
    bool separable(void) const { return true; }
    float xfilt(float x) const { return fabsf(x) <= m_w * 0.5f ? 1.0f : 0.0f; }
    float yfilt(float y) const { return fabsf(y) <= m_h * 0.5f ? 1.0f : 0.0f; }
    string_view name(void) const { return "box"; }
};



class FilterTriangle1D : public Filter1D {
public:
    FilterTriangle1D(float width)
        : Filter1D(width)
        , m_rad_inv(2.0f / width)
    {
    }
    ~FilterTriangle1D(void) {}
    float operator()(float x) const { return tri1d(x * m_rad_inv); }
    string_view name(void) const { return "triangle"; }

    static float tri1d(float x)
    {
        x = fabsf(x);
        return (x < 1.0f) ? (1.0f - x) : 0.0f;
    }

private:
    float m_rad_inv;
};



class FilterTriangle2D : public Filter2D {
public:
    FilterTriangle2D(float width, float height)
        : Filter2D(width, height)
        , m_wrad_inv(2.0f / width)
        , m_hrad_inv(2.0f / height)
    {
    }
    ~FilterTriangle2D(void) {}
    float operator()(float x, float y) const
    {
        return FilterTriangle1D::tri1d(x * m_wrad_inv)
               * FilterTriangle1D::tri1d(y * m_hrad_inv);
    }
    bool separable(void) const { return true; }
    float xfilt(float x) const
    {
        return FilterTriangle1D::tri1d(x * m_wrad_inv);
    }
    float yfilt(float y) const
    {
        return FilterTriangle1D::tri1d(y * m_hrad_inv);
    }
    string_view name(void) const { return "triangle"; }

private:
    float m_wrad_inv, m_hrad_inv;
};



class FilterGaussian1D : public Filter1D {
public:
    FilterGaussian1D(float width)
        : Filter1D(width)
        , m_rad_inv(2.0f / width)
    {
    }
    ~FilterGaussian1D(void) {}
    float operator()(float x) const { return gauss1d(x * m_rad_inv); }
    static float gauss1d(float x)
    {
        x = fabsf(x);
        return (x < 1.0f) ? fast_exp(-2.0f * (x * x)) : 0.0f;
    }
    string_view name(void) const { return "gaussian"; }

private:
    float m_rad_inv;
};



class FilterGaussian2D : public Filter2D {
public:
    FilterGaussian2D(float width, float height)
        : Filter2D(width, height)
        , m_wrad_inv(2.0f / width)
        , m_hrad_inv(2.0f / height)
    {
    }
    ~FilterGaussian2D(void) {}
    float operator()(float x, float y) const
    {
        return FilterGaussian1D::gauss1d(x * m_wrad_inv)
               * FilterGaussian1D::gauss1d(y * m_hrad_inv);
    }
    bool separable(void) const { return true; }
    float xfilt(float x) const
    {
        return FilterGaussian1D::gauss1d(x * m_wrad_inv);
    }
    float yfilt(float y) const
    {
        return FilterGaussian1D::gauss1d(y * m_hrad_inv);
    }
    string_view name(void) const { return "gaussian"; }

private:
    float m_wrad_inv, m_hrad_inv;
};



class FilterSharpGaussian1D : public Filter1D {
public:
    FilterSharpGaussian1D(float width)
        : Filter1D(width)
        , m_rad_inv(2.0f / width)
    {
    }
    ~FilterSharpGaussian1D(void) {}
    float operator()(float x) const { return gauss1d(x * m_rad_inv); }
    static float gauss1d(float x)
    {
        x = fabsf(x);
        return (x < 1.0f) ? fast_exp(-4.0f * (x * x)) : 0.0f;
    }
    string_view name(void) const { return "gaussian"; }

private:
    float m_rad_inv;
};



class FilterSharpGaussian2D : public Filter2D {
public:
    FilterSharpGaussian2D(float width, float height)
        : Filter2D(width, height)
        , m_wrad_inv(2.0f / width)
        , m_hrad_inv(2.0f / height)
    {
    }
    ~FilterSharpGaussian2D(void) {}
    float operator()(float x, float y) const
    {
        return FilterSharpGaussian1D::gauss1d(x * m_wrad_inv)
               * FilterSharpGaussian1D::gauss1d(y * m_hrad_inv);
    }
    bool separable(void) const { return true; }
    float xfilt(float x) const
    {
        return FilterSharpGaussian1D::gauss1d(x * m_wrad_inv);
    }
    float yfilt(float y) const
    {
        return FilterSharpGaussian1D::gauss1d(y * m_hrad_inv);
    }
    string_view name(void) const { return "gaussian"; }

private:
    float m_wrad_inv, m_hrad_inv;
};



class FilterCatmullRom1D : public Filter1D {
public:
    FilterCatmullRom1D(float width)
        : Filter1D(4.0f)
        , m_scale(4.0f / width)
    {
    }
    ~FilterCatmullRom1D(void) {}
    float operator()(float x) const { return catrom1d(x * m_scale); }
    string_view name(void) const { return "catmull-rom"; }

    static float catrom1d(float x)
    {
        x        = fabsf(x);
        float x2 = x * x;
        float x3 = x * x2;
        return (x >= 2.0f) ? 0.0f
                           : ((x < 1.0f) ? (3.0f * x3 - 5.0f * x2 + 2.0f)
                                         : (-x3 + 5.0f * x2 - 8.0f * x + 4.0f));
    }

private:
    float m_scale;
};



class FilterCatmullRom2D : public Filter2D {
public:
    FilterCatmullRom2D(float width, float height)
        : Filter2D(width, height)
        , m_wscale(4.0f / width)
        , m_hscale(4.0f / height)
    {
    }
    ~FilterCatmullRom2D(void) {}
    float operator()(float x, float y) const
    {
        return FilterCatmullRom1D::catrom1d(x * m_wscale)
               * FilterCatmullRom1D::catrom1d(y * m_hscale);
    }
    bool separable(void) const { return true; }
    float xfilt(float x) const
    {
        return FilterCatmullRom1D::catrom1d(x * m_wscale);
    }
    float yfilt(float y) const
    {
        return FilterCatmullRom1D::catrom1d(y * m_hscale);
    }
    string_view name(void) const { return "catmull-rom"; }

private:
    float m_wscale, m_hscale;
};



class FilterBlackmanHarris1D : public Filter1D {
public:
    FilterBlackmanHarris1D(float width)
        : Filter1D(width)
        , m_rad_inv(2.0f / width)
    {
    }
    ~FilterBlackmanHarris1D(void) {}
    float operator()(float x) const { return bh1d(x * m_rad_inv); }
    string_view name(void) const { return "blackman-harris"; }
    static float bh1d(float x)
    {
        if (x < -1.0f || x > 1.0f)  // Early out if outside filter range
            return 0.0f;
        // Compute BH.  Straight from classic BH paper, but the usual
        // formula assumes that the filter is centered at 0.5, so scale:
        x                = (x + 1.0f) * 0.5f;
        const float A0   = 0.35875f;
        const float A1   = -0.48829f;
        const float A2   = 0.14128f;
        const float A3   = -0.01168f;
        const float m_pi = float(M_PI);
#if 0
        // original -- three cos calls!
        return A0 + A1 * cosf(2.f * m_pi * x) 
             + A2 * cosf(4.f * m_pi * x) + A3 * cosf(6.f * m_pi * x);
#else
        // Use trig identities to reduce to just one cos.
        // https://en.wikipedia.org/wiki/List_of_trigonometric_identities
        // cos(2x) = 2 cos^2(x) - 1
        // cos(3x) = 4 cos^3(x) − 3 cos(x)
        float cos2pix = cosf(2.f * m_pi * x);
        float cos4pix = 2.0f * cos2pix * cos2pix - 1.0f;
        float cos6pix = cos2pix * (2.0f * cos4pix - 1.0f);
        return A0 + A1 * cos2pix + A2 * cos4pix + A3 * cos6pix;
#endif
    }

private:
    float m_rad_inv;
};



class FilterBlackmanHarris2D : public Filter2D {
public:
    FilterBlackmanHarris2D(float width, float height)
        : Filter2D(width, height)
        , m_wrad_inv(2.0f / width)
        , m_hrad_inv(2.0f / height)
    {
    }
    ~FilterBlackmanHarris2D(void) {}
    float operator()(float x, float y) const
    {
        return FilterBlackmanHarris1D::bh1d(x * m_wrad_inv)
               * FilterBlackmanHarris1D::bh1d(y * m_hrad_inv);
    }
    bool separable(void) const { return true; }
    float xfilt(float x) const
    {
        return FilterBlackmanHarris1D::bh1d(x * m_wrad_inv);
    }
    float yfilt(float y) const
    {
        return FilterBlackmanHarris1D::bh1d(y * m_hrad_inv);
    }
    string_view name(void) const { return "blackman-harris"; }

private:
    float m_wrad_inv, m_hrad_inv;
};



class FilterSinc1D : public Filter1D {
public:
    FilterSinc1D(float width)
        : Filter1D(width)
        , m_rad(width / 2.0f)
    {
    }
    ~FilterSinc1D(void) {}
    float operator()(float x) const { return sinc1d(x, m_rad); }
    string_view name(void) const { return "sinc"; }

    static float sinc1d(float x, float rad)
    {
        x = fabsf(x);
        if (x > rad)
            return 0.0f;
        const float m_pi = float(M_PI);
        return (x < 0.0001f) ? 1.0f : sinf(m_pi * x) / (m_pi * x);
    }

private:
    float m_rad;
};



class FilterSinc2D : public Filter2D {
public:
    FilterSinc2D(float width, float height)
        : Filter2D(width, height)
        , m_wrad(width / 2.0f)
        , m_hrad(height / 2.0f)
    {
    }
    ~FilterSinc2D(void) {}
    float operator()(float x, float y) const
    {
        return FilterSinc1D::sinc1d(x, m_wrad)
               * FilterSinc1D::sinc1d(y, m_hrad);
    }
    bool separable(void) const { return true; }
    float xfilt(float x) const { return FilterSinc1D::sinc1d(x, m_wrad); }
    float yfilt(float y) const { return FilterSinc1D::sinc1d(y, m_hrad); }
    string_view name(void) const { return "sinc"; }

private:
    float m_wrad, m_hrad;
};



class FilterLanczos3_1D : public Filter1D {
public:
    FilterLanczos3_1D(float width)
        : Filter1D(width)
        , m_scale(6.0f / width)
    {
    }
    ~FilterLanczos3_1D(void) {}
    float operator()(float x) const { return lanczos3(x * m_scale); }
    string_view name(void) const { return "lanczos3"; }

    static float lanczos3(float x)
    {
        const float a    = 3.0f;  // Lanczos 3 lobe
        const float ainv = 1.0f / a;
        const float m_pi = float(M_PI);
        x                = fabsf(x);
        if (x > a)
            return 0.0f;
        if (x < 0.0001f)
            return 1.0f;
#if 0
        // Full precision, for reference:
        float pix = m_pi * x;
        return a/(x*x*(m_pi*m_pi)) * sinf(pix)*sinf(pix*ainv);
#elif 0
        // Use approximate fast_sinphi -- about 0.1% absolute error, but
        // around 2.5x times faster. BUT when graphed, looks very icky near
        // f(0).
        return a / (x * x * (m_pi * m_pi)) * fast_sinpi(x)
               * fast_sinpi(x * ainv);
#else
        // Compromise: full-precision sin(), but use the trig identity
        //   sin(3x) = -4 sin^3(x) + 3 sin(x)
        // to make it so only one call to sin is sufficient. This is still
        // about 1.5x the speed of the reference implementation, but with
        // no precision compromises.
        float s1 = sinf(x * ainv * m_pi);          // sin(x*pi/a)
        float s3 = (-4.0f * s1 * s1 + 3.0f) * s1;  // sin(3*x*pi/a) == sin(x*pi)
        return a / (x * x * (m_pi * m_pi)) * s1 * s3;
#endif
    }

private:
    float m_scale;
};



class FilterLanczos3_2D : public Filter2D {
public:
    FilterLanczos3_2D(float width, float height)
        : Filter2D(width, height)
        , m_wscale(6.0f / width)
        , m_hscale(6.0f / height)
    {
    }
    ~FilterLanczos3_2D(void) {}
    float operator()(float x, float y) const
    {
        return FilterLanczos3_1D::lanczos3(x * m_wscale)
               * FilterLanczos3_1D::lanczos3(y * m_hscale);
    }
    bool separable(void) const { return true; }
    float xfilt(float x) const
    {
        return FilterLanczos3_1D::lanczos3(x * m_wscale);
    }
    float yfilt(float y) const
    {
        return FilterLanczos3_1D::lanczos3(y * m_hscale);
    }
    string_view name(void) const { return "lanczos3"; }

protected:
    float m_wscale, m_hscale;
};



class FilterRadialLanczos3_2D : public FilterLanczos3_2D {
public:
    FilterRadialLanczos3_2D(float width, float height)
        : FilterLanczos3_2D(width, height)
    {
    }
    float operator()(float x, float y) const
    {
        x *= m_wscale;
        y *= m_hscale;
        return FilterLanczos3_1D::lanczos3(sqrtf(x * x + y * y));
    }
    bool separable(void) const { return false; }
    string_view name(void) const { return "radial-lanczos3"; }
};



class FilterMitchell1D : public Filter1D {
public:
    FilterMitchell1D(float width)
        : Filter1D(width)
        , m_rad_inv(2.0f / width)
    {
    }
    ~FilterMitchell1D(void) {}
    float operator()(float x) const { return mitchell1d(x * m_rad_inv); }
    string_view name(void) const { return "mitchell"; }

    static float mitchell1d(float x)
    {
        x = fabsf(x);
        if (x > 1.0f)
            return 0.0f;
        // Computation straight out of the classic Mitchell paper.
        // In the paper, the range is -2 to 2, so we rescale:
        x *= 2.0f;
        float x2          = x * x;
        const float B     = 1.0f / 3.0f;
        const float C     = 1.0f / 3.0f;
        const float SIXTH = 1.0f / 6.0f;
        if (x >= 1.0f)
            return ((-B - 6.0f * C) * x * x2 + (6.0f * B + 30.0f * C) * x2
                    + (-12.0f * B - 48.0f * C) * x + (8.0f * B + 24.0f * C))
                   * SIXTH;
        else
            return ((12.0f - 9.0f * B - 6.0f * C) * x * x2
                    + (-18.0f + 12.0f * B + 6.0f * C) * x2 + (6.0f - 2.0f * B))
                   * SIXTH;
    }

private:
    float m_rad_inv;
};



class FilterMitchell2D : public Filter2D {
public:
    FilterMitchell2D(float width, float height)
        : Filter2D(width, height)
        , m_wrad_inv(2.0f / width)
        , m_hrad_inv(2.0f / height)
    {
    }
    ~FilterMitchell2D(void) {}
    float operator()(float x, float y) const
    {
        return FilterMitchell1D::mitchell1d(x * m_wrad_inv)
               * FilterMitchell1D::mitchell1d(y * m_hrad_inv);
    }
    bool separable(void) const { return true; }
    float xfilt(float x) const
    {
        return FilterMitchell1D::mitchell1d(x * m_wrad_inv);
    }
    float yfilt(float y) const
    {
        return FilterMitchell1D::mitchell1d(y * m_hrad_inv);
    }
    string_view name(void) const { return "mitchell"; }

private:
    float m_wrad_inv, m_hrad_inv;
};



// B-spline filter from Stark et al, JGT 10(1)
class FilterBSpline1D : public Filter1D {
public:
    FilterBSpline1D(float width)
        : Filter1D(width)
        , m_wscale(4.0f / width)
    {
    }
    ~FilterBSpline1D(void) {}
    float operator()(float x) const { return bspline1d(x * m_wscale); }
    string_view name(void) const { return "b-spline"; }

    static float bspline1d(float x)
    {
        x = fabsf(x);
        if (x <= 1.0f)
            return b1(1.0f - x);
        else if (x < 2.0f)
            return b0(2.0f - x);
        else
            return 0.0f;
    }

private:
    float m_wscale;  // width scale factor
    static float b0(float t) { return t * t * t / 6.0f; }
    static float b1(float t)
    {
        return 0.5f * t * (t * (1.0f - t) + 1.0f) + 1.0f / 6.0f;
    }
};



class FilterBSpline2D : public Filter2D {
public:
    FilterBSpline2D(float width, float height)
        : Filter2D(width, height)
        , m_wscale(4.0f / width)
        , m_hscale(4.0f / height)
    {
    }
    ~FilterBSpline2D(void) {}
    float operator()(float x, float y) const
    {
        return FilterBSpline1D::bspline1d(x * m_wscale)
               * FilterBSpline1D::bspline1d(y * m_hscale);
    }
    bool separable(void) const { return true; }
    float xfilt(float x) const
    {
        return FilterBSpline1D::bspline1d(x * m_wscale);
    }
    float yfilt(float y) const
    {
        return FilterBSpline1D::bspline1d(y * m_hscale);
    }
    string_view name(void) const { return "b-spline"; }

private:
    float m_wscale, m_hscale;
};



class FilterDisk2D : public Filter2D {
public:
    FilterDisk2D(float width, float height)
        : Filter2D(width, height)
    {
    }
    ~FilterDisk2D(void) {}
    float operator()(float x, float y) const
    {
        x /= (m_w * 0.5f);
        y /= (m_h * 0.5f);
        return ((x * x + y * y) < 1.0f) ? 1.0f : 0.0f;
    }
    string_view name(void) const { return "disk"; }
};


class FilterCubic1D : public Filter1D {
public:
    FilterCubic1D(float width)
        : Filter1D(width)
        , m_a(0.0f)
        , m_rad_inv(2.0f / width)
    {
    }
    ~FilterCubic1D(void) {}
    float operator()(float x) const { return cubic(x * m_rad_inv, m_a); }

    static float cubic(float x, float a)
    {
        x = fabsf(x);
        if (x > 1.0f)
            return 0.0f;
        // Because range is -2 to 2, we rescale
        x *= 2.0f;

        if (x >= 1.0f)
            return a * (x * (x * (x - 5.0f) + 8.0f) - 4.0f);
        // return a * x*x*x - 5.0f * a * x*x + 8.0f * a * x - 4.0f * a;
        else
            return x * x * ((a + 2.0f) * x - (a + 3.0f)) + 1.0f;
        // return (a + 2.0f) * x*x*x - (a + 3.0f) * x*x + 1.0f;
    }

    virtual string_view name(void) const { return "cubic"; }

protected:
    float m_a;
    float m_rad_inv;
};



class FilterCubic2D : public Filter2D {
public:
    FilterCubic2D(float width, float height)
        : Filter2D(width, height)
        , m_a(0.0f)
        , m_wrad_inv(2.0f / width)
        , m_hrad_inv(2.0f / height)
    {
    }
    ~FilterCubic2D(void) {}
    float operator()(float x, float y) const
    {
        return FilterCubic1D::cubic(x * m_wrad_inv, m_a)
               * FilterCubic1D::cubic(y * m_hrad_inv, m_a);
    }
    bool separable(void) const { return true; }
    float xfilt(float x) const
    {
        return FilterCubic1D::cubic(x * m_wrad_inv, m_a);
    }
    float yfilt(float y) const
    {
        return FilterCubic1D::cubic(y * m_hrad_inv, m_a);
    }
    virtual string_view name(void) const { return "cubic"; }

protected:
    float m_a;
    float m_wrad_inv, m_hrad_inv;
};



class FilterKeys1D : public FilterCubic1D {
public:
    FilterKeys1D(float width)
        : FilterCubic1D(width)
    {
        m_a = -0.5f;
    }
    ~FilterKeys1D(void) {}
    virtual string_view name(void) const { return "keys"; }
};


class FilterKeys2D : public FilterCubic2D {
public:
    FilterKeys2D(float width, float height)
        : FilterCubic2D(width, height)
    {
        m_a = -0.5f;
    }
    ~FilterKeys2D(void) {}
    virtual string_view name(void) const { return "keys"; }
};



class FilterSimon1D : public FilterCubic1D {
public:
    FilterSimon1D(float width)
        : FilterCubic1D(width)
    {
        m_a = -0.75f;
    }
    ~FilterSimon1D(void) {}
    virtual string_view name(void) const { return "simon"; }
};


class FilterSimon2D : public FilterCubic2D {
public:
    FilterSimon2D(float width, float height)
        : FilterCubic2D(width, height)
    {
        m_a = -0.75f;
    }
    ~FilterSimon2D(void) {}
    virtual string_view name(void) const { return "simon"; }
};



class FilterRifman1D : public FilterCubic1D {
public:
    FilterRifman1D(float width)
        : FilterCubic1D(width)
    {
        m_a = -1.0f;
    }
    ~FilterRifman1D(void) {}
    virtual string_view name(void) const { return "rifman"; }
};


class FilterRifman2D : public FilterCubic2D {
public:
    FilterRifman2D(float width, float height)
        : FilterCubic2D(width, height)
    {
        m_a = -1.0f;
    }
    ~FilterRifman2D(void) {}
    virtual string_view name(void) const { return "rifman"; }
};



namespace {
FilterDesc filter1d_list[] = {
    // clang-format off
    // name             dim width fixedwidth scalable separable
    { "box",             1,   1,    false,    true,     true },
    { "triangle",        1,   2,    false,    true,     true },
    { "gaussian",        1,   3,    false,    true,     true },
    { "sharp-gaussian",  1,   2,    false,    true,     true },
    { "catmull-rom",     1,   4,    false,    true,     true },
    { "blackman-harris", 1,   3,    false,    true,     true },
    { "sinc",            1,   4,    false,    true,     true },
    { "lanczos3",        1,   6,    false,    true,     true },
    { "nuke-lanczos6",   1,   6,    false,    true,     true },
    { "mitchell",        1,   4,    false,    true,     true },
    { "bspline",         1,   4,    false,    true,     true },
    { "cubic",           1,   4,    false,    true,     true },
    { "keys",            1,   4,    false,    true,     true },
    { "simon",           1,   4,    false,    true,     true },
    { "rifman",          1,   4,    false,    true,     true }
    // clang-format on
};
}

int
Filter1D::num_filters()
{
    return sizeof(filter1d_list) / sizeof(filter1d_list[0]);
}

const FilterDesc&
Filter1D::get_filterdesc(int filternum)
{
    OIIO_DASSERT(filternum >= 0 && filternum < num_filters());
    return filter1d_list[filternum];
}

void
Filter1D::get_filterdesc(int filternum, FilterDesc* filterdesc)
{
    *filterdesc = get_filterdesc(filternum);
}



// Filter1D::create is the static method that, given a filter name,
// width, and height, returns an allocated and instantiated filter of
// the correct implementation.  If the name is not recognized, return
// NULL.
Filter1D*
Filter1D::create(string_view filtername, float width)
{
    if (filtername == "box")
        return new FilterBox1D(width);
    if (filtername == "triangle")
        return new FilterTriangle1D(width);
    if (filtername == "gaussian")
        return new FilterGaussian1D(width);
    if (filtername == "sharp-gaussian")
        return new FilterSharpGaussian1D(width);
    if (filtername == "catmull-rom" || filtername == "catrom")
        return new FilterCatmullRom1D(width);
    if (filtername == "blackman-harris")
        return new FilterBlackmanHarris1D(width);
    if (filtername == "sinc")
        return new FilterSinc1D(width);
    if (filtername == "lanczos3" || filtername == "lanczos"
        || filtername == "nuke-lanczos6")
        return new FilterLanczos3_1D(width);
    if (filtername == "mitchell")
        return new FilterMitchell1D(width);
    if (filtername == "b-spline" || filtername == "bspline")
        return new FilterBSpline1D(width);
    if (filtername == "cubic")
        return new FilterCubic1D(width);
    if (filtername == "keys")
        return new FilterKeys1D(width);
    if (filtername == "simon")
        return new FilterSimon1D(width);
    if (filtername == "rifman")
        return new FilterRifman1D(width);
    return NULL;
}



void
Filter1D::destroy(Filter1D* filt)
{
    delete filt;
}



static FilterDesc filter2d_list[] = {
    // clang-format off
    // name             dim width fixedwidth scalable separable
    { "box",             2,   1,    false,    true,     true  },
    { "triangle",        2,   2,    false,    true,     true  },
    { "gaussian",        2,   3,    false,    true,     true  },
    { "sharp-gaussian",  2,   2,    false,    true,     true  },
    { "catmull-rom",     2,   4,    false,    true,     true  },
    { "blackman-harris", 2,   3,    false,    true,     true  },
    { "sinc",            2,   4,    false,    true,     true  },
    { "lanczos3",        2,   6,    false,    true,     true  },
    { "radial-lanczos3", 2,   6,    false,    true,     false },
    { "nuke-lanczos6",   2,   6,    false,    true,     false },
    { "mitchell",        2,   4,    false,    true,     true  },
    { "bspline",         2,   4,    false,    true,     true  },
    { "disk",            2,   1,    false,    true,     false },
    { "cubic",           2,   4,    false,    true,     true  },
    { "keys",            2,   4,    false,    true,     true  },
    { "simon",           2,   4,    false,    true,     true  },
    { "rifman",          2,   4,    false,    true,     true  }
    // clang-format on
};

int
Filter2D::num_filters()
{
    return sizeof(filter2d_list) / sizeof(filter2d_list[0]);
}

const FilterDesc&
Filter2D::get_filterdesc(int filternum)
{
    OIIO_DASSERT(filternum >= 0 && filternum < num_filters());
    return filter2d_list[filternum];
}

void
Filter2D::get_filterdesc(int filternum, FilterDesc* filterdesc)
{
    *filterdesc = get_filterdesc(filternum);
}



// Filter2D::create is the static method that, given a filter name,
// width, and height, returns an allocated and instantiated filter of
// the correct implementation.  If the name is not recognized, return
// NULL.

Filter2D*
Filter2D::create(string_view filtername, float width, float height)
{
    if (filtername == "box")
        return new FilterBox2D(width, height);
    if (filtername == "triangle")
        return new FilterTriangle2D(width, height);
    if (filtername == "gaussian")
        return new FilterGaussian2D(width, height);
    if (filtername == "sharp-gaussian")
        return new FilterSharpGaussian2D(width, height);
    if (filtername == "catmull-rom" || filtername == "catrom")
        return new FilterCatmullRom2D(width, height);
    if (filtername == "blackman-harris")
        return new FilterBlackmanHarris2D(width, height);
    if (filtername == "sinc")
        return new FilterSinc2D(width, height);
    if (filtername == "lanczos3" || filtername == "lanczos"
        || filtername == "nuke-lanczos6")
        return new FilterLanczos3_2D(width, height);
    if (filtername == "radial-lanczos3" || filtername == "radial-lanczos")
        return new FilterRadialLanczos3_2D(width, height);
    if (filtername == "mitchell")
        return new FilterMitchell2D(width, height);
    if (filtername == "b-spline" || filtername == "bspline")
        return new FilterBSpline2D(width, height);
    if (filtername == "disk")
        return new FilterDisk2D(width, height);
    if (filtername == "cubic")
        return new FilterCubic2D(width, height);
    if (filtername == "keys")
        return new FilterKeys2D(width, height);
    if (filtername == "simon")
        return new FilterSimon2D(width, height);
    if (filtername == "rifman")
        return new FilterRifman2D(width, height);
    return NULL;
}



void
Filter2D::destroy(Filter2D* filt)
{
    delete filt;
}


OIIO_NAMESPACE_END
