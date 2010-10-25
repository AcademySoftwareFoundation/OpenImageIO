#ifndef PtexIO_h
#define PtexIO_h

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

struct PtexIO : public Ptex {
    struct Header {
	uint32_t magic;
	uint32_t version;
	MeshType meshtype:32;
	DataType datatype:32;
	int32_t  alphachan;
	uint16_t nchannels;
	uint16_t nlevels;
	uint32_t nfaces;
	uint32_t extheadersize;
	uint32_t faceinfosize;
	uint32_t constdatasize;
	uint32_t levelinfosize;
	uint32_t minorversion;
	uint64_t leveldatasize;
	uint32_t metadatazipsize;
	uint32_t metadatamemsize;
	int pixelSize() const { return DataSize(datatype) * nchannels; }
	bool hasAlpha() const { return alphachan >= 0 && alphachan < nchannels; }
    };
    struct ExtHeader {
	BorderMode ubordermode:32;
	BorderMode vbordermode:32;
	uint32_t lmdheaderzipsize;
	uint32_t lmdheadermemsize;
	uint64_t lmddatasize;
	uint64_t editdatasize;
	uint64_t editdatapos;
    };
    struct LevelInfo {
	uint64_t leveldatasize;
	uint32_t levelheadersize;
	uint32_t nfaces;
	LevelInfo() : leveldatasize(0), levelheadersize(0), nfaces(0) {}
    };
    enum Encoding { enc_constant, enc_zipped, enc_diffzipped, enc_tiled };
    struct FaceDataHeader {
	uint32_t data; // bits 0..29 = blocksize, bits 30..31 = encoding
	uint32_t blocksize() const { return data & 0x3fffffff; }
	Encoding encoding() const { return Encoding((data >> 30) & 0x3); }
	uint32_t& val() { return *(uint32_t*) this; }
	const uint32_t& val() const { return *(uint32_t*) this; }
	void set(uint32_t blocksize, Encoding encoding)
	{ data = (blocksize & 0x3fffffff) | ((encoding & 0x3) << 30); }
	FaceDataHeader() : data(0) {}
    };
    enum EditType { et_editfacedata, et_editmetadata };
    struct EditFaceDataHeader {
	uint32_t faceid;
	FaceInfo faceinfo;
	FaceDataHeader fdh;
    };
    struct EditMetaDataHeader {
	uint32_t metadatazipsize;
	uint32_t metadatamemsize;
    };

    static const uint32_t Magic = 'P' | ('t'<<8) | ('e'<<16) | ('x'<<24);
    static const int HeaderSize = sizeof(Header);
    static const int ExtHeaderSize = sizeof(ExtHeader);
    static const int LevelInfoSize = sizeof(LevelInfo);
    static const int FaceDataHeaderSize = sizeof(FaceDataHeader);
    static const int EditFaceDataHeaderSize = sizeof(EditFaceDataHeader);
    static const int EditMetaDataHeaderSize = sizeof(EditMetaDataHeader);

    // these constants can be tuned for performance
    static const int BlockSize = 16384;        // target block size for file I/O
    static const int TileSize  = 65536;        // target tile size (uncompressed)
    static const int AllocaMax = 16384;        // max size for using alloca
    static const int MetaDataThreshold = 1024; // cutoff for large meta data

    static bool LittleEndian() {
	short word = 0x0201;
	return *(char*)&word == 1; 
    }
};

#endif
