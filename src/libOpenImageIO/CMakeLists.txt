set (libOpenImageIO_hdrs ../include/argparse.h
                         ../include/dassert.h
                         ../include/errorhandler.h
                         ../include/filesystem.h
                         ../include/filter.h
                         ../include/fmath.h
                         ../include/hash.h
                         ../include/imagebuf.h
                         ../include/imagecache.h
                         ../include/imageio.h
                         ../include/osdep.h
                         ../include/paramlist.h
                         ../include/plugin.h
                         ../include/SHA1.h
                         ../include/strutil.h
                         ../include/sysutil.h
                         ../include/texture.h
                         ../include/thread.h
                         ../include/timer.h
                         ../include/typedesc.h
                         ../include/ustring.h
                         ../include/varyingref.h    
                         )

set (libOpenImageIO_srcs formatspec.cpp imagebuf.cpp imagebufalgo.cpp
                          imageinput.cpp imageio.cpp imageioplugin.cpp
                          imageoutput.cpp iptc.cpp xmp.cpp 
                          ../libutil/argparse.cpp 
                          ../libutil/errorhandler.cpp 
                          ../libutil/filesystem.cpp 
                          ../libutil/filter.cpp 
                          ../libutil/paramlist.cpp 
                          ../libutil/plugin.cpp 
                          ../libutil/SHA1.cpp 
                          ../libutil/strutil.cpp 
                          ../libutil/sysutil.cpp 
                          ../libutil/typedesc.cpp 
                          ../libutil/ustring.cpp 
                          ../libtexture/texturesys.cpp 
                          ../libtexture/texoptions.cpp 
                          ../libtexture/imagecache.cpp
                          ${libOpenImageIO_hdrs}
                         )

# Include out own TBB if not getting it from an external area
if (NOT WIN32 AND USE_TBB)
  set (libOpenImageIO_srcs ${libOpenImageIO_srcs} ../libutil/tbb_misc.cpp)
endif ()

# If the 'EMBEDPLUGINS' option is set, we want to compile the source for
# all the plugins into libOpenImageIO.
if (EMBEDPLUGINS)
    set (libOpenImageIO_srcs ${libOpenImageIO_srcs}
        ../hdr.imageio/hdrinput.cpp ../hdr.imageio/hdroutput.cpp
          ../hdr.imageio/rgbe.cpp
        ../ico.imageio/icoinput.cpp ../ico.imageio/icooutput.cpp
        ../jpeg.imageio/jpeginput.cpp ../jpeg.imageio/jpegoutput.cpp
          ../jpeg.imageio/jpegexif.cpp
        ../png.imageio/pnginput.cpp ../png.imageio/pngoutput.cpp
        ../openexr.imageio/exrinput.cpp ../openexr.imageio/exroutput.cpp
        ../targa.imageio/targainput.cpp ../targa.imageio/targaoutput.cpp
        ../tiff.imageio/tiffinput.cpp ../tiff.imageio/tiffoutput.cpp
        ../zfile.imageio/zfile.cpp
        )
    find_package (PNG REQUIRED)
    find_package (JPEG REQUIRED)
    find_package (TIFF REQUIRED)
    find_package (ZLIB REQUIRED)
    find_package (Jasper)
    include_directories (${PNG_INCLUDE_DIR} ${JPEG_INCLUDE_DIR})
    include_directories (${TIFF_INCLUDE_DIR} ${ZLIB_INCLUDE_DIR})
    add_definitions ("-DEMBED_PLUGINS=1")
    if (JASPER_FOUND)
        set (libOpenImageIO_srcs ${libOpenImageIO_srcs}
             ../jpeg2000.imageio/jpeg2000input.cpp
             ../jpeg2000.imageio/jpeg2000output.cpp
            )
        include_directories (${JASPER_INCLUDE_DIR})
        add_definitions ("-DUSE_JASPER=1")
    else (JASPER_FOUND)
        message (STATUS "Jasper not found: jpeg2000 plugin would no be built")
        set (JASPER_LIBRARY "")
    endif (JASPER_FOUND)
endif ()


add_library (OpenImageIO SHARED ${libOpenImageIO_srcs})
target_link_libraries (OpenImageIO ${Boost_LIBRARIES})
if (MSVC)
    set_target_properties (OpenImageIO PROPERTIES LINK_FLAGS psapi.lib)
endif (MSVC)
if (WIN32 AND USE_TBB)
    target_link_libraries (OpenImageIO
                           optimized ${TBB_LIBRARY}
                           debug ${TBB_DEBUG_LIBRARY})
endif (WIN32 AND USE_TBB)
link_ilmbase (OpenImageIO)

if (EMBEDPLUGINS)
    target_link_libraries (OpenImageIO ${PNG_LIBRARIES} ${TIFF_LIBRARIES}
                               ${JPEG_LIBRARIES} ${ZLIB_LIBRARIES}
                               ${JASPER_LIBRARY}
                          )
    link_openexr (OpenImageIO)
endif ()

oiio_install_targets (OpenImageIO)


# Testing
if (GTEST_FOUND)
    add_executable (imagespec_test imagespec_test.cpp)
    include_directories (${GTEST_INCLUDES})
    link_ilmbase (imagespec_test)
    target_link_libraries (imagespec_test OpenImageIO ${Boost_LIBRARIES} 
                                ${GTEST_LIBRARY} ${CMAKE_DL_LIBS})

    add_executable (imagebufalgo_test imagebufalgo_test.cpp)
    include_directories (${GTEST_INCLUDES})
    link_ilmbase (imagebufalgo_test)
    target_link_libraries (imagebufalgo_test OpenImageIO ${Boost_LIBRARIES} 
                                ${GTEST_LIBRARY} ${CMAKE_DL_LIBS})
endif ()
add_test (imagespec ${CMAKE_BINARY_DIR}/libOpenImageIO/imagespec_test)
add_test (imagebufalgo ${CMAKE_BINARY_DIR}/libOpenImageIO/imagebufalgo_test)


add_executable (atomic_test atomic_test.cpp)
target_link_libraries (atomic_test OpenImageIO ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
add_test (unit_atomic ${CMAKE_BINARY_DIR}/libOpenImageIO/atomic_test)

add_executable (spinlock_test spinlock_test.cpp)
target_link_libraries (spinlock_test OpenImageIO ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
add_test (unit_spinlock ${CMAKE_BINARY_DIR}/libOpenImageIO/spinlock_test)

add_executable (ustring_test ustring_test.cpp)
target_link_libraries (ustring_test OpenImageIO ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
add_test (unit_ustring ${CMAKE_BINARY_DIR}/libOpenImageIO/ustring_test)
