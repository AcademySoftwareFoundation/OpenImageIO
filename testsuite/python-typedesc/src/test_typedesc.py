#!/usr/bin/env python

from __future__ import print_function
from __future__ import absolute_import
import OpenImageIO as oiio


# Test that every expected enum value of BASETYPE exists
def basetype_enum_test():
    try:
        oiio.UNKNOWN
        oiio.NONE
        oiio.UCHAR
        oiio.UINT8
        oiio.CHAR
        oiio.INT8
        oiio.USHORT
        oiio.UINT16
        oiio.SHORT
        oiio.INT16
        oiio.UINT
        oiio.UINT32
        oiio.INT
        oiio.INT32
        oiio.ULONGLONG
        oiio.UINT64
        oiio.LONGLONG
        oiio.INT64
        oiio.HALF
        oiio.FLOAT
        oiio.DOUBLE
        oiio.STRING
        oiio.PTR
        oiio.LASTBASE
        print ("Passed BASETYPE")
    except:
        print ("Failed BASETYPE")


# Test that every expected enum value of AGGREGATE exists
def aggregate_enum_test():
    try:
        oiio.NOSEMANTICS
        oiio.SCALAR
        oiio.VEC2
        oiio.VEC3
        oiio.VEC4
        oiio.MATRIX33
        oiio.MATRIX44
        print ("Passed AGGREGATE")
    except:
        print ("Failed AGGREGATE")


# Test that every expected enum value of VECSEMANTICS exists
def vecsemantics_enum_test():
    try:
        oiio.NOXFORM
        oiio.COLOR
        oiio.POINT
        oiio.VECTOR
        oiio.NORMAL
        oiio.TIMECODE
        oiio.KEYCODE
        oiio.RATIONAL
        print ("Passed VECSEMANTICS")
    except:
        print ("Failed VECSEMANTICS")

# print the details of a type t
def breakdown_test(t, name="", verbose=True):
    print ("type '%s'" % name)
    print ("    c_str \"" + t.c_str() + "\"")
    if verbose:
        print ("    basetype", t.basetype)
        print ("    aggregate", t.aggregate)
        print ("    vecsemantics", t.vecsemantics)
        print ("    arraylen", t.arraylen)
        print ("    str(t) = \"" + str(t) + "\"")
        print ("    size =", t.size())
        print ("    elementtype =", t.elementtype())
        print ("    numelements =", t.numelements())
        print ("    basevalues =", t.basevalues())
        print ("    elementsize =", t.elementsize())
        print ("    basesize =", t.basesize())


######################################################################
# main test starts here

