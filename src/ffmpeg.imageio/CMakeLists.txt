# Make the build complete for newer ffmpeg versions (3.1.1+) that have
# marked m_format_context->streams[i]->codec as deprecated.
# FIXME -- at some point, come back and figure out how to fix for real
# before the field disappears entirely.
set_source_files_properties (ffmpeginput.cpp
                             PROPERTIES COMPILE_FLAGS "-Wno-deprecated-declarations")

if (USE_FFMPEG)
    if (FFMPEG_FOUND)
        include_directories (${FFMPEG_INCLUDE_DIR})
        add_oiio_plugin (ffmpeginput.cpp ffmpegoutput.cpp
                         LINK_LIBRARIES ${FFMPEG_LIBRARY}
                        )
    else()
        message (STATUS "FFmpeg not found: ffmpeg plugin will not be built")
    endif()
endif()
