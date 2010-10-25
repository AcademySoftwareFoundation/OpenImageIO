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
#include <algorithm>
#include <vector>
#include <stdlib.h>
#include <string.h>

#include "PtexHalf.h"
#include "PtexUtils.h"

const char* Ptex::MeshTypeName(MeshType mt)
{
    static const char* names[] = { "triangle", "quad" };
    if (mt < 0 || mt >= int(sizeof(names)/sizeof(const char*)))
	return "(invalid mesh type)";
    return names[mt];
}


const char* Ptex::DataTypeName(DataType dt)
{
    static const char* names[] = { "uint8", "uint16", "float16", "float32" };
    if (dt < 0 || dt >= int(sizeof(names)/sizeof(const char*)))
	return "(invalid data type)";
    return names[dt];
}


const char* Ptex::BorderModeName(BorderMode m)
{
    static const char* names[] = { "clamp", "black", "periodic" };
    if (m < 0 || m >= int(sizeof(names)/sizeof(const char*)))
	return "(invalid border mode)";
    return names[m];
}


const char* Ptex::EdgeIdName(EdgeId eid)
{
    static const char* names[] = { "bottom", "right", "top", "left" };
    if (eid < 0 || eid >= int(sizeof(names)/sizeof(const char*)))
	return "(invalid edge id)";
    return names[eid];
}


const char* Ptex::MetaDataTypeName(MetaDataType mdt)
{
    static const char* names[] = { "string", "int8", "int16", "int32", "float", "double" };
    if (mdt < 0 || mdt >= int(sizeof(names)/sizeof(const char*)))
	return "(invalid meta data type)";
    return names[mdt];
}



namespace {
    template<typename DST, typename SRC>
    void ConvertArrayClamped(DST* dst, SRC* src, int numChannels, double scale, double round=0)
    {
        for (int i = 0; i < numChannels; i++)
	    dst[i] = DST(PtexUtils::clamp(src[i], 0.0f, 1.0f) * scale + round);
    }

    template<typename DST, typename SRC>
    void ConvertArray(DST* dst, SRC* src, int numChannels, double scale, double round=0)
    {
	for (int i = 0; i < numChannels; i++) dst[i] = DST(src[i] * scale + round);
    }
}

void Ptex::ConvertToFloat(float* dst, const void* src, Ptex::DataType dt, int numChannels)
{
    switch (dt) {
    case dt_uint8:  ConvertArray(dst, (uint8_t*)src,  numChannels, 1/255.0); break;
    case dt_uint16: ConvertArray(dst, (uint16_t*)src, numChannels, 1/65535.0); break;
    case dt_half:   ConvertArray(dst, (PtexHalf*)src, numChannels, 1.0); break;
    case dt_float:  memcpy(dst, src, sizeof(float)*numChannels); break;
    }
}


void Ptex::ConvertFromFloat(void* dst, const float* src, Ptex::DataType dt, int numChannels)
{
    switch (dt) {
    case dt_uint8:  ConvertArrayClamped((uint8_t*)dst,  src, numChannels, 255.0, 0.5); break;
    case dt_uint16: ConvertArrayClamped((uint16_t*)dst, src, numChannels, 65535.0, 0.5); break;
    case dt_half:   ConvertArray((PtexHalf*)dst, src, numChannels, 1.0); break;
    case dt_float:  memcpy(dst, src, sizeof(float)*numChannels); break;
    }
}


bool PtexUtils::isConstant(const void* data, int stride, int ures, int vres,
			   int pixelSize)
{
    int rowlen = pixelSize * ures;
    const char* p = (const char*) data + stride;

    // compare each row with the first
    for (int i = 1; i < vres; i++, p += stride)
	if (0 != memcmp(data, p, rowlen)) return 0;

    // make sure first row is constant
    p = (const char*) data + pixelSize;
    for (int i = 1; i < ures; i++, p += pixelSize)
	if (0 != memcmp(data, p, pixelSize)) return 0;

    return 1;
}


