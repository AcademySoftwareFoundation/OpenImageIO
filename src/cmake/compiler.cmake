# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

###########################################################################
# Compiler-related detection, options, and actions

if (VERBOSE)
    message (STATUS "CMAKE_SYSTEM_NAME      = ${CMAKE_SYSTEM_NAME}")
    message (STATUS "CMAKE_SYSTEM_VERSION   = ${CMAKE_SYSTEM_VERSION}")
    message (STATUS "CMAKE_SYSTEM_PROCESSOR = ${CMAKE_SYSTEM_PROCESSOR}")
endif ()

message (STATUS "CMAKE_CXX_COMPILER     = ${CMAKE_CXX_COMPILER}")
message (STATUS "CMAKE_CXX_COMPILER_ID  = ${CMAKE_CXX_COMPILER_ID}")

# Require C++11 and disable extensions for all targets
set (CMAKE_CXX_STANDARD 11 CACHE STRING "C++ standard to prefer (11, 14, 17, etc.)")
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)
message (STATUS "Building for C++${CMAKE_CXX_STANDARD}")

option (USE_LIBCPLUSPLUS "Compile with clang libc++" OFF)
set (USE_SIMD "" CACHE STRING "Use SIMD directives (0, sse2, sse3, ssse3, sse4.1, sse4.2, avx, avx2, avx512f, f16c, aes)")
option (STOP_ON_WARNING "Stop building if there are any compiler warnings" ON)
option (HIDE_SYMBOLS "Hide symbols not in the public API" OFF)
option (USE_CCACHE "Use ccache if found" ON)
set (EXTRA_CPP_ARGS "" CACHE STRING "Extra C++ command line definitions")
set (EXTRA_DSO_LINK_ARGS "" CACHE STRING "Extra command line definitions when building DSOs")
option (BUILD_SHARED_LIBS "Build shared libraries (set to OFF to build static libs)" ON)
option (LINKSTATIC  "Link with static external libraries when possible" OFF)
option (CODECOV "Build code coverage tests" OFF)
set (SANITIZE "" CACHE STRING "Build code using sanitizer (address, thread)")
option (CLANG_TIDY "Enable clang-tidy" OFF)
set (CLANG_TIDY_CHECKS "" CACHE STRING "clang-tidy checks to perform (none='-*')")
set (CLANG_TIDY_ARGS "" CACHE STRING "clang-tidy args")
option (CLANG_TIDY_FIX "Have clang-tidy fix source" OFF)
set (CLANG_FORMAT_EXE_HINT "" CACHE PATH "clang-format executable's directory (will search if not specified")
set (CLANG_FORMAT_INCLUDES "src/*.h" "src/*.cpp"
    CACHE STRING "Glob patterns to include for clang-format")
    # Eventually: want this to be: "src/*.h;src/*.cpp"
set (CLANG_FORMAT_EXCLUDES "src/include/OpenImageIO/fmt/*.h"
                           "*pugixml*" "*SHA1*" "*/farmhash.cpp" "*/tinyformat.h"
                           "src/dpx.imageio/libdpx/*"
                           "src/cineon.imageio/libcineon/*"
                           "src/dds.imageio/squish/*"
                           "src/gif.imageio/gif.h"
                           "src/hdr.imageio/rgbe.cpp"
     CACHE STRING "Glob patterns to exclude for clang-format")
set (GLIBCXX_USE_CXX11_ABI "" CACHE STRING "For gcc, use the new C++11 library ABI (0|1)")

