# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Module to find OpenEXR and Imath.
#
# For OpenEXR & Imath 3.x, this will establish the following imported
# targets:
#
#    Imath::Imath
#    Imath::Half
#    OpenEXR::OpenEXR
#    OpenEXR::Iex
#    OpenEXR::IlmThread
#
# and sets the following CMake variables:
#
#   OPENEXR_FOUND          true, if found
#   OPENEXR_INCLUDES       directory where OpenEXR headers are found
#   OPENEXR_LIBRARIES      libraries for OpenEXR + IlmBase
#   OPENEXR_VERSION        OpenEXR version
#   IMATH_INCLUDES         directory where Imath headers are found
#
#

# First, try to find just the right config files
find_package(Imath CONFIG)
find_package(OpenEXR CONFIG)

if (TARGET OpenEXR::OpenEXR AND TARGET Imath::Imath)
    # OpenEXR 3.x if both of these targets are found
    set (FOUND_OPENEXR_WITH_CONFIG 1)
    if (NOT OpenEXR_FIND_QUIETLY)
        message (STATUS "Found CONFIG for OpenEXR 3 (OpenEXR_VERSION=${OpenEXR_VERSION})")
    endif ()

    # Mimic old style variables
    set (OPENEXR_VERSION ${OpenEXR_VERSION})
    get_target_property(IMATH_INCLUDES Imath::Imath INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(IMATH_LIBRARY Imath::Imath INTERFACE_LINK_LIBRARIES)
    get_target_property(OPENEXR_IEX_LIBRARY OpenEXR::Iex INTERFACE_LINK_LIBRARIES)
    get_target_property(OPENEXR_ILMTHREAD_LIBRARY OpenEXR::IlmThread INTERFACE_LINK_LIBRARIES)
    get_target_property(OPENEXR_INCLUDES OpenEXR::OpenEXR INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(OPENEXR_ILMIMF_LIBRARY OpenEXR::OpenEXR INTERFACE_LINK_LIBRARIES)
    set (OPENEXR_LIBRARIES ${OPENEXR_ILMIMF_LIBRARY} ${OPENEXR_IEX_LIBRARY} ${OPENEXR_ILMTHREAD_LIBRARY})
    set (OPENEXR_FOUND true)

    # Link with pthreads if required
    # find_package (Threads)
    # if (CMAKE_USE_PTHREADS_INIT)
    #     list (APPEND ILMBASE_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})
    # endif ()

endif ()