namespace {
    template<typename T>
    inline void interleave(const T* src, int sstride, int uw, int vw, 
			   T* dst, int dstride, int nchan)
    {
	sstride /= sizeof(T);
	dstride /= sizeof(T);
	// for each channel
	for (T* dstend = dst + nchan; dst != dstend; dst++) {
	    // for each row
	    T* drow = dst;
	    for (const T* rowend = src + sstride*vw; src != rowend;
		 src += sstride, drow += dstride) {
		// copy each pixel across the row
		T* dp = drow;
		for (const T* sp = src, * end = sp + uw; sp != end; dp += nchan)
		    *dp = *sp++;
	    }
	}
    }
}


void PtexUtils::interleave(const void* src, int sstride, int uw, int vw, 
			   void* dst, int dstride, DataType dt, int nchan)
{
    switch (dt) {
    case dt_uint8:   ::interleave((const uint8_t*) src, sstride, uw, vw, 
				  (uint8_t*) dst, dstride, nchan); break;
    case dt_half:
    case dt_uint16:  ::interleave((const uint16_t*) src, sstride, uw, vw, 
				  (uint16_t*) dst, dstride, nchan); break;
    case dt_float:   ::interleave((const float*) src, sstride, uw, vw, 
				  (float*) dst, dstride, nchan); break;
    }
}

namespace {
    template<typename T>
    inline void deinterleave(const T* src, int sstride, int uw, int vw, 
			     T* dst, int dstride, int nchan)
    {
	sstride /= sizeof(T);
	dstride /= sizeof(T);
	// for each channel
	for (const T* srcend = src + nchan; src != srcend; src++) {
	    // for each row
	    const T* srow = src;
	    for (const T* rowend = srow + sstride*vw; srow != rowend;
		 srow += sstride, dst += dstride) {
		// copy each pixel across the row
		const T* sp = srow;
		for (T* dp = dst, * end = dp + uw; dp != end; sp += nchan)
		    *dp++ = *sp;
	    }
	}
    }
}


void PtexUtils::deinterleave(const void* src, int sstride, int uw, int vw, 
			     void* dst, int dstride, DataType dt, int nchan)
{
    switch (dt) {
    case dt_uint8:   ::deinterleave((const uint8_t*) src, sstride, uw, vw, 
				    (uint8_t*) dst, dstride, nchan); break;
    case dt_half:
    case dt_uint16:  ::deinterleave((const uint16_t*) src, sstride, uw, vw, 
				    (uint16_t*) dst, dstride, nchan); break;
    case dt_float:   ::deinterleave((const float*) src, sstride, uw, vw, 
				    (float*) dst, dstride, nchan); break;
    }
}


namespace {
    template<typename T>
    void encodeDifference(T* data, int size)
    {
	size /= sizeof(T);
	T* p = (T*) data, * end = p + size, tmp, prev = 0;
	while (p != end) { tmp = prev; prev = *p; *p++ -= tmp; }
    }
}

void PtexUtils::encodeDifference(void* data, int size, DataType dt)
{
    switch (dt) {
    case dt_uint8:  ::encodeDifference((uint8_t*) data, size); break;
    case dt_uint16: ::encodeDifference((uint16_t*) data, size); break;
    default: break; // skip other types
    }
}


namespace {
    template<typename T>
    void decodeDifference(T* data, int size)
    {
	size /= sizeof(T);
	T* p = (T*) data, * end = p + size, prev = 0;
	while (p != end) { *p += prev; prev = *p++; }
    }
}

void PtexUtils::decodeDifference(void* data, int size, DataType dt)
{
    switch (dt) {
    case dt_uint8:  ::decodeDifference((uint8_t*) data, size); break;
    case dt_uint16: ::decodeDifference((uint16_t*) data, size); break;
    default: break; // skip other types
    }
}


namespace {
    template<typename T>
    inline void reduce(const T* src, int sstride, int uw, int vw, 
		       T* dst, int dstride, int nchan)
    {
	sstride /= sizeof(T);
	dstride /= sizeof(T);
	int rowlen = uw*nchan;
	int srowskip = 2*sstride - rowlen;
	int drowskip = dstride - rowlen/2;
	for (const T* end = src + vw*sstride; src != end; 
	     src += srowskip, dst += drowskip)
	    for (const T* rowend = src + rowlen; src != rowend; src += nchan)
		for (const T* pixend = src+nchan; src != pixend; src++)
		    *dst++ = T(0.25 * (src[0] + src[nchan] +
				       src[sstride] + src[sstride+nchan]));
    }
}

