#!/usr/bin/env python
# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

import os
from pathlib import Path
import subprocess
import sys
import tempfile

import numpy as np
import OpenImageIO as oiio


BT709 = "ocio:itu709_rec709_scene"
if "--bt709-probe" in sys.argv:
    # Each process isolates debug counts; two wrappers exercise shared reuse.
    path, expected = sys.argv[2:4]
    for _ in range(2):
        config = oiio.ColorConfig(path)
        assert not config.geterror()
        assert config.resolve("Rec709") == "Rec709"
        for _ in range(2):
            assert config.resolve(BT709) == expected
        if expected == "StudioVideo":
            assert config.equivalent("StudioVideo", BT709)
            assert config.get_color_interop_id("StudioVideo") == BT709
    sys.exit(0)


pixels = np.array([[[0.1, 0.5, 0.9]]], dtype=np.float32)

# Generic-name answers captured from OpenImageIO 68dda81c1 (before identity
# recognition): name -> (resolve, getColorSpaceNameByRole,
# get_color_interop_id, isColorSpaceLinear).
NORMALIZATION = {
    "ACEScg": ("ACEScg", None, "", False),
    "Rec709": ("Rec709", None, "", False),
    "lin_ap1_scene": ("lin_ap1_scene", None, "lin_ap1_scene", False),
    "lin_rec709": ("lin_rec709", None, "lin_rec709_scene", False),
    "lin_rec709_scene": ("Reference", "Reference", "lin_rec709_scene", True),
    "lin_srgb": ("lin_srgb", None, "", False),
    "linear": ("linear", "Reference", "", False),
    "sRGB": ("sRGB", None, "", False),
    "scene_linear": ("Reference", "Reference", "lin_rec709_scene", True),
    "srgb_rec709_scene": ("Encoded", "Encoded", "srgb_rec709_scene", False),
}
# A legacy config: generic space names and no interchange roles, so nothing
# can be measured and the names remain the evidence.
LEGACY_TEXT = """ocio_profile_version: 2.3
roles: {default: linear, scene_linear: linear}
file_rules:
  - !<Rule> {name: Default, colorspace: default}
colorspaces:
  - !<ColorSpace>
    name: linear
  - !<ColorSpace>
    name: sRGB
    to_scene_reference: !<ExponentWithLinearTransform> {gamma: 2.4, offset: 0.055, direction: inverse}
  - !<ColorSpace>
    name: rec709
    to_scene_reference: !<ExponentWithLinearTransform> {gamma: 2.22222222222222, offset: 0.099, direction: inverse}
"""
LEGACY = {
    "ACEScg": ("ACEScg", None, "", False),
    "Rec709": ("rec709", "rec709", "", False),
    "lin_ap1_scene": ("lin_ap1_scene", None, "lin_ap1_scene", False),
    "lin_rec709": ("linear", None, "lin_rec709_scene", False),
    "lin_rec709_scene": ("linear", None, "lin_rec709_scene", False),
    "lin_srgb": ("linear", None, "lin_rec709_scene", False),
    "linear": ("linear", "linear", "lin_rec709_scene", True),
    "sRGB": ("sRGB", "sRGB", "srgb_rec709_scene", False),
    "scene_linear": ("linear", "linear", "lin_rec709_scene", True),
    "srgb_rec709_scene": ("sRGB", None, "srgb_rec709_scene", False),
}


def check_answers(path, expected):
    for name, answer in expected.items():
        config = oiio.ColorConfig(str(path))
        assert not config.geterror()
        got = (config.resolve(name), config.getColorSpaceNameByRole(name),
               config.get_color_interop_id(name),
               bool(config.isColorSpaceLinear(name)))
        assert got == answer, (path, name, got, answer)


