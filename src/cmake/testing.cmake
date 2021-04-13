# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/

include (CTest)

# Make a build/platform/testsuite directory, and copy the master runtest.py
# there. The rest is up to the tests themselves.
file (MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/testsuite")
file (COPY "${CMAKE_CURRENT_SOURCE_DIR}/testsuite/common"
      DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/testsuite")
add_custom_command (OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/testsuite/runtest.py"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${CMAKE_CURRENT_SOURCE_DIR}/testsuite/runtest.py"
                        "${CMAKE_CURRENT_BINARY_DIR}/testsuite/runtest.py"
                    MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/testsuite/runtest.py")
add_custom_target ( CopyFiles ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/testsuite/runtest.py" )

set(OIIO_TESTSUITE_IMAGEDIR "${PROJECT_SOURCE_DIR}/.." CACHE PATH
    "Location of oiio-images, openexr-images, libtiffpic, etc.." )



# oiio_set_testenv() - add environment variables to a test
#
# Usage:
#   oiio_set_testenv ( testname
#                      testsuite  - The root of all tests ${CMAKE_SOURCE_DIR}/testsuite
#                      testsrcdir - Current test directory in ${CMAKE_SOURCE_DIR}
#                      testdir    - Current test sandbox in ${CMAKE_BINARY_DIR}
#                      IMAGEDIR   - Optional path to image reference/compare directory)
#
macro (oiio_set_testenv testname testsuite testsrcdir testdir IMAGEDIR)
    set_property(TEST ${testname} PROPERTY ENVIRONMENT
                "OIIO_TESTSUITE_ROOT=${testsuite}"
                ";OIIO_TESTSUITE_SRC=${testsrcdir}"
                ";OIIO_TESTSUITE_CUR=${testdir}")
    if (NOT ${IMAGEDIR} STREQUAL "")
        set_property(TEST ${testname} APPEND PROPERTY ENVIRONMENT
                     "OIIO_TESTSUITE_IMAGEDIR=${IMAGEDIR}")
    endif()
endmacro ()



# oiio_add_tests() - add a set of test cases.
#
# Usage:
#   oiio_add_tests ( test1 [ test2 ... ]
#                    [ IMAGEDIR name_of_reference_image_directory ]
#                    [ URL http://find.reference.cases.here.com ]
#                    [ FOUNDVAR variable_name ... ]
#                    [ ENABLEVAR variable_name ... ]
#                  )
#
# The optional argument IMAGEDIR is used to check whether external test images
# (not supplied with OIIO) are present, and to disable the test cases if
# they're not.  If IMAGEDIR is present, URL should also be included to tell
# the user where to find such tests.
#
# The optional FOUNDVAR introduces variables (typically Foo_FOUND) that if
# not existing and true, will skip the test.
#
# The optional ENABLEVAR introduces variables (typically ENABLE_Foo) that
# if existing and yet false, will skip the test.
#
macro (oiio_add_tests)
    cmake_parse_arguments (_ats "" "" "URL;IMAGEDIR;LABEL;FOUNDVAR;ENABLEVAR;TESTNAME" ${ARGN})
       # Arguments: <prefix> <options> <one_value_keywords> <multi_value_keywords> args...
    set (_ats_testdir "${OIIO_TESTSUITE_IMAGEDIR}/${_ats_IMAGEDIR}")
    # If there was a FOUNDVAR param specified and that variable name is
    # not defined, mark the test as broken.
    foreach (_var ${_ats_FOUNDVAR})
        if (NOT ${_var})
            set (_ats_LABEL "broken")
        endif ()
    endforeach ()
    foreach (_var ${_ats_ENABLEVAR})
        if ((NOT "${${_var}}" STREQUAL "" AND NOT "${${_var}}") OR
            (NOT "$ENV{${_var}}" STREQUAL "" AND NOT "$ENV{${_var}}"))
            set (_ats_LABEL "broken")
        endif ()
    endforeach ()
    if (_ats_IMAGEDIR AND NOT EXISTS ${_ats_testdir})
        # If the directory containing reference data (images) for the test
        # isn't found, point the user at the URL.
        message (STATUS "\n\nDid not find ${_ats_testdir}")
        message (STATUS "  -> Will not run tests ${_ats_UNPARSED_ARGUMENTS}")
        message (STATUS "  -> You can find it at ${_ats_URL}\n")
    else ()
        # Add the tests if all is well.
        set (_has_generator_expr TRUE)
        foreach (_testname ${_ats_UNPARSED_ARGUMENTS})
            set (_testsuite "${CMAKE_SOURCE_DIR}/testsuite")
            set (_testsrcdir "${_testsuite}/${_testname}")
            set (_testdir "${CMAKE_BINARY_DIR}/testsuite/${_testname}")
            if (_ats_TESTNAME)
                set (_testname "${_ats_TESTNAME}")
            endif ()
            if (_ats_LABEL MATCHES "broken")
                set (_testname "${_testname}-broken")
            endif ()

            set (_runtest ${Python_EXECUTABLE} "${CMAKE_SOURCE_DIR}/testsuite/runtest.py" ${_testdir})
            if (MSVC_IDE)
                set (_runtest ${_runtest} --devenv-config $<CONFIGURATION>
                                          --solution-path "${CMAKE_BINARY_DIR}" )
            endif ()

            file (MAKE_DIRECTORY "${_testdir}")

            add_test ( NAME ${_testname}
                       COMMAND ${_runtest} )

            oiio_set_testenv("${_testname}" "${_testsuite}"
                             "${_testsrcdir}" "${_testdir}" "${_ats_testdir}")

            # For texture tests, add a second test using batch mode as well.
            if (_testname MATCHES "texture")
                set (_testname ${_testname}.batch)
                set (_testdir ${_testdir}.batch)
                set (_runtest ${Python_EXECUTABLE} "${CMAKE_SOURCE_DIR}/testsuite/runtest.py" ${_testdir})
                if (MSVC_IDE)
                    set (_runtest ${_runtest} --devenv-config $<CONFIGURATION>
                                          --solution-path "${CMAKE_BINARY_DIR}" )
                endif ()
                file (MAKE_DIRECTORY "${_testdir}")
                add_test ( NAME "${_testname}"
                           COMMAND env TESTTEX_BATCH=1 ${_runtest} )

                oiio_set_testenv("${_testname}" "${_testsuite}"
                                 "${_testsrcdir}" "${_testdir}.batch" "${_ats_testdir}")
            endif ()

            #if (VERBOSE)
            #    message (STATUS "TEST ${_testname}: ${_runtest}")
            #endif ()
        endforeach ()
        if (VERBOSE)
           message (STATUS "TESTS: ${_ats_UNPARSED_ARGUMENTS}")
        endif ()
    endif ()
endmacro ()



# The tests are organized into a macro so it can be called after all the
# directories with plugins are included.
#
macro (oiio_add_all_tests)
    #   Tests that require oiio-images:
    oiio_add_tests (gpsread
                    oiiotool oiiotool-attribs  oiiotool-copy
                    oiiotool-xform
                    maketx oiiotool-maketx
                    misnamed-file
                    texture-crop texture-cropover
                    texture-filtersize
                    texture-overscan
                    texture-wrapfill
                    texture-res texture-maxres
                    IMAGEDIR oiio-images URL "Recent checkout of oiio-images"
                   )

    #   Remaining freestanding tests:
    oiio_add_tests (
                    cmake-consumer
                    nonwhole-tiles
                    oiiotool-composite
                    oiiotool-fixnan
                    oiiotool-pattern
                    oiiotool-readerror
                    oiiotool-subimage oiiotool-text
                    diff
                    dither dup-channels
                    jpeg-corrupt
                    missingcolor
                    null
                    rational
                    texture-derivs texture-fill
                    texture-flipt texture-gettexels texture-gray
                    texture-interp-bicubic
                    texture-blurtube
                    texture-half texture-uint16
                    texture-interp-bilinear
                    texture-interp-closest
                    texture-mip-nomip texture-mip-onelevel
                    texture-mip-trilinear
                    texture-missing
                    texture-pointsample
                    texture-udim texture-udim2
                    texture-uint8
                    texture-width0blur
                    texture-fat texture-skinny
                   )

    # Add tests that require the Python bindings if we built the Python
    # bindings. This is mostly the test that are specifically about testing
    # the Python bindings themselves, but also a handful of tests that are
    # mainly about other things but happen to use Python in order to perform
    # thee test.
    # We also exclude these tests if this is a sanitizer build on Linux,
    # because the Python interpreter itself won't be linked with the right asan
    # libraries to run correctly.
    if (USE_PYTHON AND NOT BUILD_OIIOUTIL_ONLY AND NOT SANITIZE_ON_LINUX)
        oiio_add_tests (
                python-typedesc python-paramlist
                python-imagespec python-roi python-deep python-colorconfig
                python-imageinput python-imageoutput
                python-imagebuf python-imagebufalgo
                IMAGEDIR oiio-images
                )
    endif ()

    oiio_add_tests (oiiotool-color
                    FOUNDVAR OPENCOLORIO_FOUND)

    if (NOT DEFINED ENV{CI} AND NOT DEFINED ENV{GITHUB_ACTIONS})
        oiio_add_tests (texture-icwrite)
    endif ()

    # List testsuites for specific formats or features which might be not found
    # or be intentionally disabled, or which need special external reference
    # images from the web that if not found, should skip the tests:
    oiio_add_tests (bmp
                    ENABLEVAR ENABLE_BMP
                    IMAGEDIR bmpsuite
                    URL http://entropymine.com/jason/bmpsuite/bmpsuite.zip)
    oiio_add_tests (dpx
                    ENABLEVAR ENABLE_DPX
                    IMAGEDIR oiio-images URL "Recent checkout of oiio-images")
    oiio_add_tests (field3d texture-field3d
                    FOUNDVAR Field3D_FOUND ENABLEVAR ENABLE_FIELD3D)
    oiio_add_tests (fits
                    ENABLEVAR ENABLE_FITS
                    IMAGEDIR fits-images
                    URL http://www.cv.nrao.edu/fits/data/tests/)
    oiio_add_tests (gif
                    FOUNDVAR GIF_FOUND ENABLEVAR ENABLE_GIF
                    IMAGEDIR oiio-images URL "Recent checkout of oiio-images")
    oiio_add_tests (heif
                    FOUNDVAR Libheif_FOUND ENABLEVAR ENABLE_Libheif
                    URL https://github.com/nokiatech/heif/tree/gh-pages/content)
    oiio_add_tests (ico
                    ENABLEVAR ENABLE_ICO
                    IMAGEDIR oiio-images URL "Recent checkout of oiio-images")
    oiio_add_tests (iff
                    ENABLEVAR ENABLE_IFF
                    IMAGEDIR oiio-images URL "Recent checkout of oiio-images")
    oiio_add_tests (jpeg2000
                    FOUNDVAR OPENJPEG_FOUND
                    IMAGEDIR j2kp4files_v1_5
                    URL http://www.itu.int/net/ITU-T/sigdb/speimage/ImageForm-s.aspx?val=10100803)
    oiio_add_tests (openexr-suite openexr-multires openexr-chroma
                    openexr-v2 openexr-window perchannel
                    oiiotool-deep
                    IMAGEDIR openexr-images
                    URL http://www.openexr.com/downloads.html)
    if (NOT DEFINED ENV{CI} AND NOT DEFINED ENV{GITHUB_ACTIONS})
        oiio_add_tests (openexr-damaged
                        IMAGEDIR openexr-images
                        URL http://www.openexr.com/downloads.html)
    endif ()
    oiio_add_tests (openvdb
                    FOUNDVAR OpenVDB_FOUND ENABLEVAR ENABLE_OpenVDB)
    oiio_add_tests (png
                    ENABLEVAR ENABLE_PNG
                    IMAGEDIR oiio-images URL "Recent checkout of oiio-images")
    oiio_add_tests (pnm
                    ENABLEVAR ENABLE_PNM
                    IMAGEDIR oiio-images URL "Recent checkout of oiio-images")
    oiio_add_tests (psd psd-colormodes
                    ENABLEVAR ENABLE_PSD
                    IMAGEDIR oiio-images URL "Recent checkout of oiio-images")
    oiio_add_tests (ptex
                    FOUNDVAR PTEX_FOUND ENABLEVAR ENABLE_PTEX)
    oiio_add_tests (raw
                    FOUNDVAR LIBRAW_FOUND ENABLEVAR ENABLE_LIBRAW
                    IMAGEDIR oiio-images/raw
                    URL "Recent checkout of oiio-images")
    oiio_add_tests (rla
                    ENABLEVAR ENABLE_RLA
                    IMAGEDIR oiio-images URL "Recent checkout of oiio-images")
    oiio_add_tests (sgi
                    ENABLEVAR ENABLE_SGI
                    IMAGEDIR oiio-images URL "Recent checkout of oiio-images")
    oiio_add_tests (targa-tgautils
                    ENABLEVAR ENABLE_TARGA
                    IMAGEDIR oiio-images URL "Recent checkout of oiio-images")
    oiio_add_tests (tiff-suite tiff-depths tiff-misc
                    IMAGEDIR libtiffpic
                    URL http://www.simplesystems.org/libtiff/images.html)
    oiio_add_tests (webp
                    FOUNDVAR WebP_FOUND ENABLEVAR ENABLE_WebP
                    IMAGEDIR oiio-images/webp URL "Recent checkout of oiio-images")
    oiio_add_tests (zfile ENABLEVAR ENABLE_ZFILE
                    IMAGEDIR oiio-images URL "Recent checkout of oiio-images")

    if (SPI_TESTS)
        oiio_add_tests (oiiotool-spi
                        FOUNDVAR SPI_TESTS
                        IMAGEDIR spi-oiio-tests
                        URL "noplace -- it's SPI specific tests")
    endif ()

endmacro()
