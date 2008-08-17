/////////////////////////////////////////////////////////////////////////////
// Copyright 2004 NVIDIA Corporation and Copyright 2008 Larry Gritz.
// All Rights Reserved.
//
// Extensions by Larry Gritz based on open-source code by NVIDIA.
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


/////////////////////////////////////////////////////////////////////////////
// ParamType and related classes
// -----------------------------
//
// It frequently comes up (in my experience, with renderers and image
// handling programs) that you want a way to describe data that is passed
// through APIs through blind pointers.  These are some simple classes
// that provide a simple type descriptor system.  This is not meant to
// be comprehensive -- for example, there is no provision for structs,
// unions, pointers, const, or 'nested' type definitions.  Just simple
// integer and floating point, *common* aggregates such as 3-points,
// and reasonably-lengthed arrays thereof.
//
/////////////////////////////////////////////////////////////////////////////


#ifndef PARAMTYPE_H
#define PARAMTYPE_H

#ifndef NULL
#define NULL 0
#endif

#include "export.h"


// FIXME: We should clearly put this in a namespace.  But maybe not "Gelato".
// namespace Gelato {


/// Base data types.
///
enum ParamBaseType {
    PT_UNKNOWN = 0,          ///< Unknown type
    PT_VOID,                 ///< Known to have no type
    PT_STRING,               ///< String
    PT_FLOAT,                ///< 32-bit IEEE float
    PT_HALF,                 ///< 16-bit float a la OpenEXR or NVIDIA fp16
    PT_DOUBLE,               ///< 64-bit IEEE float
    PT_POINT,                ///< 3-tuple of float describing a position
    PT_VECTOR,               ///< 3-tuple of float describing a direction
    PT_NORMAL,               ///< 3-tuple of float describing a surface normal
    PT_COLOR,                ///< 3-tuple of float describing a color
    PT_HPOINT,               ///< 4-tuple of float describing a 4D position,
                             ///<        4D direction, or homogeneous point
    PT_MATRIX,               ///< 4x4-tuple of float describing a 4x4 matrix
    PT_INT8, PT_UINT8,       ///< 8 bit int, signed and unsigned
    PT_BYTE = PT_UINT8,      ///<    BYTE == synonym for UINT8
    PT_INT16, PT_UINT16,     ///< 16 bit int, signed and unsigned
    PT_INT,                  ///< 32-bit signed int
    PT_INT32 = PT_INT,       ///< 32-bit signed int
    PT_UINT,                 ///< 32-bit unsigned int
    PT_UINT32 = PT_UINT,     ///< 32-bit unsigned int
    PT_POINTER,              ///< pointer, in system address width
      // For historical reasons, DO NOT change the order of the above!
      // Future expansion takes place here.  Remember to modify the 
      // routines below that operate on ParamBaseType.
    PT_LAST                  // Mark the end
};



/// Return the name, for printing and whatnot, of a ParamBaseType.
/// For example, PT_FLOAT -> "float"
extern DLLPUBLIC const char *typestring (ParamBaseType t);

/// Return the size, in bytes, of a single item of a ParamBaseType
///
extern DLLPUBLIC int typesize (ParamBaseType t);

/// Return the scalar type corresponding to this possibly aggregate type
/// (e.g. for PT_POINT, return PT_FLOAT).  PT types that are not
/// aggregates return themselves (e.g. PT_UINT returns PT_UINT).
extern DLLPUBLIC ParamBaseType scalartype (ParamBaseType t);

/// Return the number of scalars comprising a ParamBaseType (e.g., 3 for
/// PT_POINT).  Return 0 for all types not comprised of floats.
extern DLLPUBLIC int nscalars (ParamBaseType t);

/// Return the number of floats comprising a ParamBaseType (e.g., 3 for
/// PT_POINT).  Return 0 for all types not comprised of floats.
extern DLLPUBLIC int nfloats (ParamBaseType t);



