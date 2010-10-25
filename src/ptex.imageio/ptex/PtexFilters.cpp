/* 
PTEX SOFTWARE
Copyright 2009 Disney Enterprises, Inc.  All rights reserved

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.

  * The names "Disney", "Walt Disney Pictures", "Walt Disney Animation
    Studios" or the names of its contributors may NOT be used to
    endorse or promote products derived from this software without
    specific prior written permission from Walt Disney Pictures.

Disclaimer: THIS SOFTWARE IS PROVIDED BY WALT DISNEY PICTURES AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE, NONINFRINGEMENT AND TITLE ARE DISCLAIMED.
IN NO EVENT SHALL WALT DISNEY PICTURES, THE COPYRIGHT HOLDER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND BASED ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
*/

#include "PtexPlatform.h"
#include "Ptexture.h"
#include "PtexSeparableFilter.h"
#include "PtexSeparableKernel.h"
#include "PtexTriangleFilter.h"

namespace {

/** Point-sampling filter for rectangular textures */
class PtexPointFilter : public PtexFilter, public Ptex
{
 public:
    PtexPointFilter(PtexTexture* tx) : _tx(tx) {}
    virtual void release() { delete this; }
    virtual void eval(float* result, int firstchan, int nchannels,
		      int faceid, float u, float v,
		      float /*uw1*/, float /*vw1*/, float /*uw2*/, float /*vw2*/,
		      float /*width*/, float /*blur*/)
    {
	if (!_tx || nchannels <= 0) return;
	if (faceid < 0 || faceid >= _tx->numFaces()) return;
	const FaceInfo& f = _tx->getFaceInfo(faceid);
	int resu = f.res.u(), resv = f.res.v();
	int ui = PtexUtils::clamp(int(u*resu), 0, resu-1);
	int vi = PtexUtils::clamp(int(v*resv), 0, resv-1);
	_tx->getPixel(faceid, ui, vi, result, firstchan, nchannels);
    }
    
 private:
    PtexTexture* _tx;
};


/** Point-sampling filter for triangular textures */
class PtexPointFilterTri : public PtexFilter, public Ptex
{
 public:
    PtexPointFilterTri(PtexTexture* tx) : _tx(tx) {}
    virtual void release() { delete this; }
    virtual void eval(float* result, int firstchan, int nchannels,
		      int faceid, float u, float v,
		      float /*uw1*/, float /*vw1*/, float /*uw2*/, float /*vw2*/,
		      float /*width*/, float /*blur*/)
    {
	if (!_tx || nchannels <= 0) return;
	if (faceid < 0 || faceid >= _tx->numFaces()) return;
	const FaceInfo& f = _tx->getFaceInfo(faceid);
	int res = f.res.u();
	int resm1 = res - 1;
	float ut = u * res, vt = v * res;
	int ui = PtexUtils::clamp(int(ut), 0, resm1);
	int vi = PtexUtils::clamp(int(vt), 0, resm1);
	float uf = ut - ui, vf = vt - vi;
	
	if (uf + vf <= 1.0) {
	    // "even" triangles are stored in lower-left half-texture
	    _tx->getPixel(faceid, ui, vi, result, firstchan, nchannels);
	}
	else {
	    // "odd" triangles are stored in upper-right half-texture
	    _tx->getPixel(faceid, resm1-vi, resm1-ui, result, firstchan, nchannels);
	}
    }
    
 private:
    PtexTexture* _tx;
};


/** Separable filter with width=4 support.

    The kernel width is calculated as a multiple of 4 times the filter
    width and the texture resolution is chosen such that each kernel
    axis has between 4 and 8.

    For kernel widths too large to handle (because the kernel would
    extend significantly beyond both sides of the face), a special
    Hermite smoothstep is used to interpolate the two nearest 2 samples
    along the affected axis (or axes).
*/
class PtexWidth4Filter : public PtexSeparableFilter
{
 public:
    typedef double KernelFn(double x, const double* c);

    PtexWidth4Filter(PtexTexture* tx, const PtexFilter::Options& opts, KernelFn k, const double* c = 0) 
	: PtexSeparableFilter(tx, opts), _k(k), _c(c) {}

