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


/////////////////////////////////////////////////////////////////////////
/// \file
///
/// Helper routines for managing runtime-loadable "plugins", implemented
/// variously as DSO's (traditional Unix/Linux), dynamic libraries (Mac
/// OS X), DLL's (Windows).
/////////////////////////////////////////////////////////////////////////


#ifndef OPENIMAGEIO_PLUGIN_H
#define OPENIMAGEIO_PLUGIN_H

#include <string>

#include "export.h"
#include "oiioversion.h"


OIIO_NAMESPACE_ENTER
{

namespace Plugin {

typedef void * Handle;

/// Return the platform-dependent suffix for plug-ins ("dll" on
/// Windows, "so" on Linux, "dylib" on Mac OS X.
OIIO_API const char *plugin_extension (void);

/// Open the named plugin, return its handle.  If it could not be
/// opened, return 0 and the next call to geterror() will contain
/// an explanatory message.  If the 'global' parameter is true, all
/// symbols from the plugin will be available to the app (on Unix-like
/// platforms; this has no effect on Windows).
OIIO_API Handle open (const char *plugin_filename, bool global=true);

inline Handle
open (const std::string &plugin_filename, bool global=true)
{
    return open (plugin_filename.c_str(), global);
}

/// Close the open plugin with the given handle and return true upon
/// success.  If some error occurred, return false and the next call to
/// geterror() will contain an explanatory message.
OIIO_API bool close (Handle plugin_handle);

/// Get the address of the named symbol from the open plugin handle.  If
/// some error occurred, return NULL and the next call to
/// geterror() will contain an explanatory message.
OIIO_API void * getsym (Handle plugin_handle, const char *symbol_name);

inline void *
getsym (Handle plugin_handle, const std::string &symbol_name)
{
    return getsym (plugin_handle, symbol_name.c_str());
}

/// Return any error messages associated with the last call to any of
/// open, close, or getsym.  Note that in a multithreaded environment,
/// it's up to the caller to properly mutex to ensure that no other
/// thread has called open, close, or getsym (all of which clear or
/// overwrite the error message) between the error-generating call and
/// geterror.
OIIO_API std::string geterror (void);



}  // namespace Plugin

}
OIIO_NAMESPACE_EXIT

#endif // OPENIMAGEIO_PLUGIN_H
