if (USE_RAW AND FOUND_RAW)
    include_directories (${LibRaw_INCLUDE_DIR})
    add_oiio_plugin (rawinput.cpp rawoutput.cpp LINK_LIBRARIES ${LibRaw_r_LIBRARIES})
endif ()
