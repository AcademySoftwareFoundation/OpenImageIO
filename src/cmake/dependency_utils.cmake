# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


set_cache (${PROJECT_NAME}_REQUIRED_DEPS ""
           "Additional dependencies to consider required (semicolon-separated list, or ALL)")
set_cache (${PROJECT_NAME}_OPTIONAL_DEPS ""
           "Additional dependencies to consider optional (semicolon-separated list, or ALL)")
set_option (${PROJECT_NAME}_ALWAYS_PREFER_CONFIG
            "Prefer a dependency's exported config file if it's available" OFF)

set_cache (${PROJECT_NAME}_BUILD_MISSING_DEPS ""
     "Try to download and build any of these missing dependencies (or 'all')")
set_cache (${PROJECT_NAME}_BUILD_LOCAL_DEPS ""
     "Force local builds of these dependencies if possible (or 'all')")

set_cache (${PROJECT_NAME}_LOCAL_DEPS_ROOT "${PROJECT_BINARY_DIR}/deps"
           "Directory were we do local builds of dependencies")
list (APPEND CMAKE_PREFIX_PATH ${${PROJECT_NAME}_LOCAL_DEPS_ROOT}/dist)
include_directories(BEFORE ${${PROJECT_NAME}_LOCAL_DEPS_ROOT}/include)

# Build type for locally built dependencies. Default to the same build type
# as the current project.
set_cache (${PROJECT_NAME}_DEPENDENCY_BUILD_TYPE ${CMAKE_BUILD_TYPE}
           "Build type for locally built dependencies")
set_option (${PROJECT_NAME}_DEPENDENCY_BUILD_VERBOSE
            "Make dependency builds extra verbose" OFF)
if (MSVC)
    # I haven't been able to get Windows local dependency builds to work with
    # static libraries, so default to shared libraries on Windows until we can
    # figure it out.
    set_cache (LOCAL_BUILD_SHARED_LIBS_DEFAULT ON
               DOC "Should a local dependency build, if necessary, build shared libraries" ADVANCED)
else ()
    # On non-Windows, default to static libraries for local builds.
    set_cache (LOCAL_BUILD_SHARED_LIBS_DEFAULT OFF
               DOC "Should a local dependency build, if necessary, build shared libraries" ADVANCED)
endif ()

# Search for regular libraries before searching for macOS frameworks.
if (APPLE)
    set_cache (CMAKE_FIND_FRAMEWORK LAST
               DOC "Set relative priority of finding frameworks vs. regular libraries" ADVANCED)
endif ()

# Track all build deps we find with checked_find_package
set (CFP_ALL_BUILD_DEPS_FOUND "")
set (CFP_EXTERNAL_BUILD_DEPS_FOUND "")

# Track all build deps we failed to find with checked_find_package
set (CFP_ALL_BUILD_DEPS_NOTFOUND "")
set (CFP_LOCALLY_BUILDABLE_DEPS_NOTFOUND "")

# Track all build deps we found but were of inadequate version
set (CFP_ALL_BUILD_DEPS_BADVERSION "")
set (CFP_LOCALLY_BUILDABLE_DEPS_BADVERSION "")

# Which dependencies did we build locally
set (CFP_LOCALLY_BUILT_DEPS "")



# Utility function to list the names and values of all variables matching
# the pattern (case-insensitive)
function (dump_matching_variables pattern)
    string (TOLOWER ${pattern} _pattern_lower)
    get_cmake_property(_allvars VARIABLES)
    list (SORT _allvars)
    foreach (_var IN LISTS _allvars)
        string (TOLOWER ${_var} _var_lower)
        if (_var_lower MATCHES ${_pattern_lower})
            message (STATUS "    ${_var} = ${${_var}}")
        endif ()
    endforeach ()
endfunction ()


