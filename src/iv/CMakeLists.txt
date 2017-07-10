if (Qt5_FOUND AND OPENGL_FOUND AND GLEW_FOUND)
    set (CMAKE_AUTOMOC ON)
	if (Qt5_POSITION_INDEPENDENT_CODE)
        set (CMAKE_POSITION_INDEPENDENT_CODE ON)
	endif()
    include_directories (${OPENGL_INCLUDE_DIR} ${GLEW_INCLUDES})
    set (iv_srcs imageviewer.cpp ivimage.cpp ivgl.cpp ivinfowin.cpp 
                 ivpref.cpp ivmain.cpp)
    if (FORCE_OPENGL_1)
        add_definitions(-DFORCE_OPENGL_1)
    endif()
    add_executable (iv ${iv_srcs})
    set_target_properties (iv PROPERTIES FOLDER "Tools")
    target_link_libraries (iv OpenImageIO
                               Qt5::Core Qt5::Gui Qt5::Widgets Qt5::OpenGL
                               ${OPENGL_LIBRARIES}
                               ${Boost_LIBRARIES} ${CMAKE_DL_LIBS}
                               ${GLEW_LIBRARIES})
    oiio_install_targets (iv)

else ()
    message (STATUS "\n\n   WARNING: Qt, OpenGL, or GLEW not found -- 'iv' will not be built!\n")
endif ()
