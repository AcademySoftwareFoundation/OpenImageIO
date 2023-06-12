# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio

if (Ptex_FOUND)
    add_oiio_plugin (ptexinput.cpp
                     LINK_LIBRARIES Ptex::Ptex_dynamic ZLIB::ZLIB
                     DEFINITIONS "-DUSE_PTEX")
endif ()
