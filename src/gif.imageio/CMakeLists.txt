if (USE_GIF AND GIF_FOUND)
    add_oiio_plugin (gifinput.cpp gifoutput.cpp
                     INCLUDE_DIRS ${GIF_INCLUDE_DIR}
                     LINK_LIBRARIES ${GIF_LIBRARIES})
endif()
