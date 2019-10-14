# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

file (GLOB public_headers OpenImageIO/*.h)

if (NOT USE_EXTERNAL_PUGIXML)
    list (APPEND public_headers
          OpenImageIO/pugixml.hpp
          OpenImageIO/pugiconfig.hpp
          OpenImageIO/pugixml.cpp)
endif ()


if (VERBOSE)
    message(STATUS "Create oiioversion.h from oiioversion.h.in")
endif ()
# Mangle the SOVERSION so that it's a valid C++ identifier for the versioning
# namespace defined in oiioversion.h
string (REGEX REPLACE "\\." "_" MANGLED_SOVERSION ${SOVERSION})
set (OIIO_VERSION_NS "v${MANGLED_SOVERSION}")

option (USE_GENERATED_EXPORT_HEADER "Use CMake-generated export header" OFF)
if (USE_GENERATED_EXPORT_HEADER)
    include (GenerateExportHeader)
    set (exportextras "\n#define OIIO_EXPORT OIIO_API\n")
    generate_export_header (OpenImageIO
          # [BASE_NAME baseName]
          EXPORT_FILE_NAME export.h
          EXPORT_MACRO_NAME OIIO_API
          NO_EXPORT_MACRO_NAME OIIO_LOCAL
          DEPRECATED_MACRO_NAME OPENIMAGEIO_DEPRECATED
          # STATIC_DEFINE staticDefine
          # [NO_DEPRECATED_MACRO_NAME noDeprecatedMacroName]
          # [DEFINE_NO_DEPRECATED]
          # [PREFIX_NAME prefix]
          CUSTOM_CONTENT_FROM_VARIABLE exportextras
          )
    list (REMOVE_ITEM public_headers OpenImageIO/export.h)
    list (APPEND public_headers "${CMAKE_BINARY_DIR}/src/include/export.h")
endif()

configure_file(OpenImageIO/oiioversion.h.in "${CMAKE_BINARY_DIR}/include/OpenImageIO/oiioversion.h" @ONLY)
list(APPEND public_headers "${CMAKE_BINARY_DIR}/include/OpenImageIO/oiioversion.h")

install (FILES ${public_headers}
         DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/OpenImageIO
         COMPONENT developer)

file (GLOB fmt_headers OpenImageIO/fmt/*.h)
install (FILES ${fmt_headers}
         DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/OpenImageIO/fmt
         COMPONENT developer)
