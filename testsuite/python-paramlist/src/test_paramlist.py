#!/usr/bin/env python

from __future__ import print_function
import OpenImageIO as oiio





######################################################################
# main test starts here

try:
    pl = oiio.ParamValueList()
    pl.attribute ("i", 1)
    pl.attribute ("s", "Bob")
    pl.attribute ("e", 2.718281828459045)
    pl["j"] = 42
    pl["foo"] = "bar"
    pl["pi"] = 3.141592653589793

    print ("pl length is", len(pl))
    for p in pl :
        print ("  item", p.name, p.type, p.value)

    print ("pl.contains('e') =", pl.contains('e'))
    print ("pl.contains('f') =", pl.contains('f'))
    print ("pl[1] =", pl[1].name, pl[1].type, pl[1].value)
    print ("pl['e'] =", pl['e'])
    print ("pl['pi'] =", pl['pi'])
    print ("pl['foo'] =", pl['foo'])

    pl.remove('e')
    print ("after removing 'e', len=", len(pl), "pl.contains('e')=", pl.contains('e'))

    pl.sort()
    print ("after sorting:")
    for p in pl :
        print ("  item", p.name, p.type, p.value)

    print ("Done.")
except Exception as detail:
    print ("Unknown exception:", detail)

