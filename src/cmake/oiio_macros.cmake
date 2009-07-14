# Macro to install targets to the appropriate locations.  Use this instead of
# the install(TARGETS ...) signature.
#
# Usage:
#
#    oiio_install_targets (target1 [target2 ...])
#
macro (oiio_install_targets)
    install (TARGETS ${ARGN}
             RUNTIME DESTINATION bin COMPONENT user
             LIBRARY DESTINATION lib COMPONENT user
             ARCHIVE DESTINATION lib COMPONENT developer)
endmacro()


# Macro to add a build target for an IO plugin.
#
# Usage:
#
# add_oiio_plugin ( source1 [source2 ...]
#                   [LINK_LIBRARIES external_lib1 ...] )
#
# The plugin name is deduced from the name of the current directory and the
# source is automatically linked against OpenImageIO.  Additional libraries
# (for example, libpng) may be specified after the optionl LINK_LIBRARIES
# keyword.
#
macro (add_oiio_plugin)
    parse_arguments (_plugin "LINK_LIBRARIES" "" ${ARGN})
    set (_target_name ${CMAKE_CURRENT_SOURCE_DIR})
    # Get the name of the current directory and use it as the target name.
    get_filename_component (_target_name ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    add_library (${_target_name} SHARED ${_plugin_DEFAULT_ARGS})
    target_link_libraries (${_target_name} OpenImageIO ${_plugin_LINK_LIBRARIES})
    set_target_properties (${_target_name} PROPERTIES PREFIX "")
    oiio_install_targets (${_target_name})
endmacro ()


# Macro that adds DLL to the installer created by NSIS generator
#
# Usage:
#
# add_dll_fils ()
#
macro (add_dll_files)
    install (FILES ${Boost_LIBRARY_DIRS}/boost_date_time-vc90-mt-1_38.dll
                   ${Boost_LIBRARY_DIRS}/boost_filesystem-vc90-mt-1_38.dll
                   ${Boost_LIBRARY_DIRS}/boost_regex-vc90-mt-1_38.dll
                   ${Boost_LIBRARY_DIRS}/boost_system-vc90-mt-1_38.dll
                   ${Boost_LIBRARY_DIRS}/boost_thread-vc90-mt-1_38.dll
                   ${QT_BINARY_DIR}/QtCore4.dll
                   ${QT_BINARY_DIR}/QtGui4.dll
                   ${QT_BINARY_DIR}/QtOpenGL4.dll
                   ${TBB_HOME}/tbb-${TBB_VERSION}/lib/tbb.dll
                   ${ILMBASE_HOME}/ilmbase-${ILMBASE_VERSION}/lib/Imath.dll
                   ${ILMBASE_HOME}/ilmbase-${ILMBASE_VERSION}/lib/Half.dll
                   ${ILMBASE_HOME}/ilmbase-${ILMBASE_VERSION}/lib/IlmThread.dll
                   ${ILMBASE_HOME}/ilmbase-${ILMBASE_VERSION}/lib/Iex.dll
                   ${OPENEXR_HOME}/openexr-${OPENEXR_VERSION}/lib/IlmImf.dll
                   ${ZLIB_INCLUDE_DIR}/../lib/zlib1.dll
                   ${PNG_PNG_INCLUDE_DIR}/../lib/libpng13.dll
                   ${TIFF_INCLUDE_DIR}/../lib/libtiff.dll
                   ${GLEW_INCLUDES}/../lib/glew32.dll
             DESTINATION bin COMPONENT user)
endmacro ()
