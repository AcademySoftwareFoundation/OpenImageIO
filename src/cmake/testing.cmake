# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO/

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

# set(OIIO_TESTSUITE_IMAGEDIR "${PROJECT_SOURCE_DIR}/.." CACHE PATH
set(OIIO_TESTSUITE_IMAGEDIR "${PROJECT_BINARY_DIR}/testsuite" CACHE PATH
    "Location of oiio-images, openexr-images, libtiffpic, etc.." )



# oiio_add_tests() - add a set of test cases.
#
# Usage:
#   oiio_add_tests ( test1 [ test2 ... ]
#                    [ IMAGEDIR name_of_reference_image_directory ]
#                    [ URL http://find.reference.cases.here.com ]
#                    [ FOUNDVAR variable_name ... ]
#                    [ ENABLEVAR variable_name ... ]
#                    [ SUFFIX suffix ]
#                    [ ENVIRONMENT "VAR=value" ... ]
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
# The optional SUFFIX is appended to the test name.
#
# The optinonal ENVIRONMENT is a list of environment variables to set for the
# test.
#
macro (oiio_add_tests)
    cmake_parse_arguments (_ats "" "SUFFIX;TESTNAME" "URL;IMAGEDIR;LABEL;FOUNDVAR;ENABLEVAR;ENVIRONMENT" ${ARGN})
       # Arguments: <prefix> <options> <one_value_keywords> <multi_value_keywords> args...
    set (_ats_testdir "${OIIO_TESTSUITE_IMAGEDIR}/${_ats_IMAGEDIR}")
    # If there was a FOUNDVAR param specified and that variable name is
    # not defined, mark the test as broken.
    foreach (_var ${_ats_FOUNDVAR})
        if (NOT ${_var})
            set (_ats_LABEL "broken")
        endif ()
    endforeach ()
    set (_test_disabled 0)
    foreach (_var ${_ats_ENABLEVAR})
        if ((NOT "${${_var}}" STREQUAL "" AND NOT "${${_var}}") OR
            (NOT "$ENV{${_var}}" STREQUAL "" AND NOT "$ENV{${_var}}"))
            set (_ats_LABEL "broken")
            set (_test_disabled 1)
        endif ()
    endforeach ()
    if (OpenColorIO_VERSION VERSION_GREATER_EQUAL 2.2
          AND NOT (OIIO_DISABLE_BUILTIN_OCIO_CONFIGS OR "$ENV{OIIO_DISABLE_BUILTIN_OCIO_CONFIGS}"))
        # For OCIO 2.2+, have the testsuite use the default built-in config
        list (APPEND _ats_ENVIRONMENT "OCIO=ocio://default"
                                      "OIIO_TESTSUITE_OCIOCONFIG=ocio://default")
    else ()
        # For OCIO 2.1 and earlier, have the testsuite use one we have in
        # the testsuite directory.
        list (APPEND _ats_ENVIRONMENT "OCIO=../common/OpenColorIO/nuke-default/config.ocio"
                                      "OIIO_TESTSUITE_OCIOCONFIG=../common/OpenColorIO/nuke-default/config.ocio")
    endif ()
    if (_test_disabled)
        message (STATUS "Skipping test(s) ${_ats_UNPARSED_ARGUMENTS} because of disabled ${_ats_ENABLEVAR}")
    elseif (_ats_IMAGEDIR AND NOT EXISTS ${_ats_testdir})
        # If the directory containing reference data (images) for the test
        # isn't found, point the user at the URL.
        message (STATUS "\n\nDid not find ${_ats_testdir}")
        message (STATUS "  -> Will not run tests ${_ats_UNPARSED_ARGUMENTS}")
        message (STATUS "  -> You can find it at ${_ats_URL}\n")
    else ()
        # Add the tests if all is well.
        set (_has_generator_expr TRUE)
        set (_testsuite "${CMAKE_SOURCE_DIR}/testsuite")
        foreach (_testname ${_ats_UNPARSED_ARGUMENTS})
            set (_testsrcdir "${_testsuite}/${_testname}")
            set (_testdir "${CMAKE_BINARY_DIR}/testsuite/${_testname}${_ats_SUFFIX}")
            if (_ats_TESTNAME)
                set (_testname "${_ats_TESTNAME}")
            endif ()
            if (_ats_SUFFIX)
                set (_testname "${_testname}${_ats_SUFFIX}")
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

            add_test ( NAME ${_testname} COMMAND ${_runtest} )
            set_property(TEST ${_testname} APPEND PROPERTY ENVIRONMENT
                             "OIIO_TESTSUITE_ROOT=${_testsuite}"
                             "OIIO_TESTSUITE_SRC=${_testsrcdir}"
                             "OIIO_TESTSUITE_CUR=${_testdir}"
                             ${_ats_ENVIRONMENT})
            if (NOT ${_ats_testdir} STREQUAL "")
                set_property(TEST ${_testname} APPEND PROPERTY ENVIRONMENT
                             "OIIO_TESTSUITE_IMAGEDIR=${_ats_testdir}")
            endif()

        endforeach ()
        message (VERBOSE "TESTS: ${_ats_UNPARSED_ARGUMENTS}")
    endif ()
