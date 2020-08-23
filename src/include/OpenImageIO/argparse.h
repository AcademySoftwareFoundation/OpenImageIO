// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md


#pragma once

#if defined(_MSC_VER)
// Ignore warnings about DLL exported classes with member variables that are template classes.
// This happens with the std::string m_errmessage member of ArgParse below.
#    pragma warning(disable : 4251)
#endif

#include <functional>
#include <memory>
#include <vector>

#include <OpenImageIO/export.h>
#include <OpenImageIO/oiioversion.h>
#include <OpenImageIO/paramlist.h>
#include <OpenImageIO/strutil.h>


OIIO_NAMESPACE_BEGIN


/////////////////////////////////////////////////////////////////////////////
///
/// \class ArgParse
///
/// Argument Parsing. Kind of resembles Python argparse library.
///
/// Set up argument parser:
///
///     ArgParse ap;
///     ap.intro("myapp does good things")
///       .usage("myapp [options] filename...");
///     ap.arg("filename")
///       .hidden()
///       .action([&](cspan<const char*> argv){ filenames.emplace_back(argv[0]); });
///
/// Declare arguments. Some examples of common idioms:
///
///     // Boolean option (no arguments)
///     ap.arg("-v")
///       .help("verbose mode")
///       .action(ArgParse::store_true());
///
///     // integer option
///     ap.arg("-passes NPASSES")
///        .help("number of passes")
///        .defaultval(1)
///        .action(ArgParse::store<int>);
///
///     // An option that takes 3 float arguments, like a V3f
///     ap.arg("-camera X Y Z")
///       .help("set the camera position")
///       .defaultval(Imath::V3f(0.0f, 0.0f, -1.0f))
///       .action(ArgParse::store<float>());
///
///     // Positional argument -- append strings
///     ap.arg("filename")
///       .action(ArgParse::append())
///       .hidden();
///
/// Parse the command line:
///
///     ap.parse (argc, argv);
///
/// Extract the values like they are attributes in a ParamValueList:
///
///     int passes = ap["passes"].get<int>();
///     bool verbose = ap["verbose"].get<int>();
///     Imath::V3f camera = ap["camera"].get<Imath::V3f>();
///
//
// ------------------------------------------------------------------
//
// Old syntax
// ----------
//
// We still support this old syntax as well:
//
// The parse function takes a list of options and variables or functions
// for storing option values and return <0 on failure:
//
//     static int parse_files (int argc, const char *argv[])
//     {
//         for (int i = 0;  i < argc;  i++)
//             filenames.push_back (argv[i]);
//         return 0;
//     }
//
//     static int blah_callback (int argc, const char *argv[])
//     {
//         std::cout << "blah argument was " << argv[1] << "\n";
//         return 0;
//     }
//
//     ...
//
//     ArgParse ap;
//
//     ap.options ("Usage: myapp [options] filename...",
//             "%*", parse_objects, "",
//             "-camera %f %f %f", &camera[0], &camera[1], &camera[2],
//                   "set the camera position",
//             "-lookat %f %f %f", &lx, &ly, &lz,
//                   "set the position of interest",
//             "-oversampling %d", &oversampling,  "oversamping rate",
//             "-passes %d", &passes, "number of passes",
//             "-lens %f %f %f", &aperture, &focalDistance, &focalLength,
//                    "set aperture, focal distance, focal length",
//             "-format %d %d %f", &width, &height, &aspect,
//                    "set width, height, aspect ratio",
//             "-v", &verbose, "verbose output",
//             "-q %!", &verbose, "quiet mode",
//             "--blah %@ %s", blahcallback, "Make the callback",
//             NULL);
//
//     if (ap.parse (argc, argv) < 0) {
//         std::cerr << ap.geterror() << std::endl;
//         ap.usage ();
//         return EXIT_FAILURE;
//     }
//
// The available argument types are:
//    - no \% argument - bool flag
//    - \%! - a bool flag, but set it to false if the option is set
//    - \%d - 32bit integer
//    - \%f - 32bit float
//    - \%F - 64bit float (double)
//    - \%s - std::string
//    - \%L - std::vector<std::string>  (takes 1 arg, appends to list)
//    - \%@ - a function pointer for a callback function will be invoked
//            immediately.  The prototype for the callback is
//                  int callback (int argc, char *argv[])
//    - \%* - catch all non-options and pass individually as an (argc,argv)
//            sublist to a callback, each immediately after it's found
//    - \%1 - catch all non-options that occur before any option is
//            encountered (like %*, but only for those prior to the first
//            real option.
//
// The argument type specifier may be followed by an optional colon (:) and
// then a human-readable parameter that will be printed in the help
// message. For example,
//
//     "--foo %d:WIDTH %d:HEIGHT", ...
//
// indicates that `--foo` takes two integer arguments, and the help message
// will be rendered as:
//
//     --foo WIDTH HEIGHT    Set the foo size
//
// There are several special format tokens:
//    - "<SEPARATOR>" - not an option at all, just a description to print
//                     in the usage output.
//
// Notes:
//   - If an option doesn't have any arguments, a bool flag argument is
//     assumed.
//   - No argument destinations are initialized.
//   - The empty string, "", is used as a global sublist (ie. "%*").
//   - Sublist functions are all of the form "int func(int argc, char **argv)".
//   - If a sublist function returns -1, parse() will terminate early.
//   - It is perfectly legal for the user to append ':' and more characters
//     to the end of an option name, it will match only the portion before
//     the semicolon (but a callback can detect the full string, this is
//     useful for making arguments:  myprog --flag:myopt=1 foobar
//
/////////////////////////////////////////////////////////////////////////////


