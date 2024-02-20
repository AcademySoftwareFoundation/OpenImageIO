# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

set (openexr_src exrinput.cpp exroutput.cpp)

option (OIIO_USE_EXR_C_API "Allow use of the new exr 3.1 C API if available" ON)
if (OIIO_USE_EXR_C_API AND TARGET OpenEXR::OpenEXRCore)
    set (openexr_defs OIIO_USE_EXR_C_API=1)
    list (APPEND openexr_src exrinput_c.cpp)
endif()

# Enable default use of OpenEXR core library for versions of the library
# new enough to support DWA compression in exrcore.
if (OPENEXR_VERSION VERSION_GREATER_EQUAL 3.1.10)
    list (APPEND openexr_defs OIIO_OPENEXR_CORE_DEFAULT=1)
    message (STATUS "OpenEXR core library will be used by default")
else ()
    list (APPEND openexr_defs OIIO_OPENEXR_CORE_DEFAULT=0)
    message (STATUS "OpenEXR core library will not be used by default")
endif ()

add_oiio_plugin (${openexr_src}
    INCLUDE_DIRS ${OPENEXR_INCLUDES} ${IMATH_INCLUDE_DIR}/OpenEXR
    LINK_LIBRARIES
        $<TARGET_NAME_IF_EXISTS:OpenEXR::OpenEXR>
        $<TARGET_NAME_IF_EXISTS:OpenEXR::OpenEXRCore>
    DEFINITIONS ${openexr_defs}
    )