# Helper: Print a report about missing dependencies and give insructions on
# how to turn on automatic local dependency building.
function (print_package_notfound_report)
    message (STATUS)
    message (STATUS "${ColorBoldYellow}=========================================================================${ColorReset}")
    message (STATUS "${ColorBoldYellow}= Dependency report                                                     =${ColorReset}")
    message (STATUS "${ColorBoldYellow}=========================================================================${ColorReset}")
    message (STATUS)
    if (CFP_EXTERNAL_BUILD_DEPS_FOUND)
        message (STATUS "${ColorBoldWhite}The following dependencies found externally:${ColorReset}")
        list (SORT CFP_EXTERNAL_BUILD_DEPS_FOUND CASE INSENSITIVE)
        list (REMOVE_DUPLICATES CFP_EXTERNAL_BUILD_DEPS_FOUND)
        foreach (_pkg IN LISTS CFP_EXTERNAL_BUILD_DEPS_FOUND)
            message (STATUS "    ${_pkg} ${${_pkg}_VERSION}")
        endforeach ()
    endif ()
    if (CFP_ALL_BUILD_DEPS_BADVERSION)
        message (STATUS "${ColorBoldWhite}The following dependencies were found but were too old:${ColorReset}")
        list (SORT CFP_ALL_BUILD_DEPS_BADVERSION CASE INSENSITIVE)
        list (REMOVE_DUPLICATES CFP_ALL_BUILD_DEPS_BADVERSION)
        foreach (_pkg IN LISTS CFP_ALL_BUILD_DEPS_BADVERSION)
            if (_pkg IN_LIST CFP_LOCALLY_BUILT_DEPS)
                message (STATUS "    ${_pkg} ${${_pkg}_NOT_FOUND_EXPLANATION} ${ColorMagenta}(${${_pkg}_VERSION} BUILT LOCALLY)${ColorReset}")
            else ()
                message (STATUS "    ${_pkg} ${${_pkg}_NOT_FOUND_EXPLANATION}")
            endif ()
        endforeach ()
    endif ()
    if (CFP_ALL_BUILD_DEPS_NOTFOUND)
        message (STATUS "${ColorBoldWhite}The following dependencies were not found:${ColorReset}")
        list (SORT CFP_ALL_BUILD_DEPS_NOTFOUND CASE INSENSITIVE)
        list (REMOVE_DUPLICATES CFP_ALL_BUILD_DEPS_NOTFOUND)
        foreach (_pkg IN LISTS CFP_ALL_BUILD_DEPS_NOTFOUND)
            if (_pkg IN_LIST CFP_LOCALLY_BUILT_DEPS)
                message (STATUS "    ${_pkg} ${_${_pkg}_version_range} ${${_pkg}_NOT_FOUND_EXPLANATION} ${ColorMagenta}(${${_pkg}_VERSION} BUILT LOCALLY)${ColorReset}")
            else ()
                message (STATUS "    ${_pkg} ${_${_pkg}_version_range} ${${_pkg}_NOT_FOUND_EXPLANATION}")
            endif ()
        endforeach ()
    endif ()
    if (CFP_LOCALLY_BUILDABLE_DEPS_NOTFOUND OR CFP_LOCALLY_BUILDABLE_DEPS_BADVERSION)
        message (STATUS)
        message (STATUS "${ColorBoldWhite}For some of these, we can build them locally:${ColorReset}")
        foreach (_pkg IN LISTS CFP_LOCALLY_BUILDABLE_DEPS_NOTFOUND CFP_LOCALLY_BUILDABLE_DEPS_BADVERSION)
            message (STATUS "    ${_pkg}")
        endforeach ()
        message (STATUS "${ColorBoldWhite}To build them automatically if not found, build again with option:${ColorReset}")
        message (STATUS "    ${ColorBoldGreen}-D${PROJECT_NAME}_BUILD_MISSING_DEPS=all${ColorReset}")
    endif ()
    message (STATUS)
    message (STATUS "${ColorBoldYellow}=========================================================================${ColorReset}")
    message (STATUS)
endfunction ()



# Helper: called if a package is not found, print error messages, including
# a fatal error if the package was required.
function (handle_package_notfound pkgname required)
    message (STATUS "${ColorRed}${pkgname} library not found ${ColorReset}")
    if (${pkgname}_ROOT)
        message (STATUS "    ${pkgname}_ROOT was: ${${pkgname}_ROOT}")
    elseif ($ENV{${pkgname}_ROOT})
        message (STATUS "    ENV ${pkgname}_ROOT was: ${${pkgname}_ROOT}")
    else ()
        message (STATUS "    Try setting ${pkgname}_ROOT ?")
    endif ()
    if (EXISTS "${PROJECT_SOURCE_DIR}/src/build-scripts/build_${pkgname}.bash")
        message (STATUS "    Maybe this will help:  src/build-scripts/build_${pkgname}.bash")
    elseif (EXISTS "${PROJECT_SOURCE_DIR}/src/build-scripts/build_${pkgname_upper}.bash")
        message (STATUS "    Maybe this will help:  src/build-scripts/build_${pkgname_upper}.bash")
    elseif (EXISTS "${PROJECT_SOURCE_DIR}/src/build-scripts/build_${pkgname_lower}.bash")
            message (STATUS "    Maybe this will help:  src/build-scripts/build_${pkgname_lower}.bash")
    elseif (EXISTS "${PROJECT_SOURCE_DIR}/src/build-scripts/build_lib${pkgname_lower}.bash")
            message (STATUS "    Maybe this will help:  src/build-scripts/build_lib${pkgname_lower}.bash")
    endif ()
    if (required)
        print_package_notfound_report()
        message (FATAL_ERROR "${pkgname} is required, aborting.")
    endif ()
endfunction ()


