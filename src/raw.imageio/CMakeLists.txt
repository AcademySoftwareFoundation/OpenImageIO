if (USE_LIBRAW AND LIBRAW_FOUND)
    add_oiio_plugin (rawinput.cpp
                     INCLUDE_DIRS ${LibRaw_INCLUDE_DIR}
                     LINK_LIBRARIES ${LibRaw_r_LIBRARIES}
                     DEFINITIONS "-DUSE_LIBRAW=1")
else ()
    message (WARNING "Raw plugin will not be built")
endif ()
