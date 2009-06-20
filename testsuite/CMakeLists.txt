MACRO ( RUNTEST testname )
    ADD_TEST ( ${testname} python ${PROJECT_SOURCE_DIR}/../testsuite/tests/${testname}/run.py ${PROJECT_SOURCE_DIR}/../testsuite/tests/${testname} ${CMAKE_BINARY_DIR} )
    # MESSAGE (STATUS "${testname} python ${PROJECT_SOURCE_DIR}/../testsuite/tests/${testname}/run.py ${PROJECT_SOURCE_DIR}/../testsuite/tests/${testname} ${CMAKE_BINARY_DIR}" )
ENDMACRO ()

RUNTEST ( ico )
RUNTEST ( gpsread )

if (EXISTS "${PROJECT_SOURCE_DIR}/../../libtiffpic")
    RUNTEST ( tiff-suite )
    RUNTEST ( tiff-depths )
else ()
    message (STATUS "\n\nDid not find ${PROJECT_SOURCE_DIR}/../../libtiffpic")
    message (STATUS "  -> Will not run tests 'tiff-suite' and 'tiff-depths'")
    message (STATUS "  -> You can find it at http://www.remotesensing.org/libtiff/images.html\n")
endif ()


