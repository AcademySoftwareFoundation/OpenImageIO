# Find libpng.  Obviously this needs some work.
#
# Unfortunately the cmake standard FIND_PACKAGE() scripts and the dist setup
# for the external project interact badly, hence the hardcoded mess for now.
SET ( CMAKE_INCLUDE_PATH "${THIRD_PARTY_TOOLS_HOME}/include/libpng-1.2.31" )
SET ( CMAKE_LIBRARY_PATH "${THIRD_PARTY_TOOLS_HOME}/lib/libpng-1.2.31" )
FIND_PACKAGE ( PNG )

INCLUDE_DIRECTORIES( ${PNG_INCLUDE_DIR} )

ADD_OIIO_PLUGIN ( pnginput.cpp pngoutput.cpp
                  LINK_LIBRARIES ${PNG_LIBRARY} ${ZLIB_LIBRARY} )
