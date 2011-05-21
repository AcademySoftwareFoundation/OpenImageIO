if (WEBP_FOUND)
    add_oiio_plugin (webpinput.cpp jwebpoutput.cpp LINK_LIBRARIES ${WEBP_LIBRARY})
endif()


