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


#include <cstring>
#include <cctype>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <cstdarg>
#include <iterator>
#include <string>
#include <sstream>

#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/argparse.h>
#include <OpenImageIO/dassert.h>

OIIO_NAMESPACE_BEGIN

class ArgOption {
public:
    typedef int (*callback_t) (int, const char**);

    ArgOption (const char *str);
    ~ArgOption () { }
    
    int initialize ();
    
    int parameter_count () const { return m_count; }
    const std::string & name() const { return m_flag; }

    const std::string & fmt() const { return m_format; }

    bool is_flag () const { return m_type == Flag; }
    bool is_reverse_flag () const { return m_type == ReverseFlag; }
    bool is_sublist () const { return m_type == Sublist; }
    bool is_regular () const { return m_type == Regular; }
    bool has_callback () const { return m_has_callback; }
    
    void add_parameter (int i, void *p);

    void set_parameter (int i, const char *argv);

    void add_argument (const char *argv);
    int invoke_callback () const;

    int invoke_callback (int argc, const char **argv) const {
        return m_callback ? m_callback (argc, argv) : 0;
    }

    void set_callback (callback_t cb) { m_callback = cb; }

    void found_on_command_line () { m_repetitions++; }
    int parsed_count () const { return m_repetitions; }

    void description (const char *d) { m_descript = d; }
    const std::string & description() const { return m_descript; }

    bool is_separator () const { return fmt() == "<SEPARATOR>"; }
private:
    enum OptionType { None, Regular, Flag, ReverseFlag, Sublist };

    std::string m_format;                         // original format string
    std::string m_flag;                           // just the -flag_foo part
    std::string m_code;                           // paramter types, eg "df"
    std::string m_descript;
    OptionType m_type = None;
    int m_count = 0;                              // number of parameters
    std::vector<void *> m_param;                  // pointers to app data vars
    callback_t m_callback = nullptr;
    int m_repetitions = 0;                        // number of times on cmd line
    bool m_has_callback = false;                  // needs a callback?
    std::vector<std::string> m_argv;
};



class ArgParse::Impl {
public:
    int m_argc;                           // a copy of the command line argc
    const char **m_argv;                  // a copy of the command line argv
    mutable std::string m_errmessage;     // error message
    ArgOption *m_global;                  // option for extra cmd line arguments
    std::string m_intro;
    std::vector<std::unique_ptr<ArgOption>> m_option;
    callback_t m_preoption_help = [](const ArgParse& ap, std::ostream&){};
    callback_t m_postoption_help = [](const ArgParse& ap, std::ostream&){};

    Impl (int argc, const char **argv)
        : m_argc(argc), m_argv(argv)
        {}

    int parse (int argc, const char **argv);

    ArgOption *find_option(const char *name);
    int found (const char *option);      // number of times option was parsed

    template<typename... Args>
    void error (string_view fmt, const Args&... args) const {
        m_errmessage = Strutil::format (fmt, args...);
    }
};



// Constructor.  Does not do any parsing or error checking.
// Make sure to call initialize() right after construction.
ArgOption::ArgOption (const char *str) 
    : m_format(str)
{
}



// Parses the format string ("-option %s %d %f") to extract the
// flag ("-option") and create a code string ("sdf").  After the
// code string is created, the param list of void* pointers is
// allocated to hold the argument variable pointers.
int
ArgOption::initialize()
{
    size_t n;
    const char *s;

    if (m_format.empty() || m_format == "%*") {
        m_type = Sublist;
        m_count = 1;                      // sublist callback function pointer
        m_code = "*";
        m_flag = "";
    } else if (is_separator()) {
    } else {
        // extract the flag name
        s = &m_format[0];
        assert(*s == '-');
        assert(isalpha(s[1]) || (s[1] == '-' && isalpha(s[2])));
    
        s++;
        if (*s == '-')
            s++;

        while (isalnum(*s) || *s == '_' || *s == '-') s++;

        if (! *s) {
            m_flag = m_format;
            m_type = Flag;
            m_count = 1;
            m_code = "b";
        } else {
            n = s - (&m_format[0]);
            m_flag.assign (m_format.begin(), m_format.begin()+n);

            // Parse the scanf-like parameters

            m_type = Regular;
            m_code.clear ();
    
            while (*s != '\0') {
                if (*s == '%') {
                    s++;
                    assert(*s != '\0');
            
                    m_count++;                    // adding another parameter
            
                    switch (*s) {
                    case 'd':                   // 32bit int
                    case 'g':                   // float
                    case 'f':                   // float
                    case 'F':                   // double
                    case 's':                   // string
                    case 'L':                   // vector<string>
                        assert (m_type == Regular);
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
                    case '@':
                        m_has_callback = true;
                        --m_count;
                        break;
                    default:
                        std::cerr << "Programmer error:  Unknown option ";
                        std::cerr << "type string \"" << *s << "\"" << "\n";
                        abort();
                    }
                }
        
                s++;
            }

            // Catch the case where only a callback was given, it's still
            // a bool.
            if (! *s && m_count == 0 && m_has_callback) {
                m_type = Flag;
                m_count = 1;
                m_code = "b";
            }
        }
    }
    
    // A few replacements to tidy up the format string for printing
    size_t loc;
    while ((loc = m_format.find("%L")) != std::string::npos)
        m_format.replace (loc, 2, "%s");
    while ((loc = m_format.find("%!")) != std::string::npos)
        m_format.replace (loc, 2, "");
    while ((loc = m_format.find("%@")) != std::string::npos)
        m_format.replace (loc, 2, "");

    // Allocate space for the parameter pointers and initialize to NULL
    m_param.resize (m_count, NULL);

    return 0;
}



