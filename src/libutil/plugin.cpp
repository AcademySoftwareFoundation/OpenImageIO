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

#include <cstdlib>
#include <string>

#include <OpenImageIO/platform.h>

#ifndef _WIN32
# include <dlfcn.h>
#endif

#include <OpenImageIO/thread.h>
#include <OpenImageIO/plugin.h>


OIIO_NAMESPACE_BEGIN

using namespace Plugin;

namespace {

static mutex plugin_mutex;
static std::string last_error;

}


const char *
Plugin::plugin_extension (void)
{
#if defined(_WIN32)
    return "dll";
#elif defined(__APPLE__)
    return "dylib";
#else
    return "so";
#endif
}

#if defined(_WIN32)

// Dummy values
#define RTLD_LAZY 0
#define RTLD_GLOBAL 0


Handle
dlopen (const char *plugin_filename, int)
{
    return LoadLibrary (plugin_filename);
}



bool
dlclose (Handle plugin_handle)
{
    return FreeLibrary ((HMODULE)plugin_handle) != 0;
}



void *
dlsym (Handle plugin_handle, const char *symbol_name)
{
    return GetProcAddress ((HMODULE)plugin_handle, symbol_name);
}



std::string
dlerror ()
{
    LPVOID lpMsgBuf;
    std::string win32Error;
    if (FormatMessageA( 
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR) &lpMsgBuf, 0, NULL))
        win32Error = (LPSTR)lpMsgBuf;
    LocalFree(lpMsgBuf);
    return win32Error;
}
#endif

Handle
Plugin::open (const char *plugin_filename, bool global)
{
    lock_guard guard (plugin_mutex);
    last_error.clear ();
    int mode = RTLD_LAZY;
    if (global)
        mode |= RTLD_GLOBAL;
    Handle h = dlopen (plugin_filename, mode);
    if (!h)
        last_error = dlerror();
    return h;
}



bool
Plugin::close (Handle plugin_handle)
{
    lock_guard guard (plugin_mutex);
    last_error.clear ();
    if (dlclose (plugin_handle)) {
        last_error = dlerror();
        return false;
    }
    return true;
}



void *
Plugin::getsym (Handle plugin_handle, const char *symbol_name)
{
    lock_guard guard (plugin_mutex);
    last_error.clear ();
    void *s = dlsym (plugin_handle, symbol_name);
    if (!s)
        last_error = dlerror();
    return s;
}


std::string
Plugin::geterror (void)
{
    lock_guard guard (plugin_mutex);
    std::string e = last_error;
    last_error.clear ();
    return e;
}

OIIO_NAMESPACE_END
