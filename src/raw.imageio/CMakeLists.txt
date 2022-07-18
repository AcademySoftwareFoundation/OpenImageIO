# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

if (LIBRAW_FOUND OR libraw_FOUND)
    add_oiio_plugin (rawinput.cpp
                     INCLUDE_DIRS ${LIBRAW_INCLUDE_DIR} ${libraw_INCLUDE_DIR}
                     LINK_LIBRARIES ${LIBRAW_LIBRARIES} ${libraw_LIBRARIES}
                     DEFINITIONS "-DUSE_LIBRAW=1" ${libraw_r_DEFINITIONS} $<$<AND:$<PLATFORM_ID:Windows>,$<BOOL:${LINKSTATIC}>>:LIBRAW_NODLL=1>)
else ()
    message (WARNING "Raw plugin will not be built")
endif ()
