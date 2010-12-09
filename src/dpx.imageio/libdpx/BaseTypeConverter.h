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


#ifndef _DPX_BASETYPECONVERTER_H
#define _DPX_BASETYPECONVERTER_H 1


namespace dpx
{
	// convert between all of the DPX base types in a controllable way
	
	// Note: These bit depth promotions (low precision -> high) use
	// a combination of bitshift and the 'or' operator in order to
	// fully populate the output coding space
	// 
	// For example, when converting 8->16 bits, the 'simple' method
	// (shifting 8 bits) maps 255 to 65280. This result is not ideal
	// (uint16 'max' of 65535 is preferable). Overall, the best conversion
	// is one that approximates the 'true' floating-point scale factor.
	// 8->16 : 65535.0 / 255.0. 10->16 : 65535.0 / 1023.0.
	// For performance considerations, we choose to emulate this
	// floating-poing scaling with pure integer math, using a trick
	// where we duplicate portions of the MSB in the LSB.
	//
	// For bit depth demotions, simple truncation is used.
	//
	
	inline void BaseTypeConverter(U8 &src, U8 &dst)
	{
		dst = src;
	}
	
	inline void BaseTypeConverter(U8 &src, U16 &dst)
	{
		dst = (src << 8) | src;
	}
	
	inline void BaseTypeConverter(U8 &src, U32 &dst)
	{
		dst = (src << 24) | (src << 16) | (src << 8) | src;
	}
	
	inline void BaseTypeConverter(U8 &src, R32 &dst)
	{
		dst = src;
	}
	
	inline void BaseTypeConverter(U8 &src, R64 &dst)
	{
		dst = src;
	}
	
	inline void BaseTypeConverter(U16 &src, U8 &dst)
	{
		dst = src >> 8;
	}
	
	inline void BaseTypeConverter(U16 &src, U16 &dst)
	{
		dst = src;
	}
	
	inline void BaseTypeConverter(U16 &src, U32 &dst)
	{
		dst = (src << 16) | src;
	}
	
	inline void BaseTypeConverter(U16 &src, R32 &dst)
	{
		dst = src;
	}
	
	inline void BaseTypeConverter(U16 &src, R64 &dst)
	{
		dst = src;
	}
	
	inline void BaseTypeConverter(U32 &src, U8 &dst)
	{
		dst = src >> 24;
	}
	
	inline void BaseTypeConverter(U32 &src, U16 &dst)
	{
		dst = src >> 16;
	}
	
	inline void BaseTypeConverter(U32 &src, U32 &dst)
	{
		dst = src;
	}
	
	inline void BaseTypeConverter(U32 &src, R32 &dst)
	{
		dst = src;
	}
	
	inline void BaseTypeConverter(U32 &src, R64 &dst)
	{
		dst = src;
	}
	
	inline void BaseTypeConverter(R32 &src, U8 &dst)
	{
		dst = src;
	}
	
	inline void BaseTypeConverter(R32 &src, U16 &dst)
	{
		dst = src;
	}
	
	inline void BaseTypeConverter(R32 &src, U32 &dst)
	{
		dst = src;
	}
	
	inline void BaseTypeConverter(R32 &src, R32 &dst)
	{
		dst = src;
	}
	
	inline void BaseTypeConverter(R32 &src, R64 &dst)
	{
		dst = src;
	}
	
	inline void BaseTypeConverter(R64 &src, U8 &dst)
	{
		dst = src;
	}
	
	inline void BaseTypeConverter(R64 &src, U16 &dst)
	{
		dst = src;
	}
	
	inline void BaseTypeConverter(R64 &src, U32 &dst)
	{
		dst = src;
	}
	
	inline void BaseTypeConverter(R64 &src, R32 &dst)
	{
		dst = src;
	}
	
	inline void BaseTypeConverter(R64 &src, R64 &dst)
	{
		dst = src;
	}
	
	inline void BaseTypeConvertU10ToU16(U16 &src, U16 &dst)
	{
		dst = (src << 6) | (src >> 4);
	}
	
	inline void BaseTypeConvertU12ToU16(U16 &src, U16 &dst)
	{
		dst = (src << 4) | (src >> 8);
	}
	
}

#endif


