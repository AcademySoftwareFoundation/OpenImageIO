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

#ifdef _WIN32
# include <windows.h>
#else
# include <dlfcn.h>
#endif

#include "thread.h"

#define DLL_EXPORT_PUBLIC /* Because we are implementing Plugin */
#include "plugin.h"
#undef DLL_EXPORT_PUBLIC


using namespace Plugin;

// FIXME: this implementation doesn't set error messages for Windows.
// Get a Windows expert to fix this.

static mutex plugin_mutex;
static std::string last_error;



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



Handle
Plugin::open (const char *plugin_filename)
{
    lock_guard guard (plugin_mutex);
    last_error.clear ();
#if defined(_WIN32)
    return LoadLibrary (plugin_filename);
#else
    Handle h = dlopen (plugin_filename, RTLD_LAZY | RTLD_GLOBAL);
    if (!h)
        last_error = dlerror();
    return h;
#endif
}



bool
Plugin::close (Handle plugin_handle)
{
    lock_guard guard (plugin_mutex);
    last_error.clear ();
#if defined(_WIN32)
    FreeLibrary ((HMODULE)plugin_handle);
#else
    if (dlclose (plugin_handle)) {
        last_error = dlerror();
        return false;
    }
#endif
    return true;
}



void *
Plugin::getsym (Handle plugin_handle, const char *symbol_name)
{
    lock_guard guard (plugin_mutex);
    last_error.clear ();
#if defined(_WIN32)
    return GetProcAddress ((HMODULE)plugin_handle, symbol_name);
#else
    void *s = dlsym (plugin_handle, symbol_name);
    if (!s)
        last_error = dlerror();
    return s;
#endif
}


std::string
Plugin::error_message (void)
{
    lock_guard guard (plugin_mutex);
    return last_error;
}
