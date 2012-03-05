if (WEBP_FOUND)
    include_directories (${WEBP_INCLUDE_DIR})
    add_oiio_plugin (webpinput.cpp webpoutput.cpp LINK_LIBRARIES ${WEBP_LIBRARY})
endif()


