find_package (ZLIB)

if (ZLIB_FOUND)
    include_directories (${ZLIB_INCLUDE_DIR})
    add_oiio_plugin (zfile.cpp LINK_LIBRARIES ${ZLIB_LIBRARIES})
endif ()
