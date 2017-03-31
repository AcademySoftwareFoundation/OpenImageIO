/*
  Copyright 2009 Larry Gritz and the other authors and contributors.
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

#include <cstdio>
#include <iostream>
#include <string>

#include <OpenImageIO/strutil.h>
#include <OpenImageIO/errorhandler.h>
#include <OpenImageIO/thread.h>



OIIO_NAMESPACE_BEGIN



namespace {
static ErrorHandler default_handler_instance;
static mutex err_mutex;
}


ErrorHandler &
ErrorHandler::default_handler ()
{
    return default_handler_instance;
}



void
ErrorHandler::operator() (int errcode, const std::string &msg)
{
    lock_guard guard (err_mutex);
    switch (errcode & 0xffff0000) {
    case EH_INFO :
        if (verbosity() >= VERBOSE)
            fprintf (stdout, "INFO: %s\n", msg.c_str());
        break;
    case EH_WARNING :
        if (verbosity() >= NORMAL)
            fprintf (stderr, "WARNING: %s\n", msg.c_str());
        break;
    case EH_ERROR :
        fprintf (stderr, "ERROR: %s\n", msg.c_str());
        break;
    case EH_SEVERE :
        fprintf (stderr, "SEVERE ERROR: %s\n", msg.c_str());
        break;
    case EH_DEBUG :
#ifdef NDEBUG
        break;
#endif
    default :
        if (verbosity() > QUIET)
            fprintf (stdout, "%s", msg.c_str());
        break;
    }
    fflush (stdout);
    fflush (stderr);
}

OIIO_NAMESPACE_END
