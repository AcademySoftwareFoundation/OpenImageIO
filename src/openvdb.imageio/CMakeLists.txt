if (OPENVDB_FOUND)
    add_oiio_plugin (openvdbinput.cpp
                     INCLUDE_DIRS ${OPENVDB_INCLUDE_DIRS} ${TBB_INCLUDE_DIRS}
                     LINK_LIBRARIES ${OPENVDB_LIBRARIES} ${TBB_tbb_LIBRARY} ${BOOST_LIBRARIES})
endif()
