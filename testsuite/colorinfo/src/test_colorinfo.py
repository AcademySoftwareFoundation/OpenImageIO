#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Python binding of the characterization surface:
# ColorConfig.get_color_space_info / get_color_space_infos (cheap) and
# derive_color_space_info / derive_color_space_infos (full derivation), and
# the ColorSpaceInfo record. Unavailable fields are None (never empty
# strings); invalid scalar input is None with the error on the config;
# invalid batch input is [] with one indexed error. The config here has no
# aces_interchange role, so behavioral probes cannot run and every derive
# outcome below is deterministic (table association or a stable negative).

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

# --- The derive verbs: every field attempted; a field the config cannot
# characterize is a stable negative (computed True, available False, value
# None), not an error. The table-identified space associates its reserved
# chromaticities without a probe.
print ("derive: scalar, complete record =")
full = cc.derive_color_space_info("my_srgb")
print ("  name =", full.name)
print ("  color_interop_id =", full.color_interop_id)
print ("  chromaticities =",
       None if full.chromaticities is None
       else tuple(round(c, 4) for c in full.chromaticities))
print ("  derived(Chromaticities) =", full.derived(F.Chromaticities))
print ("  computed(TransferFunction) =", full.computed(F.TransferFunction))
print ("  available(TransferFunction) =", full.available(F.TransferFunction))
print ("  transfer_function =", full.transfer_function)
print ("  computed(Range) =", full.computed(F.Range))
print ("  available(Range) =", full.available(F.Range))
print ("  range =", full.range)

print ("derive: cheap getter now sees the cached facts =")
seen = cc.get_color_space_info("my_srgb")
print ("  chromaticities =",
       None if seen.chromaticities is None
       else tuple(round(c, 4) for c in seen.chromaticities))
print ("  computed(TransferFunction) =", seen.computed(F.TransferFunction))

print ("derive: batch order and duplicates =")
infos = cc.derive_color_space_infos(["plain_space", "rawdata", "plain_space"])
print ("  names =", [i.name for i in infos])

print ("derive: invalid scalar =")
bad = cc.derive_color_space_info("no_such_space")
print ("  result =", bad)
print ("  error =", cc.geterror())

print ("derive: invalid batch =")
empty = cc.derive_color_space_infos(["plain_space", "nope"])
print ("  result =", empty)
print ("  error =", cc.geterror())

print ("Done.")
