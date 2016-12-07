if (USE_PTEX AND PTEX_FOUND)
    add_oiio_plugin (ptexinput.cpp
                     INCLUDE_DIRS ${ZLIB_INCLUDE_DIR} ${PTEX_INCLUDE_DIR}
                     LINK_LIBRARIES ${PTEX_LIBRARIES} ${ZLIB_LIBRARIES}
                     DEFINITIONS "-DUSE_PTEX")
endif ()
