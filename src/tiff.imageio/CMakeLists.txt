find_package (TIFF)
find_package (ZLIB)

if (TIFF_FOUND AND JPEG_FOUND AND ZLIB_FOUND)
    include_directories (${TIFF_INCLUDE_DIR})
    add_oiio_plugin (tiffinput.cpp tiffoutput.cpp
        LINK_LIBRARIES ${TIFF_LIBRARIES} ${JPEG_LIBRARIES} ${ZLIB_LIBRARIES})
endif ()
