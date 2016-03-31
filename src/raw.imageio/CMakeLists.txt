if (USE_LIBRAW AND LIBRAW_FOUND)
    add_oiio_plugin (rawinput.cpp rawoutput.cpp
                     INCLUDE_DIRS ${LibRaw_INCLUDE_DIR}
                     LINK_LIBRARIES ${LibRaw_r_LIBRARIES})
endif ()
