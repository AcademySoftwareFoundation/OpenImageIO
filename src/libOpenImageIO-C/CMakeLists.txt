# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

set(PUBLIC_HEADER_DIR ${PROJECT_SOURCE_DIR}/src/include/OpenImageIO)
set(COIIO_HEADERS
    ${PUBLIC_HEADER_DIR}/c-imageio.h
    ${PUBLIC_HEADER_DIR}/c-typedesc.h
    ${PUBLIC_HEADER_DIR}/c-paramlist.h
    ${PUBLIC_HEADER_DIR}/c-deepdata.h
)

add_library(OpenImageIO-C SHARED
    c-imageio.cpp
    c-typedesc.cpp
    c-paramlist.cpp
    c-deepdata.cpp
    util.h
    ${COIIO_HEADERS}
)

target_link_libraries(OpenImageIO-C PUBLIC OpenImageIO)
set_target_properties(OpenImageIO-C
    PROPERTIES
        VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
        SOVERSION ${SOVERSION}
        POSITION_INDEPENDENT_CODE ON
)
target_compile_definitions(OpenImageIO-C PRIVATE OpenImageIO_EXPORTS)

install_targets(OpenImageIO-C)
