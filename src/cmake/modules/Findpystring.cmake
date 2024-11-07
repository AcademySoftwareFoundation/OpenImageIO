# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

include (FindPackageHandleStandardArgs)

# Find include directory
find_path(pystring_INCLUDE_DIR
    NAMES
        pystring.h
    HINTS
        ${pystring_ROOT}
    PATH_SUFFIXES
        include
        include/pystring
        pystring/include
)

# Find library
find_library(pystring_LIBRARY
    NAMES
        pystring libpystring
    HINTS
        ${pystring_ROOT}
    PATH_SUFFIXES
        pystring/lib
        lib64
        lib
)

find_package_handle_standard_args(pystring
        REQUIRED_VARS 
            pystring_INCLUDE_DIR 
            pystring_LIBRARY
        VERSION_VAR pystring_VERSION
    )
    set(pystring_FOUND ${pystring_FOUND})


if(pystring_FOUND AND NOT TARGET pystring::pystring)
    add_library(pystring::pystring UNKNOWN IMPORTED GLOBAL)
    set(_pystring_TARGET_CREATE TRUE)
endif()


if(_pystring_TARGET_CREATE)
    set_target_properties(pystring::pystring PROPERTIES
        IMPORTED_LOCATION ${pystring_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${pystring_INCLUDE_DIR}
    )

    mark_as_advanced(pystring_INCLUDE_DIR pystring_LIBRARY pystring_VERSION)
endif()

