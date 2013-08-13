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





// arguments can be of the following forms

// string

// "string with spaces"

// -f

// -fname

// -f=name (equivalent)

// -f name (equivalent)

// --foo

// --foo name

// --foo=name



// where name

// can be 

// string

// "quoted string"

// string,separated,by,commas,no,spaces,to,specify,a,list



// Strings separated by commas, no spaces is only valid for

// the string list option.



// for a string list option,

// -fname1

// -fname2

// will also add two things to the same list.





#ifndef OPTIONPARSER_H

#define OPTIONPARSER_H



#include "IlmBaseConfig.h" /* [i_a] rude MSVC2005 hack */

#include <string>

#include <vector>



class OptionParser

{

public:

    class Option

    {

    public:

        Option()

        {

        }



        virtual ~Option()

        {

        }



        bool            HasShortName() const { return shortName.length() > 0; }

        bool            HasLongName() const { return longName.length() > 0; }

        std::string     ShortName() const { return shortName; }

        std::string     LongName() const { return longName; }

        std::string     Help() const { return help; }



        virtual bool        HasArgument() const = 0;

        virtual std::string ArgumentType() const = 0;

        virtual bool        Action(const std::string& s) = 0;



        std::string shortName;

        std::string longName;

        std::string help;

    };



    // Ascending date sorting function

    struct AscendingOptionSort

    {

        bool operator()(Option*& start, Option*& end)

        {

            if (start->HasShortName() && end->HasShortName())

            {

                return start->ShortName() < end->ShortName();

            }

            else if (start->HasShortName() && end->HasLongName())

            {

                return start->ShortName() < end->LongName();

            }

            else if (start->HasLongName() && end->HasShortName())

            {

                return start->LongName() < end->ShortName();

            }

            else // (start->HasLongName() && end->HasLongName())

            {

                return start->LongName() < end->LongName();

            }

        }

    };





public:

    OptionParser(const std::string& appName);

    ~OptionParser();



    void AddTrueOption(

        const std::string& shortOption, // eg "-f"

        const std::string& longOption,  // eg "--file"

        bool& modMe, 

        const std::string& help);

    void AddFalseOption(

        const std::string& shortOption, // eg "-f"

        const std::string& longOption,  // eg "--file"

        bool& modMe, 

        const std::string& help);

    void AddStringOption(

        const std::string& shortOption, // eg "-f"

        const std::string& longOption,  // eg "--file"

        std::string& modMe, 

        const std::string& help);

    void AddOptionalStringOption(

        const std::string& shortOption, // eg "-f"

        const std::string& longOption,  // eg "--file"

        std::string& modMe, 

        const std::string& help);

    void AddStringVectorOption(

        const std::string& shortOption, // eg "-f"

        const std::string& longOption,  // eg "--file"

        std::vector<std::string>& modMe, 

        const std::string& help);

    void AddIntOption(

        const std::string& shortOption, // eg "-f"

        const std::string& longOption,  // eg "--file"

        int& modMe, 

        const std::string& help);

    void AddFloatOption(

        const std::string& shortOption, // eg "-f"

        const std::string& longOption,  // eg "--file"

        float& modMe, 

        const std::string& help);

    //void AddFloat3Option(

    //    const std::string& shortOption, // eg "-f"

    //    const std::string& longOption,  // eg "--file"

    //    float* modMe, 

    //    const std::string& help);

    //void AddFloat4Option(

    //    const std::string& shortOption, // eg "-f"

    //    const std::string& longOption,  // eg "--file"

    //    float* modMe, 

    //    const std::string& help);

    void AddCallbackOption(

        const std::string& shortOption, // eg "-f"

        const std::string& longOption,  // eg "--file"

        void (*callback)(), 

        const std::string& help);

    void StringCallback(

        void (*callback)(const std::string&),

        const std::string& help);



    // default is to start parsing just beyond 0, which is the executable name

    bool Parse(const std::string& commandLine, int firstOption = 0);



    // send all the Options' help strings to the output

    // uses argument 0 to know what the executable name is

    void Usage();



    std::string Canonicalize(const std::string& commandLine);



    static bool Verbose();

    static void Verbose(bool);



protected:

    void (*stringCallback)(const std::string&);

    std::string stringCallbackHelp;

    std::vector<Option*> options;

    std::string name;

};



// utility functions for OptionParser

// these are generally useful and so are exposed here



// takes a string, and separates out according to embedded quoted strings

// examples

// abc > abc

// abc "def" > abc, "def"

// a "def" ghi > a, "def", ghi

// a\"bc > a\"bc

// as you can see, the quotes are preserved.

// and quotes are escaped



std::vector<std::string> Separate(const std::string& input);



// given a string, split it into components, at the splitter character.

// if escapes are allowed, an escaped splitter won't split

// if empties are allowed, empty strings will get pushed, otherwise not

// ";" yields two empties if empties are allowed, zero otherwise.



std::vector<std::string> Split(const std::string& input, char splitter, bool escapes=false, bool empties=false);



// given a vector of strings, join them all together with the join string

// in between each one



std::string Join(const std::vector<std::string>& input, const std::string& join);

std::string Join(int argc, char* argv[], const std::string& join);

#ifdef _WIN32

std::string Join(int argc, wchar_t* argv[], const std::string& join);

#endif



// given a joined command line, this re-splits it, respecting quoted

// contained strings. This way, no matter WHAT the OS thinks a good

// grouping of quoted strings is in an agrv list, the end result

// will be completely consistent; a list of strings that are not

// quoted, with a list of strings that are quoted.



std::vector<std::string> SplitCommandLine(const std::string& input);



#endif

