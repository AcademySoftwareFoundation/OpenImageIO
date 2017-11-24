/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
  All Rights Reserved.
  Based on BSD-licensed software Copyright 2004 NVIDIA Corp.

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


/// \file
/// \brief Simple parsing of program command-line arguments.


#ifndef OPENIMAGEIO_ARGPARSE_H
#define OPENIMAGEIO_ARGPARSE_H

#if defined(_MSC_VER)
// Ignore warnings about DLL exported classes with member variables that are template classes.
// This happens with the std::string m_errmessage member of ArgParse below.
#  pragma warning (disable : 4251)
#endif

#include <vector>

#include <OpenImageIO/export.h>
#include <OpenImageIO/oiioversion.h>
#include <OpenImageIO/strutil.h>


OIIO_NAMESPACE_BEGIN


class ArgOption;   // Forward declaration



/////////////////////////////////////////////////////////////////////////////
///
/// \class ArgParse
///
/// Argument Parsing
///
/// The parse function takes a list of options and variables or functions
/// for storing option values and return <0 on failure:
///
/// \code
///    static int parse_files (int argc, const char *argv[])
///    {
///        for (int i = 0;  i < argc;  i++)
///            filenames.push_back (argv[i]);
///        return 0;
///    }
///
///    static int blah_callback (int argc, const char *argv[])
///    {
///        std::cout << "blah argument was " << argv[1] << "\n";
///        return 0;
///    }
/// 
///    ...
///
///    ArgParse ap;
///
///    ap.options ("Usage: myapp [options] filename...",
///            "%*", parse_objects, "",
///            "-camera %f %f %f", &camera[0], &camera[1], &camera[2],
///                  "set the camera position",
///            "-lookat %f %f %f", &lx, &ly, &lz,
///                  "set the position of interest",
///            "-oversampling %d", &oversampling,  "oversamping rate",
///            "-passes %d", &passes, "number of passes",
///            "-lens %f %f %f", &aperture, &focalDistance, &focalLength,
///                   "set aperture, focal distance, focal length",
///            "-format %d %d %f", &width, &height, &aspect,
///                   "set width, height, aspect ratio",
///            "-v", &verbose, "verbose output",
///            "-q %!", &verbose, "quiet mode",
///            "--blah %@ %s", blahcallback, "Make the callback",
///            NULL);
///
///    if (ap.parse (argc, argv) < 0) {
///        std::cerr << ap.geterror() << std::endl;
///        ap.usage ();
///        return EXIT_FAILURE;
///    }
/// \endcode
///
/// The available argument types are:
///    - no \% argument - bool flag
///    - \%! - a bool flag, but set it to false if the option is set
///    - \%d - 32bit integer
///    - \%f - 32bit float
///    - \%F - 64bit float (double)
///    - \%s - std::string
///    - \%L - std::vector<std::string>  (takes 1 arg, appends to list)
///    - \%@ - a function pointer for a callback function will be invoked
///            immediately.  The prototype for the callback is
///                  int callback (int argc, char *argv[])
///    - \%* - catch all non-options and pass individually as an (argc,argv) 
///            sublist to a callback, each immediately after it's found
///
/// There are several special format tokens:
///    - "<SEPARATOR>" - not an option at all, just a description to print
///                     in the usage output.
///
/// Notes:
///   - If an option doesn't have any arguments, a bool flag argument is
///     assumed.
///   - No argument destinations are initialized.
///   - The empty string, "", is used as a global sublist (ie. "%*").
///   - Sublist functions are all of the form "int func(int argc, char **argv)".
///   - If a sublist function returns -1, parse() will terminate early.
///   - It is perfectly legal for the user to append ':' and more characters
///     to the end of an option name, it will match only the portion before
///     the semicolon (but a callback can detect the full string, this is
///     useful for making arguments:  myprog --flag:myopt=1 foobar
///
/////////////////////////////////////////////////////////////////////////////


class OIIO_API ArgParse {
public:
    ArgParse (int argc=0, const char **argv=NULL);
    ~ArgParse ();

    /// Declare the command line options.  After the introductory
    /// message, parameters are a set of format strings and variable
    /// pointers.  Each string contains an option name and a scanf-like
    /// format string to enumerate the arguments of that option
    /// (eg. "-option %d %f %s").  The format string is followed by a
    /// list of pointers to the argument variables, just like scanf.  A
    /// NULL terminates the list.  Multiple calls to options() will
    /// append additional options.
    int options (const char *intro, ...);

    /// With the options already set up, parse the command line.
    /// Return 0 if ok, -1 if it's a malformed command line.
    int parse (int argc, const char **argv);

    /// Return any error messages generated during the course of parse()
    /// (and clear any error flags).  If no error has occurred since the
    /// last time geterror() was called, it will return an empty string.
    std::string geterror () const;

    /// Print the usage message to stdout.  The usage message is
    /// generated and formatted automatically based on the command and
    /// description arguments passed to parse().
    void usage () const;

    /// Print a brief usage message to stdout.  The usage message is
    /// generated and formatted automatically based on the command and
    /// description arguments passed to parse().
    void briefusage () const;

    /// Return the entire command-line as one string.
    ///
    std::string command_line () const;

    // Type for a callback that writes something to the output stream.
    typedef std::function<void(const ArgParse& ap, std::ostream&)> callback_t;

    // Set callbacks to run that will print any matter you want as part
    // of the verbose usage, before and after the options are detailed.
    void set_preoption_help (callback_t callback) {
        m_preoption_help = callback;
    }
    void set_postoption_help (callback_t callback) {
        m_postoption_help = callback;
    }

private:
    int m_argc;                           // a copy of the command line argc
    const char **m_argv;                  // a copy of the command line argv
    mutable std::string m_errmessage;     // error message
    ArgOption *m_global;                  // option for extra cmd line arguments
    std::string m_intro;
    std::vector<ArgOption *> m_option;
    callback_t m_preoption_help = [](const ArgParse& ap, std::ostream&){};
    callback_t m_postoption_help = [](const ArgParse& ap, std::ostream&){};

    ArgOption *find_option(const char *name);

    template<typename... Args>
    void error (string_view fmt, const Args&... args) const {
        m_errmessage = Strutil::format (fmt, args...);
    }

    int found (const char *option);      // number of times option was parsed
};



// Define symbols that let client applications determine if newly added
// features are supported.
#define OIIO_ARGPARSE_SUPPORTS_BRIEFUSAGE 1


OIIO_NAMESPACE_END


#endif // OPENIMAGEIO_ARGPARSE_H
