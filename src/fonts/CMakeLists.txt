# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

file (GLOB public_fonts "*/*.ttf")

if (INSTALL_FONTS AND FREETYPE_FOUND)
    install (FILES ${public_fonts}
             DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/fonts/OpenImageIO
             COMPONENT fonts)
endif ()

