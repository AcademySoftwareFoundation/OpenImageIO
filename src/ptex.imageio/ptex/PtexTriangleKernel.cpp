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
#include "PtexUtils.h"
#include "PtexHalf.h"
#include "PtexTriangleKernel.h"


namespace {
    inline double gaussian(double x_squared)
    {
	static const double scale = -0.5 * (PtexTriangleKernelWidth * PtexTriangleKernelWidth);
	return exp(scale * x_squared);
    }
}


namespace {

    // apply to 1..4 channels (unrolled channel loop) of packed data (nTxChan==nChan)
    // the ellipse equation, Q, is calculated via finite differences (Heckbert '89 pg 57)
    template<class T, int nChan>
    void Apply(PtexTriangleKernelIter& k, double* result, void* data, int /*nChan*/, int /*nTxChan*/)
    {
	int nTxChan = nChan;
	double DDQ = 2*k.A;
	for (int vi = k.v1; vi != k.v2; vi++) {
	    int xw = k.rowlen - vi;
	    int x1 = PtexUtils::max(k.u1, xw-k.w2);
	    int x2 = PtexUtils::min(k.u2, xw-k.w1);
	    double U = x1 - k.u;
	    double V = vi - k.v;
	    double DQ = k.A*(2*U+1)+k.B*V;
	    double Q = k.A*U*U + (k.B*U + k.C*V)*V;
	    T* p = (T*)data + (vi * k.rowlen + x1) * nTxChan;
	    T* pEnd = p + (x2-x1)*nTxChan;
	    for (; p < pEnd; p += nTxChan) {
		if (Q < 1) {
		    double weight = gaussian(Q)*k.wscale;
		    k.weight += weight;
		    PtexUtils::VecAccum<T,nChan>()(result, p, weight);
		}
		Q += DQ;
		DQ += DDQ;
	    }
	}
    }

    // apply to 1..4 channels (unrolled channel loop) w/ pixel stride
    template<class T, int nChan>
    void ApplyS(PtexTriangleKernelIter& k, double* result, void* data, int /*nChan*/, int nTxChan)
    {
	double DDQ = 2*k.A;
	for (int vi = k.v1; vi != k.v2; vi++) {
	    int xw = k.rowlen - vi;
	    int x1 = PtexUtils::max(k.u1, xw-k.w2);
	    int x2 = PtexUtils::min(k.u2, xw-k.w1);
	    double U = x1 - k.u;
	    double V = vi - k.v;
	    double DQ = k.A*(2*U+1)+k.B*V;
	    double Q = k.A*U*U + (k.B*U + k.C*V)*V;
	    T* p = (T*)data + (vi * k.rowlen + x1) * nTxChan;
	    T* pEnd = p + (x2-x1)*nTxChan;
	    for (; p < pEnd; p += nTxChan) {
		if (Q < 1) {
		    double weight = gaussian(Q)*k.wscale;
		    k.weight += weight;
		    PtexUtils::VecAccum<T,nChan>()(result, p, weight);
		}
		Q += DQ;
		DQ += DDQ;
	    }
	}
    }

    // apply to N channels (general case)
    template<class T>
    void ApplyN(PtexTriangleKernelIter& k, double* result, void* data, int nChan, int nTxChan)
    {
	double DDQ = 2*k.A;
	for (int vi = k.v1; vi != k.v2; vi++) {
	    int xw = k.rowlen - vi;
	    int x1 = PtexUtils::max(k.u1, xw-k.w2);
	    int x2 = PtexUtils::min(k.u2, xw-k.w1);
	    double U = x1 - k.u;
	    double V = vi - k.v;
	    double DQ = k.A*(2*U+1)+k.B*V;
	    double Q = k.A*U*U + (k.B*U + k.C*V)*V;
	    T* p = (T*)data + (vi * k.rowlen + x1) * nTxChan;
	    T* pEnd = p + (x2-x1)*nTxChan;
	    for (; p < pEnd; p += nTxChan) {
		if (Q < 1) {
		    double weight = gaussian(Q)*k.wscale;
		    k.weight += weight;
		    PtexUtils::VecAccumN<T>()(result, p, nChan, weight);
		}
		Q += DQ;
		DQ += DDQ;
	    }
	}
    }
}


PtexTriangleKernelIter::ApplyFn
PtexTriangleKernelIter::applyFunctions[] = {
    // nChan == nTxChan
    ApplyN<uint8_t>,  ApplyN<uint16_t>,  ApplyN<PtexHalf>,  ApplyN<float>,
    Apply<uint8_t,1>, Apply<uint16_t,1>, Apply<PtexHalf,1>, Apply<float,1>,
    Apply<uint8_t,2>, Apply<uint16_t,2>, Apply<PtexHalf,2>, Apply<float,2>,
    Apply<uint8_t,3>, Apply<uint16_t,3>, Apply<PtexHalf,3>, Apply<float,3>,
    Apply<uint8_t,4>, Apply<uint16_t,4>, Apply<PtexHalf,4>, Apply<float,4>,

    // nChan != nTxChan (need pixel stride)
    ApplyN<uint8_t>,   ApplyN<uint16_t>,   ApplyN<PtexHalf>,   ApplyN<float>,
    ApplyS<uint8_t,1>, ApplyS<uint16_t,1>, ApplyS<PtexHalf,1>, ApplyS<float,1>,
    ApplyS<uint8_t,2>, ApplyS<uint16_t,2>, ApplyS<PtexHalf,2>, ApplyS<float,2>,
    ApplyS<uint8_t,3>, ApplyS<uint16_t,3>, ApplyS<PtexHalf,3>, ApplyS<float,3>,
    ApplyS<uint8_t,4>, ApplyS<uint16_t,4>, ApplyS<PtexHalf,4>, ApplyS<float,4>,
};


void PtexTriangleKernelIter::applyConst(double* dst, void* data, DataType dt, int nChan)
{
    // iterate over texel locations and calculate weight as if texture weren't const
    double DDQ = 2*A;
    for (int vi = v1; vi != v2; vi++) {
	int xw = rowlen - vi;
	int x1 = PtexUtils::max(u1, xw-w2);
	int x2 = PtexUtils::min(u2, xw-w1);
	double U = x1 - u;
	double V = vi - v;
	double DQ = A*(2*U+1)+B*V;
	double Q = A*U*U + (B*U + C*V)*V;
	for (int x = x1; x < x2; x++) {
	    if (Q < 1) {
		weight += gaussian(Q)*wscale;
	    }
	    Q += DQ;
	    DQ += DDQ;
	}
    }

    // apply weight to single texel value
    PtexUtils::applyConst(weight, dst, data, dt, nChan);
}
