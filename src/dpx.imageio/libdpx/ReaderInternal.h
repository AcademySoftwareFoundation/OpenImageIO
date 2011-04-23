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


#ifndef _DPX_READERINTERNAL_H
#define _DPX_READERINTERNAL_H 1


#include <algorithm>
#include "BaseTypeConverter.h"


#define PADDINGBITS_10BITFILLEDMETHODA	2
#define PADDINGBITS_10BITFILLEDMETHODB	0

#define MASK_10BITPACKED				0xffc0
#define MULTIPLIER_10BITPACKED			2
#define REMAIN_10BITPACKED				4
#define	REVERSE_10BITPACKED				6

#define MASK_12BITPACKED				0xfff0
#define MULTIPLIER_12BITPACKED			4
#define REMAIN_12BITPACKED				2
#define	REVERSE_12BITPACKED				4




namespace dpx 
{

	// this function is called when the DataSize is 10 bit and the packing method is kFilledMethodA or kFilledMethodB
	template<typename BUF, int PADDINGBITS>
	void Unfill10bitFilled(U32 *readBuf, const int x, BUF *data, int count, int bufoff, const int numberOfComponents)
	{
		// unpack the words in the buffer
		BUF *obuf = data + bufoff;
		
		int index = (x * sizeof(U32)) % numberOfComponents;
		
		for (int i = count - 1; i >= 0; i--)
		{
			// unpacking the buffer backwords
			register U32 word = readBuf[(i + index) / 3 / sizeof(U32)];
			register U16 d1 = U16(word >> ((2 - (i + index) % 3) * 10 + PADDINGBITS) & 0x3ff);
			BaseTypeConvertU10ToU16(d1, d1);
			BaseTypeConverter(d1, obuf[i]);
		}
#if 0
		// NOTE: REVERSE -- is this something we really need to handle?
		// There were many dpx images that write the components backwords
		// because of some confusion with DPX v1 spec
		
		switch (dpxHeader.DatumSwap(element))
		{
			case 0:			// no swap
				for (i = count - 1; i >= 0; i--)
				{
					U32 word = readBuf[(i + index) / 3 / sizeof(U32)];
					U16 d1 = U16(word >> (((i + index) % 3) * 10 + PADDINGBITS) & 0x3ff);
					BaseTypeConvertU10ToU16(d1, d1);
					BaseTypeConverter(d1, obuf[i]);
				}
				
			case 1:			// swap the three datum around so BGR becomes RGB
				for (i = count - 1; i >= 0; i--)
				{
					// unpacking the buffer backwords
					U32 word = readBuf[(i + index) / 3 / sizeof(U32)];
					U16 d1 = U16(word >> ((2 - (i + index) % 3) * 10 + PADDINGBITS) & 0x3ff);
					BaseTypeConvertU10ToU16(d1, d1);
					BaseTypeConverter(d1, obuf[i]);
				}
				
				// NOTE: NOT DONE case 2
			case 2:			// swap the second two of three datum around so YCrCb becomes YCbCr
				for (i = count - 1; i >= 0; i--)
				{
					// unpacking the buffer backwords
					U32 word = readBuf[(i + index) / 3 / sizeof(U32)];
					U16 d1 = U16(word >> ((2 - (count + index) % 3) * 10 + PADDINGBITS) & 0x3ff);
					BaseTypeConvertU10ToU16(d1, d1);
					BaseTypeConverter(d1, obuf[i]);
				}
				
		}
#endif		
	}
	
