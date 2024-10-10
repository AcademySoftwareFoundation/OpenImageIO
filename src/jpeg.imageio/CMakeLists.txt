# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

if (libuhdr_FOUND)
    add_oiio_plugin (jpeginput.cpp jpegoutput.cpp
                     INCLUDE_DIRS ${LIBUHDR_INCLUDE_DIR}
                     LINK_LIBRARIES
                         $<TARGET_NAME_IF_EXISTS:libjpeg-turbo::jpeg>
                         $<TARGET_NAME_IF_EXISTS:JPEG::JPEG>
                         ${LIBUHDR_LIBRARY}
                     DEFINITIONS "USE_UHDR"
                     )
else ()
    add_oiio_plugin (jpeginput.cpp jpegoutput.cpp
                     LINK_LIBRARIES
                         $<TARGET_NAME_IF_EXISTS:libjpeg-turbo::jpeg>
                         $<TARGET_NAME_IF_EXISTS:JPEG::JPEG>
                     )
endif ()
