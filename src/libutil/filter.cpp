// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


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

class FilterBox1D final : public Filter1D {
public:
    FilterBox1D(float width)
        : Filter1D(width > 0.0f ? width : 1.0f)
    {
    }
    ~FilterBox1D() override {}
    float operator()(float x) const override
    {
        return (fabsf(x) <= m_w * 0.5f) ? 1.0f : 0.0f;
    }
    string_view name() const override { return "box"; }
};



class FilterBox2D final : public Filter2D {
public:
    FilterBox2D(float width, float height)
        : Filter2D(width > 0.0f ? width : 1.0f, height > 0.0f ? height : 1.0f)
    {
    }
    ~FilterBox2D() override {}
    float operator()(float x, float y) const override
    {
        return (fabsf(x) <= m_w * 0.5f && fabsf(y) <= m_h * 0.5f) ? 1.0f : 0.0f;
    }
    bool separable() const override { return true; }
    float xfilt(float x) const override
    {
        return fabsf(x) <= m_w * 0.5f ? 1.0f : 0.0f;
    }
    float yfilt(float y) const override
    {
        return fabsf(y) <= m_h * 0.5f ? 1.0f : 0.0f;
    }
    string_view name() const override { return "box"; }
};



class FilterTriangle1D final : public Filter1D {
public:
    FilterTriangle1D(float width)
        : Filter1D(width > 0.0f ? width : 2.0f)
        , m_rad_inv(2.0f / m_w)
    {
    }
    ~FilterTriangle1D() override {}
    float operator()(float x) const override { return tri1d(x * m_rad_inv); }
    string_view name() const override { return "triangle"; }

    static float tri1d(float x)
    {
        x = fabsf(x);
        return (x < 1.0f) ? (1.0f - x) : 0.0f;
    }

private:
    const float m_rad_inv;
};



class FilterTriangle2D final : public Filter2D {
public:
    FilterTriangle2D(float width, float height)
        : Filter2D(width > 0.0f ? width : 2.0f, height > 0.0f ? height : 2.0f)
        , m_wrad_inv(2.0f / m_w)
        , m_hrad_inv(2.0f / m_h)
    {
    }
    ~FilterTriangle2D() override {}
    float operator()(float x, float y) const override
    {
        return FilterTriangle1D::tri1d(x * m_wrad_inv)
               * FilterTriangle1D::tri1d(y * m_hrad_inv);
    }
    bool separable() const override { return true; }
    float xfilt(float x) const override
    {
        return FilterTriangle1D::tri1d(x * m_wrad_inv);
    }
    float yfilt(float y) const override
    {
        return FilterTriangle1D::tri1d(y * m_hrad_inv);
    }
    string_view name() const override { return "triangle"; }

private:
    const float m_wrad_inv, m_hrad_inv;
};



class FilterGaussian1D final : public Filter1D {
public:
    FilterGaussian1D(float width)
        : Filter1D(width > 0.0f ? width : 3.0f)
        , m_rad_inv(2.0f / m_w)
    {
    }
    ~FilterGaussian1D() override {}
    float operator()(float x) const override { return gauss1d(x * m_rad_inv); }
    static float gauss1d(float x)
    {
        x = fabsf(x);
        return (x < 1.0f) ? fast_exp(-2.0f * (x * x)) : 0.0f;
    }
    string_view name() const override { return "gaussian"; }

private:
    float m_rad_inv;
};



class FilterGaussian2D final : public Filter2D {
public:
    FilterGaussian2D(float width, float height)
        : Filter2D(width > 0.0f ? width : 3.0f, height > 0.0f ? height : 3.0f)
        , m_wrad_inv(2.0f / m_w)
        , m_hrad_inv(2.0f / m_h)
    {
    }
    ~FilterGaussian2D() override {}
    float operator()(float x, float y) const override
    {
        return FilterGaussian1D::gauss1d(x * m_wrad_inv)
               * FilterGaussian1D::gauss1d(y * m_hrad_inv);
    }
    bool separable() const override { return true; }
    float xfilt(float x) const override
    {
        return FilterGaussian1D::gauss1d(x * m_wrad_inv);
    }
    float yfilt(float y) const override
    {
        return FilterGaussian1D::gauss1d(y * m_hrad_inv);
    }
    string_view name() const override { return "gaussian"; }

private:
    const float m_wrad_inv, m_hrad_inv;
};



