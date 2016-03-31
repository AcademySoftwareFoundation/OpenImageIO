if (FIELD3D_FOUND)
    add_oiio_plugin (field3dinput.cpp field3doutput.cpp
                     INCLUDE_DIRS ${FIELD3D_INCLUDES}
                     LINK_LIBRARIES ${FIELD3D_LIBRARY} ${HDF5_LIBRARIES} )
endif()
