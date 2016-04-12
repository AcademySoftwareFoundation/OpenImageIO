if (USE_PTEX)

find_package (PTex)

if (PTEX_FOUND)
    # External PTex found
    add_oiio_plugin (ptexinput.cpp ptexoutput.cpp
                     INCLUDE_DIRS ${ZLIB_INCLUDE_DIR} ${PTEX_INCLUDE_DIR}
                     LINK_LIBRARIES ${PTEX_LIBRARIES} ${ZLIB_LIBRARIES}
                     DEFINITIONS "-DUSE_PTEX")
endif ()

endif (USE_PTEX)
