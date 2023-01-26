# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio

option (ENABLE_RLA_IOPROXY "(Temporarily) turn this off to switch back to the old pre-IOProxy RLA implementation to address a bug." ON)
if (ENABLE_RLA_IOPROXY)
    add_oiio_plugin (iffinput.cpp iffoutput.cpp)
else ()
    add_oiio_plugin (noproxy-iffinput.cpp noproxy-iffoutput.cpp noproxy-iff_pvt.cpp)
endif ()
