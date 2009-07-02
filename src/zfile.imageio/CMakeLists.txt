find_package (ZLIB)

if (ZLIB_FOUND)
    include_directories (${ZLIB_INCLUDE_DIR})
    if (MSVC)
        add_definitions (-DBOOST_ALL_DYN_LINK)
    endif (MSVC)
    add_oiio_plugin (zfile.cpp LINK_LIBRARIES ${ZLIB_LIBRARIES})
endif ()
