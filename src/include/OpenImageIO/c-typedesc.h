// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio


#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/// OIIO_BASETYPE is a simple enum describing the base data types that
/// correspond (mostly) to the C/C++ built-in types.
enum OIIO_BASETYPE {
    OIIO_BASETYPE_UNKNOWN,  ///< unknown type
    OIIO_BASETYPE_NONE,     ///< void/no type
    OIIO_BASETYPE_UINT8,    ///< 8-bit unsigned int values ranging from 0..255,
                            ///<   (C/C++ `unsigned char`).
    OIIO_BASETYPE_UCHAR = OIIO_BASETYPE_UINT8,
    OIIO_BASETYPE_INT8,  ///< 8-bit int values ranging from -128..127,
                         ///<   (C/C++ `char`).
    OIIO_BASETYPE_CHAR = OIIO_BASETYPE_INT8,
    OIIO_BASETYPE_UINT16,  ///< 16-bit int values ranging from 0..65535,
                           ///<   (C/C++ `unsigned short`).
    OIIO_BASETYPE_USHORT = OIIO_BASETYPE_UINT16,
    OIIO_BASETYPE_INT16,  ///< 16-bit int values ranging from -32768..32767,
                          ///<   (C/C++ `short`).
    OIIO_BASETYPE_SHORT = OIIO_BASETYPE_INT16,
    OIIO_BASETYPE_UINT32,  ///< 32-bit unsigned int values (C/C++ `unsigned int`).
    OIIO_BASETYPE_UINT = OIIO_BASETYPE_UINT32,
    OIIO_BASETYPE_INT32,  ///< signed 32-bit int values (C/C++ `int`).
    OIIO_BASETYPE_INT = OIIO_BASETYPE_INT32,
    OIIO_BASETYPE_UINT64,  ///< 64-bit unsigned int values (C/C++
                           ///<   `unsigned long long` on most architectures).
    OIIO_BASETYPE_ULONGLONG = OIIO_BASETYPE_UINT64,
    OIIO_BASETYPE_INT64,  ///< signed 64-bit int values (C/C++ `long long`
                          ///<   on most architectures).
    OIIO_BASETYPE_LONGLONG = OIIO_BASETYPE_INT64,
    OIIO_BASETYPE_HALF,  ///< 16-bit IEEE floating point values (OpenEXR `half`).
    OIIO_BASETYPE_FLOAT,  ///< 32-bit IEEE floating point values, (C/C++ `float`).
    OIIO_BASETYPE_DOUBLE,  ///< 64-bit IEEE floating point values, (C/C++ `double`).
    OIIO_BASETYPE_STRING,  ///< Character string.
    OIIO_BASETYPE_PTR,     ///< A pointer value.
    OIIO_BASETYPE_LASTBASE
};

/// OIIO_AGGREGATE describes whether our TypeDesc is a simple scalar of one
/// of the OIIO_BASETYPE's, or one of several simple OIIO_AGGREGATEs.
///
/// Note that OIIO_AGGREGATEs and arrays are different. A `TypeDesc(FLOAT,3)`
/// is an array of three floats, a `TypeDesc(FLOAT,VEC3)` is a single
/// 3-component vector comprised of floats, and `TypeDesc(FLOAT,3,VEC3)`
/// is an array of 3 vectors, each of which is comprised of 3 floats.
enum OIIO_AGGREGATE {
    OIIO_AGGREGATE_SCALAR
    = 1,  ///< A single scalar value (such as a raw `int` or
          ///<   `float` in C).  This is the default.
    OIIO_AGGREGATE_VEC2     = 2,  ///< 2 values representing a 2D vector.
    OIIO_AGGREGATE_VEC3     = 3,  ///< 3 values representing a 3D vector.
    OIIO_AGGREGATE_VEC4     = 4,  ///< 4 values representing a 4D vector.
    OIIO_AGGREGATE_MATRIX33 = 9,  ///< 9 values representing a 3x3 matrix.
    OIIO_AGGREGATE_MATRIX44 = 16  ///< 16 values representing a 4x4 matrix.
};

