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
/// DLLPUBLIC and DLLEXPORT macros that are necessary for proper symbol
/// export when doing multi-platform development.


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
/// We solve this awful mess by defining these macros:
///
/// DLLPUBLIC - normally, assumes that it's being seen by a client
///             of the library, and therefore declare as 'imported'.
///             But if DLL_EXPORT_PUBLIC is defined, change the declaration
///             to 'exported' -- you want to define this macro when
///             compiling the module that actually defines the class.
///
/// DLLEXPORT - tag a symbol as exported from its DLL and therefore
///             visible from any app that links against the DLL.  Do this
///             only if you don't need to call the routine from within
///             a different compilation module within the same DLL.  Or,
///             if you just don't want to worry about the whole
///             DLL_EXPORT_PUBLIC mess, and use this everywhere without
///             fretting about the minor perf hit of not using 'import'.
///
/// Note that on Linux, all symbols are exported so this just isn't a
/// problem, so we define these macros to be nothing.
///
/// It's a shame that we have to clutter all our header files with these
/// stupid macros just because the Windows world is such a mess.
///

#ifndef DLLEXPORT
#  if defined(_MSC_VER) && (defined(_WIN32) || defined(_WIN32))
#    define DLLEXPORT __declspec(dllexport)
#  else
#    define DLLEXPORT
#  endif
#endif

#ifndef DLLPUBLIC
#  if defined(_MSC_VER) && (defined(_WIN32) || defined(_WIN32))
#    ifdef OpenImageIO_EXPORTS
#      define DLLPUBLIC __declspec(dllexport)
#    else
#      define DLLPUBLIC __declspec(dllimport)
#    endif
#  else
#    define DLLPUBLIC
#  endif
#endif

#endif // OPENIMAGEIO_EXPORT_H
