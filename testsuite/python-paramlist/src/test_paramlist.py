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
        if type(p.value) == float :
            print ("  item {} {} {:.6}".format(p.name, p.type, p.value))
        else :
            print ("  item {} {} {}".format(p.name, p.type, p.value))

    print ("pl.contains('e') =", pl.contains('e'))
    print ("pl.contains('f') =", pl.contains('f'))
    print ("pl[1] =", pl[1].name, pl[1].type, pl[1].value)
    print ("pl['e'] = {:.6}".format(pl['e']))
    print ("pl['pi'] = {:.6}".format(pl['pi']))
    print ("pl['foo'] =", pl['foo'])

    pl.remove('e')
    print ("after removing 'e', len=", len(pl), "pl.contains('e')=", pl.contains('e'))

    pl.sort()
    print ("after sorting:")
    for p in pl :
        if type(p.value) == float :
            print ("  item {} {} {:.6}".format(p.name, p.type, p.value))
        else :
            print ("  item {} {} {}".format(p.name, p.type, p.value))

    print ("Done.")
except Exception as detail:
    print ("Unknown exception:", detail)

