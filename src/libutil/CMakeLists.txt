# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

set (libOpenImageIO_Util_srcs argparse.cpp benchmark.cpp
                  errorhandler.cpp farmhash.cpp filesystem.cpp
                  fmath.cpp filter.cpp hashes.cpp paramlist.cpp
                  plugin.cpp SHA1.cpp
                  strutil.cpp sysutil.cpp thread.cpp timer.cpp
                  typedesc.cpp ustring.cpp xxhash.cpp)

if (CMAKE_COMPILER_IS_GNUCC AND NOT ${GCC_VERSION} VERSION_LESS 9.0)
    set_property (SOURCE SHA1.cpp
                  APPEND PROPERTY COMPILE_OPTIONS -Wno-stringop-truncation)
endif ()

if (CMAKE_UNITY_BUILD)
    # If unity builds are occurring, fmath.cpp MUST be compiled alone. But the
    # rest can be built together if in group mode.
    set_property (SOURCE fmath.cpp APPEND PROPERTY SKIP_UNITY_BUILD_INCLUSION TRUE)
    set_source_files_properties (${libOpenImageIO_Util_srcs}
                                 PROPERTIES UNITY_GROUP utilsrc)
endif ()


set (OpenImageIO_Util_LINK_FLAGS "${VISIBILITY_MAP_COMMAND} ${EXTRA_DSO_LINK_ARGS}")
if (UNIX AND NOT APPLE)
    # Hide symbols from any static dependent libraries embedded here.
    set (OpenImageIO_Util_LINK_FLAGS "${OpenImageIO_Util_LINK_FLAGS} -Wl,--exclude-libs,ALL")
endif ()


function (setup_oiio_util_library targetname)
    if (BUILD_SHARED_LIBS AND NOT targetname MATCHES "static")
        set (libtype SHARED)
    else ()
        set (libtype STATIC)
    endif ()
    
    if (WIN32)
        configure_file(../build-scripts/version_win32.rc.in "${CMAKE_CURRENT_BINARY_DIR}/version_win32.rc" @ONLY)
        add_library (${targetname} ${libtype} ${libOpenImageIO_Util_srcs} ${CMAKE_CURRENT_BINARY_DIR}/version_win32.rc)
    else ()
        add_library (${targetname} ${libtype} ${libOpenImageIO_Util_srcs})
    endif ()

    target_compile_definitions(${targetname} PRIVATE
                               ${${PROJECT_NAME}_compile_definitions})
    target_compile_options(${targetname} PRIVATE
                               ${${PROJECT_NAME}_compile_options})
    target_link_options(${targetname} PRIVATE
                               ${${PROJECT_NAME}_link_options})

    target_include_directories (${targetname}
            PUBLIC
                $<BUILD_INTERFACE:${OpenImageIO_LOCAL_DEPS_ROOT}/include>
                $<INSTALL_INTERFACE:include>
            PRIVATE
                ${ROBINMAP_INCLUDES}
            )
    target_link_libraries (${targetname}
            PUBLIC
                $<TARGET_NAME_IF_EXISTS:Threads::Threads>
                ${GCC_ATOMIC_LIBRARIES}
                ${OPENIMAGEIO_IMATH_DEPENDENCY_VISIBILITY}
                ${OPENIMAGEIO_IMATH_TARGETS}
            PRIVATE
                $<TARGET_NAME_IF_EXISTS:TBB::tbb>
                ${CMAKE_DL_LIBS}
            )

    if (GCC_VERSION VERSION_GREATER_EQUAL 12.0
          AND GCC_VERSION VERSION_LESS 14.0
          AND CMAKE_CXX_STANDARD VERSION_GREATER_EQUAL 23)
        target_link_libraries (${targetname} PRIVATE stdc++_libbacktrace)
    elseif (GCC_VERSION VERSION_GREATER_EQUAL 14.0
            AND CMAKE_CXX_STANDARD VERSION_GREATER_EQUAL 23)
        target_link_libraries (${targetname} PRIVATE stdc++exp)
    endif ()

    if (OIIO_INTERNALIZE_FMT OR fmt_LOCAL_BUILD)
        add_dependencies(${targetname} fmt_internal_target)
    else ()
        target_link_libraries (${targetname}
                               PUBLIC fmt::fmt-header-only)
    endif ()

    if (WIN32)
        target_compile_definitions(${targetname} PRIVATE
                                   WIN32_LEAN_AND_MEAN NOMINMAX
                                   NOGDI VC_EXTRALEAN)
        target_link_libraries (${targetname} PRIVATE psapi)
    endif()

    target_compile_definitions (${targetname} PRIVATE OpenImageIO_EXPORTS)


    if (NOT BUILD_SHARED_LIBS OR targetname MATCHES "static")
        target_compile_definitions (${targetname} PUBLIC OIIO_STATIC_DEFINE=1)
    endif ()

    # Propagate C++ minimum to downstream consumers
    target_compile_features (${targetname}
                             INTERFACE cxx_std_${DOWNSTREAM_CXX_STANDARD})

    set_target_properties(${targetname}
                          PROPERTIES
                             VERSION     ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
                             SOVERSION   ${SOVERSION}
                             OUTPUT_NAME ${targetname}${OIIO_LIBNAME_SUFFIX}
                             POSITION_INDEPENDENT_CODE ON
                         )

    set_target_properties (${targetname} PROPERTIES
                           LINK_FLAGS ${OpenImageIO_Util_LINK_FLAGS})
    
    if (SKBUILD)
        set (PYTHON_SITE_DIR .)
        install_targets (NAMELINK_SKIP ${targetname})
    else ()
        install_targets (${targetname})
    endif ()

