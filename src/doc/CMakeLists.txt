SET ( public_docs openimageio.pdf CLA-INDIVIDUAL CLA-CORPORATE )

INSTALL ( FILES ${public_docs} DESTINATION doc )

INSTALL ( FILES ${PROJECT_SOURCE_DIR}/../LICENSE
                ${PROJECT_SOURCE_DIR}/../INSTALL
                ${PROJECT_SOURCE_DIR}/../CHANGES
          DESTINATION . )
