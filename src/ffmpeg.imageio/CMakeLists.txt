if (USE_FFMPEG)
    if (FFMPEG_FOUND)
        include_directories (${FFMPEG_INCLUDE_DIR})
        add_oiio_plugin (ffmpeginput.cpp ffmpegoutput.cpp
                         LINK_LIBRARIES ${FFMPEG_LIBRARIES}
                        )
    else()
        message (STATUS "FFmpeg not found: ffmpeg plugin will not be built")
    endif()
endif()
