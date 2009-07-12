set (public_docs openimageio.pdf CLA-INDIVIDUAL CLA-CORPORATE)

install (FILES ${public_docs} DESTINATION doc COMPONENT documentation)

install (FILES ${PROJECT_SOURCE_DIR}/../LICENSE
               ${PROJECT_SOURCE_DIR}/../INSTALL
               ${PROJECT_SOURCE_DIR}/../CHANGES
         DESTINATION .)
