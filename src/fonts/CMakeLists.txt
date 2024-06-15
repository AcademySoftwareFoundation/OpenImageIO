# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause and Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

file (GLOB public_fonts "*/*.ttf")

if (INSTALL_FONTS AND (Freetype_FOUND OR FREETYPE_FOUND))
    install (FILES ${public_fonts}
             DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/fonts/OpenImageIO
             COMPONENT fonts)
endif ()

