// OpenImageIO Copyright 2008- Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md


#pragma once

#include <cassert>
#include <cstdio>
#include <cstdlib>

#include <OpenImageIO/platform.h>


/// \file
///
/// Handy macros for debugging assertions.
///
///  - ASSERT (if not already defined) is defined to check if a condition
///            is met, and if not, calls ABORT with an error message
///            indicating the module and line where it occurred.
///  - ASSERT_MSG: like ASSERT, but takes printf-like extra arguments
///  - DASSERT is the same as ASSERT when NDEBUG is not defined but a
///            no-op when not in debug mode.
///  - DASSERT_MSG: like DASSERT, but takes printf-like extra arguments
///  - OIIO_STATIC_ASSERT(cond) : static assertion
///  - OIIO_STATIC_ASSERT_MSG(cond,msg) : static assertion + message
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


/// ASSERT(condition) checks if the condition is met, and if not, prints
/// an error message indicating the module and line where the error
/// occurred and then aborts. Beware: this happens even for release/NODEBUG
/// builds!

#ifndef ASSERT
#    define ASSERT(x)                                                          \
        (OIIO_LIKELY(x)                                                        \
             ? ((void)0)                                                       \
             : (std::fprintf(stderr, "%s:%u: %s: Assertion '%s' failed.\n",    \
                             __FILE__, __LINE__, OIIO_PRETTY_FUNCTION, #x),    \
                abort()))
#endif

/// ASSERT_MSG(condition,msg,...) is like ASSERT, but lets you add
/// formatted output (a la printf) to the failure message.
#ifndef ASSERT_MSG
#    define ASSERT_MSG(x, msg, ...)                                            \
        (OIIO_LIKELY(x)                                                        \
             ? ((void)0)                                                       \
             : (std::fprintf(stderr,                                           \
                             "%s:%u: %s: Assertion '%s' failed: " msg "\n",    \
                             __FILE__, __LINE__, OIIO_PRETTY_FUNCTION, #x,     \
                             __VA_ARGS__),                                     \
                abort()))
#endif

#ifndef ASSERTMSG
#    define ASSERTMSG ASSERT_MSG
#endif


/// DASSERT(condition) is just an alias for the usual assert() macro.
/// It does nothing when in a non-DEBUG (optimized, shipping) build.
#ifndef NDEBUG
#    define DASSERT(x) assert(x)
#else
/* DASSERT does nothing when not debugging; sizeof trick prevents warnings */
#    define DASSERT(x) ((void)sizeof(x)) /*NOLINT*/
#endif

/// DASSERT_MSG(condition,msg,...) is just like ASSERT_MSG, except that it
/// only is functional in DEBUG mode, but does nothing when in a
/// non-DEBUG (optimized, shipping) build.
#ifndef NDEBUG
#    define DASSERT_MSG ASSERT_MSG
#else
/* does nothing when not debugging */
#    define DASSERT_MSG(x, ...) ((void)sizeof(x)) /*NOLINT*/
#endif

#ifndef DASSERTMSG
#    define DASSERTMSG DASSERT_MSG
#endif



/// Define OIIO_STATIC_ASSERT and OIIO_STATIC_ASSERT_MSG as wrappers around
/// static_assert and static_assert_msg, with appropriate fallbacks for
/// older C++ standards.
#if (__cplusplus >= 201700L) /* FIXME - guess the token, fix when C++17 */
#    define OIIO_STATIC_ASSERT(cond) static_assert(cond)
#    define OIIO_STATIC_ASSERT_MSG(cond, msg) static_assert(cond, msg)
#else /* (__cplusplus >= 201103L) */
#    define OIIO_STATIC_ASSERT(cond) static_assert(cond, "")
#    define OIIO_STATIC_ASSERT_MSG(cond, msg) static_assert(cond, msg)
#endif
