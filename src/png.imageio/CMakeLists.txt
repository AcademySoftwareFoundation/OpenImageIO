find_package (PNG REQUIRED)
if (PNG_FOUND)
    add_oiio_plugin (pnginput.cpp pngoutput.cpp
                     INCLUDE_DIRS ${PNG_INCLUDE_DIR}
                     LINK_LIBRARIES ${PNG_LIBRARIES})
else ()
    message (WARNING "libpng not found, so PNG support will not work")
endif ()

