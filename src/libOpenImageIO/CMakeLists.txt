if (VERBOSE)
   message (STATUS "Create imagio_pvt.h from imageio_pvt.h.in")
endif ()
file (TO_NATIVE_PATH "${PLUGIN_SEARCH_PATH}" PLUGIN_SEARCH_PATH_NATIVE)
configure_file (imageio_pvt.h.in "${CMAKE_CURRENT_BINARY_DIR}/imageio_pvt.h" @ONLY)
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

file (GLOB libOpenImageIO_hdrs ../include/OpenImageIO/*.h)

if (NOT USE_EXTERNAL_PUGIXML)
    list (APPEND libOpenImageIO_hdrs
          ../include/OpenImageIO/pugiconfig.hpp
          ../include/OpenImageIO/pugixml.hpp
          ../include/OpenImageIO/pugixml.cpp
    )
    if (CMAKE_COMPILER_IS_GNUCC AND NOT ${GCC_VERSION} VERSION_LESS 6.0)
        set_source_files_properties (formatspec.cpp xmp.cpp
                                     PROPERTIES COMPILE_FLAGS -Wno-error=placement-new)
    endif ()
endif()

# Make the build complete for newer ffmpeg versions (3.1.1+) that have
# marked m_format_context->streams[i]->codec as deprecated.
# FIXME -- at some point, come back and figure out how to fix for real
# before the field disappears entirely.
if (NOT MSVC)
    set_source_files_properties (../ffmpeg.imageio/ffmpeginput.cpp
                             PROPERTIES COMPILE_FLAGS "-Wno-deprecated-declarations")
endif()

set (libOpenImageIO_srcs
                          imagebufalgo.cpp
                          imagebufalgo_pixelmath.cpp
                          imagebufalgo_channels.cpp
                          imagebufalgo_compare.cpp
                          imagebufalgo_copy.cpp
                          imagebufalgo_deep.cpp
                          imagebufalgo_draw.cpp
                          imagebufalgo_addsub.cpp
                          imagebufalgo_muldiv.cpp
                          imagebufalgo_mad.cpp
                          imagebufalgo_orient.cpp
                          imagebufalgo_xform.cpp
                          imagebufalgo_yee.cpp imagebufalgo_opencv.cpp
                          deepdata.cpp exif.cpp exif-canon.cpp
                          formatspec.cpp imagebuf.cpp
                          imageinput.cpp imageio.cpp imageioplugin.cpp
                          imageoutput.cpp iptc.cpp xmp.cpp
                          color_ocio.cpp
                          maketexture.cpp
                          ../libutil/argparse.cpp
                          ../libutil/benchmark.cpp
                          ../libutil/errorhandler.cpp 
                          ../libutil/filesystem.cpp 
                          ../libutil/farmhash.cpp 
                          ../libutil/filter.cpp 
                          ../libutil/hashes.cpp 
                          ../libutil/paramlist.cpp 
                          ../libutil/plugin.cpp 
                          ../libutil/SHA1.cpp 
                          ../libutil/strutil.cpp 
                          ../libutil/sysutil.cpp 
                          ../libutil/thread.cpp 
                          ../libutil/timer.cpp 
                          ../libutil/typedesc.cpp 
                          ../libutil/ustring.cpp 
                          ../libutil/xxhash.cpp 
                          ../libtexture/texturesys.cpp 
                          ../libtexture/texture3d.cpp 
                          ../libtexture/environment.cpp 
                          ../libtexture/texoptions.cpp 
                          ../libtexture/imagecache.cpp
                          ${libOpenImageIO_srcs}
                          ${libOpenImageIO_hdrs}
                         )


# If the 'EMBEDPLUGINS' option is set, we want to compile the source for
# all the plugins into libOpenImageIO.
if (EMBEDPLUGINS)
    add_definitions ("-DEMBED_PLUGINS=1"
                     ${format_plugin_definitions})
    include_directories (${format_plugin_include_dirs})

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
endif ()

# Source groups for libutil and libtexture
source_group ("libutil"    REGULAR_EXPRESSION ".+/libutil/.+")
source_group ("libtexture" REGULAR_EXPRESSION ".+/libtexture/.+")

if (BUILDSTATIC)
    add_library (OpenImageIO STATIC ${libOpenImageIO_srcs})
else ()
    add_library (OpenImageIO SHARED ${libOpenImageIO_srcs})
endif ()

# if (SANITIZE AND ${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
#     # target_link_libraries (OpenImageIO "asan" "ubsan")
#     target_link_libraries (OpenImageIO ${SANITIZE_LIBRARIES})
# endif ()

target_link_libraries (OpenImageIO
                       ${SANITIZE_LIBRARIES}
                       ${format_plugin_libs} # Add all the target link libraries from the plugins
                       ${Boost_LIBRARIES})

include_directories (${ROBINMAP_INCLUDE_DIR})

# Include OpenColorIO if using it
if (USE_OCIO AND OCIO_FOUND)
    if (VERBOSE)
        message (STATUS "Linking OpenColorIO ${OCIO_LIBRARIES}")
    endif ()
    target_link_libraries (OpenImageIO ${OCIO_LIBRARIES})
endif ()

# Include OpenCV if using it
if (OpenCV_FOUND)
    include_directories (${OpenCV_INCLUDE_DIR})
    target_link_libraries (OpenImageIO ${OpenCV_LIBS})
endif ()

# Include OpenSSL if using it
if (OPENSSL_FOUND)
    include_directories (${OPENSSL_INCLUDE_DIR})
    target_link_libraries (OpenImageIO ${OPENSSL_LIBRARIES})
endif ()

# Include Freetype if using it
if (FREETYPE_FOUND)
    include_directories (${FREETYPE_INCLUDE_DIRS})
    target_link_libraries (OpenImageIO ${FREETYPE_LIBRARIES} ${BZIP2_LIBRARIES})
endif ()

# Include GDCM if using it
if (GDCM_FOUND)
    include_directories (${GDCM_INCLUDE_DIRS})
    target_link_libraries (OpenImageIO ${GDCM_LIBRARIES})
    # set_property(SOURCE dicominput.cpp APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-macro-redefined ")
    set_property(SOURCE ../dicom.imageio/dicominput.cpp APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-macro-redefined ")
endif ()


if (MSVC)
    set_property (SOURCE imagebufalgo_pixelmath.cpp APPEND_STRING PROPERTY COMPILE_FLAGS " /bigobj ")
endif ()

if (WIN32)
    target_link_libraries (OpenImageIO psapi.lib)
endif ()

if (VISIBILITY_MAP_FILE)
    add_dependencies (OpenImageIO "${VISIBILITY_MAP_FILE}")
endif ()

if (USE_EXTERNAL_PUGIXML)
    target_link_libraries (OpenImageIO ${PUGIXML_LIBRARIES})
endif ()

target_link_libraries (OpenImageIO ${ILMBASE_LIBRARIES})
target_link_libraries (OpenImageIO ${OPENEXR_LIBRARIES})
target_link_libraries (OpenImageIO ${ZLIB_LIBRARIES})

if (VERBOSE)
    message(STATUS "Setting SOVERSION to: ${SOVERSION}")
endif ()
set_target_properties(OpenImageIO
                         PROPERTIES
                         VERSION ${OIIO_VERSION_MAJOR}.${OIIO_VERSION_MINOR}.${OIIO_VERSION_PATCH}
                         SOVERSION ${SOVERSION}
                     )
if (VISIBILITY_COMMAND)
    set_target_properties (OpenImageIO
                           PROPERTIES
                           LINK_FLAGS "${VISIBILITY_COMMAND} ${VISIBILITY_MAP_COMMAND}")
endif ()

# For consistency with the linux SpComp2s, create Mac OS X SpComp2s
# with a .so suffix instead of a .dylib suffix.
if(DEFINED OVERRIDE_SHARED_LIBRARY_SUFFIX)
  if (VERBOSE)
      message(STATUS "Setting SUFFIX to: ${OVERRIDE_SHARED_LIBRARY_SUFFIX}")
  endif ()
  set_target_properties(OpenImageIO
                           PROPERTIES
                           SUFFIX ${OVERRIDE_SHARED_LIBRARY_SUFFIX}
                       )
endif(DEFINED OVERRIDE_SHARED_LIBRARY_SUFFIX)

if (EXTRA_DSO_LINK_ARGS)
    set_target_properties (OpenImageIO PROPERTIES LINK_FLAGS ${EXTRA_DSO_LINK_ARGS})
endif()

oiio_install_targets (OpenImageIO)


# Testing

if (OIIO_BUILD_TESTS)

    add_executable (imagebuf_test imagebuf_test.cpp)
    set_target_properties (imagebuf_test PROPERTIES FOLDER "Unit Tests")
    target_link_libraries (imagebuf_test OpenImageIO ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
    add_test (unit_imagebuf imagebuf_test)

    add_executable (imagecache_test imagecache_test.cpp)
    set_target_properties (imagecache_test PROPERTIES FOLDER "Unit Tests")
    target_link_libraries (imagecache_test OpenImageIO ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
    add_test (unit_imagecache imagecache_test)

    add_executable (imagebufalgo_test imagebufalgo_test.cpp)
    set_target_properties (imagebufalgo_test PROPERTIES FOLDER "Unit Tests")
    target_link_libraries (imagebufalgo_test OpenImageIO ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
    add_test (unit_imagebufalgo imagebufalgo_test)

    add_executable (imagespec_test imagespec_test.cpp)
    set_target_properties (imagespec_test PROPERTIES FOLDER "Unit Tests")
    target_link_libraries (imagespec_test OpenImageIO ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
    add_test (unit_imagespec imagespec_test)
    
    add_executable (imagespeed_test imagespeed_test.cpp)
    set_target_properties (imagespeed_test PROPERTIES FOLDER "Unit Tests")
    target_link_libraries (imagespeed_test OpenImageIO ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
    #add_test (imagespeed_test imagespeed_test)

    add_executable (compute_test compute_test.cpp)
    set_target_properties (compute_test PROPERTIES FOLDER "Unit Tests")
    target_link_libraries (compute_test OpenImageIO ${Boost_LIBRARIES}
                           ${CMAKE_DL_LIBS})
    add_test (unit_compute compute_test)

endif (OIIO_BUILD_TESTS)
