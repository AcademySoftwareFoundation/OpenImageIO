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

// Alterations:
// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause and Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

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
