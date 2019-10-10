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
set (OIIO_BUILD_CPP11 1)
if (${CMAKE_CXX_STANDARD} VERSION_GREATER_EQUAL 14)
    set (OIIO_BUILD_CPP14 1)
endif ()
if (${CMAKE_CXX_STANDARD} VERSION_GREATER_EQUAL 17)
    set (OIIO_BUILD_CPP17 1)
endif ()
if (${CMAKE_CXX_STANDARD} VERSION_GREATER_EQUAL 20)
    set (OIIO_BUILD_CPP20 1)
endif ()
# Note: When our CMake minimum is at least 3.7, we can switch to less
# complex statements with VERSION_GREATER_EQUAL.

configure_file(OpenImageIO/oiioversion.h.in "${CMAKE_BINARY_DIR}/include/OpenImageIO/oiioversion.h" @ONLY)
list(APPEND public_headers "${CMAKE_BINARY_DIR}/include/OpenImageIO/oiioversion.h")

install (FILES ${public_headers}
         DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/OpenImageIO
         COMPONENT developer)

file (GLOB fmt_headers OpenImageIO/fmt/*.h)
install (FILES ${fmt_headers}
         DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/OpenImageIO/fmt
         COMPONENT developer)
