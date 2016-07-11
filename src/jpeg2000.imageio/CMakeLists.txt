if (USE_OPENJPEG AND OPENJPEG_FOUND)
    include_directories (${OPENJPEG_INCLUDE_DIR})
    if (${OPENJPEG_VERSION} VERSION_LESS 2.0)
        # Old OpenJpeg 1.5. Remove this eventually.
        add_oiio_plugin (jpeg2000input-v1.cpp jpeg2000output-v1.cpp
                         LINK_LIBRARIES ${OPENJPEG_LIBRARIES})
    else ()
        # OpenJpeg 2.x. Eventually this should be the only one we need.
        add_oiio_plugin (jpeg2000input.cpp jpeg2000output.cpp
                         LINK_LIBRARIES ${OPENJPEG_LIBRARIES})
    endif ()
else()
    message (WARNING "Jpeg-2000 plugin will not be built")
endif()
