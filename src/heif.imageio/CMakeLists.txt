if (USE_HEIF AND LIBHEIF_FOUND)
    add_oiio_plugin (heifinput.cpp heifoutput.cpp
                     INCLUDE_DIRS ${LIBHEIF_INCLUDES}
                     LINK_LIBRARIES ${LIBHEIF_LIBRARIES}
                     DEFINITIONS "-DUSE_HEIF=1")
else ()
    message (WARNING "heif plugin will not be built")
endif ()
