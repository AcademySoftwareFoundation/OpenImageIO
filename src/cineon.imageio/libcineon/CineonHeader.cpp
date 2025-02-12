// -*- mode: C++; tab-width: 4 -*-
// vi: ts=4

/*
 * Copyright (c) 2010, Patrick A. Palmer and Leszek Godlewski.
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
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>

#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>

#include "CineonHeader.h"
#include "EndianSwap.h"



// shortcut macros
#define EmptyString(x)	memset(x, 0, sizeof(x))
#define EmptyFloat(x)	x = std::numeric_limits<R32>::infinity()
#define EmptyVector(x)	(EmptyFloat((x)[0]),	\
						EmptyFloat((x)[1]))


namespace cineon {

char Hex(char x)
{
	if (x >= 10)
		return (x-10+'A');
	else
		return (x+'0');
}



cineon::Header::Header() : GenericHeader(), IndustryHeader()
{
}



cineon::GenericHeader::GenericHeader()
{
	this->Reset();
}


void cineon::GenericHeader::Reset()
{
	// File Information
	this->magicNumber = CINEON_MAGIC_COOKIE;
	this->imageOffset = ~0;
	EmptyString(this->version);
	OIIO::Strutil::safe_strcpy(this->version, SPEC_VERSION, sizeof(this->version));
	fileSize = sizeof(cineon::Header);

	// genericSize is the size of the file/image/orientation headers
	// sizeof(cineon::GenericHeader) won't give the correct results because
	// of compiler padding
	this->genericSize = 1024;

	// industrySize is the size of the motion picture/television headers
	this->industrySize = 1024;

	this->userSize = 0;
	EmptyString(this->fileName);
	EmptyString(this->creationDate);
	EmptyString(this->creationTime);
	EmptyString(this->reserved1);

	// Image Information
	this->imageOrientation = kUndefinedOrientation;
	this->numberOfElements = 0xff;
	this->unused1[0] = this->unused1[1] = 0xff;
	EmptyVector(this->whitePoint);
	EmptyVector(this->redPrimary);
	EmptyVector(this->greenPrimary);
	EmptyVector(this->bluePrimary);
	EmptyString(this->labelText);
	EmptyString(this->reserved2);
	this->interleave = 0xff;
	this->packing = 0xff;
	this->dataSign = 0xff;
	this->imageSense = 0xff;
	this->endOfLinePadding = 0xffffffff;
	this->endOfImagePadding = 0xffffffff;

	// Image Orientation
	this->xOffset = this->yOffset = 0xffffffff;
	EmptyString(this->sourceImageFileName);
	EmptyString(this->sourceDate);
	EmptyString(this->sourceTime);
	EmptyString(this->inputDevice);
	EmptyString(this->inputDeviceModelNumber);
	EmptyString(this->inputDeviceSerialNumber);
	EmptyFloat(this->xDevicePitch);
	EmptyFloat(this->yDevicePitch);
	EmptyFloat(this->gamma);
	EmptyString(this->reserved3);
	EmptyString(this->reserved4);
}


cineon::IndustryHeader::IndustryHeader()
{
	this->Reset();
}


void cineon::IndustryHeader::Reset()
{
	// Motion Picture Industry Specific
	this->filmManufacturingIdCode = 0xFF;
	this->filmType = 0xFF;
	this->perfsOffset = 0xFF;
	this->prefix = 0xFFFFFFFF;
	this->count = 0xFFFFFFFF;
	EmptyString(this->format);
	this->framePosition = 0xffffffff;
	EmptyFloat(this->frameRate);
	EmptyString(this->frameId);
	EmptyString(this->slateInfo);
	EmptyString(this->reserved1);
}


cineon::ImageElement::ImageElement()
{
	this->lowData = R32(0xffffffff);
	this->lowQuantity = R32(0xffffffff);
	this->highData = R32(0xffffffff);
	this->highQuantity = R32(0xffffffff);
	this->bitDepth = 0xff;
}


bool cineon::Header::Read(InStream *io)
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

bool cineon::Header::Check()
{
	// genericSize is the size of the file/image/orientation headers
	// sizeof(cineon::GenericHeader) won't give the correct results because
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



#ifdef OIIO_DOES_NOT_NEED_THIS

bool cineon::Header::Write(OutStream *io)
{
	// write the header to the file
	size_t r = sizeof(GenericHeader) + sizeof(IndustryHeader);
	if (io->Write(&(this->magicNumber), r) != r)
		return false;
	return true;
}


bool cineon::Header::WriteOffsetData(OutStream *io)
{
	// calculate the number of elements
	this->CalculateNumberOfElements();

	// write the image offset
	const long FIELD2 = 4;			// offset to image in header
	if (io->Seek(FIELD2, OutStream::kStart) == false)
		return false;
	if (io->Write(&this->imageOffset, sizeof(U32)) == false)
		return false;


	// write the file size
	const long FIELD4 = 16;			// offset to total image file size in header
	if (io->Seek(FIELD4, OutStream::kStart) == false)
		return false;
	if (io->Write(&this->fileSize, sizeof(U32)) == false)
		return false;

	// write the number of elements
	const long FIELD19 = 770;		// offset to number of image elements in header
	if (io->Seek(FIELD19, OutStream::kStart) == false)
		return false;
	if (io->Write(&this->numberOfElements, sizeof(U16)) == false)
		return false;

	// write the image offsets
	//const long FIELD21_12 = 808;	// offset to image offset in image element data structure
	//const long IMAGE_STRUCTURE = 72;	// sizeof the image data structure

	/*int i;
	for (i = 0; i < CINEON_MAX_ELEMENTS; i++)
	{
			// only write if there is a defined image description
			if (this->chan[i].descriptor == kUndefinedDescriptor)
				continue;

			// seek to the image offset entry in each image element
			if (io->Seek((FIELD21_12 + (IMAGE_STRUCTURE * i)), OutStream::kStart) == false)
				return false;

			// write
			if (io->Write(&this->chan[i].dataOffset, sizeof(U32)) == false)
				return false;

	}*/

	return true;
}
#endif  /* OIIO_DOES_NOT_NEED_THIS */


