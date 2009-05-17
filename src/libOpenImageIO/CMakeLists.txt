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

# If the 'EMBEDPLUGINS' option is set, we want to compile the source for
# all the plugins into libOpenImageIO.
IF ( EMBEDPLUGINS )
    SET ( libOpenImageIO_srcs ${libOpenImageIO_srcs}
	    ../hdr.imageio/hdrinput.cpp ../hdr.imageio/hdroutput.cpp
	      ../hdr.imageio/rgbe.cpp
	    ../ico.imageio/icoinput.cpp ../ico.imageio/icooutput.cpp
	    ../jpeg.imageio/jpeginput.cpp ../jpeg.imageio/jpegoutput.cpp
	      ../jpeg.imageio/jpegexif.cpp
	    ../png.imageio/pnginput.cpp ../png.imageio/pngoutput.cpp
	    ../openexr.imageio/exrinput.cpp ../openexr.imageio/exroutput.cpp
	    ../tiff.imageio/tiffinput.cpp ../tiff.imageio/tiffoutput.cpp
	    ../zfile.imageio/zfile.cpp
         )
    FIND_PACKAGE ( PNG REQUIRED )
    FIND_PACKAGE ( JPEG REQUIRED )
    FIND_PACKAGE ( TIFF REQUIRED )
    FIND_PACKAGE ( ZLIB REQUIRED )
    INCLUDE_DIRECTORIES ( ${PNG_INCLUDE_DIR} ${JPEG_INCLUDE_DIR} )
    INCLUDE_DIRECTORIES ( ${TIFF_INCLUDE_DIR} ${ZLIB_INCLUDE_DIR} )
    ADD_DEFINITIONS ( "-DEMBED_PLUGINS=1" )
ENDIF ()


ADD_LIBRARY ( OpenImageIO SHARED ${libOpenImageIO_srcs} )
TARGET_LINK_LIBRARIES ( OpenImageIO ${Boost_LIBRARIES} )
LINK_ILMBASE ( OpenImageIO )

IF ( EMBEDPLUGINS )
    TARGET_LINK_LIBRARIES ( OpenImageIO ${PNG_LIBRARIES} ${TIFF_LIBRARIES}
                               ${JPEG_LIBRARIES} ${ZLIB_LIBRARIES} IlmImf 
                          )
ENDIF ()

INSTALL ( TARGETS OpenImageIO LIBRARY DESTINATION lib )
