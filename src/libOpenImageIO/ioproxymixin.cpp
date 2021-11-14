// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/ioproxymixin.h>

#include "imageio_pvt.h"


OIIO_NAMESPACE_BEGIN
using namespace pvt;



template<class T>
bool
IOProxyMixin<T>::ioproxy_use_or_open(string_view name,
                                     Filesystem::IOProxy::Mode mode)
{
    if (!m_io) {
        // If no proxy was supplied, create an IOFile
        m_io = new Filesystem::IOFile(name, mode);
        m_io_local.reset(m_io);
    }
    if (!m_io || m_io->mode() != mode) {
        T::errorfmt("Could not open file \"{}\"", name);
        ioproxy_clear();
        return false;
    }
    return true;
}

// Explicit instantiations
template bool
IOProxyMixin<ImageInput>::ioproxy_use_or_open(string_view name,
                                              Filesystem::IOProxy::Mode mode);
template bool
IOProxyMixin<ImageOutput>::ioproxy_use_or_open(string_view name,
                                               Filesystem::IOProxy::Mode mode);



template<class T>
bool
IOProxyMixin<T>::fseek(int64_t pos, int origin)
{
    if (!m_io->seek(pos, origin)) {
        T::errorfmt(
            "Seek error, could not seek from {} to {} (total size {}) {}",
            m_io->tell(),
            origin == SEEK_SET ? pos
                               : (origin == SEEK_CUR ? pos + m_io->tell()
                                                     : pos + m_io->size()),
            m_io->size(), m_io->error());
        return false;
    }
    return true;
}

template bool
IOProxyMixin<ImageInput>::fseek(int64_t pos, int origin);
template bool
IOProxyMixin<ImageOutput>::fseek(int64_t pos, int origin);



template<>
bool
IOProxyMixin<ImageInput>::fread(void* buf, size_t itemsize, size_t nitems)
{
    size_t size = itemsize * nitems;
    size_t n    = m_io->read(buf, size);
    if (n != size) {
        if (size_t(m_io->tell()) >= m_io->size())
            ImageInput::errorfmt("Read error on \"{}\": hit end of file",
                                 m_io->filename());
        else
            ImageInput::errorfmt(
                "Read error at position {}, could only read {}/{} bytes {}",
                m_io->tell() - n, n, size, m_io->error());
    }
    return n == size;
}



template<>
bool
IOProxyMixin<ImageOutput>::fwrite(const void* buf, size_t itemsize,
                                  size_t nitems)
{
    size_t size = itemsize * nitems;
    size_t n    = m_io->write(buf, size);
    if (n != size)
        ImageOutput::errorfmt(
            "Write error at position {}, could only write {}/{} bytes {}",
            m_io->tell() - n, n, size, m_io->error());
    return n == size;
}


OIIO_NAMESPACE_END
