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

#include <cstdio>


#include "DPXStream.h"


OutStream::OutStream() : fp(0)
{
}


OutStream::~OutStream()
{
}


bool OutStream::Open(const char *f)
{
	if (this->fp)
		this->Close();
	if ((this->fp = ::fopen(f, "wb")) == 0)
		return false;
		
	return true;
}


void OutStream::Close()
{
	if (this->fp)
	{
		::fclose(this->fp);
		this->fp = 0;
	}
}


size_t OutStream::Write(void *buf, const size_t size)
{
	if (this->fp == 0)
		return false;
	return ::fwrite(buf, 1, size, this->fp);
}


bool OutStream::Seek(long offset, Origin origin)
{
	int o = 0;
	switch (origin)
	{
	case kCurrent:
		o = SEEK_CUR;
		break;
	case kEnd:
		o = SEEK_END;
		break;
	case kStart:
		o = SEEK_SET;
		break;
	}
	
	if (this->fp == 0)
		return -1;
	return (::fseek(this->fp, offset, o) == 0);
}


void OutStream::Flush()
{
	if (this->fp)
		::fflush(this->fp);
}







