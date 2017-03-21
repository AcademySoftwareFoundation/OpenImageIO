###########################################################################
# Paths for install tree customization.  Note that relative paths are relative
# to CMAKE_INSTALL_PREFIX.
set (DEFAULT_BIN_INSTALL_DIR   "bin")
set (DEFAULT_LIB_INSTALL_DIR   "lib")
set (DEFAULT_INCLUDE_INSTALL_DIR "include/${PROJECT_NAME}")
if (UNIX AND NOT SELF_CONTAINED_INSTALL_TREE)
    # Try to be well-behaved and install into reasonable places according to
    # the "standard" unix directory heirarchy
    # TODO: Figure out how to get the correct python directory
    set (DEFAULT_PYLIB_INSTALL_DIR "lib/python/site-packages")
    set (DEFAULT_PYLIB3_INSTALL_DIR "lib/python3/site-packages")
    set (DEFAULT_DOC_INSTALL_DIR "share/doc/${PROJECT_NAME}")
    set (DEFAULT_MAN_INSTALL_DIR "share/man/man1")
    set (DEFAULT_FONTS_INSTALL_DIR "share/fonts/${PROJECT_NAME}")
else ()
    # Here is the "self-contained install tree" case: the expectation here
    # is that everything related to this project will go into its own
    # directory, not into some standard system heirarchy.
    set (DEFAULT_PYLIB_INSTALL_DIR "python")
    set (DEFAULT_PYLIB3_INSTALL_DIR "python3")
    set (DEFAULT_DOC_INSTALL_DIR "doc")
    set (DEFAULT_MAN_INSTALL_DIR "doc/man")
    set (DEFAULT_FONTS_INSTALL_DIR "fonts/${PROJECT_NAME}")
endif ()
if (EXEC_INSTALL_PREFIX)
    # Tack on an extra prefix to support multi-arch builds.
    set (DEFAULT_BIN_INSTALL_DIR   "${EXEC_INSTALL_PREFIX}/${DEFAULT_BIN_INSTALL_DIR}")
    set (DEFAULT_LIB_INSTALL_DIR   "${EXEC_INSTALL_PREFIX}/${DEFAULT_LIB_INSTALL_DIR}")
    set (DEFAULT_PYLIB_INSTALL_DIR "${EXEC_INSTALL_PREFIX}/${DEFAULT_PYLIB_INSTALL_DIR}")
    set (DEFAULT_PYLIB3_INSTALL_DIR "${EXEC_INSTALL_PREFIX}/${DEFAULT_PYLIB3_INSTALL_DIR}")
    set (DEFAULT_FONTS_INSTALL_DIR "${EXEC_INSTALL_PREFIX}/${DEFAULT_FONTS_INSTALL_DIR}")
endif ()
# Set up cmake cache variables corresponding to the defaults deduced above, so
# that the user can override them as desired:
set (BIN_INSTALL_DIR ${DEFAULT_BIN_INSTALL_DIR} CACHE STRING
     "Install location for binaries (relative to CMAKE_INSTALL_PREFIX or absolute)")
set (LIB_INSTALL_DIR ${DEFAULT_LIB_INSTALL_DIR} CACHE STRING
     "Install location for libraries (relative to CMAKE_INSTALL_PREFIX or absolute)")
set (PYLIB_INSTALL_DIR ${DEFAULT_PYLIB_INSTALL_DIR} CACHE STRING
     "Install location for python libraries (relative to CMAKE_INSTALL_PREFIX or absolute)")
set (PYLIB3_INSTALL_DIR ${DEFAULT_PYLIB3_INSTALL_DIR} CACHE STRING
     "Install location for python3 libraries (relative to CMAKE_INSTALL_PREFIX or absolute)")
set (INCLUDE_INSTALL_DIR ${DEFAULT_INCLUDE_INSTALL_DIR} CACHE STRING
     "Install location of header files (relative to CMAKE_INSTALL_PREFIX or absolute)")
set (DOC_INSTALL_DIR ${DEFAULT_DOC_INSTALL_DIR} CACHE STRING
     "Install location for documentation (relative to CMAKE_INSTALL_PREFIX or absolute)")
set (FONTS_INSTALL_DIR ${DEFAULT_FONTS_INSTALL_DIR} CACHE STRING
     "Install location for fonts (relative to CMAKE_INSTALL_PREFIX or absolute)")
if (UNIX)
    set (MAN_INSTALL_DIR ${DEFAULT_MAN_INSTALL_DIR} CACHE STRING
         "Install location for manual pages (relative to CMAKE_INSTALL_PREFIX or absolute)")
endif()
set (PLUGIN_SEARCH_PATH "" CACHE STRING "Default plugin search path")

set (INSTALL_DOCS ON CACHE BOOL "Install documentation")
set (INSTALL_FONTS ON CACHE BOOL "Install default fonts")


###########################################################################
# Rpath handling at the install step
if (CMAKE_SKIP_RPATH)
    # We need to disallow the user from truly setting CMAKE_SKIP_RPATH, since
    # we want to run the generated executables from the build tree in order to
    # generate the manual page documentation.  However, we make sure the
    # install rpath is unset so that the install tree is still free of rpaths
    # for linux packaging purposes.
    set (CMAKE_SKIP_RPATH FALSE)
    unset (CMAKE_INSTALL_RPATH)
else ()
    set (CMAKE_INSTALL_RPATH "${LIB_INSTALL_DIR}")
    if (NOT IS_ABSOLUTE ${CMAKE_INSTALL_RPATH})
        set (CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIB_INSTALL_DIR}")
    endif ()
    # add the automatically determined parts of the RPATH that
    # point to directories outside the build tree to the install RPATH
    set (CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    if (VERBOSE)
        message (STATUS "CMAKE_INSTALL_RPATH = ${CMAKE_INSTALL_RPATH}")
    endif ()
endif ()



# Macro to install targets to the appropriate locations.  Use this instead of
# the install(TARGETS ...) signature.
#
# Usage:
#
#    install_targets (target1 [target2 ...])
#
macro (install_targets)
    install (TARGETS ${ARGN}
             RUNTIME DESTINATION "${BIN_INSTALL_DIR}" COMPONENT user
             LIBRARY DESTINATION "${LIB_INSTALL_DIR}" COMPONENT user
             ARCHIVE DESTINATION "${LIB_INSTALL_DIR}" COMPONENT developer)
endmacro ()