# Check whether the package's version (in pkgversion) lies within versionmin
# and versionmax (inclusive). Store TRUE result variable if the version was in
# range, FALSE if it was out of range. If it did not match, clear a bunch of
# variables that may have been set by the find_package call (including
# clearing the package's FOUND variable).
function (reject_out_of_range_versions pkgname pkgversion versionmin versionmax result)
    set (${result} FALSE PARENT_SCOPE)
    string (TOUPPER ${pkgname} pkgname_upper)
    # message (STATUS "roorv: ${pkgname} ${pkgversion} ${versionmin} ${versionmax}")
    if (NOT ${pkgname}_FOUND AND NOT ${pkgname_upper}_FOUND)
        message (STATUS "${pkgname} was not found")
    elseif ("${pkgversion}" STREQUAL "")
        message (ERROR "${pkgname} found but version was empty")
    elseif (pkgversion VERSION_LESS versionmin
            OR pkgversion VERSION_GREATER versionmax)
        # message (STATUS "${ColorRed}${pkgname} ${pkgversion} is outside the required range ${versionmin}...${versionmax} ${ColorReset}")
        # list (APPEND CFP_ALL_BUILD_DEPS_BADVERSION ${pkgname})
        # if (${pkgname}_local_build_script_exists)
        #     list (APPEND CFP_LOCALLY_BUILDABLE_DEPS_BADVERSION ${pkgname})
        # endif ()
        if (versionmax VERSION_GREATER_EQUAL 10000)
            set (${pkgname}_NOT_FOUND_EXPLANATION
                 "(found ${pkgversion}, needed >= ${versionmin})" PARENT_SCOPE)
        else ()
            set (${pkgname}_NOT_FOUND_EXPLANATION
                 "(found ${pkgversion}, needed ${versionmin} ... ${versionmax})" PARENT_SCOPE)
        endif ()
        unset (${pkgname}_FOUND PARENT_SCOPE)
        unset (${pkgname}_VERSION PARENT_SCOPE)
        unset (${pkgname}_INCLUDE PARENT_SCOPE)
        unset (${pkgname}_INCLUDES PARENT_SCOPE)
        unset (${pkgname}_LIBRARY PARENT_SCOPE)
        unset (${pkgname}_LIBRARIES PARENT_SCOPE)
        unset (${pkgname_upper}_FOUND PARENT_SCOPE)
        unset (${pkgname_upper}_VERSION PARENT_SCOPE)
        unset (${pkgname_upper}_INCLUDE PARENT_SCOPE)
        unset (${pkgname_upper}_INCLUDES PARENT_SCOPE)
        unset (${pkgname_upper}_LIBRARY PARENT_SCOPE)
        unset (${pkgname_upper}_LIBRARIES PARENT_SCOPE)
    else ()
        # Version matched the range
        set (${result} TRUE PARENT_SCOPE)
        # message (STATUS "${pkgname} ${pkgversion} is INSIDE the required range ${versionmin}...${versionmax}")
    endif ()
endfunction ()



