# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

if (Ptex_FOUND)
    set(ptex_target Ptex::Ptex_dynamic)
    if (TARGET Ptex::Ptex_static AND (NOT TARGET Ptex::Ptex_dynamic OR LINKSTATIC))
        set(ptex_target Ptex::Ptex_static)
    endif()
    add_oiio_plugin (ptexinput.cpp
                     LINK_LIBRARIES ${ptex_target} ZLIB::ZLIB
                     DEFINITIONS "-DUSE_PTEX")
endif ()
