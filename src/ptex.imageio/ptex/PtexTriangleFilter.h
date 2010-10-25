#ifndef PtexTriangleFilter_h
#define PtexTriangleFilter_h

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

#include "Ptexture.h"
class PtexTriangleKernel;
class PtexTriangleKernelIter;

class PtexTriangleFilter : public PtexFilter, public Ptex
{
 public:
    PtexTriangleFilter(PtexTexture* tx, const PtexFilter::Options& opts ) :
	_tx(tx), _options(opts), _result(0), _weight(0), 
	_firstChanOffset(0), _nchan(0), _ntxchan(0),
	_dt((DataType)0) {}
    virtual void release() { delete this; }
    virtual void eval(float* result, int firstchan, int nchannels,
		      int faceid, float u, float v,
		      float uw1, float vw1, float uw2, float vw2, 
		      float width, float blur);

 protected:
    void buildKernel(PtexTriangleKernel& k, float u, float v, 
		     float uw1, float vw1, float uw2, float vw2,
		     float width, float blur, Res faceRes);

    void splitAndApply(PtexTriangleKernel& k, int faceid, const Ptex::FaceInfo& f);
    void applyAcrossEdge(PtexTriangleKernel& k, const Ptex::FaceInfo& f, int eid);
    void apply(PtexTriangleKernel& k, int faceid, const Ptex::FaceInfo& f);
    void applyIter(PtexTriangleKernelIter& k, PtexFaceData* dh);
    
    virtual ~PtexTriangleFilter() {}
    
    PtexTexture* _tx;		// texture being evaluated
    Options _options;		// options
    double* _result;		// temp result
    double _weight;		// accumulated weight of data in _result
    int _firstChanOffset;	// byte offset of first channel to eval
    int _nchan;			// number of channels to eval
    int _ntxchan;		// number of channels in texture
    DataType _dt;		// data type of texture
};

#endif
