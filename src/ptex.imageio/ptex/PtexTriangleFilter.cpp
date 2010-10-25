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
#include <math.h>
#include <assert.h>

#include "PtexTriangleFilter.h"
#include "PtexTriangleKernel.h"
#include "PtexUtils.h"

namespace {
    inline double squared(double x) { return x*x; }
}

void PtexTriangleFilter::eval(float* result, int firstChan, int nChannels,
			      int faceid, float u, float v,
			      float uw1, float vw1, float uw2, float vw2,
			      float width, float blur)
{
    // init
    if (!_tx || nChannels <= 0) return;
    if (faceid < 0 || faceid >= _tx->numFaces()) return;
    _ntxchan = _tx->numChannels();
    _dt = _tx->dataType();
    _firstChanOffset = firstChan*DataSize(_dt);
    _nchan = PtexUtils::min(nChannels, _ntxchan-firstChan);

    // get face info
    const FaceInfo& f = _tx->getFaceInfo(faceid);

    // if neighborhood is constant, just return constant value of face
    if (f.isNeighborhoodConstant()) {
	PtexPtr<PtexFaceData> data ( _tx->getData(faceid, 0) );
	if (data) {
	    char* d = (char*) data->getData() + _firstChanOffset;
	    Ptex::ConvertToFloat(result, d, _dt, _nchan);
	}
	return;
    }
    
    // clamp u and v
    u = PtexUtils::clamp(u, 0.0f, 1.0f);
    v = PtexUtils::clamp(v, 0.0f, 1.0f);

    // build kernel
    PtexTriangleKernel k;
    buildKernel(k, u, v, uw1, vw1, uw2, vw2, width, blur, f.res);

    // accumulate the weight as we apply
    _weight = 0;

    // allocate temporary double-precision result
    _result = (double*) alloca(sizeof(double)*_nchan);
    memset(_result, 0, sizeof(double)*_nchan);

    // apply to faces
    splitAndApply(k, faceid, f);

    // normalize (both for data type and cumulative kernel weight applied)
    // and output result
    double scale = 1.0 / (_weight * OneValue(_dt));
    for (int i = 0; i < _nchan; i++) result[i] = float(_result[i] * scale);

    // clear temp result
    _result = 0;
}



void PtexTriangleFilter::buildKernel(PtexTriangleKernel& k, float u, float v, 
				     float uw1, float vw1, float uw2, float vw2,
				     float width, float blur, Res faceRes)
{
    const double sqrt3 = 1.7320508075688772;

    // compute ellipse coefficients, A*u^2 + B*u*v + C*v^2 == AC - B^2/4
    double scaleAC = 0.25 * width*width;
    double scaleB = -2 * scaleAC;
    double A = (vw1*vw1 + vw2*vw2) * scaleAC;
    double B = (uw1*vw1 + uw2*vw2) * scaleB;
    double C = (uw1*uw1 + uw2*uw2) * scaleAC;

    // convert to cartesian domain
    double Ac = 0.75 * A;
    double Bc = (sqrt3/2) * (B-A);
    double Cc = 0.25 * A - 0.5 * B + C;

    // compute min blur for eccentricity clamping
    const double maxEcc = 15; // max eccentricity
    const double eccRatio = (maxEcc*maxEcc + 1) / (maxEcc*maxEcc - 1);
    double X = sqrt(squared(Ac - Cc) + squared(Bc));
    double b_e = 0.5 * (eccRatio * X - (Ac + Cc));

    // compute min blur for texel clamping
    // (ensure that ellipse is no smaller than a texel)
    double b_t = squared(0.5 / faceRes.u());

    // add blur
    double b_b = 0.25 * blur * blur;
    double b = PtexUtils::max(b_b, PtexUtils::max(b_e, b_t));
    Ac += b;
    Cc += b;

    // compute minor radius
    double m = sqrt(2*(Ac*Cc - 0.25*Bc*Bc) / (Ac + Cc + X));
    
    // choose desired resolution
    int reslog2 = PtexUtils::max(0, int(ceil(log2(0.5/m))));

    // convert back to triangular domain
    A = (4/3.0) * Ac;
    B = (2/sqrt3) * Bc + A;
    C = -0.25 * A + 0.5 * B + Cc;

    // scale by kernel width
    double scale = PtexTriangleKernelWidth * PtexTriangleKernelWidth;
    A *= scale;
    B *= scale;
    C *= scale;

    // find u,v,w extents
    double uw = PtexUtils::min(sqrt(C), 1.0);
    double vw = PtexUtils::min(sqrt(A), 1.0);
    double ww = PtexUtils::min(sqrt(A-B+C), 1.0);

    // init kernel
    double w = 1 - u - v;
    k.set(Res(reslog2, reslog2), u, v, u-uw, v-vw, w-ww, u+uw, v+vw, w+ww, A, B, C);
}


