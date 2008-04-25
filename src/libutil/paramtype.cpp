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

#include "dassert.h"

#define DLL_EXPORT_PUBLIC /* Because we are implementing paramtype */
#include "paramtype.h"
#undef DLL_EXPORT_PUBLIC


// FIXME: We should clearly be using a namespace.  But maybe not "Gelato".
// using namespace Gelato;



static const char * pbt_name[] = {
    "<unknown>", // PT_UNKNOWN
    "void",      // PT_VOID
    "string",    // PT_STRING
    "float",     // PT_FLOAT
    "half",      // PT_HALF
    "double",    // PT_DOUBLE
    "point",     // PT_POINT
    "vector",    // PT_VECTOR
    "normal",    // PT_NORMAL
    "color",     // PT_COLOR
    "hpoint",    // PT_HPOINT
    "matrix",    // PT_MATRIX
    "int8",      // PT_INT8
    "uint8",     // PT_UINT8
    "int16",     // PT_INT16
    "uint16",    // PT_UINT16
    "int",       // PT_INT
    "uint",      // PT_UINT
    "pointer"    // PT_POINTER
};


const char *
ParamBaseTypeNameString (int t)
{
    DASSERT (sizeof(pbt_name)/sizeof(pbt_name[0]) == PT_LAST);
    if (t < 0 || t >= PT_LAST)
        return NULL;
    return pbt_name[t];
}



static const int pbt_size[] = {
    0,                      // PT_UNKNOWN
    0,                      // PT_VOID
    sizeof(char *),         // PT_STRING
    sizeof(float),          // PT_FLOAT
    sizeof(float)/2,        // PT_HALF
    sizeof(double),         // PT_DOUBLE
    3*sizeof(float),        // PT_POINT
    3*sizeof(float),        // PT_VECTOR
    3*sizeof(float),        // PT_NORMAL
    3*sizeof(float),        // PT_COLOR
    4*sizeof(float),        // PT_HPOINT
    16*sizeof(float),       // PT_MATRIX
    sizeof(char),           // PT_INT8
    sizeof(unsigned char),  // PT_UINT8
    sizeof(short),          // PT_INT16
    sizeof(unsigned short), // PT_UINT16
    sizeof(int),            // PT_INT
    sizeof(unsigned int),   // PT_UINT
    sizeof(void *)          // PT_POINTER
};



int
ParamBaseTypeSize (int t)
{
    DASSERT (sizeof(pbt_size)/sizeof(pbt_size[0]) == PT_LAST);
    if (t < 0 || t >= PT_LAST)
        return 0;
    return pbt_size[t];
}



static const ParamBaseType pbt_scalartype[] = {
    PT_UNKNOWN,    // PT_UNKNOWN
    PT_VOID,       // PT_VOID
    PT_STRING,     // PT_STRING
    PT_FLOAT,      // PT_FLOAT
    PT_HALF,       // PT_HALF
    PT_DOUBLE,     // PT_DOUBLE
    PT_FLOAT,      // PT_POINT
    PT_FLOAT,      // PT_VECTOR
    PT_FLOAT,      // PT_NORMAL
    PT_FLOAT,      // PT_COLOR
    PT_FLOAT,      // PT_HPOINT
    PT_FLOAT,      // PT_MATRIX
    PT_INT8,       // PT_INT8
    PT_UINT8,      // PT_UINT8
    PT_INT16,      // PT_INT16
    PT_UINT16,     // PT_UINT16
    PT_INT,        // PT_INT
    PT_UINT,       // PT_UINT
    PT_POINTER     // PT_POINTER
};



ParamBaseType
ParamBaseTypeScalarType (int t)
{
    DASSERT (sizeof(pbt_scalartype)/sizeof(pbt_scalartype[0]) == PT_LAST);
    if (t < 0 || t >= PT_LAST)
        return PT_UNKNOWN;
    return pbt_scalartype[t];
}



static const int pbt_nscalars[] = {
    0,  // PT_UNKNOWN
    0,  // PT_VOID
    1,  // PT_STRING
    1,  // PT_FLOAT
    1,  // PT_HALF
    1,  // PT_DOUBLE
    3,  // PT_POINT
    3,  // PT_VECTOR
    3,  // PT_NORMAL
    3,  // PT_COLOR
    4,  // PT_HPOINT
    16, // PT_MATRIX
    1,  // PT_INT8
    1,  // PT_UINT8
    1,  // PT_INT16
    1,  // PT_UINT16
    1,  // PT_INT
    1,  // PT_UINT
    1   // PT_POINTER
};



int
ParamBaseTypeNScalars (int t)
{
    DASSERT (sizeof(pbt_nscalars)/sizeof(pbt_size[0]) == PT_LAST);
    if (t < 0 || t >= PT_LAST)
        return 0;
    return pbt_nscalars[t];
}



static const int pbt_nfloats[] = {
    0,  // PT_UNKNOWN
    0,  // PT_VOID
    0,  // PT_STRING
    1,  // PT_FLOAT
    0,  // PT_HALF
    0,  // PT_DOUBLE
    3,  // PT_POINT
    3,  // PT_VECTOR
    3,  // PT_NORMAL
    3,  // PT_COLOR
    4,  // PT_HPOINT
    16, // PT_MATRIX
    0,  // PT_INT8
    0,  // PT_UINT8
    0,  // PT_INT16
    0,  // PT_UINT16
    0,  // PT_INT
    0,  // PT_UINT
    0   // PT_POINTER
};



int
ParamBaseTypeNFloats (int t)
{
    DASSERT (sizeof(pbt_nfloats)/sizeof(pbt_size[0]) == PT_LAST);
    if (t < 0 || t >= PT_LAST)
        return 0;
    return pbt_nfloats[t];
}



int
ParamType::fromstring (const char *typestring, char *shortname)
{
    // FIXME
    ASSERT(0);
    return 0;
}



bool
ParamType::tostring (char *typestring, int maxlen, bool showinterp) const
{
    // FIXME
    ASSERT(0);
    return false;
}
