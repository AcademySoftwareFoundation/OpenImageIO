SET ( testtex_srcs testtex.cpp )
ADD_EXECUTABLE ( testtex ${testtex_srcs} )
LINK_ILMBASE ( testtex )
TARGET_LINK_LIBRARIES ( testtex OpenImageIO ${Boost_LIBRARIES} ${CMAKE_DL_LIBS} )
