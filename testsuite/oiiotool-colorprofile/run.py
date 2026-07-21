#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Spec 09 Feature 3 -- composable +/- layer-3 profile selection. Active profiles
# are selected from TWO entry points that compose: the env var
# OPENIMAGEIO_COLORPOLICY (the base) and the global attribute
# oiio:colorpolicy:profile (composed on top, so it can subtract what the env var
# added). Each entry is [+|-]<target>, where <target> is a whole profile name
# (a config rule name, e.g. oiio:blender:textures) or a single policy key
# (read:cicp_state[=value]).
#
# Proof vehicle (as in oiiotool-colorpolicy-config): a state-ambiguous CICP
# (1,13,0,1) tuple resolves display-referred by default; the profile
# oiio:blender:textures declares read:cicp_state=scene, which flips the SAME
# file to the scene-referred twin. So the resolved color space reports which
# policy keys are active after selection.
#
# The harness splits `command` on ';' and runs each fragment through its own
# shell, so note text carries no ';' and no apostrophe.

redirect = " >> out.txt 2>&1 "
ot  = oiio_app("oiiotool")
cfg = "src/profiles.ocio"


def show(label, env="", attr=""):
    # Read cicp.png under the profiles config with an optional env-var selection
    # and/or an oiio:colorpolicy:profile attribute selection, and echo the
    # resolved color space.
    line = "echo '=== " + label + " ==='" + redirect + " ;\n"
    prefix = "env OCIO=" + cfg + " "
    if env:
        prefix += "OPENIMAGEIO_COLORPOLICY='" + env + "' "
    attrarg = ""
    if attr:
        attrarg = "--oiioattrib oiio:colorpolicy:profile '" + attr + "' "
    line += (prefix + ot + " " + attrarg + "cicp.png -echo "
             + "\"  {TOP.'oiio:ColorSpace'}\"" + redirect + " ;\n")
    return line


# Build the CICP-tagged PNG once (ambient config irrelevant here).
command += oiiotool("--create 4x4 3 '--attrib:type=int[4]' CICP 1,13,0,1 "
                    "-o cicp.png")

command += show("baseline: no selection -> display")
command += show("env selects profile -> scene",
                env="oiio:blender:textures")
command += show("profile then -key drops the key -> display",
                env="oiio:blender:textures,-read:cicp_state")
command += show("+key=value sets a key directly -> scene",
                env="+read:cicp_state=scene")
command += show("attribute subtracts env-added profile -> display",
                env="oiio:blender:textures", attr="-oiio:blender:textures")
command += show("attribute alone selects profile -> scene",
                attr="oiio:blender:textures")

outputs = ["out.txt"]
