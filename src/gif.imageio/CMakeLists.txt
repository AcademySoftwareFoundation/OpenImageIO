if (USE_GIF AND GIF_FOUND)
    include_directories (${GIF_INCLUDE_DIR})
    add_oiio_plugin (gifinput.cpp gifoutput.cpp LINK_LIBRARIES ${GIF_LIBRARIES})
endif()
