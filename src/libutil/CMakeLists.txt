# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

set (libOpenImageIO_Util_srcs argparse.cpp benchmark.cpp
                  errorhandler.cpp farmhash.cpp filesystem.cpp
                  fmath.cpp filter.cpp hashes.cpp paramlist.cpp
                  plugin.cpp SHA1.cpp
                  strutil.cpp sysutil.cpp thread.cpp timer.cpp
                  typedesc.cpp ustring.cpp xxhash.cpp)

add_library (OpenImageIO_Util ${libOpenImageIO_Util_srcs})
target_include_directories (OpenImageIO_Util
        PUBLIC
            $<INSTALL_INTERFACE:include>
        )
target_link_libraries (OpenImageIO_Util
        PUBLIC
            $<TARGET_NAME_IF_EXISTS:Threads::Threads>
            ${GCC_ATOMIC_LIBRARIES}
            ${OPENIMAGEIO_IMATH_DEPENDENCY_VISIBILITY}
            ${OPENIMAGEIO_IMATH_TARGETS}
        PRIVATE
            $<TARGET_NAME_IF_EXISTS:Boost::filesystem>
            $<TARGET_NAME_IF_EXISTS:Boost::thread>
            $<TARGET_NAME_IF_EXISTS:TBB::tbb>
            ${CMAKE_DL_LIBS}
        )

if (INTERNALIZE_FMT OR OIIO_USING_FMT_LOCAL)
    add_dependencies(OpenImageIO_Util fmt_internal_target)
else ()
    target_link_libraries (OpenImageIO_Util
                           PUBLIC fmt::fmt-header-only)
endif ()

if (WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX -DNOGDI -DVC_EXTRALEAN)
    target_link_libraries (OpenImageIO_Util PRIVATE psapi)
endif()

target_compile_definitions (OpenImageIO_Util PRIVATE OpenImageIO_EXPORTS)
if (NOT BUILD_SHARED_LIBS)
    target_compile_definitions (OpenImageIO_Util PUBLIC OIIO_STATIC_DEFINE=1)
endif ()

if (OIIO_DISABLE_BOOST_STACKTRACE)
    target_compile_definitions (OpenImageIO_Util PRIVATE OIIO_DISABLE_BOOST_STACKTRACE)
endif ()

# Propagate C++ minimum to downstream consumers
target_compile_features (OpenImageIO_Util
                         INTERFACE cxx_std_${DOWNSTREAM_CXX_STANDARD})

set_target_properties(OpenImageIO_Util
                      PROPERTIES
                         VERSION     ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
                         SOVERSION   ${SOVERSION}
                         OUTPUT_NAME OpenImageIO_Util${OIIO_LIBNAME_SUFFIX}
                         POSITION_INDEPENDENT_CODE ON
                     )

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
set_target_properties (OpenImageIO_Util PROPERTIES LINK_FLAGS ${OpenImageIO_Util_LINK_FLAGS})

install_targets (OpenImageIO_Util)



if (OIIO_BUILD_TESTS AND BUILD_TESTING)

    add_executable (argparse_test argparse_test.cpp)
    target_link_libraries (argparse_test PRIVATE OpenImageIO_Util)
    set_target_properties (argparse_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_argparse ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/argparse_test)

    add_executable (atomic_test atomic_test.cpp)
    target_link_libraries (atomic_test PRIVATE OpenImageIO_Util)
    set_target_properties (atomic_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_atomic ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/atomic_test)

    add_executable (span_test span_test.cpp)
    target_link_libraries (span_test PRIVATE OpenImageIO_Util)
    set_target_properties (span_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_span ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/span_test)

    add_executable (spinlock_test spinlock_test.cpp)
    target_link_libraries (spinlock_test PRIVATE OpenImageIO_Util)
    set_target_properties (spinlock_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_spinlock ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/spinlock_test)

    add_executable (spin_rw_test spin_rw_test.cpp)
    target_link_libraries (spin_rw_test PRIVATE OpenImageIO_Util)
    set_target_properties (spin_rw_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_spin_rw ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/spin_rw_test)

    add_executable (ustring_test ustring_test.cpp)
    target_link_libraries (ustring_test PRIVATE OpenImageIO_Util)
    set_target_properties (ustring_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_ustring ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ustring_test)

    add_executable (strutil_test strutil_test.cpp)
    target_link_libraries (strutil_test PRIVATE OpenImageIO)
    set_target_properties (strutil_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_strutil ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/strutil_test)

    add_executable (fmath_test fmath_test.cpp)
    target_link_libraries (fmath_test PRIVATE OpenImageIO_Util
                                              ${OPENIMAGEIO_IMATH_TARGETS})
    set_target_properties (fmath_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_fmath ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/fmath_test)

    add_executable (filesystem_test filesystem_test.cpp)
    target_link_libraries (filesystem_test PRIVATE OpenImageIO_Util)
    set_target_properties (filesystem_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_filesystem ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/filesystem_test)

    add_executable (optparser_test optparser_test.cpp)
    target_link_libraries (optparser_test PRIVATE OpenImageIO_Util)
    set_target_properties (optparser_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_optparser ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/optparser_test)

    add_executable (hash_test hash_test.cpp)
    target_link_libraries (hash_test PRIVATE OpenImageIO_Util)
    set_target_properties (hash_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_hash ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/hash_test)

    add_executable (parallel_test parallel_test.cpp)
    target_link_libraries (parallel_test PRIVATE OpenImageIO_Util)
    set_target_properties (parallel_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_parallel ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/parallel_test)

    add_executable (timer_test timer_test.cpp)
    target_link_libraries (timer_test PRIVATE OpenImageIO_Util)
    set_target_properties (timer_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_timer ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/timer_test)

    add_executable (thread_test thread_test.cpp)
    target_link_libraries (thread_test PRIVATE OpenImageIO_Util)
    set_target_properties (thread_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_thread ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/thread_test)

    add_executable (simd_test simd_test.cpp)
    target_link_libraries (simd_test PRIVATE OpenImageIO
                                             ${OPENIMAGEIO_IMATH_TARGETS})
    set_target_properties (simd_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_simd ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/simd_test)

    add_executable (filter_test filter_test.cpp)
    target_link_libraries (filter_test PRIVATE OpenImageIO)
    set_target_properties (filter_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_filter ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/filter_test)

    add_executable (paramlist_test paramlist_test.cpp)
    target_link_libraries (paramlist_test PRIVATE OpenImageIO
                                                  ${OPENIMAGEIO_IMATH_TARGETS})
    set_target_properties (paramlist_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_paramlist ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/paramlist_test)

    add_executable (strongparam_test strongparam_test.cpp)
    target_link_libraries (strongparam_test PRIVATE OpenImageIO)
    set_target_properties (strongparam_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_strongparam ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/strongparam_test)

    add_executable (typedesc_test typedesc_test.cpp)
    target_link_libraries (typedesc_test PRIVATE OpenImageIO_Util
                           ${OPENIMAGEIO_OPENEXR_TARGETS})
    set_target_properties (typedesc_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_typedesc ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/typedesc_test)

    add_executable (type_traits_test type_traits_test.cpp)
    target_link_libraries (type_traits_test PRIVATE OpenImageIO_Util
                           ${OPENIMAGEIO_OPENEXR_TARGETS})
    set_target_properties (type_traits_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_type_traits ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/type_traits_test)

endif ()
