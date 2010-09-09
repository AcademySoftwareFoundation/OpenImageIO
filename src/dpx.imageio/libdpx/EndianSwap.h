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
 

#ifndef _DPX_ENDIANSWAP_H
#define _DPX_ENDIANSWAP_H 1


namespace dpx
{

template <typename T>
T SwapBytes(T& value)
{
	register unsigned char *pe, *ps = reinterpret_cast<unsigned char*>(&value);
	register unsigned char c;
	register size_t s = (sizeof(T));

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
	register unsigned char *p = reinterpret_cast<unsigned char*>(&value);
	register unsigned char c = p[0];
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
void SwapBuffer(T *buf, unsigned int len)
{
	for (unsigned int i = 0; i < len; i++)
		SwapBytes(buf[i]);
}


template <DataSize SIZE>
void EndianSwapImageBuffer(void *data, int length)
{
	switch (SIZE)
	{
	case dpx::kByte:
		break;
				
	case dpx::kWord:
		SwapBuffer(reinterpret_cast<U16 *>(data), length);
		break;
				
	case dpx::kInt:
		SwapBuffer(reinterpret_cast<U32 *>(data), length);
		break;
					
	case dpx::kFloat:
		SwapBuffer(reinterpret_cast<R32 *>(data), length);
		break;
		
	case dpx::kDouble:
		SwapBuffer(reinterpret_cast<R64 *>(data), length);
		break;
	}
}


inline void EndianSwapImageBuffer(DataSize size, void *data, int length)
{
	switch (size)
	{
	case dpx::kByte:
		break;
				
	case dpx::kWord:
		SwapBuffer(reinterpret_cast<U16 *>(data), length);
		break;
				
	case dpx::kInt:
		SwapBuffer(reinterpret_cast<U32 *>(data), length);
		break;
					
	case dpx::kFloat:
		SwapBuffer(reinterpret_cast<R32 *>(data), length);
		break;
		
	case dpx::kDouble:
		SwapBuffer(reinterpret_cast<R64 *>(data), length);
		break;
	}
}


}

#endif

