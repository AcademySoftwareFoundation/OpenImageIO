# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

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


install (FILES ${public_headers}
         DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
         COMPONENT developer)

set (fmt_headers
        ${FMT_INCLUDES}/fmt/core.h
        ${FMT_INCLUDES}/fmt/format-inl.h
        ${FMT_INCLUDES}/fmt/format.h
        ${FMT_INCLUDES}/fmt/ostream.h
        ${FMT_INCLUDES}/fmt/printf.h )
file (COPY ${fmt_headers}
      DESTINATION ${CMAKE_BINARY_DIR}/include/OpenImageIO/detail/fmt)
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
