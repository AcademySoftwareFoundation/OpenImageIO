#ifndef PtexTriangleKernel_h
#define PtexTriangleKernel_h

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

#include <assert.h>
#include <algorithm>
#include <numeric>
#include "Ptexture.h"
#include "PtexUtils.h"

// kernel width as a multiple of filter width (should be between 3 and 4)
// for values below 3, the gaussian is not close to zero and a contour will be formed
// larger values are more expensive (proportional to width-squared)
static const float PtexTriangleKernelWidth = 3.5;


/// Triangle filter kernel iterator (in texel coords)
class PtexTriangleKernelIter : public Ptex {
 public:
    int rowlen;			// row length (in u)
    double u, v;		// uv center in texels
    int u1, v1, w1;		// uvw lower bounds
    int u2, v2, w2;		// uvw upper bounds
    double A,B,C;		// ellipse coefficients (F = 1)
    bool valid;			// footprint is valid (non-empty)
    double wscale;		// amount to scale weights by (proportional to texel area)
    double weight;		// accumulated weight

    void apply(double* dst, void* data, DataType dt, int nChan, int nTxChan)
    {
	// dispatch specialized apply function
	ApplyFn fn = applyFunctions[(nChan!=nTxChan)*20 + ((unsigned)nChan<=4)*nChan*4 + dt];
	fn(*this, dst, data, nChan, nTxChan);
    }

    void applyConst(double* dst, void* data, DataType dt, int nChan);

 private:

    typedef void (*ApplyFn)(PtexTriangleKernelIter& k, double* dst, void* data, int nChan, int nTxChan);
    static ApplyFn applyFunctions[40];
};


/// Triangle filter kernel (in normalized triangle coords)
class PtexTriangleKernel : public Ptex {
 public:
    Res res;			// desired resolution
    double u, v;		// uv filter center
    double u1, v1, w1;		// uvw lower bounds
    double u2, v2, w2;		// uvw upper bounds
    double A,B,C;		// ellipse coefficients (F = A*C-B*B/4)

    void set(Res resVal, double uVal, double vVal,
	     double u1Val, double v1Val, double w1Val,
	     double u2Val, double v2Val, double w2Val,
	     double AVal, double BVal, double CVal)
    {
	res = resVal;
	u = uVal; v = vVal;
	u1 = u1Val; v1 = v1Val; w1 = w1Val;
	u2 = u2Val; v2 = v2Val; w2 = w2Val;
	A = AVal; B = BVal; C = CVal;
    }

    void set(double uVal, double vVal,
	     double u1Val, double v1Val, double w1Val,
	     double u2Val, double v2Val, double w2Val)
    {
	u = uVal; v = vVal;
	u1 = u1Val; v1 = v1Val; w1 = w1Val;
	u2 = u2Val; v2 = v2Val; w2 = w2Val;
    }

    void setABC(double AVal, double BVal, double CVal)
    {
	A = AVal; B = BVal; C = CVal;
    }

    void splitU(PtexTriangleKernel& ka)
    {
	ka = *this;
	u1 = 0;
	ka.u2 = 0;
    }

    void splitV(PtexTriangleKernel& ka)
    {
	ka = *this;
	v1 = 0;
	ka.v2 = 0;
    }

    void splitW(PtexTriangleKernel& ka)
    {
	ka = *this;
	w1 = 0;
	ka.w2 = 0;
    }

    void rotate1()
    {
	// rotate ellipse where u'=w, v'=u, w'=v
	// (derived by converting to Barycentric form, rotating, and converting back)
	setABC(C, 2*C-B, A+C-B);
    }

    void rotate2()
    {
	// rotate ellipse where u'=v, v'=w, w'=u
	// (derived by converting to Barycentric form, rotating, and converting back)
	setABC(A+C-B, 2*A-B, A);
    }

