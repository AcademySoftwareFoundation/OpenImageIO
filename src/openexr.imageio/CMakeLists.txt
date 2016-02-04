find_package (OpenEXR REQUIRED)

add_oiio_plugin (exrinput.cpp exroutput.cpp)

target_link_libraries (openexr.imageio ${OPENEXR_LIBRARIES})

