// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio


// Verify that the OIIO libraries can be consumed by a cmake build

#include <iostream>

#include <OpenImageIO/imageio.h>



int
main(int argc, char* argv[])
{
    auto fl = OIIO::get_string_attribute("format_list");
    if (fl.size() > 0)
        std::cout << "OK\n";
    return 0;
}
