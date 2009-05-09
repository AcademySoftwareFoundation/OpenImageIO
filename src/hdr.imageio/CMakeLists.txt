SET ( hdr_srcs rgbe.cpp hdrinput.cpp hdroutput.cpp  )
ADD_LIBRARY ( hdr.imageio SHARED ${hdr_srcs} )
TARGET_LINK_LIBRARIES ( hdr.imageio OpenImageIO )
INSTALL ( TARGETS hdr.imageio LIBRARY DESTINATION lib )
