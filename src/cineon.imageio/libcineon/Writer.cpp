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

#include <cstring>
#include <ctime>

#if defined(__GNUC__)
#    pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#include "Cineon.h"
#include "CineonStream.h"
#include "EndianSwap.h"
#include "WriterInternal.h"



cineon::Writer::Writer() : fileLoc(0)
{
}


cineon::Writer::~Writer()
{
}


void cineon::Writer::Start()
{
}


void cineon::Writer::SetFileInfo(const char *fileName, const char *creationDate, const char *creationTime)
{
	if (fileName)
		this->header.SetFileName(fileName);

	if (creationDate && creationTime) {
		this->header.SetCreationDate(creationDate);
		this->header.SetCreationTime(creationTime);
	} else {
		time_t seconds = time(0);
		this->header.SetCreationTimeDate(seconds);
	}
}


void cineon::Writer::SetImageInfo(const U32 width, const U32 height)
{
	this->header.SetImageOrientation(kLeftToRightTopToBottom);
}


// returns next available or CINEON_MAX_ELEMENTS if full
int cineon::Writer::NextAvailElement() const
{
	unsigned int i;

	for (i = 0; i < CINEON_MAX_ELEMENTS; i++)
	{
		if (this->header.ImageDescriptor(i) == kUndefinedDescriptor)
			break;
	}

	return i;
}


void cineon::Writer::SetOutStream(OutStream *fd)
{
	this->fd = fd;
}


bool cineon::Writer::WriteHeader()
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


void cineon::Writer::SetUserData(const long size)
{
	// TODO
}


bool cineon::Writer::WriteUserData(void *data)
{
	// XXX TODO
	return false;
}


void cineon::Writer::SetElement(const int num, const Descriptor desc, const U8 bitDepth,
			const U32 pixelsPerLine,
			const U32 linesPerElement,
			const R32 lowData, const R32 lowQuantity,
			const R32 highData, const R32 highQuantity)
{
	// make sure the range is good
	if (num < 0 || num >= CINEON_MAX_ELEMENTS)
		return;

	// set values
	this->header.SetLowData(num, lowData);
	this->header.SetLowQuantity(num, lowQuantity);
	this->header.SetHighData(num, highData);
	this->header.SetHighQuantity(num, highQuantity);
	this->header.SetImageDescriptor(num, desc);
	this->header.SetBitDepth(num, bitDepth);

	// determine if increases element count
	this->header.CalculateNumberOfElements();
}


// the data is processed so write it straight through
// argument count is total size in bytes of the passed data
bool cineon::Writer::WriteElement(const int element, void *data, const long count)
{
	// make sure the range is good
	if (element < 0 || element >= CINEON_MAX_ELEMENTS)
		return false;

	// make sure the entry is valid
	if (this->header.ImageDescriptor(element) == kUndefinedDescriptor)
		return false;

	// update file ptr
	//this->header.SetDataOffset(element, this->fileLoc);
	this->fileLoc += count;

	// write
	return (this->fd->Write(data, count) > 0);
}



bool cineon::Writer::WriteElement(const int element, void *data)
{
	// make sure the range is good
	if (element < 0 || element >= CINEON_MAX_ELEMENTS)
		return false;

	// make sure the entry is valid
	if (this->header.ImageDescriptor(element) == kUndefinedDescriptor)
		return false;

	return this->WriteElement(element, data, this->header.ComponentDataSize(element));
}



bool cineon::Writer::WriteElement(const int element, void *data, const DataSize size)
{
	bool status = true;

	// make sure the range is good
	if (element < 0 || element >= CINEON_MAX_ELEMENTS)
		return false;

	// make sure the entry is valid
	if (this->header.ImageDescriptor(element) == kUndefinedDescriptor)
		return false;

	// mark location in headers
	if (element == 0)
		this->header.SetImageOffset(this->fileLoc);
	//this->header.SetDataOffset(element, this->fileLoc);

	// reverse the order of the components
	bool reverse = false;

	// image parameters
	const U32 eolnPad = this->header.EndOfLinePadding();
	const U32 eoimPad = this->header.EndOfImagePadding();
	const U8 bitDepth = this->header.BitDepth(element);
	const U32 width = this->header.Width();
	const U32 height = this->header.Height();
	const int noc = this->header.NumberOfElements();
	const Packing packing = this->header.ImagePacking();

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
		memset(blank, bsize, sizeof(char));
	}

	// can we write the entire memory chunk at once without any additional processing
	if ((bitDepth == 8 && size == cineon::kByte) ||
		 (bitDepth == 12 && size == cineon::kWord /*&& packing == kFilledMethodA*/) ||
		 (bitDepth == 16 && size == cineon::kWord))
	{
		status = this->WriteThrough(data, width, height, noc, bytes, eolnPad, eoimPad, blank);
        delete [] blank;
		return status;
	}
	else
	{
		switch (bitDepth)
		{
		case 8:
			if (size == cineon::kByte)
				this->fileLoc += WriteBuffer<U8, 8, true>(this->fd, size, data, width, height, noc, packing, reverse, eolnPad, blank, status);
			else
				this->fileLoc += WriteBuffer<U8, 8, false>(this->fd, size, data, width, height, noc, packing, reverse, eolnPad, blank, status);
			break;

		case 10:
			// are the channels stored in reverse
			/*if (this->header.ImageDescriptor(element) == kRGB && this->header.DatumSwap(element) && bitDepth == 10)
				reverse = true;*/

			if (size == cineon::kWord)
				this->fileLoc += WriteBuffer<U16, 10, true>(this->fd, size, data, width, height, noc, packing, reverse, eolnPad, blank, status);
			else
				this->fileLoc += WriteBuffer<U16, 10, false>(this->fd, size, data, width, height, noc, packing, reverse, eolnPad, blank, status);
			break;

		case 12:
			if (size == cineon::kWord)
				this->fileLoc += WriteBuffer<U16, 12, true>(this->fd, size, data, width, height, noc, packing, reverse, eolnPad, blank, status);
			else
				this->fileLoc += WriteBuffer<U16, 12, false>(this->fd, size, data, width, height, noc, packing, reverse, eolnPad, blank, status);
			break;

		case 16:
			if (size == cineon::kWord)
				this->fileLoc += WriteBuffer<U16, 16, true>(this->fd, size, data, width, height, noc, packing, reverse, eolnPad, blank, status);
			else
				this->fileLoc += WriteBuffer<U16, 16, false>(this->fd, size, data, width, height, noc, packing, reverse, eolnPad, blank, status);
			break;

        default:
            delete [] blank;
            return false;
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
    delete [] blank;

	return status;
}


// the passed in image buffer is written to the file untouched

bool cineon::Writer::WriteThrough(void *data, const U32 width, const U32 height, const int noc, const int bytes, const U32 eolnPad, const U32 eoimPad, char *blank)
{
	bool status = true;
	const size_t count = size_t(width) * size_t(height) * noc;
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




bool cineon::Writer::Finish()
{
	// write the file size in the header
	this->header.SetFileSize(this->fileLoc);

	// rewrite all of the offsets in the header
	return this->header.WriteOffsetData(this->fd);
}




