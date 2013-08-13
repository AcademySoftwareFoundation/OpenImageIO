///////////////////////////////////////////////////////////////////////////

//

// Copyright (c) 2006, Industrial Light & Magic, a division of Lucas

// Digital Ltd. LLC

// 

// All rights reserved.

// 

// Redistribution and use in source and binary forms, with or without

// modification, are permitted provided that the following conditions are

// met:

// *       Redistributions of source code must retain the above copyright

// notice, this list of conditions and the following disclaimer.

// *       Redistributions in binary form must reproduce the above

// copyright notice, this list of conditions and the following disclaimer

// in the documentation and/or other materials provided with the

// distribution.

// *       Neither the name of Industrial Light & Magic nor the names of

// its contributors may be used to endorse or promote products derived

// from this software without specific prior written permission. 

// 

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS

// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT

// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR

// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT

// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,

// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT

// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,

// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY

// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT

// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE

// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

//

///////////////////////////////////////////////////////////////////////////



// OptionParser

// Author: Nick Porcino

// Purpose: Sample code that implements a simple to use

// Command Line parsing class, based loosely on Python's OptionParser.

// Also included are simple versions of Join and Split





#include "IlmBaseConfig.h" /* [i_a] rude MSVC2005 hack */

#include "OptionParser.h"

#if defined(_WIN32) || defined(_WIN64) /* [i_a] */

#include <windows.h>

#endif

#include <string>

#include <vector>

#include <algorithm>

#include <iostream>

#include <assert.h>



using std::vector;

using std::string;



// takes a string, and separates out according to embedded quoted strings

// examples

// abc > abc

// abc "def" > abc, "def"

// a "def" ghi > a, "def", ghi

// a\"bc > a\"bc

// as you can see, the quotes are preserved.

// and quotes are escaped



vector<string> Separate(const std::string& input)

{

    vector<string>output;



    size_t curr = 0;

    size_t start = 0;

    size_t end = input.length();

    bool inQuotes = false;



    while (curr < end)

    {

        if (input[curr] == '\\')

        {

            ++curr;

            if (curr != end && input[curr] == '\"')

                ++curr;

        }

        else

        {

            if (input[curr] == '\"')

            {

                // no empty string if not in quotes, otherwise preserve it

                if (inQuotes || (start != curr))

                {

                    output.push_back(input.substr(start - (inQuotes?1:0), curr - start + (inQuotes?2:0)));

                }

                inQuotes = !inQuotes;

                start = curr+1;

            }

            ++curr;

        }

    }



    // catch the case of a trailing substring that was not quoted, or a completely unquoted string

    if (curr - start > 0)

        output.push_back(input.substr(start, curr-start));



    return output;

}





// given a string, split it into components, at the splitter character.

// if escapes are allowed, an escaped splitter won't split

// if empties are allowed, empty strings will get pushed, otherwise not

// ";" yields two empties if empties are allowed, zero otherwise.



vector<string> Split(const string& input, char splitter, bool escapes, bool empties)

{

    vector<string> output;

    if (input.find(splitter) == string::npos)

    {

        output.push_back(input);

    }

    else

    {

        size_t curr = 0;

        size_t start = 0;

        size_t end = input.length();

        while (curr < end)

        {

            if (escapes && input[curr] == '\\')

            {

                ++curr;

                if (curr != end && input[curr] == splitter)

                    ++curr;

            }

            else

            {

                if (input[curr] == splitter)

                {

                    if (curr>start)

                    {

                        output.push_back(input.substr(start, curr-start));

                    }

                    else if (empties && curr == 0)

                    {

                        output.push_back("");

                    }

                    start = curr+1;

                    if (empties && (input[start] == splitter || start == end))

                    {

                        output.push_back("");

                        ++start;

                        ++curr;

                    }

                }

                ++curr;

            }

        }

        if (curr - start > 0)

            output.push_back(input.substr(start, curr-start));

    }





    return output;

}



string Join(const vector<string>& input, const string& join)

{

    string result;

    vector<string>::const_iterator i;

    for (i = input.begin(); i != input.end();)

    {

        result += *i;

        if (++i != input.end())

            result += join;

    }

    return result;

}



