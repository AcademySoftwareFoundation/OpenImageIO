// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <cstdio>
#include <iostream>
#include <string>

#include <OpenImageIO/errorhandler.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/thread.h>



OIIO_NAMESPACE_BEGIN



namespace {
static ErrorHandler default_handler_instance;
static mutex err_mutex;
}  // namespace


ErrorHandler&
ErrorHandler::default_handler()
{
    return default_handler_instance;
}



void
ErrorHandler::operator()(int errcode, const std::string& msg)
{
    lock_guard guard(err_mutex);
    switch (errcode & 0xffff0000) {
    case EH_INFO:
        if (verbosity() >= VERBOSE)
            print("INFO: {}\n", msg);
        break;
    case EH_WARNING:
        if (verbosity() >= NORMAL)
            print(stderr, "WARNING: {}\n", msg);
        break;
    case EH_ERROR: print(stderr, "ERROR: {}\n", msg); break;
    case EH_SEVERE: print(stderr, "SEVERE ERROR: {}\n", msg); break;
    case EH_DEBUG:
#ifdef NDEBUG
        break;
#endif
    default:
        if (verbosity() > QUIET)
            print("{}", msg);
        break;
    }
    fflush(stdout);
    fflush(stderr);
}

OIIO_NAMESPACE_END
