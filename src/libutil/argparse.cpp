// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <cassert>
#include <cctype>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/dassert.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/paramlist.h>
#include <OpenImageIO/platform.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>

OIIO_NAMESPACE_BEGIN

class ArgOption final : public ArgParse::Arg {
public:
    typedef int (*callback_t)(int, const char**);

    ArgOption(ArgParse& ap, const char* argspec);
    ~ArgOption() {}

    int initialize();

    int nargs() const { return m_count; }
    void nargs(int n);

    string_view metavar() const { return Strutil::join(m_prettyargs, " "); }
    void metavar(string_view name)
    {
        m_prettyargs = Strutil::splits(name);
        m_count      = 0;
        nargs(int(m_prettyargs.size()));
        compute_prettyformat();
    }

    const std::string& flag() const { return m_flag; }
    const std::string& name() const { return m_name; }
    const std::string& dest() const { return m_dest; }

    const std::string& argspec() const { return m_argspec; }
    const std::string& prettyformat() const { return m_prettyformat; }
    void compute_prettyformat()
    {
        m_prettyformat = flag();
        if (m_prettyargs.size()) {
            m_prettyformat += " ";
            m_prettyformat += Strutil::join(m_prettyargs, " ");
        }
    }

    bool is_flag() const { return m_type == Flag; }
    bool is_reverse_flag() const { return m_type == ReverseFlag; }
    bool is_sublist() const { return m_type == Sublist; }
    bool is_regular() const { return m_type == Regular; }
    bool has_callback() const { return m_has_callback; }

    void add_parameter(int i, void* p);

    void set_parameter(int i, const char* argv);

    int invoke_callback(int argc, const char** argv) const
    {
        return m_callback ? m_callback(argc, argv) : 0;
    }

    void set_callback(callback_t cb) { m_callback = cb; }

    void found_on_command_line() { m_repetitions++; }
    int parsed_count() const { return m_repetitions; }

    void help(std::string h) { m_help = std::move(h); }
    const std::string& help() const { return m_help; }

    bool is_separator() const { return argspec() == "<SEPARATOR>"; }
    bool hidden() const { return m_hidden; }

    bool always_run() const { return m_always_run; }

    bool had_error() const { return m_error; }
    void had_error(bool e) { m_error = e; }

    ArgParse::ArgAction& action() { return m_action; }

private:
    enum OptionType { None, Regular, Flag, ReverseFlag, Sublist };

    std::string m_argspec;       // original format string specification
    std::string m_prettyformat;  // human readable format
    std::string m_flag;          // just the -flag_foo part
    std::string m_name;          // just the 'flat_foo' part
    std::string m_dest;          // destination parameter name
    std::string m_code;          // parameter types, eg "df"
    std::string m_help;
    OptionType m_type = None;
    int m_count       = 0;               // number of parameters
    std::vector<void*> m_param;          // pointers to app data vars
    std::vector<TypeDesc> m_paramtypes;  // Expected param types
    std::vector<std::string> m_prettyargs;
    ArgParse::ArgAction m_action = nullptr;
    callback_t m_callback        = nullptr;
    int m_repetitions            = 0;      // number of times on cmd line
    bool m_has_callback          = false;  // needs a callback?
    bool m_hidden                = false;  // hidden?
    bool m_always_run            = false;  // always run?
    bool m_error                 = false;  // invalid option, had an error
    friend class ArgParse;
    friend class ArgParse::Arg;
    friend class ArgParse::Impl;
};



class ArgParse::Impl {
public:
    ArgParse& m_argparse;
    int m_argc;                        // a copy of the command line argc
    const char** m_argv;               // a copy of the command line argv
    mutable std::string m_errmessage;  // error message
    ArgOption* m_global    = nullptr;  // option for extra cmd line arguments
    ArgOption* m_preoption = nullptr;  // pre-switch cmd line arguments
    std::string m_intro;
    std::string m_usage;
    std::string m_description;
    std::string m_epilog;
    std::string m_prog;
    bool m_print_defaults = false;
    bool m_add_help       = true;
    bool m_exit_on_error  = true;
    bool m_running        = true;
    bool m_aborted        = false;
    int m_current_arg;
    int m_next_arg = -1;
    std::vector<std::unique_ptr<ArgOption>> m_option;
    callback_t m_preoption_help  = [](const ArgParse&, std::ostream&) {};
    callback_t m_postoption_help = [](const ArgParse&, std::ostream&) {};
    ParamValueList m_params;
    std::string m_version;

