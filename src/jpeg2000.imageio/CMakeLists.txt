find_package(Jasper)
if (JASPER_FOUND AND USE_JASPER)
    include_directories (${JASPER_INCLUDE_DIR})
    add_oiio_plugin (jpeg2000input.cpp jpeg2000output.cpp
                     LINK_LIBRARIES ${JASPER_LIBRARY})
else()
    message (STATUS "Jasper not found: jpeg2000 plugin will not be built")
endif()
