/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
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


#include <cstdio>
#include <cstdlib>
#include <string>

#include <OpenEXR/half.h>

#include "dassert.h"
#include "ustring.h"
#include "strutil.h"

#include "typedesc.h"


OIIO_NAMESPACE_ENTER
{

TypeDesc::TypeDesc (const char *typestring)
    : basetype(UNKNOWN), aggregate(SCALAR), vecsemantics(NOXFORM),
      reserved(0), arraylen(0)
{
    fromstring (typestring);
}



namespace {

static int basetype_size[] = {
    0, // UNKNOWN
    0, // VOID
    sizeof(unsigned char),   // UCHAR
    sizeof(char),            // CHAR
    sizeof(unsigned short),  // USHORT
    sizeof(short),           // SHORT
    sizeof(unsigned int),    // UINT
    sizeof(int),             // INT
    sizeof(unsigned long long), // ULONGLONG
    sizeof(long long),       // LONGLONG
    sizeof(float)/2,         // HALF
    sizeof(float),           // FLOAT
    sizeof(double),          // DOUBLE
    sizeof(char *),          // STRING
    sizeof(void *)           // PTR
};

}

size_t
TypeDesc::basesize () const
{
    DASSERT (sizeof(basetype_size)/sizeof(basetype_size[0]) == TypeDesc::LASTBASE);
    DASSERT (basetype < TypeDesc::LASTBASE);
    return basetype_size[basetype];
}

namespace {

static const char * basetype_name[] = {
    "unknown",         // UNKNOWN
    "void",            // VOID/NONE
    "uint8",           // UCHAR
    "int8",            // CHAR
    "uint16",          // USHORT
    "int16",           // SHORT
    "uint",            // UINT
    "int",             // INT
    "uint64",          // ULONGLONG
    "int64",           // LONGLONG
    "half",            // HALF
    "float",           // FLOAT
    "double",          // DOUBLE
    "string",          // STRING
    "pointer"          // PTR
};

static const char * basetype_code[] = {
    "unknown",      // UNKNOWN
    "void",         // VOID/NONE
    "uc",           // UCHAR
    "c",            // CHAR
    "us",           // USHORT
    "s",            // SHORT
    "ui",           // UINT
    "i",            // INT
    "ull",          // ULONGLONG
    "ll",           // LONGLONG
    "h",            // HALF
    "f",            // FLOAT
    "d",            // DOUBLE
    "str",          // STRING
    "ptr"           // PTR
};

}

const char *
TypeDesc::c_str () const
{
    // FIXME : how about a per-thread cache of the last one or two, so
    // we don't have to re-assemble strings all the time?

    // Timecode and Keycode are hard coded
    if (basetype == UINT && vecsemantics == TIMECODE && arraylen == 2)
        return ustring("timecode").c_str();
    else if (basetype == INT && vecsemantics == KEYCODE && arraylen == 7)
        return ustring("keycode").c_str();

    std::string result;
    if (aggregate == SCALAR)
        result = basetype_name[basetype];
    else if (aggregate == MATRIX44 && basetype == FLOAT)
        result = "matrix";
    else if (vecsemantics == NOXFORM) {
        const char *agg = "";
        switch (aggregate) {
        case VEC2 : agg = "vec2"; break;
        case VEC3 : agg = "vec3"; break;
        case VEC4 : agg = "vec4"; break;
        case MATRIX44 : agg = "matrix44"; break;
        }
        result = std::string (agg) + basetype_code[basetype];
    } else {
        // Special names for vector semantics
        const char *vec = "";
        switch (vecsemantics) {
        case COLOR  : vec = "color"; break;
        case POINT  : vec = "point"; break;
        case VECTOR : vec = "vector"; break;
        case NORMAL : vec = "normal"; break;
        default: ASSERT (0 && "Invalid vector semantics");
        }
        const char *agg = "";
        switch (aggregate) {
        case VEC2 : agg = "2"; break;
        case VEC4 : agg = "4"; break;
        case MATRIX44 : agg = "matrix"; break;
        }
        result = std::string (vec) + std::string (agg);
        if (basetype != FLOAT)
            result += basetype_code[basetype];
    }
    if (arraylen > 0)
        result += Strutil::format ("[%d]", arraylen);
    else if (arraylen < 0)
        result += "[]";
    return ustring(result).c_str();
}



// Copy src into dst until you hit the end, find a delimiter charcter,
// or have copied maxlen-1 characters, whichever comes first.  Add a 
// terminating null charcter.  Return the number of characters copied.
inline size_t
copy_until (const char *src, const char *delim, char *dst, size_t maxlen)
{
    size_t i = 0;
    while (src[i] && i < maxlen-1) {
        bool found_delim = false;
        for (int d = 0;  delim[d];  ++d)
            if (src[i] == delim[d])
                found_delim = true;
        if (found_delim)
            break;
        dst[i] = src[i];
        ++i;
    }
    dst[i] = 0;
    return i;
}



size_t
TypeDesc::fromstring (const char *typestring)
{
    TypeDesc t;
    size_t len = 0;
    if (! typestring)
        return 0;

    // The first "word" should be a type name.
    char type[16];
    len = copy_until (typestring, " [", type, sizeof(type));
    // Check the scalar types in our table above
    for (int i = 0;  i < LASTBASE;  ++i) {
        if (! strcmp (type, basetype_name[i])) {
            t.basetype = i;
            break;
        }
    }

    // Some special case names for aggregates
    if (t.basetype != UNKNOWN) {
        // already solved
    }
    else if (! strcmp (type, "color"))
        t = TypeColor;
    else if (! strcmp (type, "point"))
        t = TypePoint;
    else if (! strcmp (type, "vector"))
        t = TypeVector;
    else if (! strcmp (type, "normal"))
        t = TypeNormal;
    else if (! strcmp (type, "matrix"))
        t = TypeMatrix;
    else {
        return 0;  // unknown
    }

    // Is there an array length following the type name?
    while (typestring[len] == ' ')
        ++len;
    if (typestring[len] == '[') {
        ++len;
        while (typestring[len] == ' ')
            ++len;
        if (typestring[len] == ']') {   // '[]' indicates array of unknown len
            t.arraylen = -1;
        } else {
            t.arraylen = atoi (typestring+len);
            while ((typestring[len] >= '0' && typestring[len] <= '9') ||
                   typestring[len] == ' ')
                ++len;
        }
        if (typestring[len] == ']')
            ++len;
        else
            return 0;
    }

    *this = t;
    return len;
}

    
    
template <class T> inline std::string
sprintt (TypeDesc type, const char *format, const char *aggregate_delim,
         const char *aggregate_sep, const char *array_delim,
         const char *array_sep, T *v) {
    std::string val;
    if (type.arraylen)
        val += array_delim[0];
    const size_t n = type.arraylen ? type.arraylen : 1;
    for (size_t i = 0; i < n; ++i) {
        if (type.aggregate > 1)
            val += aggregate_delim[0];
        for (int j = 0; j < (int)type.aggregate; ++j, ++v) {
            val += Strutil::format (format, *v);
            if (type.aggregate > 1 && j < type.aggregate - 1)
                val += aggregate_sep;
        }
        if (type.aggregate > 1)
            val += aggregate_delim[1];
        if (i < n - 1)
            val += array_sep;
    }
    if (type.arraylen)
        val += array_delim[1];
    return val;
}



std::string tostring (TypeDesc type, const void *data,
                      const char *float_fmt, const char *string_fmt,
                      const char *aggregate_delim, const char *aggregate_sep,
                      const char *array_delim, const char *array_sep) {
    // Perhaps there is a way to use CType<> with a dynamic argument?
    switch (type.basetype) {
        case TypeDesc::UNKNOWN:
            return sprintt (type, "%p", aggregate_delim, aggregate_sep,
                            array_delim, array_sep, (void **)data);
        case TypeDesc::NONE:
            return sprintt (type, "None", aggregate_delim, aggregate_sep,
                            array_delim, array_sep, (void **)data);
        case TypeDesc::UCHAR:
            return sprintt (type, "%uhh", aggregate_delim, aggregate_sep,
                            array_delim, array_sep, (unsigned char *)data);
        case TypeDesc::CHAR:
            return sprintt (type, "%dhh", aggregate_delim, aggregate_sep,
                            array_delim, array_sep, (char *)data);
        case TypeDesc::USHORT:
            return sprintt (type, "%uh", aggregate_delim, aggregate_sep,
                            array_delim, array_sep, (unsigned short *)data);
        case TypeDesc::SHORT:
            return sprintt (type, "%dh", aggregate_delim, aggregate_sep,
                            array_delim, array_sep, (short *)data);
        case TypeDesc::UINT:
            return sprintt (type, "%u", aggregate_delim, aggregate_sep,
                            array_delim, array_sep, (unsigned int *)data);
        case TypeDesc::INT:
            return sprintt (type, "%d", aggregate_delim, aggregate_sep,
                            array_delim, array_sep, (int *)data);
        case TypeDesc::ULONGLONG:
            return sprintt (type, "%ull", aggregate_delim, aggregate_sep,
                            array_delim, array_sep, (unsigned long long *)data);
        case TypeDesc::LONGLONG:
            return sprintt (type, "%dll", aggregate_delim, aggregate_sep,
                            array_delim, array_sep, (long long *)data);
        case TypeDesc::HALF:
            return sprintt (type, float_fmt, aggregate_delim, aggregate_sep,
                            array_delim, array_sep, (half *) data);
        case TypeDesc::FLOAT:
            return sprintt (type, float_fmt, aggregate_delim, aggregate_sep,
                            array_delim, array_sep, (float *)data);
        case TypeDesc::DOUBLE:
            return sprintt (type, float_fmt, aggregate_delim, aggregate_sep,
                            array_delim, array_sep, (double *)data);
        case TypeDesc::STRING:
            return sprintt (type, string_fmt, aggregate_delim, aggregate_sep,
                            array_delim, array_sep, (char **)data);
        case TypeDesc::PTR:
            return sprintt (type, "%p", aggregate_delim, aggregate_sep,
                            array_delim, array_sep, (void **)data);
        default:
            return "";
    }
}




const TypeDesc TypeDesc::TypeFloat (TypeDesc::FLOAT);
const TypeDesc TypeDesc::TypeColor (TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR);
const TypeDesc TypeDesc::TypePoint (TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::POINT);
const TypeDesc TypeDesc::TypeVector (TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::VECTOR);
const TypeDesc TypeDesc::TypeNormal (TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::NORMAL);
const TypeDesc TypeDesc::TypeMatrix (TypeDesc::FLOAT,TypeDesc::MATRIX44);
const TypeDesc TypeDesc::TypeString (TypeDesc::STRING);
const TypeDesc TypeDesc::TypeInt (TypeDesc::INT);
const TypeDesc TypeDesc::TypeTimeCode (TypeDesc::UINT, TypeDesc::SCALAR, TypeDesc::TIMECODE, 2);
const TypeDesc TypeDesc::TypeKeyCode (TypeDesc::INT, TypeDesc::SCALAR, TypeDesc::KEYCODE, 7);


}
OIIO_NAMESPACE_EXIT
