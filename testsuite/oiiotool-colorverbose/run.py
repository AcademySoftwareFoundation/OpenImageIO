#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Spec 09 Feature 2 -- oiio:colorpolicy:write:verbose. A display space that
# minimally emits just a subset (cICP on PNG, colorInteropID on EXR) emits the
# FULL redundant-but-correct set under the verbose policy. The policy is
# CONFIG-declared in src/verbose.ocio's oiio:default profile -- NO OIIO
# attribute is set. The tagged source is g24_rec709_display: a pure-gamma (2.4)
# rec709 display space, so all of cICP + chromaticities + gamma are derivable
# and consistent.
#
# The harness splits `command` on ';' and runs each fragment through its own
# shell, so note text below carries no ';' and no apostrophe.

redirect = " >> out.txt 2>&1 "
ot  = oiio_app("oiiotool")
cfg = "src/verbose.ocio"
mksrc = "--create 8x8 3 --attrib oiio:ColorSpace g24_rec709_display "


def plan(fmt, note):
    lines = "echo '=== PLAN " + fmt.upper() + " verbose: " + note + " ==='" + redirect + " ;\n"
    lines += ("env OCIO=" + cfg + " " + ot + " " + mksrc + "--colorwriteplan "
              + fmt + " 2>/dev/null | grep -E "
              + "'cicp|chromaticities|gamma|interop_id'" + redirect + " ;\n")
    return lines


def plan_minimal(fmt, note):
    # Same source, NO config -> minimal plan (contrast). Uses the builtin
    # default config for name->id derivation (no OCIO env), so cICP/interop_id
    # still resolve but the redundant signals stay minimal.
    lines = "echo '=== PLAN " + fmt.upper() + " minimal: " + note + " ==='" + redirect + " ;\n"
    lines += (ot + " " + mksrc + "--colorwriteplan " + fmt
              + " 2>/dev/null | grep -E "
              + "'cicp|chromaticities|gamma|interop_id'" + redirect + " ;\n")
    return lines


def written(fmt, note, grep):
    out = "vb." + fmt
    lines = "echo '=== WRITE " + fmt.upper() + " verbose: " + note + " ==='" + redirect + " ;\n"
    lines += ("env OCIO=" + cfg + " " + ot + " " + mksrc + "-o " + out
              + " >/dev/null 2>&1 ;\n")
    lines += ("( env OCIO=" + cfg + " " + ot + " --info -v " + out
              + " 2>/dev/null | grep -E '" + grep + "' || true )"
              + redirect + " ;\n")
    return lines


# Plan proofs: verbose flips chromaticities+gamma (PNG) and chromaticities (EXR)
# from omit/suppress to derive; the minimal plans keep them out.
command += plan_minimal("png", "just cICP")
command += plan("png", "cICP + cHRM + gAMA")
command += plan_minimal("exr", "just colorInteropID")
command += plan("exr", "colorInteropID + chromaticities")

# Actual writes: EXR carries colorInteropID + chromaticities that read back;
# PNG's gAMA reads back as oiio:Gamma (the cHRM chunk is emitted too, verified
# by the plan above -- the PNG reader folds it rather than surfacing it).
command += written("exr", "colorInteropID + chromaticities read back",
                   "colorInteropID|chromaticities")
command += written("png", "gAMA reads back as oiio:Gamma", "oiio:Gamma")

outputs = ["out.txt"]
