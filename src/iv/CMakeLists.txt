if (QT4_FOUND AND OPENGL_FOUND AND GLEW_FOUND)
    include (${QT_USE_FILE})
    include_directories (${QT_INCLUDES} ${OPENGL_INCLUDE_DIR} ${GLEW_INCLUDES})
    add_custom_command (OUTPUT "${CMAKE_BINARY_DIR}/src/iv/moc_imageviewer.cpp"
                        DEPENDS imageviewer.h imageviewer.cpp
                        COMMAND ${QT_MOC_EXECUTABLE}
                        ARGS "${PROJECT_SOURCE_DIR}/src/iv/imageviewer.h"
                           -o "${CMAKE_BINARY_DIR}/src/iv/moc_imageviewer.cpp"
                        )
    add_custom_command (OUTPUT ${CMAKE_BINARY_DIR}/src/iv/moc_ivgl.cpp
                        DEPENDS ivgl.h ivgl.cpp
                        COMMAND ${QT_MOC_EXECUTABLE}
                        ARGS "${PROJECT_SOURCE_DIR}/src/iv/ivgl.h"
                           -o "${CMAKE_BINARY_DIR}/src/iv/moc_ivgl.cpp"
                        )
    set (iv_srcs imageviewer.cpp ivimage.cpp ivgl.cpp ivinfowin.cpp 
                 ivpref.cpp ivmain.cpp moc_imageviewer.cpp moc_ivgl.cpp)
    if (FORCE_OPENGL_1)
        add_definitions(-DFORCE_OPENGL_1)
    endif()
    add_executable (iv ${iv_srcs})
    set_target_properties (iv PROPERTIES FOLDER "Tools")
    link_ilmbase (iv)
    target_link_libraries (iv OpenImageIO ${QT_LIBRARIES} ${OPENGL_LIBRARIES} 
                               ${Boost_LIBRARIES} ${CMAKE_DL_LIBS}
                               ${GLEW_LIBRARIES})
    oiio_install_targets (iv)

else ()
    message (STATUS "\n\n   WARNING: Qt, OpenGL, or GLEW not found -- 'iv' will not be built!\n")
endif ()
