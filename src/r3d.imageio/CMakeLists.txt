# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Require the R3DSDK

if (NOT MSVC)
    set_source_files_properties (r3dinput.cpp
                             PROPERTIES COMPILE_FLAGS "-Wno-deprecated-declarations"
                                        COMPILE_DEFINITIONS "R3DSDKRUNTIME=${R3DSDK_RUNTIME_LIBRARIES}"
                             )
endif()

if (R3DSDK_FOUND)
    add_oiio_plugin (r3dinput.cpp
                     INCLUDE_DIRS ${R3DSDK_INCLUDE_DIR}
                     LINK_LIBRARIES ${R3DSDK_LIBRARIES}
                     DEFINITIONS "USE_R3DSDK=1")
    # target_compile_definitions(r3dinput PUBLIC "-DUSE_R3DSDK=1" "-DR3DSDKRUNTIME=\"${R3DSDK_RUNTIME_LIBRARIES}\"")
endif()