    Impl(ArgParse& parent, int argc, const char** argv)
        : m_argparse(parent)
        , m_argc(argc)
        , m_argv(argv)
        , m_prog(Filesystem::filename(Sysutil::this_program_path()))
    {
    }

    int parse_args(int argc, const char** argv);

    ArgOption* find_option(const char* name);
    int found(const char* option);  // number of times option was parsed

    // Given an incorrect argument name `badarg`, return the closest match
    // among the valid commands (that is within the threshold). Return an
    // empty string if no other command has an edit distance within the
    // threshold.
    std::string closest_match(string_view badarg, size_t threshold = 2) const;

    // Error with std::fmt-like formatting
    template<typename... Args>
    void errorfmt(const char* fmt, const Args&... args) const
    {
        m_errmessage = Strutil::fmt::format(fmt, args...);
    }
};



// Constructor.  Does not do any parsing or error checking.
// Make sure to call initialize() right after construction.
// The format may look like this: "%g:FOO", and in that case split
// the formatting part (e.g. "%g") from the self-documenting
// human-readable argument name ("FOO")
ArgOption::ArgOption(ArgParse& ap, const char* argspec)
    : ArgParse::Arg(ap)
{
    std::vector<std::string> uglyargs;
    auto args = Strutil::splits(argspec, " ");
    int i     = 0;
    for (auto&& a : args) {
        if (i == 0)
            m_flag = a;
        auto parts     = Strutil::splitsv(a, ":", 2);
        string_view ug = a, pr = a;
        if (parts.size() == 2) {
            ug = parts[0];
            pr = parts[1];
        }
        if (i && parts.size() == 1 && ug.size() && ug[0] != '%')
            ug = "%s";  // If no type spec, just assume string
        uglyargs.push_back(ug);
        if (pr == "%L")
            pr = "%s";
        if (pr != "%!" && pr != "%@")
            if (i)
                m_prettyargs.push_back(pr);
        ++i;
    }
    m_argspec = Strutil::join(uglyargs, " ");
    compute_prettyformat();
}



// Parses the format string ("-option %s %d %f") to extract the
// flag ("-option") and create a code string ("sdf").  After the
// code string is created, the param list of void* pointers is
// allocated to hold the argument variable pointers.
int
ArgOption::initialize()
{
    size_t n;
    const char* s;

    if (m_argspec.empty() || m_argspec == "%*") {
        m_type  = Sublist;
        m_count = 1;  // sublist callback function pointer
        m_code  = "*";
        m_flag  = "";
    } else if (m_argspec == "%1") {
        m_type  = Sublist;
        m_count = 1;  // sublist callback function pointer
        m_code  = "*";
        m_flag  = "";
    } else if (is_separator()) {
    } else if (m_argspec[0] != '-') {
        m_type  = Sublist;
        m_count = 1;  // sublist callback function pointer
        m_code  = "*";
        m_flag  = "";
    } else {
        // extract the flag name
        s = &m_argspec[0];
        assert(*s == '-');
        assert(isalpha(s[1]) || (s[1] == '-' && isalpha(s[2])));
        s++;
        if (*s == '-')
            s++;

        while (isalnum(*s) || *s == '_' || *s == '-')
            s++;

        if (!*s) {
            m_flag  = m_argspec;
            m_type  = Flag;
            m_count = 1;
            m_code  = "b";
        } else {
            n = s - (&m_argspec[0]);
            m_flag.assign(m_argspec.begin(), m_argspec.begin() + n);

            // Parse the scanf-like parameters

            m_type = Regular;
            m_code.clear();

            while (*s != '\0') {
                if (*s == '%') {
                    s++;
                    assert(*s != '\0');

                    m_count++;  // adding another parameter

                    switch (*s) {
                    case 'd':  // 32bit int
                    case 'g':  // float
                    case 'f':  // float
                    case 'F':  // double
                    case 's':  // string
                    case 'L':  // vector<string>
                        assert(m_type == Regular);
                        m_code += *s;
                        break;
                    case '!':
                        m_type = ReverseFlag;
                        m_code += *s;
                        break;
                    case '*':
                        assert(m_count == 1);
                        m_type = Sublist;
                        break;
                    case '1':
                        assert(m_count == 1);
                        m_type = Sublist;
                        break;
                    case '@':
                        m_has_callback = true;
                        --m_count;
                        break;
                    default:
                        std::cerr << "Programmer error:  Unknown option ";
                        std::cerr << "type string \"" << *s << "\""
                                  << "\n";
                        return 0;
                    }
                }

                s++;
            }

            // Catch the case where only a callback was given, it's still
            // a bool.
            if (!*s && m_count == 0 && m_has_callback) {
                m_type  = Flag;
                m_count = 1;
                m_code  = "b";
            }
        }
    }

    if (m_argspec[0] == '-')
        m_name = Strutil::lstrip(m_flag, "-");
    else
        m_name = m_argspec;
    m_dest = m_name;

    // Allocate space for the parameter pointers and initialize to NULL
    m_param.resize(m_count, nullptr);
    m_paramtypes.resize(m_count, TypeUnknown);

    return 0;
}



