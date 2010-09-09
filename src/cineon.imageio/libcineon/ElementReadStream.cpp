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

#include "Cineon.h"
#include "EndianSwap.h"
#include "ElementReadStream.h"
#include <cassert>


cineon::ElementReadStream::ElementReadStream(InStream *fd) : fd(fd)
{
}


cineon::ElementReadStream::~ElementReadStream()
{
}


void cineon::ElementReadStream::Reset()
{
}


bool cineon::ElementReadStream::Read(const cineon::Header &dpxHeader, const long offset, void * buf, const size_t size)
{
	long position = dpxHeader.ImageOffset() + offset;

	// seek to the memory position
	if (this->fd->Seek(position, InStream::kStart) == false)
		return false;

	// read in the data, calculate buffer offset
	if (this->fd->Read(buf, size) != size)
		return false;

	// swap the bytes if different byte order
	this->EndianDataCheck(dpxHeader, buf, size);

	return true;
}


bool cineon::ElementReadStream::ReadDirect(const cineon::Header &dpxHeader, const long offset, void * buf, const size_t size)
{
	long position = dpxHeader.ImageOffset() + offset;

	// seek to the memory position
	if (this->fd->Seek(position, InStream::kStart) == false)
		return false;

	// read in the data, calculate buffer offset
	if (this->fd->ReadDirect(buf, size) != size)
		return false;

	// swap the bytes if different byte order
	this->EndianDataCheck(dpxHeader, buf, size);

	return true;
}



void cineon::ElementReadStream::EndianDataCheck(const cineon::Header &dpxHeader, void *buf, const size_t size)
{
	if (dpxHeader.RequiresByteSwap())
	{
		// FIXME!!!
		switch (dpxHeader.BitDepth(0))
		{
		case 8:
			break;
		case 12:
			if (dpxHeader.ImagePacking() == cineon::kPacked)
				cineon::EndianSwapImageBuffer<cineon::kInt>(buf, size / sizeof(U32));
			else
				cineon::EndianSwapImageBuffer<cineon::kWord>(buf, size / sizeof(U16));
			break;
		case 16:
			cineon::EndianSwapImageBuffer<cineon::kWord>(buf, size / sizeof(U16));
			break;
		default:		// 10-bit, 32-bit, 64-bit
			cineon::EndianSwapImageBuffer<cineon::kInt>(buf, size / sizeof(U32));
		}
	}
}

