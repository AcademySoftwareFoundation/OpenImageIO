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
#include "PtexSeparableKernel.h"

namespace {
    // apply to 1..4 channels (unrolled channel loop) of packed data (nTxChan==nChan)
    template<class T, int nChan>
    void Apply(PtexSeparableKernel& k, double* result, void* data, int /*nChan*/, int /*nTxChan*/)
    {
	double* rowResult = (double*) alloca(nChan*sizeof(double));
	int rowlen = k.res.u() * nChan;
	int datalen = k.uw * nChan;
	int rowskip = rowlen - datalen;
	double* kvp = k.kv;
	T* p = (T*)data + (k.v * k.res.u() + k.u) * nChan;
	T* pEnd = p + k.vw * rowlen;
	while (p != pEnd)
	{
	    double* kup = k.ku;
	    T* pRowEnd = p + datalen;
	    // just mult and copy first element
	    PtexUtils::VecMult<T,nChan>()(rowResult, p, *kup++);
	    p += nChan;
	    // accumulate remaining elements
	    while (p != pRowEnd) {
		// rowResult[i] = p[i] * ku[u] for i in {0..n-1}
		PtexUtils::VecAccum<T,nChan>()(rowResult, p, *kup++);
		p += nChan;
	    }
	    // result[i] += rowResult[i] * kv[v] for i in {0..n-1}
	    PtexUtils::VecAccum<double,nChan>()(result, rowResult, *kvp++);
	    p += rowskip;
	}
    }

    // apply to 1..4 channels (unrolled channel loop) w/ pixel stride
    template<class T, int nChan>
    void ApplyS(PtexSeparableKernel& k, double* result, void* data, int /*nChan*/, int nTxChan)
    {
	double* rowResult = (double*) alloca(nChan*sizeof(double));
	int rowlen = k.res.u() * nTxChan;
	int datalen = k.uw * nTxChan;
	int rowskip = rowlen - datalen;
	double* kvp = k.kv;
	T* p = (T*)data + (k.v * k.res.u() + k.u) * nTxChan;
	T* pEnd = p + k.vw * rowlen;
	while (p != pEnd)
	{
	    double* kup = k.ku;
	    T* pRowEnd = p + datalen;
	    // just mult and copy first element
	    PtexUtils::VecMult<T,nChan>()(rowResult, p, *kup++);
	    p += nTxChan;
	    // accumulate remaining elements
	    while (p != pRowEnd) {
		// rowResult[i] = p[i] * ku[u] for i in {0..n-1}
		PtexUtils::VecAccum<T,nChan>()(rowResult, p, *kup++);
		p += nTxChan;
	    }
	    // result[i] += rowResult[i] * kv[v] for i in {0..n-1}
	    PtexUtils::VecAccum<double,nChan>()(result, rowResult, *kvp++);
	    p += rowskip;
	}
    }

    // apply to N channels (general case)
    template<class T>
    void ApplyN(PtexSeparableKernel& k, double* result, void* data, int nChan, int nTxChan)
    {
	double* rowResult = (double*) alloca(nChan*sizeof(double));
	int rowlen = k.res.u() * nTxChan;
	int datalen = k.uw * nTxChan;
	int rowskip = rowlen - datalen;
	double* kvp = k.kv;
	T* p = (T*)data + (k.v * k.res.u() + k.u) * nTxChan;
	T* pEnd = p + k.vw * rowlen;
	while (p != pEnd)
	{
	    double* kup = k.ku;
	    T* pRowEnd = p + datalen;
	    // just mult and copy first element
	    PtexUtils::VecMultN<T>()(rowResult, p, nChan, *kup++);
	    p += nTxChan;
	    // accumulate remaining elements
	    while (p != pRowEnd) {
		// rowResult[i] = p[i] * ku[u] for i in {0..n-1}
		PtexUtils::VecAccumN<T>()(rowResult, p, nChan, *kup++);
		p += nTxChan;
	    }
	    // result[i] += rowResult[i] * kv[v] for i in {0..n-1}
	    PtexUtils::VecAccumN<double>()(result, rowResult, nChan, *kvp++);
	    p += rowskip;
	}
    }
}



PtexSeparableKernel::ApplyFn
PtexSeparableKernel::applyFunctions[] = {
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