string Join(int argc, char* argv[], const string& join)

{

    string result;

    for (int i = 0; i < argc;)

    {

        result += argv[i];

        if (++i < argc)

            result += join;

    }

    return result;

}



#ifdef _WIN32

string Join(int argc, wchar_t* argv[], const string& join)

{

    char buff[256];

    string result;

    for (int i = 0; i < argc;)

    {

        int length = WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, 0, 0, NULL, NULL);

        assert (length < 256);

        WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, buff, 256, NULL, NULL);

        result += buff;

        if (++i < argc)

            result += join;

    }

    return result;

}

#endif





vector<string> SplitCommandLine(const std::string& input)

{

    vector<string> strings = Separate(input);

    vector<string> args;

    vector<string>::const_iterator i;

    for (i = strings.begin(); i != strings.end(); ++i)

    {

        const string& test = *i;

        if (test[0] == '\"')

            args.push_back(test);

        else

        {

            vector<string> newstrings = Split(*i, ' ');

            for (vector<string>::const_iterator j = newstrings.begin(); j != newstrings.end(); ++j)

            {

                args.push_back(*j);

            }

        }

    }

    return args;

}





class SetTrueOption : public OptionParser::Option

{

public:

    SetTrueOption(const std::string& shortNameString, 

        const std::string& longNameString,

        bool& modMe,

        const std::string& helpString) : val(modMe)

    {

        shortName = shortNameString;

        longName = longNameString;

        help = helpString;

    }



    virtual bool HasArgument() const { return false; }

    virtual std::string ArgumentType() const { return ""; }

    virtual bool Action(const std::string& s) 

    { 

        val = true; 

        if (OptionParser::Verbose())

            std::cout << "Setting flag: " << help << std::endl;

        return true;

    }



    bool& val;

};



class SetFalseOption : public OptionParser::Option

{

public:

    SetFalseOption(const std::string& shortNameString, 

        const std::string& longNameString,

        bool& modMe,

        const std::string& helpString) : val(modMe)

    {

        shortName = shortNameString;

        longName = longNameString;

        help = helpString;

    }



    virtual bool HasArgument() const { return false; }

    virtual std::string ArgumentType() const { return ""; }

    virtual bool Action(const std::string& s)

    { 

        val = false; 

        if (OptionParser::Verbose())

            std::cout << "Clearing flag: " << help << std::endl;

        return true;

    }



    bool& val;

};



class SetStringOption : public OptionParser::Option

{

public:

    SetStringOption(const std::string& shortNameString, 

        const std::string& longNameString,

        std::string& modMe,

        const std::string& helpString) : val(modMe)

    {

        shortName = shortNameString;

        longName = longNameString;

        help = helpString;

    }



    virtual bool HasArgument() const { return true; }

    virtual std::string ArgumentType() const { return "string"; }

    virtual bool Action(const std::string& s)

    { 

        val = s; 

        if (OptionParser::Verbose())

            std::cout << "Setting string: " << help << "=" << s << std::endl;

        return true;

    }



    string& val;

};



class SetOptionalStringOption : public SetStringOption

{

public:

    SetOptionalStringOption(const std::string& shortNameString, 

        const std::string& longNameString,

        std::string& modMe,

        const std::string& helpString) : SetStringOption(shortNameString, longNameString, modMe, helpString)

    {

    }



    virtual bool HasArgument() const { return false; }

};



class SetStringVectorOption : public OptionParser::Option

{

public:

    SetStringVectorOption(const std::string& shortNameString, 

        const std::string& longNameString,

        std::vector<std::string>& modMe,

        const std::string& helpString) : val(modMe), past_first_invocation(false)

    {

        shortName = shortNameString;

        longName = longNameString;

        help = helpString;

    }



    virtual bool HasArgument() const { return true; }

    virtual std::string ArgumentType() const { return "vector<string>"; }

    virtual bool Action(const std::string& s)

