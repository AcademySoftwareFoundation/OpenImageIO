find_package (PNG)

if (PNG_FOUND)
    include_directories (${PNG_INCLUDE_DIR})
    add_oiio_plugin (icoinput.cpp icooutput.cpp LINK_LIBRARIES ${PNG_LIBRARIES})
    if (MSVC)
        add_definitions (-DBOOST_ALL_DYN_LINK)
    endif (MSVC)
endif ()