bool cineon::Header::ValidMagicCookie(const U32 magic)
{
	U32 mc = CINEON_MAGIC_COOKIE;

	if (magic == mc)
		return true;
	else if (magic == SwapBytes(mc))
		return true;
	else
		return false;
}


bool cineon::Header::DetermineByteSwap(const U32 magic) const
{
	U32 mc = CINEON_MAGIC_COOKIE;

	bool byteSwap = false;

	if (magic != mc)
		byteSwap = true;

	return byteSwap;
}


bool cineon::Header::Validate()
{
	// check magic cookie
	if (!this->ValidMagicCookie(this->magicNumber))
		return false;

	// determine if bytes needs to be swapped around
	if (this->DetermineByteSwap(this->magicNumber))
	{
		// File information
		SwapBytes(this->imageOffset);
		SwapBytes(this->genericSize);
		SwapBytes(this->industrySize);
		SwapBytes(this->userSize);
		SwapBytes(this->fileSize);

		// Image information
		for (int i = 0; i < CINEON_MAX_ELEMENTS; i++)
		{
			SwapBytes(this->chan[i].pixelsPerLine);
			SwapBytes(this->chan[i].linesPerElement);
			SwapBytes(this->chan[i].lowData);
			SwapBytes(this->chan[i].lowQuantity);
			SwapBytes(this->chan[i].highData);
			SwapBytes(this->chan[i].highQuantity);
			SwapBytes(this->chan[i].bitDepth);
		}
		SwapBytes(this->whitePoint[0]);
		SwapBytes(this->whitePoint[1]);
		SwapBytes(this->redPrimary[0]);
		SwapBytes(this->redPrimary[1]);
		SwapBytes(this->greenPrimary[0]);
		SwapBytes(this->greenPrimary[1]);
		SwapBytes(this->bluePrimary[0]);
		SwapBytes(this->bluePrimary[1]);
		SwapBytes(this->endOfLinePadding);
		SwapBytes(this->endOfImagePadding);


		// Image Origination information
		SwapBytes(this->xOffset);
		SwapBytes(this->yOffset);
		SwapBytes(this->xDevicePitch);
		SwapBytes(this->yDevicePitch);
		SwapBytes(this->gamma);


		// Motion Picture Industry Specific
		SwapBytes(this->prefix);
		SwapBytes(this->count);
		SwapBytes(this->framePosition);
		SwapBytes(this->frameRate);

	}

	return true;
}



void cineon::Header::Reset()
{
	GenericHeader::Reset();
	IndustryHeader::Reset();
}



int cineon::GenericHeader::ImageElementCount() const
{
	int i = 0;

	while (i < CINEON_MAX_ELEMENTS )
	{
		if (this->ImageDescriptor(i) == kUndefinedDescriptor)
			break;
		i++;
	}

	return i;
}


#ifdef OIIO_DOES_NOT_NEED_THIS
void cineon::GenericHeader::CalculateNumberOfElements()
{
	int i = this->ImageElementCount();

	if (i == 0)
		this->numberOfElements = 0xff;
	else
		this->numberOfElements = U8(i);
}


void cineon::Header::CalculateOffsets()
{
	int i;

	for (i = 0; i < CINEON_MAX_ELEMENTS; i++)
	{
		// only write if there is a defined image description
		if (this->chan[i].designator[1] == kUndefinedDescriptor)
			continue;


	}
}
#endif  /* OIIO_DOES_NOT_NEED_THIS */


