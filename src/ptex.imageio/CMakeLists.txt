# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

if (Ptex_FOUND)
    add_oiio_plugin (ptexinput.cpp
                     LINK_LIBRARIES Ptex::Ptex_dynamic ZLIB::ZLIB
                     DEFINITIONS "-DUSE_PTEX")
endif ()
