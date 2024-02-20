# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

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
                          imagebufalgo_minmaxchan.cpp
                          imagebufalgo_orient.cpp
                          imagebufalgo_xform.cpp
                          imagebufalgo_yee.cpp imagebufalgo_opencv.cpp
                          deepdata.cpp exif.cpp exif-canon.cpp
                          formatspec.cpp
                          icc.cpp imagebuf.cpp
                          imageinput.cpp imageio.cpp imageioplugin.cpp
                          imageoutput.cpp
                          iptc.cpp xmp.cpp
                          color_ocio.cpp
                          maketexture.cpp
                          bluenoise.cpp
                          printinfo.cpp
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

# Enable default use of OpenEXR core library for versions of the library
# new enough to support DWA compression in exrcore.
if (OPENEXR_VERSION VERSION_GREATER_EQUAL 3.1.10)
    target_compile_definitions(OpenImageIO
                               PRIVATE OIIO_OPENEXR_CORE_DEFAULT=1)
    message (STATUS "OpenEXR core library will be used by default")
else ()
    target_compile_definitions(OpenImageIO
                               PRIVATE OIIO_OPENEXR_CORE_DEFAULT=0)
    message (STATUS "OpenEXR core library will not be used by default")
endif ()

# Some extra definitions we need to retrieve build related attributes.
target_compile_definitions(OpenImageIO
              PRIVATE
                  OIIO_FREETYPE_VERSION="${FREETYPE_VERSION_STRING}"
                  OIIO_OpenCV_VERSION="${OpenCV_VERSION}"
                  OIIO_PYTHON_VERSION="${Python_VERSION}"
                  OIIO_QT_VERSION="${Qt6_VERSION}${Qt5_VERSION}"
                  OIIO_TBB_VERSION="${TBB_VERSION}"
             )

# Source groups for libutil and libtexture
source_group ("libutil"    REGULAR_EXPRESSION ".+/libutil/.+")
source_group ("libtexture" REGULAR_EXPRESSION ".+/libtexture/.+")

target_include_directories (OpenImageIO
                            PUBLIC
                                $<INSTALL_INTERFACE:include>
                            ${OPENIMAGEIO_IMATH_DEPENDENCY_VISIBILITY}
                                ${OPENEXR_INCLUDES}
                            PRIVATE
                                ${ROBINMAP_INCLUDES}
                            )
target_include_directories (OpenImageIO SYSTEM PUBLIC ${OpenCV_INCLUDES})

if (NOT BUILD_SHARED_LIBS)
    target_compile_definitions (OpenImageIO PUBLIC OIIO_STATIC_DEFINE=1)
endif ()

# Propagate C++ minimum to downstream
target_compile_features (OpenImageIO
                         INTERFACE cxx_std_${DOWNSTREAM_CXX_STANDARD})

target_link_libraries (OpenImageIO
        PUBLIC
            OpenImageIO_Util
            ${OPENIMAGEIO_IMATH_DEPENDENCY_VISIBILITY}
            ${OPENIMAGEIO_IMATH_TARGETS}
        PRIVATE
            ${OPENIMAGEIO_OPENEXR_TARGETS}
            ${OpenCV_LIBRARIES}
            ${format_plugin_libs} # Add all the target link libraries from the plugins
            $<TARGET_NAME_IF_EXISTS:OpenColorIO::OpenColorIO>
            $<TARGET_NAME_IF_EXISTS:OpenColorIO::OpenColorIOHeaders>
            $<TARGET_NAME_IF_EXISTS:pugixml::pugixml>
            $<TARGET_NAME_IF_EXISTS:TBB::tbb>
            $<TARGET_NAME_IF_EXISTS:Freetype::Freetype>
            ${BZIP2_LIBRARIES}
            ZLIB::ZLIB
            $<TARGET_NAME_IF_EXISTS:Boost::thread>
            ${CMAKE_DL_LIBS}
        )

if (WIN32)
    target_link_libraries (OpenImageIO PRIVATE psapi)
endif()

if (MINGW)
    target_link_libraries (OpenImageIO PRIVATE ws2_32)
endif()

