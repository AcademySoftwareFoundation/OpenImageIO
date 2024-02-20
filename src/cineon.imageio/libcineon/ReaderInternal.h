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


#ifndef _CINEON_READERINTERNAL_H
#define _CINEON_READERINTERNAL_H 1


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




namespace cineon
{

	template <typename IR, typename BUF, int PADDINGBITS>
	bool Read10bitFilled(const Header &dpxHeader, U32 *readBuf, IR *fd, const Block &block, BUF *data)
	{
		// image height to read
		const int height = block.y2 - block.y1 + 1;

		// get the number of components for this element descriptor
		const int numberOfComponents = dpxHeader.NumberOfElements();

		// end of line padding
		int eolnPad = dpxHeader.EndOfLinePadding();

		// number of datums in one row
		int datums = dpxHeader.Width() * numberOfComponents;

		// Line length in bytes rounded to 32 bits boundary
		int lineLength = ((datums - 1) / 3 + 1) * 4;

		// read in each line at a time directly into the user memory space
		for (int line = 0; line < height; line++)
		{
			// determine offset into image element
			int actline = line + block.y1;

			// first get line offset
			long offset = actline * lineLength;

			// add in eoln padding
			offset += line * eolnPad;

			// add in offset within the current line, rounding down so to catch any components within the word
			offset += block.x1 * numberOfComponents / 3 * 4;


			// get the read count in bytes, round to the 32-bit boundary
			int readSize = (block.x2 - block.x1 + 1) * numberOfComponents;
			readSize += readSize % 3;
			readSize = readSize / 3 * 4;

			// determine buffer offset
			int bufoff = line * dpxHeader.Width() * numberOfComponents;

			fd->Read(dpxHeader, offset, readBuf, readSize);

			// unpack the words in the buffer
			BUF *obuf = data + bufoff;
			int index = (block.x1 * sizeof(U32)) % numberOfComponents;

			for (int count = (block.x2 - block.x1 + 1) * numberOfComponents - 1; count >= 0; count--)
			{
				// unpacking the buffer backwards
				U16 d1 = U16(readBuf[(count + index) / 3] >> ((2 - (count + index) % 3) * 10 + PADDINGBITS) & 0x3ff);
				BaseTypeConvertU10ToU16(d1, d1);
				BaseTypeConverter(d1, obuf[count]);
			}
		}

		return true;
	}


	template <typename IR, typename BUF>
	bool Read10bitFilledMethodA(const Header &dpx, U32 *readBuf, IR *fd, const Block &block, BUF *data)
	{
		// padding bits for PackedMethodA is 2
		return Read10bitFilled<IR, BUF, PADDINGBITS_10BITFILLEDMETHODA>(dpx, readBuf, fd, block, data);
	}


	template <typename IR, typename BUF>
	bool Read10bitFilledMethodB(const Header &dpx, U32 *readBuf, IR *fd, const Block &block, BUF *data)
	{
		return Read10bitFilled<IR, BUF, PADDINGBITS_10BITFILLEDMETHODB>(dpx, readBuf, fd, block, data);
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
			// unpacking the buffer backwards
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
	bool ReadPacked(const Header &dpxHeader, U32 *readBuf, IR *fd, const Block &block, BUF *data)
	{
		// image height to read
		const int height = block.y2 - block.y1 + 1;

		// get the number of components for this element descriptor
		const int numberOfComponents = dpxHeader.NumberOfElements();

		// end of line padding
		int eolnPad = dpxHeader.EndOfLinePadding();

		// data size in bits
		// FIXME!!!
		const int dataSize = dpxHeader.BitDepth(0);

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

			fd->Read(dpxHeader, offset, readBuf, readSize);

			// unpack the words in the buffer
			int count = (block.x2 - block.x1 + 1) * numberOfComponents;
			UnPackPacked<BUF, MASK, MULTIPLIER, REMAIN, REVERSE>(readBuf, dataSize, data, count, bufoff);
		}

		return true;
	}


	template <typename IR, typename BUF>
	bool Read10bitPacked(const Header &dpxHeader, U32 *readBuf, IR *fd, const Block &block, BUF *data)
	{
		return ReadPacked<IR, BUF, MASK_10BITPACKED, MULTIPLIER_10BITPACKED, REMAIN_10BITPACKED, REVERSE_10BITPACKED>(dpxHeader, readBuf, fd, block, data);

	}

	template <typename IR, typename BUF>
	bool Read12bitPacked(const Header &dpxHeader, U32 *readBuf, IR *fd, const Block &block, BUF *data)
	{
		return ReadPacked<IR, BUF, MASK_12BITPACKED, MULTIPLIER_12BITPACKED, REMAIN_12BITPACKED, REVERSE_12BITPACKED>(dpxHeader, readBuf, fd, block, data);
	}


