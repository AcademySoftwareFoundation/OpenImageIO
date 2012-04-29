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


#ifndef _CINEON_BASETYPECONVERTER_H
#define _CINEON_BASETYPECONVERTER_H 1


namespace cineon
{
	// convert between all of the DPX base types in a controllable way

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

	inline void BaseTypeConverter(U8 &src, U64 &dst)
	{
		dst = static_cast<U64>(src) << 56;
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
		dst = src << 16;
	}

	inline void BaseTypeConverter(U16 &src, U64 &dst)
	{
		dst = static_cast<U64>(src) << 48;
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

	inline void BaseTypeConverter(U32 &src, U64 &dst)
	{
		dst = static_cast<U64>(src) << 32;
	}

	inline void BaseTypeConverter(U32 &src, R32 &dst)
	{
		dst = src;
	}

	inline void BaseTypeConverter(U32 &src, R64 &dst)
	{
		dst = src;
	}

	inline void BaseTypeConverter(U64 &src, U8 &dst)
	{
		dst = src >> 56;
	}

	inline void BaseTypeConverter(U64 &src, U16 &dst)
	{
		dst = src >> 48;
	}

	inline void BaseTypeConverter(U64 &src, U32 &dst)
	{
		dst = src >> 32;
	}

	inline void BaseTypeConverter(U64 &src, U64 &dst)
	{
		dst = src;
	}

	inline void BaseTypeConverter(U64 &src, R32 &dst)
	{
		dst = src;
	}

	inline void BaseTypeConverter(U64 &src, R64 &dst)
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

	inline void BaseTypeConverter(R32 &src, U64 &dst)
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

	inline void BaseTypeConverter(R64 &src, U64 &dst)
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


