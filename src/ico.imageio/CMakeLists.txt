if (PNG_FOUND)
    add_oiio_plugin (icoinput.cpp icooutput.cpp
                     INCLUDE_DIRS ${PNG_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS}
                     LINK_LIBRARIES ${PNG_LIBRARIES} ${ZLIB_LIBRARIES})
else ()
    message (WARNING "libpng not found, so ICO support will not work")
endif ()

