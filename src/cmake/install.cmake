###########################################################################
# Set install paths for the python modules
# TODO: Figure out how to get the correct python directory

if (UNIX AND NOT SELF_CONTAINED_INSTALL_TREE)
    # TODO: Figure out how to get the correct python directory
    set (DEFAULT_PYLIB_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/python/site-packages")
    set (DEFAULT_PYLIB3_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/python3/site-packages")
else ()
    # Here is the "self-contained install tree" case: the expectation here
    # is that everything related to this project will go into its own
    # directory, not into some standard system heirarchy.
    set (DEFAULT_PYLIB_INSTALL_DIR "python")
    set (DEFAULT_PYLIB3_INSTALL_DIR "python3")
endif ()
if (EXEC_INSTALL_PREFIX)
    # Tack on an extra prefix to support multi-arch builds.
    set (DEFAULT_PYLIB_INSTALL_DIR "${EXEC_INSTALL_PREFIX}/${DEFAULT_PYLIB_INSTALL_DIR}")
    set (DEFAULT_PYLIB3_INSTALL_DIR "${EXEC_INSTALL_PREFIX}/${DEFAULT_PYLIB3_INSTALL_DIR}")
endif ()
# Set up cmake cache variables corresponding to the defaults deduced above, so
# that the user can override them as desired:
set (PYLIB_INSTALL_DIR ${DEFAULT_PYLIB_INSTALL_DIR} CACHE STRING
     "Install location for python libraries (relative to CMAKE_INSTALL_PREFIX or absolute)")
set (PYLIB3_INSTALL_DIR ${DEFAULT_PYLIB3_INSTALL_DIR} CACHE STRING
     "Install location for python3 libraries (relative to CMAKE_INSTALL_PREFIX or absolute)")

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
