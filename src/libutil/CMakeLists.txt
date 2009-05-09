SET ( libutil_srcs argparse.cpp filesystem.cpp filter.cpp paramlist.cpp
                   plugin.cpp strutil.cpp sysutil.cpp typedesc.cpp 
                   ustring.cpp )
ADD_LIBRARY ( util SHARED ${libutil_srcs} )
TARGET_LINK_LIBRARIES ( util ${Boost_LIBRARIES} )
INSTALL ( TARGETS util LIBRARY DESTINATION lib )
