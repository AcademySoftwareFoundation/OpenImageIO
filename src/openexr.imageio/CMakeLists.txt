# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

add_oiio_plugin (exrinput.cpp exroutput.cpp
                 INCLUDE_DIRS ${OPENEXR_INCLUDES} ${IMATH_INCLUDE_DIR}/OpenEXR
                 LINK_LIBRARIES ${OPENEXR_LIBRARIES})

