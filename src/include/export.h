/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2008 Larry Gritz.
// 
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
// 
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 
// (This is the MIT open source license.)
/////////////////////////////////////////////////////////////////////////////


#ifndef EXPORT_H
#define EXPORT_H

// On Windows, when compiling code that will end up in a DLL's, symbols
// must be marked as 'exported' (i.e. __declspec(dllexport)) or they
// won't be visible to programs linking against the DLL.
//
// In addition, when compiling the application code that calls the DLL,
// if a routine is marked as 'imported' (i.e. __declspec(dllimport)),
// the compiler can be smart about eliminating a level of calling
// indirection.  But you DON'T want to use __declspec(dllimport) when
// calling a function from within its own DLL (it will still compile
// correctly, just not with maximal efficiency).  Which is quite the
// dilemma since the same header file is used by both the library and
// its clients.  Sheesh!
//
// We solve this awful mess by defining these macros:
//
// DLLPUBLIC - normally, assumes that it's being seen by a client
//             of the library, and therefore declare as 'imported'.
//             But if EXPORT_DLLPUBLIC is defined, change the declaration
//             to 'exported' -- you want to define this macro when
//             compiling the module that actually defines the class.
//
// DLLEXPORT - tag a symbol as exported from its DLL and therefore
//             visible from any app that links against the DLL.  Do this
//             only if you don't need to call the routine from within
//             a different compilation module within the same DLL.  Or,
//             if you just don't want to worry about the whole
//             EXPORT_DLLPUBLIC mess, and use this everywhere without
//             fretting about the minor perf hit of not using 'import'.
//
// Note that on Linux, all symbols are exported so this just isn't a
// problem, so we define these macros to be nothing.
//
// It's a shame that we have to clutter all our header files with these
// stupid macros just because the Windows world is such a mess.
//

#ifndef DLLEXPORT
#  if defined(_MSC_VER) && (defined(_WINDOWS) || defined(_WIN32))
#    define DLLEXPORT __declspec(dllexport)
#  else
#    define DLLEXPORT
#  endif
#endif

#ifndef DLLPUBLIC
#  if defined(_MSC_VER) && (defined(_WINDOWS) || defined(_WIN32))
#    ifdef DLL_EXPORT_PUBLIC
#      define DLLPUBLIC __declspec(dllexport)
#    else
#      define DLLPUBLIC __declspec(dllimport)
#    endif
#  else
#    define DLLPUBLIC
#  endif
#endif

#endif // EXPORT_H
