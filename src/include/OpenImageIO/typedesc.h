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


/// \file
/// The TypeDesc class is used to describe simple data types.


#ifndef OPENIMAGEIO_TYPEDESC_H
#define OPENIMAGEIO_TYPEDESC_H

#if defined(_MSC_VER)
// Ignore warnings about conditional expressions that always evaluate true
// on a given platform but may evaluate differently on another. There's
// nothing wrong with such conditionals.
#  pragma warning (disable : 4127)
#endif

#include <limits>
#include <cmath>
#include <cstddef>
#include <iostream>

#include <OpenImageIO/export.h>
#include <OpenImageIO/oiioversion.h>
#include <OpenImageIO/dassert.h>
#include <OpenImageIO/string_view.h>


OIIO_NAMESPACE_BEGIN

/////////////////////////////////////////////////////////////////////////////
/// A TypeDesc describes simple data types.
///
/// It frequently comes up (in my experience, with renderers and image
/// handling programs) that you want a way to describe data that is passed
/// through APIs through blind pointers.  These are some simple classes
/// that provide a simple type descriptor system.  This is not meant to
/// be comprehensive -- for example, there is no provision for structs,
/// unions, pointers, const, or 'nested' type definitions.  Just simple
/// integer and floating point, *common* aggregates such as 3-points,
/// and reasonably-lengthed arrays thereof.
///
/////////////////////////////////////////////////////////////////////////////

struct OIIO_API TypeDesc {
    /// BASETYPE is a simple enum for the C/C++ built-in types.
    ///
    enum BASETYPE { UNKNOWN, NONE, 
                    UCHAR, UINT8=UCHAR, CHAR, INT8=CHAR,
                    USHORT, UINT16=USHORT, SHORT, INT16=SHORT,
                    UINT, UINT32=UINT, INT, INT32=INT,
                    ULONGLONG, UINT64=ULONGLONG, LONGLONG, INT64=LONGLONG,
                    HALF, FLOAT, DOUBLE, STRING, PTR, LASTBASE };
    /// AGGREGATE describes whether our type is a simple scalar of
    /// one of the BASETYPE's, or one of several simple aggregates.
    enum AGGREGATE { SCALAR=1, VEC2=2, VEC3=3, VEC4=4, MATRIX33=9, MATRIX44=16 };
    /// VECSEMANTICS gives hints about what the data represent (for
    /// example, if a spatial vector, whether it should transform as
    /// a point, direction vector, or surface normal).
    enum VECSEMANTICS { NOXFORM=0, NOSEMANTICS=0,  // no semantic hints
                        COLOR,    // color
                        POINT,    // spatial location
                        VECTOR,   // spatial direction
                        NORMAL,   // surface normal
                        TIMECODE, // SMPTE timecode (should be int[2])
                        KEYCODE,  // SMPTE keycode (should be int[7])
                        RATIONAL  // paired numerator and denominator
                      };

    unsigned char basetype;     ///< C data type at the heart of our type
    unsigned char aggregate;    ///< What kind of AGGREGATE is it?
    unsigned char vecsemantics; ///< What does the vec represent?
    unsigned char reserved;     ///< Reserved for future expansion
    int arraylen;               ///< Array length, 0 = not array, -1 = unsized

    /// Construct from a BASETYPE and optional aggregateness and
    /// transformation rules.
    constexpr TypeDesc (BASETYPE btype=UNKNOWN, AGGREGATE agg=SCALAR,
                        VECSEMANTICS xform=NOXFORM)
        : basetype(static_cast<unsigned char>(btype)),
          aggregate(static_cast<unsigned char>(agg)),
          vecsemantics(static_cast<unsigned char>(xform)), reserved(0),
          arraylen(0)
          { }

    /// Construct an array of a non-aggregate BASETYPE.
    ///
    constexpr TypeDesc (BASETYPE btype, int arraylength)
        : basetype(static_cast<unsigned char>(btype)),
          aggregate(SCALAR), vecsemantics(NOXFORM),
          reserved(0), arraylen(arraylength)
          { }

    /// Construct an array from BASETYPE, AGGREGATE, and array length,
    /// with unspecified (or moot) vector transformation semantics.
    constexpr TypeDesc (BASETYPE btype, AGGREGATE agg, int arraylength)
        : basetype(static_cast<unsigned char>(btype)),
          aggregate(static_cast<unsigned char>(agg)),
          vecsemantics(NOXFORM), reserved(0),
          arraylen(arraylength)
          { }

