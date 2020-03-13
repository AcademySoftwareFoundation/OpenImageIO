// -*- mode: C++; tab-width: 4 -*-
// vi: ts=4

/*
 * Copyright (c) 2009, Patrick A. Palmer.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 *   - Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *   - Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *   - Neither the name of Patrick A. Palmer nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 */
 
 

#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <limits>


#include "DPXHeader.h"
#include "EndianSwap.h"



// function prototypes
static void EmptyString(char *, const int);




char Hex(char x)
{
	if (x >= 10) 
		return (x-10+'A');
	else
		return (x+'0');
}



dpx::Header::Header() : GenericHeader(), IndustryHeader(), datumSwap(true)
{
}



dpx::GenericHeader::GenericHeader() 
{
	this->Reset();
}


void dpx::GenericHeader::Reset()
{
	// File Information
	this->magicNumber = MAGIC_COOKIE;
	this->imageOffset = ~0;
	EmptyString(this->version, sizeof(this->version));
	OIIO::Strutil::safe_strcpy(this->version, SMPTE_VERSION, sizeof(this->version));
	fileSize = sizeof(dpx::Header);
	this->dittoKey = 1;									// new

	// genericSize is the size of the file/image/orientation headers
	// sizeof(dpx::GenericHeader) won't give the correct results because
	// of compiler padding
	//		file header is 768 bytes
	//		image header is 640 bytes
	//		orientation header 256 bytes

	this->genericSize = 768 + 640 + 256;

	// industrySize is the size of the motion picture/television headers
	//		motion picture header is 256 bytes
	//		television header is 128 bytes
	this->industrySize = 256 + 128;

	this->userSize = 0;
	EmptyString(this->fileName, sizeof(this->fileName));
	EmptyString(this->creationTimeDate, sizeof(this->creationTimeDate));
	EmptyString(this->creator, sizeof(this->creator));
	EmptyString(this->project, sizeof(this->project));
	EmptyString(this->copyright, sizeof(this->copyright));
	this->encryptKey = 0xffffffff;
	EmptyString(this->reserved1, sizeof(this->reserved1));

	// Image Information
	this->imageOrientation = kUndefinedOrientation;
	this->numberOfElements = 0xffff;
	this->pixelsPerLine = this->linesPerElement = 0xffffffff;
	EmptyString(this->reserved2, sizeof(this->reserved2));

	// Image Orientation
	this->xOffset = this->yOffset = 0xffffffff;
	this->xCenter = this->yCenter = std::numeric_limits<float>::quiet_NaN();
	this->xOriginalSize = this->yOriginalSize = 0xffffffff;
	EmptyString(this->sourceImageFileName, sizeof(this->sourceImageFileName));
	EmptyString(this->sourceTimeDate, sizeof(this->sourceTimeDate));
	EmptyString(this->inputDevice, sizeof(this->inputDevice));
	EmptyString(this->inputDeviceSerialNumber, sizeof(this->inputDeviceSerialNumber));
	this->border[0] = this->border[1] = this->border[2] = this->border[3] = 0xffff;
	this->aspectRatio[0] = this->aspectRatio[1] = 0xffffffff;
	this->xScannedSize = this->yScannedSize = std::numeric_limits<float>::quiet_NaN();
	EmptyString(this->reserved3, sizeof(this->reserved3));
}


dpx::IndustryHeader::IndustryHeader()
{
	this->Reset();
}