class OIIO_API ArgParse {
public:
    class Arg;  // Forward declarion of Arg

    // ------------------------------------------------------------------
    /// @defgroup Setting up an ArgParse
    /// @{

    /// Construct an ArgParse.
    ArgParse();

    /// Destroy an ArgParse.
    ~ArgParse();

    // Old ctr. Don't use this. Instead, pass argc/argv to parse_args().
    ArgParse(int argc, const char** argv);

    // Disallow copy ctr and assignment
    ArgParse(const ArgParse&) = delete;
    const ArgParse& operator=(const ArgParse&) = delete;

    /// Move constructor
    ArgParse(ArgParse&& other)
        : m_impl(std::move(other.m_impl))
    {
    }

    /// Set an optional "intro" message, printed first when --help is used
    /// or an error is found in the program arguments.
    ArgParse& intro(string_view str);

    /// Set the "usage" string, which will be printed after the intro, and
    /// preceded by the message "Usage: ".
    ArgParse& usage(string_view str);

    /// Set an optional description of the program, to be printed after the
    /// usage but before the detailed argument help.
    ArgParse& description(string_view str);

    /// Set an optional epilog to be printed after the detailed argument
    /// help.
    ArgParse& epilog(string_view str);

    /// Optionally override the name of the program, as understood by
    /// ArgParse. If not supplied, it will be derived from the original
    /// command line.
    ArgParse& prog(string_view str);

    /// If true, this will cause the detailed help message to print the
    /// default value of all options for which it was supplied (the default
    /// is false).
    ArgParse& print_defaults(bool print);

    /// By default, every ArgParse automatically adds a `--help` argument
    /// that, if invoked, prints the full explanatory help message (i.e.,
    /// calls `print_help()`) and then exits the application with a success
    /// return code of 0.
    ///
    /// Calling `add_help(false)` disable the automatic `--help` argument.
    /// If you do this but want a help option (for example, you want to call
    /// it something different, or not exit the application, or have some
    /// other behavior), it's up to the user to add it and its behavior just
    /// like any other argument.
    ArgParse& add_help(bool add_help);

    /// By default, if the command line arguments do not conform to what is
    /// declared to ArgParse (for example, if unknown commands are
    /// encountered, required arguments are not found, etc.), the ArgParse
    /// (during `parse_args()` will print an error message, the full help
    /// guide, and then exit the application with a failure return code.
    ///
    /// Calling `exit_on_error(false)` disables this behavior. In this case,
    /// the application is responsible for checking the return code of
    /// `parse_args()` and responding appropriately.
    ArgParse& exit_on_error(bool exit_on_error);

    /// @}

    // ------------------------------------------------------------------
    /// @defgroup Parsing arguments
    /// @{

    /// With the options already set up, parse the command line. Return 0 if
    /// ok, -1 if it's a malformed command line.
    int parse_args(int argc, const char** argv);

