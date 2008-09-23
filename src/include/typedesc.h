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


/////////////////////////////////////////////////////////////////////////////
// TypeDesc and related classes
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


#ifndef TYPEDESC_H
#define TYPEDESC_H

#ifndef NULL
#define NULL 0
#endif

#include "export.h"


// FIXME: We should clearly put this in a namespace.
// namespace blah {



struct DLLPUBLIC TypeDesc {
    /// BASETYPE is a simple enum for the C/C++ built-in types.
    ///
    enum BASETYPE { UNKNOWN, VOID, 
                    UCHAR, UINT8=UCHAR, CHAR, INT8=CHAR,
                    USHORT, UINT16=USHORT, SHORT, INT16=SHORT,
                    UINT, INT,
                    HALF, FLOAT, DOUBLE, STRING, PTR, LASTBASE };
    /// AGGREGATE describes whether our type is a simple scalar of
    /// one of the BASETYPE's, or one of several simple aggregates.
    enum AGGREGATE { SCALAR=1, VEC2=2, VEC3=3, VEC4=4, MATRIX44=16 };
    /// VECSEMANTICS describes, for non-SCALAR aggregates, whether our
    /// type is like a color (raw values) or if it has coordinate
    /// transformation rules similar to a point, vector (direction),
    /// or surface normal.
    enum VECSEMANTICS { NOXFORM=0, COLOR, POINT, VECTOR, NORMAL };

    unsigned char basetype;     ///< C data type at the heart of our type
    unsigned char aggregate;    ///< What kind of AGGREGATE is it?
    unsigned char vecsemantics; ///< What does the vec represent?
    unsigned char reserved;     ///< Reserved for future expansion
    int arraylen;               ///< Array length, 0 = not array, -1 = unsized

    /// Construct from a BASETYPE and optional aggregateness and
    /// transformation rules.
    TypeDesc (BASETYPE btype=UNKNOWN, AGGREGATE agg=SCALAR,
              VECSEMANTICS xform=NOXFORM)
        : basetype(btype), aggregate(agg), vecsemantics(xform), reserved(0),
          arraylen(0)
          { }

    /// Construct an array from BASETYPE and length, and optional
    /// aggregateness and transformation rules.
    TypeDesc (BASETYPE btype, int arraylength,
              AGGREGATE agg=SCALAR, VECSEMANTICS xform=NOXFORM)
        : basetype(btype), aggregate(agg), vecsemantics(xform), reserved(0),
          arraylen(arraylength)
          { }

    /// Construct from a string (e.g., "float[3]").  If no valid
    /// type could be assembled, set base to UNKNOWN.
    TypeDesc (const char *typestring);

    /// Return the name, for printing and whatnot.  For example,
    /// "float", "int[5]", "normal"
    const char *c_str() const;

    /// Return the number of elements: 1 if not an array, or the array
    /// length.
    size_t numelements () const {
        return (arraylen >= 1 ? arraylen : 1);
    }

    /// Return the size, in bytes, of this type.
    ///
    size_t size () const {
        return (arraylen >= 1 ? arraylen : 1) * elementsize();
    }

    /// Return the type of one element, i.e., strip out the array-ness.
    ///
    TypeDesc elementtype () const {
        TypeDesc t (*this);  t.arraylen = 0;  return t;
    }

    /// Return the size, in bytes, of one element of this type (that is,
    /// ignoring whether it's an array).
    size_t elementsize () const { return aggregate * basesize(); }

    // /// Return just the underlying C scalar type, i.e., strip out the
    // /// array-ness and the aggregateness.
//    BASETYPE basetype () const { return TypeDesc(base); }

    /// Return the base type size, i.e., stripped of both array-ness
    /// and aggregateness.
    size_t basesize () const;

    /// Set *this to the type described in the string.  Return the
    /// length of the part of the string that describes the type.  If
    /// no valid type could be assembled, return 0 and do not modify
    /// *this.  If shortname is not NULL, store the word(s) in the string
    /// after the type (presumably the variable name) in shortname.
    int fromstring (const char *typestring, char *shortname=NULL);

    bool operator== (const TypeDesc &t) const {
        return *(const long long*)(this) == *(const long long*)(&t);
    } 

    bool operator!= (const TypeDesc &t) const {
        return *(const long long*)(this) != *(const long long*)(&t);
    } 

    /// Demote the type to a non-array
    ///
    void unarray (void) { arraylen = 0; }

    static const TypeDesc TypeFloat, TypeColor, TypeString, TypeInt;
};



// Deprecated!  Some back-compatibility with Gelato
typedef TypeDesc ParamType;
typedef TypeDesc ParamBaseType;
#define PT_FLOAT TypeDesc::FLOAT
#define PT_UINT8 TypeDesc::UCHAR
#define PT_INT8 TypeDesc::CHAR
#define PT_UINT16 TypeDesc::USHORT
#define PT_INT16 TypeDesc::SHORT
#define PT_UINT TypeDesc::UINT
#define PT_INT TypeDesc::INT
#define PT_FLOAT TypeDesc::FLOAT
#define PT_DOUBLE TypeDesc::DOUBLE
#define PT_HALF TypeDesc::HALF
#define PT_MATRIX TypeDesc(TypeDesc::FLOAT,TypeDesc::MATRIX44)
#define PT_STRING TypeDesc::STRING
#define PT_UNKNOWN TypeDesc::UNKNOWN





// FIXME: We should clearly put this in a namespace.
// }; // end namespace

#endif /* !defined(TYPEDESC_H) */
