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


/// \file
///
/// Wrapper so that hash_map and hash_set mean what we want regardless
/// of the compiler.
///

#ifndef OPENIMAGEIO_HASH_H
#define OPENIMAGEIO_HASH_H

#include <boost/version.hpp>

#if BOOST_VERSION >= 103600
#define OIIO_HAVE_BOOST_UNORDERED_MAP
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
#endif

#ifndef OIIO_HAVE_BOOST_UNORDERED_MAP

#ifdef __GNUC__

#include <ext/hash_map>
#include <ext/hash_set>
using __gnu_cxx::hash_map;
using __gnu_cxx::hash_set;

#else // __GNUC__

#ifdef _MSC_VER
#error Boost 1.36 or greater with "unordered_map" support required.
#endif // _MSC_VER

#endif // __GNUC__

#endif // OIIO_HAVE_BOOST_UNORDERED_MAP

#endif // OPENIMAGEIO_HASH_H