    /// Return any error messages generated during the course of parse()
    /// (and clear any error flags).  If no error has occurred since the
    /// last time geterror() was called, it will return an empty string.
    std::string geterror() const;

    /// Return the name of the program. This will be either derived from the
    /// original command line that launched this application, or else
    /// overridden by a prior call to `prog()`.
    std::string prog_name() const;

    /// Print the full help message to stdout.  The usage message is
    /// generated and formatted automatically based on the arguments
    /// declared.
    void print_help() const;

    /// Print a brief usage message to stdout.  The usage message is
    /// generated and formatted automatically based on the arguments
    /// declared.
    void briefusage() const;

    /// Return the entire command-line as one string.
    std::string command_line() const;

    /// @}

    // ------------------------------------------------------------------
    /// @defgroup Declaring arguments
    /// @{

    /// Add an argument declaration. Ordinary arguments start with a leading
    /// `-` character (or `--`, either will be accepted). Positional
    /// arguments lack a leading `-`.
    ///
    /// The argname consists of any of these options:
    ///
    ///   * "name" : a positional argument.
    ///
    ///   * "-name" or "--name" : an ordinary flag argument. If this argument
    ///     should be followed by parameters, they will later be declared
    ///     using some combination of `Arg::nargs()` and `Arg::metavar()`.
    ///
    ///   * "--name A B C" : including the names of parameters following the
    ///     flag itself implicitly means the same thing as calling `nargs()`
    ///     and `metavar()`, and there is no need to call them separately.
    ///
    /// This method returns an `Arg&`, so it is permissible to chain Arg
    /// method calls. Those chained calls are what communicates the number
    /// of parameters, help message, action, etc. See the `Arg` class for
    /// all the possible Arg methods.
    ///
    Arg& add_argument(const char* argname);

    /// Alternate way to add an argument, specifying external storage
    /// destinations for the parameters. This is more reminiscent of the
    /// older, pre-2.2 API, and also can be convenient as an alternative to
    /// extracting every parameter and putting them into variables.
    ///
    /// Flags with no parameters are followed by a `bool*`. Args with
    /// parameters must declare them as "%T:NAME", where the "%T" part
    /// corresponds to the storage type, `%d` for int, `%f` for float, `%s`
    /// for std::string, and `%L` for `std::vector<std::string>`.
    ///
    /// Examples:
    ///
    ///     ArgParse ap;
    ///     bool verbose = false;
    ///     ap.add_argument("-v", &verbose)
    ///       .help("verbose mode");
    ///
    ///     float r = 0, g = 0, b = 0;
    ///     ap.add_argument("--color %f:R %f:G %f:B", &r, &g, &b)
    ///       .help("diffuse color")
    ///       .action(ArgParse::store<float>());
    ///
    template<typename... T> Arg& add_argument(const char* argname, T... args)
    {
        return argx(argname, args...);
    }

    /// Shorter synonym for add_argument().
    Arg& arg(const char* argname) { return add_argument(argname); }

    /// Shorter synonym for add_argument().
    template<typename... T> Arg& arg(const char* argname, T... args)
    {
        return argx(argname, args...);
    }

    /// Add a separator with a text message. This can be used to group
    /// arguments with section headings.
    ///
    /// Example:
    ///
    ///     ArgParse ap;
    ///     ap.separator("Basic arguments:");
    ///     ap.add_argument("-v");
    ///     ap.add_argument("-a");
    ///     ap.separator("Advanced arguments:");
    ///     ap.add_argument("-b");
    ///     ap.add_argument("-c");
    ///
    /// Will print the help section like:
    ///
    ///     Basic arguments:
    ///         -v
    ///         -a
    ///     Advanced arguments:
    ///         -b
    ///         -c
    ///
    Arg& separator(string_view text);


    /// Holder for a callback that takes a span of C strings as arguments.
    // typedef std::function<void(cspan<const char*> myargs)> Action;
    using Action = std::function<void(cspan<const char*> myargs)>;

    /// Holder for a callback that takes an Arg ref and a span of C strings
    /// as arguments.
    using ArgAction = std::function<void(Arg& arg, cspan<const char*> myargs)>;


