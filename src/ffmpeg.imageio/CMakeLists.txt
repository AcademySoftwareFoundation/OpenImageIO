if (USE_FFMPEG AND FFMPEG_FOUND)
    find_package (BZip2)   # Used by ffmpeg
    if (NOT BZIP2_FOUND)
        set (BZIP2_LIBRARIES "")
    endif ()
    add_oiio_plugin (ffmpeginput.cpp ffmpegoutput.cpp
                     INCLUDE_DIRS ${FFMPEG_INCLUDE_DIR}
                     LINK_LIBRARIES ${FFMPEG_LIBRARIES} ${BZIP2_LIBRARIES}
                     DEFINITIONS "-DUSE_FFMPEG")
else()
    message (STATUS "FFmpeg not found: ffmpeg plugin will not be built")
endif()
