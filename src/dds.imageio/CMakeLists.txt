# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio

if (Libsquish_FOUND)
    # External libsquish was found -- use it
    add_oiio_plugin (ddsinput.cpp
                     INCLUDE_DIRS ${LIBSQUISH_INCLUDES}
                     LINK_LIBRARIES ${LIBSQUISH_LIBRARIES}
                     )
else ()
    # No external libsquish was found -- use the embedded version.
    add_oiio_plugin (ddsinput.cpp squish/alpha.cpp squish/clusterfit.cpp
                 squish/colourblock.cpp squish/colourfit.cpp squish/colourset.cpp
                 squish/maths.cpp squish/rangefit.cpp squish/singlecolourfit.cpp
                 squish/squish.cpp
        INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/squish")
endif ()
