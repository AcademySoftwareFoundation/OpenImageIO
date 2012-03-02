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


#ifndef OPENIMAGEIO_DASSERT_H
#define OPENIMAGEIO_DASSERT_H

#include <cstdio>
#include <cstdlib>


/// \file
///
/// Handy macros for debugging assertions.
///
///  - ABORT (if not already defined) is defined to print an error message
///            and then abort().
///  - ASSERT (if not already defined) is defined to check if a condition
///            is met, and if not, calls ABORT with an error message
///            indicating the module and line where it occurred.
///  - DASSERT is the same as ASSERT when in DEBUG mode, but a no-op when
///            not in debug mode.
///
/// The presumed usage is that you want ASSERT for dire conditions that
/// must be checked at runtime even in an optimized build.  DASSERT is
/// for checks we should do for debugging, but that we don't want to
/// bother with in a shipping optimized build.
///
/// In both cases, these are NOT a substitute for actual error checking
/// and recovery!  Never ASSERT or DASSERT to check invalid user input,
/// for example.  They should be used only to verify that there aren't
/// errors in the *code* that are so severe that there is no point even
/// trying to recover gracefully.


/// ABORT(msg) prints the message to stderr and then aborts.
///
#ifndef ABORT
# define ABORT(msg) fprintf(stderr,"%s",msg), abort()
#endif


/// ASSERT(condition) checks if the condition is met, and if not, prints
/// an error message indicating the module and line where the error
/// occurred and then aborts.

#ifndef ASSERT
# define ASSERT(x)                                              \
    ((x) ? ((void)0)                                            \
         : (fprintf (stderr, "%s:%u: Failed assertion '%s'\n",  \
                     __FILE__, __LINE__, #x), abort()))
#endif

/// ASSERTMSG(condition,msg,...) is like ASSERT, but lets you add
/// formatted output (a la printf) to the failure message.
#ifndef ASSERTMSG
# define ASSERTMSG(x,msg,...)                                       \
    ((x) ? ((void)0)                                                \
         : (fprintf (stderr, "%s:%u: Failed assertion '" #x "': " msg "\n",  \
                     __FILE__, __LINE__, __VA_ARGS__), abort()))
#endif


/// DASSERT(condition) is just like ASSERT, except that it only is
/// functional in DEBUG mode, but does nothing when in a non-DEBUG
/// (optimized, shipping) build.
#ifdef DEBUG
# define DASSERT(x) ASSERT(x)
#else
# define DASSERT(x) ((void)0) /* DASSERT does nothing when not debugging */
#endif

/// DASSERTMSG(condition,msg,...) is just like ASSERTMSG, except that it
/// only is functional in DEBUG mode, but does nothing when in a
/// non-DEBUG (optimized, shipping) build.
#ifdef DEBUG
# define DASSERTMSG ASSERTMSG
#else
# define DASSERTMSG(...) ((void)0) /* does nothing when not debugging */
#endif


#endif // OPENIMAGEIO_DASSERT_H