// Stores the pointer to an argument in the param list and
// initializes flag options to FALSE.
// FIXME -- there is no such initialization.  Bug?
void
ArgOption::add_parameter (int i, void *p)
{
    assert (i >= 0 && i < m_count);
    m_param[i] = p;
}



// Given a string from argv, set the associated option parameter
// at index i using the format conversion code in the code string.
void
ArgOption::set_parameter (int i, const char *argv)
{
    assert(i < m_count);
    
    if (! m_param[i])   // If they passed NULL as the address,
        return;         // don't write anything.

    switch (m_code[i]) {
    case 'd':
        *(int *)m_param[i] = atoi(argv);
        break;

    case 'f':
    case 'g':
        *(float *)m_param[i] = Strutil::stof(argv);
        break;

    case 'F':
        *(double *)m_param[i] = Strutil::stod(argv);
        break;

    case 's':
        *(std::string *)m_param[i] = argv;
        break;

    case 'S':
        *(std::string *)m_param[i] = argv;
        break;

    case 'L':
        ((std::vector<std::string> *)m_param[i])->push_back (argv);
        break;

    case 'b':
        *(bool *)m_param[i] = true;
        break;
    case '!':
        *(bool *)m_param[i] = false;
        break;
        
    case '*':
    default:
        abort();
    }
}



// Call the sublist callback if any arguments have been parsed
int
ArgOption::invoke_callback () const
{
    assert (m_count == 1);

    int argc = (int) m_argv.size();
    if (argc == 0)
        return 0;

    // Convert the argv's to char*[]
    const char **myargv = (const char **) alloca (argc * sizeof(const char *));
    for (int i = 0;  i < argc;  ++i)
        myargv[i] = m_argv[i].c_str();
    return invoke_callback (argc, myargv);
}



// Add an argument to this sublist option
void
ArgOption::add_argument (const char *argv)
{
    m_argv.emplace_back(argv);
}





ArgParse::ArgParse (int argc, const char **argv)
    : m_impl(new Impl(argc, argv))
{
}



ArgParse::~ArgParse()
{
}



// Top level command line parsing function called after all options
// have been parsed and created from the format strings.  This function
// parses the command line (argc,argv) stored internally in the constructor.
// Each command line argument is parsed and checked to see if it matches an
// existing option.  If there is no match, and error is reported and the
// function returns early.  If there is a match, all the arguments for
// that option are parsed and the associated variables are set.
int
ArgParse::parse (int xargc, const char **xargv)
{
    return m_impl->parse (xargc, xargv);
}


