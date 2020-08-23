# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

# Is the named package "enabled" via our disabling convention? If either
# USE_pkgname (or the all-uppercase USE_PKGNAME, or ENABLE_pkgname, or
# ENABLE_PKGNAME) exists as either a CMake or environment variable, is
# nonempty by contains a non-true/nonzero value, store false in the
# variable named by <enablevar>, otherwise store true.
function (check_is_enabled pkgname enablevar)
    string (TOUPPER ${pkgname} pkgname_upper)
    set (${enablevar} true PARENT_SCOPE)
    if (
        (NOT "${USE_${pkgname}}" STREQUAL "" AND NOT "${USE_${pkgname}}") OR
        (NOT "${USE_${pkgname_upper}}" STREQUAL "" AND NOT "${USE_${pkgname_upper}}") OR
        (NOT "$ENV{USE_${pkgname}}" STREQUAL "" AND NOT "$ENV{USE_${pkgname}}") OR
        (NOT "$ENV{USE_${pkgname_upper}}" STREQUAL "" AND NOT "$ENV{USE_${pkgname_upper}}") OR
        (NOT "${ENABLE_${pkgname}}" STREQUAL "" AND NOT "${ENABLE_${pkgname}}") OR
        (NOT "${ENABLE_${pkgname_upper}}" STREQUAL "" AND NOT "${ENABLE_${pkgname_upper}}") OR
        (NOT "$ENV{ENABLE_${pkgname}}" STREQUAL "" AND NOT "$ENV{ENABLE_${pkgname}}") OR
        (NOT "$ENV{ENABLE_${pkgname_upper}}" STREQUAL "" AND NOT "$ENV{ENABLE_${pkgname_upper}}") OR
        (DISABLE_${pkgname} OR DISABLE_${pkgname_upper} OR
         "$ENV{DISABLE_${pkgname}}" OR "$ENV{DISABLE_${pkgname_upper}}") OR
        (NOT "${_pkg_ENABLE}" STREQUAL "" AND NOT "${_pkg_ENABLE}")
        )
        set (${enablevar} false PARENT_SCOPE)
    endif ()
endfunction ()