// Allocate space for the parameter pointers and initialize to NULL
void
ArgOption::nargs(int n)
{
    if (n == m_count)
        return;
    m_param.resize(n, nullptr);
    m_paramtypes.resize(n, TypeUnknown);
    m_prettyargs.resize(n, Strutil::upper(m_name));
    compute_prettyformat();
    for (int i = m_count; i < n; ++i)
        m_argspec += Strutil::concat(" %s:", m_prettyargs[i]);
    initialize();
    m_count = n;
}



// Stores the pointer to an argument in the param list and
// initializes flag options to FALSE.
// FIXME -- there is no such initialization.  Bug?
void
ArgOption::add_parameter(int i, void* p)
{
    assert(i >= 0 && i < m_count);
    m_param[i]      = p;
    m_paramtypes[i] = TypeUnknown;
}



// Given a string from argv, set the associated option parameter
// at index i using the format conversion code in the code string.
void
ArgOption::set_parameter(int i, const char* argv)
{
    assert(i < m_count);

    if (!m_param[i])  // If they passed NULL as the address,
        return;       // don't write anything.

    switch (m_code[i]) {
    case 'd': *(int*)m_param[i] = Strutil::stoi(argv); break;

    case 'f':
    case 'g': *(float*)m_param[i] = Strutil::stof(argv); break;

    case 'F': *(double*)m_param[i] = Strutil::stod(argv); break;

    case 's': *(std::string*)m_param[i] = argv; break;

    case 'S': *(std::string*)m_param[i] = argv; break;

    case 'L': ((std::vector<std::string>*)m_param[i])->push_back(argv); break;

    case 'b': *(bool*)m_param[i] = true; break;
    case '!': *(bool*)m_param[i] = false; break;

    case '*':
    default: break;
    }
}



ArgParse::ArgParse()
    : m_impl(new Impl(*this, 0, nullptr))
{
}



ArgParse::ArgParse(int argc, const char** argv)
    : m_impl(new Impl(*this, argc, argv))
{
}



ArgParse::~ArgParse() {}



// Top level command line parsing function called after all options
// have been parsed and created from the format strings.  This function
// parses the command line (argc,argv) stored internally in the constructor.
// Each command line argument is parsed and checked to see if it matches an
// existing option.  If there is no match, and error is reported and the
// function returns early.  If there is a match, all the arguments for
// that option are parsed and the associated variables are set.
int
ArgParse::parse_args(int xargc, const char** xargv)
{
    int r = m_impl->parse_args(xargc, xargv);
    if (r < 0 && m_impl->m_exit_on_error) {
        Sysutil::Term term(std::cerr);
        std::cerr << term.ansi("red") << prog_name() << " error: " << geterror()
                  << term.ansi("default") << std::endl;
        print_help();
        exit(EXIT_FAILURE);
    }
    return r;
}