/// OIIO_VECSEMANTICS gives hints about what the data represent (for example,
/// if a spatial vector quantity should transform as a point, direction
/// vector, or surface normal).
enum OIIO_VECSEMANTICS {
    OIIO_VECSEMANTICS_NOXFORM     = 0,  ///< No semantic hints.
    OIIO_VECSEMANTICS_NOSEMANTICS = 0,  ///< No semantic hints.
    OIIO_VECSEMANTICS_COLOR,            ///< Color
    OIIO_VECSEMANTICS_POINT,            ///< Point: a spatial location
    OIIO_VECSEMANTICS_VECTOR,           ///< Vector: a spatial direction
    OIIO_VECSEMANTICS_NORMAL,           ///< Normal: a surface normal
    OIIO_VECSEMANTICS_TIMECODE,  ///< indicates an `int[2]` representing the standard
                                 ///<   4-byte encoding of an SMPTE timecode.
    OIIO_VECSEMANTICS_KEYCODE,  ///< indicates an `int[7]` representing the standard
                                ///<   28-byte encoding of an SMPTE keycode.
    OIIO_VECSEMANTICS_RATIONAL  ///< A VEC2 representing a rational number `val[0] / val[1]`
};

/////////////////////////////////////////////////////////////////////////////
/// A TypeDesc describes simple data types.
///
/// It frequently comes up (in my experience, with renderers and image
/// handling programs) that you want a way to describe data that is passed
/// through APIs through blind pointers.  These are some simple classes
/// that provide a simple type descriptor system.  This is not meant to
/// be comprehensive -- for example, there is no provision for structs,
/// unions, pointers, const, or 'nested' type definitions.  Just simple
/// integer and floating point, *common* OIIO_AGGREGATEs such as 3-points,
/// and reasonably-lengthed arrays thereof.
///
/////////////////////////////////////////////////////////////////////////////

typedef struct {
    unsigned char basetype;
    unsigned char aggregate;
    unsigned char vecsemantics;
    unsigned char reserved;
    int arraylen;
} OIIO_TypeDesc;

/// Construct from a string (e.g., "float[3]").  If no valid
/// type could be assembled, set basetype to OIIO_BASETYPE_UNKNOWN.
///
/// Examples:
/// ```
///      TypeDesc_from_string("int") == OIIO_TypeInt            // C++ int32_t
///      TypeDesc_from_string("float") == OIIO_TypeFloat        // C++ float
///      TypeDesc_from_string("uint16") == OIIO_TYPEUInt16      // C++ uint16_t
///      TypeDesc_from_string("float[4]") == FIXME: unimplemented!
/// ```
///
OIIOC_API OIIO_TypeDesc
OIIO_TypeDesc_from_string(const char* typestring);

extern OIIOC_API OIIO_TypeDesc OIIO_TypeUnknown;
extern OIIOC_API OIIO_TypeDesc OIIO_TypeFloat;
extern OIIOC_API OIIO_TypeDesc OIIO_TypeColor;
extern OIIOC_API OIIO_TypeDesc OIIO_TypePoint;
extern OIIOC_API OIIO_TypeDesc OIIO_TypeVector;
extern OIIOC_API OIIO_TypeDesc OIIO_TypeNormal;
extern OIIOC_API OIIO_TypeDesc OIIO_TypeMatrix33;
extern OIIOC_API OIIO_TypeDesc OIIO_TypeMatrix44;
extern OIIOC_API OIIO_TypeDesc OIIO_TypeMatrix;
extern OIIOC_API OIIO_TypeDesc OIIO_TypeFloat2;
extern OIIOC_API OIIO_TypeDesc OIIO_TypeVector2;
extern OIIOC_API OIIO_TypeDesc OIIO_TypeFloat4;
extern OIIOC_API OIIO_TypeDesc OIIO_TypeVector4;
extern OIIOC_API OIIO_TypeDesc OIIO_TypeString;
extern OIIOC_API OIIO_TypeDesc OIIO_TypeInt;
extern OIIOC_API OIIO_TypeDesc OIIO_TypeUInt;
extern OIIOC_API OIIO_TypeDesc OIIO_TypeInt32;
extern OIIOC_API OIIO_TypeDesc OIIO_TypeUInt32;
extern OIIOC_API OIIO_TypeDesc OIIO_TypeInt16;
extern OIIOC_API OIIO_TypeDesc OIIO_TypeUInt16;
extern OIIOC_API OIIO_TypeDesc OIIO_TypeInt8;
extern OIIOC_API OIIO_TypeDesc OIIO_TypeUInt8;
extern OIIOC_API OIIO_TypeDesc OIIO_TypeVector2i;
extern OIIOC_API OIIO_TypeDesc OIIO_TypeHalf;
extern OIIOC_API OIIO_TypeDesc OIIO_TypeTimecode;
extern OIIOC_API OIIO_TypeDesc OIIO_TypeKeycode;
extern OIIOC_API OIIO_TypeDesc OIIO_TypeRational;
extern OIIOC_API OIIO_TypeDesc OIIO_TypePointer;

#ifdef __cplusplus
}
#endif