# Figure out which compiler we're using
if (CMAKE_COMPILER_IS_GNUCC)
    execute_process (COMMAND ${CMAKE_CXX_COMPILER} -dumpversion
                     OUTPUT_VARIABLE GCC_VERSION
                     OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (VERBOSE)
        message (STATUS "Using gcc ${GCC_VERSION} as the compiler")
    endif ()
else ()
    set (GCC_VERSION 0)
endif ()

# Figure out which compiler we're using, for tricky cases
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER MATCHES "[Cc]lang")
    # If using any flavor of clang, set CMAKE_COMPILER_IS_CLANG. If it's
    # Apple's variety, set CMAKE_COMPILER_IS_APPLECLANG and
    # APPLECLANG_VERSION_STRING, otherwise for generic clang set
    # CLANG_VERSION_STRING.
    set (CMAKE_COMPILER_IS_CLANG 1)
    EXECUTE_PROCESS( COMMAND ${CMAKE_CXX_COMPILER} --version OUTPUT_VARIABLE clang_full_version_string )
    if (clang_full_version_string MATCHES "Apple")
        set (CMAKE_CXX_COMPILER_ID "AppleClang")
        set (CMAKE_COMPILER_IS_APPLECLANG 1)
        string (REGEX REPLACE ".* version ([0-9]+\\.[0-9]+).*" "\\1" APPLECLANG_VERSION_STRING ${clang_full_version_string})
        if (VERBOSE)
            message (STATUS "The compiler is Clang: ${CMAKE_CXX_COMPILER_ID} version ${APPLECLANG_VERSION_STRING}")
        endif ()
    else ()
        string (REGEX REPLACE ".* version ([0-9]+\\.[0-9]+).*" "\\1" CLANG_VERSION_STRING ${clang_full_version_string})
        if (VERBOSE)
            message (STATUS "The compiler is Clang: ${CMAKE_CXX_COMPILER_ID} version ${CLANG_VERSION_STRING}")
        endif ()
    endif ()
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Intel")
    set (CMAKE_COMPILER_IS_INTEL 1)
    if (VERBOSE)
        message (STATUS "Using Intel as the compiler")
    endif ()
endif ()

# turn on more detailed warnings and consider warnings as errors
if (NOT MSVC)
    add_compile_options ("-Wall")
    if (STOP_ON_WARNING OR DEFINED ENV{CI})
        add_compile_options ("-Werror")
        # N.B. Force CI builds (Travis defines $CI) to use -Werror, even if
        # STOP_ON_WARNING has been switched off by default, which we may do
        # in release branches.
    endif ()
endif ()

if ($<CONFIG:RelWithDebInfo>)
    # cmake bug workaround -- on some platforms, cmake doesn't set
    # NDEBUG for RelWithDebInfo mode
    add_definitions ("-DNDEBUG")
endif ()

# Options common to gcc and clang
if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
    #   # CMake doesn't automatically know what do do with
    #   # include_directories(SYSTEM...) when using clang or gcc.
    #   set (CMAKE_INCLUDE_SYSTEM_FLAG_CXX "-isystem ")

    # Ensure this macro is set for stdint.h
    add_definitions ("-D__STDC_LIMIT_MACROS")
    add_definitions ("-D__STDC_CONSTANT_MACROS")
    # this allows native instructions to be used for sqrtf instead of a function call
    add_compile_options ("-fno-math-errno")
endif ()

if (HIDE_SYMBOLS AND NOT $<CONFIG:Debug>
                 AND (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG))
    # Turn default symbol visibility to hidden
    set (VISIBILITY_COMMAND -fvisibility=hidden -fvisibility-inlines-hidden)
    add_compile_options (${VISIBILITY_COMMAND})
    if (CMAKE_SYSTEM_NAME MATCHES "Linux|kFreeBSD" OR CMAKE_SYSTEM_NAME STREQUAL "GNU")
        # Linux/FreeBSD/Hurd: also hide all the symbols of dependent
        # libraries to prevent clashes if an app using OIIO is linked
        # against other verions of our dependencies.
        if (NOT VISIBILITY_MAP_FILE)
            set (VISIBILITY_MAP_FILE "${PROJECT_SOURCE_DIR}/src/build-scripts/hidesymbols.map")
        endif ()
        set (VISIBILITY_MAP_COMMAND "-Wl,--version-script=${VISIBILITY_MAP_FILE}")
        if (VERBOSE)
            message (STATUS "VISIBILITY_MAP_COMMAND = ${VISIBILITY_MAP_COMMAND}")
        endif ()
    endif ()
endif ()

if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD"
    AND ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "i386")
    # minimum arch of i586 is needed for atomic cpu instructions
    add_compile_options (-march=i586)
endif ()

