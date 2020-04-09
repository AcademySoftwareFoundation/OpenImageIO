#!/usr/bin/env python

from __future__ import print_function
from __future__ import absolute_import
import os
import OpenImageIO as oiio


try:
    config = oiio.ColorConfig()
    print ("getNumColorSpaces =", config.getNumColorSpaces())
    print ("getColorSpaceNames =", config.getColorSpaceNames())

    print ("getNumLooks =", config.getNumLooks())
    print ("getLookNames =", config.getLookNames())

    print ("getNumDisplays =", config.getNumDisplays())
    print ("getDisplayNames =", config.getDisplayNames())
    print ("getDefaultDisplayName =", config.getDefaultDisplayName())

    print ("getNumViews =", config.getNumViews())
    print ("getViewNames =", config.getViewNames())
    print ("getDefaultViewName =", config.getDefaultViewName())

    print ("getNumRoles =", config.getNumRoles())
    print ("getRoles =", config.getRoles())

    print ("")

    print ("Done.")
except Exception as detail:
    print ("Unknown exception:", detail)