void PtexTriangleFilter::splitAndApply(PtexTriangleKernel& k, int faceid, const Ptex::FaceInfo& f)
{
    // do we need to split? if so, split kernel and apply across edge(s)
    if (k.u1 < 0 && f.adjface(2) >= 0) {
	PtexTriangleKernel ka;
	k.splitU(ka);
	applyAcrossEdge(ka, f, 2);
    }
    if (k.v1 < 0 && f.adjface(0) >= 0) {
	PtexTriangleKernel ka;
	k.splitV(ka);
	applyAcrossEdge(ka, f, 0);
    }
    if (k.w1 < 0 && f.adjface(1) >= 0) {
	PtexTriangleKernel ka;
	k.splitW(ka);
	applyAcrossEdge(ka, f, 1);
    }
    // apply to local face
    apply(k, faceid, f); 
}


void PtexTriangleFilter::applyAcrossEdge(PtexTriangleKernel& k, 
					 const Ptex::FaceInfo& f, int eid)
{
    int afid = f.adjface(eid), aeid = f.adjedge(eid);
    const Ptex::FaceInfo& af = _tx->getFaceInfo(afid);
    k.reorient(eid, aeid);
    splitAndApply(k, afid, af);
}


void PtexTriangleFilter::apply(PtexTriangleKernel& k, int faceid, const Ptex::FaceInfo& f)
{
    // clamp kernel face (resolution and extent)
    k.clampRes(f.res);
    k.clampExtent();

    // build kernel iterators
    PtexTriangleKernelIter keven, kodd;
    k.getIterators(keven, kodd);
    if (!keven.valid && !kodd.valid) return;

    // get face data, and apply
    PtexPtr<PtexFaceData> dh ( _tx->getData(faceid, k.res) );
    if (!dh) return;

    if (keven.valid) applyIter(keven, dh);
    if (kodd.valid) applyIter(kodd, dh);
}


void PtexTriangleFilter::applyIter(PtexTriangleKernelIter& k, PtexFaceData* dh)
{
    if (dh->isConstant()) {
	k.applyConst(_result, (char*)dh->getData()+_firstChanOffset, _dt, _nchan);
	_weight += k.weight;
    }
    else if (dh->isTiled()) {
	Ptex::Res tileres = dh->tileRes();
	PtexTriangleKernelIter kt = k;
	int tileresu = tileres.u();
	int tileresv = tileres.v();
	kt.rowlen = tileresu;
	int ntilesu = k.rowlen / kt.rowlen;
	int wOffsetBase = k.rowlen - tileresu;
	for (int tilev = k.v1 / tileresv, tilevEnd = (k.v2-1) / tileresv; tilev <= tilevEnd; tilev++) {
	    int vOffset = tilev * tileresv;
	    kt.v = k.v - vOffset;
	    kt.v1 = PtexUtils::max(0, k.v1 - vOffset);
	    kt.v2 = PtexUtils::min(k.v2 - vOffset, tileresv);
	    for (int tileu = k.u1 / tileresu, tileuEnd = (k.u2-1) / tileresu; tileu <= tileuEnd; tileu++) {
		int uOffset = tileu * tileresu;
		int wOffset = wOffsetBase - uOffset - vOffset;
		kt.u = k.u - uOffset;
		kt.u1 = PtexUtils::max(0, k.u1 - uOffset);
		kt.u2 = PtexUtils::min(k.u2 - uOffset, tileresu);
		kt.w1 = k.w1 - wOffset;
		kt.w2 = k.w2 - wOffset;
		PtexPtr<PtexFaceData> th ( dh->getTile(tilev * ntilesu + tileu) );
		if (th) {
		    kt.weight = 0;
		    if (th->isConstant())
			kt.applyConst(_result, (char*)th->getData()+_firstChanOffset, _dt, _nchan);
		    else
			kt.apply(_result, (char*)th->getData()+_firstChanOffset, _dt, _nchan, _ntxchan);
		    _weight += kt.weight;
		}
	    }
	}
    }
    else {
	k.apply(_result, (char*)dh->getData()+_firstChanOffset, _dt, _nchan, _ntxchan);
	_weight += k.weight;
    }
}
