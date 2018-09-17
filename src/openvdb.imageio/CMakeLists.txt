if (OPENVDB_FOUND)
    add_oiio_plugin (openvdbinput.cpp
                     INCLUDE_DIRS ${OPENVDB_INCLUDES}
                     LINK_LIBRARIES ${OpenVDB_LIBRARIES} ${Tbb_TBB_LIBRARY})
endif()