    virtual void buildKernel(PtexSeparableKernel& k, float u, float v, float uw, float vw,
			     Res faceRes)
    {
	buildKernelAxis(k.res.ulog2, k.u, k.uw, k.ku, u, uw, faceRes.ulog2);
	buildKernelAxis(k.res.vlog2, k.v, k.vw, k.kv, v, vw, faceRes.vlog2);
    }

 private:

    double blur(double x)
    {
	// 2-unit (x in -1..1) cubic hermite kernel
	// this produces a blur roughly 1.5 times that of the 4-unit b-spline kernel
	x = fabs(x);
	return x < 1 ? (2*x-3)*x*x+1 : 0;
    }

    void buildKernelAxis(int8_t& k_ureslog2, int& k_u, int& k_uw, double* ku,
			 float u, float uw, int f_ureslog2)
    {
	// build 1 axis (note: "u" labels may repesent either u or v axis)

	// clamp filter width to no smaller than a texel
	uw = PtexUtils::max(uw, 1.0f/(1<<f_ureslog2));

	// compute desired texture res based on filter width
	k_ureslog2 = int(ceil(log2(1.0/uw)));
	int resu = 1 << k_ureslog2;
	double uwlo = 1.0/resu;         // smallest filter width for this res

	// compute lerp weights (amount to blend towards next-lower res)
	double lerp2 = _options.lerp ? (uw-uwlo)/uwlo : 0;
	double lerp1 = 1-lerp2;

	// adjust for large filter widths
	if (uw >= .25) {
	    if (uw < .5) {
		k_ureslog2 = 2;
		double upix = u * 4 - 0.5;
		int u1 = int(ceil(upix - 2)), u2 = int(ceil(upix + 2));
		u1 = u1 & ~1;	    // round down to even pair
		u2 = (u2 + 1) & ~1; // round up to even pair
		k_u = u1;
		k_uw = u2-u1;
		double x1 = u1-upix;
		for (int i = 0; i < k_uw; i+=2) {
		    double xa = x1 + i, xb = xa + 1, xc = (xa+xb)*0.25;
		    // spread the filter gradually to approach the next-lower-res width
		    // at uw = .5, s = 1.0; at uw = 1, s = 0.8
		    double s = 1.0/(uw + .75);
		    double ka = _k(xa, _c), kb = _k(xb, _c), kc = blur(xc*s);
		    ku[i] = ka * lerp1 + kc * lerp2;
		    ku[i+1] = kb * lerp1 + kc * lerp2;
		}
		return;
	    }
	    else if (uw < 1) {
		k_ureslog2 = 1;
		double upix = u * 2 - 0.5;
		k_u = int(floor(u - .5))*2;
		k_uw = 4;
		double x1 = k_u-upix;
		for (int i = 0; i < k_uw; i+=2) {
		    double xa = x1 + i, xb = xa + 1, xc = (xa+xb)*0.5;
		    // spread the filter gradually to approach the next-lower-res width
		    // at uw = .5, s = .8; at uw = 1, s = 0.5
		    double s = 1.0/(uw*1.5 + .5);
		    double ka = blur(xa*s), kb = blur(xb*s), kc = blur(xc*s);
		    ku[i] = ka * lerp1 + kc * lerp2;
		    ku[i+1] = kb * lerp1 + kc * lerp2;
		}
		return;
	    }
	    else {
		// use res 0 (1 texel per face) w/ no lerping
		// (future: use face-blended values for filter > 2)
		k_ureslog2 = 0;
		double upix = u - .5;
		k_uw = 2;
		double ui = floor(upix);
		k_u = int(ui);
		ku[0] = blur(upix-ui);
		ku[1] = 1-ku[0];
		return;
	    }
	}

	// convert from normalized coords to pixel coords
	double upix = u * resu - 0.5;
	double uwpix = uw * resu;

	// find integer pixel extent: [u,v] +/- [2*uw,2*vw]
	// (kernel width is 4 times filter width)
	double dupix = 2*uwpix;
	int u1 = int(ceil(upix - dupix)), u2 = int(ceil(upix + dupix));

	if (lerp2) {
	    // lerp kernel weights towards next-lower res
	    // extend kernel width to cover even pairs
	    u1 = u1 & ~1;
	    u2 = (u2 + 1) & ~1;
	    k_u = u1;
	    k_uw = u2-u1;

	    // compute kernel weights
	    double step = 1.0/uwpix, x1 = (u1-upix)*step;
	    for (int i = 0; i < k_uw; i+=2) {
		double xa = x1 + i*step, xb = xa + step, xc = (xa+xb)*0.5;
		double ka = _k(xa, _c), kb = _k(xb, _c), kc = _k(xc, _c);
		ku[i] = ka * lerp1 + kc * lerp2;
		ku[i+1] = kb * lerp1 + kc * lerp2;
	    }
	}
	else {
	    k_u = u1;
	    k_uw = u2-u1;
	    // compute kernel weights
	    double x1 = (u1-upix)/uwpix, step = 1.0/uwpix;
	    for (int i = 0; i < k_uw; i++) ku[i] = _k(x1 + i*step, _c);
	}
    }