    void reorient(int eid, int aeid)
    {
	double w = 1-u-v;

#define C(eid, aeid) (eid*3 + aeid)
	switch (C(eid, aeid)) {
	case C(0, 0): set(1-u,  -v, 1-u2,  -v2, 1-w2, 1-u1,  -v1, 1-w1); break;
	case C(0, 1): set(1-w, 1-u, 1-w2, 1-u2,  -v2, 1-w1, 1-u1,  -v1); rotate1(); break;
	case C(0, 2): set( -v, 1-w,  -v2, 1-w2, 1-u2,  -v1, 1-w1, 1-u1); rotate2(); break;

	case C(1, 0): set(1-v,  -w, 1-v2,  -w2, 1-u2, 1-v1,  -w1, 1-u1); rotate2(); break;
	case C(1, 1): set(1-u, 1-v, 1-u2, 1-v2,  -w2, 1-u1, 1-v1,  -w1); break;
	case C(1, 2): set( -w, 1-u,  -w2, 1-u2, 1-v2,  -w1, 1-u1, 1-v1); rotate1(); break;

	case C(2, 0): set(1-w,  -u, 1-w2,  -u2, 1-v2, 1-w1,  -u1, 1-v1); rotate1(); break;
	case C(2, 1): set(1-v, 1-w, 1-v2, 1-w2,  -u2, 1-v1, 1-w1,  -u1); rotate2(); break;
	case C(2, 2): set( -u, 1-v,  -u2, 1-v2, 1-w2,  -u1, 1-v1, 1-w1); break;
#undef C
	}
    }
    
    void clampRes(Res fres)
    {
	res.ulog2 = PtexUtils::min(res.ulog2, fres.ulog2);
	res.vlog2 = res.ulog2;
    }

    void clampExtent()
    {
	u1 = PtexUtils::max(u1, 0.0);
	v1 = PtexUtils::max(v1, 0.0);
	w1 = PtexUtils::max(w1, 0.0);
	u2 = PtexUtils::min(u2, 1-(v1+w1));
	v2 = PtexUtils::min(v2, 1-(w1+u1));
	w2 = PtexUtils::min(w2, 1-(u1+v1));
    }

    void getIterators(PtexTriangleKernelIter& ke, PtexTriangleKernelIter& ko)
    {
	int resu = res.u();

	// normalize coefficients for texel units
	double Finv = 1.0/(resu*resu*(A*C - 0.25 * B * B));
	double Ak = A*Finv, Bk = B*Finv, Ck = C*Finv;

	// build even iterator
	ke.rowlen = resu;
	ke.wscale = 1.0/(resu*resu);
	double scale = ke.rowlen;
	ke.u = u * scale - 1/3.0;
	ke.v = v * scale - 1/3.0;
	ke.u1 = int(ceil(u1 * scale - 1/3.0));
	ke.v1 = int(ceil(v1 * scale - 1/3.0));
	ke.w1 = int(ceil(w1 * scale - 1/3.0));
	ke.u2 = int(ceil(u2 * scale - 1/3.0));
	ke.v2 = int(ceil(v2 * scale - 1/3.0));
	ke.w2 = int(ceil(w2 * scale - 1/3.0));
	ke.A = Ak; ke.B = Bk; ke.C = Ck;
	ke.valid = (ke.u2 > ke.u1 && ke.v2 > ke.v1 && ke.w2 > ke.w1);
	ke.weight = 0;

	// build odd iterator: flip kernel across diagonal (u = 1-v, v = 1-u, w = -w)
	ko.rowlen = ke.rowlen;
	ko.wscale = ke.wscale;
	ko.u = (1-v) * scale - 1/3.0;
	ko.v = (1-u) * scale - 1/3.0;
	ko.u1 = int(ceil((1-v2) * scale - 1/3.0));
	ko.v1 = int(ceil((1-u2) * scale - 1/3.0));
	ko.w1 = int(ceil(( -w2) * scale - 1/3.0));
	ko.u2 = int(ceil((1-v1) * scale - 1/3.0));
	ko.v2 = int(ceil((1-u1) * scale - 1/3.0));
	ko.w2 = int(ceil(( -w1) * scale - 1/3.0));
	ko.A = Ck; ko.B = Bk; ko.C = Ak;
	ko.valid = (ko.u2 > ko.u1 && ko.v2 > ko.v1 && ko.w2 > ko.w1);
	ko.weight = 0;
    }
};

#endif