    { 

		// clear any preset when we hit this option the first time around:

		if (!past_first_invocation)

		{

			val.clear();

			past_first_invocation = true;

		}



		vector<string> newval;



        if (s.find(';') != string::npos)

        {

            newval = Split(s, ';');

        }

        else if (s.find(',') != string::npos)

        {

            newval = Split(s, ',');

        }

        else

        {

            newval.clear();

            newval.push_back(s);

        }

		// append to existing value:

	    for (vector<string>::const_iterator i = newval.begin(); i != newval.end(); ++i)

		{

			val.push_back(*i);

		}



        if (OptionParser::Verbose())

		{

            std::cout << "Setting string vector: " << help << "="; // << s << std::endl;

			for (size_t i = 0; i < val.size(); ++i)

			{

				std::cout << (i ? ";" : "") << val[i].c_str();

			}

            std::cout << std::endl;

		}



        return true;

    }



    vector<string>& val;

	bool past_first_invocation;

};



class SetIntOption : public OptionParser::Option

{

public:

    SetIntOption(const std::string& shortNameString, 

        const std::string& longNameString,

        int& modMe,

        const std::string& helpString) : val(modMe)

    {

        shortName = shortNameString;

        longName = longNameString;

        help = helpString;

    }



    virtual bool HasArgument() const { return true; }

    virtual std::string ArgumentType() const { return "int"; }

    virtual bool Action(const std::string& s)

    { 

        val = atoi(s.c_str()); 

        if (OptionParser::Verbose())

            std::cout << "Setting int: " << help << "=" << s << std::endl;

        return true;

    }



    int& val;

};



class SetFloatOption : public OptionParser::Option

{

public:

    SetFloatOption(const std::string& shortNameString, 

        const std::string& longNameString,

        float& modMe,

        const std::string& helpString) : val(modMe)

    {

        shortName = shortNameString;

        longName = longNameString;

        help = helpString;

    }



    virtual bool HasArgument() const { return true; }

    virtual std::string ArgumentType() const { return "float"; }

    virtual bool Action(const std::string& s)

    { 

        val = (float) atof(s.c_str()); 

        if (OptionParser::Verbose())

            std::cout << "Setting float: " << help << "=" << s << std::endl;

        return true;

    }



    float& val;

};



class SetCallbackOption : public OptionParser::Option

{

public:

    SetCallbackOption(const std::string& shortNameString, 

        const std::string& longNameString,

        void (*callbackPtr)(),

        const std::string& helpString)

    {

        callback = callbackPtr;

        shortName = shortNameString;

        longName = longNameString;

        help = helpString;

    }



    virtual bool HasArgument() const { return false; }

    virtual std::string ArgumentType() const { return "callback"; }

    virtual bool Action(const std::string& s)

    { 

        if (OptionParser::Verbose())

            std::cout << "Calling: " << help << std::endl;

        if (callback)

            callback();

        return true;

    }



    void (*callback)();

};





OptionParser::OptionParser(const std::string& appName) : stringCallback(0)

{

    name = appName;

}



OptionParser::~OptionParser()

{

    for (std::vector<Option*>::iterator i = options.begin(); i != options.end(); ++i)

    {

        delete (*i);

    }

}



namespace

{

    bool optionParserVerbose = true;

}



bool OptionParser::Verbose()

{

    return optionParserVerbose;

}



void Verbose(bool v)

{

    optionParserVerbose = v;

}







void OptionParser::StringCallback(

                                  void (*callback)(const std::string&),

                                  const std::string& help)

{

    stringCallback = callback;

    stringCallbackHelp = help;

}



void OptionParser::AddTrueOption(

                                 const std::string& shortOption, // eg "-f"

                                 const std::string& longOption,  // eg "--file"

                                 bool& modMe, 

                                 const std::string& help)

{

    options.push_back(new SetTrueOption(shortOption, longOption, modMe, help));

}



void OptionParser::AddFalseOption(

                                  const std::string& shortOption, // eg "-f"

                                  const std::string& longOption,  // eg "--file"

                                  bool& modMe, 

                                  const std::string& help)

{

    options.push_back(new SetFalseOption(shortOption, longOption, modMe, help));

}



