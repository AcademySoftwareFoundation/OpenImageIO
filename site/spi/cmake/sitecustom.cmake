option (${PROJ_NAME}_SPIREZ "Enable extra bits for building for SPI Rez install" OFF)
set (${PROJ_NAME}_REZ_PACKAGE_NAME CACHE STRING "${PROJECT_NAME}")

if (${PROJ_NAME}_SPIREZ)
    message (STATUS "Creating package.py from package.py.in")
    configure_file ("${PROJECT_SOURCE_DIR}/site/spi/rez/package.py.in" "${CMAKE_BINARY_DIR}/package.py")

    set (appcfg_filename "${CMAKE_BINARY_DIR}/${PROJECT_NAME}_${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}.${PROJECT_VERSION_TWEAK}.xml")
    configure_file ("${PROJECT_SOURCE_DIR}/site/spi/appcfg.xml.in" "${appcfg_filename}")

    install (FILES ${CMAKE_BINARY_DIR}/package.py
                   "${PROJECT_SOURCE_DIR}/site/spi/rez/CMakeLists.txt"
                   ${appcfg_filename}
             DESTINATION ${CMAKE_INSTALL_PREFIX}
             COMPONENT developer)
endif ()
