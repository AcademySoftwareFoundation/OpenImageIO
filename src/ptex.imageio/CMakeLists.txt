if (USE_PTEX)

find_package (PTex)

if (PTEX_FOUND)
    # External PTex found
    add_oiio_plugin (ptexinput.cpp ptexoutput.cpp
                     INCLUDE_DIRS ${ZLIB_INCLUDE_DIR} ${PTEX_INCLUDE_DIR}
                     LINK_LIBRARIES ${PTEX_LIBRARIES} ${ZLIB_LIBRARIES}
                     DEFINITIONS "-DUSE_PTEX")
else ()
    # No external PTex. Fall back on the embedded version.
    if (WIN32)
        add_definitions ("-DPTEX_EXPORTS")
    endif()
    add_oiio_plugin (ptexinput.cpp ptexoutput.cpp
                     ptex/PtexCache.cpp
                     ptex/PtexFilters.cpp
                     ptex/PtexHalf.cpp
                     ptex/PtexReader.cpp
                     ptex/PtexSeparableFilter.cpp
                     ptex/PtexSeparableKernel.cpp
                     ptex/PtexTriangleFilter.cpp
                     ptex/PtexTriangleKernel.cpp
                     ptex/PtexUtils.cpp
                     ptex/PtexWriter.cpp
                     INCLUDE_DIRS ${ZLIB_INCLUDE_DIR}
                     LINK_LIBRARIES ${ZLIB_LIBRARIES}
                     DEFINITIONS "-DUSE_PTEX")
endif ()

endif (USE_PTEX)
