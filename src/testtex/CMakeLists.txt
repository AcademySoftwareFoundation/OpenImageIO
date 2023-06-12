# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio

# testtex ordinarily gets built for testing but is not "installed". Setting
# -DENABLE_INSTALL_testtex=ON will install this test program along with the
# rest of the command line tools.  (Note: must be in PARENT_SCOPE or it would
# be local to this directory and not override properly by command line `-D`.)
set (ENABLE_INSTALL_testtex OFF PARENT_SCOPE)

fancy_add_executable (NAME testtex
                      INCLUDE_DIRS ${ROBINMAP_INCLUDES}
                      LINK_LIBRARIES OpenImageIO )
