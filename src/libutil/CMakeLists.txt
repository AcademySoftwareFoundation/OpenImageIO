set (libOpenImageIO_Util_srcs argparse.cpp benchmark.cpp
                  errorhandler.cpp filesystem.cpp
                  farmhash.cpp filter.cpp hashes.cpp paramlist.cpp
                  plugin.cpp SHA1.cpp
                  strutil.cpp sysutil.cpp thread.cpp timer.cpp
                  typedesc.cpp ustring.cpp xxhash.cpp)

add_library (OpenImageIO_Util ${libOpenImageIO_Util_srcs})
target_include_directories (OpenImageIO_Util
        PUBLIC
            ${CMAKE_INSTALL_FULL_INCLUDEDIR}
            ${ILMBASE_INCLUDES}
        )
target_link_libraries (OpenImageIO_Util
        PUBLIC
            ${ILMBASE_LIBRARIES}
            ${GCC_ATOMIC_LIBRARIES}
        PRIVATE
            ${SANITIZE_LIBRARIES}
            ${Boost_LIBRARIES}
            ${CMAKE_DL_LIBS}
        )

if (WIN32)
    target_link_libraries (OpenImageIO_Util PRIVATE psapi)
endif()

target_compile_definitions (OpenImageIO_Util PRIVATE OpenImageIO_EXPORTS)
if (NOT BUILD_SHARED_LIBS)
    target_compile_definitions (OpenImageIO_Util PUBLIC OIIO_STATIC_DEFINE=1)
endif ()

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

# For consistency with the linux SpComp2s, create Mac OS X SpComp2s
# with a .so suffix instead of a .dylib suffix.
if (DEFINED OVERRIDE_SHARED_LIBRARY_SUFFIX)
    set_target_properties (OpenImageIO_Util PROPERTIES SUFFIX ${OVERRIDE_SHARED_LIBRARY_SUFFIX})
endif ()

install_targets (OpenImageIO_Util)



if (OIIO_BUILD_TESTS)

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
    target_link_libraries (fmath_test PRIVATE OpenImageIO_Util)
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
    target_link_libraries (simd_test PRIVATE OpenImageIO)
    set_target_properties (simd_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_simd ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/simd_test)

    add_executable (filter_test filter_test.cpp)
    target_link_libraries (filter_test PRIVATE OpenImageIO)
    set_target_properties (filter_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_filter ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/filter_test)

    add_executable (paramlist_test paramlist_test.cpp)
    target_link_libraries (paramlist_test PRIVATE OpenImageIO)
    set_target_properties (paramlist_test PROPERTIES FOLDER "Unit Tests")
    add_test (unit_paramlist ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/paramlist_test)

endif ()