    /// Construct an array from BASETYPE, AGGREGATE, VECSEMANTICS, and
    /// array length.
    constexpr TypeDesc (BASETYPE btype, AGGREGATE agg,
                        VECSEMANTICS xform, int arraylength)
        : basetype(static_cast<unsigned char>(btype)),
          aggregate(static_cast<unsigned char>(agg)),
          vecsemantics(static_cast<unsigned char>(xform)),
          reserved(0), arraylen(arraylength)
          { }

    /// Construct from a string (e.g., "float[3]").  If no valid
    /// type could be assembled, set base to UNKNOWN.
    TypeDesc (string_view typestring);

    /// Copy constructor.
    constexpr TypeDesc (const TypeDesc &t)
        : basetype(t.basetype), aggregate(t.aggregate),
          vecsemantics(t.vecsemantics), reserved(0), arraylen(t.arraylen)
          { }


    /// Return the name, for printing and whatnot.  For example,
    /// "float", "int[5]", "normal"
    const char *c_str() const;

    friend std::ostream& operator<< (std::ostream& o, TypeDesc t) {
        o << t.c_str();  return o;
    }

    /// Return the number of elements: 1 if not an array, or the array
    /// length. Invalid to call this for arrays of undetermined size.
    OIIO_CONSTEXPR14 size_t numelements () const {
        DASSERT_MSG (arraylen >= 0, "Called numelements() on TypeDesc "
                     "of array with unspecified length (%d)", arraylen);
        return (arraylen >= 1 ? arraylen : 1);
    }

    /// Return the number of basetype values: the aggregate count multiplied
    /// by the array length (or 1 if not an array). Invalid to call this
    /// for arrays of undetermined size.
    OIIO_CONSTEXPR14 size_t basevalues () const {
        return numelements() * aggregate;
    }

    /// Does this TypeDesc describe an array?
    constexpr bool is_array () const { return (arraylen != 0); }

    /// Does this TypeDesc describe an array, but whose length is not
    /// specified?
    constexpr bool is_unsized_array () const { return (arraylen < 0); }

    /// Does this TypeDesc describe an array, whose length is specified?
    constexpr bool is_sized_array () const { return (arraylen > 0); }

    /// Return the size, in bytes, of this type.
    ///
    size_t size () const {
        DASSERT_MSG (arraylen >= 0, "Called size() on TypeDesc "
                     "of array with unspecified length (%d)", arraylen);
        size_t a = (size_t) (arraylen > 0 ? arraylen : 1);
        if (sizeof(size_t) > sizeof(int)) {
            // size_t has plenty of room for this multiplication
            return a * elementsize();
        } else {
            // need overflow protection
            unsigned long long s = (unsigned long long) a * elementsize();
            const size_t toobig = std::numeric_limits<size_t>::max();
            return s < toobig ? (size_t)s : toobig;
        }
    }

