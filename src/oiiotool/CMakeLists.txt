# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

fancy_add_executable (LINK_LIBRARIES
                        OpenImageIO
                        ${Boost_LIBRARIES} # because regex
                        $<$<TARGET_EXISTS:OpenEXR::OpenEXR>:OpenEXR::OpenEXR>
                        $<$<TARGET_EXISTS:OpenEXR::IlmImf>:OpenEXR::IlmImf>
                        ${OPENEXR_LIBRARIES}
                      )
