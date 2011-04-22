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

#include <cstring>
#include <ctime>

#include "DPX.h"
#include "DPXStream.h"
#include "EndianSwap.h"
#include "WriterInternal.h"



dpx::Writer::Writer() : fileLoc(0)
{
}


dpx::Writer::~Writer()
{
}


void dpx::Writer::Start()
{
}


void dpx::Writer::SetFileInfo(const char *fileName, const char *creationTimeDate, const char *creator,
			const char *project, const char *copyright, const U32 encryptKey, const bool swapEndian)
{
	if (fileName)
		this->header.SetFileName(fileName);

	if (creationTimeDate)
		this->header.SetCreationTimeDate(creationTimeDate);
	else
	{	
		time_t seconds = time(0);
		this->header.SetCreationTimeDate(seconds);
	}
	
	if (creator)
		this->header.SetCreator(creator);
	else
		this->header.SetCreator("OpenDPX library");
		
	if (project)
		this->header.SetProject(project);
	if (copyright)
		this->header.SetCopyright(copyright);
	this->header.SetEncryptKey(encryptKey);
	
	if (swapEndian)
	    this->header.magicNumber = SwapBytes(this->header.magicNumber);
}


void dpx::Writer::SetImageInfo(const U32 width, const U32 height)
{
	this->header.SetImageOrientation(kLeftToRightTopToBottom);
	this->header.SetPixelsPerLine(width);
	this->header.SetLinesPerElement(height);
}

		
// returns next available or MAX_ELEMENTS if full
int dpx::Writer::NextAvailElement() const
{
	unsigned int i;
	
	for (i = 0; i < MAX_ELEMENTS; i++)
	{
		if (this->header.ImageDescriptor(i) == kUndefinedDescriptor)
			break;
	}
	
	return i;
}


void dpx::Writer::SetOutStream(OutStream *fd)
{
	this->fd = fd;
}


bool dpx::Writer::WriteHeader()
{
	// calculate any header info
	this->header.CalculateOffsets();

	// seek to the beginning of the file	
	if (!this->fd->Seek(0, OutStream::kStart))
		return false;

	// writing the header count
	this->fileLoc = this->header.Size(); 
	
	return this->header.Write(fd);
}


void dpx::Writer::SetUserData(const long size)
{
	// TODO
}


bool dpx::Writer::WriteUserData(void *data)
{
	// XXX TODO
	return false;
}


void dpx::Writer::SetElement(const int num, const Descriptor desc, const U8 bitDepth,
			const Characteristic transfer, const Characteristic colorimetric,
			const Packing packing, const Encoding encoding, const U32 dataSign, 
			const U32 lowData, const R32 lowQuantity,
			const U32 highData, const R32 highQuantity,
			const U32 eolnPadding, const U32 eoimPadding)
{
	// make sure the range is good
	if (num < 0 || num >= MAX_ELEMENTS)
		return;

	// set values
	this->header.SetDataSign(num, dataSign);
	this->header.SetLowData(num, lowData);
	this->header.SetLowQuantity(num, lowQuantity);
	this->header.SetHighData(num, highData);
	this->header.SetHighQuantity(num, highQuantity);
	this->header.SetImageDescriptor(num, desc);
	this->header.SetTransfer(num, transfer);	
	this->header.SetColorimetric(num, colorimetric);
	this->header.SetBitDepth(num, bitDepth);
	this->header.SetImagePacking(num, packing);
	this->header.SetImageEncoding(num, encoding);
	this->header.SetEndOfLinePadding(num, eolnPadding);
	this->header.SetEndOfImagePadding(num, eoimPadding);

	// determine if increases element count
	this->header.CalculateNumberOfElements();
}


// the data is processed so write it straight through
// argument count is total size in bytes of the passed data
bool dpx::Writer::WriteElement(const int element, void *data, const long count)
{
	// make sure the range is good
	if (element < 0 || element >= MAX_ELEMENTS)
		return false;

	// make sure the entry is valid
	if (this->header.ImageDescriptor(element) == kUndefinedDescriptor)
		return false;

	// update file ptr
	this->header.SetDataOffset(element, this->fileLoc);
	this->fileLoc += count;
		
	// write
	return (this->fd->Write(data, count) > 0);
}



bool dpx::Writer::WriteElement(const int element, void *data)
{
	// make sure the range is good
	if (element < 0 || element >= MAX_ELEMENTS)
		return false;

	// make sure the entry is valid
	if (this->header.ImageDescriptor(element) == kUndefinedDescriptor)
		return false;

	return this->WriteElement(element, data, this->header.ComponentDataSize(element));
}



