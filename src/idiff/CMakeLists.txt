# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Don't build if installing as a Python wheel
if (SKBUILD)
    return ()
endif ()

fancy_add_executable (LINK_LIBRARIES OpenImageIO)