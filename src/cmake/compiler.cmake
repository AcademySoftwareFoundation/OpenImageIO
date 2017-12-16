###########################################################################
# Compiler-related detection, options, and actions

message (STATUS "CMAKE_CXX_COMPILER is ${CMAKE_CXX_COMPILER}")
message (STATUS "CMAKE_CXX_COMPILER_ID is ${CMAKE_CXX_COMPILER_ID}")

set (USE_CPP 11 CACHE STRING "C++ standard to prefer (11, 14, etc.)")
option (USE_LIBCPLUSPLUS "Compile with clang libc++")
set (USE_SIMD "" CACHE STRING "Use SIMD directives (0, sse2, sse3, ssse3, sse4.1, sse4.2, avx, avx2, avx512f, f16c)")
option (STOP_ON_WARNING "Stop building if there are any compiler warnings" ON)
option (HIDE_SYMBOLS "Hide symbols not in the public API" OFF)
option (USE_CCACHE "Use ccache if found" ON)
option (USE_fPIC "Build with -fPIC")
set (EXTRA_CPP_ARGS "" CACHE STRING "Extra C++ command line definitions")
set (EXTRA_DSO_LINK_ARGS "" CACHE STRING "Extra command line definitions when building DSOs")
option (BUILDSTATIC "Build static libraries instead of shared" OFF)
option (LINKSTATIC  "Link with static external libraries when possible" OFF)
option (CODECOV "Build code coverage tests" OFF)
set (SANITIZE "" CACHE STRING "Build code using sanitizer (address, thread)")
option (CLANG_TIDY "Enable clang-tidy" OFF)
set (CLANG_TIDY_CHECKS "-*" CACHE STRING "clang-tidy checks to perform")
set (CLANG_TIDY_ARGS "" CACHE STRING "clang-tidy args")
option (CLANG_TIDY_FIX "Have clang-tidy fix source" OFF)