try:
    # Test that all the enum values exist
    basetype_enum_test()
    aggregate_enum_test()
    vecsemantics_enum_test()
    print ("")

    # Exercise the different constructors, make sure they create the
    # correct TypeDesc (also exercises the individual fields, c_str(),
    # conversion to string).
    breakdown_test (oiio.TypeDesc(), "(default)")
    breakdown_test (oiio.TypeDesc(oiio.UINT8), "UINT8")
    breakdown_test (oiio.TypeDesc(oiio.HALF, oiio.VEC3, oiio.COLOR),
                    "HALF, VEC3, COLOR")
    breakdown_test (oiio.TypeDesc(oiio.FLOAT, oiio.SCALAR, oiio.NOXFORM, 6),
                    "FLOAT, SCALAR, NOXFORM, array of 6")
    breakdown_test (oiio.TypeDesc(oiio.FLOAT, oiio.VEC3, oiio.POINT, 2),
                    "FLOAT, VEC3, POINT, array of 2")
    print ("")

    # Test construction from a string descriptor
    breakdown_test (oiio.TypeDesc("float[2]"), "float[2]")
    breakdown_test (oiio.TypeDesc("normal"), "normal")
    breakdown_test (oiio.TypeDesc("uint16"), "uint16")
    print ("")

    # Test equality, inequality, and equivalent
    t_uint8 = oiio.TypeDesc("uint8")
    t_uint16 = oiio.TypeDesc("uint16")
    t_uint8_b = oiio.TypeDesc("uint8")
    print ("uint8 == uint8?", (t_uint8 == t_uint8))
    print ("uint8 == uint8?", (t_uint8 == t_uint8_b))
    print ("uint8 == uint16", (t_uint8 == t_uint16))
    print ("uint8 != uint8?", (t_uint8 != t_uint8))
    print ("uint8 != uint8?", (t_uint8 != t_uint8_b))
    print ("uint8 != uint16", (t_uint8 != t_uint16))
    print ("vector == color", (oiio.TypeDesc("vector") == oiio.TypeDesc("color")))
    print ("vector.equivalent(color)", oiio.TypeDesc("vector").equivalent(oiio.TypeDesc("color")))
    print ("equivalent(vector,color)", oiio.TypeDesc.equivalent(oiio.TypeDesc("vector"), oiio.TypeDesc("color")))
    print ("vector.equivalent(float)", oiio.TypeDesc("vector").equivalent(oiio.TypeDesc("float")))
    print ("equivalent(vector,float)", oiio.TypeDesc.equivalent(oiio.TypeDesc("vector"), oiio.TypeDesc("float")))
    print ("")

    # DEPRECATED(1.8): Test the static data member types of pre-constructed types
    breakdown_test (oiio.TypeDesc.TypeFloat,    "TypeFloat",    verbose=False)
    breakdown_test (oiio.TypeDesc.TypeColor,    "TypeColor",    verbose=False)
    breakdown_test (oiio.TypeDesc.TypeString,   "TypeString",   verbose=False)
    breakdown_test (oiio.TypeDesc.TypeInt,      "TypeInt",      verbose=False)
    breakdown_test (oiio.TypeDesc.TypePoint,    "TypePoint",    verbose=False)
    breakdown_test (oiio.TypeDesc.TypeVector,   "TypeVector",   verbose=False)
    breakdown_test (oiio.TypeDesc.TypeNormal,   "TypeNormal",   verbose=False)
    breakdown_test (oiio.TypeDesc.TypeMatrix,   "TypeMatrix",   verbose=False)
    breakdown_test (oiio.TypeDesc.TypeMatrix33, "TypeMatrix33", verbose=False)
    breakdown_test (oiio.TypeDesc.TypeMatrix44, "TypeMatrix44", verbose=False)
    breakdown_test (oiio.TypeDesc.TypeTimeCode, "TypeTimeCode", verbose=False)
    breakdown_test (oiio.TypeDesc.TypeKeyCode,  "TypeKeyCode",  verbose=False)
    breakdown_test (oiio.TypeDesc.TypeRational, "TypeRational", verbose=False)
    breakdown_test (oiio.TypeDesc.TypeFloat2,   "TypeFloat2",   verbose=False)
    breakdown_test (oiio.TypeDesc.TypeVector2,  "TypeVector2",  verbose=False)
    breakdown_test (oiio.TypeDesc.TypeFloat4,   "TypeFloat4",   verbose=False)
    breakdown_test (oiio.TypeDesc.TypeVector4,  "TypeVector4",  verbose=False)
    breakdown_test (oiio.TypeDesc.TypeVector2i, "TypeVector2i", verbose=False)
    breakdown_test (oiio.TypeDesc.TypeHalf,     "TypeHalf",     verbose=False)
    print ("")

    # Test the pre-constructed types
    breakdown_test (oiio.TypeFloat,    "TypeFloat",    verbose=False)
    breakdown_test (oiio.TypeColor,    "TypeColor",    verbose=False)
    breakdown_test (oiio.TypeString,   "TypeString",   verbose=False)
    breakdown_test (oiio.TypeInt,      "TypeInt",      verbose=False)
    breakdown_test (oiio.TypeUInt,     "TypeUInt",     verbose=False)
    breakdown_test (oiio.TypeInt32,    "TypeInt32",    verbose=False)
    breakdown_test (oiio.TypeUInt32,   "TypeUInt32",   verbose=False)
    breakdown_test (oiio.TypeInt16,    "TypeInt16",    verbose=False)
    breakdown_test (oiio.TypeUInt16,   "TypeUInt16",   verbose=False)
    breakdown_test (oiio.TypeInt8,     "TypeInt8",     verbose=False)
    breakdown_test (oiio.TypeUInt8,    "TypeUInt8",    verbose=False)
    breakdown_test (oiio.TypePoint,    "TypePoint",    verbose=False)
    breakdown_test (oiio.TypeVector,   "TypeVector",   verbose=False)
    breakdown_test (oiio.TypeNormal,   "TypeNormal",   verbose=False)
    breakdown_test (oiio.TypeMatrix,   "TypeMatrix",   verbose=False)
    breakdown_test (oiio.TypeMatrix33, "TypeMatrix33", verbose=False)
    breakdown_test (oiio.TypeMatrix44, "TypeMatrix44", verbose=False)
    breakdown_test (oiio.TypeTimeCode, "TypeTimeCode", verbose=False)
    breakdown_test (oiio.TypeKeyCode,  "TypeKeyCode",  verbose=False)
    breakdown_test (oiio.TypeFloat2,   "TypeFloat2",   verbose=False)
    breakdown_test (oiio.TypeVector2,  "TypeVector2",  verbose=False)
    breakdown_test (oiio.TypeFloat4,   "TypeFloat4",   verbose=False)
    breakdown_test (oiio.TypeVector4,  "TypeVector4",  verbose=False)
    breakdown_test (oiio.TypeVector2i, "TypeVector2i", verbose=False)
    breakdown_test (oiio.TypeHalf,     "TypeHalf",     verbose=False)
    breakdown_test (oiio.TypeRational, "TypeRational", verbose=False)
    breakdown_test (oiio.TypeUInt,     "TypeUInt",     verbose=False)
    print ("")

    print ("Done.")
except Exception as detail:
    print ("Unknown exception:", detail)

