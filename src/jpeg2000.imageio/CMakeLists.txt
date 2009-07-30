find_package(Jasper)
if (JASPER_FOUND)
    include_directories (${JASPER_INCLUDE_DIR})
    add_oiio_plugin (jpeg2000input.cpp jpeg2000output.cpp
                     LINK_LIBRARIES ${JASPER_LIBRARY})
endif()