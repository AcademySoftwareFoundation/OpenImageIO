file (GLOB public_fonts "*/*.ttf")

if (INSTALL_FONTS AND FREETYPE_FOUND)
    install (FILES ${public_fonts}
             DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/fonts/OpenImageIO
             COMPONENT fonts)
endif ()

