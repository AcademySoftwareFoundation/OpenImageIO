// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


/////////////////////////////////////////////////////////////////////////
/// \file
///
/// Helper routines for managing runtime-loadable "plugins", implemented
/// variously as DSO's (traditional Unix/Linux), dynamic libraries (Mac
/// OS X), DLL's (Windows).
/////////////////////////////////////////////////////////////////////////


#pragma once

#include <string>

#include <OpenImageIO/export.h>
#include <OpenImageIO/oiioversion.h>


OIIO_NAMESPACE_BEGIN

namespace Plugin {

typedef void* Handle;

/// Return the platform-dependent suffix for plug-ins ("dll" on
/// Windows, "so" on Linux and Mac OS X.
OIIO_UTIL_API const char*
plugin_extension(void);

/// Open the named plugin, return its handle.  If it could not be
/// opened, return 0 and the next call to geterror() will contain
/// an explanatory message.  If the 'global' parameter is true, all
/// symbols from the plugin will be available to the app (on Unix-like
/// platforms; this has no effect on Windows).
OIIO_UTIL_API Handle
open(const char* plugin_filename, bool global = true);

inline Handle
open(const std::string& plugin_filename, bool global = true)
{
    return open(plugin_filename.c_str(), global);
}

/// Close the open plugin with the given handle and return true upon
/// success.  If some error occurred, return false and the next call to
/// geterror() will contain an explanatory message.
OIIO_UTIL_API bool
close(Handle plugin_handle);

/// Get the address of the named symbol from the open plugin handle.  If
/// some error occurred, return nullptr and the next call to
/// geterror() will contain an explanatory message (unless report_error
/// is false, in which case the error message will be suppressed).
OIIO_UTIL_API void*
getsym(Handle plugin_handle, const char* symbol_name, bool report_error = true);

inline void*
getsym(Handle plugin_handle, const std::string& symbol_name,
       bool report_error = true)
{
    return getsym(plugin_handle, symbol_name.c_str(), report_error);
}

/// Return any error messages associated with the last call to any of
/// open, close, or getsym from the same thread.
OIIO_UTIL_API std::string
geterror(bool clear = true);



}  // namespace Plugin

OIIO_NAMESPACE_END
