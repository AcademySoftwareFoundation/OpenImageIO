option (${PROJ_NAME}_SPIREZ "Enable extra bits for building for SPI Rez install" OFF)
set (${PROJ_NAME}_REZ_PACKAGE_NAME CACHE STRING "${PROJECT_NAME}")

if (${PROJ_NAME}_SPIREZ)
    set (appcfg_filename "${CMAKE_BINARY_DIR}/${PROJECT_NAME}_${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}.${PROJECT_VERSION_TWEAK}.xml")
    configure_file ("${PROJECT_SOURCE_DIR}/site/spi/appcfg.xml.in" "${appcfg_filename}")
endif ()
