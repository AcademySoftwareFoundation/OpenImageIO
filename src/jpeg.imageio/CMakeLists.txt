# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

if (libuhdr_FOUND)
    set (UHDR_DEFS USE_UHDR)
else ()
    set (LIBUHDR_INCLUDE_DIR "")
    set (LIBUHDR_LIBRARY "")
    set (UHDR_DEFS "")
endif ()

add_oiio_plugin (jpeginput.cpp jpegoutput.cpp
                 LINK_LIBRARIES
                     $<TARGET_NAME_IF_EXISTS:libjpeg-turbo::jpeg>
                     $<TARGET_NAME_IF_EXISTS:JPEG::JPEG>
                     $<TARGET_NAME_IF_EXISTS:libuhdr::libuhdr>
                 DEFINITIONS "${UHDR_DEFS}"
                 )
