set (iinfo_srcs iinfo.cpp)
add_executable (iinfo ${iinfo_srcs})
target_link_libraries (iinfo PRIVATE OpenImageIO ${Boost_LIBRARIES})
# N.B. Boost needs to be a direct dependency of iinfo because it uses regex.
# This will no longer be the case after we're gcc >= 6.3 only.
set_target_properties (iinfo PROPERTIES FOLDER "Tools")
oiio_install_targets (iinfo)
