# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio

if (WebP_FOUND)
    add_oiio_plugin (webpinput.cpp webpoutput.cpp
                     INCLUDE_DIRS ${WEBP_INCLUDES}
                     LINK_LIBRARIES ${WEBP_LIBRARIES}
                     DEFINITIONS "-DUSE_WEBP=1")
else ()
    message (STATUS "WebP plugin will not be built")
endif()


