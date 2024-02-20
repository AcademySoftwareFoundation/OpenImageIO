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


#include <cstdio>

#include <OpenImageIO/filesystem.h>

#include "CineonStream.h"

namespace cineon {

InStream::InStream() : fp(0)
{
}


InStream::~InStream()
{
}


bool InStream::Open(const char *f)
{
	if (this->fp)
		this->Close();
	if ((this->fp = OIIO::Filesystem::fopen(f, "rb")) == 0)
		return false;

	return true;
}


void InStream::Close()
{
	if (this->fp)
	{
		::fclose(this->fp);
		this->fp = 0;
	}
}


void InStream::Rewind()
{
	if (this->fp)
		::rewind(fp);
}


bool InStream::Seek(long offset, Origin origin)
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
		return false;
	return (::fseek(this->fp, offset, o) == 0);
}



size_t InStream::Read(void *buf, const size_t size)
{
	if (this->fp == 0)
		return 0;
	return ::fread(buf, 1, size, this->fp);
}


size_t InStream::ReadDirect(void *buf, const size_t size)
{
	return this->Read(buf, size);
}


bool InStream::EndOfFile() const
{
	if (this->fp == 0)
		return true;
	return ::feof(this->fp);
}

}



