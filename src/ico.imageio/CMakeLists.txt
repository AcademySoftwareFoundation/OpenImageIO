# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

if (PNG_TARGET)
    add_oiio_plugin (icoinput.cpp icooutput.cpp
                     INCLUDE_DIRS ${PNG_INCLUDE_DIRS}
                     DEFINITIONS ${PNG_DEFINITIONS}
                     LINK_LIBRARIES ${PNG_LIBRARIES} ${PNG_TARGET} ZLIB::ZLIB)
else ()
    message (WARNING "libpng not found, so ICO support will not work")
    set (format_plugin_definitions ${format_plugin_definitions} DISABLE_ICO=1 PARENT_SCOPE)
endif ()

