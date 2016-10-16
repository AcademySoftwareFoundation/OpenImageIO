if (USE_DICOM AND DCMTK_FOUND)
    add_oiio_plugin (dicominput.cpp dicomoutput.cpp
                     INCLUDE_DIRS ${DCMTK_INCLUDE_DIR}
                     LINK_LIBRARIES ${DCMTK_LIBRARIES}
                     DEFINITIONS "-DUSE_DCMTK=1")
else ()
    message (WARNING "DICOM plugin will not be built, no DCMTK")
endif ()
