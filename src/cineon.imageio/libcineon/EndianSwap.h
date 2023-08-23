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


#ifndef _CINEON_ENDIANSWAP_H
#define _CINEON_ENDIANSWAP_H 1


namespace cineon
{

template <typename T>
T SwapBytes(T& value)
{
	unsigned char *pe, *ps = reinterpret_cast<unsigned char*>(&value);
	unsigned char c;
	size_t s = (sizeof(T));

	pe = ps + s - 1;
	for (size_t i = s/2; i > 0; i--)
	{
		c = *ps;
		*ps = *pe;
		*pe = c;

		ps++; pe--;
	}

	return value;
}


template <>
inline unsigned short SwapBytes( unsigned short& value )
{
	unsigned char *p = reinterpret_cast<unsigned char*>(&value);
	unsigned char c = p[0];
	p[0] = p[1];
	p[1] = c;
	return value;
}

template <>
inline unsigned char SwapBytes( unsigned char& value )
{
	return value;
}


template <>
inline char SwapBytes( char& value )
{
	return value;
}


template <typename T>
void SwapBuffer(T *buf, size_t len)
{
	for (size_t i = 0; i < len; i++)
		SwapBytes(buf[i]);
}


template <DataSize SIZE>
void EndianSwapImageBuffer(void *data, size_t length)
{
	switch (SIZE)
	{
	case cineon::kByte:
		break;

	case cineon::kWord:
		SwapBuffer(reinterpret_cast<U16 *>(data), length);
		break;

	case cineon::kInt:
		SwapBuffer(reinterpret_cast<U32 *>(data), length);
		break;

	case cineon::kLongLong:
		SwapBuffer(reinterpret_cast<U64 *>(data), length);
		break;
	}
}


inline void EndianSwapImageBuffer(DataSize size, void *data, size_t length)
{
	switch (size)
	{
	case cineon::kByte:
		break;

	case cineon::kWord:
		SwapBuffer(reinterpret_cast<U16 *>(data), length);
		break;

	case cineon::kInt:
		SwapBuffer(reinterpret_cast<U32 *>(data), length);
		break;

	case cineon::kLongLong:
		SwapBuffer(reinterpret_cast<U64 *>(data), length);
		break;
	}
}


}

#endif

