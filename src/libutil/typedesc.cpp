/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2008 Larry Gritz
// 
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
// 
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 
// (this is the MIT license)
/////////////////////////////////////////////////////////////////////////////


#include <cstdio>
#include <cstdlib>
#include <string>

#include "dassert.h"
#include "ustring.h"
#include "strutil.h"

#define DLL_EXPORT_PUBLIC /* Because we are implementing paramtype */
#include "typedesc.h"
#undef DLL_EXPORT_PUBLIC


// FIXME: We should clearly be using a namespace.  But maybe not "Gelato".
// using namespace Gelato;


TypeDesc::TypeDesc (const char *typestring)
{
    ASSERT (0);
}



static int basetype_size[] = {
    0, // UNKNOWN
    0, // VOID
    sizeof(unsigned char),   // UCHAR
    sizeof(char),            // CHAR
    sizeof(unsigned short),  // USHORT
    sizeof(short),           // SHORT
    sizeof(unsigned int),    // UINT
    sizeof(int),             // INT
    sizeof(float)/2,         // HALF
    sizeof(float),           // FLOAT
    sizeof(double),          // DOUBLE
    sizeof(char *),          // STRING
    sizeof(void *)           // PTR
};


size_t
TypeDesc::basesize () const
{
    DASSERT (sizeof(basetype_size)/sizeof(basetype_size[0]) == TypeDesc::LASTBASE);
    DASSERT (basetype < TypeDesc::LASTBASE);
    return basetype_size[basetype];
}



static const char * basetype_name[] = {
    "unknown", // UNKNOWN
    "void", // VOID
    "uint8",   // UCHAR
    "int8",            // CHAR
    "uint16",  // USHORT
    "int16",           // SHORT
    "uint",    // UINT
    "int",             // INT
    "half",         // HALF
    "float",           // FLOAT
    "double",          // DOUBLE
    "string",          // STRING
    "pointer"         // PTR
};

static const char * basetype_code[] = {
    "unknown", // UNKNOWN
    "void", // VOID
    "uc",   // UCHAR
    "c",            // CHAR
    "us",  // USHORT
    "s",           // SHORT
    "ui",    // UINT
    "i",             // INT
    "h",         // HALF
    "f",           // FLOAT
    "d",          // DOUBLE
    "str",          // STRING
    "ptr"         // PTR
};



const char *
TypeDesc::c_str () const
{
    // FIXME : how about a per-thread cache of the last one or two, so
    // we don't have to re-assemble strings all the time?
    std::string result;
    if (aggregate == SCALAR)
        result = basetype_name[basetype];
    else {
        const char *agg = "";
        switch (aggregate) {
        case VEC2 : agg = "vec2"; break;
        case VEC3 : agg = "vec3"; break;
        case VEC4 : agg = "vec4"; break;
        case MATRIX44 : agg = "matrix44"; break;
        }
        result = std::string (agg) + basetype_code[basetype];
    }
    if (arraylen > 0)
        result += Strutil::format ("[%d]", arraylen);
    return ustring(result).c_str();
}



int
TypeDesc::fromstring (const char *typestring, char *shortname)
{
    // FIXME
    ASSERT(0);
    return 0;
}



const TypeDesc TypeDesc::TypeFloat (TypeDesc::FLOAT);
const TypeDesc TypeDesc::TypeColor (TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR);
const TypeDesc TypeDesc::TypeString (TypeDesc::STRING);
const TypeDesc TypeDesc::TypeInt (TypeDesc::INT);

