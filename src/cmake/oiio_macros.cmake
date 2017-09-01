# Macro to install targets to the appropriate locations.  Use this instead of
# the install(TARGETS ...) signature.
#
# Usage:
#
#    oiio_install_targets (target1 [target2 ...])
#
macro (oiio_install_targets)
    install (TARGETS ${ARGN}
             RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT user
             LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT user
             ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT developer)
endmacro ()

# Macro to add a build target for an IO plugin.
#
# Usage:
#
# add_oiio_plugin ( source1 [source2 ...]
#                   [ INCLUDE_DIRS include_dir1 ... ]
#                   [ LINK_LIBRARIES external_lib1 ... ]
#                   [ DEFINITIONS -DFOO=bar ... ])
#
# The plugin name is deduced from the name of the current directory and the
# source is automatically linked against OpenImageIO. Additional include
# directories (just for this target) may be specified after the optional
# INCLUDE_DIRS keyword. Additional libraries (for example, libpng) may be
# specified after the optionl LINK_LIBRARIES keyword. Additional
# preprocessor definitions may be specified after the optional DEFINITIONS
# keyword.
#
# What goes on under the covers is quite different depending on whether
# EMBEDPLUGINS is 0 or 1. If EMBEDPLUGINS is 0 (in which case this is
# expected to be called *after* the OpenImageIO target is declared), it will
# create a new target to build the full plugin. On the other hand, if
# EMBEDPLUGINS is 1 (in which case this should be called *before* the
# OpenImageIO target is declared), it will merely append the required
# definitions, includs, and libraries to lists format_plugin_blah that will
# be handed off too the setup of the later OpenImageIO target.
#
macro (add_oiio_plugin)
    cmake_parse_arguments (_plugin "" "" "INCLUDE_DIRS;LINK_LIBRARIES;DEFINITIONS" ${ARGN})
       # Arguments: <prefix> <options> <one_value_keywords> <multi_value_keywords> args...
    if (EMBEDPLUGINS)
        set (_target_name OpenImageIO)
        # Add each source file to the libOpenImageIO_srcs, but it takes some
        # bending over backwards to change it in the parent scope.
        set (_plugin_all_source ${libOpenImageIO_srcs})
        foreach (_plugin_source_file ${_plugin_UNPARSED_ARGUMENTS})
            list (APPEND _plugin_all_source "${CMAKE_CURRENT_SOURCE_DIR}/${_plugin_source_file}")
        endforeach ()
        set (libOpenImageIO_srcs "${_plugin_all_source}" PARENT_SCOPE)
        set (format_plugin_definitions ${format_plugin_definitions} ${_plugin_DEFINITIONS} PARENT_SCOPE)
        set (format_plugin_include_dirs ${format_plugin_include_dirs} ${_plugin_INCLUDE_DIRS} PARENT_SCOPE)
        set (format_plugin_libs ${format_plugin_libs} ${_plugin_LINK_LIBRARIES} PARENT_SCOPE)
    else ()
        # Get the name of the current directory and use it as the target name.
        set (_target_name ${CMAKE_CURRENT_SOURCE_DIR})
        get_filename_component (_target_name ${CMAKE_CURRENT_SOURCE_DIR} NAME)
        add_library (${_target_name} SHARED ${_plugin_UNPARSED_ARGUMENTS})
        add_definitions (${_plugin_DEFINITIONS})
        include_directories (${_target_name} PRIVATE ${_plugin_INCLUDE_DIRS})
        target_link_libraries (${_target_name} OpenImageIO ${_plugin_LINK_LIBRARIES})
        set_target_properties (${_target_name} PROPERTIES PREFIX "" FOLDER "Plugins")
        oiio_install_targets (${_target_name})
    endif ()
endmacro ()


# oiio_add_tests() - add a set of test cases.
#
# Usage:
#   oiio_add_tests ( test1 [ test2 ... ]
#                    [ IMAGEDIR name_of_reference_image_directory ]
#                    [ URL http://find.reference.cases.here.com ] )
#
# The optional argument IMAGEDIR is used to check whether external test images
# (not supplied with OIIO) are present, and to disable the test cases if
# they're not.  If IMAGEDIR is present, URL should also be included to tell
# the user where to find such tests.
#
macro (oiio_add_tests)
    cmake_parse_arguments (_ats "" "" "URL;IMAGEDIR;LABEL;FOUNDVAR;TESTNAME" ${ARGN})
       # Arguments: <prefix> <options> <one_value_keywords> <multi_value_keywords> args...
    set (_ats_testdir "${PROJECT_SOURCE_DIR}/../${_ats_IMAGEDIR}")
    # If there was a FOUNDVAR param specified and that variable name is
    # not defined, mark the test as broken.
    if (_ats_FOUNDVAR AND NOT ${_ats_FOUNDVAR})
        set (_ats_LABEL "broken")
    endif ()
    if (_ats_IMAGEDIR AND NOT EXISTS ${_ats_testdir})
        # If the directory containig reference data (images) for the test
        # isn't found, point the user at the URL.
        message (STATUS "\n\nDid not find ${_ats_testdir}")
        message (STATUS "  -> Will not run tests ${_ats_UNPARSED_ARGUMENTS}")
        message (STATUS "  -> You can find it at ${_ats_URL}\n")
    else ()
        # Add the tests if all is well.
        set (_has_generator_expr TRUE)
        foreach (_testname ${_ats_UNPARSED_ARGUMENTS})
            set (_testsrcdir "${CMAKE_SOURCE_DIR}/testsuite/${_testname}")
            set (_testdir "${CMAKE_BINARY_DIR}/testsuite/${_testname}")
            if (_ats_TESTNAME)
                set (_testname "${_ats_TESTNAME}")
            endif ()
            if (_ats_LABEL MATCHES "broken")
                set (_testname "${_testname}-broken")
            endif ()

            set (_runtest python "${CMAKE_SOURCE_DIR}/testsuite/runtest.py" ${_testdir})
            if (MSVC_IDE)
                set (_runtest ${_runtest} --devenv-config $<CONFIGURATION>
                                          --solution-path "${CMAKE_BINARY_DIR}" )
            endif ()

            file (MAKE_DIRECTORY "${_testdir}")

            add_test ( NAME ${_testname}
                       COMMAND ${_runtest} )

            # For texture tests, add a second test using batch mode as well.
            if (_testname MATCHES "texture")
                add_test ( NAME "${_testname}.batch"
                           COMMAND env TESTTEX_BATCH=1 ${_runtest} )
            endif ()

            if (VERBOSE)
                message (STATUS "TEST ${_testname}: ${_runtest}")
            endif ()
        endforeach ()
    endif ()
endmacro ()

