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


#ifndef _DPX_WRITERINTERNAL_H
#define _DPX_WRITERINTERNAL_H 1


#include "BaseTypeConverter.h"


namespace dpx 
{
	

	void EndianBufferSwap(int bitdepth, dpx::Packing packing, void *buf, const size_t size)
	{
		switch (bitdepth)
		{
		case 8:
			break;
		case 12:
			if (packing == dpx::kPacked)
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


	template <typename T1, typename T2>
	void MultiTypeBufferCopy(T1 *dst, T2 *src, const int len)
	{
		for (int i = 0; i < len; i++)
			BaseTypeConverter(src[i], dst[i]);
	}


	template <typename IB>
	void CopyWriteBuffer(DataSize src_size, unsigned char *src, IB * dst, const int len)
	{
		if (src_size == kByte)
			MultiTypeBufferCopy<IB, U8>(dst, reinterpret_cast<U8 *>(src), len);
		else if (src_size == kWord)
			MultiTypeBufferCopy<IB, U16>(dst, reinterpret_cast<U16 *>(src), len);
		else if (src_size == kFloat)
			MultiTypeBufferCopy<IB, R32>(dst, reinterpret_cast<R32 *>(src), len);
		else if (src_size == kDouble)
			MultiTypeBufferCopy<IB, R64>(dst, reinterpret_cast<R64 *>(src), len);

	}


	// access modifications to the buffer based on compression and packing
	struct BufferAccess 
	{ 
		int offset; 
		int length;
		BufferAccess() : offset(0), length(0) { }
	};


	
	// \todo NOT DONE
	template <typename IB, int BITDEPTH>
	void RleCompress(IB *src, IB *dst, const int bufsize, const int len, BufferAccess &access)
	{
		IB ch;
		int count;
		int i;
		int index = bufsize - 1;
		bool start = true;
		//bool match = true;
		
		// for each data type, have maximum length of rle datum
		// subtract one so it the LSBit can be used to state
		int maxCount;		
		if (BITDEPTH == 8)
			maxCount = 0xff - 1;
		else if (BITDEPTH == 10)
			maxCount = 0x3ff - 1;
		else if (BITDEPTH == 12)
			maxCount = 0xfff - 1;
		else if (BITDEPTH == 16)
			maxCount = 0xffff - 1;
		else 
			maxCount = 1000000;		// high number for floats, doubles

		
		for (i = len - 1; i >= 0; i--)
		{
			if (start)
			{
				count = 1;
				start = false;
				ch = src[i];
				dst[index--] = ch;
			}
		}
		
		access.offset = index;
		access.length = bufsize - index;
	}


	template <typename IB, int BITDEPTH>
	void WritePackedMethod(IB *src, IB *dst, const int len, const bool reverse, BufferAccess &access)
	{	
		// pack into the same memory space
		U32 *dst_u32 = reinterpret_cast<U32*>(dst);

		// bit shift count for U16 source
		const int shift = 16 - BITDEPTH;
		
		// bit mask
		U32 mask = 0;
		if (BITDEPTH == 10)
			mask = 0x03ff;
		else if (BITDEPTH == 12)
			mask = 0x0fff;
		else if (BITDEPTH == 8)
			return;

		int i, entry;
		for (i = 0; i < len; i++)
		{
			// read value and determine write location
			U32 value = static_cast<U32>(src[i+access.offset]) >> shift;

			// if reverse the order
/*** XXX TODO REVERSE
			if (reverse)
				// reverse the triplets so entry would be 2,1,0,5,4,3,8,7,6,...
				entry = ((i / 3) * 3) + (2 - (i % 3));
			else
***/
				entry = i;

			int div = (entry * BITDEPTH) / 32;			// 32 bits in a U32
			int rem = (entry * BITDEPTH) % 32;
			
			// write the bits that belong in the first U32
			// calculate the masked bits for the added value
			U32 shift_mask = mask << rem;
			
			// mask sure to mask the bits to save as part of the src_buf
			// so if writing bits 8-18, save the bits of the source material
			dst_u32[div] = (dst_u32[div] & ~shift_mask) | ((value << rem) & shift_mask);

			// write across multiple U16? count the carry bits
			int carry = BITDEPTH - (32 - rem);
			if (carry > 0)
			{
				U32 save = BITDEPTH - carry;
				dst_u32[div+1] = (dst_u32[div+1] & ~(mask >> save)) | ((value >> save) & (mask >> save));
			}
		}
		
		// adjust offset/length
		access.offset = 0;
		access.length = (((len * BITDEPTH) / 32) + ((len * BITDEPTH) % 32 ? 1 : 0)) * 2;
	}	



	// this routine expects a type of U16
	template <typename IB, Packing METHOD>
	void WritePackedMethodAB_10bit(IB *src, IB *dst, const int len, const bool reverse, BufferAccess &access)
	{	
		// pack into the same memory space
		U32 *dst_u32 = reinterpret_cast<U32*>(dst);
	
		// bit shift count
		const U32 shift = 6;  // (16 - BITDEPTH)
		const U32 bitdepth = 10;
		const U32 bitmask = 0x03ff;
		
		// shift bits over 2 if Method A
		const int method_shift = (METHOD == kFilledMethodA ? 2 : 0);
		
		// loop through the buffer
		int i;
		U32 value = 0;
		for (i = 0; i < len; i++)
		{
			int div = i / 3;			// 3 10-bit values in a U32
			int rem = i % 3;
	
			// write previously calculated value
			if (i && rem == 0)
			{
				dst_u32[div-1] = value;
				value = 0;
			}

			// if reverse the order
			if (reverse)
				rem = 2 - rem;

			// place the 10 bits in the proper place with mask
			U32 comp = ((static_cast<U32>(src[i+access.offset]) >> shift) << (bitdepth * rem)) << method_shift;
			U32 mask = (bitmask << (bitdepth * rem)) << method_shift ;
			
			// overwrite only the proper 10 bits
			value = (value & ~mask) | (comp & mask);
		}
		
		// write last
		dst_u32[(len+2)/3-1] = value;

		// adjust offset/length
		// multiply * 2 because it takes two U16 = U32 and this func packs into a U32
		access.offset = 0;
		access.length = ((len / 3) + (len % 3 ? 1 : 0)) * 2;
	}	
	
			
	
	template <typename IB, int BITDEPTH, bool SAMEBUFTYPE>
	int WriteBuffer(OutStream *fd, DataSize src_size, void *src_buf, const U32 width, const U32 height, const int noc, const Packing packing, 
					const bool rle, const bool reverse, const int eolnPad, char *blank, bool &status, bool swapEndian)
	{
		int fileOffset = 0;
		
		// determine any impact on the max line size due to RLE
		// impact may be that rle is true but the data can not be compressed at all
		// the worst possible compression with RLE is increasing the image size by 1/3
		// so we will just double the destination size if RLE
		int rleBufAdd = (rle ? ((width * noc / 3) + 1) : 0);

		// buffer access parameters
		BufferAccess bufaccess;
		bufaccess.offset = 0;
		bufaccess.length = width * noc;
		
		// allocate one line
		IB *src;
		IB *dst = new IB[(width * noc) + 1 + rleBufAdd];

		// each line in the buffer
		for (U32 h = 0; h < height; h++)
		{
			// image buffer
			unsigned char *imageBuf = reinterpret_cast<unsigned char*>(src_buf);
			const int bytes = Header::DataSizeByteCount(src_size);

			// copy buffer if need to promote data types from src to destination
			if (!SAMEBUFTYPE)
			{
				src = dst;
				CopyWriteBuffer<IB>(src_size, (imageBuf+(h*width*noc*bytes)+(h*eolnPad)), dst, (width*noc));
			} 
			else
				// not a copy, access source
				src = reinterpret_cast<IB*>(imageBuf + (h * width * noc * bytes) + (h*eolnPad));

			// if rle, compress
			if (rle)
			{
				RleCompress<IB, BITDEPTH>(src, dst, ((width * noc) + rleBufAdd), width * noc, bufaccess);
				src = dst;
			}
			
			// if 10 or 12 bit, pack
			if (BITDEPTH == 10)
			{
				if (packing == dpx::kPacked)
				{
					WritePackedMethod<IB, BITDEPTH>(src, dst, (width*noc), reverse, bufaccess);
				}
				else if (packing == kFilledMethodA)
				{
					WritePackedMethodAB_10bit<IB, dpx::kFilledMethodA>(src, dst, (width*noc), reverse, bufaccess);
				}
				else // if (packing == dpx::kFilledMethodB)
				{
					WritePackedMethodAB_10bit<IB, dpx::kFilledMethodB>(src, dst, (width*noc), reverse, bufaccess);
				}				
			}
			else if (BITDEPTH == 12)
			{
				if (packing == dpx::kPacked)
				{
					WritePackedMethod<IB, BITDEPTH>(src, dst, (width*noc), reverse, bufaccess);
				}
				else if (packing == dpx::kFilledMethodB)
				{
					// shift 4 MSB down, so 0x0f00 would become 0x00f0
					for (int w = 0; w < bufaccess.length; w++)
						dst[w] = src[bufaccess.offset+w] >> 4;
					bufaccess.offset = 0;
				}
				// a bitdepth of 12 by default is packed with dpx::kFilledMethodA
				// assumes that either a copy or rle was required
				// otherwise this routine should not be called with:
				//     12-bit Method A with the source buffer data type is kWord				
			}
			
			// write line
			fileOffset += (bufaccess.length * sizeof(IB));
			if (swapEndian)
			    EndianBufferSwap(BITDEPTH, packing, dst + bufaccess.offset, bufaccess.length * sizeof(IB));
			if (fd->Write(dst+bufaccess.offset, (bufaccess.length * sizeof(IB))) == false)
			{
				status = false;
				break;
			}
			
			// end of line padding
			if (eolnPad)
			{
				fileOffset += eolnPad;
				if (fd->Write(blank, eolnPad) == false)
				{
					status = false;
					break;
				}
			}	
	
		}
		
		// done with buffer
		delete [] dst;
		
		return fileOffset;
	}


	template <typename IB, int BITDEPTH, bool SAMEBUFTYPE>
	int WriteFloatBuffer(OutStream *fd, DataSize src_size, void *src_buf, const U32 width, const U32 height, const int noc, const Packing packing, 
					const bool rle, const int eolnPad, char *blank, bool &status, bool swapEndian)
	{
		int fileOffset = 0;
		
		// determine any impact on the max line size due to RLE
		// impact may be that rle is true but the data can not be compressed at all
		// the worst possible compression with RLE is increasing the image size by 1/3
		// so we will just double the destination size if RLE
		int rleBufAdd = (rle ? ((width * noc / 3) + 1) : 0);

		// buffer access parameters
		BufferAccess bufaccess;
		bufaccess.offset = 0;
		bufaccess.length = width * noc;
		
		// allocate one line
		IB *src;
		IB *dst = new IB[(width * noc) + rleBufAdd];

		// each line in the buffer
		for (U32 h = 0; h < height; h++)
		{
			// image buffer
			unsigned char *imageBuf = reinterpret_cast<unsigned char*>(src_buf);
			const int bytes = Header::DataSizeByteCount(src_size);

			// copy buffer if need to promote data types from src to destination
			if (!SAMEBUFTYPE)
			{
				src = dst;
				CopyWriteBuffer<IB>(src_size, (imageBuf+(h*width*noc*bytes)+(h*eolnPad)), dst, (width*noc));
			} 
			else
				// not a copy, access source
				src = reinterpret_cast<IB*>(imageBuf + (h * width * noc * bytes) + (h*eolnPad));

			// if rle, compress
			if (rle)
			{
				RleCompress<IB, BITDEPTH>(src, dst, ((width * noc) + rleBufAdd), width * noc, bufaccess);
				src = dst;
			}
			
			// write line
			fileOffset += (bufaccess.length * sizeof(IB));
			if (swapEndian)
			    EndianBufferSwap(BITDEPTH, packing, dst + bufaccess.offset, bufaccess.length * sizeof(IB));
			if (fd->Write(dst+bufaccess.offset, (bufaccess.length * sizeof(IB))) == false)
			{
				status = false;
				break;
			}
			
			// end of line padding
			if (eolnPad)
			{
				fileOffset += eolnPad;
				if (fd->Write(blank, eolnPad) == false)
				{
					status = false;
					break;
				}
			}	
	
		}
		
		// done with buffer
		delete [] dst;
		
		return fileOffset;
	}	
}

#endif


