# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio

set (openexr_src exrinput.cpp exroutput.cpp)

option (OIIO_USE_EXR_C_API "Allow use of the new exr 3.1 C API if available" ON)
if (OIIO_USE_EXR_C_API AND TARGET OpenEXR::OpenEXRCore)
    set (openexr_defs OIIO_USE_EXR_C_API=1)
    list (APPEND openexr_src exrinput_c.cpp)
endif()

add_oiio_plugin (${openexr_src}
    INCLUDE_DIRS ${OPENEXR_INCLUDES} ${IMATH_INCLUDE_DIR}/OpenEXR
    LINK_LIBRARIES
        $<TARGET_NAME_IF_EXISTS:OpenEXR::OpenEXR>
        $<TARGET_NAME_IF_EXISTS:OpenEXR::OpenEXRCore>
    DEFINITIONS ${openexr_defs}
    )