endmacro ()



# The tests are organized into a macro so it can be called after all the
# directories with plugins are included.
#
macro (oiio_add_all_tests)

    # Freestanding tests:
    oiio_add_tests (
                    cmake-consumer
                    docs-examples-cpp
                    iinfo igrep
                    nonwhole-tiles
                    oiiotool
                    oiiotool-composite oiiotool-control oiiotool-copy
                    oiiotool-fixnan
                    oiiotool-pattern
                    oiiotool-readerror
                    oiiotool-subimage oiiotool-text oiiotool-xform
                    diff
                    dither dup-channels
                    jpeg-corrupt
                    maketx oiiotool-maketx
                    misnamed-file
                    missingcolor
                    null
                    rational
                   )

    set (all_texture_tests
                    texture-derivs texture-fill
                    texture-flipt texture-gettexels texture-gray
                    texture-interp-bicubic
                    texture-blurtube
                    texture-crop texture-cropover
                    texture-half texture-uint16
                    texture-icwrite
                    texture-interp-bilinear
                    texture-interp-closest
                    texture-levels-stochaniso
                    texture-levels-stochmip
                    texture-mip-nomip texture-mip-onelevel
                    texture-mip-trilinear
                    texture-mip-stochastictrilinear
                    texture-mip-stochasticaniso
                    texture-missing
                    texture-overscan
                    texture-pointsample
                    texture-udim texture-udim2
                    texture-uint8
                    texture-width0blur
                    texture-wrapfill
                    texture-fat texture-skinny
                    texture-stats
                    texture-threadtimes
                    texture-env
                    texture-colorspace
                   )
    oiio_add_tests (${all_texture_tests})
    # Duplicate texture tests with batch mode
    oiio_add_tests (${all_texture_tests}
                    SUFFIX ".batch"
                    ENVIRONMENT TESTTEX_BATCH=1)

    # Tests that require oiio-images:
    oiio_add_tests (gpsread
                    oiiotool-attribs
                    texture-filtersize
                    texture-filtersize-stochastic
                    texture-res texture-maxres
                    IMAGEDIR oiio-images URL "Recent checkout of oiio-images"
                   )

    # Add tests that require the Python bindings.
    #
    # We also exclude these tests if this is a sanitizer build, because the
    # Python interpreter itself won't be linked with the right asan
    # libraries to run correctly.
    if (USE_PYTHON AND NOT BUILD_OIIOUTIL_ONLY AND NOT SANITIZE)
        oiio_add_tests (
            docs-examples-python
            python-colorconfig
            python-deep 
            python-imagebuf
            python-imagecache
            python-imageoutput
            python-imagespec
            python-paramlist
            python-roi
            python-texturesys
            python-typedesc
            filters
            )
        # These Python tests also need access to oiio-images
        oiio_add_tests (
            python-imageinput python-imagebufalgo
            IMAGEDIR oiio-images
            )
    endif ()

    oiio_add_tests (oiiotool-color
                    FOUNDVAR OpenColorIO_FOUND)

    # List testsuites for specific formats or features which might be not found
    # or be intentionally disabled, or which need special external reference
    # images from the web that if not found, should skip the tests:
    oiio_add_tests (bmp
                    ENABLEVAR ENABLE_BMP
                    IMAGEDIR oiio-images/bmpsuite)
    oiio_add_tests (cineon
                    ENABLEVAR ENABLE_CINEON
                    IMAGEDIR oiio-images URL "Recent checkout of oiio-images")
    oiio_add_tests (dpx
                    ENABLEVAR ENABLE_DPX
                    IMAGEDIR oiio-images URL "Recent checkout of oiio-images")
    oiio_add_tests (dds
                    ENABLEVAR ENABLE_DDS
                    IMAGEDIR oiio-images/dds URL "Recent checkout of oiio-images")
    oiio_add_tests (fits
                    ENABLEVAR ENABLE_FITS
                    IMAGEDIR fits-images
                    URL http://www.cv.nrao.edu/fits/data/tests/)
    oiio_add_tests (gif
                    FOUNDVAR GIF_FOUND ENABLEVAR ENABLE_GIF
                    IMAGEDIR oiio-images URL "Recent checkout of oiio-images")
    oiio_add_tests (hdr
                    ENABLEVAR ENABLE_HDR
                    IMAGEDIR openexr-images
                    URL http://github.com/AcademySoftwareFoundation/openexr-images)
    oiio_add_tests (heif
                    FOUNDVAR Libheif_FOUND
                    ENABLEVAR ENABLE_Libheif
                    IMAGEDIR oiio-images/heif
                    URL http://github.com/AcademySoftwareFoundation/openexr-images)
    oiio_add_tests (ico
                    ENABLEVAR ENABLE_ICO
                    IMAGEDIR oiio-images URL "Recent checkout of oiio-images")
    oiio_add_tests (iff
                    ENABLEVAR ENABLE_IFF
                    IMAGEDIR oiio-images URL "Recent checkout of oiio-images")
    oiio_add_tests (jpeg2000
                    FOUNDVAR OPENJPEG_FOUND
                    IMAGEDIR oiio-images URL "Recent checkout of oiio-images")
    oiio_add_tests (jpeg2000-j2kp4files
                    FOUNDVAR OPENJPEG_FOUND
                    IMAGEDIR j2kp4files_v1_5
                    URL http://www.itu.int/net/ITU-T/sigdb/speimage/ImageForm-s.aspx?val=10100803)
    set (all_openexr_tests
         openexr-suite openexr-multires openexr-chroma openexr-decreasingy
         openexr-v2 openexr-window perchannel oiiotool-deep)
    if (USE_PYTHON AND NOT SANITIZE)
        list (APPEND all_openexr_tests openexr-copy)
    endif ()
    if (OpenEXR_VERSION VERSION_GREATER_EQUAL 3.1.10)
        # OpenEXR 3.1.10 is the first release where the exr core library
        # properly supported all compression types (DWA in particular).
        list (APPEND all_openexr_tests openexr-compression)
    endif ()
    # Run all OpenEXR tests without core library
    oiio_add_tests (${all_openexr_tests} openexr-luminance-chroma
                    ENVIRONMENT OPENIMAGEIO_OPTIONS=openexr:core=0
                    IMAGEDIR openexr-images
                    URL http://github.com/AcademySoftwareFoundation/openexr-images)
    # For OpenEXR >= 3.1, be sure to test with the core option on
    if (OpenEXR_VERSION VERSION_GREATER_EQUAL 3.1)
        oiio_add_tests (${all_openexr_tests}
                        SUFFIX ".core"
                        ENVIRONMENT OPENIMAGEIO_OPTIONS=openexr:core=1
                        IMAGEDIR openexr-images
                        URL http://github.com/AcademySoftwareFoundation/openexr-images)
    endif ()
    # if (NOT DEFINED ENV{${PROJECT_NAME}_CI})
    #     oiio_add_tests (openexr-damaged
    #                     IMAGEDIR openexr-images
    #                     URL http://github.com/AcademySoftwareFoundation/openexr-images)
    # endif ()
    oiio_add_tests (openvdb texture-texture3d
                    FOUNDVAR OpenVDB_FOUND ENABLEVAR ENABLE_OpenVDB)
    oiio_add_tests (openvdb texture-texture3d
                    SUFFIX ".batch"
                    ENVIRONMENT TESTTEX_BATCH=1
                    FOUNDVAR OpenVDB_FOUND ENABLEVAR ENABLE_OpenVDB)
    oiio_add_tests (png png-damaged
                    ENABLEVAR ENABLE_PNG
                    IMAGEDIR oiio-images)
    oiio_add_tests (pnm
                    ENABLEVAR ENABLE_PNM
                    IMAGEDIR oiio-images)
    oiio_add_tests (psd psd-colormodes
                    ENABLEVAR ENABLE_PSD
                    IMAGEDIR oiio-images)
    oiio_add_tests (ptex
                    FOUNDVAR PTEX_FOUND ENABLEVAR ENABLE_PTEX)
    oiio_add_tests (raw
                    FOUNDVAR LIBRAW_FOUND ENABLEVAR ENABLE_LIBRAW
                    IMAGEDIR oiio-images/raw)
    oiio_add_tests (rla
                    ENABLEVAR ENABLE_RLA
                    IMAGEDIR oiio-images)
    oiio_add_tests (sgi
                    ENABLEVAR ENABLE_SGI
                    IMAGEDIR oiio-images)
    oiio_add_tests (targa
                    ENABLEVAR ENABLE_TARGA
                    IMAGEDIR oiio-images)
    if (USE_PYTHON AND NOT SANITIZE)
        oiio_add_tests (targa-thumbnail
                    ENABLEVAR ENABLE_TARGA
                    IMAGEDIR oiio-images)
    endif()
    if (NOT WIN32)
        oiio_add_tests (term
                        ENABLEVAR ENABLE_TERM)
        # I just could not get this test to work on Windows CI. The test fails
        # when comparing the output, but the saved artifacts compare just fine
        # on my system. Maybe someone will come back to this.
        endif ()
    oiio_add_tests (tiff-suite tiff-depths tiff-misc
                    IMAGEDIR oiio-images/libtiffpic)
    oiio_add_tests (webp
                    FOUNDVAR WebP_FOUND ENABLEVAR ENABLE_WebP
                    IMAGEDIR oiio-images/webp)
    oiio_add_tests (zfile ENABLEVAR ENABLE_ZFILE
                    IMAGEDIR oiio-images)

    if (SPI_TESTS)
        oiio_add_tests (oiiotool-spi
                        FOUNDVAR SPI_TESTS
                        IMAGEDIR spi-oiio-tests
                        URL "noplace -- it's SPI specific tests")
    endif ()

endmacro()


set (OIIO_LOCAL_TESTDATA_ROOT "${CMAKE_SOURCE_DIR}/.." CACHE PATH
     "Directory to check for local copies of testsuite data")
option (OIIO_DOWNLOAD_MISSING_TESTDATA "Try to download any missing test data" OFF)

function (oiio_get_test_data name)
    cmake_parse_arguments (_ogtd "" "REPO;BRANCH" "" ${ARGN})
       # Arguments: <prefix> <options> <one_value_keywords> <multi_value_keywords> args...
    if (IS_DIRECTORY "${OIIO_LOCAL_TESTDATA_ROOT}/${name}"
        AND NOT EXISTS "${CMAKE_BINARY_DIR}/testsuite/${name}")
        set (_ogtd_LINK_RESULT "")
        message (STATUS "Linking ${name} from ${OIIO_LOCAL_TESTDATA_ROOT}/${name}")
        file (CREATE_LINK "${OIIO_LOCAL_TESTDATA_ROOT}/${name}"
                          "${CMAKE_BINARY_DIR}/testsuite/${name}"
                          SYMBOLIC RESULT _ogtd_LINK_RESULT)
        # Note: Using 'COPY_ON_ERROR' in the above command should have prevented the need to
        # have the manual fall-back below. However, there's been at least one case where a user
        # noticed that copying did not happen if creating the link failed (CMake 3.24). We can
        # adjust this in the future if CMake behavior improves.
        message (VERBOSE "Link result ${_ogtd_LINK_RESULT}")
        if (NOT _ogtd_LINK_RESULT EQUAL 0)
            # Older cmake or failure to link -- copy
            message (STATUS "Copying ${name} from ${OIIO_LOCAL_TESTDATA_ROOT}/${name}")
            file (COPY "${OIIO_LOCAL_TESTDATA_ROOT}/${name}"
                  DESTINATION "${CMAKE_BINARY_DIR}/testsuite")
        endif ()
    elseif (IS_DIRECTORY "${CMAKE_BINARY_DIR}/testsuite/${name}")
        message (STATUS "Test data for ${name} already present in testsuite")
    elseif (OIIO_DOWNLOAD_MISSING_TESTDATA AND _ogtd_REPO)
        # Test data directory didn't exist -- fetch it
        message (STATUS "Cloning ${name} from ${_ogtd_REPO}")
        if (NOT _ogtd_BRANCH)
            set (_ogtd_BRANCH master)
        endif ()
        find_package (Git)
        if (Git_FOUND AND GIT_EXECUTABLE)
            execute_process(COMMAND ${GIT_EXECUTABLE} clone --depth 1
                                    ${_ogtd_REPO} -b ${_ogtd_BRANCH}
                                    ${CMAKE_BINARY_DIR}/testsuite/${name})
        else ()
            message (WARNING "${ColorRed}Could not find Git executable, could not download test data from ${_ogtd_REPO}${ColorReset}")
        endif ()
    else ()
        message (STATUS "${ColorRed}Missing test data ${name}${ColorReset}")
    endif ()
endfunction()

function (oiio_setup_test_data)
    oiio_get_test_data (oiio-images
                        REPO https://github.com/AcademySoftwareFoundation/OpenImageIO-images.git)
    oiio_get_test_data (openexr-images
                        REPO https://github.com/AcademySoftwareFoundation/openexr-images.git
                        BRANCH main)
    oiio_get_test_data (fits-images)
    oiio_get_test_data (j2kp4files_v1_5)
endfunction ()