# Clang-specific options
if (CMAKE_COMPILER_IS_CLANG OR CMAKE_COMPILER_IS_APPLECLANG)
    # Disable some warnings for Clang, for some things that are too awkward
    # to change just for the sake of having no warnings.
    add_compile_options ("-Wno-unused-function")
    add_compile_options ("-Wno-overloaded-virtual")
    add_compile_options ("-Wno-unneeded-internal-declaration")
    add_compile_options ("-Wno-unused-private-field")
    add_compile_options ("-Wno-tautological-compare")
    # disable warning about unused command line arguments
    add_compile_options ("-Qunused-arguments")
    # Don't warn if we ask it not to warn about warnings it doesn't know
    add_compile_options ("-Wunknown-warning-option")
    if (CLANG_VERSION_STRING VERSION_GREATER 3.5 OR
        APPLECLANG_VERSION_STRING VERSION_GREATER 6.1)
        add_compile_options ("-Wno-unused-local-typedefs")
    endif ()
    if (CLANG_VERSION_STRING VERSION_EQUAL 3.9 OR CLANG_VERSION_STRING VERSION_GREATER 3.9)
        # Don't warn about using unknown preprocessor symbols in #if'set
        add_compile_options ("-Wno-expansion-to-defined")
    endif ()
endif ()

# gcc specific options
if (CMAKE_COMPILER_IS_GNUCC AND NOT (CMAKE_COMPILER_IS_CLANG OR CMAKE_COMPILER_IS_APPLECLANG))
    add_compile_options ("-Wno-unused-local-typedefs")
    add_compile_options ("-Wno-unused-result")
    if (NOT ${GCC_VERSION} VERSION_LESS 7.0)
        add_compile_options ("-Wno-aligned-new")
        add_compile_options ("-Wno-noexcept-type")
    endif ()
endif ()

# Microsoft specific options
if (MSVC)
    add_compile_options (/W1)
    add_definitions (-D_CRT_SECURE_NO_DEPRECATE)
    add_definitions (-D_CRT_SECURE_NO_WARNINGS)
    add_definitions (-D_CRT_NONSTDC_NO_WARNINGS)
    add_definitions (-D_SCL_SECURE_NO_WARNINGS)
    add_definitions (-DJAS_WIN_MSVC_BUILD)
endif (MSVC)