    /// A call to `ArgParse::arg()` returns an `Arg&`. There are lots of
    /// things you can do to that reference to modify it. Nearly all
    /// Arg methods return an `Arg&` so you can chain the calls, like this:
    ///
    ///     ArgParse ap;
    ///     ...
    ///     ap.add_argument("-v")
    ///       .help("Verbose mode")
    ///       .action(Arg::store_true());
    ///
    class OIIO_API Arg {
    public:
        // Arg constructor. This should only be called by
        // ArgParse::add_argument().
        Arg(ArgParse& ap)
            : m_argparse(ap)
        {
        }
        // Disallow copy ctr and assignment
        Arg(const Arg&)    = delete;
        const Arg& operator=(const Arg&) = delete;

        /// Set the help / description of this command line argument.
        Arg& help(string_view help);

        /// Set the number of subsequent parameters to this argument.
        /// Setting `nargs(0)` means it is a flag only, with no parameters.
        Arg& nargs(int n);

        // TODO:
        // Set the number of subsequent parameters to this arguments. "?"
        // means a single optional value, "*" means any number of optional
        // values (and implies action(append)), "+" means one or more
        // optional values (just like "*", but will be an error if none are
        // supplied at all).
        // Arg& nargs(string_view n);


        /// Set the name(s) of any argument parameters as they are printed
        /// in the help message. For arguments that take multiple
        /// parameters, just put spaces between them. Note that the number
        /// of arguments is inferred by the number of metavar names, so
        /// there is no need to set nargs separately if metavar() is called
        /// properly.
        ///
        /// Examples:
        ///
        ///     ArgParse ap;
        ///     ap.add_argument("--aa")
        ///       .help("set sampling rate (per pixel)")
        ///       .metavar("SAMPLES");
        ///     ap.add_argument("--color")
        ///       .help("set the diffuse color")
        ///       .metavar("R G B");
        ///
        /// Will print help like:
        ///
        ///     --aa SAMPLES       set sampling rate (per pixel)
        ///     --color R G B      set the diffuse color
        ///
        Arg& metavar(string_view name);

        /// Override the destination attribute name (versus the default
        /// which is simply the name of the command line option with the
        /// leading dashes stripped away). The main use case is if you want
        /// two differently named command line options to both store values
        /// into the same attribute. It is very important that if you use
        /// dest(), you call it before setting the action.
        ///
        /// Example:
        ///
        ///     // Add -v argument, but store its result in `ap["verbose"]`
        ///     // rather than the default `ap["v"]`.
        ///     ap.add_argument("-v")
        ///       .help("verbose mode")
        ///       .dest("verbose")
        ///       .action(ArgParse::store_true());
        ///
        Arg& dest(string_view dest);

        /// Initialize the destination attribute with a default value. Do
        /// not call `.dest("name")` on the argument after calling
        /// defaultval, of it will end up with the default value in the
        /// wrong attribute.
        template<typename T> Arg& defaultval(const T& val)
        {
            m_argparse.params()[dest()] = val;
            return *this;
        }

        /// Mark the argument as hidden from the help message.
        Arg& hidden();

        /// Set the action for this argument to store 1 in the destination
        /// attribute. Initialize the destination attribute to 0 now. Do not
        /// call `.dest("name")` on the argument after calling store_true,
        /// you must override the destination first!
        Arg& store_true()
        {
            m_argparse.params()[dest()] = 0;
            action(ArgParse::store_true());
            return *this;
        }

        /// Set the action for this argument to store 0 in the destination
        /// attribute. Initialize the destination attribute to 1 now. Do not
        /// call `.dest("name")` on the argument after calling store_false,
        /// you must override the destination first!
        Arg& store_false()
        {
            m_argparse.params()[dest()] = 1;
            action(ArgParse::store_false());
            return *this;
        }

        /// Add an arbitrary action:   `func(Arg&, cspan<const char*>)`
        Arg& action(ArgAction&& func);

        /// Add an arbitrary action:   `func(cspan<const char*>)`
        Arg& action(Action&& func)
        {
            // Implemented with a lambda that applies a wrapper to turn
            // it into func(Arg&,cspan<const char*>).
            return action([=](Arg&, cspan<const char*> a) { func(a); });
        }
#if OIIO_MSVS_BEFORE_2017
        // MSVS 2015 seems to need this, fixed in later versions.
        Arg& action(void (*func)(cspan<const char*> myargs))
        {
            return action([=](Arg&, cspan<const char*> a) { func(a); });
        }
#endif