int
ArgParse::Impl::parse_args(int xargc, const char** xargv)
{
    m_argc    = xargc;
    m_argv    = xargv;
    m_running = true;

    // Add version option if requested
    if (m_version.size() && !find_option("--version")) {
        m_option.emplace(m_option.begin(),
                         new ArgOption(m_argparse, "--version"));
        m_option[0]->m_help   = "Print version and exit";
        m_option[0]->m_action = [&](Arg& arg, cspan<const char*> myarg) {
            Strutil::print("{}\n", m_version);
            if (m_exit_on_error)
                exit(EXIT_SUCCESS);
            else {
                m_argparse.abort();
            }
        };
        m_option[0]->initialize();
    }

    // Add help option if requested
    if (m_add_help && !find_option("--help")) {
        m_option.emplace(m_option.begin(), new ArgOption(m_argparse, "--help"));
        // m_option[0]->m_type = ArgOption::Flag;
        m_option[0]->m_help   = "Print help message";
        m_option[0]->m_action = [&](Arg& arg, cspan<const char*> myarg) {
            this->m_argparse.print_help();
            if (m_exit_on_error)
                exit(EXIT_SUCCESS);
            else {
                m_argparse.abort();
            }
        };
        m_option[0]->initialize();
    }

    bool any_option_encountered = false;
    for (int i = 1; i < m_argc; ++i) {
        m_current_arg = i;
        m_next_arg    = -1;
        if (m_aborted)
            break;
        if (m_argv[i][0] == '-'
            && (isalpha(m_argv[i][1]) || m_argv[i][1] == '-')) {  // flag
            any_option_encountered = true;
            // Look up only the part before a ':'
            std::string argname = m_argv[i];
            size_t colon        = argname.find_first_of(':');
            if (colon != std::string::npos)
                argname.erase(colon, std::string::npos);
            ArgOption* option = find_option(argname.c_str());
            if (!option) {
                std::string suggestion = closest_match(argname);
                if (suggestion.size())
                    errorfmt("Invalid option \"{}\" (did you mean {}?)",
                             m_argv[i], suggestion);
                else
                    errorfmt("Invalid option \"{}\"", m_argv[i]);
                return -1;
            }

            option->found_on_command_line();

            if (option->is_flag() || option->is_reverse_flag()) {
                if (m_running || option->always_run()) {
                    option->set_parameter(0, nullptr);
                    if (option->has_callback())
                        option->invoke_callback(1, m_argv + i);
                    if (option->m_action) {
                        option->m_action(*option, { m_argv + i, 1 });
                    } else {
                        m_params[option->dest()] = option->is_flag() ? 1 : 0;
                    }
                }
            } else {
                assert(option->is_regular());
                int n = option->nargs();
                assert(n >= 1);
                for (int j = 0; j < n; j++) {
                    if (j + i + 1 >= m_argc) {
                        errorfmt("Missing parameter {} from option \"{}\"",
                                 j + 1, option->flag());
                        return -1;
                    }
                    option->set_parameter(j, m_argv[i + j + 1]);
                }
                if (m_running || option->always_run()) {
                    if (option->has_callback())
                        option->invoke_callback(1 + n, m_argv + i);
                    if (option->m_action) {
                        option->m_action(*option,
                                         { m_argv + i, span_size_t(n + 1) });
                    } else {
                        m_params[option->dest()] = m_argv[i + 1];
                    }
                }
                i += n;
            }
        } else if (m_running) {
            // not an option nor an option parameter, glob onto global list,
            // or the preoption list if a preoption callback was given and
            // we haven't encountered any options yet.
            if (m_preoption && !any_option_encountered)
                if (m_preoption->m_action)
                    m_preoption->m_action(*m_preoption, { m_argv + i, 1 });
                else
                    m_preoption->invoke_callback(1, m_argv + i);
            else if (m_global) {
                if (m_global->m_action)
                    m_global->m_action(*m_global, { m_argv + i, 1 });
                else
                    m_global->invoke_callback(1, m_argv + i);
            } else {
                errorfmt("Argument \"{}\" does not have an associated option",
                         m_argv[i]);
                return -1;
            }
        }
        if (m_next_arg >= 0) {
            // The action we just took requested a different next arg.
            i = m_next_arg - 1;
        }
    }

    return 0;
}



std::string
ArgParse::Impl::closest_match(string_view argname, size_t threshold) const
{
    string_view badarg(argname);
    Strutil::parse_while(badarg, "-");
    std::string suggestion;
    if (badarg.size() <= 1) {
        // Single char args presumed to have no unique helpful suggestions,
        // since they are edit distance of 1 from all other 1-char options.
        return suggestion;
    }
    size_t closest = threshold;
    for (auto&& opt : m_option) {
        string_view optname = opt->name();
        if (!optname.size())
            continue;
        auto howclose = Strutil::edit_distance(badarg, optname);
        // Strutil::print("{} vs {} -> {}\n", badarg, optname, howclose);
        if (howclose < closest) {
            closest    = howclose;
            suggestion = opt->flag();
        }
    }
    // Strutil::print("suggesting '{}' with {}\n", suggestion, closest);
    return suggestion;
}



