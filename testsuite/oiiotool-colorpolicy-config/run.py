#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Spec 09 -- the ambient OCIO config drives OIIO's read color-metadata policy.
# A config author declares policy inside the config via `oiio:` FileRule custom
# keys (OCIO round-trips them byte-stably; other apps ignore them). When such a
# config is the ambient/current one ($OCIO), OIIO's readers honor the declared
# policy on REAL reads -- no OIIO attribute set. A config that declares nothing
# behaves exactly as before (no-surprise). An explicit global attribute still
# overrides the config-declared key (precedence layer 4 > 2).
#
# Proof vehicle: a CICP (1,13,0,1) tuple is state-ambiguous. Its default
# resolution is display-referred; a config declaring read:cicp_state=scene
# flips the SAME file to the scene-referred twin, purely from the config.

redirect = " >> out.txt 2>&1 "

plaincfg = "src/plain.ocio"
declcfg  = "src/decl.ocio"


def read_cs(cfg, label, extra=""):
    # Read cicp.png under the given ambient OCIO config and echo the resolved
    # color space. `env OCIO=<cfg>` switches the ambient config for just this
    # invocation (the whole test is one shell script, so per-call env beats
    # mutating os.environ, which would collapse to its last value).
    return ("env OCIO=" + cfg + " "
            + oiiotool(extra + "cicp.png -echo \"" + label
                       + ": {TOP.'oiio:ColorSpace'}\""))


# Build a CICP (1,13,0,1)-tagged PNG once (ambient config irrelevant here).
command += oiiotool("--create 4x4 3 '--attrib:type=int[4]' CICP 1,13,0,1 "
                    "-o cicp.png")

# (1) No declared policy -> ambiguous CICP resolves display-referred (builtin
# default). Nothing in the config changed OIIO's behavior.
command += read_cs(plaincfg, "plain")

# (2) The config's oiio:default rule declares read:cicp_state=scene -> the SAME
# file resolves scene-referred. The flip comes purely from the ambient config;
# no OIIO attribute is set.
command += read_cs(declcfg, "decl")

# (3) Precedence: an explicit global attribute (layer 4) overrides the
# config-declared default key (layer 2) -> back to display, under the decl
# config (whose oiio:default declares scene).
command += read_cs(declcfg, "override",
                   "--oiioattrib oiio:colorpolicy:read:cicp_state display ")

# (4) Layer 5 (matched file-rule per-file opinion) outranks layer 2: layer5.ocio
# declares the oiio:default baseline as display, but a rule matching *.png as
# scene. Reading a .png resolves scene -- the more specific rule wins.
layer5cfg = "src/layer5.ocio"
command += read_cs(layer5cfg, "matched")

# (5) The documented CSS-specificity footgun: layer 5 beats layer 4. Even with
# an explicit global attribute set to display, the matched .png rule (scene)
# still wins.
command += read_cs(layer5cfg, "matched-vs-global",
                   "--oiioattrib oiio:colorpolicy:read:cicp_state display ")


# --- Write side (spec 09): the ambient config drives write policy too. ---
wdeclcfg = "src/wdecl.ocio"

# An image carrying an explicit CICP tuple. A PNG write emits that tuple by
# default; a config declaring `write:cicp never` suppresses it.
mk = ("--pattern constant:color=0.5,0.5,0.5 16x16 3 "
      "--attrib oiio:ColorSpace srgb_rec709_display "
      "'--attrib:type=int[4]' CICP 1,13,0,1 ")

# (6) --colorwriteplan under the plain config: the cicp tuple is written,
# attributed to the explicit metadata. Under wdecl: suppressed, attributed to
# the config-declared tier -- with NO OIIO attribute set. (Grep to the cicp row
# so the reference is stable.)
command += ("env OCIO=" + plaincfg + " " + oiio_app("oiiotool") + " " + mk
            + "--colorwriteplan png | grep '^  cicp' " + redirect + " ;\n")
command += ("env OCIO=" + wdeclcfg + " " + oiio_app("oiiotool") + " " + mk
            + "--colorwriteplan png | grep '^  cicp' " + redirect + " ;\n")

# (7) Actual writes: emit real PNGs, then read the CICP chunk back. Under the
# plain config the cICP chunk is present in the file; under wdecl it was
# suppressed at write time and is absent. `grep CICP` prints the line when
# present and nothing when absent (|| true so a no-match is not a failure);
# echo labels each so the reference is self-describing.
command += ("env OCIO=" + plaincfg + " " + oiio_app("oiiotool") + " " + mk
            + "-o w_plain.png " + redirect + " ;\n")
command += ("env OCIO=" + wdeclcfg + " " + oiio_app("oiiotool") + " " + mk
            + "-o w_wdecl.png " + redirect + " ;\n")
command += ("echo 'w_plain CICP chunk:' " + redirect + " ;\n")
command += ("( env OCIO=" + plaincfg + " " + oiio_app("oiiotool")
            + " --info -v w_plain.png 2>&1 | grep CICP || true )" + redirect
            + " ;\n")
command += ("echo 'w_wdecl CICP chunk (suppressed by config):' " + redirect
            + " ;\n")
command += ("( env OCIO=" + plaincfg + " " + oiio_app("oiiotool")
            + " --info -v w_wdecl.png 2>&1 | grep CICP || true )" + redirect
            + " ;\n")

outputs = ["out.txt"]