text = """ocio_profile_version: 2.3
roles: {default: Reference, scene_linear: Reference}
file_rules:
  - !<Rule> {name: Default, colorspace: default}
colorspaces:
  - !<ColorSpace>
    name: Reference
    aliases: [lin_rec709_scene]
  - !<ColorSpace>
    name: Encoded
    aliases: [srgb_rec709_scene]
    to_scene_reference: !<ExponentWithLinearTransform> {gamma: 2.4, offset: 0.055}
"""
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / "normalization.ocio"
    path.write_text(text)
    check_answers(path, NORMALIZATION)
    legacy_path = Path(directory) / "legacy.ocio"
    legacy_path.write_text(LEGACY_TEXT)
    check_answers(legacy_path, LEGACY)
    # getColorSpaceIndex() resolves identities, namespaces and case, so an
    # index request answers for the same spellings resolve() accepts.
    legacy = oiio.ColorConfig(str(legacy_path))
    assert not legacy.geterror()
    srgb_index = legacy.getColorSpaceIndex("sRGB")
    assert srgb_index >= 0, srgb_index
    for spelling in ("srgb_rec709_scene", "acme:sRGB", "SRGB"):
        assert legacy.getColorSpaceIndex(spelling) == srgb_index, spelling
    converted = oiio.ImageBufAlgo.colorconvert(
        oiio.ImageBuf(pixels), "lin_rec709_scene", "srgb_rec709_scene",
        colorconfig=str(path))
    assert not converted.has_error, converted.geterror()
    expected = 1.055 * np.power(pixels, 1 / 2.4) - 0.055
    assert np.allclose(converted.get_pixels(oiio.FLOAT), expected, atol=2e-5)
    assert converted.spec().get_string_attribute("oiio:ColorSpace") == "srgb_rec709_scene"

    # An authored generic alias must keep its actual configured transform.
    authored_path = Path(directory) / "authored.ocio"
    authored_path.write_text(text.replace(
        "aliases: [lin_rec709_scene]",
        "aliases: [lin_rec709_scene, sRGB, linear, Rec709]"))
    authored = oiio.ColorConfig(str(authored_path))
    for name in ("sRGB", "linear", "Rec709"):
        assert authored.resolve(name) == "Reference"
        native = oiio.ImageBufAlgo.colorconvert(
            oiio.ImageBuf(pixels), name, "scene_linear",
            colorconfig=str(authored_path))
        assert not native.has_error, native.geterror()
        assert np.array_equal(native.get_pixels(oiio.FLOAT), pixels)


# Reuse the actual authored reference, renaming only the BT.709 space and
# removing its selectors. All transforms and interchange roles remain intact.
reference = sys.argv[1]
reference_text = Path(reference).read_text()
video_row = ("    name: ocio:itu709_rec709_scene\n"
             "    aliases: [itu709_rec709_scene]\n"
             "    interop_id: ocio:itu709_rec709_scene\n")
assert video_row in reference_text
studio = reference_text.replace(video_row, "    name: StudioVideo\n")
# OCIO before 2.5 cannot parse declared interop metadata.
studio = "\n".join(line for line in studio.splitlines()
                   if not line.startswith("    interop_id:")) + "\n"
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / "studio.ocio"
    path.write_text(studio)
    cold = "Identity bridge cold work: " + BT709
    shared = "Identity bridge shared hit: " + BT709
    for text, expected, count in (
        (studio, "StudioVideo", 1),
        # No correctly encoded BT.709 space: the generic name is not evidence.
        (studio.replace("name: StudioVideo", "name: Rec709").replace(
            "gamma: 2.22222222222222, offset: 0.099",
            "gamma: 2.4, offset: 0.055"), BT709, 1),
        # Direct authored aliases retain their meaning before identification.
        (studio.replace("    name: lin_rec709_scene\n",
                        "    name: lin_rec709_scene\n"
                        "    aliases: [ocio:itu709_rec709_scene]\n"),
         "lin_rec709_scene", 0),
    ):
        path.write_text(text)
        probe = subprocess.run(
            [sys.executable, __file__, "--bt709-probe", str(path), expected],
            check=True, capture_output=True, text=True,
            env=dict(os.environ, OIIO_DEBUG_COLOR="1"))
        assert probe.stdout.count(cold) == count, probe.stdout + probe.stderr
        assert probe.stdout.count(shared) == count, probe.stdout + probe.stderr


# CIF Rec 03: a namespaced name resolves through its remainder, in any case,
# but the remainder matches only names, aliases, named transforms and authored
# IDs: never roles, recognized identities or OIIO's generic names.
studio_builtin = oiio.ColorConfig("ocio://studio-config-latest")
assert studio_builtin.resolve("acme:lin_ap1") == studio_builtin.resolve("lin_ap1")
assert studio_builtin.resolve("acme:lin_ap1") != "acme:lin_ap1"
assert studio_builtin.resolve("acme:ACEScg") == "ACEScg"
for name in ("x:linear", "x:srgb", "x:scene_linear"):
    assert studio_builtin.resolve(name) == name, name
