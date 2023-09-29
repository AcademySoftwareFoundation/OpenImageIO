# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Module to find OpenEXR and Imath.
#
# I'm afraid this is a mess, due to needing to support a wide range of
# OpenEXR versions.
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
# For OpenEXR 2.4 & 2.5, it will establish the following imported targets:
#
#    IlmBase::Imath
#    IlmBase::Half
#    IlmBase::Iex
#    IlmBase::IlmThread
#    OpenEXR::IlmImf
#
# For all version, the following CMake variables:
#
#   OPENEXR_FOUND          true, if found
#   OPENEXR_INCLUDES       directory where OpenEXR headers are found
#   OPENEXR_LIBRARIES      libraries for OpenEXR + IlmBase
#   OPENEXR_VERSION        OpenEXR version
#   IMATH_INCLUDES         directory where Imath headers are found
#   ILMBASE_INCLUDES       directory where IlmBase headers are found
#   ILMBASE_LIBRARIES      libraries just IlmBase
#
#

# First, try to fine just the right config files
find_package(Imath CONFIG)
if (NOT TARGET Imath::Imath)
    # Couldn't find Imath::Imath, maybe it's older and has IlmBase?
    find_package(IlmBase CONFIG)
endif ()
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
    get_target_property(ILMBASE_INCLUDES Imath::Imath INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(ILMBASE_IMATH_LIBRARY Imath::Imath INTERFACE_LINK_LIBRARIES)
    get_target_property(IMATH_LIBRARY Imath::Imath INTERFACE_LINK_LIBRARIES)
    get_target_property(OPENEXR_IEX_LIBRARY OpenEXR::Iex INTERFACE_LINK_LIBRARIES)
    get_target_property(OPENEXR_ILMTHREAD_LIBRARY OpenEXR::IlmThread INTERFACE_LINK_LIBRARIES)
    set (ILMBASE_LIBRARIES ${ILMBASE_IMATH_LIBRARY})
    set (ILMBASE_FOUND true)

    get_target_property(OPENEXR_INCLUDES OpenEXR::OpenEXR INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(OPENEXR_ILMIMF_LIBRARY OpenEXR::OpenEXR INTERFACE_LINK_LIBRARIES)
    set (OPENEXR_LIBRARIES ${OPENEXR_ILMIMF_LIBRARY} ${OPENEXR_IEX_LIBRARY} ${OPENEXR_ILMTHREAD_LIBRARY} ${ILMBASE_LIBRARIES})
    set (OPENEXR_FOUND true)

    # Link with pthreads if required
    find_package (Threads)
    if (CMAKE_USE_PTHREADS_INIT)
        list (APPEND ILMBASE_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})
    endif ()

elseif (TARGET OpenEXR::IlmImf AND TARGET IlmBase::Imath AND
        (OPENEXR_VERSION VERSION_GREATER_EQUAL 2.4 OR OpenEXR_VERSION VERSION_GREATER_EQUAL 2.4))
    # OpenEXR 2.4 or 2.5 with exported config
    set (FOUND_OPENEXR_WITH_CONFIG 1)
    if (NOT OpenEXR_FIND_QUIETLY)
        message (STATUS "Found CONFIG for OpenEXR 2 (OpenEXR_VERSION=${OpenEXR_VERSION})")
    endif ()

    # Mimic old style variables
    get_target_property(ILMBASE_INCLUDES IlmBase::IlmBaseConfig INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(IMATH_INCLUDES IlmBase::IlmBaseConfig INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(ILMBASE_Imath_LIBRARY IlmBase::Imath INTERFACE_LINK_LIBRARIES)
    get_target_property(ILMBASE_IMATH_LIBRARY IlmBase::Imath INTERFACE_LINK_LIBRARIES)
    get_target_property(ILMBASE_HALF_LIBRARY IlmBase::Half INTERFACE_LINK_LIBRARIES)
    get_target_property(OPENEXR_IEX_LIBRARY IlmBase::Iex INTERFACE_LINK_LIBRARIES)
    get_target_property(OPENEXR_ILMTHREAD_LIBRARY IlmBase::IlmThread INTERFACE_LINK_LIBRARIES)
    set (ILMBASE_LIBRARIES ${ILMBASE_IMATH_LIBRARY} ${ILMBASE_HALF_LIBRARY} ${OPENEXR_IEX_LIBRARY} ${OPENEXR_ILMTHREAD_LIBRARY})
    set (ILMBASE_FOUND true)

    get_target_property(OPENEXR_INCLUDES OpenEXR::IlmImfConfig INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(OPENEXR_ILMIMF_LIBRARY OpenEXR::IlmImf INTERFACE_LINK_LIBRARIES)
    set (OPENEXR_LIBRARIES ${OPENEXR_ILMIMF_LIBRARY} ${ILMBASE_LIBRARIES})
    set (OPENEXR_FOUND true)

    # Link with pthreads if required
    find_package (Threads)
    if (CMAKE_USE_PTHREADS_INIT)
        list (APPEND ILMBASE_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})
    endif ()

    # Correct for how old OpenEXR config exports set the directory one
    # level lower than we prefer it.
    string(REGEX REPLACE "include/OpenEXR$" "include" ILMBASE_INCLUDES "${ILMBASE_INCLUDES}")
    string(REGEX REPLACE "include/OpenEXR$" "include" IMATH_INCLUDES "${IMATH_INCLUDES}")
    string(REGEX REPLACE "include/OpenEXR$" "include" OPENEXR_INCLUDES "${OPENEXR_INCLUDES}")

endif ()
