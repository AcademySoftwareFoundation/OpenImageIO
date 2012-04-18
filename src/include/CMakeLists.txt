set (public_headers argparse.h color.h dassert.h errorhandler.h export.h 
                    filesystem.h filter.h fmath.h hash.h
                    imagebuf.h imagebufalgo.h 
                    imagecache.h imageio.h
                    optparser.h osdep.h paramlist.h plugin.h
                    refcnt.h strutil.h sysutil.h texture.h thread.h timer.h
                    tinyformat.h typedesc.h ustring.h varyingref.h
    )

if (NOT USE_EXTERNAL_PUGIXML)
    list (APPEND public_headers pugixml.hpp pugiconfig.hpp)
endif()

message(STATUS "Create version.h from version.h.in")
# Mangle the SOVERSION so that it's a valid C++ identifier for the versioning
# namespace defined in version.h
string (REGEX REPLACE "\\." "_" MANGLED_SOVERSION ${SOVERSION})
set (OIIO_VERSION_NS "v${MANGLED_SOVERSION}")
configure_file(version.h.in ${CMAKE_BINARY_DIR}/include/version.h @ONLY)
list(APPEND public_headers ${CMAKE_BINARY_DIR}/include/version.h)

install (FILES ${public_headers} DESTINATION ${INCLUDE_INSTALL_DIR}
         COMPONENT developer)

if (USE_TBB)
    install (DIRECTORY tbb DESTINATION ${INCLUDE_INSTALL_DIR}
             COMPONENT developer)
endif ()

