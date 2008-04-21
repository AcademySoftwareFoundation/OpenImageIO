/////////////////////////////////////////////////////////////////////////////
// Copyright 2004 NVIDIA Corporation.  All Rights Reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// * Neither the name of NVIDIA nor the names of its contributors
//   may be used to endorse or promote products derived from this software
//   without specific prior written permission.
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
// (This is the Modified BSD License)
/////////////////////////////////////////////////////////////////////////////


#ifndef GELATO_PARAMTYPE_H
#define GELATO_PARAMTYPE_H

#ifndef NULL
#define NULL 0
#endif

#include "export.h"

namespace Gelato {


// Base data types
enum ParamBaseType {
    PT_UNKNOWN = 0,
    PT_VOID,
    PT_STRING,
    PT_FLOAT, PT_HALF, PT_DOUBLE,
    PT_POINT, PT_VECTOR, PT_NORMAL,
    PT_COLOR,
    PT_HPOINT, PT_MATRIX,
    PT_INT8, PT_UINT8, PT_INT16, PT_UINT16, PT_INT, PT_UINT,
    PT_POINTER,
    PT_LAST
};


// Return the name, for printing and whatnot, of a ParamBaseType
extern GELATO_PUBLIC const char *ParamBaseTypeNameString (int t);

// Return the size, in bytes, of a single item of a ParamBaseType
extern GELATO_PUBLIC int ParamBaseTypeSize (int t);

// Return the number of floats comprising a ParamBaseType
// (e.g., 3 for PT_POINT).  Return 0 for all types not comprised of floats.
extern GELATO_PUBLIC int ParamBaseTypeNFloats (int t);



// Interpolation types
enum ParamInterp {
    INTERP_CONSTANT = 0,       // Constant for all pieces/faces
    INTERP_PERPIECE = 1,       // Piecewise constant per piece/face
    INTERP_LINEAR = 2,         // Linearly interpolated across each piece/face
    INTERP_VERTEX = 3          // Interpolated like vertices
};



// ParamType is a simple type descriptor.  Contains a base type, array
// length, and other attributes.  This structure is no bigger than an
// int, and so can be very cheaply passed around.
class GELATO_PUBLIC ParamType {
public:
    ParamType (void) { /* Uninitialized! */ }

    // Construct from base type and interp, or base only (assume non-array)
    ParamType (ParamBaseType base, ParamInterp detail=INTERP_CONSTANT) {
        basetype = base;
        arraylen = 1;
        isarray = 0;
        interp = detail;
        reserved = 0;
    }

    // Construct with array length
    ParamType (ParamBaseType base, short array,
               ParamInterp det=INTERP_CONSTANT) {
        basetype = base;
        arraylen = array;
        isarray = (array != 0);
        if (! isarray)
            arraylen = 1;
        interp = det;
        reserved = 0;
    }

    // Construct from a string (e.g., "vertex float[3]").  If no valid
    // type could be assembled, set basetype to PT_UNKNOWN.
    ParamType (const char *typestring) {
        if (! fromstring(typestring))
            basetype = PT_UNKNOWN;
    }

    // Set *this to the type described in the string.  Return the
    // length of the part of the string that describes the type.  If
    // no valid type could be assembled, return 0 and do not modify
    // *this.  If shortname is not NULL, store the word(s) in the string
    // after the type (presumably the variable name) in shortname.
    int fromstring (const char *typestring, char *shortname=NULL);

    // Store the string representation of the type in typestring.  Don't
    // overwrite more than maxlen bytes of typestring!  Return true upon
    // success, false upon failure (including failure to fit).
    bool tostring (char *typestring, int maxlen, bool showinterp=false) const;

    // Return size of one element of this type, in bytes
    int datasize (void) const { return arraylen*ParamBaseTypeSize(basetype); }

    int nfloats (void) const { return arraylen*ParamBaseTypeNFloats(basetype); }

    bool operator== (const ParamType &t) {
        return *(int*)(this) == *(int*)(&t);
    } 

    // Demote the type to a non-array
    void unarray (void) { isarray = false;  arraylen = 1; }

    unsigned int basetype:5;   // Base type of the data -- one of ParamBaseType
    unsigned int arraylen:18;  // Array len (up to 256k), or 1 if not an array
    unsigned int isarray:1;    // 1 if it's an array
    unsigned int interp:3;     // Sometimes used: interpolation type
    unsigned int reserved:5;   // Future expansion
};


}; /* end namespace Gelato */

#endif /* !defined(GELATO_PARAMTYPE_H) */
