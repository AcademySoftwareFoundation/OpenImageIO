if (USE_GIF AND GIF_FOUND)
    add_oiio_plugin (gifinput.cpp gifoutput.cpp
                     INCLUDE_DIRS ${GIF_INCLUDE_DIRS}
                     LINK_LIBRARIES ${GIF_LIBRARIES}
                     DEFINITIONS "-DUSE_GIF")
else()
    message (WARNING "GIF plugin will not be built")
endif()
