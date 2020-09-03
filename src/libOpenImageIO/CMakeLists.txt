# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

if (VERBOSE)
   message (STATUS "Create imagio_pvt.h from imageio_pvt.h.in")
endif ()
configure_file (imageio_pvt.h.in "${CMAKE_BINARY_DIR}/include/imageio_pvt.h" @ONLY)

file (GLOB libOpenImageIO_hdrs ../include/OpenImageIO/*.h)

if (NOT USE_EXTERNAL_PUGIXML)
    list (APPEND libOpenImageIO_hdrs
          ../include/OpenImageIO/detail/pugixml/pugiconfig.hpp
          ../include/OpenImageIO/detail/pugixml/pugixml.hpp
          ../include/OpenImageIO/detail/pugixml/pugixml.cpp
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

if (CMAKE_COMPILER_IS_GNUCC AND ${GCC_VERSION} VERSION_GREATER_EQUAL 9.0)
    set_source_files_properties (../libutil/SHA1.cpp
                                 PROPERTIES COMPILE_FLAGS -Wno-stringop-truncation)
endif ()

if (CMAKE_COMPILER_IS_GNUCC AND ${GCC_VERSION} VERSION_GREATER_EQUAL 6.0
    AND ${GCC_VERSION} VERSION_LESS 7.0)
    set_source_files_properties (../openvdb.imageio/openvdbinput.cpp
                                 PROPERTIES COMPILE_FLAGS -Wno-error=strict-overflow)
endif ()

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


add_library (OpenImageIO ${libOpenImageIO_srcs})

# If the 'EMBEDPLUGINS' option is set, we want to compile the source for
# all the plugins into libOpenImageIO.
if (EMBEDPLUGINS)
    target_compile_definitions (OpenImageIO
                                PRIVATE
                                    EMBED_PLUGINS=1
                                    ${format_plugin_definitions})
    target_include_directories (OpenImageIO
                                PRIVATE ${format_plugin_include_dirs})

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

target_include_directories (OpenImageIO
                            PUBLIC
                                ${CMAKE_INSTALL_FULL_INCLUDEDIR}
                                ${ILMBASE_INCLUDES}
                                ${OpenCV_INCLUDES}
                            PRIVATE
                                ${ROBINMAP_INCLUDES}
                                ${FREETYPE_INCLUDE_DIRS}
                            )

if (NOT BUILD_SHARED_LIBS)
    target_compile_definitions (OpenImageIO PUBLIC OIIO_STATIC_DEFINE=1)
endif ()

target_link_libraries (OpenImageIO
        PUBLIC
            ${ILMBASE_LIBRARIES}
            ${OPENEXR_LIBRARIES}
            ${OpenCV_LIBRARIES}
            ${GCC_ATOMIC_LIBRARIES}
        PRIVATE
            ${SANITIZE_LIBRARIES}
            ${format_plugin_libs} # Add all the target link libraries from the plugins
            $<$<BOOL:${OpenColorIO_FOUND}>:OpenColorIO::OpenColorIO>
            ${BZIP2_LIBRARIES}
            ${ZLIB_LIBRARIES}
            ${Boost_LIBRARIES}
            ${CMAKE_DL_LIBS}
        )

if (USE_EXTERNAL_PUGIXML)
    if(TARGET pugixml)
        target_link_libraries (OpenImageIO PRIVATE pugixml)
    else()
        target_include_directories (OpenImageIO PRIVATE ${PUGIXML_INCLUDES})
        target_link_libraries (OpenImageIO PRIVATE ${PUGIXML_LIBRARIES})
    endif()
endif()

if (FREETYPE_FOUND)
    target_link_libraries (OpenImageIO PRIVATE ${FREETYPE_LIBRARIES})
endif()

if (WIN32)
    target_link_libraries (OpenImageIO PRIVATE psapi)
endif()

if (MINGW)
    target_link_libraries (OpenImageIO PRIVATE ws2_32)
endif()



if (MSVC)
    # In some MSVC setups, the IBA functions with huge template expansions
    # can run into trouble if the /bigobj flag is not used. Turn it on for
    # these files most likely to use it.
    # FIXME: Does anybody know if there is an advantage to limiting it to
    # just the files that most need it? Or is it better to make it totally
    # foolproof by using /bigobj for all our modules on Windows?
    file (GLOB iba_sources "imagebufalgo_*.cpp")
    set_property (SOURCE imagebuf.cpp ${iba_sources}
                  APPEND_STRING PROPERTY COMPILE_FLAGS " /bigobj ")
endif ()

set_target_properties(OpenImageIO
                      PROPERTIES
                         VERSION     ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
                         SOVERSION   ${SOVERSION}
                         OUTPUT_NAME OpenImageIO${OIIO_LIBNAME_SUFFIX}
                         POSITION_INDEPENDENT_CODE ON
                     )
if (VISIBILITY_MAP_COMMAND)
    set_property (TARGET OpenImageIO
                  APPEND PROPERTY LINK_FLAGS ${VISIBILITY_MAP_COMMAND})
endif ()

# For consistency with the linux SpComp2s, create Mac OS X SpComp2s
# with a .so suffix instead of a .dylib suffix.
if (DEFINED OVERRIDE_SHARED_LIBRARY_SUFFIX)
  if (VERBOSE)
      message(STATUS "Setting SUFFIX to: ${OVERRIDE_SHARED_LIBRARY_SUFFIX}")
  endif ()
  set_target_properties(OpenImageIO
                           PROPERTIES
                           SUFFIX ${OVERRIDE_SHARED_LIBRARY_SUFFIX}
                       )
endif ()

if (EXTRA_DSO_LINK_ARGS)
    set_target_properties (OpenImageIO PROPERTIES LINK_FLAGS ${EXTRA_DSO_LINK_ARGS})
endif()

install_targets (OpenImageIO)


# Testing

if (OIIO_BUILD_TESTS)

    add_executable (imagebuf_test imagebuf_test.cpp)
    target_link_libraries (imagebuf_test PRIVATE OpenImageIO)
    set_target_properties (imagebuf_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_imagebuf ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imagebuf_test)

    add_executable (imagecache_test imagecache_test.cpp)
    target_link_libraries (imagecache_test PRIVATE OpenImageIO)
    set_target_properties (imagecache_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_imagecache ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imagecache_test)

    add_executable (imagebufalgo_test imagebufalgo_test.cpp)
    target_link_libraries (imagebufalgo_test PRIVATE OpenImageIO)
    set_target_properties (imagebufalgo_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_imagebufalgo ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imagebufalgo_test)

    add_executable (imagespec_test imagespec_test.cpp)
    target_link_libraries (imagespec_test PRIVATE OpenImageIO)
    set_target_properties (imagespec_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_imagespec ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imagespec_test)

    add_executable (imageinout_test imageinout_test.cpp)
    target_link_libraries (imageinout_test PRIVATE OpenImageIO)
    set_target_properties (imageinout_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_imageinout ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imageinout_test)

    add_executable (imagespeed_test imagespeed_test.cpp)
    target_link_libraries (imagespeed_test PRIVATE OpenImageIO)
    set_target_properties (imagespeed_test PROPERTIES FOLDER "Unit Tests")
    #add_test (imagespeed_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imagespeed_test)

    add_executable (compute_test compute_test.cpp)
    target_link_libraries (compute_test PRIVATE OpenImageIO)
    set_target_properties (compute_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_compute ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/compute_test)

endif (OIIO_BUILD_TESTS)
