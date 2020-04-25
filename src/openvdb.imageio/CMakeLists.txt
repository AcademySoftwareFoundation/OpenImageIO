# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

if (OpenVDB_FOUND)
    add_oiio_plugin (openvdbinput.cpp
                     INCLUDE_DIRS ${TBB_INCLUDE_DIRS}
                     LINK_LIBRARIES OpenVDB::OpenVDB ${TBB_tbb_LIBRARY} ${BOOST_LIBRARIES})
endif()
