add_oiio_plugin (tiffinput.cpp tiffoutput.cpp
                 INCLUDE_DIRS ${TIFF_INCLUDE_DIR}
                 LINK_LIBRARIES ${TIFF_LIBRARIES} ${JPEG_LIBRARIES}
                                ${ZLIB_LIBRARIES})
