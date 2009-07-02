# FIXME to use OPENEXR_FOUND, OPENEXR_INCLUDE_DIR, OPENEXR_LIBRARIES when
# these are defined.

add_oiio_plugin (exrinput.cpp exroutput.cpp)
if (MSVC)
    add_definitions (-DBOOST_ALL_DYN_LINK)
    add_definitions (-DOPENEXR_DLL)
endif (MSVC)

link_ilmbase (openexr.imageio)
link_openexr (openexr.imageio)

