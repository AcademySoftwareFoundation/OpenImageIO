#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Color space search by characterization: the public
# ColorConfig::find_color_spaces API, its `oiiotool --colorspacesearch`
# in-tree consumer, and the Python binding. Refs are self-contained to this
# directory; the results below do not vary by OCIO version.

redirect = " >> out.txt 2>&1 "

acc  = "src/oiio_test_config.ocio"      # full config: chromaticity + transfer axes
core = "src/search_core.ocio"           # small config: universe + encoding/state axes

# --- oiiotool --colorspacesearch: the in-tree consumer, exercising each axis ---

# chromaticity + transfer axes (rec709 gamut, any non-linear transfer)
command += oiiotool ("-echo \"consumer: rec709 gamut, nonlinear transfer =\" "
                     "--colorconfig " + acc + " "
                     "--colorspacesearch:chromaticities=lin_rec709_scene:transfer=~lin_rec709_scene")
# chromaticity + transfer + image-state axes (the sRGB display space)
command += oiiotool ("-echo \"consumer: srgb display-referred =\" "
                     "--colorconfig " + acc + " "
                     "--colorspacesearch:chromaticities=srgb_rec709_display:transfer=srgb_rec709_display:state=display")
# encoding axis, three-valued split (plain / inverse / exclude) + visibility
command += oiiotool ("-echo \"consumer: default universe =\" "
                     "--colorconfig " + core + " --colorspacesearch")
command += oiiotool ("-echo \"consumer: include inactive =\" "
                     "--colorconfig " + core + " --colorspacesearch:inactive=1")
command += oiiotool ("-echo \"consumer: encoding=scene-linear =\" "
                     "--colorconfig " + core + " --colorspacesearch:encoding=scene-linear")
command += oiiotool ("-echo \"consumer: encoding=~scene-linear =\" "
                     "--colorconfig " + core + " --colorspacesearch:encoding=~scene-linear")
command += oiiotool ("-echo \"consumer: encoding=-scene-linear =\" "
                     "--colorconfig " + core + " --colorspacesearch:encoding=-scene-linear")
command += oiiotool ("-echo \"consumer: state=display =\" "
                     "--colorconfig " + core + " --colorspacesearch:state=display")

# --- Python binding: hint coercion, escape grammar, determinism ---
command += pythonbin + " src/test_colorspacesearch.py >> out.txt 2>&1 ;\n"

outputs = [ "out.txt" ]
