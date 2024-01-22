# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause and Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

file (GLOB public_headers OpenImageIO/*.h)

option (USE_GENERATED_EXPORT_HEADER "Use CMake-generated export header" OFF)
if (USE_GENERATED_EXPORT_HEADER)
    include (GenerateExportHeader)
    set (exportextras "\n#define ${PROJ_NAME}_EXPORT ${PROJ_NAME}_API\n")
    generate_export_header (${PROJECT_NAME}
          # [BASE_NAME baseName]
          EXPORT_FILE_NAME export.h
          EXPORT_MACRO_NAME ${PROJ_NAME}_API
          NO_EXPORT_MACRO_NAME ${PROJ_NAME}_LOCAL
          STATIC_DEFINE ${PROJ_NAME}_STATIC_DEFINE
          DEPRECATED_MACRO_NAME ${PROJ_NAME}_DEPRECATED
          # [NO_DEPRECATED_MACRO_NAME noDeprecatedMacroName]
          # [DEFINE_NO_DEPRECATED]
          # [PREFIX_NAME prefix]
          CUSTOM_CONTENT_FROM_VARIABLE exportextras
          )
    list (REMOVE_ITEM public_headers ${PROJECT_NAME}/export.h)
    list (APPEND public_headers "${CMAKE_BINARY_DIR}/src/include/export.h")
endif()


# Mangle the SOVERSION so that it's a valid C++ identifier for the
# versioning namespace defined in ${versionfile}
string (REGEX REPLACE "\\." "_" MANGLED_SOVERSION ${SOVERSION})
set (OIIO_VERSION_NS "v${MANGLED_SOVERSION}")


# Generate version.h
set (versionfile oiioversion.h)
if (VERBOSE)
    message(STATUS "Create ${versionfile} from ${versionfile}.in")
endif ()
configure_file (${PROJECT_NAME}/${versionfile}.in "${CMAKE_BINARY_DIR}/include/${PROJECT_NAME}/${versionfile}" @ONLY)
list (APPEND public_headers "${CMAKE_BINARY_DIR}/include/${PROJECT_NAME}/${versionfile}")

# Generate half.h
if (VERBOSE)
    message(STATUS "Create half.h from half.h.in")
endif ()
configure_file (${PROJECT_NAME}/half.h.in "${CMAKE_BINARY_DIR}/include/${PROJECT_NAME}/half.h" @ONLY)
list (APPEND public_headers "${CMAKE_BINARY_DIR}/include/${PROJECT_NAME}/half.h")


# Generate Imath.h
if (VERBOSE)
    message(STATUS "Create Imath.h from Imath.h.in")
endif ()
configure_file (${PROJECT_NAME}/Imath.h.in "${CMAKE_BINARY_DIR}/include/${PROJECT_NAME}/Imath.h" @ONLY)
list (APPEND public_headers "${CMAKE_BINARY_DIR}/include/${PROJECT_NAME}/Imath.h")


install (FILES ${public_headers}
         DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
         COMPONENT developer)

file (GLOB detail_headers OpenImageIO/detail/*.h)
install (FILES ${detail_headers}
         DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}/detail
         COMPONENT developer)

if (INTERNALIZE_FMT OR OIIO_USING_FMT_LOCAL)
    set (fmt_headers_base_names)
    foreach (header_name core.h format-inl.h format.h ostream.h printf.h
             std.h base.h chrono.h)
        if (EXISTS "${FMT_INCLUDE_DIR}/fmt/${header_name}")
             list (APPEND fmt_headers_base_names ${header_name})
        endif ()
    endforeach ()
    set (fmt_internal_directory ${CMAKE_BINARY_DIR}/include/OpenImageIO/detail/fmt)
    list (TRANSFORM fmt_headers_base_names
          PREPEND ${FMT_INCLUDE_DIR}/fmt/
          OUTPUT_VARIABLE fmt_headers)
    list (TRANSFORM fmt_headers_base_names
          PREPEND ${fmt_internal_directory}/
          OUTPUT_VARIABLE fmt_headers_internal)
    add_custom_command (OUTPUT ${fmt_internal_directory}
                        COMMAND
                            ${CMAKE_COMMAND} -E make_directory
                            ${fmt_internal_directory})
    add_custom_command (OUTPUT ${fmt_headers_internal}
                        DEPENDS ${fmt_headers} ${fmt_internal_directory}
                        COMMAND
                            ${CMAKE_COMMAND} -E copy
                            ${fmt_headers}
                            ${fmt_internal_directory})
    add_custom_target (fmt_internal_target DEPENDS ${fmt_headers_internal})
else ()
    set (fmt_headers
         ${CMAKE_BINARY_DIR}/include/OpenImageIO/detail/fmt/format.h
         ${CMAKE_BINARY_DIR}/include/OpenImageIO/detail/fmt/ostream.h
         ${CMAKE_BINARY_DIR}/include/OpenImageIO/detail/fmt/printf.h )
    foreach (f format.h ostream.h printf.h)
        file (WRITE "${CMAKE_BINARY_DIR}/include/OpenImageIO/detail/fmt/${f}"
                    "#include <fmt/${f}>")
    endforeach ()
    if (fmt_VERSION VERSION_GREATER_EQUAL 9)
        list (APPEND fmt_headers ${CMAKE_BINARY_DIR}/include/OpenImageIO/detail/fmt/std.h)
        file (WRITE "${CMAKE_BINARY_DIR}/include/OpenImageIO/detail/fmt/std.h"
                    "#include <fmt/std.h>")
    endif ()
endif ()
install (FILES ${fmt_headers}
         DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}/detail/fmt
         COMPONENT developer)

if (NOT USE_EXTERNAL_PUGIXML)
    set (pugixml_headers
          OpenImageIO/detail/pugixml/pugixml.hpp
          OpenImageIO/detail/pugixml/pugiconfig.hpp
          OpenImageIO/detail/pugixml/pugixml.cpp)
    file (COPY ${pugixml_headers}
          DESTINATION ${CMAKE_BINARY_DIR}/include/OpenImageIO/detail/pugixml)
    install (FILES ${pugixml_headers}
             DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}/detail/pugixml
             COMPONENT developer)
endif ()
