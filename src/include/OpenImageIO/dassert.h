// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#pragma once

#include <cassert>
#include <cstdio>
#include <cstdlib>

#include <OpenImageIO/oiioversion.h>
#include <OpenImageIO/platform.h>



// General resources about security and hardening for C++:
//
// https://best.openssf.org/Compiler-Hardening-Guides/Compiler-Options-Hardening-Guide-for-C-and-C++.html
// https://www.gnu.org/software/libc/manual/html_node/Source-Fortification.html
// https://gcc.gnu.org/onlinedocs/libstdc++/manual/using_macros.html
// https://libcxx.llvm.org/Hardening.html
// https://cheatsheetseries.owasp.org/cheatsheets/C-Based_Toolchain_Hardening_Cheat_Sheet.html
// https://stackoverflow.com/questions/13544512/what-is-the-most-hardened-set-of-options-for-gcc-compiling-c-c
// https://medium.com/@simontoth/daily-bit-e-of-c-hardened-mode-of-standard-library-implementations-18be2422c372
// https://en.cppreference.com/w/cpp/contract
// https://en.cppreference.com/w/cpp/language/contracts



// Define hardening levels for OIIO: which checks should we do?
// NONE - YOLO mode, no extra checks (not recommended)
// FAST - Minimal checks that have low performance impact
// EXTENSIVE - More thorough checks, may impact performance
// DEBUG - Maximum checks, for debugging purposes
#define OIIO_HARDENING_NONE 0
#define OIIO_HARDENING_FAST 1
#define OIIO_HARDENING_EXTENSIVE 2
#define OIIO_HARDENING_DEBUG 3

// OIIO_HARDENING_DEFAULT defines the default hardening level we actually use.
// By default, we use NONE for release builds and DEBUG for debug builds. But
// any translation unit (including clients of OIIO) may override this by
// defining OIIO_HARDENING_DEFAULT before including any OIIO headers. But note
// that this only affects calls to inline functions or templates defined in
// the headers. Non-inline functions compiled into the OIIO library, including
// OIIO internal code itself, will have been compiled with whatever hardening
// level was selected when the library was built.
#ifndef OIIO_HARDENING_DEFAULT
#    ifdef NDEBUG
#        define OIIO_HARDENING_DEFAULT OIIO_HARDENING_NONE
#    else
#        define OIIO_HARDENING_DEFAULT OIIO_HARDENING_DEBUG
#    endif
#endif


/// Choices for what to do when a contract assertion fails.
/// This mimics the C++26 standard's std::contract behavior.
#define OIIO_ASSERTION_RESPONSE_IGNORE 0
#define OIIO_ASSERTION_RESPONSE_OBSERVE 1
#define OIIO_ASSERTION_RESPONSE_ENFORCE 2
#define OIIO_ASSERTION_RESPONSE_QUICK_ENFORCE 3

// OIIO_ASSERTION_RESPONSE_DEFAULT defines the default response to failed
// contract assertions. By default, in NONE hardening mode and in release
// builds, we do nothing. In all other cases, we abort. But any translation
// unit (including clients of OIIO) may override this by defining
// OIIO_ASSERTION_RESPONSE_DEFAULT before including any OIIO headers. But note
// that this only affects calls to inline functions or templates defined in
// the headers. Non-inline functions compiled into the OIIO library, including
// OIIO internal code itself, will have been compiled with whatever response
// was selected when the library was built.
#ifndef OIIO_ASSERTION_RESPONSE_DEFAULT
#    if OIIO_HARDENING_DEFAULT == OIIO_HARDENING_NONE && defined(NDEBUG)
#        define OIIO_ASSERTION_RESPONSE_DEFAULT OIIO_ASSERTION_RESPONSE_ENFORCE
#    else
#        define OIIO_ASSERTION_RESPONSE_DEFAULT OIIO_ASSERTION_RESPONSE_ENFORCE
#    endif
#endif



// `OIIO_CONTRACT_ASSERT(condition)` checks if the condition is met, and if
// not, calls the contract violation handler with the appropriate response.
// `OIIO_CONTRACT_ASSERT_MSG(condition, msg)` is the same, but allows a
// different message to be passed to the handler.
#if OIIO_ASSERTION_RESPONSE_DEFAULT == OIIO_ASSERTION_RESPONSE_IGNORE
#    define OIIO_CONTRACT_ASSERT_MSG(condition, message) (void)0
#elif OIIO_ASSERTION_RESPONSE_DEFAULT == OIIO_ASSERTION_RESPONSE_QUICK_ENFORCE
#    define OIIO_CONTRACT_ASSERT_MSG(condition, message) \
        (OIIO_LIKELY(condition) ? ((void)0) : (std::abort(), (void)0))
#elif OIIO_ASSERTION_RESPONSE_DEFAULT == OIIO_ASSERTION_RESPONSE_OBSERVE
#    define OIIO_CONTRACT_ASSERT_MSG(condition, message)                      \
        (OIIO_LIKELY(condition) ? ((void)0)                                   \
                                : (OIIO::contract_violation_handler(          \
                                       __FILE__ ":" OIIO_STRINGIZE(__LINE__), \
                                       OIIO_PRETTY_FUNCTION, message),        \
                                   (void)0))
#elif OIIO_ASSERTION_RESPONSE_DEFAULT == OIIO_ASSERTION_RESPONSE_ENFORCE
#    define OIIO_CONTRACT_ASSERT_MSG(condition, message)                      \
        (OIIO_LIKELY(condition) ? ((void)0)                                   \
                                : (OIIO::contract_violation_handler(          \
                                       __FILE__ ":" OIIO_STRINGIZE(__LINE__), \
                                       OIIO_PRETTY_FUNCTION, message),        \
                                   std::abort(), (void)0))