void dpx::IndustryHeader::Reset()
{
	// Motion Picture Industry Specific
	EmptyString(this->filmManufacturingIdCode, sizeof(this->filmManufacturingIdCode));
	EmptyString(this->filmType, sizeof(this->filmType));
	EmptyString(this->perfsOffset, sizeof(this->perfsOffset));
	EmptyString(this->prefix, sizeof(this->prefix));
	EmptyString(this->count, sizeof(this->count));
	EmptyString(this->format, sizeof(this->format));
	this->framePosition = this->sequenceLength = this->heldCount = 0xffffffff;
	this->frameRate = this->shutterAngle = std::numeric_limits<float>::quiet_NaN();
	EmptyString(this->frameId, sizeof(this->frameId));
	EmptyString(this->slateInfo, sizeof(this->slateInfo));
	EmptyString(this->reserved4, sizeof(this->reserved4));

	// Television Industry Specific
	this->timeCode = this->userBits = 0xffffffff;
	this->interlace = this->fieldNumber = 0xff;
	this->videoSignal = kUndefined;
	this->zero = 0xff;
	this->horizontalSampleRate = this->verticalSampleRate = this->temporalFrameRate = std::numeric_limits<float>::quiet_NaN();
	this->timeOffset = this->gamma = std::numeric_limits<float>::quiet_NaN();
	this->blackLevel = this->blackGain = std::numeric_limits<float>::quiet_NaN();
	this->breakPoint = this->whiteLevel = this->integrationTimes = std::numeric_limits<float>::quiet_NaN();
	EmptyString(this->reserved5, sizeof(this->reserved5));
}


dpx::ImageElement::ImageElement()
{
	this->dataSign = 0xffffffff;
	this->lowData = 0xffffffff;
	this->lowQuantity = R32(0xffffffff);
	this->highData = 0xffffffff;
	this->highQuantity = R32(0xffffffff);
	this->descriptor = kUndefinedDescriptor;
	this->transfer = kUndefinedCharacteristic;	
	this->colorimetric = kUndefinedCharacteristic;
	this->bitDepth = 0xff;
	this->packing = this->encoding = 0xffff;
	this->dataOffset = this->endOfLinePadding = this->endOfImagePadding = 0xffffffff;
	EmptyString(this->description, sizeof(this->description));
}


bool dpx::Header::Read(InStream *io)
{
	// rewind file
	io->Rewind();

	// read in the header from the file
	size_t r = sizeof(GenericHeader) + sizeof(IndustryHeader);
	if (io->Read(&(this->magicNumber), r) != r)
		return false;

	// validate
	return this->Validate();
}


// Check to see if the compiler placed the data members in the expected memory offsets

bool dpx::Header::Check()
{
	// genericSize is the size of the file/image/orientation headers
	// sizeof(dpx::GenericHeader) won't give the correct results because
	// of compiler padding
	//		file header is 768 bytes
	//		image header is 640 bytes
	//		orientation header 256 bytes

	if (sizeof(GenericHeader) != (768 + 640 + 256))
		return false;

	// industrySize is the size of the motion picture/television headers
	//		motion picture header is 256 bytes
	//		television header is 128 bytes
	if (sizeof(IndustryHeader) != (256 + 128))
		return false;
	
	// data size checks
	if (sizeof(U8) != 1 || sizeof(U16) != 2 || sizeof(U32) != 4 || sizeof(R32) != 4 || sizeof(R64) != 8)
		return false;
		
	return true;
}



bool dpx::Header::Write(OutStream *io)
{
	// validate and byte swap, if necessary
	if (!this->Validate())
		return false;

	// write the header to the file
	size_t r = sizeof(GenericHeader) + sizeof(IndustryHeader);
	if (! io->WriteCheck(&(this->magicNumber), r))
		return false;

	// swap back - data is in file, now we need it native again
	this->Validate();
	return true;
}


