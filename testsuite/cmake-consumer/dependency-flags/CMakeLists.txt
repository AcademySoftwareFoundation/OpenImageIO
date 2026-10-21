# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

cmake_minimum_required (VERSION 3.15)
project (dependency_flags LANGUAGES CXX)
set (CMAKE_BUILD_TYPE Release)

include ("${CMAKE_CURRENT_LIST_DIR}/../../../src/cmake/set_utils.cmake")
include ("${CMAKE_CURRENT_LIST_DIR}/../../../src/cmake/dependency_utils.cmake")
set (dependency_flags_DEPENDENCY_BUILD_ALLOW_UNVERIFIED_TAGS ON)
set (dependency_flags_DEPENDENCY_BUILD_TYPE Release)
# CXXFLAGS initializes a cache entry, so each test run needs fresh children.
file (REMOVE_RECURSE "${dependency_flags_LOCAL_DEPS_ROOT}")
find_package (Git REQUIRED)

# Generate an offline dependency, with the same checks in a nested configure.
set (child_project [=[
cmake_minimum_required (VERSION 3.15)
project (flag_child LANGUAGES CXX)
if (FAIL_CONFIGURE)
    message (FATAL_ERROR "intentional dependency configure failure")
endif ()
if (EXPECT_MUTEX)
    if (NOT CMAKE_CXX_FLAGS MATCHES "-D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR")
        message (FATAL_ERROR "Missing mutex compatibility flag")
    endif ()
    if (MSVC AND NOT CMAKE_CXX_FLAGS MATCHES "/EHsc")
        message (FATAL_ERROR "Lost MSVC toolchain defaults")
    endif ()
elseif (CMAKE_CXX_FLAGS MATCHES "_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR")
    message (FATAL_ERROR "Unexpected mutex compatibility flag")
endif ()
if (EXPECT_CALLER AND NOT CMAKE_CXX_FLAGS MATCHES "CALLER_SENTINEL")
    message (FATAL_ERROR "Lost caller flags")
endif ()
file (WRITE "${CMAKE_CURRENT_BINARY_DIR}/check.cpp" "
#if ${EXPECT_MUTEX}
#  ifndef _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR
#    error Missing mutex compatibility define
#  endif
#else
#  ifdef _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR
#    error Unexpected mutex compatibility define
#  endif
#endif
#if ${EXPECT_CALLER}
#  ifndef CALLER_SENTINEL
#    error Missing caller sentinel
#  endif
static_assert(sizeof(CALLER_QUOTED) == sizeof(\"two words\"), \"Lost quoted flag\");
#endif
int main() { return 0; }
")
add_executable (check "${CMAKE_CURRENT_BINARY_DIR}/check.cpp")
if (NOT NESTED)
    execute_process (COMMAND "${CMAKE_COMMAND}"
        -S "${CMAKE_CURRENT_SOURCE_DIR}"
        -B "${CMAKE_CURRENT_BINARY_DIR}/nested"
        -DNESTED=ON -DEXPECT_MUTEX=${EXPECT_MUTEX}
        -DEXPECT_CALLER=${EXPECT_CALLER}
        RESULT_VARIABLE result)
    if (NOT result EQUAL 0)
        message (FATAL_ERROR "Nested configure failed")
    endif ()
    execute_process (COMMAND "${CMAKE_COMMAND}" --build
        "${CMAKE_CURRENT_BINARY_DIR}/nested" --config Release
        RESULT_VARIABLE result)
    if (NOT result EQUAL 0)
        message (FATAL_ERROR "Nested build failed")
    endif ()
endif ()
]=])

if (MSVC)
    set (expect_mutex 1)
else ()
    set (expect_mutex 0)
endif ()

set (scenarios unset empty caller repeat)
if (FAILURE_TEST)
    set (scenarios caller)
endif ()
foreach (scenario IN LISTS scenarios)
    if (scenario STREQUAL "unset")
        unset (ENV{CXXFLAGS})
    elseif (scenario STREQUAL "empty")
        set (ENV{CXXFLAGS} "")
    else ()
        set (ENV{CXXFLAGS}
             "-DCALLER_SENTINEL -DCALLER_QUOTED=\"\\\"two words\\\"\"")
    endif ()
    set (was_defined FALSE)
    if (DEFINED ENV{CXXFLAGS})
        set (was_defined TRUE)
    endif ()
    set (caller_flags "$ENV{CXXFLAGS}")
    set (expect_caller 0)
    if (scenario STREQUAL "caller" OR scenario STREQUAL "repeat")
        set (expect_caller 1)
    endif ()
    file (MAKE_DIRECTORY "${dependency_flags_LOCAL_DEPS_ROOT}/${scenario}")
    file (WRITE "${dependency_flags_LOCAL_DEPS_ROOT}/${scenario}/CMakeLists.txt"
          "${child_project}")
    # Keep the builder's unchecked checkout inside an empty repository;
    # no commit or network access is needed for this preseeded source.
    execute_process (COMMAND "${GIT_EXECUTABLE}" init --quiet
        "${dependency_flags_LOCAL_DEPS_ROOT}/${scenario}"
        RESULT_VARIABLE result)
    if (NOT result EQUAL 0)
        message (FATAL_ERROR "Could not initialize offline dependency")
    endif ()
    build_dependency_with_cmake (${scenario} NOINSTALL GIT_TAG offline
        CMAKE_ARGS -DEXPECT_MUTEX=${expect_mutex}
                   -DEXPECT_CALLER=${expect_caller}
                   -DFAIL_CONFIGURE=${FAILURE_TEST})
    if (NOT "$ENV{CXXFLAGS}" STREQUAL "${caller_flags}")
        message (FATAL_ERROR "Parent CXXFLAGS changed")
    endif ()
    if (was_defined AND NOT DEFINED ENV{CXXFLAGS})
        message (FATAL_ERROR "Parent CXXFLAGS became unset")
    elseif (NOT was_defined AND DEFINED ENV{CXXFLAGS})
        message (FATAL_ERROR "Parent CXXFLAGS became set")
    endif ()
endforeach ()

# Run the fatal path in a separate process so its diagnostic can be checked.
if (NOT FAILURE_TEST)
    execute_process (COMMAND "${CMAKE_COMMAND}"
        -S "${CMAKE_CURRENT_SOURCE_DIR}"
        -B "${CMAKE_CURRENT_BINARY_DIR}/failure" -DFAILURE_TEST=ON
        RESULT_VARIABLE result OUTPUT_VARIABLE diagnostic
        ERROR_VARIABLE diagnostic)
    if (result EQUAL 0 OR
        NOT diagnostic MATCHES "Configuring local caller failed" OR
        NOT diagnostic MATCHES "intentional dependency configure failure")
        message (FATAL_ERROR "Missing configure failure diagnostic: ${diagnostic}")
    endif ()
endif ()