cineon::DataSize cineon::GenericHeader::ComponentDataSize(const int element) const
{
	if (element < 0 || element >= CINEON_MAX_ELEMENTS)
		return kByte;

	cineon::DataSize ret;

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
		ret = kInt;
		break;
	case 64:
		ret = kLongLong;
		break;
	default:
		assert(0 && "Unknown bit depth");
		ret = kLongLong;
		break;
	}

	return ret;
}


int cineon::GenericHeader::ComponentByteCount(const int element) const
{
	if (element < 0 || element >= CINEON_MAX_ELEMENTS)
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


int cineon::GenericHeader::DataSizeByteCount(const DataSize ds)
{

	int ret = 0;

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
	case kLongLong:
		ret = sizeof(U64);
		break;
	}

	return ret;
}


void cineon::IndustryHeader::FilmEdgeCode(char *edge, size_t size) const
{
        if (this->filmManufacturingIdCode == 0xff && this->filmType == 0xff
            && this->perfsOffset == 0xff && this->prefix == 0xffffffff
            && this->count == 0xffffffff)
                *edge = 0;
        else {
            std::string e = OIIO::Strutil::fmt::format(
                "{:02}{:02}{:02}{:06}{:04}",
                (unsigned int)this->filmManufacturingIdCode,
                (unsigned int)this->filmType, (unsigned int)this->perfsOffset,
                this->prefix, this->count);
            OIIO::Strutil::safe_strcpy(edge, e, size);
        }
}


#ifdef OIIO_DOES_NOT_NEED_THIS
void cineon::IndustryHeader::SetFilmEdgeCode(const char *edge)
{
	this->filmManufacturingIdCode = OIIO::Strutil::stoi(OIIO::string_view(edge, 2));
	this->filmType = OIIO::Strutil::stoi(OIIO::string_view(edge + 2, 2));
	this->perfsOffset = OIIO::Strutil::stoi(OIIO::string_view(edge + 4, 2));
	this->prefix = OIIO::Strutil::stoi(OIIO::string_view(edge + 6, 6));
	this->count = OIIO::Strutil::stoi(OIIO::string_view(edge + 12, 4));
}
#endif  /* OIIO_DOES_NOT_NEED_THIS */


void cineon::GenericHeader::SetCreationTimeDate(const long sec)
{
	char str[32];

#ifdef _WIN32
	_tzset();
#endif

	const time_t t = time_t(sec);
    struct tm localtm;
    OIIO::Sysutil::get_local_time(&t, &localtm);
    ::strftime(str, 32, "%Y:%m:%d:%H:%M:%S%Z", &localtm);
	OIIO::Strutil::safe_strcpy(this->creationDate, str, 11);
	OIIO::Strutil::safe_strcpy(this->creationTime, str + 11, 12);
}


void cineon::GenericHeader::SetSourceTimeDate(const long sec)
{
	char str[32];

#ifdef _WIN32
	_tzset();
#endif

	const time_t t = time_t(sec);
    struct tm localtm;
    OIIO::Sysutil::get_local_time(&t, &localtm);
    ::strftime(str, 32, "%Y:%m:%d:%H:%M:%S%Z", &localtm);
	OIIO::Strutil::safe_strcpy(this->sourceDate, str, 11);
	OIIO::Strutil::safe_strcpy(this->sourceTime, str + 11, 12);
}



// Height()
// this function determines the height of the image taking in account for the image orientation
// if an image is 1920x1080 but is oriented top to bottom, left to right then the height stored
// in the image is 1920 rather than 1080

cineon::U32 cineon::Header::Height() const
{
	U32 h = 0;

	for (int i = 0; i < this->NumberOfElements(); i++) {
		switch (this->ImageOrientation())
		{
		case kTopToBottomLeftToRight:
		case kTopToBottomRightToLeft:
		case kBottomToTopLeftToRight:
		case kBottomToTopRightToLeft:
			if (this->PixelsPerLine(i) > h)
				h = this->PixelsPerLine(i);
			break;
		default:
			if (this->LinesPerElement(i) > h)
				h = this->LinesPerElement(i);
			break;
		}
	}

	return h;
}


// Width()
// this function determines the width of the image taking in account for the image orientation
// if an image is 1920x1080 but is oriented top to bottom, left to right then the width stored
// in the image is 1920 rather than 1080

cineon::U32 cineon::Header::Width() const
{
	U32 w = 0;

	for (int i = 0; i < this->NumberOfElements(); i++) {
		switch (this->ImageOrientation())
		{
		case kTopToBottomLeftToRight:
		case kTopToBottomRightToLeft:
		case kBottomToTopLeftToRight:
		case kBottomToTopRightToLeft:
			if (this->LinesPerElement(i) > w)
				w = this->LinesPerElement(i);
			break;
		default:
			if (this->PixelsPerLine(i) > w)
				w = this->PixelsPerLine(i);
			break;
		}
	}

	return w;
}

}

