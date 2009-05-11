# FIXME to use OPENEXR_FOUND, OPENEXR_INCLUDE_DIR, OPENEXR_LIBRARIES when
# these are defined.

ADD_OIIO_PLUGIN ( exrinput.cpp exroutput.cpp )

LINK_ILMBASE(openexr.imageio)
LINK_OPENEXR(openexr.imageio)

