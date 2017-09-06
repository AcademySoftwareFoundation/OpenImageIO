include_directories (${NUKE_INCLUDE_DIRS})
link_directories (${NUKE_LIBRARY_DIRS})

# Ignore Nuke 10's use of deprecated auto_ptr
if (NOT MSVC)
    set_source_files_properties (txWriter.cpp PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)
endif ()

add_library (txWriter SHARED
    txWriter.cpp)

target_link_libraries (txWriter
    DDImage OpenImageIO)

if (WIN32)
    add_definitions (/DFN_OS_WINDOWS /DUSE_GLEW)
    set_target_properties (txWriter PROPERTIES
        PREFIX ""
        COMPILE_FLAGS "/wd4251 /W3 /O2 /MD /GL /GR /GF"
        LINK_FLAGS "/machine:x64 /SUBSYSTEM:WINDOWS /dll")
elseif (APPLE)
    target_link_libraries (txWriter
        GLEW)
    add_definitions (-DUSE_GLEW)
    set_target_properties (txWriter PROPERTIES
        PREFIX ""
        COMPILE_FLAGS "-arch x86_64"
        LINK_FLAGS "-arch x86_64 -bundle -framework QuartzCore -framework IOKit -framework CoreFoundation -framework Carbon -framework ApplicationServices -framework OpenGL -framework AGL")
else ()
    add_definitions (-DUSE_GLEW)
    set_target_properties (txWriter PROPERTIES
        PREFIX ""
        COMPILE_FLAGS "-fPIC -msse")
endif ()

oiio_install_targets (txWriter)
