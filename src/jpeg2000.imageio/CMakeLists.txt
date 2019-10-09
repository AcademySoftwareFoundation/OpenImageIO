# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

if (OPENJPEG_FOUND)
    if (${OPENJPEG_VERSION} VERSION_LESS 2.0)
        # Old OpenJpeg 1.5. Remove this eventually.
        #
        # For reasons I don't understand, OpenJPEG 1.5 fails imageinout_test
        # when we build in EMBEDPLUGINS=0 mode (works fine for
        # EMBEDPLUGINS=1). I don't care to do the work to figure out why,
        # for a rarely used build mode and an obsolete library version. So
        # for now, I'm just disabling jpeg2000 support for that combination.
        if (EMBEDPLUGINS)
            add_oiio_plugin (jpeg2000input-v1.cpp jpeg2000output-v1.cpp
                             INCLUDE_DIRS ${OPENJPEG_INCLUDES}
                             LINK_LIBRARIES ${OPENJPEG_LIBRARIES}
                             DEFINITIONS "-DUSE_OPENJPEG")
        endif()
    else ()
        # OpenJpeg 2.x. Eventually this should be the only one we need.
        add_oiio_plugin (jpeg2000input.cpp jpeg2000output.cpp
                         INCLUDE_DIRS ${OPENJPEG_INCLUDES}
                         LINK_LIBRARIES ${OPENJPEG_LIBRARIES}
                         DEFINITIONS "-DUSE_OPENJPEG")
    endif ()
else()
    message (WARNING "Jpeg-2000 plugin will not be built")
endif()
