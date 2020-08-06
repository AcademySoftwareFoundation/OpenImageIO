// -*- mode: C++; tab-width: 4 -*-
// vi: ts=4

// Copyright 2020-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

#include "InStreamMem.h"

#include <string.h>


InStreamMem::InStreamMem() : memBuf(nullptr), memSize(0)
{
}

InStreamMem::~InStreamMem()
{
}

bool InStreamMem::Open(const char *f)
{
	return false;
}

bool InStreamMem::Open(const void * memBuf, const size_t memSize)
{
	this->memBuf	= memBuf;
	this->memSize	= memSize;
	this->curPos	= 0;

	return (this->memBuf != nullptr);
}

void InStreamMem::Close()
{
	this->memBuf	= nullptr;
	this->memSize	= 0;
	this->curPos	= 0;
}

void InStreamMem::Rewind()
{
	this->curPos = 0;
}

bool InStreamMem::Seek(long offset, Origin origin)
{
	if (this->memBuf == nullptr)
		return false;

	auto pos = (long long) this->curPos;
	switch (origin)
	{
	case kStart:
		pos = offset;
		break;
	case kCurrent:
		pos += offset;
		break;
	case kEnd:
		pos = (long long) this->memSize + offset;
		break;
	}

	//NOTE: We allow seeking 1 byte beyond to last valid byte for various purposes; e.g. for
	// getting the file size (/ memory size), or for signaling the file end (/ memory end)
	if (pos < 0 || (unsigned long long) pos > this->memSize)
		return false;

	this->curPos = pos;
	return true;
}

size_t InStreamMem::Read(void *buf, const size_t size)
{
	if (EndOfFile() || buf == nullptr || size <= 0)
		return 0;

	auto remSize	= this->memSize - this->curPos;
	auto cpySize	= (remSize < size) ? remSize : size;
	auto srcBuf		= static_cast<void*>((unsigned char*) this->memBuf + this->curPos);
	memcpy(buf, srcBuf, cpySize);
	this->curPos	+= cpySize;
	return cpySize;
}

size_t InStreamMem::ReadDirect(void *buf, const size_t size)
{
	return this->Read(buf, size);
}

bool InStreamMem::EndOfFile() const
{
	if (this->memBuf == nullptr)
		return true;
	return (this->curPos >= this->memSize);
}

long InStreamMem::Tell()
{
	if (this->memBuf == nullptr)
		return -1;
	return (long) this->curPos;
}