#else
#    error "Unknown OIIO_ASSERTION_RESPONSE_DEFAULT"
#endif

#define OIIO_CONTRACT_ASSERT(condition) \
    OIIO_CONTRACT_ASSERT_MSG(condition, #condition)

// Macros to use to select whether or not to do a contract check, based on the
// hardening level:
// - OIIO_HARDENING_ASSERT_FAST : only checks contract for >= FAST hardening.
// - OIIO_HARDENING_ASSERT_EXTENSIVE : only checks contract for >= EXTENSIVE.
// - OIIO_HARDENING_ASSERT_DEBUG : only checks contract for DEBUG hardening.
#if OIIO_HARDENING_DEFAULT >= OIIO_HARDENING_FAST
#    define OIIO_HARDENING_ASSERT_FAST_MSG(condition, message) \
        OIIO_CONTRACT_ASSERT_MSG(condition, message)
#else
#    define OIIO_HARDENING_ASSERT_FAST_MSG(...) (void)0
#endif

#if OIIO_HARDENING_DEFAULT >= OIIO_HARDENING_EXTENSIVE
#    define OIIO_HARDENING_ASSERT_EXTENSIVE_MSG(condition, message) \
        OIIO_CONTRACT_ASSERT_MSG(condition, message)
#else
#    define OIIO_HARDENING_ASSERT_EXTENSIVE_MSG(...) (void)0
#endif

#if OIIO_HARDENING_DEFAULT >= OIIO_HARDENING_DEBUG
#    define OIIO_HARDENING_ASSERT_DEBUG_MSG(condition, message) \
        OIIO_CONTRACT_ASSERT_MSG(condition, message)
#else
#    define OIIO_HARDENING_ASSERT_DEBUG_MSG(...) (void)0
#endif

#define OIIO_HARDENING_ASSERT_NONE(condition) \
    OIIO_HARDENING_ASSERT_NONE_MSG(condition, #condition)
#define OIIO_HARDENING_ASSERT_FAST(condition) \
    OIIO_HARDENING_ASSERT_FAST_MSG(condition, #condition)
#define OIIO_HARDENING_ASSERT_EXTENSIVE(condition) \
    OIIO_HARDENING_ASSERT_EXTENSIVE_MSG(condition, #condition)
#define OIIO_HARDENING_ASSERT_DEBUG(condition) \
    OIIO_HARDENING_ASSERT_DEBUG_MSG(condition, #condition)


OIIO_NAMESPACE_3_1_BEGIN
// Internal contract assertion handler
OIIO_UTIL_API void
contract_violation_handler(const char* location, const char* function,
                           const char* msg = "");
OIIO_NAMESPACE_3_1_END

OIIO_NAMESPACE_BEGIN
#ifndef OIIO_DOXYGEN
using v3_1::contract_violation_handler;
#endif
OIIO_NAMESPACE_END


/// OIIO_ABORT_IF_DEBUG is a call to abort() for debug builds, but does
/// nothing for release builds.
#ifndef NDEBUG
#    define OIIO_ABORT_IF_DEBUG abort()
#else
#    define OIIO_ABORT_IF_DEBUG (void)0
#endif


/// OIIO_ASSERT(condition) checks if the condition is met, and if not,
/// prints an error message indicating the module and line where the error
/// occurred, and additionally aborts if in debug mode. When in release
/// mode, it prints the error message if the condition fails, but does not
/// abort.
///
/// OIIO_ASSERT_MSG(condition,msg,...) lets you add formatted output (a la
/// printf) to the failure message.
#define OIIO_ASSERT(x)                                                  \
    (OIIO_LIKELY(x)                                                     \
         ? ((void)0)                                                    \
         : (std::fprintf(stderr, "%s:%u: %s: Assertion '%s' failed.\n", \
                         __FILE__, __LINE__, OIIO_PRETTY_FUNCTION, #x), \
            OIIO_ABORT_IF_DEBUG))
#define OIIO_ASSERT_MSG(x, msg, ...)                                            \
    (OIIO_LIKELY(x)                                                             \
         ? ((void)0)                                                            \
         : (std::fprintf(stderr, "%s:%u: %s: Assertion '%s' failed: " msg "\n", \
                         __FILE__, __LINE__, OIIO_PRETTY_FUNCTION, #x,          \
                         __VA_ARGS__),                                          \
            OIIO_ABORT_IF_DEBUG))


/// OIIO_DASSERT and OIIO_DASSERT_MSG are the same as OIIO_ASSERT for debug
/// builds (test, print error, abort), but do nothing at all in release
/// builds (not even perform the test). This is similar to C/C++ assert(),
/// but gives us flexibility in improving our error messages. It is also ok
/// to use regular assert() for this purpose if you need to eliminate the
/// dependency on this header from a particular place (and don't mind that
/// assert won't format identically on all platforms).
#ifndef NDEBUG
#    define OIIO_DASSERT OIIO_ASSERT
#    define OIIO_DASSERT_MSG OIIO_ASSERT_MSG
#else
#    define OIIO_DASSERT(x) ((void)sizeof(x))          /*NOLINT*/
#    define OIIO_DASSERT_MSG(x, ...) ((void)sizeof(x)) /*NOLINT*/
#endif


/// Define OIIO_STATIC_ASSERT(cond) as a wrapper around static_assert(cond),
/// with appropriate fallbacks for older C++ standards.
#if (__cplusplus >= 201700L) /* FIXME - guess the token, fix when C++17 */
#    define OIIO_STATIC_ASSERT(cond) static_assert(cond)
#else
#    define OIIO_STATIC_ASSERT(cond) static_assert(cond, "")
#endif

/// Deprecated synonym:
#define OIIO_STATIC_ASSERT_MSG(cond, msg) static_assert(cond, msg)
