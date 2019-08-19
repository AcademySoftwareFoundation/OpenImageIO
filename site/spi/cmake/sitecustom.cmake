option (OIIO_SPIREZ "Enable extra bits for building for SPI Rez install" OFF)
set (OIIO_REZ_PACKAGE_NAME CACHE STRING "${PROJECT_NAME}")

if (OIIO_SPIREZ)
    message (STATUS "Creating package.py from package.py.in")
    configure_file ("${PROJECT_SOURCE_DIR}/site/spi/rez/package.py.in" "${CMAKE_BINARY_DIR}/package.py")

    set (appcfg_filename "${CMAKE_BINARY_DIR}/OpenImageIO_${OIIO_VERSION_MAJOR}.${OIIO_VERSION_MINOR}.${OIIO_VERSION_PATCH}.${OIIO_VERSION_TWEAK}.xml")
    configure_file ("${PROJECT_SOURCE_DIR}/site/spi/appcfg.xml.in" "${appcfg_filename}")

    install (FILES ${CMAKE_BINARY_DIR}/package.py
                   "${PROJECT_SOURCE_DIR}/site/spi/rez/build.py"
                   ${appcfg_filename}
             DESTINATION ${CMAKE_INSTALL_PREFIX}
             COMPONENT developer)
endif ()