class FilterSharpGaussian1D final : public Filter1D {
public:
    FilterSharpGaussian1D(float width)
        : Filter1D(width > 0.0f ? width : 2.0f)
        , m_rad_inv(2.0f / m_w)
    {
    }
    ~FilterSharpGaussian1D() override {}
    float operator()(float x) const override { return gauss1d(x * m_rad_inv); }
    static float gauss1d(float x)
    {
        x = fabsf(x);
        return (x < 1.0f) ? fast_exp(-4.0f * (x * x)) : 0.0f;
    }
    string_view name() const override { return "gaussian"; }

private:
    const float m_rad_inv;
};



class FilterSharpGaussian2D final : public Filter2D {
public:
    FilterSharpGaussian2D(float width, float height)
        : Filter2D(width > 0.0f ? width : 2.0f, height > 0.0f ? height : 2.0f)
        , m_wrad_inv(2.0f / m_w)
        , m_hrad_inv(2.0f / m_h)
    {
    }
    ~FilterSharpGaussian2D() override {}
    float operator()(float x, float y) const override
    {
        return FilterSharpGaussian1D::gauss1d(x * m_wrad_inv)
               * FilterSharpGaussian1D::gauss1d(y * m_hrad_inv);
    }
    bool separable() const override { return true; }
    float xfilt(float x) const override
    {
        return FilterSharpGaussian1D::gauss1d(x * m_wrad_inv);
    }
    float yfilt(float y) const override
    {
        return FilterSharpGaussian1D::gauss1d(y * m_hrad_inv);
    }
    string_view name() const override { return "gaussian"; }

private:
    const float m_wrad_inv, m_hrad_inv;
};



class FilterCatmullRom1D final : public Filter1D {
public:
    FilterCatmullRom1D(float width)
        : Filter1D(4.0f)
        , m_scale(4.0f / (width > 0.0f ? width : 4.0f))
    {
    }
    ~FilterCatmullRom1D() override {}
    float operator()(float x) const override { return catrom1d(x * m_scale); }
    string_view name() const override { return "catmull-rom"; }

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
    const float m_scale;
};



class FilterCatmullRom2D final : public Filter2D {
public:
    FilterCatmullRom2D(float width, float height)
        : Filter2D(width > 0.0f ? width : 4.0f, height > 0.0f ? height : 4.0f)
        , m_wscale(4.0f / m_w)
        , m_hscale(4.0f / m_h)
    {
    }
    ~FilterCatmullRom2D() override {}
    float operator()(float x, float y) const override
    {
        return FilterCatmullRom1D::catrom1d(x * m_wscale)
               * FilterCatmullRom1D::catrom1d(y * m_hscale);
    }
    bool separable() const override { return true; }
    float xfilt(float x) const override
    {
        return FilterCatmullRom1D::catrom1d(x * m_wscale);
    }
    float yfilt(float y) const override
    {
        return FilterCatmullRom1D::catrom1d(y * m_hscale);
    }
    string_view name() const override { return "catmull-rom"; }

private:
    const float m_wscale, m_hscale;
};



class FilterBlackmanHarris1D final : public Filter1D {
public:
    FilterBlackmanHarris1D(float width)
        : Filter1D(width > 0.0f ? width : 3.0f)
        , m_rad_inv(2.0f / m_w)
    {
    }
    ~FilterBlackmanHarris1D() override {}
    float operator()(float x) const override { return bh1d(x * m_rad_inv); }
    string_view name() const override { return "blackman-harris"; }
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
    const float m_rad_inv;
};



