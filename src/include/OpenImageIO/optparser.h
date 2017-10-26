/*
  Copyright 2011 Larry Gritz and the other authors and contributors.
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
/// @file  optparser.h
///
/// @brief Option parser template
/////////////////////////////////////////////////////////////////////////


#ifndef OPENIMAGEIO_OPTPARSER_H
#define OPENIMAGEIO_OPTPARSER_H

#include <string>
#include <OpenImageIO/strutil.h>

OIIO_NAMESPACE_BEGIN


/// Parse a string of the form "name=value" and then call
/// system.attribute (name, value), with appropriate type conversions.
template<class C>
inline bool
optparse1 (C &system, const std::string &opt)
{
    std::string::size_type eq_pos = opt.find_first_of ("=");
    if (eq_pos == std::string::npos) {
        // malformed option
        return false;
    }
    std::string name (opt, 0, eq_pos);
    // trim the name
    while (name.size() && name[0] == ' ')
        name.erase (0);
    while (name.size() && name[name.size()-1] == ' ')
        name.erase (name.size()-1);
    std::string value (opt, eq_pos+1, std::string::npos);
    if (name.empty())
        return false;
    char v = value.size() ? value[0] : ' ';
    if ((v >= '0' && v <= '9') || v == '+' || v == '-') {  // numeric
        if (strchr (value.c_str(), '.'))  // float
            return system.attribute (name, Strutil::stof(value));
        else  // int
            return system.attribute (name, Strutil::stoi(value));
    }
    // otherwise treat it as a string

    // trim surrounding double quotes
    if (value.size() >= 2 &&
            value[0] == '\"' && value[value.size()-1] == '\"')
        value = std::string (value, 1, value.size()-2);

    return system.attribute (name.c_str(), value.c_str());
}



/// Parse a string with comma-separated name=value directives, calling
/// system.attribute(name,value) for each one, with appropriate type
/// conversions.  Examples:
///    optparser(texturesystem, "verbose=1");
///    optparser(texturesystem, "max_memory_MB=32.0");
///    optparser(texturesystem, "a=1,b=2,c=3.14,d=\"a string\"");
template<class C>
inline bool
optparser (C &system, const std::string &optstring)
{
    bool ok = true;
    size_t len = optstring.length();
    size_t pos = 0;
    while (pos < len) {
        std::string opt;
        bool inquote = false;
        while (pos < len) {
            unsigned char c = optstring[pos];
            if (c == '\"') {
                // Hit a double quote -- toggle "inquote" and add the quote
                inquote = !inquote;
                opt += c;
                ++pos;
            } else if (c == ',' && !inquote) {
                // Hit a comma and not inside a quote -- we have an option
                ++pos;  // skip the comma
                break;  // done with option
            } else {
                // Anything else: add to the option
                opt += c;
                ++pos;
            }
        }
        // At this point, opt holds an option
        ok &= optparse1 (system, opt);
    }
    return ok;
}


OIIO_NAMESPACE_END

#endif // OPENIMAGEIO_OPTPARSER_H
