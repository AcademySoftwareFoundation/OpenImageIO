IF ( QT4_FOUND AND OPENGL_FOUND )
  INCLUDE ( ${QT_USE_FILE} )
  INCLUDE_DIRECTORIES ( ${QT_INCLUDES} ${OPENGL_INCLUDE_DIR} )
  QT4_GENERATE_MOC ( imageviewer.h moc_imageviewer.cpp )
  SET ( iv_srcs imageviewer.cpp ivimage.cpp ivgl.cpp ivinfowin.cpp 
                ivpref.cpp ivmain.cpp moc_imageviewer.cpp )
  ADD_EXECUTABLE ( iv ${iv_srcs} )
  LINK_ILMBASE ( iv )
  TARGET_LINK_LIBRARIES ( iv OpenImageIO ${QT_LIBRARIES} ${OPENGL_LIBRARIES} 
                             ${Boost_LIBRARIES} ${SYSTEM_DL_LIB} )
  INSTALL ( TARGETS iv RUNTIME DESTINATION bin )

ENDIF ()
