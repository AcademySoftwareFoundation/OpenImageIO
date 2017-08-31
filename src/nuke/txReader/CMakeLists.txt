include_directories (${NUKE_INCLUDE_DIRS})
link_directories (${NUKE_LIBRARY_DIRS})

add_library (txReader SHARED
    txReader.cpp)

target_link_libraries (txReader
    DDImage OpenImageIO)

if (WIN32)
    add_definitions (/DFN_OS_WINDOWS /DUSE_GLEW)
    set_target_properties (txReader PROPERTIES
        PREFIX ""
        COMPILE_FLAGS "/wd4251 /W3 /O2 /MD /GL /GR /GF"
        LINK_FLAGS "/machine:x64 /SUBSYSTEM:WINDOWS /dll")
elseif (APPLE)
    target_link_libraries (txReader
        GLEW)
    add_definitions (-DUSE_GLEW)
    set_target_properties (txReader PROPERTIES
        PREFIX ""
        COMPILE_FLAGS "-arch x86_64"
        LINK_FLAGS "-arch x86_64 -bundle -framework QuartzCore -framework IOKit -framework CoreFoundation -framework Carbon -framework ApplicationServices -framework OpenGL -framework AGL")
else ()
    add_definitions (-DUSE_GLEW)
    set_target_properties (txReader PROPERTIES
        PREFIX ""
        COMPILE_FLAGS "-fPIC -msse")
endif ()

install (TARGETS txReader
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/nuke")
