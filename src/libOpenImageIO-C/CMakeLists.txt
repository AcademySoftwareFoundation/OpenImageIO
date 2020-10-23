# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

add_library(OpenImageIO-C SHARED 
    c-imageio.cpp
    c-imageio.h
    c-imageio_defines.h
    c-typedesc.cpp
    c-typedesc.h
)

target_link_libraries(OpenImageIO-C PUBLIC OpenImageIO)
set_target_properties(OpenImageIO-C 
    PROPERTIES
        VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
        SOVERSION ${SOVERSION}
        POSITION_INDEPENDENT_CODE ON
)

set(public_headers
    c-imageio.h
    c-imageio_defines.h
    c-typedesc.h
)

install_targets(OpenImageIO-C)
install(FILES ${public_headers}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/OpenImageIO-C
    COMPONENT developer
)