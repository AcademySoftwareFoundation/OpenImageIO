if (FIELD3D_FOUND)
    find_library (SZIP_LIBRARY NAMES sz)
    if (NOT SZIP_LIBRARY)
        set (SZIP_LIBRARY "")
    endif ()
    add_oiio_plugin (field3dinput.cpp field3doutput.cpp
                     INCLUDE_DIRS ${FIELD3D_INCLUDES}
                     LINK_LIBRARIES ${FIELD3D_LIBRARY} ${HDF5_LIBRARIES}
                                    ${SZIP_LIBRARY})
endif()
