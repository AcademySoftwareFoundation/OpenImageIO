if (USE_OPENJPEG AND OPENJPEG_FOUND)
    add_oiio_plugin (jpeg2000input.cpp jpeg2000output.cpp
                     INCLUDE_DIRS ${OPENJPEG_INCLUDE_DIR}
                     LINK_LIBRARIES ${OPENJPEG_LIBRARIES}
                     DEFINITIONS "-DUSE_OPENJPEG")
else()
    message (WARNING "Jpeg-2000 plugin will not be built")
endif()