bool dpx::Header::WriteOffsetData(OutStream *io)
{
	// calculate the number of elements
	this->CalculateNumberOfElements();

	// write the image offset
	const long FIELD2 = 4;			// offset to image in header
	if (io->Seek(FIELD2, OutStream::kStart) == false)
		return false;
	if (this->RequiresByteSwap())
		SwapBytes(this->imageOffset);
	if (!io->WriteCheck(&this->imageOffset, sizeof(U32)))
		return false;
	if (this->RequiresByteSwap())
		SwapBytes(this->imageOffset);
			
	
	// write the file size
	const long FIELD4 = 16;			// offset to total image file size in header
	if (io->Seek(FIELD4, OutStream::kStart) == false)
		return false;
	if (this->RequiresByteSwap())
		SwapBytes(this->fileSize);
	if (! io->WriteCheck(&this->fileSize, sizeof(U32)))
		return false;
	if (this->RequiresByteSwap())
		SwapBytes(this->fileSize);
			
	// write the number of elements
	const long FIELD19 = 770;		// offset to number of image elements in header
	if (io->Seek(FIELD19, OutStream::kStart) == false)
		return false;
	if (this->RequiresByteSwap())
		SwapBytes(this->numberOfElements);
	if (! io->WriteCheck(&this->numberOfElements, sizeof(U16)))
		return false;
	if (this->RequiresByteSwap())
		SwapBytes(this->numberOfElements);
	
	// write the image offsets
	const long FIELD21_12 = 808;	// offset to image offset in image element data structure
	const long IMAGE_STRUCTURE = 72;	// sizeof the image data structure
	
	int i;
	for (i = 0; i < MAX_ELEMENTS; i++)
	{
			// only write if there is a defined image description
			if (this->chan[i].descriptor == kUndefinedDescriptor)
				continue;
				
			// seek to the image offset entry in each image element
			if (io->Seek((FIELD21_12 + (IMAGE_STRUCTURE * i)), OutStream::kStart) == false)
				return false;

			// write
			if (this->RequiresByteSwap())
				SwapBytes(this->chan[i].dataOffset);
			if (! io->WriteCheck(&this->chan[i].dataOffset, sizeof(U32)))
				return false;
			if (this->RequiresByteSwap())
				SwapBytes(this->chan[i].dataOffset);

	}
	
	return true;
}


bool dpx::Header::ValidMagicCookie(const U32 magic)
{
	U32 mc = MAGIC_COOKIE;
	
	if (magic == mc)
		return true;
	else if (magic == SwapBytes(mc))
		return true;
	else
		return false;
}


bool dpx::Header::DetermineByteSwap(const U32 magic) const
{
	U32 mc = MAGIC_COOKIE;
	
	bool byteSwap = false;
	
	if (magic != mc)
		byteSwap = true;
	
	return byteSwap;
}

		
bool dpx::Header::Validate()
{
	// check magic cookie
	if (!this->ValidMagicCookie(this->magicNumber))
		return false;
		
	// determine if bytes needs to be swapped around
	if (this->DetermineByteSwap(this->magicNumber))
	{
		// File information
		SwapBytes(this->imageOffset);
		SwapBytes(this->fileSize);
		SwapBytes(this->dittoKey);
		SwapBytes(this->genericSize);
		SwapBytes(this->industrySize);
		SwapBytes(this->userSize);
		SwapBytes(this->encryptKey);

		// Image information
		SwapBytes(this->imageOrientation);
		SwapBytes(this->numberOfElements);
		SwapBytes(this->pixelsPerLine);
		SwapBytes(this->linesPerElement);
		for (int i = 0; i < MAX_ELEMENTS; i++) 
		{
			SwapBytes(this->chan[i].dataSign);
			SwapBytes(this->chan[i].lowData);
			SwapBytes(this->chan[i].lowQuantity);
			SwapBytes(this->chan[i].highData);
			SwapBytes(this->chan[i].highQuantity);
			SwapBytes(this->chan[i].descriptor);
			SwapBytes(this->chan[i].transfer);
			SwapBytes(this->chan[i].colorimetric);
			SwapBytes(this->chan[i].bitDepth);
			SwapBytes(this->chan[i].packing);
			SwapBytes(this->chan[i].encoding);
			SwapBytes(this->chan[i].dataOffset);
			SwapBytes(this->chan[i].endOfLinePadding);
			SwapBytes(this->chan[i].endOfImagePadding);
		}


		// Image Origination information
		SwapBytes(this->xOffset);
		SwapBytes(this->yOffset);
		SwapBytes(this->xCenter);
		SwapBytes(this->yCenter);
		SwapBytes(this->xOriginalSize);
		SwapBytes(this->yOriginalSize);
		SwapBytes(this->border[0]);
		SwapBytes(this->border[1]);
		SwapBytes(this->border[2]);
		SwapBytes(this->border[3]);
		SwapBytes(this->aspectRatio[0]);
		SwapBytes(this->aspectRatio[1]);


		// Motion Picture Industry Specific
		SwapBytes(this->framePosition);
		SwapBytes(this->sequenceLength);
		SwapBytes(this->heldCount);
		SwapBytes(this->frameRate);
		SwapBytes(this->shutterAngle);


		// Television Industry Specific
		SwapBytes(this->timeCode);
		SwapBytes(this->userBits);
		SwapBytes(this->interlace);
		SwapBytes(this->fieldNumber);
		SwapBytes(this->videoSignal);
		SwapBytes(this->zero);
		SwapBytes(this->horizontalSampleRate);
		SwapBytes(this->verticalSampleRate);
		SwapBytes(this->temporalFrameRate);
		SwapBytes(this->timeOffset);
		SwapBytes(this->gamma);
		SwapBytes(this->blackLevel);
		SwapBytes(this->blackGain);
		SwapBytes(this->breakPoint);
		SwapBytes(this->whiteLevel);
		SwapBytes(this->integrationTimes);
	}
	
	return true;
}



