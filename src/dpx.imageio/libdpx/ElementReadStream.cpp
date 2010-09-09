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

#include "DPX.h"
#include "EndianSwap.h"
#include "ElementReadStream.h"



dpx::ElementReadStream::ElementReadStream(InStream *fd) : fd(fd) 
{
}


dpx::ElementReadStream::~ElementReadStream()
{
}


void dpx::ElementReadStream::Reset()
{
}


bool dpx::ElementReadStream::Read(const dpx::Header &dpxHeader, const int element, const long offset, void * buf, const size_t size)
{
	long position = dpxHeader.DataOffset(element) + offset;

	// seek to the memory position				 				 
	if (this->fd->Seek(position, InStream::kStart) == false)
		return false;
				
	// read in the data, calculate buffer offset
	if (this->fd->Read(buf, size) != size)
		return false;
	
	// swap the bytes if different byte order	
	this->EndianDataCheck(dpxHeader, element, buf, size);

	return true;
}


bool dpx::ElementReadStream::ReadDirect(const dpx::Header &dpxHeader, const int element, const long offset, void * buf, const size_t size)
{
	long position = dpxHeader.DataOffset(element) + offset;

	// seek to the memory position				 				 
	if (this->fd->Seek(position, InStream::kStart) == false)
		return false;
				
	// read in the data, calculate buffer offset
	if (this->fd->ReadDirect(buf, size) != size)
		return false;
	
	// swap the bytes if different byte order	
	this->EndianDataCheck(dpxHeader, element, buf, size);

	return true;
}



void dpx::ElementReadStream::EndianDataCheck(const dpx::Header &dpxHeader, const int element, void *buf, const size_t size)
{
	if (dpxHeader.RequiresByteSwap())
	{
		switch (dpxHeader.BitDepth(element))
		{
		case 8:
			break;
		case 12:
			if (dpxHeader.ImagePacking(element) == dpx::kPacked)
				dpx::EndianSwapImageBuffer<dpx::kInt>(buf, size / sizeof(U32));
			else
				dpx::EndianSwapImageBuffer<dpx::kWord>(buf, size / sizeof(U16));
			break;
		case 16:
			dpx::EndianSwapImageBuffer<dpx::kWord>(buf, size / sizeof(U16));
			break;
		default:		// 10-bit, 32-bit, 64-bit
			dpx::EndianSwapImageBuffer<dpx::kInt>(buf, size / sizeof(U32));
		}	
	}
}

