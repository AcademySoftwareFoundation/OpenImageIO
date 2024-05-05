// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <OpenImageIO/platform.h>

OIIO_PRAGMA_WARNING_PUSH
OIIO_CLANG_PRAGMA(clang diagnostic ignored "-Wdeprecated-declarations")
// Force include of locale header with deprecated warnings turned off to
// combat clang18 vs unicode deprecation warnings.
#include <locale>
OIIO_PRAGMA_WARNING_POP

#include <cstdlib>
#include <string>

#ifdef _WIN32
#    include <windows.h>
#else
#    include <dlfcn.h>
#endif

#include <OpenImageIO/plugin.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/thread.h>


OIIO_NAMESPACE_BEGIN

using namespace Plugin;

namespace {

static mutex plugin_mutex;
static thread_local std::string last_error;

}  // namespace


const char*
Plugin::plugin_extension(void)
{
#if defined(_WIN32)
    return "dll";
#else
    return "so";
#endif
}

#if defined(_WIN32)

// Dummy values
#    define RTLD_LAZY 0
#    define RTLD_GLOBAL 0


Handle
dlopen(const char* plugin_filename, int)
{
    std::wstring w = Strutil::utf8_to_utf16wstring(plugin_filename);
    return LoadLibraryW(w.c_str());
}



bool
dlclose(Handle plugin_handle)
{
    return FreeLibrary((HMODULE)plugin_handle) != 0;
}



void*
dlsym(Handle plugin_handle, const char* symbol_name)
{
    return (void*)GetProcAddress((HMODULE)plugin_handle, symbol_name);
}



std::string
dlerror()
{
    LPVOID lpMsgBuf;
    std::string win32Error;
    if (FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER
                           | FORMAT_MESSAGE_FROM_SYSTEM
                           | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, GetLastError(),
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       (LPSTR)&lpMsgBuf, 0, NULL))
        win32Error = (LPSTR)lpMsgBuf;
    LocalFree(lpMsgBuf);
    return win32Error;
}
#endif

Handle
Plugin::open(const char* plugin_filename, bool global)
{
    lock_guard guard(plugin_mutex);
    last_error.clear();
    int mode = RTLD_LAZY;
    if (global)
        mode |= RTLD_GLOBAL;
    Handle h = dlopen(plugin_filename, mode);
    if (!h)
        last_error = dlerror();
    return h;
}



bool
Plugin::close(Handle plugin_handle)
{
    lock_guard guard(plugin_mutex);
    last_error.clear();
    if (dlclose(plugin_handle)) {
        last_error = dlerror();
        return false;
    }
    return true;
}



void*
Plugin::getsym(Handle plugin_handle, const char* symbol_name, bool report_error)
{
    lock_guard guard(plugin_mutex);
    last_error.clear();
    void* s = dlsym(plugin_handle, symbol_name);
    if (!s && report_error)
        last_error = dlerror();
    return s;
}


std::string
Plugin::geterror(bool clear)
{
    std::string e = last_error;
    if (clear)
        last_error.clear();
    return e;
}

OIIO_NAMESPACE_END
