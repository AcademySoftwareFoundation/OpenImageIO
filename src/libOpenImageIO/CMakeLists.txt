set (libOpenImageIO_hdrs ../include/argparse.h
                         ../include/color.h
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

if (NOT USE_EXTERNAL_PUGIXML)
    list (APPEND libOpenImageIO_hdrs
          ../include/pugiconfig.hpp
          ../include/pugixml.hpp
    )
endif()

set (libOpenImageIO_srcs exif.cpp formatspec.cpp imagebuf.cpp
                          imageinput.cpp imageio.cpp imageioplugin.cpp
                          imageoutput.cpp iptc.cpp xmp.cpp
                          color_ocio.cpp
                          imagebufalgo.cpp imagebufalgo_orient.cpp
                          imagebufalgo_yee.cpp imagebufalgo_opencv.cpp
                          maketexture.cpp
                          ../libutil/argparse.cpp
                          ../libutil/errorhandler.cpp 
                          ../libutil/filesystem.cpp 
                          ../libutil/filter.cpp 
                          ../libutil/hashes.cpp 
                          ../libutil/paramlist.cpp 
                          ../libutil/plugin.cpp 
                          ../libutil/pystring.cpp
                          ../libutil/SHA1.cpp 
                          ../libutil/strutil.cpp 
                          ../libutil/sysutil.cpp 
                          ../libutil/typedesc.cpp 
                          ../libutil/ustring.cpp 
                          ../libtexture/texturesys.cpp 
                          ../libtexture/texture3d.cpp 
                          ../libtexture/environment.cpp 
                          ../libtexture/texoptions.cpp 
                          ../libtexture/imagecache.cpp
                          ${libOpenImageIO_hdrs}
                         )

if (NOT USE_EXTERNAL_PUGIXML)
    list (APPEND libOpenImageIO_srcs ../libutil/pugixml.cpp)
endif ()

# Include our own TBB if using it
if (USE_TBB AND USE_EXTERNAL_TBB)
    message (STATUS "System TBB library will be used.")
    find_package (TBB REQUIRED)
    include_directories (${TBB_INCLUDE_DIRS})
    set (libOpenImageIO_srcs ${libOpenImageIO_srcs})
elseif (USE_TBB AND NOT USE_EXTERNAL_TBB)
    message (STATUS "Built-in TBB library will be used.")
    set (libOpenImageIO_srcs ${libOpenImageIO_srcs} ../libutil/tbb_misc.cpp)
endif ()

# If the 'EMBEDPLUGINS' option is set, we want to compile the source for
# all the plugins into libOpenImageIO.
if (EMBEDPLUGINS)
    set (libOpenImageIO_srcs ${libOpenImageIO_srcs}
        ../bmp.imageio/bmpinput.cpp ../bmp.imageio/bmpoutput.cpp
        ../bmp.imageio/bmp_pvt.cpp
        ../cineon.imageio/cineoninput.cpp ../cineon.imageio/cineonoutput.cpp
          ../cineon.imageio/libcineon/Cineon.cpp 
          ../cineon.imageio/libcineon/Codec.cpp ../cineon.imageio/libcineon/Reader.cpp
          ../cineon.imageio/libcineon/Writer.cpp ../cineon.imageio/libcineon/CineonHeader.cpp
          ../cineon.imageio/libcineon/ElementReadStream.cpp ../cineon.imageio/libcineon/InStream.cpp
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
		../iff.imageio/iffinput.cpp
		  ../iff.imageio/iffoutput.cpp
		  ../iff.imageio/iff_pvt.cpp
        ../jpeg.imageio/jpeginput.cpp ../jpeg.imageio/jpegoutput.cpp
        ../png.imageio/pnginput.cpp ../png.imageio/pngoutput.cpp
        ../pnm.imageio/pnminput.cpp ../pnm.imageio/pnmoutput.cpp
        ../psd.imageio/psdinput.cpp ../psd.imageio/psdoutput.cpp
        ../psd.imageio/jpeg_memory_src.cpp
        ../ptex.imageio/ptexinput.cpp ../ptex.imageio/ptexoutput.cpp
        ../ptex.imageio/ptex/PtexCache.cpp ../ptex.imageio/ptex/PtexFilters.cpp
        ../ptex.imageio/ptex/PtexHalf.cpp ../ptex.imageio/ptex/PtexReader.cpp
        ../ptex.imageio/ptex/PtexSeparableFilter.cpp
        ../ptex.imageio/ptex/PtexSeparableKernel.cpp
        ../ptex.imageio/ptex/PtexTriangleFilter.cpp
        ../ptex.imageio/ptex/PtexTriangleKernel.cpp
        ../ptex.imageio/ptex/PtexUtils.cpp ../ptex.imageio/ptex/PtexWriter.cpp
        ../openexr.imageio/exrinput.cpp ../openexr.imageio/exroutput.cpp
        ../rla.imageio/rlainput.cpp ../rla.imageio/rlaoutput.cpp
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
    include_directories (${PNG_INCLUDE_DIR} ${JPEG_INCLUDE_DIR})
    include_directories (${TIFF_INCLUDE_DIR} ${ZLIB_INCLUDE_DIR})

    add_definitions ("-DEMBED_PLUGINS=1")

    if (FIELD3D_FOUND AND USE_FIELD3D)
        set (libOpenImageIO_srcs ${libOpenImageIO_srcs}
             ../field3d.imageio/field3dinput.cpp
             ../field3d.imageio/field3doutput.cpp
            )
        include_directories (${FIELD3D_INCLUDE_DIR})
    else ()
        message (STATUS "Field3D plugin will not be built")
        set (FIELD3D_LIBRARY "")
        set (HDF5_LIBRARIES "")
    endif ()

    if (OPENJPEG_FOUND AND USE_OPENJPEG)
        set (libOpenImageIO_srcs ${libOpenImageIO_srcs}
             ../jpeg2000.imageio/jpeg2000input.cpp ../jpeg2000.imageio/jpeg2000output.cpp)
        include_directories (${OPENJPEG_INCLUDE_DIR})
        add_definitions("-DUSE_OPENJPEG")
    else()
        message (STATUS "Jpeg-2000 plugin will not be built")
    endif()

    if (WEBP_FOUND)
        set (libOpenImageIO_srcs ${libOpenImageIO_srcs}
             ../webp.imageio/webpinput.cpp
             ../webp.imageio/webpoutput.cpp
            )
        include_directories (${WEBP_INCLUDE_DIR})
        add_definitions ("-DUSE_WEBP=1")
    else ()
        message (STATUS "WebP plugin will not be built")
        set (WEBP_LIBRARY "")
    endif ()

    # Organize the embedded plugins into source folders
    set (plugin_types "")
    foreach (src ${libOpenImageIO_srcs})
        if (src MATCHES "^.+/([^/]+)\\.imageio/.+$")
            set (plugin_types ${plugin_types} ${CMAKE_MATCH_1})
        endif ()
    endforeach ()
    list (REMOVE_DUPLICATES plugin_types)
    foreach (plugin ${plugin_types})
        source_group ("Plugins\\${plugin}"
                      REGULAR_EXPRESSION "^.+/${plugin}\\.imageio/.+$"
                     )
    endforeach ()
else()
    find_package (TIFF REQUIRED)
    include_directories (${TIFF_INCLUDE_DIR})
endif ()

# Source groups for libutil and libtexture
source_group ("libutil"    REGULAR_EXPRESSION ".+/libutil/.+")
source_group ("libtexture" REGULAR_EXPRESSION ".+/libtexture/.+")

if (BUILDSTATIC)
    add_library (OpenImageIO STATIC ${libOpenImageIO_srcs})
else ()
    add_library (OpenImageIO SHARED ${libOpenImageIO_srcs})
endif ()
target_link_libraries (OpenImageIO
                           ${VISIBILITY_COMMAND} ${VISIBILITY_MAP_COMMAND}
                           ${Boost_LIBRARIES})

# Link against system TBB library if specified
if (USE_TBB AND USE_EXTERNAL_TBB)
    message (STATUS "Linking TBB: ${TBB_LIBRARIES}")
    target_link_libraries (OpenImageIO ${TBB_LIBRARIES})
endif ()

# Include OpenColorIO if using it
if (USE_OCIO AND OCIO_FOUND)
    message (STATUS "Linking OpenColorIO ${OCIO_LIBRARIES}")
    target_link_libraries (OpenImageIO ${OCIO_LIBRARIES})
endif ()


# Include OpenCV if using it
if (OpenCV_FOUND)
    include_directories (${OpenCV_INCLUDE_DIR})
    target_link_libraries (OpenImageIO opencv_core opencv_highgui)
endif ()

# Include OpenSSL if using it
if (OPENSSL_FOUND)
    include_directories (${OPENSSL_INCLUDE_DIR})
    target_link_libraries (OpenImageIO ${OPENSSL_LIBRARIES})
endif ()

# Include Freetype if using it
if (FREETYPE_FOUND)
    include_directories (${FREETYPE_INCLUDE_DIRS})
    target_link_libraries (OpenImageIO ${FREETYPE_LIBRARIES})
endif ()



if (WIN32)
    target_link_libraries (OpenImageIO psapi.lib)
endif ()
link_ilmbase (OpenImageIO)
add_dependencies (OpenImageIO ${CMAKE_CURRENT_SOURCE_DIR}/libOpenImageIO.map)

if (EMBEDPLUGINS)
    target_link_libraries (OpenImageIO ${PNG_LIBRARIES} ${TIFF_LIBRARIES}
                               ${JPEG_LIBRARIES} ${ZLIB_LIBRARIES}
                               ${FIELD3D_LIBRARY}
                               ${HDF5_LIBRARIES}
                               ${OPENJPEG_LIBRARIES}
                               ${WEBP_LIBRARY}
                               ${OPENJPEG_LIBRARIES}
                          )
    link_openexr (OpenImageIO)
endif ()

if (USE_EXTERNAL_PUGIXML)
    target_link_libraries (OpenImageIO ${PUGIXML_LIBRARIES})
endif ()

message(STATUS "Setting SOVERSION to: ${SOVERSION}")
set_target_properties(OpenImageIO
                         PROPERTIES
                         VERSION ${OIIO_VERSION_MAJOR}.${OIIO_VERSION_MINOR}.${OIIO_VERSION_PATCH}
                         SOVERSION ${SOVERSION}
                     )

# For consistency with the linux SpComp2s, create Mac OS X SpComp2s
# with a .so suffix instead of a .dylib suffix.
if(DEFINED OVERRIDE_SHARED_LIBRARY_SUFFIX)
  message(STATUS "Setting SUFFIX to: ${OVERRIDE_SHARED_LIBRARY_SUFFIX}")
  set_target_properties(OpenImageIO
                           PROPERTIES
                           SUFFIX ${OVERRIDE_SHARED_LIBRARY_SUFFIX}
                       )
endif(DEFINED OVERRIDE_SHARED_LIBRARY_SUFFIX)

oiio_install_targets (OpenImageIO)


# Testing

# Definition needed by boost_unit_test_framework library
# and by PTEX DLL
if (NOT LINKSTATIC)
    add_definitions (-DBOOST_TEST_DYN_LINK)
    add_definitions (-DPTEX_EXPORTS)
endif ()


if (OIIO_BUILD_TESTS)

add_executable (imagebuf_test imagebuf_test.cpp)
link_ilmbase (imagebuf_test)
target_link_libraries (imagebuf_test OpenImageIO ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
add_test (unit_imagebuf imagebuf_test)

add_executable (imagebufalgo_test imagebufalgo_test.cpp)
link_ilmbase (imagebufalgo_test)
target_link_libraries (imagebufalgo_test OpenImageIO ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
add_test (unit_imagebufalgo imagebufalgo_test)

add_executable (atomic_test atomic_test.cpp)
target_link_libraries (atomic_test OpenImageIO ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
add_test (unit_atomic atomic_test)

add_executable (spinlock_test spinlock_test.cpp)
target_link_libraries (spinlock_test OpenImageIO ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
add_test (unit_spinlock spinlock_test)

add_executable (spin_rw_test spin_rw_test.cpp)
target_link_libraries (spin_rw_test OpenImageIO ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
add_test (unit_spin_rw spin_rw_test)

add_executable (ustring_test ustring_test.cpp)
target_link_libraries (ustring_test OpenImageIO ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
add_test (unit_ustring ustring_test)

add_executable (imagespec_test imagespec_test.cpp)
target_link_libraries (imagespec_test OpenImageIO ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
add_test (unit_imagespec imagespec_test)

add_executable (strutil_test strutil_test.cpp)
target_link_libraries (strutil_test OpenImageIO ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
add_test (unit_strutil strutil_test)

add_executable (fmath_test fmath_test.cpp)
target_link_libraries (fmath_test OpenImageIO ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
add_test (unit_fmath fmath_test)

add_executable (filesystem_test filesystem_test.cpp)
target_link_libraries (filesystem_test OpenImageIO ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
add_test (unit_filesystem filesystem_test)

add_executable (optparser_test optparser_test.cpp)
target_link_libraries (optparser_test OpenImageIO ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
add_test (unit_optparser optparser_test)

add_executable (hash_test hash_test.cpp)
target_link_libraries (hash_test OpenImageIO ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
add_test (unit_hash hash_test)

add_executable (imagespeed_test imagespeed_test.cpp)
target_link_libraries (imagespeed_test OpenImageIO ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
#add_test (imagespeed_test imagespeed_test)

add_executable (timer_test timer_test.cpp)
target_link_libraries (timer_test OpenImageIO ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
add_test (timer_test timer_test)

endif (OIIO_BUILD_TESTS)
