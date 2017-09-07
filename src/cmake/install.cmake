###########################################################################
# Set default install options

set (PLUGIN_SEARCH_PATH "" CACHE STRING "Default plugin search path")

set (INSTALL_DOCS ON CACHE BOOL "Install documentation")
set (INSTALL_FONTS ON CACHE BOOL "Install default fonts")


###########################################################################
# Rpath handling at the install step
set (MACOSX_RPATH ON)
if (CMAKE_SKIP_RPATH)
    # We need to disallow the user from truly setting CMAKE_SKIP_RPATH, since
    # we want to run the generated executables from the build tree in order to
    # generate the manual page documentation.  However, we make sure the
    # install rpath is unset so that the install tree is still free of rpaths
    # for linux packaging purposes.
    set (CMAKE_SKIP_RPATH FALSE)
    unset (CMAKE_INSTALL_RPATH)
else ()
    set (CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_FULL_LIBDIR}")
    # add the automatically determined parts of the RPATH that
    # point to directories outside the build tree to the install RPATH
    set (CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    if (VERBOSE)
        message (STATUS "CMAKE_INSTALL_RPATH = ${CMAKE_INSTALL_RPATH}")
    endif ()
endif ()