void PtexUtils::reduce(const void* src, int sstride, int uw, int vw,
		       void* dst, int dstride, DataType dt, int nchan)
{
    switch (dt) {
    case dt_uint8:   ::reduce((const uint8_t*) src, sstride, uw, vw, 
			      (uint8_t*) dst, dstride, nchan); break;
    case dt_half:    ::reduce((const PtexHalf*) src, sstride, uw, vw, 
			      (PtexHalf*) dst, dstride, nchan); break;
    case dt_uint16:  ::reduce((const uint16_t*) src, sstride, uw, vw, 
			      (uint16_t*) dst, dstride, nchan); break;
    case dt_float:   ::reduce((const float*) src, sstride, uw, vw, 
			      (float*) dst, dstride, nchan); break;
    }
}


namespace {
    template<typename T>
    inline void reduceu(const T* src, int sstride, int uw, int vw, 
			T* dst, int dstride, int nchan)
    {	
	sstride /= sizeof(T);
	dstride /= sizeof(T);
	int rowlen = uw*nchan;
	int srowskip = sstride - rowlen;
	int drowskip = dstride - rowlen/2;
	for (const T* end = src + vw*sstride; src != end; 
	     src += srowskip, dst += drowskip)
	    for (const T* rowend = src + rowlen; src != rowend; src += nchan)
		for (const T* pixend = src+nchan; src != pixend; src++)
		    *dst++ = T(0.5 * (src[0] + src[nchan]));
    }
}

void PtexUtils::reduceu(const void* src, int sstride, int uw, int vw,
			void* dst, int dstride, DataType dt, int nchan)
{
    switch (dt) {
    case dt_uint8:   ::reduceu((const uint8_t*) src, sstride, uw, vw, 
			       (uint8_t*) dst, dstride, nchan); break;
    case dt_half:    ::reduceu((const PtexHalf*) src, sstride, uw, vw, 
			       (PtexHalf*) dst, dstride, nchan); break;
    case dt_uint16:  ::reduceu((const uint16_t*) src, sstride, uw, vw, 
			       (uint16_t*) dst, dstride, nchan); break;
    case dt_float:   ::reduceu((const float*) src, sstride, uw, vw, 
			       (float*) dst, dstride, nchan); break;
    }
}


namespace {
    template<typename T>
    inline void reducev(const T* src, int sstride, int uw, int vw, 
			T* dst, int dstride, int nchan)
    {
	sstride /= sizeof(T);
	dstride /= sizeof(T);
	int rowlen = uw*nchan;
	int srowskip = 2*sstride - rowlen;
	int drowskip = dstride - rowlen;
	for (const T* end = src + vw*sstride; src != end; 
	     src += srowskip, dst += drowskip)
	    for (const T* rowend = src + rowlen; src != rowend; src++)
		*dst++ = T(0.5 * (src[0] + src[sstride]));
    }
}

void PtexUtils::reducev(const void* src, int sstride, int uw, int vw,
			void* dst, int dstride, DataType dt, int nchan)
{
    switch (dt) {
    case dt_uint8:   ::reducev((const uint8_t*) src, sstride, uw, vw, 
			       (uint8_t*) dst, dstride, nchan); break;
    case dt_half:    ::reducev((const PtexHalf*) src, sstride, uw, vw, 
			       (PtexHalf*) dst, dstride, nchan); break;
    case dt_uint16:  ::reducev((const uint16_t*) src, sstride, uw, vw, 
			       (uint16_t*) dst, dstride, nchan); break;
    case dt_float:   ::reducev((const float*) src, sstride, uw, vw, 
			       (float*) dst, dstride, nchan); break;
    }
}