	template <typename IR, typename BUF, int PADDINGBITS>
	bool Read10bitFilled(const Header &dpxHeader, U32 *readBuf, IR *fd, const int element, const Block &block, BUF *data)
	{
		// image height to read
		const int height = block.y2 - block.y1 + 1;

		// get the number of components for this element descriptor
		const int numberOfComponents = dpxHeader.ImageElementComponentCount(element);
	
		// end of line padding
		int eolnPad = dpxHeader.EndOfLinePadding(element);
		
		// number of datums in one row
		int datums = dpxHeader.Width() * numberOfComponents;
		
		// read in each line at a time directly into the user memory space
		for (int line = 0; line < height; line++)
		{
			// determine offset into image element

			int actline = line + block.y1;

			// first get line offset
			long offset = actline * datums;
			
			// add in the accumulated round-up offset - the following magical formula is
			// just an unrolling of a loop that does the same work in constant time:
			// for (int i = 1; i <= actline; ++i)
			//     offset += (i * datums) % 3;
			offset += datums % 3 * ((actline + 2) / 3) + (3 - datums % 3) % 3 * ((actline + 1) / 3);
			
			// round up to the 32-bit boundary
			offset = offset / 3 * 4;
			
			// add in eoln padding
			offset += line * eolnPad;
			
			// add in offset within the current line, rounding down so to catch any components within the word
			offset += block.x1 * numberOfComponents / 3 * 4;
			
			
			// get the read count in bytes, round to the 32-bit boundry
			int readSize = (block.x2 - block.x1 + 1) * numberOfComponents;
			readSize += readSize % 3;
			readSize = readSize / 3 * 4;
			
			// determine buffer offset
			int bufoff = line * datums;
	
			fd->Read(dpxHeader, element, offset, readBuf, readSize);
	
			// unpack the words in the buffer
#if RLE_WORKING			
			int count = (block.x2 - block.x1 + 1) * numberOfComponents;
			Unfill10bitFilled<BUF, PADDINGBITS>(readBuf, block.x1, data, count, bufoff, numberOfComponents);
#else					
			BUF *obuf = data + bufoff;
			int index = (block.x1 * sizeof(U32)) % numberOfComponents;

			for (int count = (block.x2 - block.x1 + 1) * numberOfComponents - 1; count >= 0; count--)
			{
				// unpacking the buffer backwords
				U16 d1 = U16(readBuf[(count + index) / 3] >> ((2 - (count + index) % 3) * 10 + PADDINGBITS) & 0x3ff);
				BaseTypeConvertU10ToU16(d1, d1);

				BaseTypeConverter(d1, obuf[count]);

				// work-around for 1-channel DPX images - to swap the outlying pixels, otherwise the columns are in the wrong order
				if (numberOfComponents == 1 && count % 3 == 0)
					std::swap(obuf[count], obuf[count + 2]);
			}
#endif		
		}
	
		return true;
	}


	template <typename IR, typename BUF>
	bool Read10bitFilledMethodA(const Header &dpx, U32 *readBuf, IR *fd, const int element, const Block &block, BUF *data)
	{
		// padding bits for PackedMethodA is 2
		return Read10bitFilled<IR, BUF, PADDINGBITS_10BITFILLEDMETHODA>(dpx, readBuf, fd, element, block, data);
	}


	template <typename IR, typename BUF>
	bool Read10bitFilledMethodB(const Header &dpx, U32 *readBuf, IR *fd, const int element, const Block &block, BUF *data)
	{
		return Read10bitFilled<IR, BUF, PADDINGBITS_10BITFILLEDMETHODB>(dpx, readBuf, fd, element, block, data);
	}