// Primary entry point.  This function accepts a set of format strings
// and variable pointers.  Each string contains an option name and a
// scanf-like format string to enumerate the arguments of that option
// (eg. "-option %d %f %s").  The format string is followed by a list
// of pointers to the argument variables, just like scanf.  All format
// strings and arguments are parsed to create a list of ArgOption objects.
// After all ArgOptions are created, the command line is parsed and
// the sublist option callbacks are invoked.
int
ArgParse::options(const char* intro, ...)
{
    va_list ap;
    va_start(ap, intro);

    m_impl->m_description += intro;
    for (const char* cur = va_arg(ap, char*); cur; cur = va_arg(ap, char*)) {
        if (m_impl->find_option(cur) && strcmp(cur, "<SEPARATOR>")) {
            m_impl->errorfmt("Option \"{}\" is multiply defined", cur);
            return -1;
        }

        // Build a new option and then parse the values
        std::unique_ptr<ArgOption> option(new ArgOption(*this, cur));
        if (option->initialize() < 0) {
            return -1;
        }

        if (cur[0] == '\0'
            || (cur[0] == '%' && cur[1] == '*' && cur[2] == '\0')) {
            // set default global option
            m_impl->m_global = option.get();
        }

        if (cur[0] == '%' && cur[1] == '1' && cur[2] == '\0') {
            // set default pre-switch option
            m_impl->m_preoption = option.get();
        }

        if (option->has_callback())
            option->set_callback((ArgOption::callback_t)va_arg(ap, void*));

        // Grab any parameters and store them with this option
        for (int i = 0; i < option->nargs(); i++) {
            void* p = va_arg(ap, void*);
            option->add_parameter(i, p);
            if (option.get() == m_impl->m_global
                || option.get() == m_impl->m_preoption)
                option->set_callback((ArgOption::callback_t)p);
        }

        // Last argument is description
        option->help((const char*)va_arg(ap, const char*));
        if (option->help().empty())
            option->hidden();
        m_impl->m_option.emplace_back(std::move(option));
    }

    va_end(ap);
    return 0;
}



string_view
ArgParse::Arg::name() const
{
    return static_cast<const ArgOption*>(this)->name();
}



string_view
ArgParse::Arg::dest() const
{
    return static_cast<const ArgOption*>(this)->dest();
}



ParamValueList&
ArgParse::params()
{
    return m_impl->m_params;
}



const ParamValueList&
ArgParse::cparams() const
{
    return m_impl->m_params;
}



ArgParse::Arg&
ArgParse::add_argument(const char* argname)
{
    // Build a new option and then parse the values
    auto option = new ArgOption(*this, argname);
    m_impl->m_option.emplace_back(option);
    option->m_param.resize(option->nargs(), nullptr);
    option->m_paramtypes.resize(option->nargs(), TypeUnknown);

    if (option->initialize() < 0) {
        option->had_error(true);
        return *static_cast<Arg*>(m_impl->m_option.back().get());
    }

    if (argname[0] == '\0'
        || (argname[0] == '%' && argname[1] == '*' && argname[2] == '\0')) {
        // set default global option
        m_impl->m_global = option;
    } else if (argname[0] == '%' && argname[1] == '1' && argname[2] == '\0') {
        // set default pre-switch option
        m_impl->m_preoption = option;
    } else if (argname[0] != '-' && argname[0] != '<') {
        // set default global option
        m_impl->m_global = option;
    }

    return *static_cast<Arg*>(m_impl->m_option.back().get());
}



ArgParse::Arg&
ArgParse::argx(const char* argname, ...)
{
    va_list ap;
    va_start(ap, argname);

    // Build a new option and then parse the values
    std::unique_ptr<ArgOption> option(new ArgOption(*this, argname));
    if (option->initialize() < 0) {
        option->had_error(true);
        m_impl->m_option.emplace_back(std::move(option));
        va_end(ap);
        return *static_cast<Arg*>(m_impl->m_option.back().get());
    }

    if (argname[0] == '\0'
        || (argname[0] == '%' && argname[1] == '*' && argname[2] == '\0')) {
        // set default global option
        m_impl->m_global = option.get();
    }

    if (argname[0] == '%' && argname[1] == '1' && argname[2] == '\0') {
        // set default pre-switch option
        m_impl->m_preoption = option.get();
    }

    if (option->has_callback())
        option->set_callback((ArgOption::callback_t)va_arg(ap, void*));

    // Grab any parameters and store them with this option
    for (int i = 0; i < option->nargs(); i++) {
        void* p = va_arg(ap, void*);
        option->add_parameter(i, p);
        if (option.get() == m_impl->m_global
            || option.get() == m_impl->m_preoption)
            option->set_callback((ArgOption::callback_t)p);
    }

    m_impl->m_option.emplace_back(std::move(option));
    va_end(ap);
    return *static_cast<Arg*>(m_impl->m_option.back().get());
}



