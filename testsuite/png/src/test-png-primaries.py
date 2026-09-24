#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause and Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# What the PNG writer records about a color space it cannot write as cICP:
# cHRM and gAMA where they can carry the primaries and the transfer function,
# gAMA alone where its Rec.709 reading loses nothing (or, with a warning, where
# a linear space's primaries are beyond cHRM), and nothing at all, with a
# warning, where the file cannot carry both. The chunks are parsed from the
# file directly, not through the reader, which does not read cHRM. The warning
# needs OPENIMAGEIO_DEBUG, which would change every other command's output, so
# these cases run out of line with their output captured.

import os
from pathlib import Path
import re
import subprocess
import sys

oiiotool = sys.argv[1]
icc_profile = sys.argv[2]
debug_environment = dict(os.environ, OPENIMAGEIO_DEBUG="1")


def oiio(argv, environment=None):
    result = subprocess.run([oiiotool, *argv], env=environment,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True)
    assert result.returncode == 0, result.stdout
    return result.stdout


def write(colorspace, filename, environment=debug_environment):
    return oiio(["--create", "4x4", "3", "-d", "uint16", "--iscolorspace",
                 colorspace, "-o", filename], environment)


def chunks(filename):
    """The (type, contents) of each chunk of a PNG file, in order."""
    data = Path(filename).read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", filename
    at = 8
    while at + 8 <= len(data):
        length = int.from_bytes(data[at:at + 4], "big")
        yield data[at + 4:at + 8].decode("latin-1"), data[at + 8:at + 8 + length]
        at += 12 + length


def chunk(filename, want):
    """The contents of the named PNG chunk, or None if the file has none."""
    return next((raw for kind, raw in chunks(filename) if kind == want), None)


def color_chunks(filename):
    return {kind for kind, raw in chunks(filename)
            if kind in ("cHRM", "gAMA", "cICP", "sRGB", "iCCP")}


def same_pixels(filename, source):
    assert "PASS" in oiio([filename, source, "--diff"]), (filename, source)


def chromaticities(filename):
    """cHRM as (Rx, Ry, Gx, Gy, Bx, By, Wx, Wy), the OIIO order, or None."""
    raw = chunk(filename, "cHRM")
    if raw is None:
        return None
    xy = [int.from_bytes(raw[i:i + 4], "big") / 100000.0
          for i in range(0, 32, 4)]
    return tuple(xy[2:] + xy[:2])


def gamma(filename):
    """The exponent the gAMA chunk encodes, or None."""
    raw = chunk(filename, "gAMA")
    return None if raw is None else round(1.0 / (
        int.from_bytes(raw, "big") / 100000.0), 2)


# Primaries PNG can write as cICP lose nothing, Rec.709 primaries lose nothing
# to gAMA, and a bare gamma name has no primaries to lose, so none of these
# warn and none needs cHRM.
for colorspace, filename in (("lin_p3d65_scene", "quiet-p3d65.png"),
                             ("g22_rec709_display", "quiet-g22.png"),
                             ("Gamma 2.2", "quiet-gamma.png")):
    quiet = write(colorspace, filename)
    assert "WARNING" not in quiet, quiet

# Whether this libPNG can write cICP at all. Without it every color space
# falls back to cHRM, which carries the same primaries.
cicp_supported = "CICP: 12, 8" in oiio(["--info", "-v", "quiet-p3d65.png"])
for filename in ("quiet-p3d65.png", "quiet-g22.png", "quiet-gamma.png"):
    assert ((chromaticities(filename) is None)
            == (cicp_supported or filename != "quiet-p3d65.png")), filename

# The same holds where the derived properties are not retained process-wide,
# as on this small config, whose "linear" has exactly Rec.709 primaries.
Path("rec709-linear.ocio").write_text("""ocio_profile_version: 2.3
roles: {default: ref, scene_linear: ref, aces_interchange: ref, cie_xyz_d65_interchange: xyzd65}
file_rules:
  - !<Rule> {name: Default, colorspace: default}
colorspaces:
  - !<ColorSpace>
    name: ref
  - !<ColorSpace>
    name: xyzd65
    to_display_reference: !<MatrixTransform> {matrix: [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1]}
""")
quiet = write("linear", "quiet-linear.png",
              dict(debug_environment,
                   OCIO=str(Path("rec709-linear.ocio").resolve())))
assert "WARNING" not in quiet, quiet
assert chromaticities("quiet-linear.png") is None, "quiet-linear.png"