	// 10 bit, packed data
	// 12 bit, packed data
	template <typename BUF, U32 MASK, int MULTIPLIER, int REMAIN, int REVERSE>
	void UnPackPacked(U32 *readBuf, const int bitDepth, BUF *data, int count, int bufoff)
	{
		// unpack the words in the buffer
		BUF *obuf = data + bufoff;
				
		for (int i = count - 1; i >= 0; i--)
		{
			// unpacking the buffer backwords
			// find the byte that the data starts in, read in as a 16 bits then shift and mask
			// the pattern with byte offset is:
			//	10 bits datasize rotates every 4 data elements
			//		element 0 -> 6 bit shift to normalize at MSB (10 LSB shifted 6 bits)
			//		element 1 -> 4 bit shift to normalize at MSB
			//		element 2 -> 2 bit shift to normalize at MSB
			//		element 3 -> 0 bit shift to normalize at MSB
			//  10 bit algorithm: (6-((count % 4)*2))
			//      the pattern repeats every 160 bits
			//	12 bits datasize rotates every 2 data elements
			//		element 0 -> 4 bit shift to normalize at MSB
			//		element 1 -> 0 bit shift to normalize at MSB
			//  12 bit algorithm: (4-((count % 2)*4))
			//      the pattern repeats every 96 bits
			
			// first determine the word that the data element completely resides in
			U16 *d1 = reinterpret_cast<U16 *>(reinterpret_cast<U8 *>(readBuf)+((i * bitDepth) / 8 /*bits*/));
			
			// place the component in the MSB and mask it for both 10-bit and 12-bit
			U16 d2 = (*d1 << (REVERSE - ((i % REMAIN) * MULTIPLIER))) & MASK;
			
			// For the 10/12 bit cases, specialize the 16-bit conversion by
			// repacking into the LSB and using a specialized conversion
			if(bitDepth == 10)
			{
				d2 = d2 >> REVERSE;
				BaseTypeConvertU10ToU16(d2, d2);
			}
			else if(bitDepth == 12)
			{
				d2 = d2 >> REVERSE;
				BaseTypeConvertU12ToU16(d2, d2);
			}
			
			BaseTypeConverter(d2, obuf[i]);
		}		
	}

	
	template <typename IR, typename BUF, U32 MASK, int MULTIPLIER, int REMAIN, int REVERSE>
	bool ReadPacked(const Header &dpxHeader, U32 *readBuf, IR *fd, const int element, const Block &block, BUF *data)
	{	
		// image height to read
		const int height = block.y2 - block.y1 + 1;

		// get the number of components for this element descriptor
		const int numberOfComponents = dpxHeader.ImageElementComponentCount(element);

		// end of line padding
		int eolnPad = dpxHeader.EndOfLinePadding(element);

		// data size in bits
		const int dataSize = dpxHeader.BitDepth(element);

		// number of bytes 
		const int lineSize = (dpxHeader.Width() * numberOfComponents * dataSize + 31) / 32;

		// read in each line at a time directly into the user memory space
		for (int line = 0; line < height; line++)
		{
			// determine offset into image element
			long offset = (line + block.y1) * (lineSize * sizeof(U32)) +
						(block.x1 * numberOfComponents * dataSize / 32 * sizeof(U32)) + (line * eolnPad);
	
			// calculate read size
			int readSize = ((block.x2 - block.x1 + 1) * numberOfComponents * dataSize);
			readSize += (block.x1 * numberOfComponents * dataSize % 32);			// add the bits left over from the beginning of the line
			readSize = ((readSize + 31) / 32) * sizeof(U32);

			// calculate buffer offset
			int bufoff = line * dpxHeader.Width() * numberOfComponents;
	
			fd->Read(dpxHeader, element, offset, readBuf, readSize);

			// unpack the words in the buffer
			int count = (block.x2 - block.x1 + 1) * numberOfComponents;
			UnPackPacked<BUF, MASK, MULTIPLIER, REMAIN, REVERSE>(readBuf, dataSize, data, count, bufoff);
		}

		return true;
	}
	
	
	template <typename IR, typename BUF>
	bool Read10bitPacked(const Header &dpxHeader, U32 *readBuf, IR *fd, const int element, const Block &block, BUF *data)
	{
		return ReadPacked<IR, BUF, MASK_10BITPACKED, MULTIPLIER_10BITPACKED, REMAIN_10BITPACKED, REVERSE_10BITPACKED>(dpxHeader, readBuf, fd, element, block, data);
		
	}
	
	template <typename IR, typename BUF>
	bool Read12bitPacked(const Header &dpxHeader, U32 *readBuf, IR *fd, const int element, const Block &block, BUF *data)
	{
		return ReadPacked<IR, BUF, MASK_12BITPACKED, MULTIPLIER_12BITPACKED, REMAIN_12BITPACKED, REVERSE_12BITPACKED>(dpxHeader, readBuf, fd, element, block, data);
	}