void dpx::Header::Reset()
{
	GenericHeader::Reset();
	IndustryHeader::Reset();
}


int dpx::GenericHeader::ImageElementComponentCount(const int element) const
{
	int count = 1;

	switch (this->chan[element].descriptor) 
	{
	case kUserDefinedDescriptor:
	case kRed:
	case kGreen:
	case kBlue:
	case kAlpha:
	case kLuma:
	case kColorDifference:
	case kDepth:
		count = 1;
		break;
	case kCompositeVideo:
		count = 1; 
		break;
	case kRGB:
		count = 3;
		break;
	case kRGBA:
	case kABGR:
		count = 4;
		break;
	case kCbYCrY:
		count = 2;
		break;
	case kCbYACrYA:
		count = 3;
		break;
	case kCbYCr:
		count = 3;
		break;
	case kCbYCrA:
		count = 4;
		break;
	case kUserDefined2Comp:			
		count = 2; 
		break;
	case kUserDefined3Comp:
		count = 3;
		break;
	case kUserDefined4Comp:
		count = 4;
		break;
	case kUserDefined5Comp:
		count = 5;
		break;
	case kUserDefined6Comp:
		count = 6;
		break;
	case kUserDefined7Comp:
		count = 7;
		break;
	case kUserDefined8Comp:
		count = 8;
		break;
	};

	return count;
}


int dpx::GenericHeader::ImageElementCount() const
{
	if(this->numberOfElements>0 && this->numberOfElements<=MAX_ELEMENTS)
		return this->numberOfElements;
	
	// If the image header does not list a valid number of elements,
	// count how many defined image descriptors we have...
	
	int i = 0;
	
	while (i < MAX_ELEMENTS )
	{
		if (this->ImageDescriptor(i) == kUndefinedDescriptor)
			break;
		i++;
	}
	
	return i;
}


void dpx::GenericHeader::CalculateNumberOfElements()
{
	this->numberOfElements = 0xffff;
	int i = this->ImageElementCount();
	
	if (i == 0)
		this->numberOfElements = 0xffff;
	else
		this->numberOfElements = U16(i);	
}


void dpx::Header::CalculateOffsets()
{
	int i;

	for (i = 0; i < MAX_ELEMENTS; i++)
	{
		// only write if there is a defined image description
		if (this->chan[i].descriptor == kUndefinedDescriptor)
			continue;
	

	}
}


