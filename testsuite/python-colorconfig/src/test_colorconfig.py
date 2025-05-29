#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

from __future__ import annotations

import os
from pathlib import Path
import OpenImageIO as oiio
import numpy as np

TEST_CONFIG_PATH: Path = Path(__file__).parent/"oiio_test_v0.9.2.ocio"

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

    config = oiio.ColorConfig(str(TEST_CONFIG_PATH))
    print (f"Loaded test OCIO config: {TEST_CONFIG_PATH.name}")
    display = config.getDefaultDisplayName()
    default_cs = config.getColorSpaceFromFilepath("foo.exr")
    filepath_cs = config.getColorSpaceFromFilepath("foo_lin_ap1.exr")
    print (f"Parsed color space for filepath 'foo_lin_ap1.exr': {filepath_cs}")
    print (f"Default color space: {default_cs}")
    print (f"Default display: {display}")
    print (f"Default view for {display} (from {default_cs}): {config.getDefaultViewName(display, default_cs)}")
    print (f"Default view for {display} (from 'srgb_tx'): {config.getDefaultViewName(display, 'srgb_tx')}")
    print (f"Color space name from DisplayView transform referencing Shared View: {config.getDisplayViewColorSpaceName('sRGB (~2.22) - Display', 'Colorimetry')}")
    buf = oiio.ImageBuf(np.array([[[0.1, 0.5, 0.9]]]))
    spec  = buf.specmod()
    spec.set_colorspace(config.getColorSpaceFromFilepath("foo_lin_ap1.exr"))
    print (f"Test buffer -- initial values:                      {buf.get_pixels(oiio.HALF)}                 ({spec['oiio:ColorSpace']})")
    buf = oiio.ImageBufAlgo.ociodisplay(buf, "", "", colorconfig=str(TEST_CONFIG_PATH))
    print (f"ociodisplay #1 (apply default display/view):        {buf.get_pixels(oiio.HALF)}     ({buf.spec()['oiio:ColorSpace']})")
    buf = oiio.ImageBufAlgo.ociodisplay(buf, "", "", colorconfig=str(TEST_CONFIG_PATH))
    print (f"ociodisplay #2 (apply default display/view again):  {buf.get_pixels(oiio.HALF)}     ({buf.spec()['oiio:ColorSpace']})")
    buf = oiio.ImageBufAlgo.ociodisplay(buf, "", "", colorconfig=str(TEST_CONFIG_PATH), looks="-ACES 1.3 Reference Gamut Compression")
    print (f"ociodisplay #3 (inverse look):                      {buf.get_pixels(oiio.HALF)}     ({buf.spec()['oiio:ColorSpace']})")
    buf = oiio.ImageBufAlgo.ociodisplay(buf, "", "", colorconfig=str(TEST_CONFIG_PATH), looks="ACES 1.3 Reference Gamut Compression")
    print (f"ociodisplay #4 (forwards look):                     {buf.get_pixels(oiio.HALF)}     ({buf.spec()['oiio:ColorSpace']})")
    buf = oiio.ImageBufAlgo.ociodisplay(buf, "", "", colorconfig=str(TEST_CONFIG_PATH), looks="-ACES 1.3 Reference Gamut Compression, +ACES 1.3 Reference Gamut Compression")
    print (f"ociodisplay #5 (inverse look + forwards look):      {buf.get_pixels(oiio.HALF)}     ({buf.spec()['oiio:ColorSpace']})")
    print ("")

    print ("Done.")

except Exception as detail:
    print ("Unknown exception:", detail)