namespace {
    // generate a reduction of a packed-triangle texture
    // note: this method won't work for tiled textures
    template<typename T>
    inline void reduceTri(const T* src, int sstride, int w, int /*vw*/, 
			  T* dst, int dstride, int nchan)
    {
	sstride /= sizeof(T);
	dstride /= sizeof(T);
	int rowlen = w*nchan;
	const T* src2 = src + (w-1) * sstride + rowlen - nchan;
	int srowinc2 = -2*sstride - nchan;
	int srowskip = 2*sstride - rowlen;
	int srowskip2 = w*sstride - 2 * nchan;
	int drowskip = dstride - rowlen/2;
	for (const T* end = src + w*sstride; src != end; 
	     src += srowskip, src2 += srowskip2, dst += drowskip)
	    for (const T* rowend = src + rowlen; src != rowend; src += nchan, src2 += srowinc2)
		for (const T* pixend = src+nchan; src != pixend; src++, src2++)
		    *dst++ = T(0.25 * (src[0] + src[nchan] + src[sstride] + src2[0]));
    }
}

void PtexUtils::reduceTri(const void* src, int sstride, int w, int /*vw*/,
			  void* dst, int dstride, DataType dt, int nchan)
{
    switch (dt) {
    case dt_uint8:   ::reduceTri((const uint8_t*) src, sstride, w, 0, 
				 (uint8_t*) dst, dstride, nchan); break;
    case dt_half:    ::reduceTri((const PtexHalf*) src, sstride, w, 0, 
				 (PtexHalf*) dst, dstride, nchan); break;
    case dt_uint16:  ::reduceTri((const uint16_t*) src, sstride, w, 0, 
				 (uint16_t*) dst, dstride, nchan); break;
    case dt_float:   ::reduceTri((const float*) src, sstride, w, 0, 
				 (float*) dst, dstride, nchan); break;
    }
}


void PtexUtils::fill(const void* src, void* dst, int dstride,
		     int ures, int vres, int pixelsize)
{
    // fill first row
    int rowlen = ures*pixelsize;
    char* ptr = (char*) dst;
    char* end = ptr + rowlen;
    for (; ptr != end; ptr += pixelsize) memcpy(ptr, src, pixelsize);

    // fill remaining rows from first row
    ptr = (char*) dst + dstride;
    end = (char*) dst + vres*dstride;
    for (; ptr != end; ptr += dstride) memcpy(ptr, dst, rowlen);
}


void PtexUtils::copy(const void* src, int sstride, void* dst, int dstride,
		     int vres, int rowlen)
{
    // regular non-tiled case
    if (sstride == rowlen && dstride == rowlen) {
	// packed case - copy in single block
	memcpy(dst, src, vres*rowlen);
    } else {
	// copy a row at a time
	char* sptr = (char*) src;
	char* dptr = (char*) dst;
	for (char* end = sptr + vres*sstride; sptr != end;) {
	    memcpy(dptr, sptr, rowlen);
	    dptr += dstride;
	    sptr += sstride;
	}
    }
}


namespace {
    template<typename T>
    inline void blend(const T* src, float weight, T* dst, int rowlen, int nchan)
    {
	for (const T* end = src + rowlen * nchan; src != end; dst++)
	    *dst = *dst + T(weight * *src++);
    }

    template<typename T>
    inline void blendflip(const T* src, float weight, T* dst, int rowlen, int nchan)
    {
	dst += (rowlen-1) * nchan;
	for (const T* end = src + rowlen * nchan; src != end;) {
	    for (int i = 0; i < nchan; i++, dst++)
		*dst = *dst + T(weight * *src++);
	    dst -= nchan*2;
	}
    }
}


void PtexUtils::blend(const void* src, float weight, void* dst, bool flip,
		      int rowlen, DataType dt, int nchan)
{
    switch ((dt<<1) | int(flip)) {
    case (dt_uint8<<1):      ::blend((const uint8_t*) src, weight,
				     (uint8_t*) dst, rowlen, nchan); break;
    case (dt_uint8<<1 | 1):  ::blendflip((const uint8_t*) src, weight,
					 (uint8_t*) dst, rowlen, nchan); break;
    case (dt_half<<1):       ::blend((const PtexHalf*) src, weight,
				     (PtexHalf*) dst, rowlen, nchan); break;
    case (dt_half<<1 | 1):   ::blendflip((const PtexHalf*) src, weight,
					 (PtexHalf*) dst, rowlen, nchan); break;
    case (dt_uint16<<1):     ::blend((const uint16_t*) src, weight,
				     (uint16_t*) dst, rowlen, nchan); break;
    case (dt_uint16<<1 | 1): ::blendflip((const uint16_t*) src, weight,
					 (uint16_t*) dst, rowlen, nchan); break;
    case (dt_float<<1):      ::blend((const float*) src, weight,
				     (float*) dst, rowlen, nchan); break;
    case (dt_float<<1 | 1):  ::blendflip((const float*) src, weight,
					 (float*) dst, rowlen, nchan); break;
    }
}


