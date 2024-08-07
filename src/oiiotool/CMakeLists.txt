# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

fancy_add_executable (SYSTEM_INCLUDE_DIRS
                        ${ROBINMAP_INCLUDES}
                        ${OpenCV_INCLUDES}
                      LINK_LIBRARIES
                        OpenImageIO
                        $<TARGET_NAME_IF_EXISTS:OpenEXR::OpenEXR>
                        ${OpenCV_LIBRARIES}
                     )
