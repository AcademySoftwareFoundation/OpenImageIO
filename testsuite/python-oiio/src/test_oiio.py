#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

from __future__ import annotations

import OpenImageIO as oiio


def version_string_looks_valid(version: str) -> bool:
    # Loose check: non-empty, dotted, contains digits (e.g. "3.2.0.2dev").
    return (len(version) > 0 and "." in version
            and any(ch.isdigit() for ch in version))


def version_code(major: int, minor: int, patch: int) -> int:
    return major * 10000 + minor * 100 + patch





######################################################################
# main test starts here

try:
    print ("Testing version constants:")
    print ("  __version__ looks valid:",
           version_string_looks_valid (oiio.__version__))
    print ("  VERSION_STRING looks valid:",
           version_string_looks_valid (oiio.VERSION_STRING))
    print ("  __version__ == VERSION_STRING:",
           oiio.__version__ == oiio.VERSION_STRING)
    print ("  VERSION == openimageio_version:",
           oiio.VERSION == oiio.openimageio_version)
    print ("  VERSION matches major/minor/patch encoding:",
           oiio.VERSION == version_code (oiio.VERSION_MAJOR,
                                         oiio.VERSION_MINOR,
                                         oiio.VERSION_PATCH))
    print ("  VERSION is positive:", oiio.VERSION > 0)
    print ("  VERSION_MAJOR >= 1:", oiio.VERSION_MAJOR >= 1)
    print ("  VERSION_MINOR >= 0:", oiio.VERSION_MINOR >= 0)
    print ("  VERSION_PATCH >= 0:", oiio.VERSION_PATCH >= 0)
    print ("  INTRO_STRING starts with OpenImageIO:",
           oiio.INTRO_STRING.startswith ("OpenImageIO"))
    print ("  AutoStride =", oiio.AutoStride)
    print ("")

    print ("Testing is_imageio_format_name:")
    print ("  tiff =", oiio.is_imageio_format_name ('tiff'))
    print ("  openexr =", oiio.is_imageio_format_name ('openexr'))
    print ("  txff =", oiio.is_imageio_format_name ('txff'))
    print ("")

    print ("Testing equivalent_colorspace:")
    print ("  sRGB vs sRGB =", oiio.equivalent_colorspace ("sRGB", "sRGB"))
    print ("  linear vs Linear =", oiio.equivalent_colorspace ("linear", "Linear"))
    print ("  sRGB vs linear =", oiio.equivalent_colorspace ("sRGB", "linear"))
    print ("")

    print ("Testing set_colorspace module helpers:")
    spec = oiio.ImageSpec()
    oiio.set_colorspace (spec, "sRGB")
    print ("  after set_colorspace(sRGB):",
           spec.get_string_attribute ("oiio:ColorSpace"))
    oiio.set_colorspace (spec, "")
    print ("  after set_colorspace(empty):",
           spec.get_string_attribute ("oiio:ColorSpace"))
    oiio.set_colorspace_rec709_gamma (spec, 2.2)
    print ("  after set_colorspace_rec709_gamma(2.2):",
           spec.get_string_attribute ("oiio:ColorSpace"))
    print ("")

    print ("Testing global attribute() one-arg:")
    oiio.attribute ("plugin_searchpath", "path/A:path/B")
    print ("  plugin_searchpath str:",
           oiio.get_string_attribute ("plugin_searchpath", ""))
    oiio.attribute ("threads", 6)
    print ("  threads int:", oiio.get_int_attribute ("threads", 0))
    oiio.attribute ("debug", 0)
    print ("  debug int:", oiio.get_int_attribute ("debug", -1))
    oiio.attribute ("font_searchpath", b"/fonts")
    print ("  font_searchpath str:",
           oiio.get_string_attribute ("font_searchpath", ""))
    gb = oiio.get_bytes_attribute ("font_searchpath", b"")
    print ("  font_searchpath bytes type:", isinstance (gb, bytes))
    print ("  font_searchpath bytes:", gb)
    print ("")

    print ("Testing global attribute() typed:")
    oiio.attribute ("threads", oiio.TypeInt, 4)
    print ("  threads typed get:", oiio.getattribute ("threads", oiio.TypeInt))
    version_typed = oiio.getattribute ("version", oiio.TypeString)
    print ("  version typed looks valid:",
           version_string_looks_valid (version_typed))
    print ("  version typed == __version__:", version_typed == oiio.__version__)
    print ("  missing typed get:",
           oiio.getattribute ("no_such_global_attr", oiio.TypeString))
    print ("")

    print ("Testing global get_*_attribute defaults:")
    print ("  get_int missing:", oiio.get_int_attribute ("not_a_real_attr", 99))
    print ("  get_float missing:",
           oiio.get_float_attribute ("not_a_real_attr", 1.5))
    print ("  get_string missing:",
           oiio.get_string_attribute ("not_a_real_attr", "default"))
    print ("  get_bytes missing:",
           oiio.get_bytes_attribute ("not_a_real_attr", b"default"))
    print ("  get_int plugin_searchpath (wrong type) default:",
           oiio.get_int_attribute ("plugin_searchpath", 77))
    print ("  get_float on int attribute returns default:",
           oiio.get_float_attribute ("threads", -1.0) == -1.0)
    print ("")

    print ("Testing getattribute() with TypeUnknown:")
    print ("  getattribute(missing, TypeUnknown):",
           oiio.getattribute ("not_a_real_attr", oiio.TypeUnknown))
    print ("")

    print ("Testing getattribute() one-arg (pybind default):")
    try:
        oiio.getattribute ("not_a_real_attr")
        print ("  one-arg: no exception")
    except TypeError:
        print ("  one-arg: TypeError")
    print ("")

    print ("Testing attribute() type error:")
    try:
        oiio.attribute ("threads", [])
        print ("  attribute bad type: no exception")
    except TypeError:
        print ("  attribute bad type: TypeError")
    print ("")

    print ("Testing geterror:")
    print ("  geterror before error:", repr (oiio.geterror()))
    inp = oiio.ImageInput.open ("no_such_file_for_oiio_test.tif")
    print ("  open returned:", inp)
    err_noclear = oiio.geterror (clear=False)
    print ("  geterror(clear=False) nonempty:", len (err_noclear) > 0)
    err_noclear2 = oiio.geterror (clear=False)
    print ("  geterror persists:", err_noclear == err_noclear2)
    err_clear = oiio.geterror (clear=True)
    print ("  geterror(clear=True) same as prior:", err_clear == err_noclear)
    print ("  geterror after clear:", repr (oiio.geterror()))
    print ("")

    print ("Done.")
except Exception as detail:
    print ("Unknown exception:", detail)