# A pure power only a configured space's own properties give is not a Rec.709
# gamma, and gAMA alone would read back as one, so without primaries to write
# beside it (Odd235's are unavailable) the file carries no color chunk at all.
Path("odd.ocio").write_text("""ocio_profile_version: 2.3
roles: {default: ACES, scene_linear: ACES, aces_interchange: ACES, cie_xyz_d65_interchange: XYZ}
file_rules:
  - !<Rule> {name: Default, colorspace: default}
colorspaces:
  - !<ColorSpace>
    name: ACES
    encoding: scene-linear
  - !<ColorSpace>
    name: Odd235
    encoding: sdr-video
    to_scene_reference: !<GroupTransform>
      children:
        - !<ExponentTransform> {value: 2.35}
        - !<MatrixTransform> {matrix: [0.4395770431186348, 0.3839148953677938, 0.1765080615135714, 0, 0.08960303383941091, 0.8147638539946282, 0.09563311216596088, 0, 0.01741197617524438, 0.1087181258178095, 0.8738698980069461, 0, 0, 0, 0, 1]}
display_colorspaces:
  - !<ColorSpace>
    name: XYZ
    encoding: display-linear
""")
odd_environment = dict(debug_environment, OCIO=str(Path("odd.ocio").resolve()))
warning = write("Odd235", "odd235.png", odd_environment)
assert ('WARNING: PNG cannot represent the primaries of "Odd235"; the file '
        'carries no color chunk at all' in warning), warning
assert color_chunks("odd235.png") == set(), color_chunks("odd235.png")

# A supplied ICC profile describes the primaries, so no cICP is inferred.
oiio(["--create", "4x4", "3", "-d", "uint16", "--iscolorspace",
      "lin_p3d65_scene", "--iccread", icc_profile, "-o", "icc-p3d65.png"])
icc_info = oiio(["--info", "-v", "icc-p3d65.png"])
assert "ICCProfile" in icc_info and "CICP" not in icc_info, icc_info

# A profile supplied for a space cICP cannot carry suppresses cHRM as well:
# the profile states the primaries, and a cHRM disagreeing with it would be
# what a decoder that skips the profile reads.
oiio(["--create", "4x4", "3", "-d", "uint16", "--iscolorspace",
      "lin_ap1_scene", "--iccread", icc_profile, "-o", "icc-ap1.png"])
icc_info = oiio(["--info", "-v", "icc-ap1.png"])
assert "ICCProfile" in icc_info and "CICP" not in icc_info, icc_info
assert chromaticities("icc-ap1.png") is None, "icc-ap1.png"
assert gamma("icc-ap1.png") == 1.0, gamma("icc-ap1.png")

# ACES AP0's blue y is negative, which is outside the xy triangle cHRM can
# hold, so those primaries really are lost and the writer says so.
warning = write("lin_ap0_scene", "warn-ap0.png")
readback = oiio(["--info", "-v", "warn-ap0.png"])
assert 'oiio:ColorSpace: "lin_rec709_scene"' in readback, readback
assert chromaticities("warn-ap0.png") is None, "warn-ap0.png"
expected = ('WARNING: PNG cannot represent the primaries of "lin_ap0_scene"; '
            'the file carries gAMA alone and reads back as Rec.709')
assert expected in warning, warning
assert gamma("warn-ap0.png") == 1.0, gamma("warn-ap0.png")

# A camera log encoding has no exponent for gAMA either, so the same file
# carries no color chunk at all, and the warning says so.
warning = write("ocio:slog3_sgamut3_scene", "warn-slog3.png")
expected = ('WARNING: PNG cannot represent the primaries of '
            '"ocio:slog3_sgamut3_scene"; the file carries no color chunk at '
            'all and reads back as Rec.709')
assert expected in warning, warning
assert chromaticities("warn-slog3.png") is None, "warn-slog3.png"
assert gamma("warn-slog3.png") is None, gamma("warn-slog3.png")

# ACEScg's are not, so it keeps them where libPNG accepts them. libPNG older
# than 1.6.44 refuses some that cHRM can hold, AP1's red (x + y above 1) among
# them, and the file then carries gAMA 1.0 alone, as for AP0, and says so.
output = write("lin_ap1_scene", "ap1.png")
ap1_accepted = chromaticities("ap1.png") is not None
if ap1_accepted:
    assert "WARNING" not in output, output
    assert chromaticities("ap1.png") == (0.713, 0.293, 0.165, 0.83, 0.128,
                                         0.044, 0.32168, 0.33767), "ap1.png"
