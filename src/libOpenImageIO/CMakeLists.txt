set (libOpenImageIO_hdrs ../include/argparse.h
                         ../include/colortransfer.h
                         ../include/boosttest.h
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
                         ../include/pugiconfig.hpp
                         ../include/pugixml.hpp
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
                          ../libutil/colortransfer.cpp
                          ../libutil/errorhandler.cpp 
                          ../libutil/filesystem.cpp 
                          ../libutil/filter.cpp 
                          ../libutil/paramlist.cpp 
                          ../libutil/plugin.cpp 
                          ../libutil/pystring.cpp
                          ../libutil/pugixml.cpp
                          ../libutil/SHA1.cpp 
                          ../libutil/strutil.cpp 
                          ../libutil/sysutil.cpp 
                          ../libutil/typedesc.cpp 
                          ../libutil/ustring.cpp 
                          ../libtexture/texturesys.cpp 
                          ../libtexture/texture3d.cpp 
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
        ../bmp.imageio/bmpinput.cpp ../bmp.imageio/bmpoutput.cpp
        ../bmp.imageio/bmp_pvt.cpp
#        ../cineon.imageio/cineoninput.cpp ../cineon.imageio/cineonoutput.cpp
#          ../cineon.imageio/libcineon/Cineon.cpp 
#          ../cineon.imageio/libcineon/Codec.cpp ../cineon.imageio/libcineon/Reader.cpp
#          ../cineon.imageio/libcineon/Writer.cpp ../cineon.imageio/libcineon/CineonHeader.cpp
#          ../cineon.imageio/libcineon/ElementReadStream.cpp ../cineon.imageio/libcineon/InStream.cpp
        ../dds.imageio/ddsinput.cpp ../dds.imageio/ddsoutput.cpp
          ../dds.imageio/squish/alpha.cpp ../dds.imageio/squish/clusterfit.cpp
          ../dds.imageio/squish/colourblock.cpp ../dds.imageio/squish/colourfit.cpp
          ../dds.imageio/squish/colourset.cpp ../dds.imageio/squish/maths.cpp
          ../dds.imageio/squish/rangefit.cpp ../dds.imageio/squish/singlecolourfit.cpp
          ../dds.imageio/squish/squish.cpp
        ../dpx.imageio/dpxinput.cpp ../dpx.imageio/dpxoutput.cpp
          ../dpx.imageio/libdpx/DPX.cpp ../dpx.imageio/libdpx/OutStream.cpp
          ../dpx.imageio/libdpx/RunLengthEncoding.cpp ../dpx.imageio/libdpx/Codec.cpp
          ../dpx.imageio/libdpx/Reader.cpp ../dpx.imageio/libdpx/Writer.cpp
          ../dpx.imageio/libdpx/DPXHeader.cpp ../dpx.imageio/libdpx/ElementReadStream.cpp
          ../dpx.imageio/libdpx/InStream.cpp ../dpx.imageio/libdpx/DPXColorConverter.cpp
        ../fits.imageio/fitsinput.cpp ../fits.imageio/fitsoutput.cpp
          ../fits.imageio/fits_pvt.cpp
        ../hdr.imageio/hdrinput.cpp ../hdr.imageio/hdroutput.cpp
          ../hdr.imageio/rgbe.cpp
        ../ico.imageio/icoinput.cpp ../ico.imageio/icooutput.cpp
        ../jpeg.imageio/jpeginput.cpp ../jpeg.imageio/jpegoutput.cpp
          ../jpeg.imageio/jpegexif.cpp
        ../png.imageio/pnginput.cpp ../png.imageio/pngoutput.cpp
        ../pnm.imageio/pnminput.cpp ../pnm.imageio/pnmoutput.cpp
        ../ptex.imageio/ptexinput.cpp ../ptex.imageio/ptexoutput.cpp
        ../ptex.imageio/ptex/PtexCache.cpp ../ptex.imageio/ptex/PtexFilters.cpp
        ../ptex.imageio/ptex/PtexHalf.cpp ../ptex.imageio/ptex/PtexReader.cpp
        ../ptex.imageio/ptex/PtexSeparableFilter.cpp
        ../ptex.imageio/ptex/PtexSeparableKernel.cpp
        ../ptex.imageio/ptex/PtexTriangleFilter.cpp
        ../ptex.imageio/ptex/PtexTriangleKernel.cpp
        ../ptex.imageio/ptex/PtexUtils.cpp ../ptex.imageio/ptex/PtexWriter.cpp
        ../openexr.imageio/exrinput.cpp ../openexr.imageio/exroutput.cpp
        ../sgi.imageio/sgiinput.cpp ../sgi.imageio/sgioutput.cpp
        ../softimage.imageio/softimageinput.cpp 
           ../softimage.imageio/softimageoutput.cpp 
           ../softimage.imageio/softimage_pvt.cpp
        ../targa.imageio/targainput.cpp ../targa.imageio/targaoutput.cpp
        ../tiff.imageio/tiffinput.cpp ../tiff.imageio/tiffoutput.cpp
        ../zfile.imageio/zfile.cpp
        )
    if (NOT Boost_VERSION LESS "103500")
        # Boost < 1.35 is too old to support asio that socket needs
        set (libOpenImageIO_srcs ${libOpenImageIO_srcs}
             ../socket.imageio/socketinput.cpp
             ../socket.imageio/socketoutput.cpp 
             ../socket.imageio/socket_pvt.cpp
            )
        add_definitions ("-DUSE_BOOST_ASIO=1")
    endif ()
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
        message (STATUS "Jasper not found: jpeg2000 plugin will not be built")
        set (JASPER_LIBRARY "")
    endif (JASPER_FOUND)

    if (FIELD3D_FOUND)
        set (libOpenImageIO_srcs ${libOpenImageIO_srcs}
             ../field3d.imageio/field3dinput.cpp
             ../field3d.imageio/field3doutput.cpp
            )
        include_directories (${FIELD3D_INCLUDE_DIR})
    else (FIELD3D_FOUND)
        message (STATUS "Field3D not found: field3d plugin will not be built")
        set (FIELD3D_LIBRARY "")
        set (HDF5_LIBRARIES "")
    endif (FIELD3D_FOUND)