        /// Add an arbitrary action:  `func()`
        Arg& action(void (*func)())
        {
            return action([=](Arg&, cspan<const char*>) { func(); });
        }

        // Old style action for compatibility
        Arg& action(int (*func)(int, const char**))
        {
            return action([=](Arg&, cspan<const char*> a) {
                func(int(a.size()), (const char**)a.data());
            });
        }

        /// Return the name of the argument.
        string_view name() const;

        /// Return the "destination", the name of the attribute in which
        /// the argument value will be stored.
        string_view dest() const;

        /// Get a reference to the ArgParse that owns this Arg.
        ArgParse& argparse() { return m_argparse; }

    protected:
        ArgParse& m_argparse;
    };

    /// @}

    // ------------------------------------------------------------------
    /// @defgroup Action library
    /// @{
    ///
    /// These are actions provided for convenience.
    /// Examples of their use:
    ///
    ///     ArgParse ap;
    ///     ...
    ///
    ///     // Flag, just store 1 if set (default to 0)
    ///     ap.add_argument("-v")
    ///       .action(ArgParse::store_true());
    ///
    ///     // Store 42 if the flag is encountered (default is 1)
    ///     ap.add_argument("--foo")
    ///       .defaultval(1)
    ///       .action(ArgParse::store_const(42));
    ///
    ///     // Take an integer argument and store it
    ///     ap.add_argument("--bar")
    ///       .action(ArgParse::store<int>());
    ///
    ///     // Take 3 floating point arguments, pass default as Color3f.
    ///     ap.add_argument("--color")
    ///       .nargs(3)
    ///       .metavar("R G B")
    ///       .action(ArgParse::store<float>)
    ///       .defaultval(Imath::Color3f(0.5f, 0.5f, 0.5f));
    ///
    ///     // Take a single string argument, but *append* it, so if the
    ///     // option appears multiple time, you get a list of strings.
    ///     ap.add_argument("-o")
    ///       .action(ArgParse::append());
    ///
    ///     // Another string appending example, but using a positional
    ///     // argument.
    ///     ap.add_argument("filename")
    ///       .action(ArgParse::append());
    ///

    /// Return an action that stores 1 into its destination attribute.
    static ArgAction store_true();

    /// Return an action that stores 0 into its destination attribute.
    static ArgAction store_false();

    /// Return an action that stores a constant value into its destination
    /// attribute.
    template<typename T> static ArgAction store_const(const T& value)
    {
        return [&, value](Arg& arg, cspan<const char*> myarg) {
            arg.argparse().params()[arg.dest()] = value;
        };
    }

    static ArgAction store_const(const char* value)
    {
        return [&, value](Arg& arg, cspan<const char*> myarg) {
            arg.argparse().params()[arg.dest()] = string_view(value);
        };
    }

    /// Return an action that stores into its destination attribute the
    /// following `n` command line arguments (where `n` is the number of
    /// additional command line arguments that this option requires).
    template<typename T = ustring> static ArgAction store()
    {
        return [&](Arg& arg, cspan<const char*> myarg) {
            if (myarg[0][0] == '-')  // Skip command itself
                myarg = myarg.subspan(1);
            ParamValueList& pl(arg.argparse().params());
            int n   = int(myarg.size());
            T* vals = OIIO_ALLOCA(T, n);
            for (int i = 0; i < n; ++i)
                vals[i] = Strutil::from_string<T>(myarg[i]);
            if (n == 1) {
                pl[arg.dest()] = vals[0];
            } else {  // array case -- always store as strings
                pl.attribute(arg.dest(), TypeDesc(BaseTypeFromC<T>::value, n),
                             vals);
            }
        };
    }