# checked_find_package(Pkgname ...) is a wrapper for find_package, with the
# following extra features:
#   * If either `USE_Pkgname` or the all-uppercase `USE_PKGNAME` (or
#     `ENABLE_Pkgname` or `ENABLE_PKGNAME`) exists as either a CMake or
#     environment variable, is nonempty by contains a non-true/nonzero
#     value, do not search for or use the package. The optional ENABLE <var>
#     arguments allow you to override the name of the enabling variable. In
#     other words, support for the dependency is presumed to be ON, unless
#     turned off explicitly from one of these sources.
#   * Print a message if the package is enabled but not found. This is based
#     on ${Pkgname}_FOUND or $PKGNAME_FOUND.
#   * Optional DEFINITIONS <string>... are passed to add_compile_definitions
#     if the package is found.
#   * Optional SETVARIABLES <id>... is a list of CMake variables to set to
#     TRUE if the package is found (they will not be set or changed if the
#     package is not found).
#   * Optional PRINT <list> is a list of variables that will be printed
#     if the package is found, if VERBOSE is on.
#   * Optional DEPS <list> is a list of hard dependencies; for each one, if
#     dep_FOUND is not true, disable this package with an error message.
#   * Optional ISDEPOF <downstream> names another package for which the
#     present package is only needed because it's a dependency, and
#     therefore if <downstream> is disabled, we don't bother with this
#     package either.
#   * Optional VERSION_MIN and VERSION_MAX, if supplied, give minimum and
#     maximum versions that will be accepted. The min is inclusive, the max
#     is exclusive (i.e., check for min <= version < max). Note that this is
#     not the same as providing a version number to find_package, which
#     checks compatibility, not minimum. Sometimes we really do just want to
#     say a minimum or a range. (N.B. When our minimum CMake >= 3.19, the
#     built-in way to do this is with version ranges passed to
#     find_package.)
#   * Optional RECOMMEND_MIN, if supplied, gives a minimum recommended
#     version, accepting but warning if it is below this number (even
#     if above the true minimum version accepted). The warning message
#     can give an optional explanation, passed as RECOMMEND_MIN_REASON.
#   * Optional CONFIG, if supplied, only accepts the package from an
#     exported config and never uses a FindPackage.cmake module.
#   * Optional PREFER_CONFIG, if supplied, tries to use an exported config
#     file from the package before using a FindPackage.cmake module.
#   * Optional DEBUG turns on extra debugging information related to how
#     this package is found.
#   * Found package "name version" or "name NONE" are accumulated in the list
#     CFP_ALL_BUILD_DEPS_FOUND. If the optional NO_RECORD_NOTFOUND is
#     supplied, un-found packags will not be recorded.
#   * Optional BUILD_LOCAL, if supplied, if followed by a token that specifies
#     the conditions under which to build the package locally by including a
#     script included in src/cmake/build_${pkgname}.cmake. If the condition is
#     "always", it will attempt to do so unconditionally. If "missing", it
#     will only do so if the package is not found. Also note that if the
#     global ${PROJECT_NAME}_BUILD_LOCAL_DEPS contains the package name or
#     is "all", it will behave as if set to "always", and if the variable
#     ${PROJECT_NAME}_BUILD_MISSING_DEPS contains the package name or is
#     "all", it will behave as if set to "missing".
#   * Optional NO_FP_RANGE_CHECK avoids passing the version range to
#     find_package itself.
#
# Explanation about local builds:
#
# If the package isn't found externally in the usual places or doesn't meet
# the version criteria, we check for the existance of a file at
# `src/build-scripts/build_<pkgname>.cmake`. If that exists, we include and
# execute it. The script can do whatever it wants, but should either (a)
# somehow set up the link targets that would have been found had the package
# had been found, or (b) set the variable `<pkgname>_REFIND` to a true value
# and have done something to ensure that the package will be found if we try a
# second time. For (b), typically that might mean downloading the package and
# building it locally, and then setting the `<pkgname>_ROOT` to where it's
# installed.
#
# N.B. This needs to be a macro, not a function, because the find modules
# will set(blah val PARENT_SCOPE) and we need that to be the global scope,
# not merely the scope for this function.
macro (checked_find_package pkgname)
    #
    # Various setup logic
    #
    cmake_parse_arguments(_pkg   # prefix
        # noValueKeywords:
        "REQUIRED;CONFIG;PREFER_CONFIG;DEBUG;NO_RECORD_NOTFOUND;NO_FP_RANGE_CHECK"
        # singleValueKeywords:
        "ENABLE;ISDEPOF;VERSION_MIN;VERSION_MAX;RECOMMEND_MIN;RECOMMEND_MIN_REASON;BUILD_LOCAL"
        # multiValueKeywords:
        "DEFINITIONS;PRINT;DEPS;SETVARIABLES"
        # argsToParse:
        ${ARGN})
    string (TOLOWER ${pkgname} pkgname_lower)
    string (TOUPPER ${pkgname} pkgname_upper)
    set (_pkg_VERBOSE ${VERBOSE})
    if (_pkg_DEBUG)
        set (_pkg_VERBOSE ON)
    endif ()
    if (NOT _pkg_VERBOSE)
        set (${pkgname}_FIND_QUIETLY true)
        set (${pkgname_upper}_FIND_QUIETLY true)
    endif ()
    if ("${pkgname}" IN_LIST ${PROJECT_NAME}_REQUIRED_DEPS OR "ALL" IN_LIST ${PROJECT_NAME}_REQUIRED_DEPS)
        set (_pkg_REQUIRED 1)
    endif ()
    if ("${pkgname}" IN_LIST ${PROJECT_NAME}_OPTIONAL_DEPS OR "ALL" IN_LIST ${PROJECT_NAME}_OPTIONAL_DEPS)
        set (_pkg_REQUIRED 0)
    endif ()
    # string (TOLOWER "${_pkg_BUILD_LOCAL}" _pkg_BUILD_LOCAL)
    if ("${pkgname}" IN_LIST ${PROJECT_NAME}_BUILD_LOCAL_DEPS
        OR ${PROJECT_NAME}_BUILD_LOCAL_DEPS STREQUAL "all")
        set (_pkg_BUILD_LOCAL "always")
    elseif ("${pkgname}" IN_LIST ${PROJECT_NAME}_BUILD_MISSING_DEPS
            OR ${PROJECT_NAME}_BUILD_MISSING_DEPS STREQUAL "all")
        set_if_not (_pkg_BUILD_LOCAL "missing")
    endif ()
    set (${pkgname}_local_build_script "${PROJECT_SOURCE_DIR}/src/cmake/build_${pkgname}.cmake")
    if (EXISTS ${${pkgname}_local_build_script})
        set (${pkgname}_local_build_script_exists TRUE)
    endif ()
    if (_pkg_BUILD_LOCAL AND NOT EXISTS "${${pkgname}_local_build_script}")
        unset (_pkg_BUILD_LOCAL)
    endif ()
    set (_quietskip false)
    check_is_enabled (${pkgname} _enable)
    set (_disablereason "")
    foreach (_dep ${_pkg_DEPS})
        if (_enable AND NOT ${_dep}_FOUND)
            set (_enable false)
            set (ENABLE_${pkgname} OFF PARENT_SCOPE)
            set (_disablereason "(because ${_dep} was not found)")
        endif ()
    endforeach ()
    if (_pkg_ISDEPOF)
        check_is_enabled (${_pkg_ISDEPOF} _dep_enabled)
        if (NOT _dep_enabled)
            set (_enable false)
            set (_quietskip true)
        endif ()
    endif ()
    set (_config_status "")
    unset (_${pkgname}_version_range)
    if (_pkg_BUILD_LOCAL AND NOT _pkg_NO_FP_RANGE_CHECK)
        # SKIP THIS -- I find it unreliable because the package's exported
        # PKGConfigVersion.cmake has might have arbitrary rules. Use our own
        # MIN_VERSION and MAX_VERSION parameters to manually check instead.
        #
        if (_pkg_VERSION_MIN AND _pkg_VERSION_MAX AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.19)
            set (_${pkgname}_version_range "${_pkg_VERSION_MIN}...<${_pkg_VERSION_MAX}")
        elseif (_pkg_VERSION_MIN)
            set (_${pkgname}_version_range "${_pkg_VERSION_MIN}")
        endif ()
    endif ()
    set_if_not (_pkg_VERSION_MIN "0.0.1")
    set_if_not (_pkg_VERSION_MAX "10000.0.0")
    #
    # Now we try to find or build
    #
    set (${pkgname}_FOUND FALSE)
    set (${pkgname}_LOCAL_BUILD FALSE)
    if (_enable OR _pkg_REQUIRED)
        # Unless instructed not to, try to find the package externally
        # installed.
        if (${pkgname}_FOUND OR ${pkgname_upper}_FOUND OR _pkg_BUILD_LOCAL STREQUAL "always")
            # was already found, or we're forcing a local build
        elseif (_pkg_CONFIG OR _pkg_PREFER_CONFIG OR ${PROJECT_NAME}_ALWAYS_PREFER_CONFIG)
            find_package (${pkgname} ${_${pkgname}_version_range} CONFIG ${_pkg_UNPARSED_ARGUMENTS})
            reject_out_of_range_versions (${pkgname} "${${pkgname}_VERSION}"
                                          ${_pkg_VERSION_MIN} ${_pkg_VERSION_MAX}
                                          _pkg_version_in_range)
            if (${pkgname}_FOUND OR ${pkgname_upper}_FOUND)
                set (_config_status "from CONFIG")
            endif ()
        endif ()
        if (NOT ${pkgname}_FOUND AND NOT ${pkgname_upper}_FOUND AND NOT _pkg_BUILD_LOCAL STREQUAL "always" AND NOT _pkg_CONFIG)
            find_package (${pkgname} ${_${pkgname}_version_range} ${_pkg_UNPARSED_ARGUMENTS})
        endif()
        if (NOT ${pkgname}_FOUND AND NOT ${pkgname_upper}_FOUND)
            list (APPEND CFP_ALL_BUILD_DEPS_NOTFOUND ${pkgname})
            if (${pkgname}_local_build_script_exists)
                list (APPEND CFP_LOCALLY_BUILDABLE_DEPS_NOTFOUND ${pkgname})
            endif ()
        endif ()
        # Some FindPackage modules set nonstandard variables for the versions
        if (NOT ${pkgname}_VERSION AND ${pkgname_upper}_VERSION)
            set (${pkgname}_VERSION ${${pkgname_upper}_VERSION})
        endif ()
        if (NOT ${pkgname}_VERSION AND ${pkgname_upper}_VERSION_STRING)
            set (${pkgname}_VERSION ${${pkgname_upper}_VERSION_STRING})
        endif ()
        # If the package was found but the version is outside the required
        # range, unset the relevant variables so that we can try again fresh.
        if ((${pkgname}_FOUND OR ${pkgname_upper}_FOUND) AND ${pkgname}_VERSION)
            reject_out_of_range_versions (${pkgname} ${${pkgname}_VERSION}
                                          ${_pkg_VERSION_MIN} ${_pkg_VERSION_MAX}
                                          _pkg_version_in_range)
            if (_pkg_version_in_range)
                list (APPEND CFP_EXTERNAL_BUILD_DEPS_FOUND ${pkgname})
            else ()
                message (STATUS "${ColorRed}${pkgname} ${${pkgname}_VERSION} is outside the required range ${_pkg_VERSION_MIN}...${_pkg_VERSION_MAX} ${ColorReset}")
                list (APPEND CFP_ALL_BUILD_DEPS_BADVERSION ${pkgname})
                if (${pkgname}_local_build_script_exists)
                    list (APPEND CFP_LOCALLY_BUILDABLE_DEPS_BADVERSION ${pkgname})
                endif ()
            endif ()
        endif ()
        # If we haven't found the package yet and are allowed to build a local
        # version, and a build_<pkgname>.cmake exists, include it to build the
        # package locally.
        if (NOT ${pkgname}_FOUND AND NOT ${pkgname_upper}_FOUND
            AND (_pkg_BUILD_LOCAL STREQUAL "always" OR _pkg_BUILD_LOCAL STREQUAL "missing")
            AND EXISTS "${${pkgname}_local_build_script}")
            message (STATUS "${ColorMagenta}Building package ${pkgname} ${${pkgname}_VERSION} locally${ColorReset}")
            list(APPEND CMAKE_MESSAGE_INDENT "        ")
            include("${${pkgname}_local_build_script}")
            list(POP_BACK CMAKE_MESSAGE_INDENT)
            # set (${pkgname}_FOUND TRUE)
            set (${pkgname}_LOCAL_BUILD TRUE)
            list (APPEND CFP_LOCALLY_BUILT_DEPS ${pkgname})
            list (REMOVE_ITEM CFP_LOCALLY_BUILDABLE_DEPS_NOTFOUND ${pkgname})
        endif()
        # If the local build instrctions set <pkgname>_REFIND, then try a find
        # again to pick up the local one, at which point we can proceed as if
        # it had been found externally all along. The local build script can
        # also optionally set the following hints:
        #   ${pkgname}_REFIND_VERSION : the version that was just installed,
        #                               to specifically find.
        #   ${pkgname}_REFIND_ARGS    : additional arguments to pass to find_package
        if (${pkgname}_REFIND)
            message (STATUS "Refinding ${pkgname} with ${pkgname}_ROOT=${${pkgname}_ROOT}")
            find_package (${pkgname} ${${pkgname}_REFIND_VERSION} REQUIRED ${_pkg_UNPARSED_ARGUMENTS} ${${pkgname}_REFIND_ARGS})
            unset (${pkgname}_REFIND)
        endif()
        # It's all downhill from here: if we found the package, follow the
        # various instructions we got about variables to set, compile
        # definitions to add, etc.
        if (${pkgname}_FOUND OR ${pkgname_upper}_FOUND)
            foreach (_vervar ${pkgname_upper}_VERSION ${pkgname}_VERSION_STRING
                             ${pkgname_upper}_VERSION_STRING)
                if (NOT ${pkgname}_VERSION AND ${_vervar})
                    set (${pkgname}_VERSION ${${_vervar}})
                endif ()
            endforeach ()
            message (STATUS "${ColorGreen}Found ${pkgname} ${${pkgname}_VERSION} ${_config_status}${ColorReset}")
            add_compile_definitions (${_pkg_DEFINITIONS})
            foreach (_v IN LISTS _pkg_SETVARIABLES)
                set (${_v} TRUE)
            endforeach ()
            if (_pkg_RECOMMEND_MIN)
                if (${${pkgname}_VERSION} VERSION_LESS ${_pkg_RECOMMEND_MIN})
                    message (STATUS "${ColorYellow}Recommend ${pkgname} >= ${_pkg_RECOMMEND_MIN} ${_pkg_RECOMMEND_MIN_REASON} ${ColorReset}")
                endif ()
            endif ()
            string (STRIP "${pkgname} ${${pkgname}_VERSION}" app_)
            list (APPEND CFP_ALL_BUILD_DEPS_FOUND "${app_}")
        else ()
            handle_package_notfound (${pkgname} ${_pkg_REQUIRED})
            if (NOT _pkg_NO_RECORD_NOTFOUND)
                list (APPEND CFP_ALL_BUILD_DEPS_FOUND "${pkgname} NONE")
            endif ()
        endif()
        if (_pkg_VERBOSE AND (${pkgname}_FOUND OR ${pkgname_upper}_FOUND OR _pkg_DEBUG))
            if (_pkg_DEBUG)
                dump_matching_variables (${pkgname})
            endif ()
            set (_vars_to_print ${pkgname}_INCLUDES ${pkgname_upper}_INCLUDES
                                ${pkgname}_INCLUDE_DIR ${pkgname_upper}_INCLUDE_DIR
                                ${pkgname}_INCLUDE_DIRS ${pkgname_upper}_INCLUDE_DIRS
                                ${pkgname}_LIBRARIES ${pkgname_upper}_LIBRARIES
                                ${_pkg_PRINT})
            list (REMOVE_DUPLICATES _vars_to_print)
            foreach (_v IN LISTS _vars_to_print)
                if (NOT "${${_v}}" STREQUAL "")
                    message (STATUS "    ${_v} = ${${_v}}")
                endif ()
            endforeach ()
        endif ()
    else ()
        if (NOT _quietskip)
            message (STATUS "${ColorRed}Not using ${pkgname} -- disabled ${_disablereason} ${ColorReset}")
        endif ()
    endif ()
    # unset (_${pkgname}_version_range)
