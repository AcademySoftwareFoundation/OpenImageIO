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


// A variety of helper routines for runtime-loadable "plugins",
// implemented variously as DSO's (traditional Unix/Linux), dynamic
// libraries (Mac OS X), DLL's (Windows).


#ifndef PLUGIN_H
#define PLUGIN_H

#include "export.h"



namespace Plugin {

typedef void * Handle;

/// Open the named plugin, return its handle.  If it could not be
/// opened, return 0 and the next call to error_message() will contain
/// an explanatory message.
DLLPUBLIC Handle open (const char *plugin_filename);

Handle open (const std::string &plugin_filename) {
    return open (plugin_filename.c_str());
}

/// Close the open plugin with the given handle and return true upon
/// success.  If some error occurred, return false and the next call to
/// error_message() will contain an explanatory message.
DLLPUBLIC bool close (Handle plugin_handle);

/// Get the address of the named symbol from the open plugin handle.  If
/// some error occurred, return NULL and the next call to
/// error_message() will contain an explanatory message.
DLLPUBLIC void * getsym (Handle plugin_handle, const char *symbol_name);

void * getsym (Handle plugin_handle, const std::string &symbol_name) {
    return getsym (plugin_handle, symbol_name.c_str());
}

/// Return any error messages associated with the last call to any of
/// open, close, or getsym.  Note that in a multithreaded environment,
/// it's up to the caller to properly mutex to ensure that no other
/// thread has called open, close, or getsym (all of which clear or
/// overwrite the error message) between the error-generating call and
/// error_message.
std::string error_message (void);



DLLPUBLIC
class DsoLoader {
public:
    DsoLoader () { }
    ~DsoLoader () { }
private:
};


};  // namespace Plugin

#endif // PLUGIN_H