dpx::DataSize dpx::GenericHeader::ComponentDataSize(const int element) const
{
	if (element < 0 || element >= MAX_ELEMENTS)
		return kByte;
		
	dpx::DataSize ret;
	
	switch (this->chan[element].bitDepth)
	{
	case 8:
		ret = kByte;
		break;
	case 10:
	case 12:
	case 16:
		ret = kWord;
		break;
	case 32:
		ret = kFloat;
		break;
	case 64:
		ret = kDouble;
		break;
	default:
		assert(0 && "Unknown bit depth");
		ret = kDouble;
		break;
	}
	
	return ret;
}


int dpx::GenericHeader::ComponentByteCount(const int element) const
{
	if (element < 0 || element >= MAX_ELEMENTS)
		return kByte;
		
	int ret;
	
	switch (this->chan[element].bitDepth)
	{
	case 8:
		ret = sizeof(U8);
		break;
	case 10:
	case 12:
	case 16:
		ret = sizeof(U16);
		break;
	case 32:
		ret = sizeof(R32);
		break;
	case 64:
		ret = sizeof(R64);
		break;
	default:
		assert(0 && "Unknown bit depth");
		ret = sizeof(R64);
		break;
	}
	
	return ret;
}


int dpx::GenericHeader::DataSizeByteCount(const DataSize ds)
{

	int ret;
	
	switch (ds)
	{
	case kByte:
		ret = sizeof(U8);
		break;
	case kWord:
		ret = sizeof(U16);
		break;
	case kInt:
		ret = sizeof(U32);
		break;
	case kFloat:
		ret = sizeof(R32);
		break;
	case kDouble:
		ret = sizeof(R64);
		break;
	default:
		assert(0 && "Unknown data size");
		ret = sizeof(R64);
		break;
	}
	
	return ret;
}


void dpx::IndustryHeader::FilmEdgeCode(char *edge) const
{
	edge[0] = this->filmManufacturingIdCode[0];
	edge[1] = this->filmManufacturingIdCode[1];
	edge[2] = this->filmType[0];
	edge[3] = this->filmType[1];
	edge[4] = this->perfsOffset[0];
	edge[5] = this->perfsOffset[1];
	edge[6] = this->prefix[0];
	edge[7] = this->prefix[1];
	edge[8] = this->prefix[2];
	edge[9] = this->prefix[3];
	edge[10] = this->prefix[4];
	edge[11] = this->prefix[5];
	edge[12] = this->count[0];
	edge[13] = this->count[1];
	edge[14] = this->count[2];
	edge[15] = this->count[3];
	edge[16] = '\0';
}


void dpx::IndustryHeader::SetFileEdgeCode(const char *edge)
{
	this->filmManufacturingIdCode[0] = edge[0];
	this->filmManufacturingIdCode[1] = edge[1];
	this->filmType[0] = edge[2];
	this->filmType[1] = edge[3];
	this->perfsOffset[0] = edge[4];
	this->perfsOffset[1] = edge[5];
	this->prefix[0] = edge[6];
	this->prefix[1] = edge[7];
	this->prefix[2] = edge[8];
	this->prefix[3] = edge[9];
	this->prefix[4] = edge[10];
	this->prefix[5] = edge[11];
	this->count[0] = edge[12];
	this->count[1] = edge[13];
	this->count[2] = edge[14];
	this->count[3] = edge[15];
}


void dpx::IndustryHeader::TimeCode(char *str) const
{
	U32 tc = this->timeCode;
	::sprintf(str, "%c%c:%c%c:%c%c:%c%c", 
		Hex((tc & 0xf0000000) >> 28),  Hex((tc & 0xf000000) >> 24),
		Hex((tc & 0xf00000) >> 20),  Hex((tc & 0xf0000) >> 16),
		Hex((tc & 0xf000) >> 12),  Hex((tc & 0xf00) >> 8),
		Hex((tc & 0xf0) >> 4),  Hex(tc & 0xf));
}


void dpx::IndustryHeader::UserBits(char *str) const
{
	U32 ub = this->userBits;
	::sprintf(str, "%c%c:%c%c:%c%c:%c%c", 
		Hex((ub & 0xf0000000) >> 28),  Hex((ub & 0xf000000) >> 24),
		Hex((ub & 0xf00000) >> 20),  Hex((ub & 0xf0000) >> 16),
		Hex((ub & 0xf000) >> 12),  Hex((ub & 0xf00) >> 8),
		Hex((ub & 0xf0) >> 4),  Hex(ub & 0xf));
}