endfunction()


setup_oiio_util_library (OpenImageIO_Util)

option (OpenImageIO_BUILD_STATIC_UTIL_LIBRARY
        "Build additional static OpenImageIO_Util_static library" OFF)
if (OpenImageIO_BUILD_STATIC_UTIL_LIBRARY)
    setup_oiio_util_library (OpenImageIO_Util_static)
endif ()


if (OIIO_BUILD_TESTS AND BUILD_TESTING)

    fancy_add_executable (NAME argparse_test SRC argparse_test.cpp
                          NO_INSTALL  FOLDER "Unit Tests"
                          LINK_LIBRARIES OpenImageIO_Util)
    add_test (unit_argparse ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/argparse_test)

    fancy_add_executable (NAME atomic_test SRC atomic_test.cpp
                          NO_INSTALL  FOLDER "Unit Tests"
                          LINK_LIBRARIES OpenImageIO_Util)
    add_test (unit_atomic ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/atomic_test)

    fancy_add_executable (NAME span_test SRC span_test.cpp
                          NO_INSTALL  FOLDER "Unit Tests"
                          LINK_LIBRARIES OpenImageIO_Util)
    add_test (unit_span ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/span_test)

    fancy_add_executable (NAME spinlock_test SRC spinlock_test.cpp
                          NO_INSTALL  FOLDER "Unit Tests"
                          LINK_LIBRARIES OpenImageIO_Util)
    add_test (unit_spinlock ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/spinlock_test)

    fancy_add_executable (NAME spin_rw_test SRC spin_rw_test.cpp
                          NO_INSTALL  FOLDER "Unit Tests"
                          LINK_LIBRARIES OpenImageIO_Util)
    add_test (unit_spin_rw ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/spin_rw_test)

    fancy_add_executable (NAME ustring_test SRC ustring_test.cpp
                          NO_INSTALL  FOLDER "Unit Tests"
                          LINK_LIBRARIES OpenImageIO_Util)
    add_test (unit_ustring ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ustring_test)

    fancy_add_executable (NAME strutil_test SRC strutil_test.cpp
                          NO_INSTALL  FOLDER "Unit Tests"
                          LINK_LIBRARIES OpenImageIO_Util)
    add_test (unit_strutil ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/strutil_test)

    fancy_add_executable (NAME fmath_test SRC fmath_test.cpp
                          NO_INSTALL  FOLDER "Unit Tests"
                          LINK_LIBRARIES OpenImageIO_Util
                                         ${OPENIMAGEIO_IMATH_TARGETS})
    add_test (unit_fmath ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/fmath_test)

    fancy_add_executable (NAME filesystem_test SRC filesystem_test.cpp
                          NO_INSTALL  FOLDER "Unit Tests"
                          LINK_LIBRARIES OpenImageIO_Util)
    add_test (unit_filesystem ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/filesystem_test)

    fancy_add_executable (NAME optparser_test SRC optparser_test.cpp
                          NO_INSTALL  FOLDER "Unit Tests"
                          LINK_LIBRARIES OpenImageIO_Util)
    add_test (unit_optparser ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/optparser_test)

    fancy_add_executable (NAME hash_test SRC hash_test.cpp
                          NO_INSTALL  FOLDER "Unit Tests"
                          LINK_LIBRARIES OpenImageIO_Util)
    add_test (unit_hash ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/hash_test)

    fancy_add_executable (NAME parallel_test SRC parallel_test.cpp
                          NO_INSTALL  FOLDER "Unit Tests"
                          LINK_LIBRARIES OpenImageIO_Util)
    add_test (unit_parallel ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/parallel_test)

    fancy_add_executable (NAME timer_test SRC timer_test.cpp
                          NO_INSTALL  FOLDER "Unit Tests"
                          LINK_LIBRARIES OpenImageIO_Util)
    add_test (unit_timer ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/timer_test)

    fancy_add_executable (NAME thread_test SRC thread_test.cpp
                          NO_INSTALL  FOLDER "Unit Tests"
                          LINK_LIBRARIES OpenImageIO_Util)
    add_test (unit_thread ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/thread_test)

    fancy_add_executable (NAME simd_test SRC simd_test.cpp
                          NO_INSTALL  FOLDER "Unit Tests"
                          LINK_LIBRARIES OpenImageIO
                                         ${OPENIMAGEIO_IMATH_TARGETS})
    add_test (unit_simd ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/simd_test)

    fancy_add_executable (NAME filter_test SRC filter_test.cpp
                          NO_INSTALL  FOLDER "Unit Tests"
                          LINK_LIBRARIES OpenImageIO)
    add_test (unit_filter ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/filter_test)

    fancy_add_executable (NAME paramlist_test SRC paramlist_test.cpp
                          NO_INSTALL  FOLDER "Unit Tests"
                          LINK_LIBRARIES OpenImageIO_Util
                                         ${OPENIMAGEIO_IMATH_TARGETS})
    add_test (unit_paramlist ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/paramlist_test)

    fancy_add_executable (NAME strongparam_test SRC strongparam_test.cpp
                          NO_INSTALL  FOLDER "Unit Tests"
                          LINK_LIBRARIES OpenImageIO_Util)
    add_test (unit_strongparam ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/strongparam_test)

    fancy_add_executable (NAME typedesc_test SRC typedesc_test.cpp
                          NO_INSTALL  FOLDER "Unit Tests"
                          LINK_LIBRARIES OpenImageIO_Util
                                         ${OPENIMAGEIO_OPENEXR_TARGETS})
    add_test (unit_typedesc ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/typedesc_test)

    fancy_add_executable (NAME type_traits_test SRC type_traits_test.cpp
                          NO_INSTALL  FOLDER "Unit Tests"
                          LINK_LIBRARIES OpenImageIO_Util
                                         ${OPENIMAGEIO_OPENEXR_TARGETS})
    add_test (unit_type_traits ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/type_traits_test)

endif ()
