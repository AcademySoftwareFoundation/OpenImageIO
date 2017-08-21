set (libOpenImageIO_Util_srcs argparse.cpp benchmark.cpp
                  errorhandler.cpp filesystem.cpp
                  farmhash.cpp filter.cpp hashes.cpp paramlist.cpp
                  plugin.cpp SHA1.cpp
                  strutil.cpp sysutil.cpp thread.cpp timer.cpp
                  typedesc.cpp ustring.cpp xxhash.cpp)

if (BUILDSTATIC)
    add_library (OpenImageIO_Util STATIC ${libOpenImageIO_Util_srcs})
else ()
    add_library (OpenImageIO_Util SHARED ${libOpenImageIO_Util_srcs})
endif ()
target_link_libraries (OpenImageIO_Util ${SANITIZE_LIBRARIES})
target_link_libraries (OpenImageIO_Util ${Boost_LIBRARIES})
if (WIN32)
    target_link_libraries (OpenImageIO_Util psapi.lib)
endif ()
target_link_libraries (OpenImageIO_Util ${ILMBASE_LIBRARIES})

if (VERBOSE)
    message(STATUS "Setting SOVERSION to: ${SOVERSION}")
endif ()
set_target_properties(OpenImageIO_Util
                         PROPERTIES
                         VERSION ${OIIO_VERSION_MAJOR}.${OIIO_VERSION_MINOR}.${OIIO_VERSION_PATCH}
                         SOVERSION ${SOVERSION}
                     )

if (VISIBILITY_COMMAND)
    set_target_properties (OpenImageIO_Util
                           PROPERTIES
                           LINK_FLAGS "${VISIBILITY_COMMAND} ${VISIBILITY_MAP_COMMAND}")
endif ()

# For consistency with the linux SpComp2s, create Mac OS X SpComp2s
# with a .so suffix instead of a .dylib suffix.
if (DEFINED OVERRIDE_SHARED_LIBRARY_SUFFIX)
    set_target_properties (OpenImageIO_Util PROPERTIES SUFFIX ${OVERRIDE_SHARED_LIBRARY_SUFFIX})
endif ()

oiio_install_targets (OpenImageIO_Util)



if (OIIO_BUILD_TESTS)

    add_executable (atomic_test atomic_test.cpp)
    set_target_properties (atomic_test PROPERTIES FOLDER "Unit Tests")
    target_link_libraries (atomic_test OpenImageIO_Util ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
    add_test (unit_atomic atomic_test)

    add_executable (array_view_test array_view_test.cpp)
    set_target_properties (array_view_test PROPERTIES FOLDER "Unit Tests")
    target_link_libraries (array_view_test OpenImageIO_Util ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
    add_test (unit_array_view array_view_test)

    add_executable (spinlock_test spinlock_test.cpp)
    set_target_properties (spinlock_test PROPERTIES FOLDER "Unit Tests")
    target_link_libraries (spinlock_test OpenImageIO_Util ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
    add_test (unit_spinlock spinlock_test)

    add_executable (spin_rw_test spin_rw_test.cpp)
    set_target_properties (spin_rw_test PROPERTIES FOLDER "Unit Tests")
    target_link_libraries (spin_rw_test OpenImageIO_Util ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
    add_test (unit_spin_rw spin_rw_test)

    add_executable (ustring_test ustring_test.cpp)
    set_target_properties (ustring_test PROPERTIES FOLDER "Unit Tests")
    target_link_libraries (ustring_test OpenImageIO_Util ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
    add_test (unit_ustring ustring_test)

    add_executable (strutil_test strutil_test.cpp)
    set_target_properties (strutil_test PROPERTIES FOLDER "Unit Tests")
    target_link_libraries (strutil_test OpenImageIO ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
    add_test (unit_strutil strutil_test)

    add_executable (fmath_test fmath_test.cpp)
    set_target_properties (fmath_test PROPERTIES FOLDER "Unit Tests")
    target_link_libraries (fmath_test OpenImageIO_Util ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
    add_test (unit_fmath fmath_test)

    add_executable (filesystem_test filesystem_test.cpp)
    set_target_properties (filesystem_test PROPERTIES FOLDER "Unit Tests")
    target_link_libraries (filesystem_test OpenImageIO_Util ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
    add_test (unit_filesystem filesystem_test)

    add_executable (optparser_test optparser_test.cpp)
    set_target_properties (optparser_test PROPERTIES FOLDER "Unit Tests")
    target_link_libraries (optparser_test OpenImageIO_Util ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
    add_test (unit_optparser optparser_test)

    add_executable (hash_test hash_test.cpp)
    set_target_properties (hash_test PROPERTIES FOLDER "Unit Tests")
    target_link_libraries (hash_test OpenImageIO_Util ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
    add_test (unit_hash hash_test)

    add_executable (parallel_test parallel_test.cpp)
    set_target_properties (parallel_test PROPERTIES FOLDER "Unit Tests")
    target_link_libraries (parallel_test OpenImageIO_Util ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
    add_test (unit_parallel parallel_test)

    add_executable (timer_test timer_test.cpp)
    set_target_properties (timer_test PROPERTIES FOLDER "Unit Tests")
    target_link_libraries (timer_test OpenImageIO_Util ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
    add_test (unit_timer timer_test)

    add_executable (thread_test thread_test.cpp)
    set_target_properties (thread_test PROPERTIES FOLDER "Unit Tests")
    target_link_libraries (thread_test OpenImageIO_Util ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
    add_test (unit_thread thread_test)

    add_executable (simd_test simd_test.cpp)
    set_target_properties (simd_test PROPERTIES FOLDER "Unit Tests")
    target_link_libraries (simd_test OpenImageIO ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
    add_test (unit_simd simd_test)

    add_executable (filter_test filter_test.cpp)
    set_target_properties (filter_test PROPERTIES FOLDER "Unit Tests")
    target_link_libraries (filter_test OpenImageIO ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
    add_test (unit_filter filter_test)

    add_executable (paramlist_test paramlist_test.cpp)
    set_target_properties (paramlist_test PROPERTIES FOLDER "Unit Tests")
    target_link_libraries (paramlist_test OpenImageIO ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
    add_test (unit_paramlist paramlist_test)

endif ()
