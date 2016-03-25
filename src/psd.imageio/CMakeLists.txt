if (JPEG_FOUND)
    include_directories (${JPEG_INCLUDE_DIR})
    add_oiio_plugin (psdinput.cpp psdoutput.cpp jpeg_memory_src.cpp
        LINK_LIBRARIES ${JPEG_LIBRARIES})
endif ()