class FilterBlackmanHarris2D final : public Filter2D {
public:
    FilterBlackmanHarris2D(float width, float height)
        : Filter2D(width > 0.0f ? width : 3.0f, height > 0.0f ? height : 3.0f)
        , m_wrad_inv(2.0f / m_w)
        , m_hrad_inv(2.0f / m_h)
    {
    }
    ~FilterBlackmanHarris2D() override {}
    float operator()(float x, float y) const override
    {
        return FilterBlackmanHarris1D::bh1d(x * m_wrad_inv)
               * FilterBlackmanHarris1D::bh1d(y * m_hrad_inv);
    }
    bool separable() const override { return true; }
    float xfilt(float x) const override
    {
        return FilterBlackmanHarris1D::bh1d(x * m_wrad_inv);
    }
    float yfilt(float y) const override
    {
        return FilterBlackmanHarris1D::bh1d(y * m_hrad_inv);
    }
    string_view name() const override { return "blackman-harris"; }

private:
    const float m_wrad_inv, m_hrad_inv;
};



class FilterSinc1D final : public Filter1D {
public:
    FilterSinc1D(float width)
        : Filter1D(width > 0.0f ? width : 4.0f)
        , m_rad(m_w / 2.0f)
    {
    }
    ~FilterSinc1D() override {}
    float operator()(float x) const override { return sinc1d(x, m_rad); }
    string_view name() const override { return "sinc"; }

    static float sinc1d(float x, float rad)
    {
        x = fabsf(x);
        if (x > rad)
            return 0.0f;
        const float m_pi = float(M_PI);
        return (x < 0.0001f) ? 1.0f : sinf(m_pi * x) / (m_pi * x);
    }

private:
    const float m_rad;
};



class FilterSinc2D final : public Filter2D {
public:
    FilterSinc2D(float width, float height)
        : Filter2D(width > 0.0f ? width : 4.0f, height > 0.0f ? height : 4.0f)
        , m_wrad(m_w / 2.0f)
        , m_hrad(m_h / 2.0f)
    {
    }
    ~FilterSinc2D() override {}
    float operator()(float x, float y) const override
    {
        return FilterSinc1D::sinc1d(x, m_wrad)
               * FilterSinc1D::sinc1d(y, m_hrad);
    }
    bool separable() const override { return true; }
    float xfilt(float x) const override
    {
        return FilterSinc1D::sinc1d(x, m_wrad);
    }
    float yfilt(float y) const override
    {
        return FilterSinc1D::sinc1d(y, m_hrad);
    }
    string_view name() const override { return "sinc"; }

private:
    const float m_wrad, m_hrad;
};



class FilterLanczos3_1D final : public Filter1D {
public:
    FilterLanczos3_1D(float width)
        : Filter1D(width > 0.0f ? width : 6.0f)
        , m_scale(6.0f / m_w)
    {
    }
    ~FilterLanczos3_1D() override {}
    float operator()(float x) const override { return lanczos3(x * m_scale); }
    string_view name() const override { return "lanczos3"; }

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
    const float m_scale;
};



class FilterLanczos3_2D final : public Filter2D {
public:
    FilterLanczos3_2D(float width, float height)
        : Filter2D(width > 0.0f ? width : 6.0f, height > 0.0f ? height : 6.0f)
        , m_wscale(6.0f / m_w)
        , m_hscale(6.0f / m_h)
    {
    }
    ~FilterLanczos3_2D() override {}
    float operator()(float x, float y) const override
    {
        return FilterLanczos3_1D::lanczos3(x * m_wscale)
               * FilterLanczos3_1D::lanczos3(y * m_hscale);
    }
    bool separable() const override { return true; }
    float xfilt(float x) const override
    {
        return FilterLanczos3_1D::lanczos3(x * m_wscale);
    }
    float yfilt(float y) const override
    {
        return FilterLanczos3_1D::lanczos3(y * m_hscale);
    }
    string_view name() const override { return "lanczos3"; }

protected:
    const float m_wscale, m_hscale;
};