namespace {
    template<typename T>
    inline void average(const T* src, int sstride, int uw, int vw, 
			T* dst, int nchan)
    {
	float* buff = (float*) alloca(nchan*sizeof(float));
	memset(buff, 0, nchan*sizeof(float));
	sstride /= sizeof(T);
	int rowlen = uw*nchan;
	int rowskip = sstride - rowlen;
	for (const T* end = src + vw*sstride; src != end; src += rowskip)
	    for (const T* rowend = src + rowlen; src != rowend;)
		for (int i = 0; i < nchan; i++) buff[i] += *src++;
	double scale = 1.0/(uw*vw);
	for (int i = 0; i < nchan; i++) dst[i] = T(buff[i]*scale);
    }
}

void PtexUtils::average(const void* src, int sstride, int uw, int vw,
			void* dst, DataType dt, int nchan)
{
    switch (dt) {
    case dt_uint8:   ::average((const uint8_t*) src, sstride, uw, vw, 
			       (uint8_t*) dst, nchan); break;
    case dt_half:    ::average((const PtexHalf*) src, sstride, uw, vw, 
			       (PtexHalf*) dst, nchan); break;
    case dt_uint16:  ::average((const uint16_t*) src, sstride, uw, vw, 
			       (uint16_t*) dst, nchan); break;
    case dt_float:   ::average((const float*) src, sstride, uw, vw, 
			       (float*) dst, nchan); break;
    }
}


namespace {
    struct CompareRfaceIds {
	const Ptex::FaceInfo* faces;
	CompareRfaceIds(const Ptex::FaceInfo* faces) : faces(faces) {}
	bool operator() (uint32_t faceid1, uint32_t faceid2)
	{
	    const Ptex::FaceInfo& f1 = faces[faceid1];
	    const Ptex::FaceInfo& f2 = faces[faceid2];
	    int min1 = f1.isConstant() ? 1 : PtexUtils::min(f1.res.ulog2, f1.res.vlog2);
	    int min2 = f2.isConstant() ? 1 : PtexUtils::min(f2.res.ulog2, f2.res.vlog2);
	    return min1 > min2;
	}
    };
}


namespace {
    template<typename T>
    inline void multalpha(T* data, int npixels, int nchannels, int alphachan, double scale)
    {
	int alphaoffset; // offset to alpha chan from data ptr
	int nchanmult;   // number of channels to alpha-multiply
	if (alphachan == 0) {
	    // first channel is alpha chan: mult the rest of the channels
	    data++;
	    alphaoffset = -1;
	    nchanmult = nchannels - 1; 
	}
	else {
	    // mult all channels up to alpha chan
	    alphaoffset = alphachan;
	    nchanmult = alphachan;
	}
	
	for (T* end = data + npixels*nchannels; data != end; data += nchannels) {
	    double aval = scale * data[alphaoffset];
	    for (int i = 0; i < nchanmult; i++)	data[i] = T(data[i] * aval);
	}
    }
}

void PtexUtils::multalpha(void* data, int npixels, DataType dt, int nchannels, int alphachan)
{
    double scale = OneValueInv(dt);
    switch(dt) {
    case dt_uint8:  ::multalpha((uint8_t*) data, npixels, nchannels, alphachan, scale); break;
    case dt_uint16: ::multalpha((uint16_t*) data, npixels, nchannels, alphachan, scale); break;
    case dt_half:   ::multalpha((PtexHalf*) data, npixels, nchannels, alphachan, scale); break;
    case dt_float:  ::multalpha((float*) data, npixels, nchannels, alphachan, scale); break;
    }
}