bool dpx::Writer::WriteElement(const int element, void *data, const DataSize size)
{
	bool status = true;	
	
	// make sure the range is good
	if (element < 0 || element >= MAX_ELEMENTS)
		return false;

	// make sure the entry is valid
	if (this->header.ImageDescriptor(element) == kUndefinedDescriptor)
		return false;

	// mark location in headers
	if (element == 0)
		this->header.SetImageOffset(this->fileLoc);
	this->header.SetDataOffset(element, this->fileLoc);
	
	// reverse the order of the components
	bool reverse = false;

	// rle encoding?
	const bool rle = this->header.ImageEncoding(element) == kRLE;
	
	// image parameters
	const U32 eolnPad = this->header.EndOfLinePadding(element);
	const U32 eoimPad = this->header.EndOfImagePadding(element);
	const U8 bitDepth = this->header.BitDepth(element);
	const U32 width = this->header.Width();
	const U32 height = this->header.Height();
	const int noc = this->header.ImageElementComponentCount(element);
	const Packing packing = this->header.ImagePacking(element);

	// check width & height, just in case
	if (width == 0 || height == 0)
		return false;

	//  sizeof a component in an image
	const int bytes = (bitDepth + 7) / 8;

	// allocate memory for use to write blank space
	char *blank = 0;
	if (eolnPad || eoimPad)
	{
		int bsize = eolnPad > eoimPad ? eolnPad : eoimPad;
		blank = new char[bsize];
		memset(blank, 0, bsize * sizeof(char));
	}

	// can we write the entire memory chunk at once without any additional processing
	if (!rle  &&
		((bitDepth == 8 && size == dpx::kByte) ||
		 (bitDepth == 12 && size == dpx::kWord && packing == kFilledMethodA) || 
		 (bitDepth == 16 && size == dpx::kWord) || 
		 (bitDepth == 32 && size == dpx::kFloat) ||
		 (bitDepth == 64 && size == dpx::kDouble)))
	{
		status = this->WriteThrough(data, width, height, noc, bytes, eolnPad, eoimPad, blank);
		if (blank)
			delete [] blank;
		return status;
	}
	else 
	{
		switch (bitDepth)
		{
		case 8:
			if (size == dpx::kByte)
				this->fileLoc += WriteBuffer<U8, 8, true>(this->fd, size, data, width, height, noc, packing, rle, reverse, eolnPad, blank, status, this->header.RequiresByteSwap());
			else
				this->fileLoc += WriteBuffer<U8, 8, false>(this->fd, size, data, width, height, noc, packing, rle, reverse, eolnPad, blank, status, this->header.RequiresByteSwap());
			break;

		case 10:
			// are the channels stored in reverse
			if (this->header.ImageDescriptor(element) == kRGB && this->header.DatumSwap(element) && bitDepth == 10)
				reverse = true;

			if (size == dpx::kWord)
				this->fileLoc += WriteBuffer<U16, 10, true>(this->fd, size, data, width, height, noc, packing, rle, reverse, eolnPad, blank, status, this->header.RequiresByteSwap());
			else
				this->fileLoc += WriteBuffer<U16, 10, false>(this->fd, size, data, width, height, noc, packing, rle, reverse, eolnPad, blank, status, this->header.RequiresByteSwap());
			break;

		case 12:
			if (size == dpx::kWord)
				this->fileLoc += WriteBuffer<U16, 12, true>(this->fd, size, data, width, height, noc, packing, rle, reverse, eolnPad, blank, status, this->header.RequiresByteSwap());
			else
				this->fileLoc += WriteBuffer<U16, 12, false>(this->fd, size, data, width, height, noc, packing, rle, reverse, eolnPad, blank, status, this->header.RequiresByteSwap());
			break;

		case 16:
			if (size == dpx::kWord)
				this->fileLoc += WriteBuffer<U16, 16, true>(this->fd, size, data, width, height, noc, packing, rle, reverse, eolnPad, blank, status, this->header.RequiresByteSwap());
			else
				this->fileLoc += WriteBuffer<U16, 16, false>(this->fd, size, data, width, height, noc, packing, rle, reverse, eolnPad, blank, status, this->header.RequiresByteSwap());
			break;

		case 32:
			if (size == dpx::kFloat)
				this->fileLoc += WriteFloatBuffer<R32, 32, true>(this->fd, size, data, width, height, noc, packing, rle, eolnPad, blank, status, this->header.RequiresByteSwap());
			else
				this->fileLoc += WriteFloatBuffer<R32, 32, false>(this->fd, size, data, width, height, noc, packing, rle, eolnPad, blank, status, this->header.RequiresByteSwap());
			break;

		case 64:
			if (size == dpx::kDouble)
				this->fileLoc += WriteFloatBuffer<R64, 64, true>(this->fd, size, data, width, height, noc, packing, rle, eolnPad, blank, status, this->header.RequiresByteSwap());
			else
				this->fileLoc += WriteFloatBuffer<R64, 64, false>(this->fd, size, data, width, height, noc, packing, rle, eolnPad, blank, status, this->header.RequiresByteSwap());
			break;
		}
	}
	
	// if successful
	if (status && eoimPad)
	{
		// end of image padding
		this->fileLoc += eoimPad;
		status = (this->fd->Write(blank, eoimPad) > 0);
	}
	
	// rid of memory
	if (blank)
		delete [] blank;
		
	return status;
}


// the passed in image buffer is written to the file untouched

bool dpx::Writer::WriteThrough(void *data, const U32 width, const U32 height, const int noc, const int bytes, const U32 eolnPad, const U32 eoimPad, char *blank)
{
	bool status = true;
	const int count = width * height * noc;
	unsigned int i;
	unsigned char *imageBuf = reinterpret_cast<unsigned char*>(data);
	
	// file pointer location after write
	this->fileLoc += bytes * count + (eolnPad * height);
		
	// write data
	if (eolnPad)
	{
		// loop if have end of line padding
		for (i = 0; i < height; i++)
		{
			// write one line
			if (this->fd->Write(imageBuf+(width*bytes*i), bytes * width) == false)
			{
				status = false;
				break;
			}
			
			// write end of line padding
			if (this->fd->Write(blank, eoimPad) == false)
			{
				status = false;
				break;
			}
		}
	}
	else
	{
		// write data as one chunk
		if (this->fd->Write(imageBuf, bytes * count) == false)
		{
			status = false;
		}
	}		

	// end of image padding
	if (status && eoimPad)
	{
		this->fileLoc += eoimPad;
		status = (this->fd->Write(blank, eoimPad) > 0);
	}
	
	return status;
}




bool dpx::Writer::Finish()
{
	// write the file size in the header
	this->header.SetFileSize(this->fileLoc);
	
	// rewrite all of the offsets in the header
	return this->header.WriteOffsetData(this->fd);
}




