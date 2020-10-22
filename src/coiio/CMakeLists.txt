# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

add_library(coiio SHARED 
    c-imageio.cpp
    c-imageio.h
    c-imageio_defines.h
    c-typedesc.cpp
    c-typedesc.h
)

target_link_libraries(coiio PUBLIC OpenImageIO)
set_target_properties(coiio 
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

install_targets(coiio)
install(FILES ${public_headers}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/coiio
    COMPONENT developer
)