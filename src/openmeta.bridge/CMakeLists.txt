# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

if (BUILD_SHARED_LIBS)
    add_library (OpenImageIO_OpenMetaBridge SHARED openmeta_bridge.cpp)
    target_compile_definitions (OpenImageIO_OpenMetaBridge
                                PRIVATE OIIO_OPENMETA_BRIDGE_EXPORTS=1)
else ()
    add_library (OpenImageIO_OpenMetaBridge STATIC openmeta_bridge.cpp)
    target_compile_definitions (OpenImageIO_OpenMetaBridge
                                PUBLIC OIIO_OPENMETA_BRIDGE_STATIC=1)
endif ()
target_compile_features (OpenImageIO_OpenMetaBridge PRIVATE cxx_std_20)
target_include_directories (OpenImageIO_OpenMetaBridge
                            PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries (OpenImageIO_OpenMetaBridge PRIVATE OpenMeta::openmeta)
set (OpenImageIO_OpenMetaBridge_LINK_FLAGS "")
if (BUILD_SHARED_LIBS)
    set (OpenImageIO_OpenMetaBridge_LINK_FLAGS "${EXTRA_DSO_LINK_ARGS}")
    if (UNIX AND NOT APPLE)
        # OpenMeta is commonly static; do not export its symbols from the bridge.
        string (APPEND OpenImageIO_OpenMetaBridge_LINK_FLAGS
                " -Wl,--exclude-libs,ALL")
    endif ()
    if ((CMAKE_SYSTEM_NAME MATCHES "Linux|kFreeBSD")
            OR CMAKE_SYSTEM_NAME STREQUAL "GNU")
        set (_openmeta_bridge_map
             "${CMAKE_CURRENT_SOURCE_DIR}/openmeta_bridge.map")
        string (APPEND OpenImageIO_OpenMetaBridge_LINK_FLAGS
                " -Wl,--version-script=${_openmeta_bridge_map}")
    elseif (APPLE)
        set (_openmeta_bridge_exports
             "${CMAKE_CURRENT_SOURCE_DIR}/openmeta_bridge.exports")
        string (APPEND OpenImageIO_OpenMetaBridge_LINK_FLAGS
                " -Wl,-exported_symbols_list," "${_openmeta_bridge_exports}")
    endif ()
endif ()
set_target_properties (OpenImageIO_OpenMetaBridge PROPERTIES
                       CXX_STANDARD 20
                       CXX_STANDARD_REQUIRED ON
                       CXX_EXTENSIONS OFF
                       POSITION_INDEPENDENT_CODE ON
                       LINK_FLAGS "${OpenImageIO_OpenMetaBridge_LINK_FLAGS}"
                       FOLDER "Support")

add_library (OpenImageIO_OpenMeta STATIC openmeta_oiio.cpp)
target_compile_features (OpenImageIO_OpenMeta PRIVATE cxx_std_17)
target_include_directories (OpenImageIO_OpenMeta
                            PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries (OpenImageIO_OpenMeta
                       PRIVATE OpenImageIO_Util OpenImageIO_OpenMetaBridge)
set_target_properties (OpenImageIO_OpenMeta PROPERTIES
                       CXX_STANDARD 17
                       CXX_STANDARD_REQUIRED ON
                       CXX_EXTENSIONS OFF
                       POSITION_INDEPENDENT_CODE ON
                       FOLDER "Support")

if (BUILD_SHARED_LIBS)
    install (TARGETS OpenImageIO_OpenMetaBridge
             RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT user
             LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT user
             ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT developer)
elseif (EMBEDPLUGINS)
    install_targets (OpenImageIO_OpenMetaBridge OpenImageIO_OpenMeta)
endif ()

if (OIIO_BUILD_TESTS AND BUILD_TESTING)
    fancy_add_executable (NAME openmeta_bridge_test
                          SRC openmeta_bridge_test.cpp
                          NO_INSTALL
                          FOLDER "Unit Tests"
                          LINK_LIBRARIES OpenImageIO_OpenMeta OpenImageIO_Util)
    set_target_properties (openmeta_bridge_test PROPERTIES
                           CXX_STANDARD 17
                           CXX_STANDARD_REQUIRED ON
                           CXX_EXTENSIONS OFF)
    add_test (unit_openmeta_bridge
              ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/openmeta_bridge_test)
endif ()
