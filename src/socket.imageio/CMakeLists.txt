# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

add_oiio_plugin (socketinput.cpp socketoutput.cpp socket_pvt.cpp
                 DEFINITIONS "-DUSE_BOOST_ASIO=1")
