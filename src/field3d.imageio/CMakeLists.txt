if (USE_FIELD3D)
  if (FIELD3D_FOUND AND HDF5_FOUND)
    include_directories (${FIELD3D_INCLUDES})
    add_oiio_plugin (field3dinput.cpp field3doutput.cpp
                     LINK_LIBRARIES ${FIELD3D_LIBRARY}
                                    ${HDF5_LIBRARIES}
                     )
  else()
    message (STATUS "Field3D not found: field3d plugin will not be built")
  endif()
endif()