class FilterRadialLanczos3_2D final : public Filter2D {
public:
    FilterRadialLanczos3_2D(float width, float height)
        : Filter2D(width > 0.0f ? width : 6.0f, height > 0.0f ? height : 6.0f)
        , m_wscale(6.0f / m_w)
        , m_hscale(6.0f / m_h)
    {
    }
    float operator()(float x, float y) const override
    {
        x *= m_wscale;
        y *= m_hscale;
        return FilterLanczos3_1D::lanczos3(sqrtf(x * x + y * y));
    }
    bool separable() const override { return false; }
    float xfilt(float x) const override
    {
        return FilterLanczos3_1D::lanczos3(x * m_wscale);
    }
    float yfilt(float y) const override
    {
        return FilterLanczos3_1D::lanczos3(y * m_hscale);
    }
    string_view name() const override { return "radial-lanczos3"; }

protected:
    const float m_wscale, m_hscale;
};



class FilterMitchell1D final : public Filter1D {
public:
    FilterMitchell1D(float width)
        : Filter1D(width > 0.0f ? width : 4.0f)
        , m_rad_inv(2.0f / m_w)
    {
    }
    ~FilterMitchell1D() override {}
    float operator()(float x) const override
    {
        return mitchell1d(x * m_rad_inv);
    }
    string_view name() const override { return "mitchell"; }

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
    const float m_rad_inv;
};



class FilterMitchell2D final : public Filter2D {
public:
    FilterMitchell2D(float width, float height)
        : Filter2D(width > 0.0f ? width : 4.0f, height > 0.0f ? height : 4.0f)
        , m_wrad_inv(2.0f / m_w)
        , m_hrad_inv(2.0f / m_h)
    {
    }
    ~FilterMitchell2D() override {}
    float operator()(float x, float y) const override
    {
        return FilterMitchell1D::mitchell1d(x * m_wrad_inv)
               * FilterMitchell1D::mitchell1d(y * m_hrad_inv);
    }
    bool separable() const override { return true; }
    float xfilt(float x) const override
    {
        return FilterMitchell1D::mitchell1d(x * m_wrad_inv);
    }
    float yfilt(float y) const override
    {
        return FilterMitchell1D::mitchell1d(y * m_hrad_inv);
    }
    string_view name() const override { return "mitchell"; }

private:
    const float m_wrad_inv, m_hrad_inv;
};



// B-spline filter from Stark et al, JGT 10(1)
class FilterBSpline1D final : public Filter1D {
public:
    FilterBSpline1D(float width)
        : Filter1D(width > 0.0f ? width : 4.0f)
        , m_wscale(4.0f / m_w)
    {
    }
    ~FilterBSpline1D() override {}
    float operator()(float x) const override { return bspline1d(x * m_wscale); }
    string_view name() const override { return "b-spline"; }

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
    const float m_wscale;  // width scale factor
    static float b0(float t) { return t * t * t / 6.0f; }
    static float b1(float t)
    {
        return 0.5f * t * (t * (1.0f - t) + 1.0f) + 1.0f / 6.0f;
    }
};



class FilterBSpline2D final : public Filter2D {
public:
    FilterBSpline2D(float width, float height)
        : Filter2D(width > 0.0f ? width : 4.0f, height > 0.0f ? height : 4.0f)
        , m_wscale(4.0f / m_w)
        , m_hscale(4.0f / m_h)
    {
    }
    ~FilterBSpline2D() override {}
    float operator()(float x, float y) const override
    {
        return FilterBSpline1D::bspline1d(x * m_wscale)
               * FilterBSpline1D::bspline1d(y * m_hscale);
    }
    bool separable() const override { return true; }
    float xfilt(float x) const override
    {
        return FilterBSpline1D::bspline1d(x * m_wscale);
    }
    float yfilt(float y) const override
    {
        return FilterBSpline1D::bspline1d(y * m_hscale);
    }
    string_view name() const override { return "b-spline"; }

private:
    const float m_wscale, m_hscale;
};



class FilterDisk2D final : public Filter2D {
public:
    FilterDisk2D(float width, float height)
        : Filter2D(width > 0.0f ? width : 1.0f, height > 0.0f ? height : 1.0f)
    {
    }
    ~FilterDisk2D() override {}
    float operator()(float x, float y) const override
    {
        x /= (m_w * 0.5f);
        y /= (m_h * 0.5f);
        return ((x * x + y * y) < 1.0f) ? 1.0f : 0.0f;
    }
    string_view name() const override { return "disk"; }
};


