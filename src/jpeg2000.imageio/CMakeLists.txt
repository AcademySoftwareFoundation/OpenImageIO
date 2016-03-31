if (USE_OPENJPEG AND OPENJPEG_FOUND)
    add_oiio_plugin (jpeg2000input.cpp jpeg2000output.cpp
                     INCLUDE_DIRS ${OPENJPEG_INCLUDE_DIR}
                     LINK_LIBRARIES ${OPENJPEG_LIBRARIES})
endif()
