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
/// @file  sysutil.h
///
/// @brief Platform-independent utilities for various OS, hardware, and
/// system resource functionality, all in namespace Sysutil.
/////////////////////////////////////////////////////////////////////////


#pragma once

#include <string>
#include <time.h>

#ifdef __MINGW32__
#include <malloc.h> // for alloca
#endif

#include "export.h"
#include "oiioversion.h"
#include "platform.h"
#include "string_view.h"


OIIO_NAMESPACE_BEGIN

/// @namespace  Sysutil
///
/// @brief Platform-independent utilities for various OS, hardware, and
/// system resource functionality.
namespace Sysutil {

/// The amount of memory currently being used by this process, in bytes.
/// If resident==true (the default), it will report just the resident
/// set in RAM; if resident==false, it returns the full virtual arena
/// (which can be misleading because gcc allocates quite a bit of
/// virtual, but not actually resident until malloced, memory per
/// thread).
OIIO_API size_t memory_used (bool resident=true);

/// The amount of physical RAM on this machine, in bytes.
/// If it can't figure it out, it will return 0.
OIIO_API size_t physical_memory ();

/// Convert calendar time pointed by 'time' into local time and save it in
/// 'converted_time' variable
OIIO_API void get_local_time (const time_t *time, struct tm *converted_time);

/// Return the full path of the currently-running executable program.
///
OIIO_API std::string this_program_path ();

/// Return the value of an environment variable (or the empty string_view
/// if it is not found in the environment.)
OIIO_API string_view getenv (string_view name);

/// Sleep for the given number of microseconds.
///
OIIO_API void usleep (unsigned long useconds);

/// Try to put the process into the background so it doesn't continue to
/// tie up any shell that it was launched from.  The arguments are the
/// argc/argv that describe the program and its command line arguments.
/// Return true if successful, false if it was unable to do so.
OIIO_API bool put_in_background (int argc, char* argv[]);

/// Number of virtual cores available on this platform (including
/// hyperthreads).
OIIO_API unsigned int hardware_concurrency ();

/// Number of full hardware cores available on this platform (does not
/// include hyperthreads). This is not always accurate and on some
/// platforms will return the number of virtual cores.
OIIO_API unsigned int physical_concurrency ();

/// Get the maximum number of open file handles allowed on this system.
OIIO_API size_t max_open_files ();

/// Try to figure out how many columns wide the terminal window is. May not
/// be correct on all systems, will default to 80 if it can't figure it out.
OIIO_API int terminal_columns ();

/// Try to figure out how many rows tall the terminal window is. May not be
/// correct on all systems, will default to 24 if it can't figure it out.
OIIO_API int terminal_rows ();


/// Term object encapsulates information about terminal output for the sake
/// of constructing ANSI escape sequences.
class OIIO_API Term {
public:
    /// Default ctr: assume ANSI escape sequences are ok.
    Term () : m_is_console(true) { }
    /// Construct from a FILE*: ANSI codes ok if the file describes a
    /// live console, otherwise they will be supressed.
    Term (FILE *file);
    /// Construct from a stream: ANSI codes ok if the file describes a
    /// live console, otherwise they will be supressed.
    Term (const std::ostream &stream);

    /// ansi("appearance") returns the ANSI escape sequence for the named
    /// command (if ANSI codes are ok, otherwise it will return the empty
    /// string). Accepted commands include: "default", "bold", "underscore",
    /// "blink", "reverse", "concealed", "black", "red", "green", "yellow",
    /// "blue", "magenta", "cyan", "white", "black_bg", "red_bg",
    /// "green_bg", "yellow_bg", "blue_bg", "magenta_bg", "cyan_bg",
    /// "white_bg". Commands may be combined with "," for example:
    /// "bold,green,white_bg".
    std::string ansi (string_view command) const;

    /// ansi("appearance", "text") returns the text, with the formatting
    /// command, then the text, then the formatting command to return to
    /// default appearance.
    std::string ansi (string_view command, string_view text) const {
        return std::string(ansi(command)) + std::string(text) + ansi("default");
    }

    /// Extended color control: take RGB values from 0-255
    std::string ansi_fgcolor (int r, int g, int b);
    std::string ansi_bgcolor (int r, int g, int b);

    bool is_console () const { return m_is_console; }
private:
    bool m_is_console;
};


}  // namespace Sysutils

OIIO_NAMESPACE_END
