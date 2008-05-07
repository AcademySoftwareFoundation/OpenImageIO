/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2008 Larry Gritz
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
// (this is the MIT license)
/////////////////////////////////////////////////////////////////////////////


#include <cstdlib>
#include <string>

#ifdef WINDOWS
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



Handle
Plugin::open (const char *plugin_filename)
{
    lock_guard guard (plugin_mutex);
    last_error.clear ();
#if defined(WINDOWS)
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
#if defined(WINDOWS)
    FreeLibrary (plugin_handle);
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
#if defined(WINDOWS)
    return GetProcAddress (plugin_handle, symbol_name);
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