    KernelFn* _k;		// kernel function
    const double* _c;		// kernel coefficients (if any)
};


/** Separable bicubic filter */
class PtexBicubicFilter : public PtexWidth4Filter
{
 public:
    PtexBicubicFilter(PtexTexture* tx, const PtexFilter::Options& opts, float sharpness)
	: PtexWidth4Filter(tx, opts, kernelFn, _coeffs)
    {
	// compute Cubic filter coefficients:
	// abs(x) < 1:
	//   1/6 * ((12 - 9*B - 6*C)*x^3 + (-18 + 12*B + 6*C)*x^2 + (6 - 2*B))
	//   == c[0]*x^3 + c[1]*x^2 + c[2]
	// abs(x) < 2:
	//   1/6 * ((-B - 6*C)*x^3 + (6*B + 30*C)*x^2 + (-12*B - 48*C)*x + (8*B + 24*C))
	//   == c[3]*x^3 + c[4]*x^2 + c[5]*x + c[6]
	// else: 0

	float B = 1 - sharpness; // choose C = (1-B)/2
	_coeffs[0] = 1.5 - B;
	_coeffs[1] = 1.5 * B - 2.5;
	_coeffs[2] = 1 - (1./3) * B;
	_coeffs[3] = (1./3) * B - 0.5;
	_coeffs[4] = 2.5 - 1.5 * B;
	_coeffs[5] = 2 * B - 4;
	_coeffs[6] = 2 - (2./3) * B;
    }

 private:
    static double kernelFn(double x, const double* c)
    {
	x = fabs(x);
	if (x < 1)      return (c[0]*x + c[1])*x*x + c[2];
	else if (x < 2) return ((c[3]*x + c[4])*x + c[5])*x + c[6];
	else            return 0;
    }

    double _coeffs[7]; // filter coefficients for current sharpness
};



/** Separable gaussian filter */
class PtexGaussianFilter : public PtexWidth4Filter
{
 public:
    PtexGaussianFilter(PtexTexture* tx, const PtexFilter::Options& opts)
	: PtexWidth4Filter(tx, opts, kernelFn) {}

 private:
    static double kernelFn(double x, const double*)
    {
	return exp(-2*x*x);
    }
};



/** Rectangular box filter.
    The box is convolved with the texels as area samples and thus the kernel function is
    actually trapezoidally shaped.
 */
class PtexBoxFilter : public PtexSeparableFilter
{
 public:
    PtexBoxFilter(PtexTexture* tx, const PtexFilter::Options& opts)
	: PtexSeparableFilter(tx, opts) {}

 protected:
    virtual void buildKernel(PtexSeparableKernel& k, float u, float v, float uw, float vw,
			     Res faceRes)
    {
	// clamp filter width to no larger than 1.0
	uw = PtexUtils::min(uw, 1.0f);
	vw = PtexUtils::min(vw, 1.0f);

	// clamp filter width to no smaller than a texel
	uw = PtexUtils::max(uw, 1.0f/(faceRes.u()));
	vw = PtexUtils::max(vw, 1.0f/(faceRes.v()));

	// compute desired texture res based on filter width
	int ureslog2 = int(ceil(log2(1.0/uw))),
	    vreslog2 = int(ceil(log2(1.0/vw)));
	Res res(ureslog2, vreslog2);
	k.res = res;
	
	// convert from normalized coords to pixel coords
	u = u * k.res.u();
	v = v * k.res.v();
	uw *= k.res.u();
	vw *= k.res.v();

	// find integer pixel extent: [u,v] +/- [uw/2,vw/2]
	// (box is 1 unit wide for a 1 unit filter period)
	double u1 = u - 0.5*uw, u2 = u + 0.5*uw;
	double v1 = v - 0.5*vw, v2 = v + 0.5*vw;
	double u1floor = floor(u1), u2ceil = ceil(u2);
	double v1floor = floor(v1), v2ceil = ceil(v2);
	k.u = int(u1floor);
	k.v = int(v1floor);
	k.uw = int(u2ceil)-k.u;
	k.vw = int(v2ceil)-k.v;

	// compute kernel weights along u and v directions
	computeWeights(k.ku, k.uw, 1-(u1-u1floor), 1-(u2ceil-u2));
	computeWeights(k.kv, k.vw, 1-(v1-v1floor), 1-(v2ceil-v2));
    }

