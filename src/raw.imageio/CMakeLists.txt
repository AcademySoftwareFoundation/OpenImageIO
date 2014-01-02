if (USE_LIBRAW AND LIBRAW_FOUND)
    include_directories (${LibRaw_INCLUDE_DIR})
    add_oiio_plugin (rawinput.cpp rawoutput.cpp LINK_LIBRARIES ${LibRaw_r_LIBRARIES})
endif ()
