set (public_headers argparse.h dassert.h errorhandler.h export.h 
                    filesystem.h fmath.h hash.h
                    imagebuf.h imagebufalgo.h 
                    imagecache.h imageio.h osdep.h paramlist.h
                    refcnt.h strutil.h sysutil.h texture.h thread.h timer.h
                    typedesc.h ustring.h varyingref.h
                    colortransfer.h pugixml.hpp pugiconfig.hpp
    )

message(STATUS "Create version.h from version.h.in")
configure_file(version.h.in ${CMAKE_BINARY_DIR}/include/version.h @ONLY)
list(APPEND public_headers ${CMAKE_BINARY_DIR}/include/version.h)

install (FILES ${public_headers} DESTINATION include/OpenImageIO
         COMPONENT developer)

if (USE_TBB)
    file (GLOB tbb_headers tbb/*.h)
    file (GLOB tbb_headers2 tbb/machine/*.h)
    install (FILES ${tbb_headers} DESTINATION include/OpenImageIO/tbb
             COMPONENT developer)
    install (FILES ${tbb_headers2} DESTINATION include/OpenImageIO/tbb/machine
             COMPONENT developer)
endif ()

