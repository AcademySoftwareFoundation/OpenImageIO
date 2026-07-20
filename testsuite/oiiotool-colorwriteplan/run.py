#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# oiiotool --colorwriteplan: a dry-run preview of the color-metadata write
# plan -- what OIIO would write for the top image to a given format, and
# which layer (builtin default / global attribute / per-spec attribute /
# explicit metadata / format incapability) decided each signal. No file is
# written. Every vector below uses explicit metadata, policy attributes, or
# an unresolvable color space name, so no name->id derivation against an
# ambient OCIO config is exercised and the output is config- and
# OCIO-version-independent.

redirect = " >> out.txt 2>&1 "

# (1) Default plan, no usable color metadata: EXR (interop_id capable,
# nothing determinable) and PNG (cicp capable, nothing determinable);
# everything else is format-incapable.
command += oiiotool ("--pattern constant:color=0.5,0.5,0.5 16x16 3 "
                     "--attrib oiio:ColorSpace not-a-real-space-xyzzy "
                     "--colorwriteplan exr "
                     "--colorwriteplan png")

# (2) Explicit-CICP passthrough: the author's tuple is written verbatim,
# attributed to the explicit metadata.
command += oiiotool ("--pattern constant:color=0.5,0.5,0.5 16x16 3 "
                     "--attrib oiio:ColorSpace not-a-real-space-xyzzy "
                     "'--attrib:type=int[4]' CICP 1,13,0,1 "
                     "--colorwriteplan png")

# (3) A global 'never' suppresses the same explicit tuple, attributed to the
# global attribute tier.
command += oiiotool ("--oiioattrib oiio:colorpolicy:write:cicp never "
                     "--pattern constant:color=0.5,0.5,0.5 16x16 3 "
                     "--attrib oiio:ColorSpace not-a-real-space-xyzzy "
                     "'--attrib:type=int[4]' CICP 1,13,0,1 "
                     "--colorwriteplan png")

# (4) A per-spec hint outranks the global 'never': back to writing, and a
# per-spec 'never' is attributed to the per-spec tier.
command += oiiotool ("--oiioattrib oiio:colorpolicy:write:cicp never "
                     "--pattern constant:color=0.5,0.5,0.5 16x16 3 "
                     "--attrib oiio:ColorSpace not-a-real-space-xyzzy "
                     "'--attrib:type=int[4]' CICP 1,13,0,1 "
                     "--attrib oiio:colorpolicy:write:cicp auto "
                     "--colorwriteplan png "
                     "--attrib oiio:colorpolicy:write:cicp never "
                     "--colorwriteplan png")

# (5) An author-supplied colorInteropID on EXR is written verbatim.
command += oiiotool ("--pattern constant:color=0.5,0.5,0.5 16x16 3 "
                     "--attrib oiio:ColorSpace not-a-real-space-xyzzy "
                     "--attrib colorInteropID lin_adobergb_scene "
                     "--colorwriteplan exr")

outputs = [ "out.txt" ]
