# install pkgconfig file
IF ( NOT WIN32 )
   configure_file(OpenImageIO.pc.in "${CMAKE_BINARY_DIR}/OpenImageIO.pc" @ONLY) 
   install (FILES "${CMAKE_BINARY_DIR}/OpenImageIO.pc"
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
            COMPONENT developer)
ENDIF()
