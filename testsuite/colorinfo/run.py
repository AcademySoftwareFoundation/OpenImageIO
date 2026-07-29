#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# The public cheap characterization surface: `oiiotool --colorinfo` (the
# in-tree consumer of ColorConfig::get_color_space_info) and the Python
# binding. The config is self-contained to this directory and identifies its
# interop-id-named space through the static table (no interop_id attribute),
# so the output does not vary by OCIO version. Everything here exercises the
# CHEAP contract only: no field is derived, so the derivable fields print as
# uncomputed.

redirect = " >> out.txt 2>&1 "

cfg = "src/colorinfo.ocio"

# A batch of named spaces (order preserved; an alias reports its canonical
# name): a table-identified scene space, a data space, a display space, and
# a space with no authored facts.
command += oiiotool ("-echo \"consumer: named spaces =\" "
                     "--colorconfig " + cfg + " "
                     "--colorinfo my_srgb,rawdata,screen,plain_space")

# An empty list means the current top image's color space.
command += oiiotool ("-echo \"consumer: current image =\" "
                     "--colorconfig " + cfg + " "
                     "--pattern constant:color=0.5,0.5,0.5 16x16 3 "
                     "--iscolorspace my_srgb "
                     "--colorinfo \"\"")

# An unknown name fails the whole batch with one indexed error.
command += oiiotool ("-echo \"consumer: invalid name =\" "
                     "--colorconfig " + cfg + " "
                     "--colorinfo plain_space,nope_xyzzy", failureok = 1)

# --- Python binding: None-for-unavailable, batch, error convention ---
command += pythonbin + " src/test_colorinfo.py >> out.txt 2>&1 ;\n"

# --- Python binding: ColorConfig.get_builtin_interop_ids() ---
command += pythonbin + " src/test_color_interop_ids.py >> out.txt 2>&1 ;\n"

outputs = [ "out.txt" ]
