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
#include "RunLengthEncoding.h"
#include "ElementReadStream.h"
#include "ReaderInternal.h"

#if defined(__GNUC__)
#    pragma GCC diagnostic ignored "-Wunused-parameter"
#endif


// Basic size of a packet is the number of bytes that all data packing methods will fit into that are whole and complete
#define PACKET_REPEAT			(10 * sizeof(U32))					// 320 bits repeating pattern
#define BUFFER_SIZE				(PACKET_REPEAT * 1002)				// read in temp buffer size
#define EXPANDED_BUFFER_SIZE	(BUFFER_SIZE + (BUFFER_SIZE / 3))	// expanded size after unpacking (max)


dpx::RunLengthEncoding::RunLengthEncoding() : buf(0)
{
}


dpx::RunLengthEncoding::~RunLengthEncoding()
{
	if (this->buf)
		delete [] buf;
}


void dpx::RunLengthEncoding::Reset()
{
	if (this->buf)
	{
		delete [] buf;
		this->buf = 0;
	}
}


bool dpx::RunLengthEncoding::Read(const Header &dpxHeader, ElementReadStream *fd, const int element, const Block &block, void *data, const DataSize size)
{
	int i;
	
	// just check to make sure that this element is RLE
	if (dpxHeader.ImageEncoding(element) != kRLE)
		return false;

	
	// get the number of components for this element descriptor
	const int numberOfComponents = dpxHeader.ImageElementComponentCount(element);
	const int width = dpxHeader.Width();
	const int height = dpxHeader.Height();
	const int byteCount = dpxHeader.ComponentByteCount(element);
	const U32 eolnPad = dpxHeader.EndOfLinePadding(element);
	//DataSize srcSize = dpxHeader.ComponentDataSize(element);
	
	// has the buffer been read in and decoded?
	if (this->buf == 0)
	{
		// not yet
				
		// bit depth of the image element
		const int bitDepth = dpxHeader.BitDepth(element);
	
		// error out if the bit depth 10 or 12 and have eoln bytes
		// this is particularly slow to parse and eoln padding bytes
		// are not needed for those formats
		// also if end of padding makes the line length odd, error out for that as well	
		if (bitDepth != 8 && bitDepth != 16 && eolnPad > 0)
			return false;
		else if (bitDepth == 16 && eolnPad != 2 && eolnPad != 0)
			return false;
		
		// error out for real types since bit operations don't really make sense
		if (size == kFloat || size == kDouble)
			return false;
			
		// find start and possible end of the element
		U32 startOffset = dpxHeader.DataOffset(element);
		U32 endOffset = 0xffffffff;
		U32 currentOffset = startOffset;
		
		for (i = 0; i < MAX_ELEMENTS; i++)
		{
			if (i == element)
				continue;
			U32 doff = dpxHeader.DataOffset(i);
			if (doff == 0xffffffff)
				continue;
			if (doff > startOffset && (endOffset == 0xffffffff || doff < endOffset))
				endOffset = doff - 1;
		}


		// size of the image
		const size_t imageSize = size_t(width) * size_t(height) * numberOfComponents;
		const size_t imageByteSize = imageSize * byteCount;
		
		// allocate the buffer that will store the entire image
		this->buf = new U8[imageByteSize];

		// allocate the temporary buffer that will read in the encoded image
		U8 *tempBuf = new U8[EXPANDED_BUFFER_SIZE];
		
		// xpos, ypos in decoding
		/*int xpos = 0;
		int ypos = 0;*/
		
		// read in the encoded image block at a time
		bool done = false;
		while (!done)
		{
			// read in temp buffer
			size_t rs = fd->ReadDirect(dpxHeader, element, (currentOffset - startOffset), tempBuf, BUFFER_SIZE);
			currentOffset += rs;
			if (rs != BUFFER_SIZE)
				done = true;
			else if (endOffset != 0xffffffff && currentOffset >= endOffset)
				done = true;

			// if 10-bit, 12-bit, unpack or unfill
			
			// step through and decode
			
			
		}
		
		// no longer need temp buffer
		delete [] tempBuf;
	}


#ifdef RLE_WORKING
	// NOT COMPLETE YET
	// copy buffer
	CopyImageBlock(dpxHeader, element, buf, srcSize, data, size, dstSize, block);
#endif
    return true;
}	


