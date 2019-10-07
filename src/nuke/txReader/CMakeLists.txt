include_directories (${NUKE_INCLUDES})

# Ignore Nuke 10's use of deprecated auto_ptr
if (NOT MSVC)
    set_source_files_properties (txReader.cpp PROPERTIES
                                    COMPILE_OPTIONS -Wno-deprecated-declarations)
endif ()

add_library (txReader MODULE txReader.cpp)

target_link_libraries (txReader PUBLIC ${NUKE_LIBRARIES} OpenImageIO)

if (WIN32)
    add_definitions (/DFN_OS_WINDOWS /DUSE_GLEW)
    set_target_properties (txReader PROPERTIES
        PREFIX ""
        COMPILE_OPTIONS /wd4251 /W3 /O2 /MD /GL /GR /GF
        LINK_FLAGS "/machine:x64 /SUBSYSTEM:WINDOWS /dll")
elseif (APPLE)
    target_link_libraries (txReader PRIVATE GLEW)
    add_definitions (-DUSE_GLEW)
    set_target_properties (txReader PROPERTIES
        PREFIX ""
        COMPILE_OPTIONS -arch x86_64
        LINK_FLAGS "-arch x86_64 -bundle -framework QuartzCore -framework IOKit -framework CoreFoundation -framework Carbon -framework ApplicationServices -framework OpenGL -framework AGL")
else ()
    add_definitions (-DUSE_GLEW)
    set_target_properties (txReader PROPERTIES
        PREFIX "")
endif ()

oiio_install_targets (txReader)
