# Macro to install targets to the appropriate locations.  Use this instead of
# the install(TARGETS ...) signature.
#
# Usage:
#
#    oiio_install_targets (target1 [target2 ...])
#
macro (oiio_install_targets)
    install (TARGETS ${ARGN}
             RUNTIME DESTINATION "${BIN_INSTALL_DIR}" COMPONENT user
             LIBRARY DESTINATION "${LIB_INSTALL_DIR}" COMPONENT user
             ARCHIVE DESTINATION "${LIB_INSTALL_DIR}" COMPONENT developer)
endmacro ()

# Macro to add a build target for an IO plugin.
#
# Usage:
#
# add_oiio_plugin ( source1 [source2 ...]
#                   [LINK_LIBRARIES external_lib1 ...] )
#
# The plugin name is deduced from the name of the current directory and the
# source is automatically linked against OpenImageIO.  Additional libraries
# (for example, libpng) may be specified after the optionl LINK_LIBRARIES
# keyword.
#
macro (add_oiio_plugin)
    parse_arguments (_plugin "LINK_LIBRARIES" "" ${ARGN})
    set (_target_name ${CMAKE_CURRENT_SOURCE_DIR})
    # Get the name of the current directory and use it as the target name.
    get_filename_component (_target_name ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    add_library (${_target_name} SHARED ${_plugin_DEFAULT_ARGS})
    target_link_libraries (${_target_name} OpenImageIO ${_plugin_LINK_LIBRARIES})
    set_target_properties (${_target_name} PROPERTIES PREFIX "" FOLDER "Plugins")
    oiio_install_targets (${_target_name})
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
    parse_arguments (_ats "URL;IMAGEDIR;LABEL;FOUNDVAR" "" ${ARGN})
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
        message (STATUS "  -> Will not run tests ${_ats_DEFAULT_ARGS}")
        message (STATUS "  -> You can find it at ${_ats_URL}\n")
    else ()
        # Add the tests if all is well.
        if (DEFINED CMAKE_VERSION AND NOT CMAKE_VERSION VERSION_LESS 2.8)
            set (_has_generator_expr TRUE)
        endif ()
        foreach (_testname ${_ats_DEFAULT_ARGS})
            set (_testdir "${CMAKE_BINARY_DIR}/testsuite/${_testname}")
            if (_ats_LABEL MATCHES "broken")
                set (_testname "${_testname}-broken")
            endif ()
            if (_has_generator_expr)
                set (_add_test_args NAME ${_testname} 
#                                    WORKING_DIRECTORY ${_testdir}
                                    COMMAND python)
                if (MSVC_IDE)
                    set (_extra_test_args
                        --devenv-config $<CONFIGURATION>
                        --solution-path "${PROJECT_BINARY_DIR}" )
                else ()
                    set (_extra_test_args "")
                endif ()
            else ()
                set (_add_test_args ${_testname} python)
                set (_extra_test_args "")
            endif ()
            if (VERBOSE)
                message (STATUS "TEST ${_testname}: ${CMAKE_BINARY_DIR}/testsuite/runtest.py ${_testdir} ${_extra_test_args}")
            endif ()
            add_test (${_add_test_args}
                      "${CMAKE_BINARY_DIR}/testsuite/runtest.py"
                      ${_testdir}
                      ${_extra_test_args})
        endforeach ()
    endif ()
endmacro ()

