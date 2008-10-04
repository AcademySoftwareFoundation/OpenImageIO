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
/// Simple parsing of program command-line arguments.


#ifndef ARGPARSE_H
#define ARGPARSE_H

#include <vector>


// namespace I have no idea... {


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
///    ArgParse ap(argc, argv);
///
///    if (ap.parse ("Usage: myapp [options] filename...",
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
///            "-v", &flag, "verbose output",
///            NULL) < 0) {
///        std::cerr << ap.error_message() << std::endl;
///        ap.usage ();
///        return EXIT_FAILURE;
///    }
/// \endcode
///
/// The available argument types are:
///    - \%d - 32bit integer
///    - \%f - 32bit float
///    - \%F - 64bit float (double)
///    - \%s - std::string
///    - \%! (or no % argument) - bool flag
///    - \%* - (argc,argv) sublist with callback
///
/// There are several special format tokens:
///    - "<SEPARATOR>" - not an option at all, just a description to print
///                     in the usage output.
///
/// Notes:
///   - If an option doesn't have any arguments, a flag argument is assumed.
///   - Flags are initialized to false.  No other variables are initialized.
///   - The empty string, "", is used as a global sublist (ie. "%*").
///   - Sublist functions are all of the form "int func(int argc, char **argv)".
///   - If a sublist function returns -1, parse() will terminate early.
///
/////////////////////////////////////////////////////////////////////////////


class ArgParse {
public:
    ArgParse (int argc, const char **argv);
    ~ArgParse ();
    
    int parse (const char *intro, ...);  // parse the command line  <0 on error

    std::string error_message () const { return errmessage; }

    /// Print the usage message to stdout.  The usage message is
    /// generated and formatted automatically based on the command and
    /// description arguments passed to parse().
    void usage () const;

    /// Return the entire command-line as one string.
    ///
    std::string command_line () const;

private:
    int argc;                           // a copy of the command line argc
    char **argv;                        // a copy of the command line argv
    std::string errmessage;             // error message
    ArgOption *global;                  // option for extra cmd line arguments
    std::string intro;
    std::vector<ArgOption *> option;

    ArgOption *find_option(const char *name);
    int invoke_all_sublist_callbacks();
    int parse_command_line();
    void error (const char *format, ...);
    int found (const char *option);      // number of times option was parsed
};



// }; // namespace ???

#endif // ARGPARSE_H