	template <typename IR, typename SRC, DataSize SRCTYPE, typename BUF, DataSize BUFTYPE>
	bool ReadBlockTypes(const Header &dpxHeader, SRC *readBuf, IR *fd, const int element, const Block &block, BUF *data)
	{
		// get the number of components for this element descriptor
		const int numberOfComponents = dpxHeader.ImageElementComponentCount(element);
		
		// byte count component type
		const int bytes = dpxHeader.ComponentByteCount(element);
			
		// image image/height to read
		const int width = (block.x2 - block.x1 + 1) * numberOfComponents;
		const int height = block.y2 - block.y1 + 1;
		
		// end of line padding
		int eolnPad = dpxHeader.EndOfLinePadding(element);
		if (eolnPad == ~0)
			eolnPad = 0;

		// image width
		const int imageWidth = dpxHeader.Width();
		
		// read in each line at a time directly into the user memory space
		for (int line = 0; line < height; line++)
		{
			
			// determine offset into image element
			long offset = (line + block.y1) * imageWidth * numberOfComponents * bytes +
						block.x1 * numberOfComponents * bytes + (line * eolnPad);
						
			if (BUFTYPE == SRCTYPE)
			{
				fd->ReadDirect(dpxHeader, element, offset, reinterpret_cast<unsigned char *>(data + (width*line)), width*bytes);
			}
			else
			{
				fd->Read(dpxHeader, element, offset, readBuf, width*bytes);
							
				// convert data		
				for (int i = 0; i < width; i++)
					BaseTypeConverter(readBuf[i], data[width*line+i]);
			}
	
		}

		return true;
	}


	template <typename IR, typename BUF>
	bool Read12bitFilledMethodB(const Header &dpxHeader, U16 *readBuf, IR *fd, const int element, const Block &block, BUF *data)
	{
		// get the number of components for this element descriptor
		const int numberOfComponents = dpxHeader.ImageElementComponentCount(element);

		// image width & height to read
		const int width = (block.x2 - block.x1 + 1) * numberOfComponents;
		const int height = block.y2 - block.y1 + 1;
		
		// width of image
		const int imageWidth = dpxHeader.Width();
	
		// end of line padding (not a required data element so check for ~0)
		int eolnPad = dpxHeader.EndOfLinePadding(element);
		if (eolnPad == ~0)
			eolnPad = 0;
				
		// read in each line at a time directly into the user memory space
		for (int line = 0; line < height; line++)
		{
			// determine offset into image element
			long offset = (line + block.y1) * imageWidth * numberOfComponents * 2 +
						block.x1 * numberOfComponents * 2 + (line * eolnPad);
	
			fd->Read(dpxHeader, element, offset, readBuf, width*2);
				
			// convert data		
			for (int i = 0; i < width; i++)
			{
				U16 d1 = readBuf[i];
				BaseTypeConvertU12ToU16(d1, d1);
				BaseTypeConverter(d1, data[width*line+i]);
			}
		}

		return true;
	}

#ifdef RLE_WORKING
	template <typename BUF, DataSize BUFTYPE>
	void ProcessImageBlock(const Header &dpxHeader,  const int element, U32 *readBuf, const int x, BUF *data, const int bufoff)
	{
		const int bitDepth = dpxHeader.BitDepth(element);
		const int numberOfComponents = dpxHeader.ImageElementComponentCount(element);
		const Packing packing = dpxHeader.ImagePacking(element);

		if (bitDepth == 10)
		{	
			if (packing == kFilledMethodA)
				Read10bitFilledMethodA<IR, BUF>(dpxHeader, readBuf, fd, element, block, reinterpret_cast<BUF *>(data));
				Unfill10bitFilled<BUF, PADDINGBITS_10BITFILLEDMETHODA>(readBuf, x, data, count, bufoff, numberOfComponents);
			else if (packing == kFilledMethodB)
				Read10bitFilledMethodB<IR, BUF>(dpxHeader, readBuf, fd, element, block, reinterpret_cast<BUF *>(data));
			else if (packing == kPacked)
				Read10bitPacked<IR, BUF>(dpxHeader, readBuf, fd, element, block, reinterpret_cast<BUF *>(data));
				UnPackPacked<BUF, MASK_10BITPACKED, MULTIPLIER_10BITPACKED, REMAIN_10BITPACKED, REVERSE_10BITPACKED>(readBuf, dataSize, data, count, bufoff);
		} 
		else if (bitDepth == 12)
		{			
			if (packing == kPacked)
				Read12bitPacked<IR, BUF>(dpxHeader, readBuf, fd, element, block, reinterpret_cast<BUF *>(data));
			else if (packing == kFilledMethodB)
				Read12bitFilledMethodB<IR, BUF>(dpxHeader, reinterpret_cast<U16 *>(readBuf), fd, element, block, reinterpret_cast<BUF *>(data));
		}
	}
#endif
	
