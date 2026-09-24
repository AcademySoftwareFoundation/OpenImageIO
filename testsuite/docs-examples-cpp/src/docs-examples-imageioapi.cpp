// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


///////////////////////////////////////////////////////////////////////////
// This file contains code examples from the ImageIO API chapter of the
// main OpenImageIO documentation.
//
// To add an additional test, replicate the section below. Change
// "example1" to a helpful short name that identifies the example.

// BEGIN-imageioapi-example1
#include <OpenImageIO/imageio.h>
using namespace OIIO;

void
example1()
{
    //
    // Example code fragment from the docs goes here.
    //
    // It probably should generate either some text output (which will show up
    // in "out.txt" that captures each test's output), or it should produce a
    // (small) image file that can be compared against a reference image that
    // goes in the ref/ subdirectory of this test.
    //
}
// END-imageioapi-example1



#include <OpenImageIO/color.h>

void
example_colorspaceinfo()
{
    print("example_colorspaceinfo\n");
    // BEGIN-imageioapi-colorspaceinfo
    const ColorConfig& config = ColorConfig::default_colorconfig();
    for (string_view name :
         { "ACEScg", "g22_rec709_scene", "srgb_rec709_scene" }) {
        // Derive whatever properties the config does not declare
        ColorSpaceInfo info = config.derive_color_space_info(name);
        print("{}: gamma {:.3g}, chromaticities", name,
              info.transfer_function_gamma());
        for (float xy : info.chromaticities())
            print(" {:.3f}", xy);
        print("\n");
    }
    // END-imageioapi-colorspaceinfo
}

//
///////////////////////////////////////////////////////////////////////////



int
main(int /*argc*/, char** /*argv*/)
{
    // Each example function needs to get called here, or it won't execute
    // as part of the test.
    example1();
    example_colorspaceinfo();
    return 0;
}
