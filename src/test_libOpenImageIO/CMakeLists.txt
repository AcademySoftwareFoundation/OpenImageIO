if (GTEST_FOUND)
    add_executable (test_libOpenImageIO test_imageio.cpp)
    include_directories (${GTEST_INCLUDES})
    link_ilmbase (test_libOpenImageIO)
    target_link_libraries (test_libOpenImageIO OpenImageIO ${Boost_LIBRARIES} 
                                ${GTEST_LIBRARY} ${CMAKE_DL_LIBS})
endif ()