file (GLOB iba_sources "imagebufalgo_*.cpp")
if (MSVC)
    # In some MSVC setups, the IBA functions with huge template expansions
    # can run into trouble if the /bigobj flag is not used. Turn it on for
    # these files most likely to use it.
    # FIXME: Does anybody know if there is an advantage to limiting it to
    # just the files that most need it? Or is it better to make it totally
    # foolproof by using /bigobj for all our modules on Windows?
    set_property (SOURCE imagebuf.cpp ${iba_sources}
                  APPEND_STRING PROPERTY COMPILE_FLAGS " /bigobj ")
endif ()

if (CMAKE_UNITY_BUILD)
    # If doing a unity build in group mode, make each plugin dir one group (and
    # the texture library one group), and make the rest of the non-IBA module one
    # group together. Regardless, leave IBA modules to compile indidividually
    # because they tend to be very expensive.
    set_source_files_properties (
        # ${libOpenImageIO_srcs}
            deepdata.cpp exif.cpp exif-canon.cpp formatspec.cpp imagebuf.cpp
            imageinput.cpp imageio.cpp imageioplugin.cpp imageoutput.cpp
            iptc.cpp xmp.cpp color_ocio.cpp maketexture.cpp bluenoise.cpp
        PROPERTIES
            UNITY_GROUP oiiolib)
    foreach (plugin_dir ${all_format_plugin_dirs} ../libtexture)
        file (GLOB plugin_dir_sources ${plugin_dir}/*.cpp ${plugin_dir}/*/*.cpp)
        get_filename_component (_group_name ${plugin_dir} NAME_WE)
        set_source_files_properties (${plugin_dir_sources} PROPERTIES
                                     UNITY_GROUP ${_group_name})
        if (VERBOSE)
            message (STATUS "Unity group ${_group_name} = ${plugin_dir_sources}")
        endif ()
    endforeach ()
    set_property (SOURCE ${iba_sources} APPEND PROPERTY SKIP_UNITY_BUILD_INCLUSION TRUE)
endif ()

set_target_properties(OpenImageIO
                      PROPERTIES
                         VERSION     ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
                         SOVERSION   ${SOVERSION}
                         OUTPUT_NAME OpenImageIO${OIIO_LIBNAME_SUFFIX}
                         POSITION_INDEPENDENT_CODE ON
                        # UNITY_BUILD_BATCH_SIZE 4
                     )

set (OpenImageIO_LINK_FLAGS "${VISIBILITY_MAP_COMMAND} ${EXTRA_DSO_LINK_ARGS}")
if (UNIX AND NOT APPLE)
    # Hide symbols from any static dependent libraries embedded here.
    set (OpenImageIO_LINK_FLAGS "${OpenImageIO_LINK_FLAGS} -Wl,--exclude-libs,ALL")
endif ()
set_target_properties (OpenImageIO PROPERTIES LINK_FLAGS ${OpenImageIO_LINK_FLAGS})

install_targets (OpenImageIO)


# Testing

if (OIIO_BUILD_TESTS AND BUILD_TESTING)

    add_executable (color_test color_test.cpp)
    target_link_libraries (color_test PRIVATE OpenImageIO)
    set_target_properties (color_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_color ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/color_test)

    add_executable (imagebuf_test imagebuf_test.cpp)
    target_link_libraries (imagebuf_test PRIVATE OpenImageIO)
    set_target_properties (imagebuf_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_imagebuf ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imagebuf_test)

    add_executable (imagecache_test imagecache_test.cpp)
    target_link_libraries (imagecache_test PRIVATE OpenImageIO)
    set_target_properties (imagecache_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_imagecache ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imagecache_test)

    add_executable (imagebufalgo_test imagebufalgo_test.cpp)
    target_link_libraries (imagebufalgo_test PRIVATE OpenImageIO ${OpenCV_LIBRARIES})
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

    if (NOT DEFINED ENV{OpenImageIO_CI})
        add_executable (imagespeed_test imagespeed_test.cpp)
        target_link_libraries (imagespeed_test PRIVATE OpenImageIO)
        set_target_properties (imagespeed_test PROPERTIES FOLDER "Unit Tests")
        #add_test (imagespeed_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/imagespeed_test)
    endif ()

    add_executable (compute_test compute_test.cpp)
    target_link_libraries (compute_test PRIVATE OpenImageIO)
    set_target_properties (compute_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_compute ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/compute_test)

endif ()
