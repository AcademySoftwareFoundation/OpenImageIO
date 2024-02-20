# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


set (REQUIRED_DEPS "" CACHE STRING
     "Additional dependencies to consider required (semicolon-separated list, or ALL)")
set (OPTIONAL_DEPS "" CACHE STRING
     "Additional dependencies to consider optional (semicolon-separated list, or ALL)")
option (ALWAYS_PREFER_CONFIG "Prefer a dependency's exported config file if it's available" OFF)

# Track all build deps we find with checked_find_package
set (CFP_ALL_BUILD_DEPS_FOUND "")

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
#   * Optional DEFINITIONS <string>... are passed to add_definitions if the
#     package is found.
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
#   * Optional PREFER_CONFIG, if supplied, tries to use an exported config
#     file from the package before using a FindPackage.cmake module.
#   * Optional DEBUG turns on extra debugging information related to how
#     this package is found.
#   * Found package "name version" or "name NONE" are accumulated in the list
#     CFP_ALL_BUILD_DEPS_FOUND. If the optional NO_RECORD_NOTFOUND is
#     supplied, un-found packags will not be recorded.
#
# N.B. This needs to be a macro, not a function, because the find modules
# will set(blah val PARENT_SCOPE) and we need that to be the global scope,
# not merely the scope for this function.
macro (checked_find_package pkgname)
    cmake_parse_arguments(_pkg   # prefix
        # noValueKeywords:
        "REQUIRED;PREFER_CONFIG;DEBUG;NO_RECORD_NOTFOUND"
        # singleValueKeywords:
        "ENABLE;ISDEPOF;VERSION_MIN;VERSION_MAX;RECOMMEND_MIN;RECOMMEND_MIN_REASON"
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
    if ("${pkgname}" IN_LIST REQUIRED_DEPS OR "ALL" IN_LIST REQUIRED_DEPS)
        set (_pkg_REQUIRED 1)
    endif ()
    if ("${pkgname}" IN_LIST OPTIONAL_DEPS OR "ALL" IN_LIST OPTIONAL_DEPS)
        set (_pkg_REQUIRED 0)
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
    if (_enable OR _pkg_REQUIRED)
        if (${pkgname}_FOUND OR ${pkgname_upper}_FOUND)
            # was already found
        elseif (_pkg_PREFER_CONFIG OR ALWAYS_PREFER_CONFIG)
            find_package (${pkgname} CONFIG ${_pkg_UNPARSED_ARGUMENTS})
            if (${pkgname}_FOUND OR ${pkgname_upper}_FOUND)
                set (_config_status "from CONFIG")
            endif ()
        endif ()
        if (NOT (${pkgname}_FOUND OR ${pkgname_upper}_FOUND))
            find_package (${pkgname} ${_pkg_UNPARSED_ARGUMENTS})
        endif()
        if ((${pkgname}_FOUND OR ${pkgname_upper}_FOUND)
              AND ${pkgname}_VERSION
              AND (_pkg_VERSION_MIN OR _pkg_VERSION_MAX))
            if ((_pkg_VERSION_MIN AND ${pkgname}_VERSION VERSION_LESS _pkg_VERSION_MIN)
                  OR (_pkg_VERSION_MAX AND ${pkgname}_VERSION VERSION_GREATER _pkg_VERSION_MAX))
                message (STATUS "${ColorRed}${pkgname} ${${pkgname}_VERSION} is outside the required range ${_pkg_VERSION_MIN}...${_pkg_VERSION_MAX} ${ColorReset}")
                unset (${pkgname}_FOUND)
                unset (${pkgname}_VERSION)
                unset (${pkgname_upper}_FOUND)
                unset (${pkgname_upper}_VERSION)
            endif ()
        endif ()
        if (${pkgname}_FOUND OR ${pkgname_upper}_FOUND)
            foreach (_vervar ${pkgname_upper}_VERSION ${pkgname}_VERSION_STRING
                             ${pkgname_upper}_VERSION_STRING)
                if (NOT ${pkgname}_VERSION AND ${_vervar})
                    set (${pkgname}_VERSION ${${_vervar}})
                endif ()
            endforeach ()
            message (STATUS "${ColorGreen}Found ${pkgname} ${${pkgname}_VERSION} ${_config_status}${ColorReset}")
            add_definitions (${_pkg_DEFINITIONS})
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
            message (STATUS "${ColorRed}${pkgname} library not found ${ColorReset}")
            if (${pkgname}_ROOT)
                message (STATUS "${ColorRed}    ${pkgname}_ROOT was: ${${pkgname}_ROOT} ${ColorReset}")
            elseif ($ENV{${pkgname}_ROOT})
                message (STATUS "${ColorRed}    ENV ${pkgname}_ROOT was: ${${pkgname}_ROOT} ${ColorReset}")
            else ()
                message (STATUS "${ColorRed}    Try setting ${pkgname}_ROOT ? ${ColorReset}")
            endif ()
            if (EXISTS "${PROJECT_SOURCE_DIR}/src/build-scripts/build_${pkgname}.bash")
                message (STATUS "${ColorRed}    Maybe this will help:  src/build-scripts/build_${pkgname}.bash ${ColorReset}")
            elseif (EXISTS "${PROJECT_SOURCE_DIR}/src/build-scripts/build_${pkgname_upper}.bash")
                message (STATUS "${ColorRed}    Maybe this will help:  src/build-scripts/build_${pkgname_upper}.bash ${ColorReset}")
            elseif (EXISTS "${PROJECT_SOURCE_DIR}/src/build-scripts/build_${pkgname_lower}.bash")
                    message (STATUS "${ColorRed}    Maybe this will help:  src/build-scripts/build_${pkgname_lower}.bash ${ColorReset}")
            elseif (EXISTS "${PROJECT_SOURCE_DIR}/src/build-scripts/build_lib${pkgname_lower}.bash")
                    message (STATUS "${ColorRed}    Maybe this will help:  src/build-scripts/build_lib${pkgname_lower}.bash ${ColorReset}")
            endif ()
            if (_pkg_REQUIRED)
                message (FATAL_ERROR "${ColorRed}${pkgname} is required, aborting.${ColorReset}")
            endif ()
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
endmacro()

