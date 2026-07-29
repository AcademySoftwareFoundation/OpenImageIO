#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Python binding of ColorConfig.get_builtin_interop_ids(): the canonical
# Color Interop Forum ids declared by OIIO's built-in interop identities
# registry (the same data as the C++ static method of the same name).
# Assertions are printed as properties rather than as the id list itself, so
# this reference does not have to be regenerated every time the registry
# gains an identity.

import OpenImageIO as oiio

ids = oiio.ColorConfig.get_builtin_interop_ids()

print("get_builtin_interop_ids: shape")
print("  is a tuple           =", isinstance(ids, tuple))
print("  non-empty            =", len(ids) > 0)
print("  all str              =", all(isinstance(i, str) for i in ids))

# Canonical form: the registry's own ids are lowercase, non-empty, and carry
# no whitespace. (The id grammar as a whole is open -- custom:*, icc:*, and
# <config>:local:* ids cannot be enumerated -- but none of those are
# registry entries.)
print("  all canonical form   =",
      all(i and i == i.lower() and not any(c.isspace() for c in i)
          for i in ids))
print("  unique               =", len(set(ids)) == len(ids))
print("  sorted               =", list(ids) == sorted(ids))

# Process-lifetime data: a repeated call yields the same ids.
print("  stable across calls  =",
      oiio.ColorConfig.get_builtin_interop_ids() == ids)

# Spec-mandated members that the registry must always declare.
print("get_builtin_interop_ids: known members")
for known in ("data", "lin_ap0_scene", "srgb_rec709_scene",
              "srgb_rec709_display"):
    print("  {:<20} =".format(known), known in ids)

# Cross-check against what the C++ side actually hands out: every id
# get_color_interop_id() returns for a registry-identified space must be a
# member of this tuple.
cc = oiio.ColorConfig("src/colorinfo.ocio")
print("get_builtin_interop_ids: agree with get_color_interop_id")
print("  reachable via instance =", cc.get_builtin_interop_ids() == ids)
for space in ("my_srgb", "rawdata"):
    got = cc.get_color_interop_id(space)
    print("  {:<10} -> {:<20} member = {}".format(space, got, got in ids))

print("Done.")
