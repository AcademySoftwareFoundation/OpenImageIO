find_package (TIFF)

if (JPEG_FOUND AND TIFF_FOUND)
    include_directories (${TIFF_INCLUDE_DIR})
    include_directories (${JPEG_INCLUDE_DIR})
    add_oiio_plugin (jpeginput.cpp jpegoutput.cpp
        LINK_LIBRARIES ${JPEG_LIBRARIES})
endif ()
