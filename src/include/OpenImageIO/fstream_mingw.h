/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/


/// @file  fstream_mingw.h
///
/// @brief Utilities for dealing with fstream on MingW.
/// Basically accepting wchar_t* filenames in the std::ifstream::open function
/// is a Windows MSVC extension and does not work on MingW. This file implements
/// ifstream and ofstream so that they work with UTF-16 filenames.


#ifndef OPENIMAGEIO_FSTREAM_MINGW_H
#define OPENIMAGEIO_FSTREAM_MINGW_H

#include <cassert>
#include <istream>
#include <ostream>

#if defined(_WIN32) && defined(__GLIBCXX__)
#include <ext/stdio_filebuf.h> // __gnu_cxx::stdio_filebuf
#include <fcntl.h>
#include <sys/stat.h>
#include <Share.h>


OIIO_NAMESPACE_BEGIN


template <class _CharT, class _Traits = std::char_traits<_CharT> >
class basic_ifstream
: public std::basic_istream<_CharT, _Traits>
{
public:
    typedef _CharT                         char_type;
    typedef _Traits                        traits_type;
    typedef typename traits_type::int_type int_type;
    typedef typename traits_type::pos_type pos_type;
    typedef typename traits_type::off_type off_type;
    
    typedef typename __gnu_cxx::stdio_filebuf<char_type, traits_type> stdio_filebuf;

    
    basic_ifstream();
    explicit basic_ifstream(const std::wstring& path, std::ios_base::openmode __mode = std::ios_base::in);
    
    virtual ~basic_ifstream();
    
    stdio_filebuf* rdbuf() const;
    bool is_open() const;
    void open(const std::wstring& path, std::ios_base::openmode __mode = std::ios_base::in);
    void close();
    
private:
    
    void open_internal(const std::wstring& path, std::ios_base::openmode mode);
    
    stdio_filebuf* __sb_;
};

template <class _CharT, class _Traits>
inline
basic_ifstream<_CharT, _Traits>::basic_ifstream()
: std::basic_istream<char_type, traits_type>(0)
, __sb_(0)
{
}


template <class _CharT, class _Traits>
inline
basic_ifstream<_CharT, _Traits>::basic_ifstream(const std::wstring& path, std::ios_base::openmode __mode)
: std::basic_istream<char_type, traits_type>(0)
, __sb_(0)
{
    open_internal(path, __mode | std::ios_base::in);
}

template <class _CharT, class _Traits>
inline
basic_ifstream<_CharT, _Traits>::~basic_ifstream()
{
    delete __sb_;
}


inline int
ios_open_mode_to_oflag(std::ios_base::openmode mode)
{
    int f = 0;
    if (mode & std::ios_base::in) {
        f |= _O_RDONLY;
    }
    if (mode & std::ios_base::out) {
        f |= _O_WRONLY;
        f |= _O_CREAT;
        if (mode & std::ios_base::app) {
            f |= _O_APPEND;
        }
        if (mode & std::ios_base::trunc) {
            f |= _O_TRUNC;
        }
    }
    if (mode & std::ios_base::binary) {
        f |= _O_BINARY;
    } else {
        f |= _O_TEXT;
    }
    return f;
}

template <class _CharT, class _Traits>
inline
void
basic_ifstream<_CharT, _Traits>::open_internal(const std::wstring& path, std::ios_base::openmode mode)
{
	if (is_open()) {
		// if the stream is already associated with a file (i.e., it is already open), calling this function fails.
		this->setstate(std::ios_base::failbit);
        return;
	}
    int fd;
    int oflag = ios_open_mode_to_oflag(mode);
    errno_t errcode = _wsopen_s(&fd, path.c_str(), oflag, _SH_DENYNO, _S_IREAD | _S_IWRITE);
    if (errcode != 0) {
        this->setstate(std::ios_base::failbit);
        return;
    }
    __sb_ = new stdio_filebuf(fd, mode, 1);
    if (__sb_ == 0) {
        this->setstate(std::ios_base::failbit);
        return;
    }
	// 409. Closing an fstream should clear error state
    this->clear();
	assert(__sb_);
	
	// In init() the rdstate() is set to badbit if __sb_ is NULL and
	// goodbit otherwise. The assert afterwards ensures this.
    this->init(__sb_);
	assert(this->good() && !this->fail());
}

template <class _CharT, class _Traits>
inline
typename basic_ifstream<_CharT, _Traits>::stdio_filebuf*
basic_ifstream<_CharT, _Traits>::rdbuf() const
{
    return const_cast<stdio_filebuf*>(__sb_);
}


