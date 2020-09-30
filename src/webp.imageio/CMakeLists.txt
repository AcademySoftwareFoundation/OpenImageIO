# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

if (WebP_FOUND)
    add_oiio_plugin (webpinput.cpp webpoutput.cpp
                     LINK_LIBRARIES WebP::WebP WebP::WebPDemux
                     DEFINITIONS "-DUSE_WEBP=1")
else ()
    message (STATUS "WebP plugin will not be built")
endif()


