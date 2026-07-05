# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

if (Ktx_FOUND)
    add_oiio_plugin (ktxinput.cpp ktxoutput.cpp
      LINK_LIBRARIES KTX::ktx
      DEFINITIONS "-DKtx_VERSION=\"${Ktx_VERSION}\"")
else ()
  message (WARNING "KTX plugin will not be built, no libktx")
  set (format_plugin_definitions ${format_plugin_definitions} DISABLE_KTX=1 PARENT_SCOPE)
endif ()
