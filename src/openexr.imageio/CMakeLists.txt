# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

option (OIIO_USE_EXR_C_API "Use the new 3.1 C API for exr I/O" OFF)
if (OIIO_USE_EXR_C_API)
  if (NOT TARGET OpenEXR::OpenEXRCore)
    message(FATAL_ERROR "OpenEXR find did not find the new C library")
  endif()
  add_oiio_plugin (exrinput_c.cpp exroutput.cpp
    INCLUDE_DIRS ${OPENEXR_INCLUDES} ${IMATH_INCLUDE_DIR}/OpenEXR
    LINK_LIBRARIES OpenEXR::OpenEXRCore)
else()
  add_oiio_plugin (exrinput.cpp exroutput.cpp
    INCLUDE_DIRS ${OPENEXR_INCLUDES} ${IMATH_INCLUDE_DIR}/OpenEXR
    LINK_LIBRARIES ${OPENEXR_LIBRARIES})
endif()

