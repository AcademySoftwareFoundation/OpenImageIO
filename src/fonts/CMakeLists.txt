file (GLOB public_fonts "*/*.ttf")

if (INSTALL_FONTS AND USE_FREETYPE)
    install (FILES ${public_fonts}
             DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/fonts/OpenImageIO
             COMPONENT fonts)
endif ()

