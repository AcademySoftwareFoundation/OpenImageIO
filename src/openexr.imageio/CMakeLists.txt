# FIXME to use OPENEXR_FOUND, OPENEXR_INCLUDE_DIR, OPENEXR_LIBRARIES when
# these are defined.

add_oiio_plugin (exrinput.cpp exroutput.cpp)

link_ilmbase (openexr.imageio)
link_openexr (openexr.imageio)

