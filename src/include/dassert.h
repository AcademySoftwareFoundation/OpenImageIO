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


#ifndef DASSERT_H
#define DASSERT_H


// Handy macros:
//   ABORT (if not already defined) is defined to print an error message
//           and then abort().
//   ASSERT (if not already defined) is defined to check if a condition
//           is met, and if not, calls ABORT with an error message 
//           indicating the module and line where it occurred.
//   DASSERT is the same as ASSERT when in DEBUG mode, but a no-op when
//           not in debug mode.
//
// The presumed usage is that you want ASSERT for dire conditions that 
// must be checked at runtime even in an optimized build.  DASSERT is
// for checks we should do for debugging, but that we don't want to 
// bother with in a shipping optimized build.
//
// In both cases, these are NOT a substitute for actual error checking
// and recovery!  Never ASSERT or DASSERT to check invalid user input,
// for examples.  They should be used only to verify that there aren't
// errors in the *code* that are so severe that there is no point even
// trying to recover gracefully.


#ifndef ABORT
# define ABORT(msg) fprintf(stderr,"%s",msg), abort()
#endif


#ifndef ASSERT
# define ASSERT(x)                                                      \
    if (!(x)) {                                                         \
        char buf[2048];                                                 \
        snprintf (buf, 4096, "Assertion failed in \"%s\", line %d\n"    \
                 "\tProbable bug in software.\n",                       \
                 __FILE__, __LINE__);                                   \
        ABORT (buf);                                                    \
    }                                                                   \
    else   // This 'else' exists to catch the user's following semicolon
#endif


#ifdef DEBUG
# define DASSERT(x) ASSERT(x)
#else
# define DASSERT(x) /* DASSERT does nothing when not debugging */
#endif


#endif // DASSERT_H