dpx::U32 dpx::IndustryHeader::TCFromString(const char *str) const
{
	// make sure the string is the correct length
	if (::strlen(str) != 11)
		return U32(~0);

	U32 tc = 0;
	int i, idx = 0;
	U8 ch;
	U32 value, mask;

	for (i = 0; i < 8; i++, idx++)
	{
		// determine string index skipping :
		idx += idx % 3 == 2 ? 1 : 0;
		ch = str[idx];

		// error check
		if (ch < '0' || ch > '9')
			return 0xffffffff;

		value = U32(ch - '0') << (28 - (i*4));
		mask = 0xf << (28 - (i*4));

		// mask in new value
		tc = (tc & ~mask) | (value & mask);
	}

	return tc;
}


void dpx::IndustryHeader::SetTimeCode(const char *str)
{
	U32 tc = this->TCFromString(str);
	if (tc != 0xffffffff)
		this->timeCode = tc;
}


void dpx::IndustryHeader::SetUserBits(const char *str)
{
	U32 ub = this->TCFromString(str);
	if (ub != 0xffffffff)
		this->userBits = ub;
}
		


static void EmptyString(char *str, const int len)
{
	for (int i = 0; i < len; i++)
		str[i] = '\0';
}


void dpx::GenericHeader::SetCreationTimeDate(const long sec)
{
	struct tm *tm_time;
	char str[32];
	
#ifdef _WIN32
	_tzset();
#endif

	const time_t t = time_t(sec);
	tm_time = ::localtime(&t);
	::strftime(str, 32, "%Y:%m:%d:%H:%M:%S%Z", tm_time);
	OIIO::Strutil::safe_strcpy(this->creationTimeDate, str, 24);
}


void dpx::GenericHeader::SetSourceTimeDate(const long sec)
{
	struct tm *tm_time;
	char str[32];
	
#ifdef _WIN32
	_tzset();
#endif

	const time_t t = time_t(sec);
	tm_time = ::localtime(&t);
	::strftime(str, 32, "%Y:%m:%d:%H:%M:%S%Z", tm_time);
	OIIO::Strutil::safe_strcpy(this->sourceTimeDate, str, 24);
}



bool dpx::Header::DatumSwap(const int element) const
{
	if (this->datumSwap)
	{
		if (this->ImageDescriptor(element) == kRGB || this->ImageDescriptor(element) == kCbYCrY)
			return true;
	}
	return false;
}


void dpx::Header::SetDatumSwap(const bool swap)
{
	this->datumSwap = swap;
}

// Height()
// this function determines the height of the image taking in account for the image orientation
// if an image is 1920x1080 but is oriented top to bottom, left to right then the height stored
// in the image is 1920 rather than 1080

dpx::U32 dpx::Header::Height() const
{
	U32 h;
	
	switch (this->ImageOrientation())
	{
	case kTopToBottomLeftToRight:
	case kTopToBottomRightToLeft:
	case kBottomToTopLeftToRight:
	case kBottomToTopRightToLeft:
		h = this->PixelsPerLine();
		break;
	default:	
		h = this->LinesPerElement();
		break;
	}

	return h;
}


// Width()
// this function determines the width of the image taking in account for the image orientation
// if an image is 1920x1080 but is oriented top to bottom, left to right then the width stored
// in the image is 1920 rather than 1080

dpx::U32 dpx::Header::Width() const
{
	U32 w;

	switch (this->ImageOrientation())
	{
	case kTopToBottomLeftToRight:
	case kTopToBottomRightToLeft:
	case kBottomToTopLeftToRight:
	case kBottomToTopRightToLeft:
		w = this->LinesPerElement();
		break;
	default:	
		w = this->PixelsPerLine();
		break;
	}

	return w;
}



