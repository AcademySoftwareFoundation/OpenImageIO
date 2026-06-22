# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

if (Ktx_FOUND)
    add_oiio_plugin (ktxinput.cpp ktxoutput.cpp LINK_LIBRARIES KTX::ktx)
else ()
  message (WARNING "KTX plugin will not be built, no libktx")
endif ()
