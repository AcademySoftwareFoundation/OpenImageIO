SET ( libOpenImageIO_srcs formatspec.cpp imagebuf.cpp imagebufalgo.cpp
                          imageinput.cpp imageio.cpp imageioplugin.cpp
                          imageoutput.cpp iptc.cpp xmp.cpp 
                          ../libutil/argparse.cpp 
                          ../libutil/filesystem.cpp 
                          ../libutil/filter.cpp 
                          ../libutil/paramlist.cpp 
                          ../libutil/plugin.cpp 
                          ../libutil/strutil.cpp 
                          ../libutil/sysutil.cpp 
                          ../libutil/typedesc.cpp 
                          ../libutil/ustring.cpp 
                          ../libtexture/texturesys.cpp 
                          ../libtexture/texoptions.cpp 
                          ../libtexture/imagecache.cpp 
                         )
ADD_LIBRARY ( OpenImageIO SHARED ${libOpenImageIO_srcs} )
TARGET_LINK_LIBRARIES ( OpenImageIO ${Boost_LIBRARIES} )
LINK_ILMBASE ( OpenImageIO )
INSTALL ( TARGETS OpenImageIO LIBRARY DESTINATION lib )