// Deprecated names from Gelato's paramtype.h
inline const char *ParamBaseTypeNameString (int t) { return typestring((ParamBaseType)t); }
inline int ParamBaseTypeSize (int t) { return typesize ((ParamBaseType)t); }
inline int ParamBaseTypeNFloats (int t) { return nfloats ((ParamBaseType)t); }








/// Interpolation types
///
enum ParamInterp {
    INTERP_CONSTANT = 0,       //< Constant for all pieces/faces
    INTERP_PERPIECE = 1,       //< Piecewise constant per piece/face
    INTERP_LINEAR = 2,         //< Linearly interpolated across each piece/face
    INTERP_VERTEX = 3          //< Interpolated like vertices
};



/// ParamType is a simple type descriptor.  Contains a base type, array
/// length, and other attributes.  This structure is no bigger than an
/// int, and so can be very cheaply passed around.
class DLLPUBLIC ParamType {
public:
    ParamType (void) { /* Uninitialized! */ }

    /// Construct from base type and interp, or base only (assume non-array)
    ///
    ParamType (ParamBaseType base, ParamInterp detail=INTERP_CONSTANT) {
        basetype = base;
        arraylen = 1;
        isarray = 0;
        interp = detail;
        reserved = 0;
    }

    /// Construct with array length
    ///
    ParamType (ParamBaseType base, short array,
               ParamInterp det=INTERP_CONSTANT) {
        basetype = base;
        arraylen = array ? array : 1;
        isarray = (array != 0);
        interp = det;
        reserved = 0;
    }

    /// Construct from a string (e.g., "vertex float[3]").  If no valid
    /// type could be assembled, set basetype to PT_UNKNOWN.
    ParamType (const char *typestring) {
        if (! fromstring(typestring))
            basetype = PT_UNKNOWN;
    }

    /// Set *this to the type described in the string.  Return the
    /// length of the part of the string that describes the type.  If
    /// no valid type could be assembled, return 0 and do not modify
    /// *this.  If shortname is not NULL, store the word(s) in the string
    /// after the type (presumably the variable name) in shortname.
    int fromstring (const char *typestring, char *shortname=NULL);

    /// Store the string representation of the type in typestring.  Don't
    /// overwrite more than maxlen bytes of typestring!  Return true upon
    /// success, false upon failure (including failure to fit).
    bool tostring (char *typestring, int maxlen, bool showinterp=false) const;

    /// Return size of one item of this type, in bytes
    ///
    int datasize (void) const { return arraylen*typesize((ParamBaseType)basetype); }

    /// Return the number of floats in one element of this type, or 0 if
    /// it's not constructed out of floats.
    int nfloats (void) const { return arraylen*::nfloats((ParamBaseType)basetype); }

    bool operator== (const ParamType &t) const {
        return *(const int*)(this) == *(const int*)(&t);
    } 

    bool operator!= (const ParamType &t) const {
        return *(const int*)(this) != *(const int*)(&t);
    } 

    /// equiv tests that they are the same, but ignoring 'interp'
    ///
    bool equiv (const ParamType &t) const {
        return (this->basetype == t.basetype && 
                this->arraylen == t.arraylen &&
                this->isarray == t.isarray);
    } 

    /// Demote the type to a non-array
    ///
    void unarray (void) { isarray = false;  arraylen = 1; }

    unsigned int basetype:5;   //< Base type of the data -- one of ParamBaseType
    unsigned int arraylen:18;  //< Array len (up to 256k), or 1 if not an array
    unsigned int isarray:1;    //< 1 if it's an array
    unsigned int interp:3;     //< Sometimes used: interpolation type
    unsigned int reserved:5;   //< Future expansion
};


// FIXME: We should clearly put this in a namespace.  But maybe not "Gelato".
// }; /* end namespace Gelato */

#endif /* !defined(PARAMTYPE_H) */