endmacro()




# Function: remove_prefixes_from_variable
# Removes specified directory prefixes from a given environment variable or CMake variable.
#
# Parameters:
#   VAR_TYPE  - Type of variable to modify:
#               'ENV'   for environment variables
#               'CMAKE' for CMake variables
#   VAR_NAME  - Name of the variable to modify.
#   PREFIXES  - List of directory prefixes to remove from the variable's value.
#
# Description:
#   This function updates the specified variable by removing any paths that start
#   with the given prefixes. It is useful for excluding certain directories
#   (e.g., Homebrew paths) from environment variables or CMake variables to prevent
#   unintended dependencies during the build process.
#
# Usage Example:
#   remove_prefixes_from_variable(ENV LD_LIBRARY_PATH "/opt/homebrew" "/usr/local")
#   remove_prefixes_from_variable(CMAKE CMAKE_PREFIX_PATH "${HOMEBREW_PREFIXES}")
function(remove_prefixes_from_variable VAR_TYPE VAR_NAME PREFIXES)
    # Retrieve the original value based on the variable type
    if(VAR_TYPE STREQUAL "ENV")
        if(DEFINED ENV{${VAR_NAME}})
            set(_original_value "$ENV{${VAR_NAME}}")
        else()
            return()
        endif()
    elseif(VAR_TYPE STREQUAL "CMAKE")
        if(DEFINED ${VAR_NAME})
            set(_original_value "${${VAR_NAME}}")
        else()
            return()
        endif()
    else()
        message(FATAL_ERROR "Invalid VAR_TYPE: ${VAR_TYPE}. Expected 'ENV' or 'CMAKE'.")
    endif()

    # Convert the variable value into a list of paths
    string(REPLACE ":" ";" _path_list "${_original_value}")

    foreach(_prefix ${PREFIXES})
        # Normalize the prefix path
        file(TO_CMAKE_PATH "${_prefix}" _norm_prefix)
        foreach(_path IN LISTS _path_list)
            # Normalize paths to avoid issues with different formats
            file(TO_CMAKE_PATH "${_path}" _norm_path)
            # Check if the normalized path starts with the normalized prefix
            string(FIND "${_norm_path}" "${_norm_prefix}" _pos)
            if(_pos EQUAL 0)
                list(REMOVE_ITEM _path_list "${_path}")
            endif()
        endforeach()
    endforeach()

    # Convert the list back to the appropriate separator
    string(REPLACE ";" ":" _new_value "${_path_list}")

    # Update the variable based on the variable type
    if(VAR_TYPE STREQUAL "ENV")
        set(ENV{${VAR_NAME}} "${_new_value}")
        message(STATUS "${ColorYellow}Updated environment variable ${VAR_NAME}: ${_new_value}")
    else()
        set(${VAR_NAME} "${_new_value}" PARENT_SCOPE)
        message(STATUS "${ColorYellow}Updated CMake variable ${VAR_NAME}: ${_new_value}")
    endif()
