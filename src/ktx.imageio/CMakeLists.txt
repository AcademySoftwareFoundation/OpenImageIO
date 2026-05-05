# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

if (Ktx_FOUND)
    add_oiio_plugin ( ktxinput.cpp ktxoutput.cpp
      # TODO: add certain warning ignores
      # bc7enc_rdo dependency (for BC1, BC2, BC3, BC4, BC5 and BC7 decoding/encoding)
      bc7enc-rdo/rgbcx.cpp bc7enc-rdo/bc7decomp.cpp bc7enc-rdo/bc7enc.cpp
      LINK_LIBRARIES KTX::ktx)
else ()
  message (WARNING "KTX plugin will not be built, no libktx")
endif ()