	template <typename IR, typename BUF, DataSize BUFTYPE>
	bool ReadImageBlock(const Header &dpxHeader, U32 *readBuf, IR *fd, const int element, const Block &block, BUF *data)
	{
		const int bitDepth = dpxHeader.BitDepth(element);
		const DataSize size = dpxHeader.ComponentDataSize(element);	
		const Packing packing = dpxHeader.ImagePacking(element);
			
		if (bitDepth == 10)
		{	
			if (packing == kFilledMethodA)
				return Read10bitFilledMethodA<IR, BUF>(dpxHeader, readBuf, fd, element, block, reinterpret_cast<BUF *>(data));	
			else if (packing == kFilledMethodB)
				return Read10bitFilledMethodB<IR, BUF>(dpxHeader, readBuf, fd, element, block, reinterpret_cast<BUF *>(data));
			else if (packing == kPacked)
				return Read10bitPacked<IR, BUF>(dpxHeader, readBuf, fd, element, block, reinterpret_cast<BUF *>(data));
		} 
		else if (bitDepth == 12)
		{			
			if (packing == kPacked)
				return Read12bitPacked<IR, BUF>(dpxHeader, readBuf, fd, element, block, reinterpret_cast<BUF *>(data));
			else if (packing == kFilledMethodB)
				// filled method B
				// 12 bits fill LSB of 16 bits
				return Read12bitFilledMethodB<IR, BUF>(dpxHeader, reinterpret_cast<U16 *>(readBuf), fd, element, block, reinterpret_cast<BUF *>(data));
			else	
				// filled method A
				// 12 bits fill MSB of 16 bits
				return ReadBlockTypes<IR, U16, kWord, BUF, BUFTYPE>(dpxHeader, reinterpret_cast<U16 *>(readBuf), fd, element, block, reinterpret_cast<BUF *>(data));
		}
		else if (size == dpx::kByte)
			return ReadBlockTypes<IR, U8, kByte, BUF, BUFTYPE>(dpxHeader, reinterpret_cast<U8 *>(readBuf), fd, element, block, reinterpret_cast<BUF *>(data));
		else if (size == dpx::kWord)
			return ReadBlockTypes<IR, U16, kWord, BUF, BUFTYPE>(dpxHeader, reinterpret_cast<U16 *>(readBuf), fd, element, block, reinterpret_cast<BUF *>(data));
		else if (size == dpx::kInt)
			return ReadBlockTypes<IR, U32, kInt, BUF, BUFTYPE>(dpxHeader, reinterpret_cast<U32 *>(readBuf), fd, element, block, reinterpret_cast<BUF *>(data));
		else if (size == dpx::kFloat)
			return ReadBlockTypes<IR, R32, kFloat, BUF, BUFTYPE>(dpxHeader, reinterpret_cast<R32 *>(readBuf), fd, element, block, reinterpret_cast<BUF *>(data));
		else if (size == dpx::kDouble)
			return ReadBlockTypes<IR, R64, kDouble, BUF, BUFTYPE>(dpxHeader, reinterpret_cast<R64 *>(readBuf), fd, element, block, reinterpret_cast<BUF *>(data));

		// should not reach here
		return false;
	}

	template <typename IR>
	bool ReadImageBlock(const Header &dpxHeader, U32 *readBuf, IR *fd, const int element, const Block &block, void *data, const DataSize size)
	{
		if (size == dpx::kByte)
			return ReadImageBlock<IR, U8, dpx::kByte>(dpxHeader, readBuf, fd, element, block, reinterpret_cast<U8 *>(data));
		else if (size == dpx::kWord)
			return ReadImageBlock<IR, U16, dpx::kWord>(dpxHeader, readBuf, fd, element, block, reinterpret_cast<U16 *>(data));
		else if (size == dpx::kInt)
			return ReadImageBlock<IR, U32, dpx::kInt>(dpxHeader, readBuf, fd, element, block, reinterpret_cast<U32 *>(data));
		else if (size == dpx::kFloat)
			return ReadImageBlock<IR, R32, dpx::kFloat>(dpxHeader, readBuf, fd, element, block, reinterpret_cast<R32 *>(data));	
		else if (size == dpx::kDouble)
			return ReadImageBlock<IR, R64, dpx::kDouble>(dpxHeader, readBuf, fd, element, block, reinterpret_cast<R64 *>(data));

		// should not reach here
		return false;
	}


#ifdef RLE_WORKING
	// THIS IS PART OF THE INCOMPLETE RLE CODE
	
