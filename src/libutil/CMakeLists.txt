set (libutil_srcs argparse.cpp errorhandler.cpp
                  filesystem.cpp filter.cpp paramlist.cpp plugin.cpp SHA1.cpp
                  strutil.cpp sysutil.cpp typedesc.cpp ustring.cpp pystring.cpp
                  pugixml.cpp pugixpath.cpp)
add_library (util SHARED ${libutil_srcs})
target_link_libraries (util ${Boost_LIBRARIES})
oiio_install_targets (util)