void OptionParser::AddStringOption(

                                   const std::string& shortOption, // eg "-f"

                                   const std::string& longOption,  // eg "--file"

                                   std::string& modMe, 

                                   const std::string& help)

{

    options.push_back(new SetStringOption(shortOption, longOption, modMe, help));

}



void OptionParser::AddOptionalStringOption(

                                   const std::string& shortOption, // eg "-f"

                                   const std::string& longOption,  // eg "--file"

                                   std::string& modMe, 

                                   const std::string& help)

{

    options.push_back(new SetOptionalStringOption(shortOption, longOption, modMe, help));

}



void OptionParser::AddStringVectorOption(

                                   const std::string& shortOption, // eg "-f"

                                   const std::string& longOption,  // eg "--file"

                                   std::vector<std::string>& modMe, 

                                   const std::string& help)

{

    options.push_back(new SetStringVectorOption(shortOption, longOption, modMe, help));

}



void OptionParser::AddIntOption(

                                const std::string& shortOption, // eg "-f"

                                const std::string& longOption,  // eg "--file"

                                int& modMe, 

                                const std::string& help)

{

    options.push_back(new SetIntOption(shortOption, longOption, modMe, help));

}



void OptionParser::AddFloatOption(

                                  const std::string& shortOption, // eg "-f"

                                  const std::string& longOption,  // eg "--file"

                                  float& modMe, 

                                  const std::string& help)

{

    options.push_back(new SetFloatOption(shortOption, longOption, modMe, help));

}



//void OptionParser::AddFloat3Option(

//                                   const std::string& shortOption, // eg "-f"

//                                   const std::string& longOption,  // eg "--file"

//                                   float* modMe, 

//                                   const std::string& help)

//{

//}

//

//void OptionParser::AddFloat4Option(

//                                   const std::string& shortOption, // eg "-f"

//                                   const std::string& longOption,  // eg "--file"

//                                   float* modMe, 

//                                   const std::string& help)

//{

//}



void OptionParser::AddCallbackOption(

                                     const std::string& shortOption, // eg "-f"

                                     const std::string& longOption,  // eg "--file"

                                     void (*callback)(), 

                                     const std::string& help)

{

    options.push_back(new SetCallbackOption(shortOption, longOption, callback, help));

}



// default is to start parsing just beyond 0, which is the executable name

bool OptionParser::Parse(const std::string& commandLine, int firstOption)

{

    vector<string> args = SplitCommandLine(commandLine);



    enum ArgType { stringArg, shortArg, longArg };



    for (vector<string>::const_iterator i = args.begin(); i != args.end(); ++i)

    {

        ArgType argType;

        if ((*i)[0] == '-')

        {

            argType = (*i)[1] == '-' ? longArg : shortArg;

            if ((argType == shortArg && (*i).length() == 1) ||

                (argType == longArg && (*i).length() == 2))

            {

                std::cout << "Malformed argument: " << (*i) << std::endl;

                return false;

            }



            std::string arg = *i;

            std::string val;

            bool gotArg = false;



            // if there's an '=', split on the '='

            if (arg.find('=') != string::npos)

            {

                vector<string> split = Split(arg, '=', false, true);

                if (split.size() != 2)

                {

                    std::cout << "Malformed argument: " << (*i) << std::endl;

                    return false;

                }

                arg = split[0];

                val = split[1];

                gotArg = true;

            }

            // otherwise, if it's a short argument, like -foo, split after the first letter f, oo

            else if (argType == shortArg)

            {

                if (arg.length() > 2)

                {

                    val = arg.substr(2, arg.length()-2);

                    arg = arg.substr(0, 2);

                    gotArg = true;

                }

            }



            vector<Option*>::const_iterator j;

            for (j = options.begin(); j != options.end(); ++j)

            {

                if ((argType == shortArg) && ((*j)->ShortName() == arg) ||

                    ((*j)->LongName() == arg))

                {

                    if ((*j)->HasArgument())

                    {

                        if (!gotArg)

                        {

                            ++i;

                            if (i == args.end())

                            {

                                std::cout << "Missing argument for option: " << arg << " (" << (*j)->Help() << ")" << std::endl;

                                return false;

                            }

                            val = *i;

                        }

                    }



                    if (!(*j)->Action(val))

                    {

                        std::cout << "Bad argument for option: " << arg << " (" << (*j)->Help() << ")" << std::endl;

                        return false;

                    }

                    break;

                }

            }

            if (j == options.end())

            {

                if (arg == "-h" || arg == "--help" || arg == "/?" || arg == "-?" || arg == "?")

                {

                    Usage();

                }

                else

                {

                    std::cout << "Unknown option: " << arg << std::endl;

                    // could call unknown option callback here.

                    return false;

                }

            }

        }

        else

        {

            if (stringCallback)

                stringCallback(*i);

            else

            {

                std::cout << "Malformed argument: " << (*i) << std::endl;

                return false;

            }

        }

    }

    return true;

}



