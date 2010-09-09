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

#include <cstdio>
#include <cstring>
#include <ctime>

#include "DPX.h"
#include "EndianSwap.h"
#include "ReaderInternal.h"
#include "ElementReadStream.h"
#include "Codec.h"
#include "RunLengthEncoding.h"



dpx::Reader::Reader() : fd(0), rio(0)
{
	// initialize all of the Codec* to NULL
	for (int i = 0; i < MAX_ELEMENTS; i++)
		this->codex[i] = 0;
}


dpx::Reader::~Reader()
{
	this->Reset();
}	


void dpx::Reader::Reset()
{
	// delete all of the Codec * entries	
	for (int i = 0; i < MAX_ELEMENTS; i++)
		if (this->codex[i])
		{
			delete codex[i];
			this->codex[i] = 0;
		}

	// Element Reader
	if (this->rio)
	{
		delete rio;
		this->rio = 0;
	}
	if (this->fd)
		this->rio = new ElementReadStream(this->fd);
}


void dpx::Reader::SetInStream(InStream *fd)
{
	this->fd = fd;
	this->Reset();
}


bool dpx::Reader::ReadHeader()
{
	return this->header.Read(this->fd);
}


bool dpx::Reader::ReadImage(const int element, void *data)
{
	// make sure the range is good
	if (element < 0 || element >= MAX_ELEMENTS)
		return false;

	// make sure the entry is valid
	if (this->header.ImageDescriptor(element) == kUndefinedDescriptor)
		return false;

	return this->ReadImage(data, this->header.ComponentDataSize(element), this->header.ImageDescriptor(element));
}


bool dpx::Reader::ReadImage(void *data, const DataSize size, const Descriptor desc)
{
	Block block(0, 0, this->header.Width()-1, this->header.Height()-1);
	return this->ReadBlock(data, size, block, desc);
}



/**
	block - this contains the square block of data to read in.  The data elements in this
		structure need to be normalized Left to Right, Top to Bottom.
  */
  

bool dpx::Reader::ReadBlock(const int element, unsigned char *data, Block &block)
{
	// make sure the range is good
	if (element < 0 || element >= MAX_ELEMENTS)
		return false;

	// make sure the entry is valid
	if (this->header.ImageDescriptor(element) == kUndefinedDescriptor)
		return false;

	return this->ReadBlock(data, this->header.ComponentDataSize(element), block, this->header.ImageDescriptor(element));
}
  

/* 
	implementation notes:

	dpx::readBlock reads in the image starting from the beginning of the channel.  This can
	be optimized if the image isn't encoded; we can skip forward in the file to close to the
	start of (block.x1, block.y1) and determine exactly which bit will be the start.  This
	certainly will save some time for cases where we are only reading ROIs (regions of interest).

*/

bool dpx::Reader::ReadBlock(void *data, const DataSize size, Block &block, const Descriptor desc)
{	
	int i;
	int element;

	// check the block coordinates
	block.Check();

	// determine which element we are viewing
	for (i = 0; i < MAX_ELEMENTS; i++) 
	{
		if (this->header.ImageDescriptor(i) == desc) 
		{
			element = i;
			break;
		}
	}
	if (i == MAX_ELEMENTS)					// was it found?
		return false;
	
				
	// get the number of components for this element descriptor
	const int numberOfComponents = this->header.ImageElementComponentCount(element);

	// bit depth of the image element
	const int bitDepth = this->header.BitDepth(element);

	// rle encoding?
	const bool rle = (this->header.ImageEncoding(element) == kRLE);


	// lets see if this can be done in a single fast read
	if (!rle && this->header.EndOfLinePadding(element) == 0 &&
		((bitDepth == 8 && size == dpx::kByte) || 
		 (bitDepth == 16 && size == dpx::kWord) ||
		 (bitDepth == 32 && size == dpx::kFloat) ||
		 (bitDepth == 64 && size == dpx::kDouble)) &&
		block.x1 == 0 && block.x2 == (int)(this->header.Width()-1))
	{
		// seek to the beginning of the image block
		if (this->fd->Seek((this->header.DataOffset(element) + (block.y1 * this->header.Width() * (bitDepth / 8) * numberOfComponents)), InStream::kStart) == false)
			return false;

		// size of the image
		const size_t imageSize = this->header.Width() * (block.y2 - block.y1 + 1) * numberOfComponents;
		const size_t imageByteSize = imageSize * bitDepth / 8;

		size_t rs = this->fd->ReadDirect(data, imageByteSize);
		if (rs != imageByteSize)
			return false;
			
		// swap the bytes if different byte order	
		if (this->header.RequiresByteSwap())
			dpx::EndianSwapImageBuffer(size, data, imageSize);
	
		return true;
	}

	
	// determine if the encoding system is loaded
	if (this->codex[element] == 0)
	{
		// this element reader has not been used
		if (rle)
			// TODO
			//this->codex[element] = new RunLengthEncoding;
			return false;
		else
			this->codex[element] = new Codec;
	}

	// read the image block
	return this->codex[element]->Read(this->header, this->rio, element, block, data, size);
}	



bool dpx::Reader::ReadUserData(unsigned char *data)
{
	// check to make sure there is some user data
	if (this->header.UserSize() == 0)
		return true;
		
	// seek to the beginning of the user data block
	if (this->fd->Seek(sizeof(GenericHeader) + sizeof(IndustryHeader), InStream::kStart) == false)
		return false;

	size_t rs = this->fd->ReadDirect(data, this->header.UserSize());
	if (rs != this->header.UserSize())
		return false;
	
	return true;
}