ArgParse::Arg&
ArgParse::separator(string_view text)
{
    return arg("<SEPARATOR>").help(text);
}



ArgParse::Arg&
ArgParse::Arg::help(string_view help)
{
    static_cast<ArgOption*>(this)->help(help);
    return *this;
}



ArgParse::Arg&
ArgParse::Arg::nargs(int n)
{
    static_cast<ArgOption*>(this)->nargs(n);
    return *this;
}



ArgParse::Arg&
ArgParse::Arg::metavar(string_view name)
{
    static_cast<ArgOption*>(this)->metavar(name);
    return *this;
}



ArgParse::Arg&
ArgParse::Arg::action(ArgAction&& action)
{
    static_cast<ArgOption*>(this)->m_action = std::move(action);
    return *this;
}



ArgParse::Arg&
ArgParse::Arg::dest(string_view dest)
{
    static_cast<ArgOption*>(this)->m_dest = dest;
    return *this;
}



ArgParse::Arg&
ArgParse::Arg::hidden()
{
    static_cast<ArgOption*>(this)->m_hidden = true;
    return *this;
}



ArgParse::Arg&
ArgParse::Arg::always_run()
{
    static_cast<ArgOption*>(this)->m_always_run = true;
    return *this;
}



ArgParse::Action
ArgParse::do_nothing()
{
    return [](cspan<const char*> myarg) { return true; };
}



ArgParse::ArgAction
ArgParse::store_true()
{
    return [](Arg& arg, cspan<const char*> myarg) {
        arg.argparse().params()[arg.dest()] = 1;
    };
}



ArgParse::ArgAction
ArgParse::store_false()
{
    return [](Arg& arg, cspan<const char*> myarg) {
        arg.argparse().params()[arg.dest()] = 0;
    };
}



// Find an option by name in the option vector
ArgOption*
ArgParse::Impl::find_option(const char* name)
{
    for (auto&& opt : m_option) {
        const char* optname = opt->flag().c_str();
        if (!strcmp(name, optname))
            return opt.get();
        // Match even if the user mixes up one dash or two
        if (name[0] == '-' && name[1] == '-' && optname[0] == '-'
            && optname[1] != '-')
            if (!strcmp(name + 1, optname))
                return opt.get();
        if (name[0] == '-' && name[1] != '-' && optname[0] == '-'
            && optname[1] == '-')
            if (!strcmp(name, optname + 1))
                return opt.get();
    }

    return nullptr;
}



ArgParse&
ArgParse::intro(string_view str)
{
    m_impl->m_intro = str;
    return *this;
}



ArgParse&
ArgParse::usage(string_view str)
{
    m_impl->m_usage = str;
    return *this;
}



ArgParse&
ArgParse::description(string_view str)
{
    m_impl->m_description = str;
    return *this;
}



ArgParse&
ArgParse::epilog(string_view str)
{
    m_impl->m_epilog = str;
    return *this;
}



ArgParse&
ArgParse::prog(string_view str)
{
    m_impl->m_prog = str;
    return *this;
}



ArgParse&
ArgParse::print_defaults(bool print)
{
    m_impl->m_print_defaults = print;
    return *this;
}



ArgParse&
ArgParse::add_help(bool add_help)
{
    m_impl->m_add_help = add_help;
    return *this;
}



ArgParse&
ArgParse::add_version(string_view version)
{
    m_impl->m_version = version;
    return *this;
}



ArgParse&
ArgParse::exit_on_error(bool exit_on_error)
{
    m_impl->m_exit_on_error = exit_on_error;
    return *this;
}



int
ArgParse::Impl::found(const char* option_name)
{
    ArgOption* option = find_option(option_name);
    return option ? option->parsed_count() : 0;
}



bool
ArgParse::has_error() const
{
    return !m_impl->m_errmessage.empty();
}



std::string
ArgParse::geterror(bool clear) const
{
    std::string e = m_impl->m_errmessage;
    if (clear)
        m_impl->m_errmessage.clear();
    return e;
}



