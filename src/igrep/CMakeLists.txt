set (igrep_srcs igrep.cpp)
add_executable (igrep ${igrep_srcs})
target_link_libraries (igrep PRIVATE OpenImageIO ${Boost_LIBRARIES})
# N.B. Boost needs to be a direct dependency of igrep because it uses regex.
# This will no longer be the case after we're gcc >= 6.3 only.
set_target_properties (igrep PROPERTIES FOLDER "Tools")
oiio_install_targets (igrep)
