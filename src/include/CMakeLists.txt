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
if (USE_CPP VERSION_GREATER 11)
    set (OIIO_BUILD_CPP14 1)
endif ()
if (USE_CPP VERSION_GREATER 17)
    set (OIIO_BUILD_CPP17 1)
endif ()
configure_file(OpenImageIO/oiioversion.h.in "${CMAKE_BINARY_DIR}/include/OpenImageIO/oiioversion.h" @ONLY)
list(APPEND public_headers "${CMAKE_BINARY_DIR}/include/OpenImageIO/oiioversion.h")

install (FILES ${public_headers}
         DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/OpenImageIO
         COMPONENT developer)