template <class _CharT, class _Traits>
inline
bool
basic_ifstream<_CharT, _Traits>::is_open() const
{
    return __sb_ && __sb_->is_open();
}


template <class _CharT, class _Traits>
void
basic_ifstream<_CharT, _Traits>::open(const std::wstring& path, std::ios_base::openmode __mode)
{
    open_internal(path, __mode | std::ios_base::in);
}

template <class _CharT, class _Traits>
inline
void
basic_ifstream<_CharT, _Traits>::close()
{
    if (!__sb_) {
        return;
    }
    if (__sb_->close() == 0)
        this->setstate(std::ios_base::failbit);
    
    delete __sb_;
	__sb_= 0;

}



template <class _CharT, class _Traits = std::char_traits<_CharT> >
class basic_ofstream
: public std::basic_ostream<_CharT, _Traits>
{
public:
    typedef _CharT                         char_type;
    typedef _Traits                        traits_type;
    typedef typename traits_type::int_type int_type;
    typedef typename traits_type::pos_type pos_type;
    typedef typename traits_type::off_type off_type;
    
    typedef typename __gnu_cxx::stdio_filebuf<char_type, traits_type> stdio_filebuf;
    
    
    basic_ofstream();
    explicit basic_ofstream(const std::wstring& path, std::ios_base::openmode __mode = std::ios_base::out);
    
    virtual ~basic_ofstream();
    
    stdio_filebuf* rdbuf() const;
    bool is_open() const;
    void open(const std::wstring& path, std::ios_base::openmode __mode = std::ios_base::out);
    void close();
    
private:
    
    void open_internal(const std::wstring& path, std::ios_base::openmode mode);
    
    stdio_filebuf* __sb_;
};

template <class _CharT, class _Traits>
inline
basic_ofstream<_CharT, _Traits>::basic_ofstream()
: std::basic_ostream<char_type, traits_type>(0)
, __sb_(0)
{
}


template <class _CharT, class _Traits>
inline
basic_ofstream<_CharT, _Traits>::basic_ofstream(const std::wstring& path, std::ios_base::openmode __mode)
: std::basic_ostream<char_type, traits_type>(0)
, __sb_(0)
{
    open_internal(path, __mode  | std::ios_base::out);
}

template <class _CharT, class _Traits>
inline
basic_ofstream<_CharT, _Traits>::~basic_ofstream()
{
    delete __sb_;
}


template <class _CharT, class _Traits>
inline
void
basic_ofstream<_CharT, _Traits>::open_internal(const std::wstring& path, std::ios_base::openmode mode)
{
	if (is_open()) {
		// if the stream is already associated with a file (i.e., it is already open), calling this function fails.
		this->setstate(std::ios_base::failbit);
        return;
	}
    int fd;
    int oflag = ios_open_mode_to_oflag(mode);
    errno_t errcode = _wsopen_s(&fd, path.c_str(), oflag, _SH_DENYNO, _S_IREAD | _S_IWRITE);
    if (errcode != 0) {
        this->setstate(std::ios_base::failbit);
        return;
    }
    __sb_ = new stdio_filebuf(fd, mode, 1);
    if (__sb_ == 0) {
        this->setstate(std::ios_base::failbit);
        return;
    }
	// 409. Closing an fstream should clear error state
    this->clear();
	assert(__sb_);
	
	// In init() the rdstate() is set to badbit if __sb_ is NULL and
	// goodbit otherwise. The assert afterwards ensures this.
    this->init(__sb_);
	assert(this->good() && !this->fail());
}


template <class _CharT, class _Traits>
inline
typename basic_ofstream<_CharT, _Traits>::stdio_filebuf*
basic_ofstream<_CharT, _Traits>::rdbuf() const
{
    return const_cast<stdio_filebuf*>(__sb_);
}



template <class _CharT, class _Traits>
inline
bool
basic_ofstream<_CharT, _Traits>::is_open() const
{
    return __sb_ && __sb_->is_open();
}


template <class _CharT, class _Traits>
void
basic_ofstream<_CharT, _Traits>::open(const std::wstring& path, std::ios_base::openmode __mode)
{
    open_internal(path, __mode | std::ios_base::out);
}

template <class _CharT, class _Traits>
inline 
void
basic_ofstream<_CharT, _Traits>::close()
{
    if (!__sb_) {
        return;
    }
    if (__sb_->close() == 0)
        this->setstate(std::ios_base::failbit);
    
    delete __sb_;
	__sb_= 0;
}
// basic_fstream

OIIO_NAMESPACE_END


#endif // #if defined(_WIN32) && defined(__GLIBCXX__)


#endif // OPENIMAGEIO_FSTREAM_MINGW_H