    /// Return the type of one element, i.e., strip out the array-ness.
    ///
    OIIO_CONSTEXPR14 TypeDesc elementtype () const {
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

    /// True if it's a floating-point type (versus a fundamentally
    /// integral type or something else like a string).
    bool is_floating_point () const;

    /// True if it's a signed type that allows for negative values.
    bool is_signed () const;

    /// Shortcut: is it UNKNOWN?
    constexpr bool is_unknown () const { return (basetype == UNKNOWN); }

    /// if (typespec) is the same as asking whether it's not UNKNOWN.
    constexpr operator bool () const { return (basetype != UNKNOWN); }

    /// Set *this to the type described in the string.  Return the
    /// length of the part of the string that describes the type.  If
    /// no valid type could be assembled, return 0 and do not modify
    /// *this.
    size_t fromstring (string_view typestring);

    /// Compare two TypeDesc values for equality.
    ///
    constexpr bool operator== (const TypeDesc &t) const {
        return basetype == t.basetype && aggregate == t.aggregate &&
            vecsemantics == t.vecsemantics && arraylen == t.arraylen;
    }

    /// Compare two TypeDesc values for inequality.
    ///
    constexpr bool operator!= (const TypeDesc &t) const { return ! (*this == t); }

    /// Compare a TypeDesc to a basetype (it's the same if it has the
    /// same base type and is not an aggregate or an array).
    friend constexpr bool operator== (const TypeDesc &t, BASETYPE b) {
        return (BASETYPE)t.basetype == b && (AGGREGATE)t.aggregate == SCALAR && !t.is_array();
    }
    friend constexpr bool operator== (BASETYPE b, const TypeDesc &t) {
        return (BASETYPE)t.basetype == b && (AGGREGATE)t.aggregate == SCALAR && !t.is_array();
    }

    /// Compare a TypeDesc to a basetype (it's the same if it has the
    /// same base type and is not an aggregate or an array).
    friend constexpr bool operator!= (const TypeDesc &t, BASETYPE b) {
        return (BASETYPE)t.basetype != b || (AGGREGATE)t.aggregate != SCALAR || t.is_array();
    }
    friend constexpr bool operator!= (BASETYPE b, const TypeDesc &t) {
        return (BASETYPE)t.basetype != b || (AGGREGATE)t.aggregate != SCALAR || t.is_array();
    }

    /// TypeDesc's are equivalent if they are equal, or if their only
    /// inequality is differing vector semantics.
    friend constexpr bool equivalent (const TypeDesc &a, const TypeDesc &b) {
        return a.basetype == b.basetype && a.aggregate == b.aggregate &&
               (a.arraylen == b.arraylen || (a.is_unsized_array() && b.is_sized_array())
                                         || (a.is_sized_array()   && b.is_unsized_array()));
    }
    /// Member version of equivalent
    constexpr bool equivalent (const TypeDesc &b) const {
        return this->basetype == b.basetype && this->aggregate == b.aggregate &&
               (this->arraylen == b.arraylen || (this->is_unsized_array() && b.is_sized_array())
                                             || (this->is_sized_array()   && b.is_unsized_array()));
    }

    /// Is this a 3-vector aggregate (of the given type, float by default)?
    constexpr bool is_vec3 (BASETYPE b=FLOAT) const {
        return this->aggregate == VEC3 && this->basetype == b && !is_array();
    }

    /// Is this a 4-vector aggregate (of the given type, float by default)?
    constexpr bool is_vec4 (BASETYPE b=FLOAT) const {
        return this->aggregate == VEC4 && this->basetype == b && !is_array();
    }

    /// Demote the type to a non-array
    ///
    void unarray (void) { arraylen = 0; }

    /// Test for lexicographic 'less', comes in handy for lots of STL
    /// containers and algorithms.
    bool operator< (const TypeDesc &x) const;

    // DEPRECATED(1.8): These static const member functions were mildly
    // problematic because they required external linkage (and possibly
    // even static initialization order fiasco) and were a memory reference
    // that incurred some performance penalty and inability to optimize.
    // Please instead use the out-of-class constexpr versions below.  We
    // will eventually remove these.
    static const TypeDesc TypeFloat;
    static const TypeDesc TypeColor;
    static const TypeDesc TypeString;
    static const TypeDesc TypeInt;
    static const TypeDesc TypeHalf;
    static const TypeDesc TypePoint;
    static const TypeDesc TypeVector;
    static const TypeDesc TypeNormal;
    static const TypeDesc TypeMatrix;
    static const TypeDesc TypeMatrix33;
    static const TypeDesc TypeMatrix44;
    static const TypeDesc TypeTimeCode;
    static const TypeDesc TypeKeyCode;
    static const TypeDesc TypeFloat4;
    static const TypeDesc TypeRational;
};




// Static values for commonly used types. Because these are constexpr,
// they should incur no runtime construction cost and should optimize nicely
// in various ways.
static constexpr TypeDesc TypeUnknown (TypeDesc::UNKNOWN);
static constexpr TypeDesc TypeFloat (TypeDesc::FLOAT);
static constexpr TypeDesc TypeColor (TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR);
static constexpr TypeDesc TypePoint (TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::POINT);
static constexpr TypeDesc TypeVector (TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::VECTOR);
static constexpr TypeDesc TypeNormal (TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::NORMAL);
static constexpr TypeDesc TypeMatrix33 (TypeDesc::FLOAT, TypeDesc::MATRIX33);
static constexpr TypeDesc TypeMatrix44 (TypeDesc::FLOAT, TypeDesc::MATRIX44);
static constexpr TypeDesc TypeMatrix = TypeMatrix44;
static constexpr TypeDesc TypeString (TypeDesc::STRING);
static constexpr TypeDesc TypeInt (TypeDesc::INT);
static constexpr TypeDesc TypeUInt (TypeDesc::UINT);
static constexpr TypeDesc TypeInt32 (TypeDesc::INT);
static constexpr TypeDesc TypeUInt32 (TypeDesc::UINT);
static constexpr TypeDesc TypeInt16 (TypeDesc::INT16);
static constexpr TypeDesc TypeUInt16 (TypeDesc::UINT16);
static constexpr TypeDesc TypeInt8 (TypeDesc::INT8);
static constexpr TypeDesc TypeUInt8 (TypeDesc::UINT8);
static constexpr TypeDesc TypeHalf (TypeDesc::HALF);
static constexpr TypeDesc TypeTimeCode (TypeDesc::UINT, TypeDesc::SCALAR, TypeDesc::TIMECODE, 2);
static constexpr TypeDesc TypeKeyCode (TypeDesc::INT, TypeDesc::SCALAR, TypeDesc::KEYCODE, 7);
static constexpr TypeDesc TypeFloat4 (TypeDesc::FLOAT, TypeDesc::VEC4);
static constexpr TypeDesc TypeRational(TypeDesc::INT, TypeDesc::VEC2, TypeDesc::RATIONAL);



/// Return a string containing the data values formatted according
/// to the type and the optional formatting arguments.
std::string tostring (TypeDesc type, const void *data,
                      const char *float_fmt = "%f",         // E.g. "%g"
                      const char *string_fmt = "%s",        // E.g. "\"%s\""
                      const char aggregate_delim[2] = "()", // Both sides of vector
                      const char *aggregate_sep = ",",      // E.g. ", "
                      const char array_delim[2] = "{}",     // Both sides of array
                      const char *array_sep = ",");         // E.g. "; "



/// A template mechanism for getting the a base type from C type
///
template<typename T> struct BaseTypeFromC {};
template<> struct BaseTypeFromC<unsigned char> { static const TypeDesc::BASETYPE value = TypeDesc::UINT8; };
template<> struct BaseTypeFromC<char> { static const TypeDesc::BASETYPE value = TypeDesc::INT8; };
template<> struct BaseTypeFromC<unsigned short> { static const TypeDesc::BASETYPE value = TypeDesc::UINT16; };
template<> struct BaseTypeFromC<short> { static const TypeDesc::BASETYPE value = TypeDesc::INT16; };
template<> struct BaseTypeFromC<unsigned int> { static const TypeDesc::BASETYPE value = TypeDesc::UINT; };
template<> struct BaseTypeFromC<int> { static const TypeDesc::BASETYPE value = TypeDesc::INT; };
template<> struct BaseTypeFromC<unsigned long long> { static const TypeDesc::BASETYPE value = TypeDesc::UINT64; };
template<> struct BaseTypeFromC<long long> { static const TypeDesc::BASETYPE value = TypeDesc::INT64; };
#ifdef _HALF_H_
template<> struct BaseTypeFromC<half> { static const TypeDesc::BASETYPE value = TypeDesc::HALF; };
#endif
template<> struct BaseTypeFromC<float> { static const TypeDesc::BASETYPE value = TypeDesc::FLOAT; };
template<> struct BaseTypeFromC<double> { static const TypeDesc::BASETYPE value = TypeDesc::DOUBLE; };



/// A template mechanism for getting C type of TypeDesc::BASETYPE.
///
template<int b> struct CType {};
template<> struct CType<(int)TypeDesc::UINT8> { typedef unsigned char type; };
template<> struct CType<(int)TypeDesc::INT8> { typedef char type; };
template<> struct CType<(int)TypeDesc::UINT16> { typedef unsigned short type; };
template<> struct CType<(int)TypeDesc::INT16> { typedef short type; };
template<> struct CType<(int)TypeDesc::UINT> { typedef unsigned int type; };
template<> struct CType<(int)TypeDesc::INT> { typedef int type; };
template<> struct CType<(int)TypeDesc::UINT64> { typedef unsigned long long type; };
template<> struct CType<(int)TypeDesc::INT64> { typedef long long type; };
#ifdef _HALF_H_
template<> struct CType<(int)TypeDesc::HALF> { typedef half type; };
#endif
template<> struct CType<(int)TypeDesc::FLOAT> { typedef float type; };
template<> struct CType<(int)TypeDesc::DOUBLE> { typedef double type; };


OIIO_NAMESPACE_END

#endif /* !defined(OPENIMAGEIO_TYPEDESC_H) */
