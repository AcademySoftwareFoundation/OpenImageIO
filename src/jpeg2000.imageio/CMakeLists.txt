if (USE_OPENJPEG AND OPENJPEG_FOUND)
    include_directories (${OPENJPEG_INCLUDES})
    add_oiio_plugin (jpeg2000input.cpp jpeg2000output.cpp LINK_LIBRARIES ${OPENJPEG_LIBRARIES})
endif()