int
ArgParse::Impl::parse (int xargc, const char **xargv)
{
    m_argc = xargc;
    m_argv = xargv;

    for (int i = 1; i < m_argc; i++) {
        if (m_argv[i][0] == '-' && 
              (isalpha (m_argv[i][1]) || m_argv[i][1] == '-')) {     // flag
            // Look up only the part before a ':'
            std::string argname = m_argv[i];
            size_t colon = argname.find_first_of (':');
            if (colon != std::string::npos)
                argname.erase (colon, std::string::npos);
            ArgOption *option = find_option (argname.c_str());
            if (option == NULL) {
                error ("Invalid option \"%s\"", m_argv[i]);
                return -1;
            }

            option->found_on_command_line();
            
            if (option->is_flag() || option->is_reverse_flag()) {
                option->set_parameter(0, NULL);
                if (option->has_callback())
                    option->invoke_callback (1, m_argv+i);
            } else {
                ASSERT (option->is_regular());
                for (int j = 0; j < option->parameter_count(); j++) {
                    if (j+i+1 >= m_argc) {
                        error ("Missing parameter %d from option "
                                      "\"%s\"", j+1, option->name().c_str());
                        return -1;
                    }
                    option->set_parameter (j, m_argv[i+j+1]);
                }
                if (option->has_callback())
                    option->invoke_callback (1+option->parameter_count(), m_argv+i);
                i += option->parameter_count();
            }
        } else {
            // not an option nor an option parameter, glob onto global list
            if (m_global)
                m_global->invoke_callback (1, m_argv+i);
            else {
                error ("Argument \"%s\" does not have an associated "
                    "option", m_argv[i]);
                return -1;
            }
        }
    }

    return 0;
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
ArgParse::options (const char *intro, ...)
{
    va_list ap;
    va_start (ap, intro);

    m_impl->m_intro += intro;
    for (const char *cur = va_arg(ap, char *); cur; cur = va_arg(ap, char *)) {
        if (m_impl->find_option (cur) &&
                strcmp(cur, "<SEPARATOR>")) {
            m_impl->error ("Option \"%s\" is multiply defined", cur);
            return -1;
        }
        
        // Build a new option and then parse the values
        std::unique_ptr<ArgOption> option (new ArgOption (cur));
        if (option->initialize() < 0) {
            return -1;
        }

        if (cur[0] == '\0' ||
            (cur[0] == '%' && cur[1] == '*' && cur[2] == '\0')) {
            // set default global option
            m_impl->m_global = option.get();
        }

        if (option->has_callback())
            option->set_callback ((ArgOption::callback_t)va_arg(ap,void*));

        // Grab any parameters and store them with this option
        for (int i = 0; i < option->parameter_count(); i++) {
            void *p = va_arg (ap, void *);
            option->add_parameter (i, p);
            if (option.get() == m_impl->m_global)
                option->set_callback ((ArgOption::callback_t)p);
        }

        // Last argument is description
        option->description ((const char *) va_arg (ap, const char *));
        m_impl->m_option.emplace_back (std::move(option));
    }

    va_end (ap);
    return 0;
}



// Find an option by name in the option vector
ArgOption *
ArgParse::Impl::find_option (const char *name)
{
    for (auto&& opt : m_option) {
        const char *optname = opt->name().c_str();
        if (! strcmp(name, optname))
            return opt.get();
        // Match even if the user mixes up one dash or two
        if (name[0] == '-' && name[1] == '-' && optname[0] == '-' && optname[1] != '-')
            if (! strcmp (name+1, optname))
                return opt.get();
        if (name[0] == '-' && name[1] != '-' && optname[0] == '-' && optname[1] == '-')
            if (! strcmp (name, optname+1))
                return opt.get();
    }

    return NULL;
}



int
ArgParse::Impl::found (const char *option_name)
{
    ArgOption *option = find_option(option_name);
    return option ? 0 : option->parsed_count();
}



std::string
ArgParse::geterror () const
{
    std::string e = m_impl->m_errmessage;
    m_impl->m_errmessage.clear ();
    return e;
}



void
ArgParse::usage () const
{
    const size_t longline = 35;
    std::cout << m_impl->m_intro << '\n';
    m_impl->m_preoption_help (*this, std::cout);
    size_t maxlen = 0;
    
    for (auto&& opt : m_impl->m_option) {
        size_t fmtlen = opt->fmt().length();
        // Option lists > 40 chars will be split into multiple lines
        if (fmtlen < longline)
            maxlen = std::max (maxlen, fmtlen);
    }

    // Try to figure out how wide the terminal is, so we can word wrap.
    int columns = Sysutil::terminal_columns ();

    for (auto&& opt : m_impl->m_option) {
        if (opt->description().length()) {
            size_t fmtlen = opt->fmt().length();
            if (opt->is_separator()) {
                std::cout << Strutil::wordwrap(opt->description(), columns-2, 0) << '\n';
            } else {
                std::cout << "    " << opt->fmt();
                if (fmtlen < longline)
                    std::cout << std::string (maxlen + 2 - fmtlen, ' ');
                else
                    std::cout << "\n    " << std::string (maxlen + 2, ' ');
                std::cout << Strutil::wordwrap(opt->description(), columns-2, (int)maxlen+2+4+2) << '\n';
            }
        }
    }
    m_impl->m_postoption_help (*this, std::cout);
}



void
ArgParse::briefusage () const
{
    std::cout << m_impl->m_intro << '\n';
    // Try to figure out how wide the terminal is, so we can word wrap.
    int columns = Sysutil::terminal_columns ();

    std::string pending;
    for (auto&& opt : m_impl->m_option) {
        if (opt->description().length()) {
            if (opt->is_separator()) {
                if (pending.size())
                    std::cout << "    " << Strutil::wordwrap(pending, columns-2, 4) << '\n';
                pending.clear ();
                std::cout << Strutil::wordwrap(opt->description(), columns-2, 0) << '\n';
            } else {
                pending += opt->name() + " ";
            }
        }
    }
    if (pending.size())
        std::cout << "    " << Strutil::wordwrap(pending, columns-2, 4) << '\n';
}



std::string
ArgParse::command_line () const
{
    std::string s;
    for (int i = 0;  i < m_impl->m_argc;  ++i) {
        if (strchr (m_impl->m_argv[i], ' ')) {
            s += '\"';
            s += m_impl->m_argv[i];
            s += '\"';
        } else {
            s += m_impl->m_argv[i];
        }
        if (i < m_impl->m_argc-1)
            s += ' ';
    }
    return s;
}



void
ArgParse::set_preoption_help (callback_t callback)
{
    m_impl->m_preoption_help = callback;
}



void
ArgParse::set_postoption_help (callback_t callback)
{
    m_impl->m_postoption_help = callback;
}



OIIO_NAMESPACE_END
