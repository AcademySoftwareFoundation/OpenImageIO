set (oiiotool_srcs oiiotool.cpp diff.cpp imagerec.cpp printinfo.cpp)
add_executable (oiiotool ${oiiotool_srcs})
target_link_libraries (oiiotool PRIVATE OpenImageIO ${Boost_LIBRARIES})
# N.B. Boost needs to be a direct dependency of oiiotool because it uses regex.
# This will no longer be the case after we're gcc >= 6.3 only.
set_target_properties (oiiotool PROPERTIES FOLDER "Tools")
oiio_install_targets (oiiotool)