endfunction()

# Helper to build a dependency with CMake. Given a package name, git repo and
# tag, and optional cmake args, it will clone the repo into the surrounding
# project's build area, configures, and build sit, and installs it into a
# special dist area (unless the NOINSTALL option is given).
#
# After running, it leaves the following variables set:
#   ${pkgname}_LOCAL_SOURCE_DIR
#   ${pkgname}_LOCAL_BUILD_DIR
#   ${pkgname}_LOCAL_INSTALL_DIR
#
# Unless NOINSTALL is specified, the after the installation step, the
# installation directory will be added to the CMAKE_PREFIX_PATH and also will
# be stored in the ${pkgname}_ROOT variable.
#
macro (build_dependency_with_cmake pkgname)
    cmake_parse_arguments(_pkg   # prefix
        # noValueKeywords:
        "NOINSTALL"
        # singleValueKeywords:
        "GIT_REPOSITORY;GIT_TAG;VERSION;SOURCE_SUBDIR"
        # multiValueKeywords:
        "CMAKE_ARGS"
        # argsToParse:
        ${ARGN})

    message (STATUS "Building local ${pkgname} ${_pkg_VERSION} from ${_pkg_GIT_REPOSITORY}")

    set (${pkgname}_LOCAL_SOURCE_DIR "${${PROJECT_NAME}_LOCAL_DEPS_ROOT}/${pkgname}")
    set (${pkgname}_LOCAL_BUILD_DIR "${${PROJECT_NAME}_LOCAL_DEPS_ROOT}/${pkgname}-build")
    set (${pkgname}_LOCAL_INSTALL_DIR "${${PROJECT_NAME}_LOCAL_DEPS_ROOT}/dist")
    message (STATUS "Downloading local ${_pkg_GIT_REPOSITORY}")

    set (_pkg_quiet OUTPUT_QUIET)

    # Clone the repo if we don't already have it
    find_package (Git REQUIRED)
    if (NOT IS_DIRECTORY ${${pkgname}_LOCAL_SOURCE_DIR})
        execute_process(COMMAND ${GIT_EXECUTABLE} clone ${_pkg_GIT_REPOSITORY}
                                -b ${_pkg_GIT_TAG} --depth 1 -q
                                ${${pkgname}_LOCAL_SOURCE_DIR}
                        ${_pkg_quiet})
        if (NOT IS_DIRECTORY ${${pkgname}_LOCAL_SOURCE_DIR})
            message (FATAL_ERROR "Could not download ${_pkg_GIT_REPOSITORY}")
        endif ()
    endif ()
    execute_process(COMMAND ${GIT_EXECUTABLE} checkout ${_pkg_GIT_TAG}
                    WORKING_DIRECTORY ${${pkgname}_LOCAL_SOURCE_DIR}
                    ${_pkg_quiet})

    # Configure the package
    if (${PROJECT_NAME}_DEPENDENCY_BUILD_VERBOSE)
        set (_pkg_cmake_verbose -DCMAKE_VERBOSE_MAKEFILE=ON
                                -DCMAKE_MESSAGE_LOG_LEVEL=VERBOSE
                                -DCMAKE_RULE_MESSAGES=ON
                                )
    else ()
        set (_pkg_cmake_verbose -DCMAKE_VERBOSE_MAKEFILE=OFF
                                -DCMAKE_MESSAGE_LOG_LEVEL=ERROR
                                -DCMAKE_RULE_MESSAGES=OFF
                                -Wno-dev
                                )
    endif ()

    # Make sure to inherit CMAKE_IGNORE_PATH
    set(_pkg_CMAKE_ARGS ${_pkg_CMAKE_ARGS} ${_pkg_CMAKE_ARGS})
    if (CMAKE_IGNORE_PATH)
        string(REPLACE ";" "\\;" CMAKE_IGNORE_PATH_ESCAPED "${CMAKE_IGNORE_PATH}")
        list(APPEND _pkg_CMAKE_ARGS "-DCMAKE_IGNORE_PATH=${CMAKE_IGNORE_PATH_ESCAPED}")
    endif()

    # Pass along any CMAKE_MSVC_RUNTIME_LIBRARY
    if (WIN32 AND CMAKE_MSVC_RUNTIME_LIBRARY)
        list (APPEND _pkg_CMAKE_ARGS -DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY})
    endif ()

    execute_process (COMMAND
        ${CMAKE_COMMAND}
            # Put things in our special local build areas
                -S ${${pkgname}_LOCAL_SOURCE_DIR}/${_pkg_SOURCE_SUBDIR}
                -B ${${pkgname}_LOCAL_BUILD_DIR}
                -DCMAKE_INSTALL_PREFIX=${${pkgname}_LOCAL_INSTALL_DIR}
            # Same build type as us
                -DCMAKE_BUILD_TYPE=${${PROJECT_NAME}_DEPENDENCY_BUILD_TYPE}
            # Shhhh
                -DCMAKE_MESSAGE_INDENT="        "
                -DCMAKE_COMPILE_WARNING_AS_ERROR=OFF
                ${_pkg_cmake_verbose}
            # Build args passed by caller
                ${_pkg_CMAKE_ARGS}
        ${pkg_quiet}
        )

    # Build the package
    execute_process (COMMAND ${CMAKE_COMMAND}
                        --build ${${pkgname}_LOCAL_BUILD_DIR}
                        --config ${${PROJECT_NAME}_DEPENDENCY_BUILD_TYPE}
                     ${pkg_quiet}
                    )

    # Install the project, unless instructed not to do so
    if (NOT _pkg_NOINSTALL)
        execute_process (COMMAND ${CMAKE_COMMAND}
                            --build ${${pkgname}_LOCAL_BUILD_DIR}
                            --config ${${PROJECT_NAME}_DEPENDENCY_BUILD_TYPE}
                            --target install
                         ${pkg_quiet}
                        )
        set (${pkgname}_ROOT ${${pkgname}_LOCAL_INSTALL_DIR})
        list (APPEND CMAKE_PREFIX_PATH ${${pkgname}_LOCAL_INSTALL_DIR})
    endif ()
