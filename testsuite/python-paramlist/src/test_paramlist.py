#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

from __future__ import annotations

import numpy
import OpenImageIO as oiio


def print_param_value(p: oiio.ParamValue) :
    if type(p.value) == float :
        print ("  item {} {} {:.6}".format(p.name, p.type, p.value))
    else :
        print ("  item {} {} {}".format(p.name, p.type, p.value))

def print_param_list(pl: oiio.ParamValueList) :
    for p in pl :
        print_param_value(p)



######################################################################
# main test starts here

try:
    print ("Testing individual ParamValue:")
    # Construct from scalars
    pv = oiio.ParamValue("a", 42)
    print_param_value(pv)
    pv = oiio.ParamValue("b", 3.5)
    print_param_value(pv)
    pv = oiio.ParamValue("c", "xyzpdq")
    print_param_value(pv)
    # Construct from tuple
    pv = oiio.ParamValue("d", "float[4]", (3.5, 4.5, 5.5, 6.5))
    print_param_value(pv)
    # Construct from tuple with nvalues/interp
    pv = oiio.ParamValue("e", "float", 4, oiio.Interp.LINEAR, (1, 3, 5, 7))
    print_param_value(pv)
    # Construct from a list
    pv = oiio.ParamValue("f", "point", [3.5, 4.5, 5.5, 6.5])
    print_param_value(pv)
    # Construct from a numpy array
    pv = oiio.ParamValue("g", "color",
                         numpy.array([0.25, 0.5, 0.75], dtype='f'))
    print_param_value(pv)
    # Construct from numpy byte array
    pv = oiio.ParamValue("ucarr", "uint8[10]", numpy.array([49, 50, 51, 0, 0, 97, 98, 99, 1, 88], dtype='B'))
    print_param_value(pv)

    # C++: paramvalue_from_pyobject UINT8 + Python bytes (pybind parity). Copy raw
    # bytes when type.arraylen is set, len(bytes) == arraylen*nvalues, and if
    # arraylen<0 (uint8[]), set arraylen = len(bytes)//nvalues first.

    # Setup: fixed width uint8[10], single value — exercises bytes special case without inference.
    pv = oiio.ParamValue("bts", "uint8[10]", b'123\x00\x00abc\x01X')
    # Check: name/type/value match ref; bytes land as uint8[10] (10*1==10).
    print_param_value(pv)

    # Setup: unsized uint8[], nvalues 1 — must infer arraylen 10 from len(bytes).
    pv = oiio.ParamValue("u8unsized", oiio.TypeDesc("uint8[]"), b"1234567890")
    # Check: type prints as uint8[10] (inferred) and payload matches the string bytes.
    print_param_value(pv)

    # Setup: uint8[2] with nvalues=3, 6 bytes — fixed arraylen, multiple ParamValue "values".
    pv = oiio.ParamValue(
        "u8fix_n2",
        oiio.TypeDesc("uint8[2]"),
        3,
        oiio.Interp.CONSTANT,
        b"abcdef",
    )
    # Check: 2*3==6; same special-case path, no arraylen<0 branch.
    print_param_value(pv)

    # Setup: unsized uint8[], nvalues=2, 8 bytes — infer arraylen=4, then 4*2==8.
    pv = oiio.ParamValue(
        "u8var_n2",
        oiio.TypeDesc("uint8[]"),
        2,
        oiio.Interp.CONSTANT,
        b"12345678",
    )
    # Check: type uint8[4] with 2 nvalues; exercises inference with nvalues>1.
    print_param_value(pv)

    print ("")

    print ("Testing ParamValueList:")
    pl = oiio.ParamValueList()
    pl.attribute ("i", 1)
    pl.attribute ("s", "Bob")
    pl.attribute ("e", 2.718281828459045)
    pl.attribute ("P", "point", (2.0, 42.0, 1.0))
    pl.attribute ("pressure", "float", 4, [98.0, 98.5, 99.0, 99.5])
    pl.attribute ("ucarr", "uint8[10]", numpy.array([49, 50, 51, 0, 0, 97, 98, 99, 1, 88], dtype='B'))
    pl["j"] = 42
    pl["foo"] = "bar"
    pl["pi"] = 3.141592653589793

    print ("pl length is", len(pl))
    print_param_list (pl)

    print ("pl.contains('e') =", pl.contains('e'))
    print ("pl.contains('f') =", pl.contains('f'))
    print ("pl[1] =", pl[1].name, pl[1].type, pl[1].value)
    print ("pl['e'] = {:.6}".format(pl['e']))
    print ("pl['pi'] = {:.6}".format(pl['pi']))
    print ("pl['foo'] =", pl['foo'])
    print ("pl['ucarr'] =", pl['ucarr'])
    print ("'e' in pl =", 'e' in pl)

    pl.remove('e')
    print ("after removing 'e', len=", len(pl), "pl.contains('e')=", pl.contains('e'))

    pl['x'] = 123
    print ("after adding 'x', then 'x' in pl =", 'x' in pl)
    del pl['x']
    print ("after removing 'x', then 'x' in pl =", 'x' in pl)

    try :
        print ("pl['unknown'] =", pl['unknown'])
    except KeyError :
        print ("pl['unknown'] raised a KeyError (as expected)")
    except :
        print ("pl['unknown'] threw an unknown exception (oh no!)")

    pl2 = oiio.ParamValueList()
    pl2.attribute ("a", "aval")
    pl2.attribute ("m", 1)
    print ("pl2 =")
    print_param_list(pl2)
    pl.merge(pl2)
    print ("After merge, pl =")
    print_param_list(pl)

    pl.sort()
    print ("after sorting:")
    print_param_list(pl)

    print ("Done.")
except Exception as detail:
    print ("Unknown exception:", detail)