	template <typename IR, typename SRC, DataSize SRCTYPE, typename BUF, DataSize BUFTYPE>
	bool ReadBlockTypes(const Header &dpxHeader, SRC *readBuf, IR *fd, const Block &block, BUF *data)
	{
		// get the number of components for this element descriptor
		const int numberOfComponents = dpxHeader.NumberOfElements();

		// byte count component type
		// FIXME!!!
		const int bytes = dpxHeader.ComponentByteCount(0);

		// image image/height to read
		const int width = (block.x2 - block.x1 + 1) * numberOfComponents;
		const int height = block.y2 - block.y1 + 1;

		// end of line padding
		int eolnPad = dpxHeader.EndOfLinePadding();
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
				fd->ReadDirect(dpxHeader, offset, reinterpret_cast<unsigned char *>(data + (width*line)), width*bytes);
			}
			else
			{
				fd->Read(dpxHeader, offset, readBuf, width*bytes);

				// convert data
				for (int i = 0; i < width; i++)
					BaseTypeConverter(readBuf[i], data[width*line+i]);
			}

		}

		return true;
	}


	template <typename IR, typename BUF>
	bool Read12bitFilledMethodB(const Header &dpxHeader, U16 *readBuf, IR *fd, const Block &block, BUF *data)
	{
		// get the number of components for this element descriptor
		const int numberOfComponents = dpxHeader.NumberOfElements();

		// image width & height to read
		const int width = (block.x2 - block.x1 + 1) * numberOfComponents;
		const int height = block.y2 - block.y1 + 1;

		// width of image
		const int imageWidth = dpxHeader.Width();

		// end of line padding (not a required data element so check for ~0)
		int eolnPad = dpxHeader.EndOfLinePadding();
		if (eolnPad == ~0)
			eolnPad = 0;

		// read in each line at a time directly into the user memory space
		for (int line = 0; line < height; line++)
		{
			// determine offset into image element
			long offset = (line + block.y1) * imageWidth * numberOfComponents * 2 +
						block.x1 * numberOfComponents * 2 + (line * eolnPad);

			fd->Read(dpxHeader, offset, readBuf, width*2);

			// convert data
			for (int i = 0; i < width; i++)
			{
				U16 d1 = readBuf[i] << 4;
				BaseTypeConverter(d1, data[width*line+i]);
			}
		}

		return true;
	}

	template <typename IR, typename BUF, DataSize BUFTYPE>
	bool ReadImageBlock(const Header &dpxHeader, U32 *readBuf, IR *fd, const Block &block, BUF *data)
	{
		// FIXME!!!
		const int bitDepth = dpxHeader.BitDepth(0);
		const DataSize size = dpxHeader.ComponentDataSize(0);
		const Packing packing = dpxHeader.ImagePacking();

		if (bitDepth == 10)
		{
			if (packing == kLongWordLeft)
				return Read10bitFilledMethodA<IR, BUF>(dpxHeader, readBuf, fd, block, reinterpret_cast<BUF *>(data));
			else if (packing == kLongWordRight)
				return Read10bitFilledMethodB<IR, BUF>(dpxHeader, readBuf, fd, block, reinterpret_cast<BUF *>(data));
			else if (packing == kPacked)
				return Read10bitPacked<IR, BUF>(dpxHeader, readBuf, fd, block, reinterpret_cast<BUF *>(data));
		}
		else if (bitDepth == 12)
		{
			if (packing == kPacked)
				return Read12bitPacked<IR, BUF>(dpxHeader, readBuf, fd, block, reinterpret_cast<BUF *>(data));
			/*else if (packing == kFilledMethodB)
				// filled method B
				// 12 bits fill LSB of 16 bits
				return Read12bitFilledMethodB<IR, BUF>(dpxHeader, reinterpret_cast<U16 *>(readBuf), fd, block, reinterpret_cast<BUF *>(data));
			else
				// filled method A
				// 12 bits fill MSB of 16 bits
				return ReadBlockTypes<IR, U16, kWord, BUF, BUFTYPE>(dpxHeader, reinterpret_cast<U16 *>(readBuf), fd, block, reinterpret_cast<BUF *>(data));*/
		}
		else if (size == cineon::kByte)
			return ReadBlockTypes<IR, U8, kByte, BUF, BUFTYPE>(dpxHeader, reinterpret_cast<U8 *>(readBuf), fd, block, reinterpret_cast<BUF *>(data));
		else if (size == cineon::kWord)
			return ReadBlockTypes<IR, U16, kWord, BUF, BUFTYPE>(dpxHeader, reinterpret_cast<U16 *>(readBuf), fd, block, reinterpret_cast<BUF *>(data));
		else if (size == cineon::kInt)
			return ReadBlockTypes<IR, U32, kInt, BUF, BUFTYPE>(dpxHeader, reinterpret_cast<U32 *>(readBuf), fd, block, reinterpret_cast<BUF *>(data));
		else if (size == cineon::kLongLong)
			return ReadBlockTypes<IR, U64, kLongLong, BUF, BUFTYPE>(dpxHeader, reinterpret_cast<U64 *>(readBuf), fd, block, reinterpret_cast<BUF *>(data));

		// should not reach here
		return false;
	}

	template <typename IR>
	bool ReadImageBlock(const Header &dpxHeader, U32 *readBuf, IR *fd, const Block &block, void *data, const DataSize size)
	{
		if (size == cineon::kByte)
			return ReadImageBlock<IR, U8, cineon::kByte>(dpxHeader, readBuf, fd, block, reinterpret_cast<U8 *>(data));
		else if (size == cineon::kWord)
			return ReadImageBlock<IR, U16, cineon::kWord>(dpxHeader, readBuf, fd, block, reinterpret_cast<U16 *>(data));
		else if (size == cineon::kInt)
			return ReadImageBlock<IR, U32, cineon::kInt>(dpxHeader, readBuf, fd,  block, reinterpret_cast<U32 *>(data));
		else if (size == cineon::kLongLong)
			return ReadImageBlock<IR, U64, cineon::kLongLong>(dpxHeader, readBuf, fd, block, reinterpret_cast<U64 *>(data));

		// should not reach here
		return false;
	}

}


#endif