 private:
    void computeWeights(double* kernel, int size, double f1, double f2)
    {
	assert(size >= 1 && size <= 3);

	if (size == 1) {
	    kernel[0] = f1 + f2 - 1;
	}
	else {
	    kernel[0] = f1;
	    for (int i = 1; i < size-1; i++) kernel[i] = 1.0;
	    kernel[size-1] = f2;
	}
    }
};


/** Bilinear filter (for rectangular textures) */
class PtexBilinearFilter : public PtexSeparableFilter
{
 public:
    PtexBilinearFilter(PtexTexture* tx, const PtexFilter::Options& opts)
	: PtexSeparableFilter(tx, opts) {}

 protected:
    virtual void buildKernel(PtexSeparableKernel& k, float u, float v, float uw, float vw,
			     Res faceRes)
    {
	// clamp filter width to no larger than 1.0
	uw = PtexUtils::min(uw, 1.0f);
	vw = PtexUtils::min(vw, 1.0f);

	// clamp filter width to no smaller than a texel
	uw = PtexUtils::max(uw, 1.0f/(faceRes.u()));
	vw = PtexUtils::max(vw, 1.0f/(faceRes.v()));

	// choose resolution closest to filter res
	// there are three choices of "closest" that come to mind:
	// 1) closest in terms of filter width, i.e. period of signal
	// 2) closest in terms of texel resolution, (1 / filter width), i.e. freq of signal
	// 3) closest in terms of resolution level (log2(1/filter width))
	// Choice (1) probably makes the most sense.  In log2 terms, that means you should
	// use the next higher level when the fractional part of the log2 res is > log2(1/.75),
	// and you should add 1-log2(1/.75) to round up.
	const double roundWidth = 0.5849625007211563; // 1-log2(1/.75)
	int ureslog2 = int(log2(1.0/uw) + roundWidth);
	int vreslog2 = int(log2(1.0/vw) + roundWidth);
	Res res(ureslog2, vreslog2);
	k.res = res;
	
	// convert from normalized coords to pixel coords
	double upix = u * k.res.u() - 0.5;
	double vpix = v * k.res.v() - 0.5;

	float ufloor = floor(upix);
	float vfloor = floor(vpix);
	k.u = int(ufloor);
	k.v = int(vfloor);
	k.uw = 2;
	k.vw = 2;

	// compute kernel weights
	float ufrac = upix-ufloor, vfrac = vpix-vfloor;
	k.ku[0] = 1 - ufrac;
	k.ku[1] = ufrac;
	k.kv[0] = 1 - vfrac;
	k.kv[1] = vfrac;
    }
};

} // end local namespace


PtexFilter* PtexFilter::getFilter(PtexTexture* tex, const PtexFilter::Options& opts)
{
    switch (tex->meshType()) {
    case Ptex::mt_quad:
	switch (opts.filter) {
	case f_point:       return new PtexPointFilter(tex);
	case f_bilinear:    return new PtexBilinearFilter(tex, opts);
	default:
	case f_box:         return new PtexBoxFilter(tex, opts);
	case f_gaussian:    return new PtexGaussianFilter(tex, opts);
	case f_bicubic:     return new PtexBicubicFilter(tex, opts, opts.sharpness);
	case f_bspline:     return new PtexBicubicFilter(tex, opts, 0.0);
	case f_catmullrom:  return new PtexBicubicFilter(tex, opts, 1.0);
	case f_mitchell:    return new PtexBicubicFilter(tex, opts, 2.0/3.0);
	}
	break;

    case Ptex::mt_triangle:
 	switch (opts.filter) {
 	case f_point:       return new PtexPointFilterTri(tex);
	default:            return new PtexTriangleFilter(tex, opts);
 	}
	break;
    }
    return 0;
}