class FilterCubic1D : public Filter1D {
public:
    FilterCubic1D(float width, float a = 0.0f)
        : Filter1D(width > 0.0f ? width : 4.0f)
        , m_a(a)
        , m_rad_inv(2.0f / m_w)
    {
    }
    ~FilterCubic1D() override {}
    float operator()(float x) const override
    {
        return cubic(x * m_rad_inv, m_a);
    }

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

    string_view name() const override { return "cubic"; }

protected:
    const float m_a;
    const float m_rad_inv;
};



class FilterCubic2D : public Filter2D {
public:
    FilterCubic2D(float width, float height, float a = 0.0f)
        : Filter2D(width > 0.0f ? width : 4.0f, height > 0.0f ? height : 4.0f)
        , m_a(a)
        , m_wrad_inv(2.0f / m_w)
        , m_hrad_inv(2.0f / m_h)
    {
    }
    ~FilterCubic2D() override {}
    float operator()(float x, float y) const override
    {
        return FilterCubic1D::cubic(x * m_wrad_inv, m_a)
               * FilterCubic1D::cubic(y * m_hrad_inv, m_a);
    }
    bool separable() const override { return true; }
    float xfilt(float x) const override
    {
        return FilterCubic1D::cubic(x * m_wrad_inv, m_a);
    }
    float yfilt(float y) const override
    {
        return FilterCubic1D::cubic(y * m_hrad_inv, m_a);
    }
    string_view name() const override { return "cubic"; }

protected:
    const float m_a;
    const float m_wrad_inv, m_hrad_inv;
};



class FilterKeys1D final : public FilterCubic1D {
public:
    FilterKeys1D(float width)
        : FilterCubic1D(width, -0.5f)
    {
    }
    ~FilterKeys1D() override {}
    string_view name() const override { return "keys"; }
};


class FilterKeys2D final : public FilterCubic2D {
public:
    FilterKeys2D(float width, float height)
        : FilterCubic2D(width, height, -0.5f)
    {
    }
    ~FilterKeys2D() override {}
    string_view name() const override { return "keys"; }
};



class FilterSimon1D final : public FilterCubic1D {
public:
    FilterSimon1D(float width)
        : FilterCubic1D(width, -0.75f)
    {
    }
    ~FilterSimon1D() override {}
    string_view name() const override { return "simon"; }
};


class FilterSimon2D final : public FilterCubic2D {
public:
    FilterSimon2D(float width, float height)
        : FilterCubic2D(width, height, -0.75f)
    {
    }
    ~FilterSimon2D() override {}
    string_view name() const override { return "simon"; }
};



class FilterRifman1D final : public FilterCubic1D {
public:
    FilterRifman1D(float width)
        : FilterCubic1D(width, -1.0f)
    {
    }
    ~FilterRifman1D() override {}
    string_view name() const override { return "rifman"; }
};


class FilterRifman2D final : public FilterCubic2D {
public:
    FilterRifman2D(float width, float height)
        : FilterCubic2D(width, height, -1.0f)
    {
    }
    ~FilterRifman2D() override {}
    string_view name() const override { return "rifman"; }
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



std::shared_ptr<const Filter1D>
Filter1D::create_shared(string_view filtername, float width)
{
    return std::shared_ptr<const Filter1D>(create(filtername, width),
                                           Filter1D::destroy);
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



std::shared_ptr<const Filter2D>
Filter2D::create_shared(string_view filtername, float width, float height)
{
    return std::shared_ptr<const Filter2D>(create(filtername, width, height),
                                           Filter2D::destroy);
}



// Filter2D::create is the static method that, given a filter name,
// width, and height, returns an allocated and instantiated filter of
// the correct implementation.  If the name is not recognized, return
// NULL.

Filter2D*
Filter2D::create(string_view filtername, float width, float height)
{
    if (height <= 0.0f)
        height = width;
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