	// src is full image without any eoln padding
	template <typename SRC, typename DST>
	void CopyImageBlock(const Header &dpxHeader, const int element, SRC *src, DataSize srcSize, DST *dst, DataSize dstSize, const Block &block)
	{
		const int numberOfComponents = dpxHeader.ImageElementComponentCount(element);
		const int width = dpxHeader.Width();
		const int byteCount = dpxHeader.ComponentByteCount(element);
		const int pixelByteCount = numberOfComponents * byteCount;

		int srcoff, dstoff;
		int x, y, nc;
		
		if (srcSize == dstSize)
		{
			int lineSize = (width * numberOfComponents * byteCount);
			U8 * srcU8 = reinterpret_cast<U8 *>(src);
			U8 * dstU8 = reinterpret_cast<U8 *>(dst);
			for (y = block.y1; y <= block.y2; y++)
			{
				int copySize = (block.x2 - block.x1 + 1) * numberOfComponents * byteCount;
				memcpy(srcU8 + (y * lineSize) + (block.x1 * numberOfComponents * byteCount), dstU8, copySize);
				outBuf += copySize;
			}
			return;
		}
		

		for (y = block.y1; y <= block.y2; y++)
		{
			dstoff = (y - block.y1) * ((block.x2 - block.x1 + 1) * numberOfComponents) - block.x1;
			for (x = block.x1; x <= block.x2; x++)
			{
				for (nc = 0; nc < numberOfComponents; nc++)
				{
					SRC d1 = src[(y * width * numberOfComponents) + (x * numberOfComponents) + nc];
					BaseTypeConverter(d1, dst[dstoff+((x-block.x1)*numberOfComponents) + nc]);
				}	
			}
		}
	}
	
	
	template<typename SRC>
	void CopyImageBlock(const Header &dpxHeader, const int element, SRC *src, DataSize srcSize, void *dst, DataSize dstSize, const Block &block)
	{
		if (dstSize == dpx::kByte)
			CopyImageBlock<SRC, U8>(dpxHeader, element, src, srcSize, reinterpret_cast<U8 *>(dst), dstSize, block);
		else if (dstSize == dpx::kWord)
			CopyImageBlock<SRC, U16>(dpxHeader, element, src, srcSize, reinterpret_cast<U16 *>(dst), dstSize, block);
		else if (dstSize == dpx::kInt)
			CopyImageBlock<SRC, U32>(dpxHeader, element, src, srcSize, reinterpret_cast<U32 *>(dst), dstSize, block);
		else if (dstSize == dpx::kFloat)
			CopyImageBlock<SRC, R32>(dpxHeader, element, src, srcSize, reinterpret_cast<R32 *>(dst), dstSize, block);
		else if (dstSize == dpx::kDouble)
			CopyImageBlock<SRC, R64>(dpxHeader, element, src, srcSize, reinterpret_cast<R64 *>(dst), dstSize, block);	
		
	}

	
	void CopyImageBlock(const Header &dpxHeader, const int element, void *src, DataSize srcSize, void *dst, DataSize dstSize, const Block &block)
	{
		if (srcSize == dpx::kByte)
			CopyImageBlock<U8, dpx::kByte>(dpxHeader, element, reinterpret_cast<U8 *>(src), srcSize, dst, dstSize, block);
		else if (srcSize == dpx::kWord)
			CopyImageBlock<U16, dpx::kWord>(dpxHeader, element, reinterpret_cast<U16 *>(src), srcSize, dst, dstSize, block);
		else if (srcSize == dpx::kInt)
			CopyImageBlock<U32, dpx::kInt>(dpxHeader, element, reinterpret_cast<U32 *>(src), srcSize, dst, dstSize, block);
		else if (srcSize == dpx::kFloat)
			CopyImageBlock<R32, dpx::kFloat>(dpxHeader, element, reinterpret_cast<R32 *>(src), srcSize, dst, dstSize, block);
		else if (srcSize == dpx::kDouble)
			CopyImageBlock<R64, dpx::kDouble>(dpxHeader, element, reinterpret_cast<R64 *>(src), srcSize, dst, dstSize, block);	
		
	}
#endif
	
}


#endif


