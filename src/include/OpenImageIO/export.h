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


#ifndef OPENIMAGEIO_EXPORT_H
#define OPENIMAGEIO_EXPORT_H

/// \file
/// Macros necessary for proper symbol export from dynamic libraries.


///
/// On Windows, when compiling code that will end up in a DLL, symbols
/// must be marked as 'exported' (i.e. __declspec(dllexport)) or they
/// won't be visible to programs linking against the DLL.
///
/// In addition, when compiling the application code that calls the DLL,
/// if a routine is marked as 'imported' (i.e. __declspec(dllimport)),
/// the compiler can be smart about eliminating a level of calling
/// indirection.  But you DON'T want to use __declspec(dllimport) when
/// calling a function from within its own DLL (it will still compile
/// correctly, just not with maximal efficiency).  Which is quite the
/// dilemma since the same header file is used by both the library and
/// its clients.  Sheesh!
///
/// But on Linux/OSX as well, we want to only have the DSO export the
/// symbols we designate as the public interface.  So we link with
/// -fvisibility=hidden to default to hiding the symbols.  See
/// http://gcc.gnu.org/wiki/Visibility
///
/// We solve this awful mess by defining these macros:
///
/// OIIO_API - used for the OpenImageIO public API.  Normally, assumes
///               that it's being seen by a client of the library, and
///               therefore declare as 'imported'.  But if
///               OpenImageIO_EXPORT is defined (as is done by CMake
///               when compiling the library itself), change the
///               declaration to 'exported'.
/// OIIO_EXPORT - explicitly exports a symbol that isn't part of the 
///               public API but still needs to be visible.
/// OIIO_LOCAL -  explicitly hides a symbol that might otherwise be
///               exported
///
/// 

#if defined(_MSC_VER) || defined(__CYGWIN__)
  #ifndef OIIO_STATIC_BUILD
    #define OIIO_IMPORT __declspec(dllimport)
    #define OIIO_EXPORT __declspec(dllexport)
  #else
    #define OIIO_IMPORT
    #define OIIO_EXPORT
  #endif
  #define OIIO_LOCAL
#else
  #if (10000*__GNUC__ + 100*__GNUC_MINOR__ + __GNUC_PATCHLEVEL__) > 40102
    #define OIIO_IMPORT __attribute__ ((visibility ("default")))
    #define OIIO_EXPORT __attribute__ ((visibility ("default")))
    #define OIIO_LOCAL  __attribute__ ((visibility ("hidden")))
  #else
    #define OIIO_IMPORT
    #define OIIO_EXPORT
    #define OIIO_LOCAL
  #endif
#endif

#if defined(OpenImageIO_EXPORTS) || defined(OpenImageIO_Util_EXPORTS)
#  define OIIO_API OIIO_EXPORT
#else
#  define OIIO_API OIIO_IMPORT
#endif


// Back compatibility macros (DEPRECATED)
#define DLLPUBLIC OIIO_API
#define DLLEXPORT OIIO_EXPORT

#endif // OPENIMAGEIO_EXPORT_H