// send all the Options' help strings to the output

// uses argument 0 to know what the executable name is

void OptionParser::Usage()

{

    std::cout << std::endl << "usage: " << name << " [options]" << std::endl << std::endl << "options:" << std::endl;

    std::sort(options.begin(), options.end(), AscendingOptionSort());

    for (vector<Option*>::iterator i = options.begin(); i != options.end(); ++i)

    {

        std::cout << "  ";

        if ((*i)->HasShortName())

        {

            std::cout << (*i)->ShortName();

            if ((*i)->HasArgument())

                std::cout << " " << (*i)->ArgumentType();

            if ((*i)->HasLongName())

                std::cout << ", ";

        }

        if ((*i)->HasLongName())

        {

            std::cout << (*i)->LongName();

            if ((*i)->HasArgument())

                std::cout << " " << (*i)->ArgumentType();

        }

        else

            std::cerr << "\t";

        std::cout << "\t" << (*i)->Help() << std::endl;

    }

    if (stringCallbackHelp.length() > 0)

        std::cout << "Stand alone string: " << stringCallbackHelp << std::endl;



    std::cout << std::endl;

}



std::string OptionParser::Canonicalize(const std::string& commandLine)

{

    std::string result;

    vector<string> args = SplitCommandLine(commandLine);



    enum ArgType { stringArg, shortArg, longArg };



    for (vector<string>::const_iterator i = args.begin(); i != args.end(); ++i)

    {

        ArgType argType;

        if ((*i)[0] == '-')

        {

            argType = (*i)[1] == '-' ? longArg : shortArg;

            if ((argType == shortArg && (*i).length() == 1) ||

                (argType == longArg && (*i).length() == 2))

            {

                std::cout << "Malformed argument: " << (*i) << std::endl;

                return false;

            }



            std::string arg = *i;

            std::string val;

            bool gotArg = false;



            if (arg.find('=') != string::npos)

            {

                vector<string> split = Split(arg, '=', false, true);

                if (split.size() != 2)

                {

                    std::cout << "Malformed argument: " << (*i) << std::endl;

                    return false;

                }

                arg = split[0];

                val = split[1];

                gotArg = true;

            }



            vector<Option*>::const_iterator j;

            for (j = options.begin(); j != options.end(); ++j)

            {

                if ((argType == shortArg) && ((*j)->ShortName() == arg) ||

                    ((*j)->LongName() == arg))

                {

                    if ((*j)->HasArgument())

                    {

                        if (!gotArg)

                        {

                            ++i;

                            if (i == args.end())

                            {

                                std::cout << "Missing argument for option: " << arg << " (" << (*j)->Help() << ")" << std::endl;

                                return false;

                            }

                            val = *i;

                        }

                    }



                    if (result.length() > 0)

                        result += ' ';

                    if ((*j)->LongName().length() > 0)

                        result += (*j)->LongName();

                    else

                        result += (*j)->ShortName();

                    

                    if ((*j)->HasArgument())

                    {

                        result += '=';

                        result += val;

                    }

                    break;

                }

            }

            if (j == options.end())

            {

                if (result.length() > 0)

                    result += ' ';

                result += "UNKNOWN-ARG";

            }

        }

        else

        {

            if (result.length() > 0)

                result += ' ';



            result += *i;

        }

    }

    return result;

}