endif ()

if (BUILDSTATIC)
    add_library (OpenImageIO STATIC ${libOpenImageIO_srcs})
else ()
    add_library (OpenImageIO SHARED ${libOpenImageIO_srcs})
endif ()
target_link_libraries (OpenImageIO ${Boost_LIBRARIES})
if (MSVC)
    target_link_libraries (OpenImageIO psapi.lib)
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
                               ${FIELD3D_LIBRARY}
                               ${HDF5_LIBRARIES}
                          )
    link_openexr (OpenImageIO)
endif ()

if (SOVERSION)
set_target_properties(OpenImageIO
                         PROPERTIES
                         VERSION ${OpenImageIO_VERSION_MAJOR}.${OpenImageIO_VERSION_MINOR}.${OpenImageIO_VERSION_PATCH}
                         SOVERSION ${OpenImageIO_VERSION_MAJOR}
                     )
endif ()

oiio_install_targets (OpenImageIO)


# Testing

# Definition needed by boost_unit_test_framework library
if (NOT LINKSTATIC)
    add_definitions (-DBOOST_TEST_DYN_LINK)
endif ()

if (GTEST_FOUND)
    add_executable (imagebufalgo_test imagebufalgo_test.cpp)
    include_directories (${GTEST_INCLUDES})
    link_ilmbase (imagebufalgo_test)
    target_link_libraries (imagebufalgo_test OpenImageIO ${Boost_LIBRARIES} 
                                ${GTEST_LIBRARY} ${CMAKE_DL_LIBS})
endif ()
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

add_executable (imagespec_test imagespec_test.cpp)
target_link_libraries (imagespec_test OpenImageIO ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
add_test (unit_imagespec ${CMAKE_BINARY_DIR}/libOpenImageIO/imagespec_test)

add_executable (strutil_test strutil_test.cpp)
target_link_libraries (strutil_test OpenImageIO ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
add_test (unit_strutil ${CMAKE_BINARY_DIR}/libOpenImageIO/strutil_test)
