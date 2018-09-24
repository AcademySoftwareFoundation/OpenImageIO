if (OPENVDB_FOUND)
    add_oiio_plugin (openvdbinput.cpp
                     INCLUDE_DIRS ${OpenVDB_INCLUDE_DIR} ${TBB_INCLUDE_DIRS}
                     LINK_LIBRARIES ${OpenVDB_LIBRARIES} ${TBB_tbb_LIBRARY})
endif()
