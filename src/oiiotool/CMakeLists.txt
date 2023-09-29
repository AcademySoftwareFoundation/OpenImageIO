# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause and Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

fancy_add_executable (LINK_LIBRARIES
                        OpenImageIO
                        $<TARGET_NAME_IF_EXISTS:OpenEXR::OpenEXR>
                        $<TARGET_NAME_IF_EXISTS:OpenEXR::IlmImf>
                        ${OPENEXR_LIBRARIES}
                     )