namespace {
    template<typename T>
    inline void divalpha(T* data, int npixels, int nchannels, int alphachan, double scale)
    {
	int alphaoffset; // offset to alpha chan from data ptr
	int nchandiv;    // number of channels to alpha-divide
	if (alphachan == 0) {
	    // first channel is alpha chan: div the rest of the channels
	    data++;
	    alphaoffset = -1;
	    nchandiv = nchannels - 1; 
	}
	else {
	    // div all channels up to alpha chan
	    alphaoffset = alphachan;
	    nchandiv = alphachan;
	}
	
	for (T* end = data + npixels*nchannels; data != end; data += nchannels) {
	    T alpha = data[alphaoffset];
	    if (!alpha) continue; // don't divide by zero!
	    double aval = scale / alpha;
	    for (int i = 0; i < nchandiv; i++)	data[i] = T(data[i] * aval);
	}
    }
}

void PtexUtils::divalpha(void* data, int npixels, DataType dt, int nchannels, int alphachan)
{
    double scale = OneValue(dt);
    switch(dt) {
    case dt_uint8:  ::divalpha((uint8_t*) data, npixels, nchannels, alphachan, scale); break;
    case dt_uint16: ::divalpha((uint16_t*) data, npixels, nchannels, alphachan, scale); break;
    case dt_half:   ::divalpha((PtexHalf*) data, npixels, nchannels, alphachan, scale); break;
    case dt_float:  ::divalpha((float*) data, npixels, nchannels, alphachan, scale); break;
    }
}


void PtexUtils::genRfaceids(const FaceInfo* faces, int nfaces,
			    uint32_t* rfaceids, uint32_t* faceids)
{
    // stable_sort faceids by smaller dimension (u or v) in descending order
    // treat const faces as having res of 1

    // init faceids
    for (int i = 0; i < nfaces; i++) faceids[i] = i;

    // sort faceids by rfaceid
    std::stable_sort(faceids, faceids + nfaces, CompareRfaceIds(faces));

    // generate mapping from faceid to rfaceid
    for (int i = 0; i < nfaces; i++) {
	// note: i is the rfaceid
	rfaceids[faceids[i]] = i;
    }
}

namespace {
    // apply to 1..4 channels, unrolled
    template<class T, int nChan>
    void ApplyConst(double weight, double* dst, void* data, int /*nChan*/)
    {
	// dst[i] += data[i] * weight for i in {0..n-1}
	PtexUtils::VecAccum<T,nChan>()(dst, (T*) data, weight);
    }

    // apply to N channels (general case)
    template<class T>
    void ApplyConstN(double weight, double* dst, void* data, int nChan)
    {
	// dst[i] += data[i] * weight for i in {0..n-1}
	PtexUtils::VecAccumN<T>()(dst, (T*) data, nChan, weight);
    }
}

PtexUtils::ApplyConstFn
PtexUtils::applyConstFunctions[20] = {
    ApplyConstN<uint8_t>,  ApplyConstN<uint16_t>,  ApplyConstN<PtexHalf>,  ApplyConstN<float>,
    ApplyConst<uint8_t,1>, ApplyConst<uint16_t,1>, ApplyConst<PtexHalf,1>, ApplyConst<float,1>,
    ApplyConst<uint8_t,2>, ApplyConst<uint16_t,2>, ApplyConst<PtexHalf,2>, ApplyConst<float,2>,
    ApplyConst<uint8_t,3>, ApplyConst<uint16_t,3>, ApplyConst<PtexHalf,3>, ApplyConst<float,3>,
    ApplyConst<uint8_t,4>, ApplyConst<uint16_t,4>, ApplyConst<PtexHalf,4>, ApplyConst<float,4>,
};
	
#ifndef PTEX_USE_STDSTRING
Ptex::String::~String()
{
    if (_str) free(_str);
}


Ptex::String& Ptex::String::operator=(const char* str)
{
    if (_str) free(_str);
    _str = str ? strdup(str) : 0;
    return *this;
}

std::ostream& operator << (std::ostream& stream, const Ptex::String& str)
{
    stream << str.c_str();
    return stream;
}
#endif