else:
    assert ('WARNING: PNG cannot represent the primaries of "lin_ap1_scene"; '
            'the file carries gAMA alone' in output), output
# Every libPNG from 1.6.44 on accepts them.
libpng = re.search(r"(?:^|;)png:libpng (\d+)\.(\d+)\.(\d+)",
                   oiio(["--echo", "{getattribute(library_list)}"]))
assert ap1_accepted or (libpng and tuple(int(v) for v in libpng.groups())
                        < (1, 6, 44)), libpng
assert gamma("ap1.png") == 1.0, "ap1.png"

# A built-in identity the config (ocio://default) does not define is described
# by the built-in interop-identities config, also after a copy through
# OpenEXR, and keeps its primaries and its transfer function: as cICP where it
# has a code point, otherwise as cHRM plus gAMA.
p3dci = (0.68, 0.32, 0.265, 0.69, 0.15, 0.06, 0.314, 0.351)
p3d65 = (0.68, 0.32, 0.265, 0.69, 0.15, 0.06, 0.3127, 0.329)
rec2020 = (0.708, 0.292, 0.17, 0.797, 0.131, 0.046, 0.3127, 0.329)
for colorspace, xy, exponent in (
        ("lin_p3d65_display", p3d65, 1.0),
        ("oiio:lin_p3dci_display", p3dci, 1.0),
        ("oiio:g24_rec2020_display", rec2020, 2.4),
        ("oiio:g22_p3d65_display", p3d65, 2.2)):
    name = colorspace.replace(":", "-")
    oiio(["--create", "4x4", "3", "-d", "half", "--attrib", "oiio:ColorSpace",
          colorspace, "-o", name + ".exr"])
    output = oiio([name + ".exr", "-d", "uint16", "-o", name + ".png"],
                  debug_environment)
    assert "WARNING" not in output, output
    png = name + ".png"
    if cicp_supported and colorspace == "lin_p3d65_display":
        # The only one of the four with a CICP code point.
        assert "CICP: 12, 8, 0, 1" in oiio(["--info", "-v", png]), png
        assert chromaticities(png) is None, png
    else:
        assert chromaticities(png) == xy, (png, chromaticities(png))
    assert gamma(png) == exponent, (png, gamma(png))

# A linear encoding is written as gamma 1.0 even on a configuration too thin
# to measure, where only the color interop ID says the encoding is linear.
Path("thin.ocio").write_text("""ocio_profile_version: 2.1
roles: {default: ACEScg}
file_rules:
  - !<Rule> {name: Default, colorspace: default}
colorspaces:
  - !<ColorSpace> {name: ACEScg}
  - !<ColorSpace> {name: Lin}
""")
thin = dict(debug_environment, OCIO=str(Path("thin.ocio").resolve()))
write("ACEScg", "thin-acescg.png", thin)
assert gamma("thin-acescg.png") == 1.0, gamma("thin-acescg.png")
assert 'oiio:ColorSpace: "lin_rec709_scene"' in oiio(
    ["--info", "-v", "thin-acescg.png"])
# A color space the configuration says nothing about still gets no gamma.
write("Lin", "thin-lin.png", thin)
assert gamma("thin-lin.png") is None, gamma("thin-lin.png")

# A non-linear identity is not swept up in that: it keeps its own exponent.
write("g22_adobergb_display", "adobergb.png")
# Exactly Adobe RGB's 563/256, which the built-in identity describes, at gAMA's
# precision, not a rounded 2.2.
assert chunk("adobergb.png", "gAMA") == round(100000 * 256 / 563).to_bytes(
    4, "big"), gamma("adobergb.png")
assert chromaticities("adobergb.png") == (0.64, 0.33, 0.21, 0.71, 0.15, 0.06,
                                          0.3127, 0.329), "adobergb.png"

# OIIO_DISABLE_BUILTIN_OCIO_CONFIGS also disables the built-in
# interop-identities config: an identity only it describes publishes no
# colorInteropID, and the PNG writer has no primaries to record or to lose.
for flag, expected in (("0", True), ("1", False)):
    environment = dict(os.environ, OCIO="ocio://default",
                       OIIO_DISABLE_BUILTIN_OCIO_CONFIGS=flag)
    oiio(["--create", "4x4", "3", "-d", "half", "--attrib", "oiio:ColorSpace",
          "oiio:lin_p3dci_display", "-o", "disabled.exr"], environment)
    info = oiio(["--info", "-v", "disabled.exr"])
    assert ('colorInteropID: "oiio:lin_p3dci_display"' in info) == expected, info
    warning = write("lin_ap0_scene", "disabled.png",
                    dict(environment, OPENIMAGEIO_DEBUG="1"))
    assert ("WARNING" in warning) == expected, warning

