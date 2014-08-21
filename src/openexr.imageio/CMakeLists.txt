# FIXME to use OPENEXR_FOUND, OPENEXR_INCLUDE_DIR, OPENEXR_LIBRARIES when
# these are defined.

add_oiio_plugin (exrinput.cpp exroutput.cpp)

link_openexr (openexr.imageio)
link_ilmbase (openexr.imageio)

