# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md


set (REQUIED_DEPS "" CACHE STRING
     "Additional dependencies to consider required (semicolon-separated list, or ALL)")
set (OPTIONAL_DEPS "" CACHE STRING
     "Additional dependencies to consider optional (semicolon-separated list, or ALL)")


# checked_find_package(pkgname ..) is a wrapper for find_package, with the
# following extra features:
#   * If either `USE_pkgname` or the all-uppercase `USE_PKGNAME` (or
#     `ENABLE_pkgname` or `ENABLE_PKGNAME`) exists as either a CMake or
#     environment variable, is nonempty by contains a non-true/nonzero
#     value, do not search for or use the package. The optional ENABLE <var>
#     arguments allow you to override the name of the enabling variable. In
#     other words, support for the dependency is presumed to be ON, unless
#     turned off explicitly from one of these sources.
#   * Print a message if the package is enabled but not found. This is based
#     on ${pkgname}_FOUND or $PKGNNAME_FOUND.
#   * Optional DEFINITIONS <string> are passed to add_definitions if the
#     package is found.
#   * Optional PRINT <list> is a list of variables that will be printed
#     if the package is found, if VERBOSE is on.
#   * Optional DEPS <list> is a list of hard dependencies; for each one, if
#     dep_FOUND is not true, disable this package with an error message.
#   * Optional ISDEPOF <downstream> names another package for which the
#     present package is only needed because it's a dependency, and
#     therefore if <downstream> is disabled, we don't bother with this
#     package either.
#   * Optional RECOMMEND_MIN, if supplied, gives a minimum recommended
#     version, accepting but warning if it is below this number (even
#     if above the true minimum version accepted). The warning message
#     can give an optional explanation, passed as RECOMMEND_MIN_REASON.
#
# N.B. This needs to be a macro, not a function, because the find modules
# will set(blah val PARENT_SCOPE) and we need that to be the global scope,
# not merely the scope for this function.
macro (checked_find_package pkgname)
    cmake_parse_arguments(_pkg   # prefix
        # noValueKeywords:
        "REQUIRED"
        # singleValueKeywords:
        "ENABLE;ISDEPOF;RECOMMEND_MIN;RECOMMEND_MIN_REASON"
        # multiValueKeywords:
        "DEFINITIONS;PRINT;DEPS"
        # argsToParse:
        ${ARGN})
    string (TOLOWER ${pkgname} pkgname_lower)
    string (TOUPPER ${pkgname} pkgname_upper)
    if (NOT VERBOSE)
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
    if (_enable OR _pkg_REQUIRED)
        find_package (${pkgname} ${_pkg_UNPARSED_ARGUMENTS})
        if (${pkgname}_FOUND OR ${pkgname_upper}_FOUND)
            foreach (_vervar ${pkgname_upper}_VERSION ${pkgname}_VERSION_STRING
                             ${pkgname_upper}_VERSION_STRING)
                if (NOT ${pkgname}_VERSION AND ${_vervar})
                    set (${pkgname}_VERSION ${${_vervar}})
                endif ()
            endforeach ()
            message (STATUS "${ColorGreen}Found ${pkgname} ${${pkgname}_VERSION} ${ColorReset}")
            if (VERBOSE)
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
            add_definitions (${_pkg_DEFINITIONS})
            if (_pkg_RECOMMEND_MIN)
                if (${${pkgname}_VERSION} VERSION_LESS ${_pkg_RECOMMEND_MIN})
                    message (STATUS "${ColorYellow}Recommend ${pkgname} >= ${_pkg_RECOMMEND_MIN} ${_pkg_RECOMMEND_MIN_REASON} ${ColorReset}")
                endif ()
            endif ()
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
        endif()
    else ()
        if (NOT _quietskip)
            message (STATUS "${ColorRed}Not using ${pkgname} -- disabled ${_disablereason} ${ColorReset}")
        endif ()
    endif ()
endmacro()