# A representable gamut on a transfer function neither linear nor a pure power
# gets no color chunk at all: cHRM is written only beside a transfer function
# the file also carries. Pixels are written as they are.
oiio(["--pattern", "fill:top=0.1,0.5,0.9:bottom=0.9,0.5,0.1", "4x4", "3",
      "-d", "uint16", "-o", "source3.tif"])
default_config = dict(debug_environment, OCIO="ocio://default")
names = oiio(["--colorconfiginfo"], default_config)
for colorspace in ("ACEScc", "ACEScct",
                   "sRGB Encoded AP1" if '"sRGB Encoded AP1"' in names
                   else "sRGB Encoded AP1 - Texture"):
    png = colorspace.replace(" ", "-") + ".png"
    warning = oiio(["source3.tif", "--iscolorspace", colorspace, "-o", png],
                   default_config)
    expected = ('OpenImageIO WARNING: PNG cannot represent the transfer '
                'function of "' + colorspace + '", so the file carries no '
                'color chunk')
    assert warning.count("WARNING") == 1 and expected in warning, warning
    assert color_chunks(png) == set(), (png, color_chunks(png))
    same_pixels(png, "source3.tif")

# A gamma a legacy "Gamma" name gives is written beside the space's cHRM, and
# where libPNG refuses those primaries, not alone either, since gAMA alone
# would read back as Rec.709: the file carries no color chunk and says so.
warning = oiio(["source3.tif", "--iscolorspace", "Gamma 2.2 AP1 - Texture",
                "-o", "g22-ap1.png"], default_config)
if ap1_accepted:
    assert "WARNING" not in warning, warning
    assert chromaticities("g22-ap1.png") == chromaticities("ap1.png")
    assert gamma("g22-ap1.png") == 2.2, gamma("g22-ap1.png")
else:
    assert ('WARNING: PNG cannot represent the primaries of "Gamma 2.2 AP1 - '
            'Texture"; the file carries no color chunk at all' in warning), \
        warning
    assert color_chunks("g22-ap1.png") == set(), color_chunks("g22-ap1.png")
same_pixels("g22-ap1.png", "source3.tif")

# The same holds for a built-in identity whose primaries the built-in config
# does not state, whether it is a pure power (gAMA alone would claim Rec.709)
# or not, while a Rec.709 gamma keeps gAMA alone and says nothing. A data
# space states no encoding, so there is nothing to warn about.
for colorspace, lost, chunkless in (
        ("oiio:g22_p3d50_display", "primaries", True),
        ("oiio:g22_adobergbd50_display", "primaries", True),
        ("oiio:g18_prophoto_display", "primaries", True),
        ("dcdm_p3d65_display", "transfer function", True),
        ("oiio:pq_rec709_display", "transfer function", True),
        ("g24_rec709_display", None, False),
        ("data", None, True)):
    png = colorspace.replace(":", "-") + ".png"
    warning = oiio(["source3.tif", "--iscolorspace", colorspace, "-o", png],
                   default_config)
    if lost is None:
        assert "WARNING" not in warning, (png, warning)
    else:
        expected = ('OpenImageIO WARNING: PNG cannot represent the ' + lost
                    + ' of "' + colorspace + '"')
        assert warning.count("WARNING") == 1 and expected in warning, warning
    assert color_chunks(png) == (set() if chunkless else {"gAMA"}), (
        png, color_chunks(png))
    same_pixels(png, "source3.tif")

# P3-D65 on a pure gamma 2.6 is written as cHRM plus gAMA, in color and in
# grey, with pixels written as they are.
p3d65_wire = (68000, 32000, 26500, 69000, 15000, 6000, 31270, 32900)
oiio(["--pattern", "fill:top=0.1:bottom=0.9", "4x4", "1", "-d", "uint16",
      "-o", "source1.tif"])
for channels in (3, 1):
    png = "g26-p3d65-%d.png" % channels
    source = "source%d.tif" % channels
    oiio([source, "--iscolorspace", "g26_p3d65_display", "-o", png])
    assert color_chunks(png) == {"cHRM", "gAMA"}, (png, color_chunks(png))
    assert chromaticities(png) == tuple(v / 100000.0 for v in p3d65_wire), png
    assert chunk(png, "gAMA") == (38462).to_bytes(4, "big"), png
    same_pixels(png, source)
