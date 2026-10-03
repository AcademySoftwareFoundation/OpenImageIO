#!/usr/bin/env python
# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

from pathlib import Path
import sys
import tempfile

import numpy as np
import OpenImageIO as oiio


reference = Path(sys.argv[1])
_compat_dir = tempfile.TemporaryDirectory()
config_path = reference
if oiio.OpenColorIO_version_hex < 0x02050000:
    config_path = Path(_compat_dir.name) / reference.name
    config_path.write_text("\n".join(
        line for line in reference.read_text().splitlines()
        if not line.startswith("    interop_id:")) + "\n")
config = oiio.ColorConfig(str(config_path))
assert not config.geterror()
info = config.derive_color_space_info("lin_ap1_scene")
assert info.valid()
assert np.allclose(info.chromaticities(),
                   [.713, .293, .165, .830, .128, .044, .32168, .33767],
                   atol=1e-5)
assert info.transfer_function_gamma() == 1
empty = oiio.ColorSpaceInfo()
assert not empty.valid()
assert not empty.chromaticities()
assert empty.transfer_function_gamma() == 0
assert not config.derive_color_space_info("not a color space").valid()

# The Python object owns its immutable snapshot after the wrapper is gone.
expected = (info.chromaticities(), info.transfer_function_gamma())
del config
assert (info.chromaticities(), info.transfer_function_gamma()) == expected

# The cheap query reports a bridge any wrapper of the same config retained, so
# its answer does not depend on which wrapper derived first.
# cg-config v2.2 (OCIO 2.4+) names no legacy selector, only the bridge does.
if oiio.OpenColorIO_version_hex >= 0x02040000:
    builtin = "ocio://cg-config-v2.2.0_aces-v1.3_ocio-v2.4"
    for wrapper in (oiio.ColorConfig(builtin), oiio.ColorConfig(builtin)):
        for name in wrapper.getColorSpaceNames():
            wrapper.get_color_interop_id(name)
    assert wrapper.get_color_space_info("srgb_rec709_display").valid()

# An is-unique space is never named by comparison with a reference definition,
# but its separately measured curve and primaries still identify it.
to_rec709 = ("!<MatrixTransform> {matrix: [2.52168618674388, -1.13413098823972,"
             " -0.387555198504164, 0, -0.276479914229922, 1.37271908766826,"
             " -0.096239173438334, 0, -0.0153780649660342, -0.152975335867399,"
             " 1.16835340083343, 0, 0, 0, 0, 1]}")
srgb_curve = ("!<ExponentWithLinearTransform> {gamma: 2.4, offset: 0.055,"
              " direction: inverse}")
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / "unique.ocio"
    path.write_text(f'''ocio_profile_version: 2.3
roles: {{aces_interchange: ACES2065-1, scene_linear: ACES2065-1, default: ACES2065-1}}
file_rules:
  - !<Rule> {{name: Default, colorspace: default}}
colorspaces:
  - !<ColorSpace> {{name: ACES2065-1, encoding: scene-linear}}
  - !<ColorSpace>
    name: UniqueSRGB
    categories: [is-unique]
    from_scene_reference: !<GroupTransform> {{children: [{to_rec709}, {srgb_curve}]}}
  - !<ColorSpace>
    name: UniqueLinear
    categories: [is-unique]
    from_scene_reference: {to_rec709}
''')
    unique = oiio.ColorConfig(str(path))
    assert not unique.geterror()
    assert unique.get_color_interop_id("UniqueSRGB") == "srgb_rec709_scene"
    linear_info = unique.derive_color_space_info("UniqueLinear")
    assert len(linear_info.chromaticities()) == 8
    assert linear_info.transfer_function_gamma() == 1

# A pure power reports its exponent whether its identity was measured or
# declared: a declaration takes it from the identity's built-in definition.
reference_config = oiio.ColorConfig(str(config_path))
declared = [(reference_config, ("g18_rec709_scene", "g22_ap1_scene", "g24_rec709_scene"))]
if oiio.OpenColorIO_version_hex >= 0x02050000:
    studio = oiio.ColorConfig("ocio://studio-config-v4.0.0_aces-v2.0_ocio-v2.5")
    assert not studio.geterror()
    declared.append((studio, ("Gamma 1.8 Encoded Rec.709",
                              "Gamma 2.2 Encoded AP1",
                              "Gamma 2.4 Encoded Rec.709")))
for power_config, names in declared:
    for name, gamma in zip(names, (1.8, 2.2, 2.4)):
        power = power_config.derive_color_space_info(name)
        assert abs(power.transfer_function_gamma() - gamma) < 1e-6, \
            (name, power.transfer_function_gamma())
# Scaled before its power, DCDM is no pure power, declared or not.
dcdm = reference_config.derive_color_space_info("dcdm_p3d65_display")
assert dcdm.transfer_function_gamma() == 0, dcdm.transfer_function_gamma()

# A definition with a 3D LUT is not measured, even behind a FileTransform, so
# an identity 3D LUT is not identified as its reference space.
with tempfile.TemporaryDirectory() as directory:
    (Path(directory) / "identity3d.cube").write_text(
        "LUT_3D_SIZE 2\n0 0 0\n1 0 0\n0 1 0\n1 1 0\n"
        "0 0 1\n1 0 1\n0 1 1\n1 1 1\n")
    path = Path(directory) / "lut3d.ocio"
    path.write_text('''ocio_profile_version: 2.3
search_path: .
roles: {default: ACES2065-1, scene_linear: ACES2065-1, aces_interchange: ACES2065-1}
file_rules:
  - !<Rule> {name: Default, colorspace: default}
colorspaces:
  - !<ColorSpace> {name: ACES2065-1, encoding: scene-linear}
  - !<ColorSpace>
    name: Lut3DPlate
    to_scene_reference: !<FileTransform> {src: identity3d.cube, interpolation: linear}
''')
    lut3d = oiio.ColorConfig(str(path))
    assert not lut3d.geterror(), lut3d.geterror()
    assert lut3d.get_color_interop_id("Lut3DPlate") == "", \
        lut3d.get_color_interop_id("Lut3DPlate")
    assert lut3d.derive_color_space_info("Lut3DPlate").transfer_function_gamma() == 0
