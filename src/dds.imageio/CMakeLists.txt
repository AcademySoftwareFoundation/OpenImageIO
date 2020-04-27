# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

if (Libsquish_FOUND)
    # External libsquish was found -- use it
    add_oiio_plugin (ddsinput.cpp
                     LINK_LIBRARIES Libsquish::Libsquish
                     )
else ()
    # No external libsquish was found -- use the embedded version.
    add_oiio_plugin (ddsinput.cpp squish/alpha.cpp squish/clusterfit.cpp
                 squish/colourblock.cpp squish/colourfit.cpp squish/colourset.cpp
                 squish/maths.cpp squish/rangefit.cpp squish/singlecolourfit.cpp
                 squish/squish.cpp
        INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/squish")
endif ()
