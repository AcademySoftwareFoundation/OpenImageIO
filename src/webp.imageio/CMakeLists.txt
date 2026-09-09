# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

if (WebP_FOUND)
    set (webp_optional_libraries)
    set (webp_optional_definitions)
    if (TARGET OpenImageIO_OpenMeta)
        list (APPEND webp_optional_libraries OpenImageIO_OpenMeta)
        list (APPEND webp_optional_definitions OIIO_USE_OPENMETA=1)
    endif ()
    add_oiio_plugin (webpinput.cpp webpoutput.cpp
                     LINK_LIBRARIES WebP::webp WebP::webpdemux WebP::libwebpmux
                                    ${webp_optional_libraries}
                     DEFINITIONS "USE_WEBP=1" ${webp_optional_definitions})
else ()
    message (STATUS "WebP plugin will not be built")
endif()
