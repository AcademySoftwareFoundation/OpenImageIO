find_package (ZLIB)

if (ZLIB_FOUND)
	if (WIN32)
        add_definitions ("-DPTEX_EXPORTS")
	endif()
    include_directories (${ZLIB_INCLUDE_DIR})
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
                 LINK_LIBRARIES ${ZLIB_LIBRARIES} )
endif ()

