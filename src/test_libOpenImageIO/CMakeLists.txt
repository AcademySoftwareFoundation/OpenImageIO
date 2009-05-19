IF ( GTEST_FOUND )
    ADD_EXECUTABLE ( test_libOpenImageIO test_imageio.cpp )
    INCLUDE_DIRECTORIES ( ${GTEST_INCLUDES} )
    LINK_ILMBASE ( test_libOpenImageIO )
    TARGET_LINK_LIBRARIES ( test_libOpenImageIO OpenImageIO ${Boost_LIBRARIES} 
                                ${GTEST_LIBRARY} ${CMAKE_DL_LIBS} )
ENDIF ()
