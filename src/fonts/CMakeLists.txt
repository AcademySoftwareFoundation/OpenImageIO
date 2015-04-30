file (GLOB public_fonts "*/*.ttf")

if (INSTALL_FONTS)
    install (FILES ${public_fonts} DESTINATION ${FONTS_INSTALL_DIR}
             COMPONENT fonts)
endif ()

