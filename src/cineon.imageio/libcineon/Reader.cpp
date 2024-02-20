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

#include <cstdio>
#include <cstring>
#include <ctime>
#include <cassert>

#include "Cineon.h"
#include "EndianSwap.h"
#include "ReaderInternal.h"
#include "ElementReadStream.h"
#include "Codec.h"


cineon::Reader::Reader() : fd(0), rio(0)
{
	// initialize all of the Codec* to NULL
	this->codec = 0;
}


cineon::Reader::~Reader()
{
	this->Reset();
    delete this->rio;
}


void cineon::Reader::Reset()
{
	// delete all of the Codec * entries
	delete this->codec;
	this->codec = 0;

	// Element Reader
	if (this->rio)
	{
		delete rio;
		this->rio = 0;
	}
	if (this->fd)
		this->rio = new ElementReadStream(this->fd);
}


void cineon::Reader::SetInStream(InStream *fd)
{
	this->fd = fd;
	this->Reset();
}


bool cineon::Reader::ReadHeader()
{
	return this->header.Read(this->fd);
}


bool cineon::Reader::ReadImage(void *data, const DataSize size)
{
	Block block(0, 0, this->header.Width()-1, this->header.Height()-1);
	return this->ReadBlock(data, size, block);
}



/*
	implementation notes:

	cineon::readBlock reads in the image starting from the beginning of the channel.  This can
	be optimized if the image isn't encoded; we can skip forward in the file to close to the
	start of (block.x1, block.y1) and determine exactly which bit will be the start.  This
	certainly will save some time for cases where we are only reading ROIs (regions of interest).

*/

bool cineon::Reader::ReadBlock(void *data, const DataSize size, Block &block)
{
	int i;

	// check the block coordinates
	block.Check();

	// get the number of components for this element descriptor
	const int numberOfComponents = this->header.NumberOfElements();

	// check the widths and bit depths of the image elements
	bool consistentDepth = true;
	bool consistentWidth = true;
	const int bitDepth = this->header.BitDepth(0);
	const int width = this->header.PixelsPerLine(0);
	for (i = 1; i < numberOfComponents; i++) {
		if (this->header.BitDepth(i) != bitDepth) {
			consistentDepth = false;
			if (!consistentWidth)
				break;
		}
		if ((int)this->header.PixelsPerLine(i) != width) {
			consistentWidth = false;
			if (!consistentDepth)
				break;
		}
	}

	// lets see if this can be done in a single fast read
	if (consistentDepth && consistentWidth && this->header.EndOfLinePadding() == 0 &&
		((bitDepth == 8 && size == cineon::kByte) ||
		 (bitDepth == 16 && size == cineon::kWord) ||
		 (bitDepth == 32 && size == cineon::kInt) ||
		 (bitDepth == 64 && size == cineon::kLongLong)) &&
		block.x1 == 0 && block.x2 == (int)(this->header.Width()-1))
	{
		// seek to the beginning of the image block
		if (this->fd->Seek((this->header.ImageOffset() + (block.y1 * this->header.Width() * (bitDepth / 8) * numberOfComponents)), InStream::kStart) == false)
			return false;

		// size of the image
		const size_t imageSize = this->header.Width() * (block.y2 - block.y1 + 1) * numberOfComponents;
		const size_t imageByteSize = imageSize * bitDepth / 8;

		size_t rs = this->fd->ReadDirect(data, imageByteSize);
		if (rs != imageByteSize)
			return false;

		// swap the bytes if different byte order
		if (this->header.RequiresByteSwap())
			cineon::EndianSwapImageBuffer(size, data, imageSize);

		return true;
	}


	// determine if the encoding system is loaded
	if (this->codec == 0)
		// this element reader has not been used
		this->codec = new Codec;

	// read the image block
	return this->codec->Read(this->header, this->rio, block, data, size);
}



bool cineon::Reader::ReadUserData(unsigned char *data)
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