with tempfile.TemporaryDirectory() as directory:
    # No interchange roles, so nothing here can be measured.
    path = Path(directory) / "named.ocio"
    path.write_text(LEGACY_TEXT.replace("roles:", "name: My Legacy\nroles:")
                    + "  - !<ColorSpace> {name: Log Space, aliases: [logc]}\n")
    named = oiio.ColorConfig(str(path))
    assert not named.geterror(), named.geterror()
    for name in ("x:lin_rec709_scene", "x:srgb_rec709_scene", "x:default"):
        assert named.resolve(name) == name, name
    assert named.resolve("X:LOGC") == "Log Space"
    assert named.resolve("x:SRGB") == "sRGB"
    assert named.resolve("my_legacy:local:log_space") == "Log Space"
    # Standard data is the last fallback, also after a namespace.
    for name in ("data", "x:data", "X:Data"):
        assert named.isData(name), name
    # A role names a data space, but not after a namespace.
    path.write_text(LEGACY_TEXT.replace("roles: {", "roles: {plates: raw, ")
                    + "  - !<ColorSpace> {name: raw, isdata: true}\n")
    roles = oiio.ColorConfig(str(path))
    assert not roles.geterror(), roles.geterror()
    assert roles.isData("plates") and roles.isData("x:raw")
    assert not roles.isData("x:plates")

# An authored interop ID matching the whole name wins before the namespace is
# stripped, and a bare ID never selects a space declaring a namespaced one.
if oiio.OpenColorIO_version_hex >= 0x02050000:
    studio_g22 = """ocio_profile_version: 2.5
roles: {default: lin, scene_linear: lin}
file_rules:
  - !<Rule> {name: Default, colorspace: default}
colorspaces:
  - !<ColorSpace> {name: lin, interop_id: lin_rec709_scene}
  - !<ColorSpace> {name: Scan Plates, isdata: true, interop_id: "acme:scans"}
  - !<ColorSpace>
    name: Studio G22
    interop_id: "super_vfx:g22_rec709_scene"
    to_scene_reference: !<ExponentTransform> {value: 2.3, style: pass_thru}
"""
    standard_g22 = """  - !<ColorSpace>
    name: Standard G22
    aliases: [g22_rec709_scene]
    to_scene_reference: !<ExponentTransform> {value: 2.2, style: pass_thru}
"""
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / "nsorder.ocio"
        for text, full, bare in ((studio_g22 + standard_g22, "Studio G22",
                                  "Standard G22"),
                                 (studio_g22, "Studio G22", None)):
            path.write_text(text)
            config = oiio.ColorConfig(str(path))
            assert not config.geterror(), config.geterror()
            assert config.resolve("super_vfx:g22_rec709_scene") == full
            # The cheap query selects by an authored ID, as resolve() does.
            info = config.get_color_space_info("super_vfx:g22_rec709_scene")
            assert info.valid()
            assert config.isData("super_vfx:g22_rec709_scene") is False
            # A stripped remainder may still match an authored ID.
            assert config.resolve("acme:lin_rec709_scene") == "lin"
            assert config.isData("acme:scans")
            assert config.isData("x:acme:scans")
            if bare:
                assert config.resolve("g22_rec709_scene") == bare
            else:
                assert config.resolve("g22_rec709_scene") != full

# ociodisplay from an ID the config does not define tags its output with the
# display and view its processor used.
source_buf = oiio.ImageBuf(oiio.ImageSpec(2, 2, 3, "float"))
default_config = oiio.ColorConfig("ocio://default")
display = default_config.getDefaultDisplayName()
tag = default_config.getDisplayViewColorSpaceName(
    display, default_config.getDefaultViewName(display, "scene_linear"))
assert tag
missing = "oiio:g22_p3d65_display"
assert default_config.resolve(missing) not in default_config.getColorSpaceNames()
shown = oiio.ImageBufAlgo.ociodisplay(source_buf, "", "", missing,
                                      colorconfig="ocio://default")
assert not shown.has_error, shown.geterror()
assert shown.spec().get_string_attribute("oiio:ColorSpace") == tag

# The cross-config display chain keeps alpha exact.
rgba = oiio.ImageBuf(oiio.ImageSpec(4, 1, 4, "float"))
rgba_pixels = np.array([[[0.18, 0.5, 0.9, 0.3], [1.2, 0.02, 0.4, 0.7],
                         [0.001, 0.25, 0.6, 0.05], [0.9, 0.9, 0.1, 0.999]]],
                       dtype=np.float32)
rgba.set_pixels(oiio.ROI(), rgba_pixels)
for unpremult in (False, True):
    shown = oiio.ImageBufAlgo.ociodisplay(rgba, "", "", missing,
                                          unpremult=unpremult,
                                          colorconfig="ocio://default")
    assert np.array_equal(shown.get_pixels(oiio.FLOAT)[..., 3],
                          rgba_pixels[..., 3])

