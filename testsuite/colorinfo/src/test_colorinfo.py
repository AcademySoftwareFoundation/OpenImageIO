#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Python binding of the cheap characterization surface:
# ColorConfig.get_color_space_info / get_color_space_infos and the
# ColorSpaceInfo record. Unavailable fields are None (never empty strings);
# invalid scalar input is None with the error on the config; invalid batch
# input is [] with one indexed error.

import OpenImageIO as oiio

cc = oiio.ColorConfig("src/colorinfo.ocio")
F = oiio.ColorSpaceInfoField

print ("binding: scalar, queried by alias =")
info = cc.get_color_space_info("my_srgb")
print ("  name =", info.name)
print ("  image_state =", info.image_state)
print ("  color_interop_id =", info.color_interop_id)
print ("  encoding =", info.encoding)
print ("  range =", info.range)
print ("  equality_id =", info.equality_id)
print ("  chromaticities =", info.chromaticities)
print ("  transfer_function_kind =", info.transfer_function_kind)
print ("  transfer_function =", info.transfer_function)
print ("  computed(Encoding) =", info.computed(F.Encoding))
print ("  derived(Encoding) =", info.derived(F.Encoding))
print ("  computed(Range) =", info.computed(F.Range))
print ("  available(Range) =", info.available(F.Range))
print ("  computed(EqualityID) =", info.computed(F.EqualityID))
print ("  computed(Chromaticities) =", info.computed(F.Chromaticities))
print ("  computed(TransferFunction) =", info.computed(F.TransferFunction))

print ("binding: data space =")
data = cc.get_color_space_info("rawdata")
print ("  image_state =", data.image_state)
print ("  color_interop_id =", data.color_interop_id)
print ("  encoding =", data.encoding)

print ("binding: batch order and duplicates =")
infos = cc.get_color_space_infos(["plain_space", "rawdata", "plain_space"])
print ("  names =", [i.name for i in infos])

print ("binding: invalid scalar =")
bad = cc.get_color_space_info("no_such_space")
print ("  result =", bad)
print ("  error =", cc.geterror())

print ("binding: invalid batch =")
empty = cc.get_color_space_infos(["plain_space", "nope"])
print ("  result =", empty)
print ("  error =", cc.geterror())

print ("Done.")