    /// Return an action that appends into its destination attribute the
    /// following `n` command line arguments (where `n` is the number of
    /// additional command line arguments that this option requires).
    template<typename T = ustring> static ArgAction append()
    {
        return [&](Arg& arg, cspan<const char*> myarg) {
            if (myarg[0][0] == '-')  // Skip command itself
                myarg = myarg.subspan(1);
            ParamValueList& pl(arg.argparse().params());
            ParamValue* pv = pl.find_pv(arg.dest());
            // TypeDesc t = pv ? pv->type() : TypeUnknown;
            int nold = pv ? pv->type().basevalues() : 0;
            int nnew = int(myarg.size());
            int n    = nold + nnew;
            T* vals  = OIIO_ALLOCA(T, n);
            for (int i = 0; i < nold; ++i)
                vals[i] = Strutil::from_string<T>(pv->get_string_indexed(i));
            for (int i = 0; i < nnew; ++i)
                vals[i + nold] = Strutil::from_string<T>(myarg[i]);
            if (n == 1) {
                pl[arg.dest()] = vals[0];
            } else {  // array case -- always store as strings
                pl.attribute(arg.dest(), TypeDesc(BaseTypeFromC<T>::value, n),
                             vals);
            }
        };
    }

    /// Return an action that does nothing. I guess you could use use this
    /// for an argument that is obsolete and is still accepted, but no
    /// longer has any function.
    static Action do_nothing();

    /// @}


    // ------------------------------------------------------------------
    /// @defgroup Retrieving values of parsed arguments
    /// @{
    ///
    /// Retrieve arguments in the same manner that you would access them
    /// from a OIIO::ParamValueList.
    ///
    /// Examples:
    ///
    ///     // Please see the code example in the "Action library" section above
    ///     // for the argument declarations.
    ///
    ///     // retrieve whether -v flag was set
    ///     bool verbose = ap["v"].get<int>();
    ///
    ///     // Retrieve the parameter passed to --bar, defaulting to 13 if
    ///     // never set on the command line:
    ///     int bar = ap["bar"].get<int>(13);
    ///
    ///     // Retrieve the color, which had 3 float parameters. Extract
    ///     // it as an Imath::Color3f.
    ///     Imath::Color3f diffuse = ap["color"].get<Imath::Color3f>();
    ///
    ///     // Retrieve the filename list as a vector:
    ///     auto filenames = ap["filename"].as_vec<std::string>();
    ///     for (auto& f : filenames)
    ///         Strutil::printf("  file: \"%s\"\n", f);
    ///

    /// Access a single argument result by name.
    AttrDelegate<const ParamValueList> operator[](string_view name) const
    {
        return { &cparams(), name };
    }
    /// Access a single argument result by name.
    AttrDelegate<ParamValueList> operator[](string_view name)
    {
        return { &params(), name };
    }

    /// Directly access the ParamValueList that holds the argument results.
    ParamValueList& params();
    /// Directly access the ParamValueList that holds the argument results
    /// (const version).
    const ParamValueList& cparams() const;

    /// @}


private:
    class Impl;
    std::shared_ptr<Impl> m_impl;  // PIMPL pattern
    Arg& argx(const char* argname, ...);
    friend class ArgOption;

public:
    // ------------------------------------------------------------------
    // Old API.  DEPRECATED(2.2)

    // Declare the command line options.  After the introductory message,
    // parameters are a set of format strings and variable pointers.  Each
    // string contains an option name and a scanf-like format string to
    // enumerate the arguments of that option (eg. `"-option %d %f %s"`).
    // The format string is followed by a list of pointers to the argument
    // variables, just like scanf.  A NULL terminates the list.  Multiple
    // calls to options() will append additional options.
    int options(const char* intro, ...);

    // old name
    // DEPRECATED(2.2)
    int parse(int argc, const char** argv) { return parse_args(argc, argv); }

    // Type for a callback that writes something to the output stream.
    typedef std::function<void(const ArgParse& ap, std::ostream&)> callback_t;
    // Set callbacks to run that will print any matter you want as part
    // of the verbose usage, before and after the options are detailed.
    void set_preoption_help(callback_t callback);
    void set_postoption_help(callback_t callback);

    // DEPRECATED(2.2) synonym for `print_help()`.
    void usage() const { print_help(); }
};



// Define symbols that let client applications determine if newly added
// features are supported.
#define OIIO_ARGPARSE_SUPPORTS_BRIEFUSAGE 1
#define OIIO_ARGPARSE_SUPPORTS_HUMAN_PARAMNAME 1

OIIO_NAMESPACE_END