# Use ccache if found
find_program (CCACHE_FOUND ccache)
if (CCACHE_FOUND AND USE_CCACHE)
    if (CMAKE_COMPILER_IS_CLANG AND USE_QT AND (NOT DEFINED ENV{CCACHE_CPP2}))
        message (STATUS "Ignoring ccache because clang + Qt + env CCACHE_CPP2 is not set")
    else ()
        set_property (GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
        set_property (GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
    endif ()
endif ()

set (CSTD_FLAGS "")
if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG OR CMAKE_COMPILER_IS_INTEL)
    set (CSTD_FLAGS "-std=c++${CMAKE_CXX_STANDARD}")
    if (CMAKE_COMPILER_IS_CLANG)
        # C++ >= 11 doesn't like 'register' keyword, which is in Qt headers
        add_compile_options ("-Wno-deprecated-register")
    endif ()
endif ()

if (USE_LIBCPLUSPLUS AND CMAKE_COMPILER_IS_CLANG)
    message (STATUS "Using libc++")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif ()

# GCC 5+: honor build-time option for whether or not to use new string ABI.
# FIXME: In theory, this should also be needed for clang, if compiling with
# the gcc libstdc++ toolchain. In practice, I could not get things to build
# with clang properly when using this option, and I haven't yet seen a case
# where it's needed. We can return to this and fix for clang if it becomes a
# legit problem later.
if (CMAKE_COMPILER_IS_GNUCC AND NOT ${GCC_VERSION} VERSION_LESS 5.0)
    if (NOT ${GLIBCXX_USE_CXX11_ABI} STREQUAL "")
        add_definitions ("-D_GLIBCXX_USE_CXX11_ABI=${GLIBCXX_USE_CXX11_ABI}")
    endif ()
endif ()


# SIMD and machine architecture options
set (SIMD_COMPILE_FLAGS "")
if (NOT USE_SIMD STREQUAL "")
    message (STATUS "Compiling with SIMD level ${USE_SIMD}")
    if (USE_SIMD STREQUAL "0")
        set (SIMD_COMPILE_FLAGS ${SIMD_COMPILE_FLAGS} "-DOIIO_NO_SSE=1")
    else ()
        string (REPLACE "," ";" SIMD_FEATURE_LIST ${USE_SIMD})
        foreach (feature ${SIMD_FEATURE_LIST})
            if (VERBOSE)
                message (STATUS "SIMD feature: ${feature}")
            endif ()
            if (MSVC OR CMAKE_COMPILER_IS_INTEL)
                set (SIMD_COMPILE_FLAGS ${SIMD_COMPILE_FLAGS} "/arch:${feature}")
            else ()
                set (SIMD_COMPILE_FLAGS ${SIMD_COMPILE_FLAGS} "-m${feature}")
            endif ()
            if (feature STREQUAL "fma" AND (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG))
                # If fma is requested, for numerical accuracy sake, turn it
                # off by default except when we explicitly use madd. At some
                # future time, we should look at this again carefully and
                # see if we want to use it more widely by ffp-contract=fast.
                add_compile_options ("-ffp-contract=off")
            endif ()
        endforeach()
    endif ()
    add_compile_options (${SIMD_COMPILE_FLAGS})
endif ()


# Test for features
if (NOT VERBOSE)
    set (CMAKE_REQUIRED_QUIET 1)
endif ()
include (CMakePushCheckState)
include (CheckCXXSourceRuns)

# Find out if it's safe for us to use std::regex or if we need boost.regex
cmake_push_check_state ()
set (CMAKE_REQUIRED_DEFINITIONS ${CSTD_FLAGS})
check_cxx_source_runs("
      #include <regex>
      int main() {
          std::string r = std::regex_replace(std::string(\"abc\"), std::regex(\"b\"), \" \");
          return r == \"a c\" ? 0 : -1;
      }"
      USE_STD_REGEX)
if (USE_STD_REGEX)
    add_definitions (-DUSE_STD_REGEX)
else ()
    add_definitions (-DUSE_BOOST_REGEX)
endif ()
cmake_pop_check_state ()

# Code coverage options
if (CODECOV AND (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG))
    message (STATUS "Compiling for code coverage analysis")
    add_compile_options ("-ftest-coverage -fprofile-arcs -O0")
    add_definitions ("-D${PROJ_NAME}_CODE_COVERAGE=1")
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -ftest-coverage -fprofile-arcs")
    set (CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -ftest-coverage -fprofile-arcs")
    set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -ftest-coverage -fprofile-arcs")
endif ()

# Sanitizer options
if (SANITIZE AND (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG))
    message (STATUS "Compiling for sanitizer=${SANITIZE}")
    string (REPLACE "," ";" SANITIZE_FEATURE_LIST ${SANITIZE})
    foreach (feature ${SANITIZE_FEATURE_LIST})
        message (STATUS "  sanitize feature: ${feature}")
        add_compile_options (-fsanitize=${feature})
        set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=${feature}")
        set (CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -fsanitize=${feature}")
        set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=${feature}")
    endforeach()
    add_compile_options (-g -fno-omit-frame-pointer)
    if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        set (SANITIZE_ON_LINUX 1)
    endif ()
    if (CMAKE_COMPILER_IS_GNUCC AND ${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        add_compile_options ("-fuse-ld=gold")
        set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=gold")
        set (CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -fuse-ld=gold")
        set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=gold")
        set (SANITIZE_LIBRARIES "asan;pthread")
        # set (SANITIZE_LIBRARIES "asan" "ubsan")
    endif()
    if (CMAKE_COMPILER_IS_GNUCC)
        # turn on glibcxx extra annotations to find vector writes past end
        add_definitions ("-D_GLIBCXX_SANITIZE_VECTOR=1")
    endif ()
    add_definitions ("-D${PROJECT_NAME}_SANITIZE=1")
endif ()

# clang-tidy options
if (CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES "clang-tidy"
                 DOC "Path to clang-tidy executable")
    message (STATUS "CLANG_TIDY_EXE ${CLANG_TIDY_EXE}")
    if (CLANG_TIDY_EXE AND NOT ${CMAKE_VERSION} VERSION_LESS 3.6)
        set (CMAKE_CXX_CLANG_TIDY
             "${CLANG_TIDY_EXE}"
             )
        if (CLANG_TIDY_ARGS)
            list (APPEND CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_ARGS})
        endif ()
        if (CLANG_TIDY_CHECKS)
            list (APPEND CMAKE_CXX_CLANG_TIDY -checks="${CLANG_TIDY_CHECKS}")
        endif ()
        execute_process (COMMAND ${CMAKE_CXX_CLANG_TIDY} -list-checks
                         OUTPUT_VARIABLE tidy_checks
                         OUTPUT_STRIP_TRAILING_WHITESPACE)
        if (CLANG_TIDY_FIX)
            list (APPEND CMAKE_CXX_CLANG_TIDY "-fix")
        endif ()
        message (STATUS "clang-tidy command line is: ${CMAKE_CXX_CLANG_TIDY}")
        message (STATUS "${tidy_checks}")
    else ()
        message (STATUS "Cannot run clang-tidy as requested")
    endif ()
    # Hint: run with CLANG_TIDY_ARGS=-list-checks to list all the checks
endif ()

# clang-format
find_program (CLANG_FORMAT_EXE
              NAMES clang-format bin/clang-format
              HINTS ${CLANG_FORMAT_EXE_HINT} ENV CLANG_FORMAT_EXE_HINT
                    ENV LLVM_DIRECTORY
              NO_DEFAULT_PATH
              DOC "Path to clang-format executable")
find_program (CLANG_FORMAT_EXE NAMES clang-format bin/clang-format)
if (CLANG_FORMAT_EXE)
    message (STATUS "clang-format found: ${CLANG_FORMAT_EXE}")
    # Start with the list of files to include when formatting...
    file (GLOB_RECURSE FILES_TO_FORMAT ${CLANG_FORMAT_INCLUDES})
    # ... then process any list of excludes we are given
    foreach (_pat ${CLANG_FORMAT_EXCLUDES})
        file (GLOB_RECURSE _excl ${_pat})
        list (REMOVE_ITEM FILES_TO_FORMAT ${_excl})
    endforeach ()
    #message (STATUS "clang-format file list: ${FILES_TO_FORMAT}")
    file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/.clang-format
          DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    add_custom_target (clang-format
        COMMAND "${CLANG_FORMAT_EXE}" -i -style=file ${FILES_TO_FORMAT} )
else ()
    message (STATUS "clang-format not found.")
endif ()


if (EXTRA_CPP_ARGS)
    message (STATUS "Extra C++ args: ${EXTRA_CPP_ARGS}")
    add_compile_options ("${EXTRA_CPP_ARGS}")
endif()


# Use .a files if LINKSTATIC is enabled
if (LINKSTATIC)
    set (_orig_link_suffixes ${CMAKE_FIND_LIBRARY_SUFFIXES})
    message (STATUS "Statically linking external libraries")
    if (WIN32)
        set (CMAKE_FIND_LIBRARY_SUFFIXES .lib .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
    else ()
        set (CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
    endif ()
    add_definitions (-DBoost_USE_STATIC_LIBS=1)
    set (Boost_USE_STATIC_LIBS 1)
else ()
    if (MSVC)
        add_definitions (-DBOOST_ALL_DYN_LINK)
        add_definitions (-DOPENEXR_DLL)
    endif ()
endif ()

if (DEFINED ENV{TRAVIS} OR DEFINED ENV{APPVEYOR} OR DEFINED ENV{CI} OR DEFINED ENV{GITHUB_ACTIONS})
    add_definitions ("-D${PROJ_NAME}_CI=1" "-DBUILD_CI=1")
    if (APPLE)
        # Keep Mono framework from being incorrectly searched for include
        # files on GitHub Actions CI.
        set(CMAKE_FIND_FRAMEWORK LAST)
    endif ()
endif ()
