// -*- mode: C++; tab-width: 4 -*-
// vi: ts=4

// Copyright 2020-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

#include "DPXStream.h"

#include <cstdio>


void InStream::Rewind()
{
    if (IsValid())
        m_io->seek(0);
}

bool InStream::Seek(long offset, Origin origin)
{
    if (!IsValid())
        return false;

    int ioOrigin;
    switch (origin) {
    case kStart: ioOrigin = SEEK_SET; break;
    case kCurrent: ioOrigin = SEEK_CUR; break;
    case kEnd: ioOrigin = SEEK_END; break;
    default: ioOrigin = SEEK_SET; break;
    }

    return m_io->seek(offset, ioOrigin);
}

size_t InStream::Read(void *buf, const size_t size)
{
    return IsValid() ? m_io->read(buf, size) : false;
}

size_t InStream::ReadDirect(void *buf, const size_t size)
{
    return Read(buf, size);
}

bool InStream::EndOfFile() const
{
    return IsValid() ? (size_t(m_io->tell()) >= m_io->size()) : true;
}

long InStream::Tell()
{
    return IsValid() ? m_io->tell() : -1;
}