# Figure out which compiler we're using
if (CMAKE_COMPILER_IS_GNUCC)
    execute_process (COMMAND ${CMAKE_CXX_COMPILER} -dumpversion
                     OUTPUT_VARIABLE GCC_VERSION
                     OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (VERBOSE)
        message (STATUS "Using gcc ${GCC_VERSION} as the compiler")
    endif ()
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
    add_definitions ("-Wall")
    if (STOP_ON_WARNING)
        add_definitions ("-Werror")
    endif ()
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    # cmake bug workaround -- on some platforms, cmake doesn't set
    # NDEBUG for RelWithDebInfo mode
    add_definitions ("-DNDEBUG")
endif ()

# Options common to gcc and clang
if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
    # CMake doesn't automatically know what do do with
    # include_directories(SYSTEM...) when using clang or gcc.
    set (CMAKE_INCLUDE_SYSTEM_FLAG_CXX "-isystem ")
    # Ensure this macro is set for stdint.h
    add_definitions ("-D__STDC_LIMIT_MACROS")
    add_definitions ("-D__STDC_CONSTANT_MACROS")
    # this allows native instructions to be used for sqrtf instead of a function call
    add_definitions ("-fno-math-errno")
endif ()

if (HIDE_SYMBOLS AND NOT DEBUGMODE AND (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG))
    # Turn default symbol visibility to hidden
    set (VISIBILITY_COMMAND "-fvisibility=hidden -fvisibility-inlines-hidden")
    add_definitions (${VISIBILITY_COMMAND})
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

# Clang-specific options
if (CMAKE_COMPILER_IS_CLANG OR CMAKE_COMPILER_IS_APPLECLANG)
    # Disable some warnings for Clang, for some things that are too awkward
    # to change just for the sake of having no warnings.
    add_definitions ("-Wno-unused-function")
    add_definitions ("-Wno-overloaded-virtual")
    add_definitions ("-Wno-unneeded-internal-declaration")
    add_definitions ("-Wno-unused-private-field")
    add_definitions ("-Wno-tautological-compare")
    # disable warning about unused command line arguments
    add_definitions ("-Qunused-arguments")
    # Don't warn if we ask it not to warn about warnings it doesn't know
    add_definitions ("-Wunknown-warning-option")
    if (CLANG_VERSION_STRING VERSION_GREATER 3.5 OR
        APPLECLANG_VERSION_STRING VERSION_GREATER 6.1)
        add_definitions ("-Wno-unused-local-typedefs")
    endif ()
    if (CLANG_VERSION_STRING VERSION_EQUAL 3.9 OR CLANG_VERSION_STRING VERSION_GREATER 3.9)
        # Don't warn about using unknown preprocessor symbols in #if'set
        add_definitions ("-Wno-expansion-to-defined")
    endif ()
endif ()

# gcc specific options
if (CMAKE_COMPILER_IS_GNUCC AND NOT (CMAKE_COMPILER_IS_CLANG OR CMAKE_COMPILER_IS_APPLECLANG))
    if (NOT ${GCC_VERSION} VERSION_LESS 4.8)
        # suppress a warning that Boost::Python hits in g++ 4.8
        add_definitions ("-Wno-error=unused-local-typedefs")
        add_definitions ("-Wno-unused-local-typedefs")
    endif ()
    add_definitions ("-Wno-unused-result")
    if (NOT ${GCC_VERSION} VERSION_LESS 6.0)
        add_definitions ("-Wno-error=misleading-indentation")
    endif ()
    if (NOT ${GCC_VERSION} VERSION_LESS 7.0)
        add_definitions ("-Wno-aligned-new")
        add_definitions ("-Wno-error=noexcept-type")
    endif ()
endif ()

# Microsoft specific options
if (MSVC)
    add_definitions (/W1)
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
    if (USE_CPP VERSION_EQUAL 17)
        message (STATUS "Building for C++17")
        set (CSTD_FLAGS "-std=c++17")
    elseif (USE_CPP VERSION_EQUAL 14)
        message (STATUS "Building for C++14")
        set (CSTD_FLAGS "-std=c++14")
    else ()
        message (STATUS "Building for C++11")
        set (CSTD_FLAGS "-std=c++11")
    endif ()
    add_definitions (${CSTD_FLAGS})
    if (CMAKE_COMPILER_IS_CLANG)
        # C++ >= 11 doesn't like 'register' keyword, which is in Qt headers
        add_definitions ("-Wno-deprecated-register")
    endif ()
endif ()

if (USE_LIBCPLUSPLUS AND CMAKE_COMPILER_IS_CLANG)
    message (STATUS "Using libc++")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
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
                add_definitions ("-ffp-contract=off")
            endif ()
        endforeach()
    endif ()
    add_definitions (${SIMD_COMPILE_FLAGS})
endif ()


if (USE_fPIC)
    add_definitions ("-fPIC")
endif ()


# Test for features
if (NOT VERBOSE)
    set (CMAKE_REQUIRED_QUIET 1)
endif ()
include (CMakePushCheckState)
include (CheckCXXSourceRuns)

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
    add_definitions ("-ftest-coverage -fprofile-arcs -O0 -D${PROJ_NAME}_CODE_COVERAGE=1")
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
        add_definitions (-fsanitize=${feature})
        set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=${feature}")
        set (CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -fsanitize=${feature}")
        set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=${feature}")
    endforeach()
    add_definitions (-g -fno-omit-frame-pointer)
    if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        set (SANITIZE_ON_LINUX 1)
    endif ()
    if (CMAKE_COMPILER_IS_GNUCC AND ${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        add_definitions ("-fuse-ld=gold")
        set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=gold")
        set (CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -fuse-ld=gold")
        set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=gold")
        set (SANITIZE_LIBRARIES "asan;pthread")
        # set (SANITIZE_LIBRARIES "asan" "ubsan")
    endif()
    add_definitions ("-D${PROJECT_NAME}_SANITIZE=1")
endif ()

# clang-tidy options
if (CLANG_TIDY)
    set (CMAKE_CXX_CLANG_TIDY "clang-tidy;-header-filter=(${PROJECT_NAME})|(${PROJ_NAME})|(${PROJ_NAME_LOWER})|(_pvt.h)")
    if (CLANG_TIDY_ARGS)
        set (CMAKE_CXX_CLANG_TIDY "${CMAKE_CXX_CLANG_TIDY};${CLANG_TIDY_ARGS}")
    endif ()
    if (CLANG_TIDY_FIX)
        set (CMAKE_CXX_CLANG_TIDY "${CMAKE_CXX_CLANG_TIDY};-fix")
    endif ()
    set (CMAKE_CXX_CLANG_TIDY "${CMAKE_CXX_CLANG_TIDY};-checks=${CLANG_TIDY_CHECKS}")
    message (STATUS "clang-tidy command line is: ${CMAKE_CXX_CLANG_TIDY}")
endif ()

if (EXTRA_CPP_ARGS)
    message (STATUS "Extra C++ args: ${EXTRA_CPP_ARGS}")
    add_definitions ("${EXTRA_CPP_ARGS}")
endif()


if (BUILDSTATIC)
    message (STATUS "Building static libraries")
    set (LIBRARY_BUILD_TYPE STATIC)
    add_definitions(-D${PROJ_NAME}_STATIC_BUILD=1)
    if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        # On Linux, the lack of -fPIC when building static libraries seems
        # incompatible with the dynamic library needed for the Python bindings.
        set (USE_PYTHON OFF)
        set (USE_PYTHON3 OFF)
    endif ()
else ()
    set (LIBRARY_BUILD_TYPE SHARED)
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

if (DEFINED ENV{TRAVIS} OR DEFINED ENV{APPVEYOR} OR DEFINED ENV{CI})
    add_definitions ("-D${PROJ_NAME}_CI=1" "-DBUILD_CI=1")
endif ()
