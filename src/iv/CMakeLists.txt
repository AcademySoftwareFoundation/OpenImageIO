if (QT4_FOUND AND OPENGL_FOUND)
    include (${QT_USE_FILE})
    include_directories (${QT_INCLUDES} ${OPENGL_INCLUDE_DIR})
    qt4_generate_moc (imageviewer.h moc_imageviewer.cpp)
    set (iv_srcs imageviewer.cpp ivimage.cpp ivgl.cpp ivinfowin.cpp 
                 ivpref.cpp ivmain.cpp moc_imageviewer.cpp)
    add_executable (iv ${iv_srcs})
    link_ilmbase (iv)
    target_link_libraries (iv OpenImageIO ${QT_LIBRARIES} ${OPENGL_LIBRARIES} 
                               ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
    oiio_install_targets (iv)

else ()
    message (STATUS "\n\n   WARNING: No QT/OpenGL -- 'iv' will not be built!\n")
endif ()
