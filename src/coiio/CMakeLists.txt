add_library(coiio SHARED 
    imageio.cpp
    imageio.h
    imageio_defines.h
    typedesc.cpp
    typedesc.h
)

target_link_libraries(coiio PUBLIC OpenImageIO)
set_target_properties(coiio 
    PROPERTIES
        VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
        SOVERSION ${SOVERSION}
        POSITION_INDEPENDENT_CODE ON
)

set(public_headers
    imageio.h
    imageio_defines.h
    typedesc.h
)

install_targets(coiio)
install(FILES ${public_headers}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/coiio
    COMPONENT developer
)