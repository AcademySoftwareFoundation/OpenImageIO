#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO



############################################################################
# This file contains code examples from the ImageIO API chapter of the
# main OpenImageIO documentation.
#
# To add an additional test, replicate the section below. Change
# "example1" to a helpful short name that identifies the example.


# BEGIN-imageioapi-example1
import OpenImageIO as oiio
import numpy as np

def example1() -> None:
    #
    # Example code fragment from the docs goes here.
    #
    # It probably should generate either some text output (which will show up
    # in "out.txt" that captures each test's output), or it should produce a
    # (small) image file that can be compared against a reference image that
    # goes in the ref/ subdirectory of this test.
    #
    return

# END-imageioapi-example1


def example_colorspaceinfo() -> None:
    print("example_colorspaceinfo")
    # BEGIN-imageioapi-colorspaceinfo
    config = oiio.ColorConfig.default_colorconfig()
    for name in ("ACEScg", "g22_rec709_scene", "srgb_rec709_scene"):
        # Derive whatever properties the config does not declare
        info = config.derive_color_space_info(name)
        print(f"{name}: gamma {info.transfer_function_gamma():.3g}, chromaticities",
              *(f"{xy:.3f}" for xy in info.chromaticities()))
    # END-imageioapi-colorspaceinfo

#
############################################################################





if __name__ == '__main__':
    print("docs-examples-imageioapi.py")
    # Each example function needs to get called here, or it won't execute
    # as part of the test.
    example1()
    example_colorspaceinfo()
