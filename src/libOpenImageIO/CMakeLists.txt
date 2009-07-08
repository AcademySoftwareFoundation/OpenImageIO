set (libOpenImageIO_hdrs ../include/argparse.h
                         ../include/dassert.h
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
    include_directories (${PNG_INCLUDE_DIR} ${JPEG_INCLUDE_DIR})
    include_directories (${TIFF_INCLUDE_DIR} ${ZLIB_INCLUDE_DIR})
    add_definitions ("-DEMBED_PLUGINS=1")
endif ()


add_library (OpenImageIO SHARED ${libOpenImageIO_srcs})
target_link_libraries (OpenImageIO ${Boost_LIBRARIES})
if (MSVC)
    add_definitions (-DOPENEXR_DLL)
    add_definitions (-DBOOST_ALL_DYN_LINK)
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
                          )
    link_openexr (OpenImageIO)
endif ()

oiio_install_targets (OpenImageIO)


# Testing
if (GTEST_FOUND)
    add_executable (test_libOpenImageIO test_imageio.cpp)
    include_directories (${GTEST_INCLUDES})
    link_ilmbase (test_libOpenImageIO)
    target_link_libraries (test_libOpenImageIO OpenImageIO ${Boost_LIBRARIES} 
                                ${GTEST_LIBRARY} ${CMAKE_DL_LIBS})
endif ()
add_test (libOpenImageIO ${CMAKE_BINARY_DIR}/libOpenImageIO/test_libOpenImageIO)
