if (USE_PTEX)

find_package (ZLIB REQUIRED)
find_package (PTex)

if (PTEX_FOUND)
    # External PTex found
    include_directories (${ZLIB_INCLUDE_DIR})
    include_directories (${PTEX_INCLUDE_DIR})
    add_oiio_plugin (ptexinput.cpp ptexoutput.cpp
                 LINK_LIBRARIES ${PTEX_LIBRARIES} ${ZLIB_LIBRARIES} )
endif ()

endif (USE_PTEX)
