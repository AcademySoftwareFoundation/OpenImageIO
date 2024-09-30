#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

import os
import OpenImageIO as oiio


try:
    config = oiio.ColorConfig()
    print ("getNumColorSpaces =", config.getNumColorSpaces())
    print ("getColorSpaceNames =", config.getColorSpaceNames())
    print ("Index of 'lin_srgb' =", config.getColorSpaceIndex('lin_srgb'))
    print ("Index of 'unknown' =", config.getColorSpaceIndex('unknown'))
    print ("Name of color space 2 =", config.getColorSpaceNameByIndex(2))
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

    print ("aliases of 'scene_linear' are", config.getAliases("scene_linear"))

    print ("resolve('foo'):", config.resolve("foo"))
    print ("resolve('linear'):", config.resolve("linear"))
    print ("resolve('scene_linear'):", config.resolve("scene_linear"))
    print ("resolve('lin_srgb'):", config.resolve("lin_srgb"))
    print ("resolve('srgb'):", config.resolve("srgb"))
    print ("resolve('ACEScg'):", config.resolve("ACEScg"))
    print ("equivalent('lin_srgb', 'srgb'):", config.equivalent("lin_srgb", "srgb"))
    print ("equivalent('scene_linear', 'srgb'):", config.equivalent("scene_linear", "srgb"))
    print ("equivalent('linear', 'lin_srgb'):", config.equivalent("scene_linear", "lin_srgb"))
    print ("equivalent('scene_linear', 'lin_srgb'):", config.equivalent("scene_linear", "lin_srgb"))
    print ("equivalent('ACEScg', 'scene_linear'):", config.equivalent("ACEScg", "scene_linear"))
    print ("equivalent('lnf', 'scene_linear'):", config.equivalent("lnf", "scene_linear"))
    print ("")

    print ("Done.")
except Exception as detail:
    print ("Unknown exception:", detail)

