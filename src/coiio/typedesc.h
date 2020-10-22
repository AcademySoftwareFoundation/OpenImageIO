#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/// BASETYPE is a simple enum describing the base data types that
/// correspond (mostly) to the C/C++ built-in types.
enum BASETYPE {
    BASETYPE_UNKNOWN,  ///< unknown type
    BASETYPE_NONE,     ///< void/no type
    BASETYPE_UINT8,    ///< 8-bit unsigned int values ranging from 0..255,
                       ///<   (C/C++ `unsigned char`).
    BASETYPE_UCHAR = BASETYPE_UINT8,
    BASETYPE_INT8,  ///< 8-bit int values ranging from -128..127,
                    ///<   (C/C++ `char`).
    BASETYPE_CHAR = BASETYPE_INT8,
    BASETYPE_UINT16,  ///< 16-bit int values ranging from 0..65535,
                      ///<   (C/C++ `unsigned short`).
    BASETYPE_USHORT = BASETYPE_UINT16,
    BASETYPE_INT16,  ///< 16-bit int values ranging from -32768..32767,
                     ///<   (C/C++ `short`).
    BASETYPE_SHORT = BASETYPE_INT16,
    BASETYPE_UINT32,  ///< 32-bit unsigned int values (C/C++ `unsigned int`).
    BASETYPE_UINT = BASETYPE_UINT32,
    BASETYPE_INT32,  ///< signed 32-bit int values (C/C++ `int`).
    BASETYPE_INT = BASETYPE_INT32,
    BASETYPE_UINT64,  ///< 64-bit unsigned int values (C/C++
                      ///<   `unsigned long long` on most architectures).
    BASETYPE_ULONGLONG = BASETYPE_UINT64,
    BASETYPE_INT64,  ///< signed 64-bit int values (C/C++ `long long`
                     ///<   on most architectures).
    BASETYPE_LONGLONG = BASETYPE_INT64,
    BASETYPE_HALF,    ///< 16-bit IEEE floating point values (OpenEXR `half`).
    BASETYPE_FLOAT,   ///< 32-bit IEEE floating point values, (C/C++ `float`).
    BASETYPE_DOUBLE,  ///< 64-bit IEEE floating point values, (C/C++ `double`).
    BASETYPE_STRING,  ///< Character string.
    BASETYPE_PTR,     ///< A pointer value.
    BASETYPE_LASTBASE
};

/// AGGREGATE describes whether our TypeDesc is a simple scalar of one
/// of the BASETYPE's, or one of several simple aggregates.
///
/// Note that aggregates and arrays are different. A `TypeDesc(FLOAT,3)`
/// is an array of three floats, a `TypeDesc(FLOAT,VEC3)` is a single
/// 3-component vector comprised of floats, and `TypeDesc(FLOAT,3,VEC3)`
/// is an array of 3 vectors, each of which is comprised of 3 floats.
enum AGGREGATE {
    AGGREGATE_SCALAR = 1,    ///< A single scalar value (such as a raw `int` or
                             ///<   `float` in C).  This is the default.
    AGGREGATE_VEC2     = 2,  ///< 2 values representing a 2D vector.
    AGGREGATE_VEC3     = 3,  ///< 3 values representing a 3D vector.
    AGGREGATE_VEC4     = 4,  ///< 4 values representing a 4D vector.
    AGGREGATE_MATRIX33 = 9,  ///< 9 values representing a 3x3 matrix.
    AGGREGATE_MATRIX44 = 16  ///< 16 values representing a 4x4 matrix.
};

/// VECSEMANTICS gives hints about what the data represent (for example,
/// if a spatial vector quantity should transform as a point, direction
/// vector, or surface normal).
enum VECSEMANTICS {
    VECSEMANTICS_NOXFORM     = 0,  ///< No semantic hints.
    VECSEMANTICS_NOSEMANTICS = 0,  ///< No semantic hints.
    VECSEMANTICS_COLOR,            ///< Color
    VECSEMANTICS_POINT,            ///< Point: a spatial location
    VECSEMANTICS_VECTOR,           ///< Vector: a spatial direction
    VECSEMANTICS_NORMAL,           ///< Normal: a surface normal
    VECSEMANTICS_TIMECODE,  ///< indicates an `int[2]` representing the standard
                            ///<   4-byte encoding of an SMPTE timecode.
    VECSEMANTICS_KEYCODE,   ///< indicates an `int[7]` representing the standard
                            ///<   28-byte encoding of an SMPTE keycode.
    VECSEMANTICS_RATIONAL  ///< A VEC2 representing a rational number `val[0] / val[1]`
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
/// integer and floating point, *common* aggregates such as 3-points,
/// and reasonably-lengthed arrays thereof.
///
/////////////////////////////////////////////////////////////////////////////

typedef struct {
    unsigned char basetype;
    unsigned char aggregate;
    unsigned char vecsemantics;
    unsigned char reserved;
    int arraylen;
} TypeDesc;

/// Construct from a string (e.g., "float[3]").  If no valid
/// type could be assembled, set base to UNKNOWN.
///
/// Examples:
/// ```
///      TypeDesc("int") == TypeDesc(TypeDesc::INT)            // C++ int32_t
///      TypeDesc("float") == TypeDesc(TypeDesc::FLOAT)        // C++ float
///      TypeDesc("uint16") == TypeDesc(TypeDesc::UINT16)      // C++ uint16_t
///      TypeDesc("float[4]") == TypeDesc(TypeDesc::FLOAT, 4)  // array
///      TypeDesc("point") == TypeDesc(TypeDesc::FLOAT,
///                                    TypeDesc::VEC3, TypeDesc::POINT)
/// ```
///
TypeDesc
TypeDesc_from_string(const char* typestring);

#ifdef __cplusplus
}
#endif