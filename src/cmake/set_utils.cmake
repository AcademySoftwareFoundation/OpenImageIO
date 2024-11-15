# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# Set a variable to a value if it is not already defined.
macro (set_if_not var value)
    if (NOT DEFINED ${var})
        set (${var} ${value})
    endif ()
endmacro ()


# Set a variable to a `replacement`, replacing its previous value, but only if
# `replacement` is non-empty.
macro (set_replace_if_nonempty var replacement)
    if (NOT "${replacement}" STREQUAL "")
        set (${var} ${replacement})
    endif ()
endmacro ()



# Set a cmake variable `var` from an environment variable, if it is not
# already defined (or if the FORCE flag is used). By default, the env var is
# the same name as `var`, but you can specify a different env var name with
# the ENVVAR keyword. If the env var doesn't exist or is empty, and a DEFAULT
# is supplied, assign the default to the variable instead.  If the VERBOSE
# CMake variable is set, or if the VERBOSE flag to this function is used,
# print a message.
macro (set_from_env var)
    cmake_parse_arguments(_sfe   # prefix
        # noValueKeywords:
        "VERBOSE;FORCE"
        # singleValueKeywords:
        "ENVVAR;DEFAULT;NAME"
        # multiValueKeywords:
        ""
        # argsToParse:
        ${ARGN})
    if (NOT DEFINED ${var} OR _sfe_FORCE)
        set_if_not (_sfe_ENVVAR ${var})
        set_if_not (_sfe_NAME ${var})
        if (DEFINED ENV{${_sfe_ENVVAR}} AND NOT "$ENV{${_sfe_ENVVAR}}" STREQUAL "")
            set (${var} $ENV{${_sfe_ENVVAR}})
            if (_sfe_VERBOSE OR VERBOSE)
                message (VERBOSE "set ${_sfe_NAME} = $ENV{${_sfe_ENVVAR}} (from env)")
            endif ()
        elseif (DEFINED _sfe_DEFAULT)
            set (${var} ${_sfe_DEFAULT})
        endif ()
    endif ()
endmacro ()



# Wrapper for CMake `set()` functionality with extensions:
# - If an env variable of the same name exists, it overrides the default
#   value.
# - In verbose mode or if the optional VERBOSE argument is passed, print the
#   value and whether it came from the env.
# - CACHE optional token makes it a cache variable.
# - ADVANCED optional token sets it as "mark_as_advanced" without the need
#   for a separate call (only applies to cache variables.)
# - FILEPATH, PATH, BOOL, STRING optional token works as usual (only applies
#   to cache variables).
# - `DOC <docstring>` specifies a doc string for cache variables. If omitted,
#   an empty doc string will be used.
# Other extensions may be added in the future.
macro (super_set name value)
    cmake_parse_arguments(_sce   # prefix
        # noValueKeywords:
        "FORCE;ADVANCED;FILEPATH;PATH;BOOL;STRING;CACHE;VERBOSE"
        # singleValueKeywords:
        "DOC"
        # multiValueKeywords:
        ""
        # argsToParse:
        ${ARGN})
    set (_sce_extra_args "")
    if (_sce_FILEPATH)
        set (_sce_type "FILEPATH")
    elseif (_sce_PATH)
        set (_sce_type "PATH")
    elseif (_sce_BOOL)
        set (_sce_type "BOOL")
    else ()
        set (_sce_type "STRING")
    endif ()
    if (_sce_FORCE)
        list (APPEND _sce_extra_args FORCE)
    endif ()
    set_if_not (_sce_DOC "empty")
    set_from_env (_sce_val ENVVAR ${name} NAME ${name} DEFAULT ${value})
    if (_sce_CACHE)
        set (${name} ${_sce_val} CACHE ${_sce_type} "${_sce_DOC}" ${_sce_extra_args})
    else ()
        set (${name} ${_sce_val} ${_sce_extra_args})
    endif ()
    if (_sce_VERBOSE)
        message (STATUS "${name} = ${${name}} (${_sce_DOC})")
    else ()
        message (VERBOSE "${name} = ${${name}} (${_sce_DOC})")
    endif ()
    if (_sce_ADVANCED)
        mark_as_advanced (${name})
    endif ()
    unset (_sce_extra_args)
    unset (_sce_type)
    unset (_sce_val)
endmacro ()


# `set(... CACHE ...)` workalike using super_set underneath.
macro (set_cache name value docstring)
    super_set (${name} "${value}" DOC ${docstring} CACHE ${ARGN})
endmacro ()


# `option()` workalike using super_set underneath.
macro (set_option name docstring value)
    set_cache (${name} "${value}" ${docstring} BOOL ${ARGN})
endmacro ()