endmacro ()


# Copy libraries from a locally-built dependency into our own install area.
# This is useful for dynamic libraries that we need to be part of our own
# installation.
macro (install_local_dependency_libs pkgname libname)
    # We need to include the Imath dynamic libraries in our own install.
    # get_target_property(_lib_files Imath::Imath INTERFACE_LINK_LIBRARIES)
    set (patterns ${ARGN})
    file (GLOB _lib_files
            "${${pkgname}_LOCAL_INSTALL_DIR}/lib/*${libname}*"
            "${${pkgname}_LOCAL_INSTALL_DIR}/lib/${${PROJECT_NAME}_DEPENDENCY_BUILD_TYPE}/*${libname}*"
         )
    install (FILES ${_lib_files} TYPE LIB COMPONENT user)
    # message("${pkgname}_LOCAL_INSTALL_DIR = ${${pkgname}_LOCAL_INSTALL_DIR}")
    # message("  lib files = ${_lib_files}")
    if (WIN32)
        # On Windows, check for DLLs, which go in the bin directory
        file (GLOB _lib_files
                "${${pkgname}_LOCAL_INSTALL_DIR}/bin/*${libname}*.dll"
                "${${pkgname}_LOCAL_INSTALL_DIR}/bin/${${PROJECT_NAME}_DEPENDENCY_BUILD_TYPE}/*${libname}*.dll"
             )
        # message("  dll files = ${_lib_files}")
        install (FILES ${_lib_files} TYPE BIN COMPONENT user)
    endif ()
    unset (_lib_files)
endmacro ()


# If the target `newalias` doesn't yet exist but `realtarget` does, create an
# alias for `newalias` to mean the real target.
macro (alias_library_if_not_exists newalias realtarget)
    if (NOT TARGET ${newalias} AND TARGET ${realtarget})
        add_library(${newalias} ALIAS ${realtarget})
    endif ()
endmacro ()