# CICP recognition: transfers 6, 14 and 15 read as 1, primaries 7 as 6, and
# 12,17 as the read-only DCDM identity. g26_p3d65_display writes no CICP, and
# the oiio: Rec.601 (525-line) and PAL (625-line) identities round-trip.
for tuple_, identity in (
    ([1, 6, 0, 1], "g24_rec709_display"),
    ([1, 14, 0, 1], "g24_rec709_display"),
    ([1, 15, 0, 1], "g24_rec709_display"),
    ([7, 1, 0, 1], "oiio:g24_rec601_display"),
    ([7, 6, 0, 1], "oiio:g24_rec601_display"),
    ([12, 17, 0, 1], "dcdm_p3d65_display"),
):
    assert default_config.get_color_interop_id(tuple_) == identity, tuple_
assert default_config.get_cicp("g26_p3d65_display") is None
for identity, tuple_ in (
    ("oiio:g24_rec601_display", [6, 1, 0, 1]),
    ("oiio:lin_rec601_display", [6, 8, 0, 1]),
    ("oiio:g24_rec601pal_display", [5, 1, 0, 1]),
    ("oiio:lin_rec601pal_display", [5, 8, 0, 1]),
):
    assert list(default_config.get_cicp(identity)) == tuple_, identity
    assert default_config.get_color_interop_id(tuple_) == identity, identity
# The linear display identities carry their scene identities' codes, which
# read back as the scene identity.
for identity, tuple_ in (("lin_rec709_display", [1, 8, 1, 1]),
                         ("lin_p3d65_display", [12, 8, 1, 1]),
                         ("lin_rec2020_display", [9, 8, 10, 1])):
    assert list(default_config.get_cicp(identity)) == tuple_, identity
    assert (default_config.get_color_interop_id(tuple_)
            == identity.replace("_display", "_scene")), identity

# A built-in identity this config does not define is described by the
# built-in interop-identities config, named exactly as conversions accept it.
for query in (default_config.get_color_space_info,
              default_config.derive_color_space_info):
    for name, gamma in (("oiio:g24_rec2020_display", 2.4),
                        ("OIIO:G24_REC2020_DISPLAY", 2.4),
                        ("g22_adobergb_display", 2.2),
                        ("oiio:lin_p3dci_display", 1.0)):
        info = query(name)
        assert info.valid(), name
        assert len(info.chromaticities()) == 8, name
        assert abs(info.transfer_function_gamma() - gamma) < 0.001, name
    assert not query("g24_rec2020_display").valid()

# The cheap query resolves as resolve() does without measuring: a stripped
# namespace and OIIO's generic names, through their legacy aliases.
for name in ("acme:ACEScg", "sRGB", "linear"):
    assert default_config.get_color_space_info(name).valid(), name

# isData() looks names up as OpenColorIO does: roles, aliases, any case.
for name in ("Raw", "RAW", *default_config.getAliases("Raw")):
    assert default_config.isData(name), name
assert not default_config.isData("ACEScg")

# A conversion that optimizes to nothing is equivalent: this config's linear
# is a round trip through the interchange space.
test_config = oiio.ColorConfig(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "oiio_test_v0.9.2.ocio"))
for pair in (("linear", "scene_linear"), ("lnf", "lin_rec709")):
    assert test_config.equivalent(*pair), pair

# isColorSpaceActive() answers about the space a name selects, so an identity
# this config both recognizes and lists inactive (rec709_tx) now answers false,
# where the unresolved name answered true. A name nothing here identifies is
# not one of this config's spaces, so it still answers true.
assert not test_config.isColorSpaceActive(BT709)
assert test_config.isColorSpaceActive("g22_adobergb_display")

# Without OCIO, the built-in names answer as they always have.
disabled = subprocess.run(
    [sys.executable, "-c", """
import OpenImageIO as oiio
c = oiio.ColorConfig()
# A linear encoding is honored here too, so writers still tag it gamma 1.0.
# Asked first: a later query that recognizes any name completes this one.
assert c.get_color_space_info("linear").transfer_function_gamma() == 1.0
assert c.resolve("sRGB") == "srgb_rec709_scene", c.resolve("sRGB")
assert c.resolve("srgb") == "srgb_rec709_scene"
assert c.resolve("lin_rec709_scene") == "linear"
assert c.resolve("lin_srgb") == "linear"
assert c.resolve("rec709") == "Rec709"
assert c.resolve("x:linear") == "x:linear"
assert c.get_color_interop_id("Linear") == "lin_rec709_scene"
assert c.get_color_interop_id("Rec709") == ""
"""], capture_output=True, text=True,
    env=dict(os.environ, OIIO_DISABLE_OCIO="1"))
assert disabled.returncode == 0, disabled.stdout + disabled.stderr
