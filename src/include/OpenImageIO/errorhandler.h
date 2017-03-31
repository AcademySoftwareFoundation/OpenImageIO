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


#ifndef OPENIMAGEIO_ERRORMANAGER_H
#define OPENIMAGEIO_ERRORMANAGER_H

#include <OpenImageIO/export.h>
#include <OpenImageIO/oiioversion.h>
#include <OpenImageIO/strutil.h>


OIIO_NAMESPACE_BEGIN

/// ErrorHandler is a simple class that accepts error messages
/// (classified as errors, severe errors, warnings, info, messages, or
/// debug output) and handles them somehow.  By default it just prints
/// the messages to stdout and/or stderr (and supresses some based on a
/// "verbosity" level).
/// 
/// The basic idea is that your library code has no idea whether some
/// application that will use it someday will want errors or other
/// output to be sent to the console, go to a log file, be intercepted
/// by the calling application, or something else.  So you punt, by
/// having your library take a pointer to an ErrorHandler, passed in
/// from the calling app (and possibly subclassed to have arbitrarily
/// different behavior from the default console output) and make all
/// error-like output via the ErrorHandler*.
///
class OIIO_API ErrorHandler {
public:
    /// Error categories.  We use broad categories in the high order bits.
    /// A library may just use these categories, or may create individual
    /// error codes as long as they have the right high bits to designate
    /// their category (file not found = ERROR + 1, etc.).  
    enum ErrCode {
        EH_NO_ERROR    = 0,    // never sent to handler
        EH_MESSAGE     = 0 << 16,
        EH_INFO        = 1 << 16,
        EH_WARNING     = 2 << 16,
        EH_ERROR       = 3 << 16,
        EH_SEVERE      = 4 << 16,
        EH_DEBUG       = 5 << 16
    };

    /// VerbosityLevel controls how much detail the calling app wants.
    ///
    enum VerbosityLevel {
        QUIET   = 0,  ///< Show MESSAGE, SEVERE, ERROR only
        NORMAL  = 1,  ///< Show MESSAGE, SEVERE, ERROR, WARNING
        VERBOSE = 2   ///< Like NORMAL, but also show INFO
    };

    ErrorHandler () : m_verbosity(NORMAL) { }
    virtual ~ErrorHandler () { }

    /// The main (or "full detail") method -- takes a code (with high
    /// bits being an ErrCode) and writes the message, with a prefix
    /// indicating the error category (no prefix for "MESSAGE") and
    /// error string.
    virtual void operator () (int errcode, const std::string &msg);

    /// Info message with printf-like formatted error message.
    /// Will not print unless verbosity >= VERBOSE.
    template<typename... Args>
    void info (string_view format, const Args&... args) {
        if (verbosity() >= VERBOSE)
            info (Strutil::format (format, args...));
    }

    /// Warning message with printf-like formatted error message.
    /// Will not print unless verbosity >= NORMAL (i.e. will suppress
    /// for QUIET).
    template<typename... Args>
    void warning (string_view format, const Args&... args) {
        if (verbosity() >= NORMAL)
            warning (Strutil::format (format, args...));
    }

    /// Error message with printf-like formatted error message.
    /// Will print regardless of verbosity.
    template<typename... Args>
    void error (string_view format, const Args&... args) {
        error (Strutil::format (format, args...));
    }

    /// Severe error message with printf-like formatted error message.
    /// Will print regardless of verbosity.
    template<typename... Args>
    void severe (string_view format, const Args&... args) {
        severe (Strutil::format (format, args...));
    }

    /// Prefix-less message with printf-like formatted error message.
    /// Will not print if verbosity is QUIET.  Also note that unlike
    /// the other routines, message() will NOT append a newline.
    template<typename... Args>
    void message (string_view format, const Args&... args) {
        if (verbosity() > QUIET)
            message (Strutil::format (format, args...));
    }

    /// Debugging message with printf-like formatted error message.
    /// This will not produce any output if not in DEBUG mode, or
    /// if verbosity is QUIET.
    template<typename... Args>
    void debug (string_view format, const Args&... args) {
#ifndef NDEBUG
        debug (Strutil::format (format, args...));
#endif
    }

    // Base cases
    void info    (const std::string &msg) { (*this)(EH_INFO, msg); }
    void warning (const std::string &msg) { (*this)(EH_WARNING, msg); }
    void error   (const std::string &msg) { (*this)(EH_ERROR, msg); }
    void severe  (const std::string &msg) { (*this)(EH_SEVERE, msg); }
    void message (const std::string &msg) { (*this)(EH_MESSAGE, msg); }
#ifndef NDEBUG
    void debug   (const std::string &msg) { (*this)(EH_DEBUG, msg); }
#else
    void debug   (const std::string &) { }
#endif

    /// Set desired verbosity level.
    ///
    void verbosity (int v) { m_verbosity = v; }

    /// Return the current verbosity level.
    ///
    int verbosity () const { return m_verbosity; }

    /// One built-in handler that can always be counted on to be present
    /// and just echoes the error messages to the console (stdout or
    /// stderr, depending on the error category).
    static ErrorHandler & default_handler ();

private:
    int m_verbosity;
};

OIIO_NAMESPACE_END

#endif /* !defined(OPENIMAGEIO_ERRORMANAGER_H) */