static void
println(std::ostream& out, string_view str, int blanklines = 1)
{
    if (str.size()) {
        out << str;
        if (str.back() != '\n')
            out << '\n';
        while (blanklines-- > 0)
            out << '\n';
    }
}



std::string
ArgParse::prog_name() const
{
    return m_impl->m_prog;
}



void
ArgParse::print_help() const
{
    const size_t longline = 35;

    println(std::cout, m_impl->m_intro);
    if (m_impl->m_usage.size()) {
        std::cout << "Usage: ";
        println(std::cout, m_impl->m_usage);
    }
    println(std::cout, m_impl->m_description);

    m_impl->m_preoption_help(*this, std::cout);
    size_t maxlen = 0;

    for (auto&& opt : m_impl->m_option) {
        size_t fmtlen = opt->prettyformat().length();
        // Option lists > 40 chars will be split into multiple lines
        if (fmtlen < longline)
            maxlen = std::max(maxlen, fmtlen);
    }

    // Try to figure out how wide the terminal is, so we can word wrap.
    int columns = Sysutil::terminal_columns();

    for (auto&& opt : m_impl->m_option) {
        if (!opt->hidden() /*opt->help().length()*/) {
            size_t fmtlen = opt->prettyformat().length();
            if (opt->is_separator()) {
                std::cout << Strutil::wordwrap(opt->help(), columns - 2, 0)
                          << '\n';
            } else {
                std::cout << "    " << opt->prettyformat();
                if (fmtlen < longline)
                    std::cout << std::string(maxlen + 2 - fmtlen, ' ');
                else
                    std::cout << "\n    " << std::string(maxlen + 2, ' ');
                std::string h = opt->help();
                if (m_impl->m_print_defaults && cparams().contains(opt->dest()))
                    h += Strutil::fmt::format(" (default: {})",
                                              cparams()[opt->dest()].get());
                std::cout << Strutil::wordwrap(h, columns - 2,
                                               (int)maxlen + 2 + 4 + 2);
                std::cout << '\n';
            }
        }
    }
    m_impl->m_postoption_help(*this, std::cout);
    println(std::cout, m_impl->m_epilog, 0);
}



void
ArgParse::briefusage() const
{
    println(std::cout, m_impl->m_intro);
    if (m_impl->m_usage.size()) {
        std::cout << "Usage: ";
        println(std::cout, m_impl->m_usage);
    }

    // Try to figure out how wide the terminal is, so we can word wrap.
    int columns = Sysutil::terminal_columns();

    std::string pending;
    for (auto&& opt : m_impl->m_option) {
        if (!opt->hidden() /*opt->help().length()*/) {
            if (opt->is_separator()) {
                if (pending.size())
                    std::cout << "    "
                              << Strutil::wordwrap(pending, columns - 2, 4)
                              << '\n';
                pending.clear();
                std::cout << Strutil::wordwrap(opt->help(), columns - 2, 0)
                          << '\n';
            } else {
                pending += opt->flag() + " ";
            }
        }
    }
    if (pending.size())
        std::cout << "    " << Strutil::wordwrap(pending, columns - 2, 4)
                  << '\n';
}



std::string
ArgParse::command_line() const
{
    std::string s;
    for (int i = 0; i < m_impl->m_argc; ++i) {
        if (strchr(m_impl->m_argv[i], ' ')) {
            s += '\"';
            s += m_impl->m_argv[i];
            s += '\"';
        } else {
            s += m_impl->m_argv[i];
        }
        if (i < m_impl->m_argc - 1)
            s += ' ';
    }
    return s;
}



void
ArgParse::set_preoption_help(callback_t callback)
{
    m_impl->m_preoption_help = callback;
}



void
ArgParse::set_postoption_help(callback_t callback)
{
    m_impl->m_postoption_help = callback;
}



void
ArgParse::running(bool run)
{
    m_impl->m_running = run;
}



bool
ArgParse::running() const
{
    return m_impl->m_running;
}



void
ArgParse::abort(bool aborted)
{
    m_impl->m_aborted = aborted;
}



bool
ArgParse::aborted() const
{
    return m_impl->m_aborted;
}



int
ArgParse::current_arg() const
{
    return m_impl->m_current_arg;
}



void
ArgParse::set_next_arg(int nextarg)
{
    m_impl->m_next_arg = nextarg;
}

OIIO_NAMESPACE_END
